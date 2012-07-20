#include <pthread.h>
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <assert.h>
#include <stdlib.h>

#define MAX 30

static int G[MAX];

static lua_State *
_new(const char * name,int n) {
	lua_State * L = luaL_newstate();
	lua_gc(L, LUA_GCSTOP, 0);
	luaL_openlibs(L);  
	lua_gc(L, LUA_GCRESTART, 0);

	lua_newtable(L);
	int i;
	for (i=0;i<MAX;i++) {
		lua_pushinteger(L,G[i]);
		lua_rawseti(L,-2,i+1);
	}
	lua_setglobal(L,"handles");
	lua_pushinteger(L,n);
	lua_setglobal(L,"thread");

	int err = luaL_dofile(L, name);
	if (err != 0) {
		printf("err: %p :",L);
		fflush(stdout);
		printf("%s\n", lua_tostring(L,-1));
		exit(1);
	}

	return L;
}

struct args {
	int n;
};

static void *
thread_test(void* ptr) { 
	struct args *ud = ptr;
	lua_State * L = _new("test_client.lua",ud->n);
	lua_close(L);
	return NULL;
}

int 
main() {
	lua_State * L = _new("test_server.lua", -1);

	lua_getglobal(L, "handles");

	int i;
	for (i=0;i<MAX;i++) {
		lua_rawgeti(L,-1,i+1);
		G[i] = lua_tointeger(L,-1);
		lua_pop(L,1);
		printf("%d: %d\t",i, G[i]);
	}

	printf("\ninit\n");

	pthread_t pid[MAX];

	for (i=0;i<MAX;i++) {
		struct args *ud = malloc(sizeof(struct args));
		ud->n = i;
		pthread_create(&pid[i], NULL, thread_test, ud);
	}

	for (i=0;i<MAX;i++) {
		pthread_join(pid[i], NULL); 
	}

	lua_close(L);	
	printf("main exit\n");

	return 0;
}

#include <lua.h>
#include <lauxlib.h>
#include <assert.h>
#include <stdio.h>

/*
	integer new_handle( { keyname = initvalue , ... } , typename or nil)  -- create an atomdict handle 
	delete_handle(integer handle)	-- release an atomdict handle
	userdata:object new(integer handle) -- create an atomdict object from a handle
	barrier()	-- call barrier for commit atomdict objects
	dump(object) -- dump for debug
	dump_handle(handle) 
 */

#include "atomdict.h"

#define MAX_HANDLE (1024 * 16)
#define MAX_OBJECT 1024

static struct atomdict * G[MAX_HANDLE];

struct barrier;

struct ad_object {
	struct atomdict *ad;
	struct barrier *barrier;
	int version;
	int col;
};

struct barrier {
	int version;
	int object;
	struct ad_object *obj[MAX_OBJECT];
};

static int
_find_slot(lua_State *L , struct atomdict *ad) {
	int i,j;
	for (j=0;j<2;j++) {
		for (i=0;i<MAX_HANDLE;i++) {
			if (G[i] == NULL) {
				if (__sync_bool_compare_and_swap(&(G[i]), NULL , ad)) {
					return i;
				}
			}
		}
	}
	atomdict_delete(ad);
	return luaL_error(L, "Can't find free slot for atomdict");
}

/*
	table [key - value]
	string typename (or nil)
 */
static int
_new_handle(lua_State * L) {
	struct atomdict *ad = atomdict_new();
	int idx = _find_slot(L,ad);
	luaL_checktype(L,1,LUA_TTABLE);
	const char * typename = NULL;
	if (lua_gettop(L) > 1) {
		typename = luaL_checkstring(L,2);
	}
	
	int n=0;
	lua_pushnil(L);  /* first key */
	while (lua_next(L, 1) != 0) {
		++n;
		lua_pop(L, 1);
	}
	struct atomslot slot[n+1];
	int i;
	lua_pushnil(L);
	for (i=0;i<n;i++) {
		int r = lua_next(L, 1);
		assert(r!=0);
		luaL_checktype(L,-2,LUA_TSTRING);
		slot[i].key = lua_tostring(L,-2);
		int type = lua_type(L,-1);
		switch (type) {
		case LUA_TSTRING:
			slot[i].vs = lua_tostring(L,-1);
			slot[i].vn = 0;
			break;
		case LUA_TNUMBER:
			slot[i].vn = (float)lua_tonumber(L,-1);
			slot[i].vs = NULL;
			break;
		default:
			return luaL_error(L,"Unsupport type for atomdict : %s",lua_typename(L,type));
		}
		lua_pop(L,1);
	}
	slot[n].key = NULL;
	slot[n].vs = NULL;
	slot[n].vn = 0;

	int r = atomdict_init(ad, slot, typename);
	if (r != 0) {
		return luaL_error(L, "Init atomdict error");
	}

	lua_pushinteger(L,idx);
	return 1;
}

static int
_delete_handle(lua_State *L) {
	int idx = luaL_checkinteger(L,1);
	struct atomdict *ad = G[idx];
	G[idx] = NULL;
	__sync_synchronize();
	atomdict_delete(ad);
	return 0;

}

#define _update_object(L,object) if (object->barrier->version == object->version) {} else _update_object_(L,object->barrier,object)

static void
_update_object_(lua_State *L, struct barrier *b, struct ad_object *object) {
	if (b->object >= MAX_OBJECT) {
		luaL_error(L, "Commit object too many");
	}
	object->version = b->version;
	b->obj[b->object++] = object;
	object->col = atomdict_grab(object->ad);
}

static void
_gen_keyindex(lua_State *L, struct atomdict *ad) {
	lua_newtable(L);
	int i = 0;
	for (;;) {
		struct atomslot slot;
		const char * key = atomdict_key(ad, i , &slot);
		if (key == NULL) {
			return;
		}
		if (slot.vs == NULL) {
			lua_pushinteger(L,(i+1));
		} else {
			lua_pushinteger(L,-(i+1));
		}
		lua_setfield(L,-2,key);
		++i;
	}
}

static void
_gen_keyindex_from_meta(lua_State *L, struct atomdict *ad, const char *typename) {
	lua_getfield(L,-1,"type");
	// stack now : ... , metatable , typecache 
	lua_getfield(L, -1 , typename);
	if (lua_isnil(L,-1)) {
		lua_pop(L,-1);
		_gen_keyindex(L, ad);
		lua_pushvalue(L,-1);
		lua_setfield(L,-3,typename);
		// stack now : ... , metatable, typecache, keyindex
	}
	lua_remove(L,-2);
}

/*
	uv1: barrier
	uv2: metatable
	integer handle

	return userdata
 */
static int
_new_object(lua_State *L) {
	struct barrier * b = lua_touserdata(L, lua_upvalueindex(1));
	int index = luaL_checkinteger(L,1);
	struct atomdict *ad = G[index];
	if (ad == NULL) {
		return luaL_error(L, "Atomdict binding a null handle");
	}

	struct ad_object *object = lua_newuserdata(L,sizeof(struct ad_object));
	object->ad = ad;
	object->barrier = b;
	object->version = -1;

	_update_object(L, object);

	lua_pushvalue(L, lua_upvalueindex(2));
	// stack now : handle , ... , object, metatable
	const char * typename = atomdict_typename(ad);

	if (typename) {
		_gen_keyindex_from_meta(L, ad, typename);
	} else {
		_gen_keyindex(L, ad);
	}
	// stack now : handle , ... , object, metatable, keyindex
	lua_setuservalue(L,-3);
	lua_setmetatable(L,-2);

	return 1;
}

static int
_name_object(lua_State *L) {
	struct ad_object *object = lua_touserdata(L,1);
	const char * typename = atomdict_typename(object->ad);
	if (typename == NULL) {
		lua_pushliteral(L, "[ATOMDICT]");
	} else {
		luaL_Buffer b;
		luaL_buffinit(L, &b);
		luaL_addchar(&b, '[');
		luaL_addstring(&b, typename);
		luaL_addchar(&b, ']');
		luaL_pushresult(&b);
	}
	return 1;
}

static int
_get_object(lua_State *L) {
	struct ad_object *object = lua_touserdata(L,1);
	_update_object(L, object);
	lua_getuservalue(L,1);
	lua_pushvalue(L,2);
	lua_rawget(L,-2);
	if (lua_isnil(L,-1)) {
		return luaL_error(L, "Invalid key %s", luaL_checkstring(L,2));
	}
	int index = lua_tointeger(L,-1);
	if (index > 0) {
		float v = atomdict_get_number(object->ad, object->col, index-1);
		lua_pushnumber(L,v);
	} else {
		const char * v = atomdict_get_string(object->ad, object->col, -(index+1));
		lua_pushstring(L,v);
	}
	return 1;
}

static int
_set_object(lua_State *L) {
	struct ad_object *object = lua_touserdata(L,1);
	_update_object(L, object);
	lua_getuservalue(L,1);
	lua_pushvalue(L,2);
	// stack now : object key value , ... , type , key
	lua_rawget(L,-2);
	if (lua_isnil(L,-1)) {
		return luaL_error(L, "Invalid key %s", luaL_checkstring(L,2));
	}
	int index = lua_tointeger(L,-1);
	int new_col ;

	if (index > 0) {
		float v = luaL_checknumber(L,3);
		new_col = atomdict_set_number(object->ad, object->col, index-1, v);
	} else {
		const char * v = luaL_checkstring(L,3);
		new_col = atomdict_set_string(object->ad, object->col, -(index+1), v);
	}
	if (new_col < 0) {
		return luaL_error(L, "atomdict set %s error", lua_tostring(L,2));
	}
	object->col = new_col;

	return 0;
}

/*
	userdata:object
	string or nil (key)
*/
static int
_next_property(lua_State *L) {
	if (lua_gettop(L) == 1) {
		lua_getuservalue(L,1);
		lua_pushnil(L);
	} else {
		lua_getuservalue(L,1);
		lua_pushvalue(L,2);
	}
	int end = lua_next(L,-2);
	// stack now: object , keyindex, (nextkey , value)
	if (end == 0) {
		return 0;
	}
	int index = lua_tointeger(L,-1);
	lua_pop(L,1);
	struct ad_object *object = lua_touserdata(L,1);
	_update_object(L, object);
	if (index > 0) {
		float v = atomdict_get_number(object->ad, object->col, index-1);
		lua_pushnumber(L,v);
	} else {
		const char * v = atomdict_get_string(object->ad, object->col, -(index+1));
		lua_pushstring(L,v);
	}
	return 2;
}

static int
_pairs_object(lua_State *L) {
	lua_pushcfunction(L,_next_property);
	lua_pushvalue(L,1);
	lua_pushnil(L);
	return 3;
}

static int
_create_meta(lua_State *L) {
	luaL_Reg meta[] = {
		{ "__index", _get_object },
		{ "__newindex", _set_object },
		{ "__tostring", _name_object },
		{ "__pairs", _pairs_object },
		{ NULL , NULL },
	};
	luaL_newlib(L,meta);
	lua_newtable(L);
	lua_setfield(L,-2,"type");
	return 1;
}

static int
_barrier(lua_State *L) {
	struct barrier * b = lua_touserdata(L, lua_upvalueindex(1));
	++b->version;
	int i;
	for (i=0;i<b->object;i++) {
		atomdict_commit(b->obj[i]->ad, b->obj[i]->col);
	}
	b->object = 0;
	return 0;
}

static int
_dump_object(lua_State *L) {
	struct ad_object * obj = lua_touserdata(L,1);
	atomdict_dump(obj->ad, obj->col);
	return 0;
}

static int
_dump_handle(lua_State *L) {
	int idx = luaL_checkinteger(L,1);
	struct atomdict * ad = G[idx];
	if (ad == NULL) {
		return luaL_error(L, "atomdict handle %d is invalid", idx);
	}
	atomdict_dump(ad, -1);
	return 0;
}

int
luaopen_atomdict(lua_State *L) {
	luaL_checkversion(L);

	// stack now : barrier 

	luaL_Reg l[] = {
		{ "new_handle", _new_handle },
		{ "delete_handle", _delete_handle },
		{ "dump_handle", _dump_handle },
		{ "dump", _dump_object },
		{ NULL , NULL },
	};

	luaL_newlib(L,l);

	struct barrier * b = lua_newuserdata(L, sizeof(struct barrier));
	b->version = 0;
	b->object = 0;

	// stack now : libtable barrier

	lua_pushvalue(L,-1);
	_create_meta(L);

	// stack now : libtable barrier barrier metatable 

	lua_pushcclosure(L, _new_object, 2);
	lua_setfield(L,-3, "new");

	// stack now : libtable barrier

	luaL_Reg l2[] = {
		{ "barrier", _barrier },
		{ NULL , NULL },
	};
	luaL_setfuncs(L, l2, 1);

	return 1;
}


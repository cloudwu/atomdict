#include "atomdict.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define ATOMDICT_NUMBER 0
#define ATOMDICT_STRING 1

#define MAX_COL 32
#define AVERAGE_STRING 64

#define DIRTY_WRITE 1
#define DIRTY_STRING 2

#if 0

static int g_count = 0;

static void *
my_malloc(size_t sz) {
	++g_count;
	return malloc(sz);
}

static void
my_free(void *p) {
	--g_count;
	free(p);
}

#define DUMP_MEM printf("memory count = %d\n",g_count);

#define malloc my_malloc
#define free my_free

#else

#define DUMP_MEM

#endif

union atomvalue {
	int s;
	float n;
} ;

struct atomcol {
	union atomvalue *v;
	char * stringbuffer;
	int sb_size;
	int dirty;
	int ref;
};

#define COL_STRING(col, n) (col->stringbuffer + col->v[n].s)

struct atomkey {
	const char *key;
	int type;
};

struct atomdict {
	int current;
	int property_count;
	int stringbuffer;
	char * typename;
	struct atomkey * key;
	struct atomcol col[MAX_COL]; 
};

struct atomdict * 
atomdict_new(void) {
	struct atomdict * ad = malloc(sizeof(struct atomdict));
	memset(ad, 0 , sizeof(*ad));
	return ad;
}

static int
_init_key(struct atomdict *ad, struct atomslot *slot, const char * typename) {
	int keybuffer_size = 0;
	int n=0;
	while (slot[n].key) {
		keybuffer_size += strlen(slot[n].key) + 1;
		++n;
	}
	if (typename) {
		keybuffer_size += strlen(typename) + 1;
	}
	assert(n>0);
	ad->property_count = n;
	ad->key = malloc( n * sizeof(struct atomkey) + keybuffer_size);
	char * keybuffer = (char *)(ad->key + n);
	if (typename) {
		int len = strlen(typename) + 1;
		memcpy(keybuffer, typename , len);
		ad->typename = keybuffer;
		keybuffer += len;
	}
	int i;
	int stringkey = 0;
	for (i=0;i<n;i++) {
		int len = strlen(slot[i].key) + 1;
		memcpy(keybuffer, slot[i].key, len);
		ad->key[i].key = keybuffer;
		keybuffer += len;
		if (slot[i].vs == NULL) {
			ad->key[i].type = ATOMDICT_NUMBER;
		} else {
			++stringkey;
			ad->key[i].type = ATOMDICT_STRING;
		}
	}

	return stringkey;
}

static void
_init_col(struct atomdict *ad, int n) {
	ad->stringbuffer = n * AVERAGE_STRING;
	int i;
	int pn = ad->property_count;
	for (i=0;i<MAX_COL;i++) {
		ad->col[i].v = malloc(pn * sizeof(union atomvalue));
		memset(ad->col[i].v, 0 , pn * sizeof(union atomvalue));
	}
	if (n > 0) {
		for (i=0;i<MAX_COL;i++) {
			ad->col[i].stringbuffer = malloc(ad->stringbuffer);
			ad->col[i].stringbuffer[0] = '\0';
		}
	}
}

static int
_insert_string(struct atomcol * col, const char *key, int cap) {
	int len = strlen(key) + 1;
	if (col->sb_size + len > cap) {
		return -1;
	}
	int ret = col->sb_size;
	memcpy(col->stringbuffer + ret , key, len);
	col->sb_size += len;
	
	return ret;
}

static int
_init_value(struct atomdict *ad, struct atomslot *slot) {
	int pn = ad->property_count;
	int i;
	struct atomcol *c = &(ad->col[0]);
	for (i=0;i<pn;i++) {
		if (slot[i].vs) {
			int index = _insert_string(c, slot[i].vs , ad->stringbuffer);
			if (index < 0) {
				return -1;
			}
			c->v[i].s = index;
		} else {
			c->v[i].n = slot[i].vn;
		}
	}
	return 0;
}

int
atomdict_init(struct atomdict *ad, struct atomslot *slot, const char *typename) {
	int stringkey = _init_key(ad, slot, typename);
	_init_col(ad, stringkey);
	return _init_value(ad, slot);
}

const char * 
atomdict_key(struct atomdict *ad, int n, struct atomslot *slot) {
	if (n>=ad->property_count) {
		return NULL;
	}
	const char * key = ad->key[n].key;
	if (slot) {
		slot->key = key;
		switch (ad->key[n].type) {
		case ATOMDICT_NUMBER:
			slot->vs = NULL;
			slot->vn = ad->col[ad->current].v[n].n;
			break;
		case ATOMDICT_STRING:
			slot->vs = COL_STRING(ad->col, n);
			slot->vn = 0;
			break;
		}
	}
	return key;
}

const char * 
atomdict_typename(struct atomdict *ad) {
	return ad->typename;
}

void
atomdict_delete(struct atomdict *ad) {
	if (ad == NULL)
		return;
	free(ad->key);
	int i;
	for (i=0;i<MAX_COL;i++) {
		struct atomcol *c = &(ad->col[i]);
		free(c->v);
		free(c->stringbuffer);
	}
	free(ad);
}

static void
_dump_col(struct atomdict *ad, int n, int current) {
	struct atomcol *c = &(ad->col[n]);
	struct atomkey *key = ad->key;
	if (n == current) {
		printf(" *");
	} else {
		if (c->ref == 0 && current >=0) {
			return;
		} else {
			printf(" ");
		}
	}

	printf("[%d] ref=%d dirty=%d string_size=%d\n",n,c->ref, c->dirty,c->sb_size);
	int i;
	for (i=0;i<ad->property_count;i++) {
		printf("  %s = ",key[i].key);
		switch(key[i].type) {
		case ATOMDICT_NUMBER:
			printf("%f",c->v[i].n);
			break;
		case ATOMDICT_STRING:
			printf("'%s'",COL_STRING(c,i));
			break;
		}
		printf("\n");
	}
}

void 
atomdict_dump(struct atomdict * ad, int col) {
	printf("ATOM DICT %p [%s]\n",ad, ad->typename);
	if (col >= 0) {
		printf(" current = %d\n",ad->current);
		if (col >= ad->property_count) {
			printf(" property_count = %d , request = %d \n",ad->property_count, col);
			return;
		}
		_dump_col(ad, col, -1);
	} else {
		col = atomdict_grab(ad);
		printf(" current = %d\n",col);
		printf(" property_count = %d\n",ad->property_count);
		printf(" stringbuffer_cap = %d\n",ad->stringbuffer);
		int i;
		for (i=0;i<MAX_COL;i++) {
			_dump_col(ad, i, col);
		}
		atomdict_commit(ad,col);
	}
}

int 
atomdict_grab(struct atomdict * ad) {
	for (;;) {
		int current = ad->current;
		__sync_add_and_fetch(&(ad->col[current].ref), 1);
		if (current != ad->current) {
			__sync_sub_and_fetch(&(ad->col[current].ref), 1);
		} else {
			return current;
		}
	}
}

static int
_get_free_col(struct atomdict * ad) {
	int i,j;
	for (j=0;j<4;j++) {
		for (i=0;i<MAX_COL;i++) {
			int ref = ad->col[i].ref;
			if (ref > 0) 
				continue;
			if (__sync_bool_compare_and_swap(&(ad->col[i].ref), 0 , 1)) {
				return i;
			}
		}
	}
	return -1;
}

static int 
_atomdict_dup(struct atomdict * ad, int copy) {
	int pn = ad->property_count;
	int c = _get_free_col(ad);
	if (c<0) {
		return -1;
	}
	struct atomcol * from = &(ad->col[copy]);
	struct atomcol * to = &(ad->col[c]);
	memcpy(to->v,from->v,pn * sizeof(union atomvalue));
	if (to->stringbuffer) {
		to->dirty = 0;
		to->sb_size = from->sb_size;
		memcpy(to->stringbuffer, from->stringbuffer, from->sb_size);
	}
	return c;
}

int 
atomdict_set_string(struct atomdict * ad, int col, int idx, const char * v) {
	assert(idx < ad->property_count && col < MAX_COL);
	struct atomcol *c = &(ad->col[col]);
	if (c->dirty == 0) {
		int old = col;
		col = _atomdict_dup(ad, old);
		if (col < 0) {
			return -2;
		}
		c = &(ad->col[col]);
		assert(c->ref == 1);
		c->dirty = DIRTY_WRITE | DIRTY_STRING;

		int ref = __sync_sub_and_fetch(&(ad->col[old].ref), 1);
		assert(ref >= 0);
	} else {
		c->dirty |= DIRTY_STRING;
	}
	int index = _insert_string(c, v, ad->stringbuffer);
	if (index < 0) {
		return -2;
	}
	c->v[idx].s = index;

	return col;
}

int 
atomdict_set_number(struct atomdict * ad, int col, int idx, float v) {
	assert(idx < ad->property_count && col < MAX_COL);
	struct atomcol *c = &(ad->col[col]);
	if (c->v[idx].n == v) {
		return col;
	}
	if (c->dirty == 0) {
		int old = col;
		col = _atomdict_dup(ad, old);
		c = &(ad->col[col]);
		assert(c->ref == 1);
		c->v[idx].n = v;
		c->dirty = DIRTY_WRITE;

		int ref = __sync_sub_and_fetch(&(ad->col[old].ref), 1);
		assert(ref >= 0);
	} else {
		c->v[idx].n = v;
	}
//	printf("set col=%d idx=%d %f\n",col,idx,v);

	return col;
}

const char * 
atomdict_get_string(struct atomdict * ad, int col, int idx) {
	assert(idx < ad->property_count && col < MAX_COL);
	struct atomcol *c = &(ad->col[col]);
	return COL_STRING(c, idx);
}

float 
atomdict_get_number(struct atomdict * ad, int col, int idx) {
	assert(idx < ad->property_count && col < MAX_COL);
	struct atomcol *c = &(ad->col[col]);
//	printf("get col=%d idx=%d %f\n",col,idx,c->v[idx].n);
	return c->v[idx].n;
}

void 
atomdict_commit(struct atomdict * ad, int col) {
	struct atomcol *c = &(ad->col[col]);
	if (c->dirty == 0) {
		int ref = __sync_sub_and_fetch(&(ad->col[col].ref), 1);
		assert(ref >= 0);
		return;
	}
	assert(c->ref == 1);
	if (c->dirty | DIRTY_STRING) {
		// arrange string buffer
		char tmp[c->sb_size];
		int pn = ad->property_count;
		int i;
		int cap = ad->stringbuffer;
		memcpy(tmp, c->stringbuffer, c->sb_size);
		c->sb_size = 0;
		for (i=0;i<pn;i++) {
			if (ad->key[i].type == ATOMDICT_STRING) {
				c->v[i].s = _insert_string(c, tmp+c->v[i].s , cap);
			}
		}
		c->dirty = 0;
	}
	__sync_synchronize();
	c->ref = 0;
	ad->current = col;
}


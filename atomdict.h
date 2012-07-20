#ifndef ATOMDICT_H
#define ATOMDICT_H

struct atomdict;

struct atomslot {
	const char *key;
	const char *vs;
	float vn;
};

struct atomdict * atomdict_new(void);
int atomdict_init(struct atomdict *, struct atomslot *, const char * typename);
const char * atomdict_key(struct atomdict *, int n, struct atomslot *);
const char * atomdict_typename(struct atomdict *);
void atomdict_delete(struct atomdict *);

int atomdict_grab(struct atomdict *);
void atomdict_commit(struct atomdict *, int col);

int atomdict_set_string(struct atomdict *, int col, int idx, const char * v);
int atomdict_set_number(struct atomdict *, int col, int idx, float v);
const char * atomdict_get_string(struct atomdict *, int col, int idx);
float atomdict_get_number(struct atomdict *, int col, int idx);

// for debug
void atomdict_dump(struct atomdict *, int col);

#endif

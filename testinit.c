#include <stdio.h>
#include "atomdict.h"

static void
test() {
	struct atomdict * ad = atomdict_new();
	struct atomslot s[] = {
		{ "HP" , NULL, 100 },
		{ "NAME" , "Hello World", 0 },
		{ NULL , NULL , 0 },
	};
	atomdict_init(ad, s , "TYPE");
	int i=0;
	for (;;) {
		struct atomslot as;
		const char * key = atomdict_key(ad, i, &as);
		if (key) {
			printf("%s = %s, %f\n",key, as.vs , as.vn);
		} else {
			break;
		}
		++i;
	}
	atomdict_dump(ad, -1);

	int col = atomdict_grab(ad);
	col = atomdict_set_string(ad, col, 1, "Bingo");

	atomdict_commit(ad, col);

	atomdict_dump(ad, -1);

	atomdict_delete(ad);
}

int 
main(int argc, char *argv[]) {

	test();	

	return 0;
}

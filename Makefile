linux : atomdict.so testinit testmt

mingw : atomdict.dll

testinit : testinit.c atomdict.c
	gcc -Wall -g -o $@ $^

testmt : testmt.c atomdict.so
	gcc -g -Wall -o $@  $< -I/usr/local/include -Wl,-E -llua -lpthread  -lm -ldl

atomdict.so : atomdict.c lua-ad.c
	gcc -Wall -g -fPIC --shared -o $@ -I/usr/local/include $^ 

atomdict.dll : atomdict.c lua-ad.c
	gcc -Wall -g -march=i686 --shared -o $@ -I/usr/local/include $^ -L/usr/local/bin -llua52

clean :
	rm testinit testmt atomdict.so

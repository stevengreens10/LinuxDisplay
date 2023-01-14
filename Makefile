all: graphics

graphics: main.o
	gcc -g main.o -o graphics -lm

main.o: drm.c
	gcc -c -g ./drm.c -o main.o -I "/usr/include/libdrm"

clean:
	rm -f main.o graphics

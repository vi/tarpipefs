tarpipefs: *.c *.h
		gcc -Wall `pkg-config fuse --cflags --libs` -lulockmgr *.c -lbsd -o tarpipefs

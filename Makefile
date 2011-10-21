tarpipefs: *.c *.h
		gcc -Wall `pkg-config fuse --cflags --libs` -lulockmgr *.c -lbsd -o tarpipefs

test: tarpipefs
		mkdir -p m
		./tarpipefs m > q.tar&
		sleep 1
		mkdir m/q
		echo Goodbye, world... > m/q/rip.txt
		fusermount -u m
		sleep 1
		tar -tf q.tar > q.ls
		printf "q\nq/rip.txt\n" > q.ls.orig
		diff -u q.ls q.ls.orig
		rm q.tar q.ls q.ls.orig

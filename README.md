You write to special write-only FS - it produces tar archive. Can be used for conversion of cpio archive into tar one or for creating tar archives on the fly without prior saving everything to disk.

    /tmp$ mkdir m
    /tmp$ tarpipefs m | gzip > ololo.tar.gz&
    [1] 1337
    /tmp/m$ cd m
    /tmp/m$ mkdir qqq
    /tmp/m$ mkdir qqq/lol # no mkdir -p yet
    /tmp/m$ echo Hello, world > qqq/lol/hello.txt
    /tmp/m$ cd ..
    /tmp$ fusermount -u m
    [1]+  Done      tarpipefs m | gzip > ololo.tar.gz
    /tmp$ tar -tvf ololo.tar.gz
    drwxrwxr-x root/root         0 1970-01-01 03:00 qqq
    drwxrwxr-x root/root         0 1970-01-01 03:00 qqq/lol
    -rwxrwxr-x root/root        13 1970-01-01 03:00 qqq/lol/hello.txt

This version is early and hacky:

1. Using tar implementation extracted from Git;
2. No chmod, no dates for directories/symlinks, no device nodes;
3. Can't do "mkdir -p";
4. Each single file is kept in memory;
5. Not tested much;

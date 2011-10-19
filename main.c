#include <stdio.h>
#include <string.h>

struct archiver_args;

int write_global_extended_header(struct archiver_args *args);

int write_tar_entry(struct archiver_args *args,
		const unsigned char *sha1, const char *path, size_t pathlen,
		unsigned int mode, void *buffer, unsigned long size);

void write_trailer(void);


int main() {
    write_global_extended_header(NULL);

    char* str = "Hello, world\n";
    char* path = "hello.txt";


    write_tar_entry(NULL, (unsigned char*)"", path, strlen(path), 0100644, str, strlen(str));

    write_trailer();
    return 0;
}

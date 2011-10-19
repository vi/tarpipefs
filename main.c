#include <stdio.h>
#include <string.h>

int write_tar_entry(
		const char *path, size_t pathlen,
		unsigned int mode, void *buffer, unsigned long size);
void write_trailer();


int main() {

    char* str = "Hello, world\n";
    char* path = "hello.txt";


    write_tar_entry(path, strlen(path), 0100644, str, strlen(str));

    write_trailer();
    return 0;
}

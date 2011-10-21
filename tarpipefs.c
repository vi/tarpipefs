/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags --libs` -lulockmgr fusexmp_fh.c -o fusexmp_fh
*/

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE

#include <fuse.h>
#include <ulockmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <bsd/string.h>
#include <time.h>

#include "tar.h"

char current_directory[PATH_MAX];
char current_file[PATH_MAX];

char* buffer;
size_t buffer_size;
off_t file_length;
unsigned long mtime;


static int is_in_current_directory(const char* path) {
    int i;
    for(i=0; ; ++i) {
        if(path[i]==current_directory[i]) {} else {
            if(!path[i] && current_directory[i] == '/') {
                return 1;
            }
            return 0;
        }
        if(path[i]==0 && current_directory[i]==0) {
            return 1;
        }
    }
}

static void write_to_tar_and_flush() {
    if(!current_file[0]) {
        return;
    }
    write_tar_entry(current_file+1, strlen(current_file+1), 0100755, buffer, file_length, mtime);

    if(buffer_size > 3*file_length) {
        free(buffer);
        buffer = malloc(1024);
        buffer_size = 1024;
    }

    current_file[0]=0;
    file_length=0;
}

static void ensure_buffer_size(size_t size) {
    if(size>buffer_size) {
        buffer = (char*) realloc(buffer, size*2);
    }
}

static void check_file(const char* path) {
    /* If we start dealing with some new file then write the old one first */
    if(strcmp(path, current_file)) {
        write_to_tar_and_flush();
        strlcpy(current_file, path, PATH_MAX);
    }
}

static int xmp_getattr(const char *path, struct stat *stbuf)
{
    if(!strcmp(path, current_file)) {
        stbuf->st_mode = 0100777;
        return 0;
    }
    if(is_in_current_directory(path)) {
        // dir
        stbuf->st_mode = 0040777;
        return 0;
    } else {
        return -ENOENT;
    }
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
    return -ENOTSUP;
}


static int xmp_opendir(const char *path, struct fuse_file_info *fi)
{
    return -ENOTSUP;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
    return -ENOTSUP;
}

static int xmp_releasedir(const char *path, struct fuse_file_info *fi)
{
    return -ENOTSUP;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
    return -ENOTSUP;
    // XXX
}

static int xmp_mkdir(const char *path, mode_t mode)
{
    strlcpy(current_directory, path, PATH_MAX);

    write_tar_entry(path+1, strlen(path+1), 0040644, NULL, 0, mtime);

	return 0;
}

static int xmp_unlink(const char *path)
{
    return -ENOTSUP;
}

static int xmp_rmdir(const char *path)
{
    return -ENOTSUP;
}

static int xmp_symlink(const char *from, const char *to)
{
    write_tar_entry(to+1, strlen(to+1), 0120755, from, strlen(from), (unsigned long)time(NULL));
    return 0;
}

static int xmp_rename(const char *from, const char *to)
{
    return -ENOTSUP;
}

static int xmp_link(const char *from, const char *to)
{
    return -ENOTSUP;
}

static int xmp_chmod(const char *path, mode_t mode)
{
    return -ENOTSUP;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{
    return -ENOTSUP;
}

static int xmp_truncate(const char *path, off_t size)
{
    check_file(path);
    ensure_buffer_size(size);
    file_length = size;

	return 0;
}

static int xmp_ftruncate(const char *path, off_t size,
			 struct fuse_file_info *fi)
{
    check_file(path);
    ensure_buffer_size(size);
    file_length = size;

	return 0;
}

static int xmp_utimens(const char *path, const struct timespec ts[2])
{
    if(strcmp(path, current_file)) {
        return -ENOTSUP;
    }       
    
    struct timeval tv[2];

    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;

    mtime = tv[1].tv_sec;
    return 0;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    check_file(path);

	return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
    check_file(path);

	return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
    if(!strcmp(path, current_file) && (size+offset <= file_length)) {
        memcpy(buf, buffer+offset, size);
        return size;
    }
    return -ENOTSUP;
}

static int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
    check_file(path);
    ensure_buffer_size(offset+size);
    if(offset+size>file_length) file_length = offset+size;

    memcpy(buffer+offset, buf, size);

	return size;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
    return -ENOTSUP;
}

static int xmp_flush(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
    /* Do not write yet because of we don't know all attributes */
	return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	return 0;
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
    return -ENOTSUP;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
    return -ENOTSUP;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
    return -ENOTSUP;
}

static int xmp_removexattr(const char *path, const char *name)
{
    return -ENOTSUP;
}
#endif /* HAVE_SETXATTR */

static int xmp_lock(const char *path, struct fuse_file_info *fi, int cmd,
		    struct flock *lock)
{
	(void) path;

	return ulockmgr_op(fi->fh, cmd, lock, &fi->lock_owner,
			   sizeof(fi->lock_owner));
}

static struct fuse_operations xmp_oper = {
	.getattr	= xmp_getattr,
	.readlink	= xmp_readlink,
	.opendir	= xmp_opendir,
	.readdir	= xmp_readdir,
	.releasedir	= xmp_releasedir,
	.mknod		= xmp_mknod,
	.mkdir		= xmp_mkdir,
	.symlink	= xmp_symlink,
	.unlink		= xmp_unlink,
	.rmdir		= xmp_rmdir,
	.rename		= xmp_rename,
	.link		= xmp_link,
	.chmod		= xmp_chmod,
	.chown		= xmp_chown,
	.truncate	= xmp_truncate,
	.ftruncate	= xmp_ftruncate,
	.utimens	= xmp_utimens,
	.create		= xmp_create,
	.open		= xmp_open,
	.read		= xmp_read,
	.write		= xmp_write,
	.statfs		= xmp_statfs,
	.flush		= xmp_flush,
	.release	= xmp_release,
	.fsync		= xmp_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= xmp_setxattr,
	.getxattr	= xmp_getxattr,
	.listxattr	= xmp_listxattr,
	.removexattr	= xmp_removexattr,
#endif
	.lock		= xmp_lock,

	.flag_nullpath_ok = 1,
};

int main(int argc, char *argv[])
{
    char* argv2[argc+2];
    int i;
    int argc2=0;

    argv2[argc2++]=argv[0];
    for(i=1;i<argc;++i)argv2[argc2++]=argv[i];
    argv2[argc2++]="-f";
    argv2[argc2++]="-s";
    argv2[argc2]=0;

    current_directory[0]=0;
    current_file[0]=0;
    buffer=(char*)malloc(1024);
    buffer_size=1024;
    file_length=0;
    
    mtime=(unsigned long)time(NULL);

	int ret = fuse_main(argc2, argv2, &xmp_oper, NULL);
    write_to_tar_and_flush();
    write_trailer();
    return ret;
}

/*
 * Copyright (c) 2005, 2006 Rene Scharfe
 *
 * Based on archive-tar.c from Git
 */

#include <string.h>
#include <bsd/string.h>
#include <sys/stat.h>
#include "strbuf.h"
#include "tar.h"

// Crudely extracted piece of Git

extern void write_or_die(int fd, const void *buf, size_t count);

#define RECORDSIZE	(512)
#define BLOCKSIZE	(RECORDSIZE * 20)

static char block[BLOCKSIZE];
static unsigned long offset;

static int tar_umask = 002;

/* writes out the whole block, but only if it is full */
static void write_if_needed(void)
{
	if (offset == BLOCKSIZE) {
		write_or_die(1, block, BLOCKSIZE);
		offset = 0;
	}
}

/*
 * queues up writes, so that all our write(2) calls write exactly one
 * full block; pads writes to RECORDSIZE
 */
static void write_blocked(const void *data, unsigned long size)
{
	const char *buf = data;
	unsigned long tail;

	if (offset) {
		unsigned long chunk = BLOCKSIZE - offset;
		if (size < chunk)
			chunk = size;
		memcpy(block + offset, buf, chunk);
		size -= chunk;
		offset += chunk;
		buf += chunk;
		write_if_needed();
	}
	while (size >= BLOCKSIZE) {
		write_or_die(1, buf, BLOCKSIZE);
		size -= BLOCKSIZE;
		buf += BLOCKSIZE;
	}
	if (size) {
		memcpy(block + offset, buf, size);
		offset += size;
	}
	tail = offset % RECORDSIZE;
	if (tail)  {
		memset(block + offset, 0, RECORDSIZE - tail);
		offset += RECORDSIZE - tail;
	}
	write_if_needed();
}

/*
 * The end of tar archives is marked by 2*512 nul bytes and after that
 * follows the rest of the block (if any).
 */
void write_trailer(void)
{
	int tail = BLOCKSIZE - offset;
	memset(block + offset, 0, tail);
	write_or_die(1, block, BLOCKSIZE);
	if (tail < 2 * RECORDSIZE) {
		memset(block, 0, offset);
		write_or_die(1, block, BLOCKSIZE);
	}
}

/*
 * pax extended header records have the format "%u %s=%s\n".  %u contains
 * the size of the whole string (including the %u), the first %s is the
 * keyword, the second one is the value.  This function constructs such a
 * string and appends it to a struct strbuf.
 */
static void strbuf_append_ext_header(struct strbuf *sb, const char *keyword,
                                     const char *value, unsigned int valuelen)
{
	int len, tmp;

	/* "%u %s=%s\n" */
	len = 1 + 1 + strlen(keyword) + 1 + valuelen + 1;
	for (tmp = len; tmp > 9; tmp /= 10)
		len++;

	strbuf_grow(sb, len);
	strbuf_addf(sb, "%u %s=", len, keyword);
	strbuf_add(sb, value, valuelen);
	strbuf_addch(sb, '\n');
}

static unsigned int ustar_header_chksum(const struct ustar_header *header)
{
	char *p = (char *)header;
	unsigned int chksum = 0;
	while (p < header->chksum)
		chksum += *p++;
	chksum += sizeof(header->chksum) * ' ';
	p += sizeof(header->chksum);
	while (p < (char *)header + sizeof(struct ustar_header))
		chksum += *p++;
	return chksum;
}

static size_t get_path_prefix(const char *path, size_t pathlen, size_t maxlen)
{
	size_t i = pathlen;
	if (i > maxlen)
		i = maxlen;
	do {
		i--;
	} while (i > 0 && path[i] != '/');
	return i;
}

int write_tar_entry(
		const char *path, size_t pathlen,
		unsigned int mode, const void *buffer, unsigned long size)
{
	struct ustar_header header;
	struct strbuf ext_header = STRBUF_INIT;
	int err = 0;

	memset(&header, 0, sizeof(header));

	if (!path) {
		*header.typeflag = TYPEFLAG_EXT_HEADER;
		mode = 0100666;
		sprintf(header.name, "%s.paxheader", "here_was_sha1_to_hex");
	} else {
		if (S_ISDIR(mode)) {
			*header.typeflag = TYPEFLAG_DIR;
			mode = (mode | 0777) & ~tar_umask;
		} else if (S_ISLNK(mode)) {
			*header.typeflag = TYPEFLAG_LNK;
			mode |= 0777;
		} else if (S_ISREG(mode)) {
			*header.typeflag = TYPEFLAG_REG;
			mode = (mode | ((mode & 0100) ? 0777 : 0666)) & ~tar_umask;
		} else {
            fprintf(stderr, "unsuppoerted file mode: 0%o\n", mode);
            return -1;
		}
		if (pathlen > sizeof(header.name)) {
			size_t plen = get_path_prefix(path, pathlen,
					sizeof(header.prefix));
			size_t rest = pathlen - plen - 1;
			if (plen > 0 && rest <= sizeof(header.name)) {
				memcpy(header.prefix, path, plen);
				memcpy(header.name, path + plen + 1, rest);
			} else {
				sprintf(header.name, "%s.data",
				        "here_was_sha1_to_hex");
				strbuf_append_ext_header(&ext_header, "path",
						path, pathlen);
			}
		} else
			memcpy(header.name, path, pathlen);
	}

	if (S_ISLNK(mode) && buffer) {
		if (size > sizeof(header.linkname)) {
			sprintf(header.linkname, "see %s.paxheader",
			        "here_was_sha1_to_hex");
			strbuf_append_ext_header(&ext_header, "linkpath",
			                         buffer, size);
		} else
			memcpy(header.linkname, buffer, size);
	}

	sprintf(header.mode, "%07o", mode & 07777);
	sprintf(header.size, "%011lo", S_ISREG(mode) ? size : 0);
	sprintf(header.mtime, "%011lo", (unsigned long) 0 /* XXX */);

	sprintf(header.uid, "%07o", 0);
	sprintf(header.gid, "%07o", 0);
	strlcpy(header.uname, "root", sizeof(header.uname));
	strlcpy(header.gname, "root", sizeof(header.gname));
	sprintf(header.devmajor, "%07o", 0);
	sprintf(header.devminor, "%07o", 0);

	memcpy(header.magic, "ustar", 6);
	memcpy(header.version, "00", 2);

	sprintf(header.chksum, "%07o", ustar_header_chksum(&header));

	if (ext_header.len > 0) {
		err = write_tar_entry(NULL, 0, 0, ext_header.buf,
				ext_header.len);
		if (err)
			return err;
	}
	strbuf_release(&ext_header);
	write_blocked(&header, sizeof(header));
	if (S_ISREG(mode) && buffer && size > 0)
		write_blocked(buffer, size);
	return err;
}


#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

void vreportf(const char *prefix, const char *err, va_list params) {
        char msg[4096];
        vsnprintf(msg, sizeof(msg), err, params);
        fprintf(stderr, "%s%s\n", prefix, msg);
}


void __attribute__((noreturn)) die_errno(const char *fmt, ...) {
    va_list params;
    va_start(params, fmt);
    vreportf("error: ", fmt, params);
    va_end(params);
    perror("");
    exit(1);
}

ssize_t xwrite(int fd, const void *buf, size_t len) {
        ssize_t nr;
        while (1) {
                nr = write(fd, buf, len);
                if ((nr < 0) && (errno == EAGAIN || errno == EINTR))
                        continue;
                return nr;
        }
}


ssize_t write_in_full(int fd, const void *buf, size_t count) {
        const char *p = buf;
        ssize_t total = 0;

        while (count > 0) {
                ssize_t written = xwrite(fd, p, count);
                if (written < 0)
                        return -1;
                if (!written) {
                        errno = ENOSPC;
                        return -1;
                }
                count -= written;
                p += written;
                total += written;
        }

        return total;
}



/*
 * Some cases use stdio, but want to flush after the write
 * to get error handling (and to get better interactive
 * behaviour - not buffering excessively).
 *
 * Of course, if the flush happened within the write itself,
 * we've already lost the error code, and cannot report it any
 * more. So we just ignore that case instead (and hope we get
 * the right error code on the flush).
 *
 * If the file handle is stdout, and stdout is a file, then skip the
 * flush entirely since it's not needed.
 */
void maybe_flush_or_die(FILE *f, const char *desc)
{
	static int skip_stdout_flush = -1;
	struct stat st;
	char *cp;

	if (f == stdout) {
		if (skip_stdout_flush < 0) {
			cp = getenv("GIT_FLUSH");
			if (cp)
				skip_stdout_flush = (atoi(cp) == 0);
			else if ((fstat(fileno(stdout), &st) == 0) &&
				 S_ISREG(st.st_mode))
				skip_stdout_flush = 1;
			else
				skip_stdout_flush = 0;
		}
		if (skip_stdout_flush && !ferror(f))
			return;
	}
	if (fflush(f)) {
		/*
		 * On Windows, EPIPE is returned only by the first write()
		 * after the reading end has closed its handle; subsequent
		 * write()s return EINVAL.
		 */
		if (errno == EPIPE || errno == EINVAL)
			exit(0);
		die_errno("write failure on '%s'", desc);
	}
}

void fsync_or_die(int fd, const char *msg)
{
	if (fsync(fd) < 0) {
		die_errno("fsync error on '%s'", msg);
	}
}

void write_or_die(int fd, const void *buf, size_t count)
{
	if (write_in_full(fd, buf, count) < 0) {
		if (errno == EPIPE)
			exit(0);
		die_errno("write error");
	}
}

int write_or_whine_pipe(int fd, const void *buf, size_t count, const char *msg)
{
	if (write_in_full(fd, buf, count) < 0) {
		if (errno == EPIPE)
			exit(0);
		fprintf(stderr, "%s: write error (%s)\n",
			msg, strerror(errno));
		return 0;
	}

	return 1;
}

int write_or_whine(int fd, const void *buf, size_t count, const char *msg)
{
	if (write_in_full(fd, buf, count) < 0) {
		fprintf(stderr, "%s: write error (%s)\n",
			msg, strerror(errno));
		return 0;
	}

	return 1;
}

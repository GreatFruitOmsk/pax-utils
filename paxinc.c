/*
 * Copyright 2003-2006 Gentoo Foundation
 * Distributed under the terms of the GNU General Public License v2
 * $Header: /var/cvsroot/gentoo-projects/pax-utils/paxinc.c,v 1.4 2006/01/14 01:39:55 vapier Exp $
 *
 * Copyright 2005-2006 Ned Ludd        - <solar@gentoo.org>
 * Copyright 2005-2006 Mike Frysinger  - <vapier@gentoo.org>
 */

/* stick common symbols here that are needed by paxinc.h */

#define IN_paxinc
#include "paxinc.h"

#define argv0 "paxinc"

char do_reverse_endian;

/* some of this ar code was taken from busybox */

#define AR_MAGIC "!<arch>"
#define AR_MAGIC_SIZE (sizeof(AR_MAGIC)-1) /* dont count null byte */
archive_handle *ar_open_fd(const char *filename, int fd)
{
	static archive_handle ret;
	char buf[AR_MAGIC_SIZE];

	ret.filename = filename;
	ret.fd = fd;
	ret.skip = 0;

	if (read(ret.fd, buf, AR_MAGIC_SIZE) != AR_MAGIC_SIZE)
		return NULL;
	if (strncmp(buf, AR_MAGIC, AR_MAGIC_SIZE))
		return NULL;

	return &ret;
}
archive_handle *ar_open(const char *filename)
{
	int fd;
	archive_handle *ret;

	if ((fd=open(filename, O_RDONLY)) == -1)
		err("Could not open '%s'", filename);

	ret = ar_open_fd(filename, fd);
	if (ret == NULL)
		close(fd);

	return ret;
}

archive_member *ar_next(archive_handle *ar)
{
	char *s;
	size_t len;
	static archive_member ret;

	if (ar->skip && lseek(ar->fd, ar->skip, SEEK_CUR) == -1) {
close_and_ret:
		close(ar->fd);
		return NULL;
	}

	if (read(ar->fd, ret.buf.raw, sizeof(ret.buf.raw)) != sizeof(ret.buf.raw))
		goto close_and_ret;

	/* ar header starts on an even byte (2 byte aligned)
	 * '\n' is used for padding */
	if (ret.buf.raw[0] == '\n') {
		memmove(ret.buf.raw, ret.buf.raw+1, 59);
		read(ar->fd, ret.buf.raw+59, 1);
	}

	if ((ret.buf.formated.magic[0] != '`') || (ret.buf.formated.magic[1] != '\n')) {
		warn("Invalid ar entry");
		goto close_and_ret;
	}

	if (ret.buf.formated.name[0] == '/' && ret.buf.formated.name[1] == '/') {
		warn("Sorry, long names not yet supported; output will be incomplete for %s", ar->filename);
		ar->skip = atoi(ret.buf.formated.size);
		return ar_next(ar);
	}

	len = strlen(ar->filename);
	assert(len < sizeof(ret.name)-sizeof(ret.buf.formated.name)-1);
	memcpy(ret.name, ar->filename, len);
	ret.name[len++] = ':';
	memcpy(ret.name+len, ret.buf.formated.name, sizeof(ret.buf.formated.name));
	if ((s=strchr(ret.name+len, '/')) != NULL)
		*s = '\0';
	else
		ret.name[len+sizeof(ret.name)-1] = '\0';
	ret.date = atoi(ret.buf.formated.date);
	ret.uid = atoi(ret.buf.formated.uid);
	ret.gid = atoi(ret.buf.formated.gid);
	ret.mode = strtol(ret.buf.formated.mode, NULL, 8);
	ret.size = atoi(ret.buf.formated.size);
	ar->skip = ret.size;

	return &ret;
}

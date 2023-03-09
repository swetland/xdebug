// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#include "xdebug.h"
#include "transport.h"
#include "arm-v7-debug.h"
#include "arm-v7-system-control.h"

void *load_file(const char *fn, size_t *_sz) {
	int fd;
	off_t sz;
	void *data = NULL;
	fd = open(fn, O_RDONLY);
	if (fd < 0) goto fail;
	sz = lseek(fd, 0, SEEK_END);
	if (sz < 0) goto fail;
	if (lseek(fd, 0, SEEK_SET)) goto fail;
	if ((data = malloc(sz + 4)) == NULL) goto fail;
	if (read(fd, data, sz) != sz) goto fail;
	*_sz = sz;
	return data;
fail:
	if (data) free(data);
	if (fd >= 0) close(fd);
	return NULL;
}

static long long now() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return ((long long) tv.tv_usec) + ((long long) tv.tv_sec) * 1000000LL;
}

int do_upload(DC* dc, CC* cc) {
	int status = DBG_ERR;
	const char* fn;
	uint32_t addr;
	uint32_t len;
	uint32_t sz;
	long long t0, t1;
	int fd, r;
	void *data;

	if (cmd_arg_str(cc, 1, &fn)) return DBG_ERR;
	if (cmd_arg_u32(cc, 2, &addr)) return DBG_ERR;	
	if (cmd_arg_u32(cc, 3, &len)) return DBG_ERR;
	if (addr & 3) {
		ERROR("address not word aligned\n");
		return DBG_ERR;
	}
	if ((fd = open(fn, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
		ERROR("cannot open '%s'\n", fn);
		return DBG_ERR;
	}
	sz = (len + 3) & ~3;

	if ((data = malloc(sz)) == NULL) {
		ERROR("out of memory\n");
		goto done;
	}

	INFO("upload: reading %d bytes...\n", sz);
	t0 = now();
	if (dc_mem_rd_words(dc, addr, sz / 4, data) < 0) {
		ERROR("failed to read data\n");
		goto done;
	}
	t1 = now();
	INFO("upload: %lld uS -> %lld B/s\n", (t1 - t0),
		(((long long)sz) * 1000000LL) / (t1 - t0));

	char *x = data;
	while (len > 0) {
		r = write(fd, x, sz);
		if (r < 0) {
			if (errno == EINTR) continue;
			ERROR("write error\n");
			goto done;
		}
		x += r;
		len -= r;
	}
	status = 0;
done:
	close(fd);
	if (data) free(data);
	return status;
}

int do_download(DC* dc, CC* cc) {
	const char* fn;
	uint32_t addr;
	void *data;
	size_t sz;
	long long t0, t1;

	if (cmd_arg_str(cc, 1, &fn)) return DBG_ERR;
	if (cmd_arg_u32(cc, 2, &addr)) return DBG_ERR;	

	if ((data = load_file(fn, &sz)) == NULL) {
		ERROR("cannot read '%s'\n", fn);
		return DBG_ERR;
	}
	sz = (sz + 3) & ~3;

	INFO("download: sending %ld bytes...\n", sz);
	t0 = now();
	if (dc_mem_wr_words(dc, addr, sz / 4, data) < 0) {
		ERROR("failed to write data\n");
		free(data);
		return DBG_ERR;
	}
	t1 = now();
	INFO("download: %lld uS -> %lld B/s\n", (t1 - t0), 
		(((long long)sz) * 1000000LL) / (t1 - t0));
	free(data);
	return 0;
}



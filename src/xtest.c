// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#include "xdebug.h"
#include "transport.h"
#include "arm-debug.h"
#include "cmsis-dap-protocol.h"

void MSG(uint32_t flags, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void dump(uint32_t* w, int count) {
	int n = 0;
	while (count > 0) {
		INFO(" %08x", *w++);
		count--;
		n++;
		if (n == 4) {
			INFO("\n");
			n = 0;
		}
	}
	if (n) INFO("\n");
}

int main(int argc, char **argv) {
	uint32_t n = 0;

	dctx_t* dc;
	if (dc_create(&dc, 0, 0) < 0) {
		return -1;
	}

	dc_set_clock(dc, 4000000);

	dc_attach(dc, 0, 0, &n);

#if 1
	// dump some info
	dc_dp_rd(dc, DP_DPIDR, &n);
	INFO("DP.DPIDR    %08x\n", n);
	dc_dp_rd(dc, DP_TARGETID, &n);
	INFO("DP.TARGETID %08x\n", n);
	dc_dp_rd(dc, DP_DLPIDR, &n);
	INFO("DP.DLPIDR   %08x\n", n);
	dc_ap_rd(dc, MAP_IDR, &n);
	INFO("MAP.IDR     %08x\n", n);
	dc_ap_rd(dc, MAP_CSW, &n);
	INFO("MAP.CSW     %08x\n", n);
	dc_ap_rd(dc, MAP_CFG, &n);
	INFO("MAP.CFG     %08x\n", n);
	dc_ap_rd(dc, MAP_CFG1, &n);
	INFO("MAP.CFG1    %08x\n", n);
	dc_ap_rd(dc, MAP_BASE, &n);
	INFO("MAP.BASE    %08x\n", n);
#endif

#if 0
	dc_mem_rd32(dc, 0, &n);
	INFO("%08x: %08x\n", 0, n);

	dc_mem_rd32(dc, 0xc0000000, &n);
	INFO("%08x: %08x\n", 0, n);

	dc_mem_rd32(dc, 0, &n);
	INFO("%08x: %08x\n", 0, n);

	unsigned addr = 0x00000000;
	for (unsigned a = addr; a < addr + 32; a += 4) {
		dc_mem_rd32(dc, a, &n);
		INFO("%08x: %08x\n", a, n);
	}
#endif

#if 0
	int r = dc_core_halt(dc);
	INFO("halt ? %d\n", r);
	r = dc_core_reg_rd(dc, 0, &n);
	INFO("r0 %x ? %d\n", n, r);
	r = dc_core_reg_wr(dc, 0, 0xe0a0b0c0);
	INFO("wr ? %d\n", r);
	r = dc_core_reg_rd(dc, 15, &n);
	INFO("r15 %x ? %d\n", n, r);
	r = dc_core_reg_rd(dc, 0, &n);
	INFO("r0 %x ? %d\n", n, r);
	r = dc_core_resume(dc);
	INFO("resume ? %d\n", r);
	r = dc_core_reg_rd(dc, 0, &n);
	INFO("r0 %x ? %d\n", n, r);
	r = dc_core_reg_rd(dc, 15, &n);
	INFO("r15 %x ? %d\n", n, r);
#endif

#if 0
	uint32_t w[1024];
	int r = dc_mem_rd_words(dc, 0x00000000, 1024, w);
	if (r == 0) {
		dump(w, 1024);
	}
	for (unsigned n = 0; n < 250; n++) {
		uint32_t w[1024];
		dc_mem_rd_words(dc, 0x00000000, 1024, w);
	}
#endif	
	return 0;
}


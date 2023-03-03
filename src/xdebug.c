// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "arm-debug.h"
#include "cmsis-dap-protocol.h"
#include "transport.h"

int main(int argc, char **argv) {
	uint32_t n = 0;

	dctx_t* dc;
	if (dc_create(&dc) < 0) {
		return -1;
	}

	dc_set_clock(dc, 1000000);

	dc_attach(dc, 0, 0, &n);

#if 1
	// dump some info
	dc_dp_rd(dc, DP_DPIDR, &n);
	printf("DP.DPIDR    %08x\n", n);
	dc_dp_rd(dc, DP_TARGETID, &n);
	printf("DP.TARGETID %08x\n", n);
	dc_dp_rd(dc, DP_DLPIDR, &n);
	printf("DP.DLPIDR   %08x\n", n);
	dc_ap_rd(dc, MAP_IDR, &n);
	printf("MAP.IDR     %08x\n", n);
	dc_ap_rd(dc, MAP_CSW, &n);
	printf("MAP.CSW     %08x\n", n);
	dc_ap_rd(dc, MAP_CFG, &n);
	printf("MAP.CFG     %08x\n", n);
	dc_ap_rd(dc, MAP_CFG1, &n);
	printf("MAP.CFG1    %08x\n", n);
	dc_ap_rd(dc, MAP_BASE, &n);
	printf("MAP.BASE    %08x\n", n);
#endif

	dc_mem_rd32(dc, 0, &n);
	printf("%08x: %08x\n", 0, n);

	unsigned addr = 0x00000000;
	for (unsigned a = addr; a < addr + 32; a += 4) {
		dc_mem_rd32(dc, a, &n);
		printf("%08x: %08x\n", a, n);
	}

	return 0;
}


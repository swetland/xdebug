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

	dc_attach(dc, 0, 0, &n);
	printf("IDCODE %08x\n", n);

	// If this is a RP2040, we need to connect in multidrop
	// mode before doing anything else.
	if (n == 0x0bc12477) {
		dc_dp_rd(dc, DP_TARGETID, &n);
		if (n == 0x01002927) { // RP2040
			dc_attach(dc, DC_MULTIDROP, 0x01002927, &n);
		}
	}

	// power up system & debug
	dc_dp_rd(dc, DP_CS, &n);
	printf("CTRL/STAT   %08x\n", n);
	dc_dp_wr(dc, DP_CS, DP_CS_CDBGPWRUPREQ | DP_CS_CSYSPWRUPREQ);
	dc_dp_rd(dc, DP_CS, &n);
	printf("CTRL/STAT   %08x\n", n);

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

	return 0;
}


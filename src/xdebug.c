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
	uint32_t n;

	dctx_t* dc;
	if (dc_create(&dc) < 0) {
		return -1;
	}

	dc_attach(dc);

	n = 0;
	dc_dp_rd(dc, DP_DPIDR, &n);
	printf("IDCODE %08x\n", n);

	dc_dp_rd(dc, DP_CS, &n);
	printf("CTRL/STAT %08x\n", n);

	dc_dp_wr(dc, DP_CS, DP_CS_CDBGPWRUPREQ | DP_CS_CSYSPWRUPREQ);

	dc_dp_rd(dc, DP_CS, &n);
	printf("CTRL/STAT %08x\n", n);
	return 0;
}


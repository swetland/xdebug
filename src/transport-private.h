// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#pragma once

#include "xdebug.h"

#include <stdint.h>

#include "usb.h"

struct debug_context {
	usb_handle* usb;
	unsigned status;

	// dap protocol info
	uint32_t max_packet_count;
	uint32_t max_packet_size;

	// dap internal state cache
	uint32_t cfg_idle;
	uint32_t cfg_wait;
	uint32_t cfg_match;
	uint32_t cfg_mask;

	// configured DP.SELECT register value
	uint32_t dp_select;
	// last known state of DP.SELECT on the target
	uint32_t dp_select_cache;

	// MAP cached state
	uint32_t map_csw_keep;
	uint32_t map_csw_cache;
	uint32_t map_tar_cache;

	// transfer queue state
	uint8_t txbuf[1024];
	uint32_t* rxptr[256];
	uint8_t *txnext;
	uint32_t** rxnext;
	uint32_t txavail;
	uint32_t rxavail;
	int qerror;
};

typedef struct debug_context DC;

#define DC_ATTACHED 0 // attached and ready to do txns
#define DC_FAILURE  1 // last txn failed, need to re-attach
#define DC_DETACHED 2 // have not yet attached
#define DC_UNCONFIG 3 // configure failed
#define DC_OFFLINE  4 // usb connection not available

#define INVALID 0xFFFFFFFFU


#if 0
static void dump(const char* str, const void* ptr, unsigned len) {
	const uint8_t* x = ptr;
	TRACE("%s", str);
	while (len > 0) {
		TRACE(" %02x", *x++);
		len--;
	}
	TRACE("\n");
}
#else
#define dump(...) do {} while (0)
#endif



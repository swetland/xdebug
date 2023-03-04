// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include "transport.h"
#include "transport-private.h"

#include "arm-debug.h"

static void dc_q_map_csw_wr(DC* dc, uint32_t val) {
	if (val != dc->map_csw_cache) {
		dc->map_csw_cache = val;
		dc_q_ap_wr(dc, MAP_CSW, val | dc->map_csw_keep);
	}
}

static void dc_q_map_tar_wr(DC* dc, uint32_t val) {
	if (val != dc->map_tar_cache) {
		dc->map_tar_cache = val;
		dc_q_ap_wr(dc, MAP_TAR, val);
	}
}


void dc_q_mem_rd32(DC* dc, uint32_t addr, uint32_t* val) {
	if (addr & 3) {
		dc->qerror = DC_ERR_BAD_PARAMS;
	} else {
		dc_q_map_csw_wr(dc, MAP_CSW_SZ_32 | MAP_CSW_INC_OFF | MAP_CSW_DEVICE_EN);
		dc_q_map_tar_wr(dc, addr);
		dc_q_ap_rd(dc, MAP_DRW, val);
	}
}

void dc_q_mem_wr32(DC* dc, uint32_t addr, uint32_t val) {
	if (addr & 3) {
		dc->qerror = DC_ERR_BAD_PARAMS;
	} else {
		dc_q_map_csw_wr(dc, MAP_CSW_SZ_32 | MAP_CSW_INC_OFF | MAP_CSW_DEVICE_EN);
		dc_q_map_tar_wr(dc, addr);
		dc_q_ap_wr(dc, MAP_DRW, val);
	}
}

int dc_mem_rd32(DC* dc, uint32_t addr, uint32_t* val) {
	dc_q_init(dc);
	dc_q_mem_rd32(dc, addr, val);
	return dc_q_exec(dc);
}

int dc_mem_wr32(DC* dc, uint32_t addr, uint32_t val) {
	dc_q_init(dc);
	dc_q_mem_wr32(dc, addr, val);
	return dc_q_exec(dc);
}



#if 0
int dc_mem_rd_words(dctx_t* dc, uint32_t addr, uint32_t num, uint32_t* ptr) {
	while (num > 0) {
		dc_q_mem_rd32(dc, addr, ptr);
		num--;
		addr += 4;
		ptr++;
	}
	return dc_q_exec(dc);
}

int dc_mem_wr_words(dctx_t* dc, uint32_t addr, uint32_t num, const uint32_t* ptr) {
	while (num > 0) {
		dc_q_mem_wr32(dc, addr, *ptr);
		num--;
		addr += 4;
		ptr++;
	}
	return dc_q_exec(dc);
}
#else
// some implementations support >10 bits, but 10 is the minimum required
// by spec (and some targets like rp2040 are limited to this)
#define WRAPSIZE 0x400
#define WRAPMASK (WRAPSIZE - 1)

int dc_mem_rd_words(dctx_t* dc, uint32_t addr, uint32_t num, uint32_t* ptr) {
	while (num > 0) {
		uint32_t xfer = (WRAPSIZE - (addr & WRAPMASK)) / 4;
		if (xfer > num) {
			xfer = num;
		}
		dc_q_init(dc);
		dc_q_map_csw_wr(dc, MAP_CSW_SZ_32 | MAP_CSW_INC_SINGLE | MAP_CSW_DEVICE_EN);
		dc_q_map_tar_wr(dc, addr);
		num -= xfer;
		addr += xfer * 4;
		while (xfer > 0) {
			dc_q_ap_rd(dc, MAP_DRW, ptr++);
			xfer--;
		}
		int r = dc_q_exec(dc);
		if (r != DC_OK) {
			return r;
		}
	}
	return DC_OK;
}

int dc_mem_wr_words(dctx_t* dc, uint32_t addr, uint32_t num, const uint32_t* ptr) {
	while (num > 0) {
		uint32_t xfer = (WRAPSIZE - (addr & WRAPMASK)) / 4;
		if (xfer > num) {
			xfer = num;
		}
		dc_q_init(dc);
		dc_q_map_csw_wr(dc, MAP_CSW_SZ_32 | MAP_CSW_INC_SINGLE | MAP_CSW_DEVICE_EN);
		dc_q_map_tar_wr(dc, addr);
		num -= xfer;
		addr += xfer * 4;
		while (xfer > 0) {
			dc_q_ap_wr(dc, MAP_DRW, *ptr++);
			xfer--;
		}
		int r = dc_q_exec(dc);
		if (r != DC_OK) {
			return r;
		}
	}
	return DC_OK;
}
#endif

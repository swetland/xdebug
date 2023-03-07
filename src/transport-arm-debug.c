// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include "transport.h"
#include "transport-private.h"

#include "arm-debug.h"
#include "arm-v7-debug.h"

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

void dc_q_mem_match32(DC* dc, uint32_t addr, uint32_t val) {
	if (addr & 3) {
		dc->qerror = DC_ERR_BAD_PARAMS;
	} else {
		dc_q_map_csw_wr(dc, MAP_CSW_SZ_32 | MAP_CSW_INC_OFF | MAP_CSW_DEVICE_EN);
		dc_q_map_tar_wr(dc, addr);
		dc_q_ap_match(dc, MAP_DRW, val);
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

int dc_core_check_halt(dctx_t* dc) {
	uint32_t val;
	int r;
	if ((r = dc_mem_rd32(dc, DHCSR, &val)) < 0) {
		return r;
	}
	if (val & DHCSR_S_HALT) {
		return 1;
	}
	return 0;
}

int dc_core_halt(DC* dc) {
	uint32_t val;
	int r;
	if ((r = dc_mem_rd32(dc, DHCSR, &val)) < 0) {
		return r;
	}
	if (val & DHCSR_C_DEBUGEN) {
		// preserve C_MASKINTS
		val &= DHCSR_C_MASKINTS;
	} else {
		// when setting C_DEBUGEN to 1 (from 0),
		// must write 0 to C_MASKINTS
		val = 0;
	}
	// set C_HALT and C_DEBUGEN
	val |= DHCSR_C_HALT | DHCSR_C_DEBUGEN | DHCSR_DBGKEY;
	if ((r = dc_mem_wr32(dc, DHCSR, val)) < 0) {
		return r;
	}
	for (unsigned n = 0; n < 64; n++) {
		if (dc_core_check_halt(dc) == 1) {
			return 0;
		}
	}
	return DC_ERR_TIMEOUT;
}

int dc_core_resume(DC* dc){
	uint32_t val;
	int r;
	if ((r = dc_mem_rd32(dc, DHCSR, &val)) < 0) {
		return r;
	}
	if (val & DHCSR_C_DEBUGEN) {
		// preserve C_MASKINTS
		val &= DHCSR_C_MASKINTS;
	} else {
		// when setting C_DEBUGEN to 1 (from 0),
		// must write 0 to C_MASKINTS
		val = 0;
	}
	// clear C_HALT
	val |= DHCSR_C_DEBUGEN | DHCSR_DBGKEY;
	if ((r = dc_mem_wr32(dc, DHCSR, val)) < 0) {
		return r;
	}
	for (unsigned n = 0; n < 64; n++) {
		if (dc_core_check_halt(dc) == 0) {
			return 0;
		}
	}
	return DC_ERR_TIMEOUT;
}

int dc_core_step(DC* dc) {
	uint32_t val;
	int r;
	if ((r = dc_mem_rd32(dc, DHCSR, &val)) < 0) {
		return r;
	}
	val &= (DHCSR_C_DEBUGEN | DHCSR_C_HALT | DHCSR_C_MASKINTS);
	val |= DHCSR_DBGKEY;

	if (!(val & DHCSR_C_HALT)) {
		val |= DHCSR_C_HALT | DHCSR_C_DEBUGEN;
		if ((r = dc_mem_wr32(dc, DHCSR, val)) < 0) {
			return r;
		}
	} else {
		val = (val & (~DHCSR_C_HALT)) | DHCSR_C_STEP;
		if ((r = dc_mem_wr32(dc, DHCSR, val)) < 0) {
			return r;
		}
	}
	return 0;
}

int dc_core_wait_halt(DC* dc) {
	uint32_t last = dc_get_attn_value(dc);
	uint32_t val;
	int r;
	for (;;) {
		if ((r = dc_mem_rd32(dc, DHCSR, &val)) < 0) {
			return r;
		}
		if (val & DHCSR_S_HALT) {
			return 0;
		}
		if (last != dc_get_attn_value(dc)) {
			return DC_ERR_INTERRUPTED;
		}
	}
	return 0;
}

static void dc_q_core_reg_rd(DC* dc, unsigned id, uint32_t* val) {
	dc_q_mem_wr32(dc, DCRSR, DCRSR_RD | (id & DCRSR_ID_MASK));
	dc_q_set_mask(dc, DHCSR_S_REGRDY);
	dc_q_mem_match32(dc, DHCSR, DHCSR_S_REGRDY);
	dc_q_mem_rd32(dc, DCRDR, val);
}
static void dc_q_core_reg_wr(DC* dc, unsigned id, uint32_t val) {
	dc_q_mem_wr32(dc, DCRDR, val);
	dc_q_mem_wr32(dc, DCRSR, DCRSR_WR | (id & DCRSR_ID_MASK));
	dc_q_set_mask(dc, DHCSR_S_REGRDY);
	dc_q_mem_match32(dc, DHCSR, DHCSR_S_REGRDY);
}

int dc_core_reg_rd(DC* dc, unsigned id, uint32_t* val) {
	dc_q_init(dc);
	dc_q_core_reg_rd(dc, id, val);
	return dc_q_exec(dc);
}
int dc_core_reg_wr(DC* dc, unsigned id, uint32_t val) {
	dc_q_init(dc);
	dc_q_core_reg_wr(dc, id, val);
	return dc_q_exec(dc);
}
int dc_core_reg_rd_list(DC* dc, uint32_t* id, uint32_t* val, unsigned count) {
	dc_q_init(dc);
	while (count > 0) {
		dc_q_core_reg_rd(dc, *id++, val++);
		count--;
	}
	return dc_q_exec(dc);
}

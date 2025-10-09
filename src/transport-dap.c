// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "usb.h"
#include "arm-debug.h"
#include "cmsis-dap-protocol.h"
#include "transport.h"
#include "transport-private.h"

uint32_t dc_flags(dctx_t* dc, uint32_t clr, uint32_t set) {
	dc->flags = (dc->flags & (~clr)) | set;
	return dc->flags;
}

void dc_interrupt(DC *dc) {
	dc->attn++;
}

uint32_t dc_get_attn_value(DC *dc) {
	return dc->attn;
}

void dc_set_status(DC* dc, uint32_t status) {
	dc->status = status;
	if (dc->status_callback) {
		dc->status_callback(dc->status_cookie, status);
	}
}

static void usb_failure(DC* dc, int status) {
	ERROR("usb_failure status %d usb %p\n", status, dc->usb);
	if (dc->usb != NULL) {
		usb_close(dc->usb);
		dc->usb = NULL;
	}
	dc_set_status(dc, DC_OFFLINE);
}

static int dap_get_info(DC* dc, unsigned di, void *out, unsigned minlen, unsigned maxlen) {
	uint8_t	buf[256 + 2];
	buf[0] = DAP_Info;
	buf[1] = di;
	int r;
	if ((r = usb_write(dc->usb, buf, 2)) != 2) {
		if (r < 0) {
			usb_failure(dc, r);
		}
		return DC_ERR_IO;
	}
	int sz = usb_read(dc->usb, buf, 256 + 2);
	if ((sz < 2) || (buf[0] != DAP_Info)) {
		if (sz < 0) {
			usb_failure(dc, sz);
		}
		return DC_ERR_PROTOCOL;
	}
	if ((buf[1] < minlen) || (buf[1] > maxlen)) {
		return DC_ERR_PROTOCOL;
	}
	memcpy(out, buf + 2, buf[1]);
	return buf[1];
}

static int dap_cmd(DC* dc, const void* tx, unsigned txlen, void* rx, unsigned rxlen) {
	uint8_t cmd = ((const uint8_t*) tx)[0];
	dump("TX>", tx, txlen);
	int r;
	if ((r = usb_write(dc->usb, tx, txlen)) != txlen) {
		ERROR("dap_cmd(0x%02x): usb write error\n", cmd);
		if (r < 0) {
			usb_failure(dc, r);
		}
		return DC_ERR_IO;
	}
	int sz = usb_read(dc->usb, rx, rxlen);
	if (sz < 1) {
		ERROR("dap_cmd(0x%02x): usb read error\n", cmd);
		if (sz < 0) {
			usb_failure(dc, sz);
		}
		return DC_ERR_IO;
	}
	dump("RX>", rx, rxlen);
	if (((uint8_t*) rx)[0] != cmd) {
		ERROR("dap_cmd(0x%02x): unsupported (0x%02x)\n",
			cmd, ((uint8_t*) rx)[0]);
		return DC_ERR_UNSUPPORTED;
	}
	return sz;
}

static int dap_cmd_std(DC* dc, const char* name, uint8_t* io,
		       unsigned txlen, unsigned rxlen) {
	int r = dap_cmd(dc, io, txlen, io, rxlen);
	if (r < 0) {
		return r;
	}
	if (io[1] != 0) {
		ERROR("%s status 0x%02x\n", name, io[1]);
		return DC_ERR_REMOTE;
	}
	return 0;
}

static int dap_connect(DC* dc) {
	uint8_t io[2] = { DAP_Connect, PORT_SWD };
	int r = dap_cmd(dc, io, 2, io, 2);
	if (r < 0) {
		return r;
	}
	if (io[1] != PORT_SWD) {
		return DC_ERR_REMOTE;
	}
	return 0;
}

static int dap_swd_configure(DC* dc, unsigned cfg) {
	uint8_t io[2] = { DAP_SWD_Configure, cfg };
	return dap_cmd_std(dc, "dap_swd_configure()", io, 2, 2);
}

int dc_set_clock(DC* dc, uint32_t hz) {
	uint8_t io[5] = { DAP_SWJ_Clock,
		hz, hz >> 8, hz >> 16, hz >> 24 };
	return dap_cmd_std(dc, "dap_swj_clock()", io, 5, 2);
}

static int dap_xfer_config(DC* dc, unsigned idle, unsigned wait, unsigned match) {
	// clamp to allowed max values
	if (idle > 255) idle = 255;
	if (wait > 65535) wait = 65535;
	if (match > 65535) match = 65535;

	// do nothing if unchanged from last set values
	if ((dc->cfg_idle == idle) &&
		(dc->cfg_wait == wait) &&
		(dc->cfg_match == match)) {
		return 0;
	}

	// cache new values
	dc->cfg_idle = idle;
	dc->cfg_wait = wait;
	dc->cfg_match = match;

	// inform the probe
	uint8_t io[6] = { DAP_TransferConfigure, idle, wait, wait >> 8, match, match >> 8};
	return dap_cmd_std(dc, "dap_transfer_configure()", io, 6, 2); 
}

static void dc_q_clear(DC* dc) {
	dc->txnext = dc->txbuf + 3;
	dc->rxnext = dc->rxptr;
	dc->txavail = dc->max_packet_size - 3;
	dc->rxavail = dc->max_packet_size - 3;
	dc->qerror = 0;
	dc->txbuf[0] = DAP_Transfer;
	dc->txbuf[1] = 0; // Index 0 for SWD
	dc->txbuf[2] = 0; // Count 0 initially

	// TODO: less conservative mode: don't always invalidate
	dc->dp_select_cache = INVALID;
	dc->cfg_mask = INVALID;
	dc->map_csw_cache = INVALID;
	dc->map_tar_cache = INVALID;
}

static inline void _dc_q_init(DC* dc) {
	// no side-effects version for use from dc_attach(), etc
	dc_q_clear(dc);
}

void dc_q_init(DC* dc) {
	// TODO: handle error cleanup, re-attach, etc
	dc_q_clear(dc);

	if ((dc->status == DC_DETACHED) && (dc->flags & DCF_AUTO_ATTACH)) {
		INFO("attach: auto\n");
		dc->qerror = dc_attach(dc, 0, 0, 0);
	}
}

// unpack the status bits into a useful status code
static int dc_decode_status(unsigned n) {
	unsigned ack = n & RSP_ACK_MASK;
	if (n & RSP_ProtocolError) {
		ERROR("DAP SWD Parity Error\n");
		return DC_ERR_SWD_PARITY;
	}
	switch (ack) {
	case RSP_ACK_OK:
		break;
	case RSP_ACK_WAIT:
		ERROR("DAP SWD WAIT (Timeout)\n");
		return DC_ERR_TIMEOUT;
	case RSP_ACK_FAULT:
		ERROR("DAP SWD FAULT\n");
		return DC_ERR_SWD_FAULT;
	case RSP_ACK_MASK: // all bits set
		ERROR("DAP SWD SILENT\n");
		return DC_ERR_SWD_SILENT;
	default:
		ERROR("DAP SWD BOGUS %x\n", ack);
		return DC_ERR_SWD_BOGUS;
	}
	if (n & RSP_ValueMismatch) {
		ERROR("DAP Value Mismatch\n");
		return DC_ERR_MATCH;
	}
	return DC_OK;
}

// this internal version is called from the "public" dc_q_exec
// as well as when we need to flush outstanding txns before
// continuing to queue up more
static int _dc_q_exec(DC* dc) {
	// if we're already in error, don't generate more usb traffic
	if (dc->qerror) {
		int r = dc->qerror;
		dc_q_clear(dc);
		return r;
	}
	// if we have no work to do, succeed
	if (dc->txbuf[2] == 0) {
		return 0;
	}
	int sz = dc->txnext - dc->txbuf;
	dump("TX>", dc->txbuf, sz);
	int n = usb_write(dc->usb, dc->txbuf, sz);
	if (n != sz) {
		ERROR("dc_q_exec() usb write error\n");
		if (n < 0) {
			usb_failure(dc, n);
		}
		return DC_ERR_IO;
	}
	sz = 3 + (dc->rxnext - dc->rxptr) * 4;
	uint8_t rxbuf[1024];
	memset(rxbuf, 0xEE, 1024); // DEBUG
	n = usb_read(dc->usb, rxbuf, sz);
	if (n < 0) {
		ERROR("dc_q_exec() usb read error\n");
		usb_failure(dc, n);
		return DC_ERR_IO;
	}
	dump("RX>", rxbuf, sz);
	if ((n < 3) || (rxbuf[0] != DAP_Transfer)) {
		ERROR("dc_q_exec() bad response\n");
		return DC_ERR_PROTOCOL;
	}
	int r = dc_decode_status(rxbuf[2]);
	if (r == DC_OK) {
		// how many response words available?
		n = (n - 3) / 4;
		uint8_t* rxptr = rxbuf + 3;
		for (unsigned i = 0; i < n; i++) {
			memcpy(dc->rxptr[i], rxptr, 4);
			rxptr += 4;
		}
	}

	dc_q_clear(dc);
	return r;
}

// internal use only -- queue raw dp reads and writes
// these do not check req for correctness
static void dc_q_raw_rd(DC* dc, unsigned req, uint32_t* val) {
	if ((dc->txavail < 1) || (dc->rxavail < 4)) {
		// exec q to make space for more work,
		// but if there's an error, latch it
		// so we don't send any further txns
		if ((dc->qerror = _dc_q_exec(dc)) != DC_OK) {
			return;
		}
	}
	dc->txnext[0] = req;
	dc->rxnext[0] = val;
	dc->txnext += 1;
	dc->rxnext += 1;
	dc->txbuf[2] += 1;
	dc->txavail -= 1;
	dc->rxavail -= 4;
}

static void dc_q_raw_wr(DC* dc, unsigned req, uint32_t val) {
	if (dc->txavail < 5) {
		// exec q to make space for more work,
		// but if there's an error, latch it
		// so we don't send any further txns
		if ((dc->qerror = _dc_q_exec(dc)) != DC_OK) {
			return;
		}
	}
	dc->txnext[0] = req;
	memcpy(dc->txnext + 1, &val, 4);
	dc->txnext += 5;
	dc->txavail -= 5;
	dc->txbuf[2] += 1;
}

// adjust DP.SELECT for desired DP access, if necessary
void dc_q_dp_sel(DC* dc, uint32_t dpaddr) {
	// DP address is BANK:4 REG:4
	if (dpaddr & 0xFFFFFF03U) {
		ERROR("invalid DP addr 0x%08x\n", dpaddr);
		dc->qerror = DC_ERR_FAILED;
		return;
	}
	// only register 4 cares about the value of DP.SELECT.DPBANK
	// so do nothing unless we're setting a register 4 variant
	if ((dpaddr & 0xF) != 0x4) {
		if (dc->dp_version < 3) return;
	}
	uint32_t mask = DP_SELECT_DPBANK(0xFU);
	uint32_t addr = DP_SELECT_DPBANK((dpaddr >> 4));
	uint32_t select = (dc->dp_select & mask) | addr;
	if (select != dc->dp_select_cache) {
		dc->dp_select_cache = select;
		dc_q_raw_wr(dc, XFER_DP | XFER_WR | DP_SELECT, select);
	}
}

// adjust DP.SELECT for desired AP access, if necessary
void dc_q_ap_sel(DC* dc, uint32_t apaddr) {
	//DEBUG("APADDR %08x\n", apaddr);
	uint32_t select;
	if (dc->dp_version < 3) {
		// AP address is AP:8 BANK:4 REG:4
		if (apaddr & 0xFFFF0003U) {
			ERROR("invalid DP addr 0x%08x\n", apaddr);
			dc->qerror = DC_ERR_FAILED;
			return;
		}
		select =
			DP_SELECT_AP((apaddr & 0xFF00U) << 16) |
			DP_SELECT_APBANK(apaddr >> 4);
	} else {
		// in v3 SELECT is just a linear address,
		// except for bits 0-3 which are DPBANK
		select = apaddr & 0xFFFFFFF0;
	}

	// we always return DPBANK to 0 when adjusting AP & APBANK
	// since it preceeds an AP write which will need DPBANK at 0
	if (select != dc->dp_select_cache) {
		dc->dp_select_cache = select;
		dc_q_raw_wr(dc, XFER_DP | XFER_WR | DP_SELECT, select);
	}
}

// DP and AP reads and writes
// DP.SELECT will be adjusted as necessary to ensure proper addressing
void dc_q_dp_rd(DC* dc, unsigned dpaddr, uint32_t* val) {
	if (dc->qerror) return;
	dc_q_dp_sel(dc, dpaddr);
	dc_q_raw_rd(dc, XFER_DP | XFER_RD | (dpaddr & 0x0C), val);
}

void dc_q_dp_wr(DC* dc, unsigned dpaddr, uint32_t val) {
	if (dc->qerror) return;
	dc_q_dp_sel(dc, dpaddr);
	dc_q_raw_wr(dc, XFER_DP | XFER_WR | (dpaddr & 0x0C), val);
}

void dc_q_ap_rd(DC* dc, unsigned apaddr, uint32_t* val) {
	if (dc->qerror) return;
	dc_q_ap_sel(dc, apaddr);
	dc_q_raw_rd(dc, XFER_AP | XFER_RD | (apaddr & 0x0C), val);
}

void dc_q_ap_wr(DC* dc, unsigned apaddr, uint32_t val) {
	if (dc->qerror) return;
	dc_q_ap_sel(dc, apaddr);
	dc_q_raw_wr(dc, XFER_AP | XFER_WR | (apaddr & 0x0C), val);
}

void dc_q_set_mask(DC* dc, uint32_t mask) {
	if (dc->qerror) return;
	if (dc->cfg_mask == mask) return;
	dc->cfg_mask = mask;
	dc_q_raw_wr(dc, XFER_WR | XFER_MatchMask, mask);
}

void dc_set_match_retry(dctx_t* dc, unsigned num) {
	if (dc->qerror) return;
	dap_xfer_config(dc, dc->cfg_idle, dc->cfg_wait, num);
}

void dc_q_ap_match(DC* dc, unsigned apaddr, uint32_t val) {
	if (dc->qerror) return;
	dc_q_ap_sel(dc, apaddr);
	dc_q_raw_wr(dc, XFER_AP | XFER_RD | XFER_ValueMatch | (apaddr & 0x0C), val);
}

void dc_q_dp_match(DC* dc, unsigned apaddr, uint32_t val) {
	if (dc->qerror) return;
	dc_q_ap_sel(dc, apaddr);
	dc_q_raw_wr(dc, XFER_DP | XFER_RD | XFER_ValueMatch | (apaddr & 0x0C), val);
}

// register access to the active memory ap
void dc_q_map_rd(DC* dc, unsigned offset, uint32_t* val) {
	dc_q_ap_rd(dc, dc->map_reg_base + offset, val);
}

void dc_q_map_wr(DC* dc, unsigned offset, uint32_t val) {
	dc_q_ap_wr(dc, dc->map_reg_base + offset, val);
}

void dc_q_map_match(DC* dc, unsigned offset, uint32_t val) {
	dc_q_ap_match(dc, dc->map_reg_base + offset, val);
}


// write to ABORT, which should never cause a fault
static int _dc_wr_abort(DC* dc, uint32_t val) {
	_dc_q_init(dc);
	dc_q_raw_wr(dc, XFER_DP | XFER_WR | XFER_00, val);
	return _dc_q_exec(dc);
}

// the public dc_q_exec() is called from higher layers
int dc_q_exec(DC* dc) {
	int r = _dc_q_exec(dc);
	if (r == DC_ERR_SWD_FAULT) {
		// clear all sticky errors
		if (_dc_wr_abort(dc, DP_ABORT_ALLCLR) < 0) {
			dc_set_status(dc, DC_DETACHED);
			DEBUG("dc_q_exec() failed (%d)\n", r);
		} else {
			DEBUG("dc_q_exec() failed (%d) and cleared\n", r);
		}
	}
	return r;
}


// convenience wrappers for single reads and writes
int dc_dp_rd(DC* dc, unsigned dpaddr, uint32_t* val) {
	dc_q_init(dc);
	dc_q_dp_rd(dc, dpaddr, val);
	return dc_q_exec(dc);
}
int dc_dp_wr(DC* dc, unsigned dpaddr, uint32_t val) {
	dc_q_init(dc);
	dc_q_dp_wr(dc, dpaddr, val);
	return dc_q_exec(dc);
}
int dc_ap_rd(DC* dc, unsigned apaddr, uint32_t* val) {
	dc_q_init(dc);
	dc_q_ap_rd(dc, apaddr, val);
	return dc_q_exec(dc);
}
int dc_ap_wr(DC* dc, unsigned apaddr, uint32_t val) {
	dc_q_init(dc);
	dc_q_ap_wr(dc, apaddr, val);
	return dc_q_exec(dc);
}

// SWD Attach Sequence:
// 1. Send >50 1s and then the JTAG to SWD escape code
//    (in case this is a JTAG-SWD DAP in JTAG mode)
// 2. Send >8 1s and then the Selection Alert Sequence
//    and then the SWD Activation Code
//    (in case this is a SWD v2 DAP in Dormant State)
// 3. Send >50 1s and then 4 0s -- the Line Reset Sequence
// 4. If multidrop, issue a write to DP.TARGETSEL, but
//    ignore the ACK response
// 5. Issue a read from DP.IDR

static uint8_t attach_cmd[54] = {
	DAP_SWD_Sequence, 5,

	//    [--- 64 1s ----------------------------------]
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	//    [JTAG2SWD]  [- 16 1s ]  [---------------------
	0x00, 0x9E, 0xE7, 0xFF, 0xFF, 0x92, 0xF3, 0x09, 0x62,
	//    ----- Selection Alert Sequence ---------------
	0x00, 0x95, 0x2D, 0x85, 0x86, 0xE9, 0xAF, 0xDD, 0xE3,
        //    ---------------------]  [Act Code]  [---------
	0x00, 0xA2, 0x0E, 0xBC, 0x19, 0xA0, 0xF1, 0xFF, 0xFF,
	//    ----- Line Reset Sequence -------]
	0x30, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F,

	//    WR DP TARGETSEL
	0x08, 0x99,
	//    5 bits idle
	0x85,
	//    WR VALUE:32, PARTY:1, ZEROs:7
	0x28, 0x00, 0x00, 0x00, 0x00, 0x00
};

static int _dc_attach(DC* dc, unsigned flags, uint32_t tgt, uint32_t* idcode) {
	uint8_t rsp[3];

	if (flags & DC_MULTIDROP) {
		// Copy and patch the attach sequence to include
		// the DP.TARGETSEL write and insert the target
		// id and parity
		uint8_t cmd[54];
		memcpy(cmd, attach_cmd, 54);
		cmd[1] = 8;
		memcpy(cmd + 49, &tgt, sizeof(tgt));
		cmd[53] = __builtin_parity(tgt);
		dap_cmd(dc, cmd, 54, rsp, 3);
	} else {
		// use the common part of the attach sequence, as-is
		dap_cmd(dc, attach_cmd, 45, rsp, 2);
	}

	// Issue a bare DP.IDR read, as required after a line reset
	// or line reset + target select
	_dc_q_init(dc);
	dc_q_raw_rd(dc, XFER_DP | XFER_RD | XFER_00, idcode);
	// Writes to ABORT will not cause a fault.  Clear any faults now.
	dc_q_raw_wr(dc, XFER_DP | XFER_WR | XFER_00, DP_ABORT_ALLCLR);
	int r = _dc_q_exec(dc);
	DEBUG("attach status %d\n", r);
	return r;
}

#define CSI_ICTRL	0xF00 // Integration Mode Control
#define CSI_CLAIMSET	0xFA0
#define CSI_CLAIMCLR	0xFA4
#define CSI_DEVAFF0	0xFA8
#define CSI_DEVAFF1	0xFAC
#define CSI_LAR		0xFB0 // Lock Access
#define CIS_LSR		0xFB4 // Lock Status
#define CSI_AUTHSTATUS	0xFB8
#define CSI_DEVARCH	0xFBC
#define CSI_DEVARCH_ARCHITECT(n) ((n) >> 21)
#define CSI_DEVARCH_PRESENT(n) (((n) >> 20) & 1)
#define CSI_DEVARCH_REVISION(n) (((n) >> 16) & 0xF)
#define CSI_DEVARCH_ARCHID(n) ((n) & 0xFFFF)
#define CSI_DEVID2	0xFC0
#define CSI_DEVID1	0xFC4
#define CSI_DEVID	0xFC8 // Device ID
#define CSI_DEVTYPE	0xFCC // Device Type

// per ADIv6, CoreSight ROM Table has:
// CIDR1.CLASS = 0x9
// DEVARCH.ARCHID = 0x0AF7
// DEVARCH.REVISION = 0
// DEVARCH.PRESENT = 1
// DEVARCH.ARCHITECT = 0x23B (ARM)

// DEVTYPE = 0 (misc/other)

// DEVID.CP (6)  1 == COM Port at D00-D7C
// DEVID.PRR (5) 1 == Power Request Functionality Included
// DEVID.SYSMEM (4) 0 == Debug Only, 1 = System Memory Present  (deprecated)
// DEVID.FORMAT (3:0) 0 == 32bit, 1 == 64bit, >1 == reserved

// 0000 00 af7 misc/other ROM
// 2000 00 a17 misc/other MAPv2
// 4000 00 a17 misc/other MAPv2
// 6000
// 7000 12 000 trace link funnel
// 8000 11 000 trace sink port
// 9000 14 a14 trace sink rsvd
// a000 00 a17 misc/other MAPv2

int dump_rom_table(DC* dc, uint32_t base, int rt) {
	uint32_t cidr[4];
	uint32_t pidr[8];
	uint32_t memtype, devtype, devarch;
	_dc_q_init(dc);
	dc_q_ap_rd(dc, 0xFF0, cidr + 0);
	dc_q_ap_rd(dc, 0xFF4, cidr + 1);
	dc_q_ap_rd(dc, 0xFF8, cidr + 2);
	dc_q_ap_rd(dc, 0xFFC, cidr + 3);
	dc_q_ap_rd(dc, 0xFE0, pidr + 0);
	dc_q_ap_rd(dc, 0xFE4, pidr + 1);
	dc_q_ap_rd(dc, 0xFE8, pidr + 2);
	dc_q_ap_rd(dc, 0xFEC, pidr + 3);
	dc_q_ap_rd(dc, 0xFD0, pidr + 4);
	dc_q_ap_rd(dc, 0xFD4, pidr + 5);
	dc_q_ap_rd(dc, 0xFD8, pidr + 6);
	dc_q_ap_rd(dc, 0xFDC, pidr + 7);
	dc_q_ap_rd(dc, 0xFCC, &memtype);
	dc_q_ap_rd(dc, CSI_DEVTYPE, &devtype);
	dc_q_ap_rd(dc, CSI_DEVARCH, &devarch);
	int r = dc_q_exec(dc);

	if (r == 0) {
		if ((cidr[3] != 0xB1) || (cidr[2] != 0x05) ||
			(cidr[1] & 0xFFFFFF0F) || (cidr[0] != 0x0D)) {
			DEBUG("%08x: CIDR %02x %02x %02x %02x ???\n", base,
				cidr[0], cidr[1], cidr[2], cidr[3]);
		} else {
			DEBUG("%08x: CIDR %02x\n", base, cidr[1]);
			if (cidr[1] == 0x90) {
				DEBUG("%08x: DEVTYPE %08x DEVARCH %08x\n",
					base, devtype, devarch);
			}
		}
		DEBUG("%08x: PIDR %02x %02x %02x %02x %02x %02x %02x %02x\n", base,
			pidr[0], pidr[1], pidr[2], pidr[3],
			pidr[4], pidr[5], pidr[6], pidr[7]);
		DEBUG("%08x: MEMTYPE %08x\n", base, memtype);

		if (rt) {
		for (unsigned a = 0; a < 32;a += 4 ) {
			unsigned v = 0xeeeeeeee;
			if ((dc_ap_rd(dc, a, &v) < 0) || (v == 0)) break;
			dump_rom_table(dc, v & 0xFFFFF000, 0);
		}
		}
	}
	return r;
}

// TARGETID -> JEP106 ContCode + IdentCode + Present
//    8      10
// CCCCIIIIIIIP
// ARM = 0x4,0x3B: 0x23B
// rPI = 0x9,0x27: 0x493  PN 0x01002=RP2040, 0x00040=RP2350

int dc_attach(DC* dc, unsigned flags, unsigned tgt, uint32_t* idcode) {
	uint32_t n, nn;

	dc->dp_version = 0;
	dc->map_reg_base = 0;

	_dc_attach(dc, 0, 0, &n);

	dc->dp_version = (n >> 12) & 7;

	if (dc->dp_version == 3) {
		// todo: query ROM table
		dc->map_reg_base = 0x2D00;
	}

	INFO("attach: IDCODE %08x v%d\n", n, dc->dp_version);
	if (idcode != NULL) {
		*idcode = n;
	}

	dc_dp_rd(dc, DP_TARGETID, &nn);
	INFO("attach: TARGETID %08x\n", nn);

	//_dc_attach(dc, DC_MULTIDROP, 0x01002927, &nn);

	// If this is a RP2040, we need to connect in multidrop
	// mode before doing anything else.
	if ((n == 0x0bc12477) && (tgt == 0)) {
		dc_dp_rd(dc, DP_TARGETID, &n);
		if (n == 0x01002927) { // RP2040
			_dc_attach(dc, DC_MULTIDROP, 0x01002927, &n);
		}
	}

	//_dc_q_init(dc);
	//dc_q_dp_rd(dc, DP_CS, &n);
	//dc_q_exec(dc);
	//DEBUG("attach: CTRL/STAT   %08x\n", n);

	// clear all sticky errors
	_dc_q_init(dc);
	dc_q_dp_wr(dc, DP_ABORT, DP_ABORT_ALLCLR);
	dc_q_exec(dc);

	// power up and wait for ack
	_dc_q_init(dc);
	dc_q_set_mask(dc, DP_CS_CDBGPWRUPACK | DP_CS_CSYSPWRUPACK);
	dc_q_dp_wr(dc, DP_CS, DP_CS_CDBGPWRUPREQ | DP_CS_CSYSPWRUPREQ);
	dc_q_dp_match(dc, DP_CS, DP_CS_CDBGPWRUPACK | DP_CS_CSYSPWRUPACK);
	dc_q_dp_rd(dc, DP_CS, &n);
	if (dc->dp_version >= 3) {
		dc_q_dp_wr(dc, DP_SELECT1, 0);
	}
	dc_q_ap_rd(dc, dc->map_reg_base + MAP_CSW, &dc->map_csw_keep);
	dc_q_exec(dc);
	DEBUG("attach: CTRL/STAT   %08x\n", n);
	DEBUG("attach: MAP.CSW     %08x\n", dc->map_csw_keep);

	//preserving existing settings is insufficient
	//dc->map_csw_keep &= MAP_CSW_KEEP;
	//dc->map_csw_keep &= (~0x40000000);
	//dc->map_csw_keep = AHB_CSW_PROT_PRIV | AHB_CSW_MASTER_DEBUG;
	//dc->map_csw_keep = AHB_CSW_MASTER_DEBUG | (1U << 23);

	dc_set_status(dc, DC_ATTACHED);

#if 0
	if (dc->dp_version >= 3) {
		dump_rom_table(dc, 0, 1);
	}
#endif
	if (dc->flags & DCF_AUTO_CONFIG) {
		// ...
	}

	return 0;
}


static unsigned dc_vid = 0;
static unsigned dc_pid = 0;
static const char* dc_serialno = NULL;

void dc_require_vid_pid(unsigned vid, unsigned pid) {
	dc_vid = vid;
	dc_pid = pid;
}

void dc_require_serialno(const char* sn) {
	dc_serialno = sn;
}

static usb_handle* usb_connect(void) {
	return usb_open(dc_vid, dc_pid, dc_serialno);
}

static const char* di_name(unsigned n) {
	switch (n) {
	case DI_Vendor_Name: return "Vendor Name";
	case DI_Product_Name: return "Product Name";
	case DI_Serial_Number: return "Serial Number";
	case DI_Protocol_Version: return "Protocol Version";
	case DI_Target_Device_Vendor: return "Target Device Vendor";
	case DI_Target_Device_Name: return "Target Device Name";
	case DI_Target_Board_Vendor: return "Target Board Vendor";
	case DI_Target_Board_Name: return "Target Board Name";
	case DI_Product_Firmware_Version: return "Product Firmware Version";
	default: return "???";
	}
}

// setup a newly connected DAP device
static int dap_configure(DC* dc) {
	uint8_t buf[256 + 2];
	uint32_t n32;
	uint16_t n16;
	uint8_t n8;

	// invalidate cached state
	dc->cfg_idle = INVALID;
	dc->cfg_wait = INVALID;
	dc->cfg_match = INVALID;
	dc->cfg_mask = INVALID;

	dc->map_csw_keep = 0;
	dc->map_csw_cache = INVALID;
	dc->map_tar_cache = INVALID;

	// setup default packet limits
	dc->max_packet_count = 1;
	dc->max_packet_size = 64;

	// flush queue
	dc_q_clear(dc);

	buf[0] = DAP_Info;
	for (unsigned n = 0; n < 10; n++) {
		int sz = dap_get_info(dc, n, buf, 0, 255);
		if (sz > 0) {
			buf[sz] = 0;
			INFO("connect: %s: '%s'\n", di_name(n), (char*) buf);
		}
	}

	buf[0] = 0; buf[1] = 0;
	if (dap_get_info(dc, DI_Capabilities, buf, 1, 2) > 0) {
		INFO("connect: Capabilities:");
		if (buf[0] & I0_SWD) INFO(" SWD");
		if (buf[0] & I0_JTAG) INFO(" JTAG");
		if (buf[0] & I0_SWO_UART) INFO(" SWO(UART)");
		if (buf[0] & I0_SWO_Manchester) INFO(" SWO(Manchester)");
		if (buf[0] & I0_Atomic_Commands) INFO(" ATOMIC");
		if (buf[0] & I0_Test_Domain_Timer) INFO(" TIMER");
		if (buf[0] & I0_SWO_Streaming_Trace) INFO(" SWO(Streaming)");
		if (buf[0] & I0_UART_Comm_Port) INFO(" UART");
		if (buf[1] & I1_USB_COM_Port) INFO(" USBCOM");
		INFO("\n");
	}
	if (dap_get_info(dc, DI_UART_RX_Buffer_Size, &n32, 4, 4) == 4) {
		INFO("connect: UART RX Buffer Size: %u\n", n32);
	}
	if (dap_get_info(dc, DI_UART_TX_Buffer_Size, &n32, 4, 4) == 4) {
		INFO("connect: UART TX Buffer Size: %u\n", n32);
	}
	if (dap_get_info(dc, DI_SWO_Trace_Buffer_Size, &n32, 4, 4) == 4) {
		INFO("connect: SWO Trace Buffer Size: %u\n", n32);
	}
	if (dap_get_info(dc, DI_Max_Packet_Count, &n8, 1, 1) == 1) {
		dc->max_packet_count = n8;
	}
	if (dap_get_info(dc, DI_Max_Packet_Size, &n16, 2, 2) == 2) {
		dc->max_packet_size = n16;
	}
	INFO("connect: Max Packet Count: %u, Size: %u\n",
		dc->max_packet_count, dc->max_packet_size);
	if ((dc->max_packet_count < 1) || (dc->max_packet_size < 64)) {
		ERROR("dc_init() impossible packet configuration\n");
		return DC_ERR_PROTOCOL;
	}

	// invalidate register cache
	dc->dp_select_cache = INVALID;

	// clip to our buffer size
	if (dc->max_packet_size > 1024) {
		dc->max_packet_size = 1024;
	}

	dap_connect(dc);
	dap_swd_configure(dc, CFG_Turnaround_1);
	dap_xfer_config(dc, 8, 64, 64);
	return DC_OK;
}

static int dc_connect(DC* dc) {
	if ((dc->usb = usb_connect()) != NULL) {
		if (dap_configure(dc) == 0) {
			dc_set_status(dc, DC_DETACHED);
		} else {
			dc_set_status(dc, DC_UNCONFIG);
		}
		return 0;
	}
	return DC_ERR_FAILED;
}

int dc_create(DC** out, void (*cb)(void *cookie, uint32_t status), void *cookie) {
	DC* dc;

	if ((dc = calloc(1, sizeof(DC))) == NULL) {
		return DC_ERR_FAILED;
	}
	dc->status_callback = cb;
	dc->status_cookie = cookie;
	dc->flags = DCF_POLL; // | DCF_AUTO_ATTACH;
	*out = dc;
	dc_set_status(dc, DC_OFFLINE);
	dc_connect(dc);
	return 0;
}

int dc_periodic(DC* dc) {
	switch (dc->status) {
	case DC_OFFLINE:
		if (dc_connect(dc) < 0) {
			return 500;
		} else {
			return 100;
		}
	case DC_ATTACHED:
		if (dc->flags & DCF_POLL) {
			uint32_t n;
			int r = dc_dp_rd(dc, DP_CS, &n);
			if (r == DC_ERR_IO) {
				dc_set_status(dc, DC_OFFLINE);
				ERROR("offline\n");
			} else if (r < 0) {
				dc_set_status(dc, DC_DETACHED);
				ERROR("detached\n");
			}
		}
		return 100;
	case DC_FAILURE:
	case DC_UNCONFIG:
	case DC_DETACHED: {
		// ping the probe to see if USB is still connected
		uint8_t buf[256 + 2];
		dap_get_info(dc, DI_Protocol_Version, buf, 0, 255);
		return 500;
	}
	default:
		return 1000;
	}
}


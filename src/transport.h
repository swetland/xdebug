// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#pragma once

#include <stdint.h>


void dc_require_vid_pid(unsigned vid, unsigned pid);
void dc_require_serialno(const char* sn);

typedef struct debug_context dctx_t;
int dc_periodic(dctx_t* dc);

void dc_interrupt(dctx_t* dc);

#define DC_OK               0
#define DC_ERR_FAILED      -1  // generic internal failure
#define DC_ERR_BAD_PARAMS  -2  // Invalid parameters
#define DC_ERR_IO          -3  // IO error (USB read fail, etc)
#define DC_ERR_OFFLINE     -4  // IO error (USB device offline)
#define DC_ERR_PROTOCOL    -5  // Protocol error (bug in sw or fw)
#define DC_ERR_TIMEOUT     -6  // WAIT response from DP (retries insufficient)
#define DC_ERR_SWD_FAULT   -7  // FAULT response from DP
#define DC_ERR_SWD_PARITY  -8  // Corrupt Data
#define DC_ERR_SWD_SILENT  -9  // No Status Indicated
#define DC_ERR_SWD_BOGUS   -10 // Nonsensical Status Indicated
#define DC_ERR_MATCH       -11 // read match failure
#define DC_ERR_UNSUPPORTED -12 // unsupported operation
#define DC_ERR_REMOTE      -13 // failure from debug probe
#define DC_ERR_DETACHED    -14 // transport not connected to target
#define DC_ERR_BAD_STATE   -15
#define DC_ERR_INTERRUPTED -16

int dc_set_clock(dctx_t* dc, uint32_t hz);

// queue Debug Port reads and writes
// DP.SELECT will be updated as necessary
void dc_q_dp_rd(dctx_t* dc, unsigned dpaddr, uint32_t* val);
void dc_q_dp_wr(dctx_t* dc, unsigned dpaddr, uint32_t val);

// queue Access Port reads and writes
// DP.SELECT will be updated as necessary
void dc_q_ap_rd(dctx_t* dc, unsigned apaddr, uint32_t* val); 
void dc_q_ap_wr(dctx_t* dc, unsigned apaddr, uint32_t val);

// set the max retry count for match operations
void dc_set_match_retry(dctx_t* dc, unsigned num);

// set the mask pattern (in the probe, not the target)
void dc_q_set_mask(dctx_t* dc, uint32_t mask);

// try to read until (readval & mask) == val or timeout
void dc_q_ap_match(dctx_t* dc, unsigned apaddr, uint32_t val);
void dc_q_dp_match(dctx_t* dc, unsigned apaddr, uint32_t val);

// prepare for a set of transactions
void dc_q_init(dctx_t* dc);

// execute any outstanding transactions, return final status
int dc_q_exec(dctx_t* dc);

// convenince wrappers for a single read/write and then exec
int dc_dp_rd(dctx_t* dc, unsigned dpaddr, uint32_t* val);
int dc_dp_wr(dctx_t* dc, unsigned dpaddr, uint32_t val);
int dc_ap_rd(dctx_t* dc, unsigned apaddr, uint32_t* val);
int dc_ap_wr(dctx_t* dc, unsigned apaddr, uint32_t val);

// create debug connection
int dc_create(dctx_t** dc, void (*cb)(void *cookie, uint32_t status), void *cookie);

// status values
#define DC_ATTACHED 0 // attached and ready to do txns
#define DC_FAILURE  1 // last txn failed, need to re-attach
#define DC_DETACHED 2 // have not yet attached
#define DC_UNCONFIG 3 // configure failed
#define DC_OFFLINE  4 // usb connection not available

// attempt to attach to the debug target
int dc_attach(dctx_t* dc, unsigned flags, uint32_t tgt, uint32_t* idcode);
#define DC_MULTIDROP 1


void dc_q_mem_rd32(dctx_t* dc, uint32_t addr, uint32_t* val);
void dc_q_mem_wr32(dctx_t* dc, uint32_t addr, uint32_t val);
void dc_q_mem_match32(dctx_t* dc, uint32_t addr, uint32_t val);

int dc_mem_rd32(dctx_t* dc, uint32_t addr, uint32_t* val);
int dc_mem_wr32(dctx_t* dc, uint32_t addr, uint32_t val);

int dc_mem_rd_words(dctx_t* dc, uint32_t addr, uint32_t num, uint32_t* ptr);
int dc_mem_wr_words(dctx_t* dc, uint32_t addr, uint32_t num, const uint32_t* ptr);



int dc_core_halt(dctx_t* dc);
int dc_core_resume(dctx_t* dc);
int dc_core_step(dctx_t* dc);
int dc_core_wait_halt(dctx_t* dc);

int dc_core_reg_rd(dctx_t* dc, unsigned id, uint32_t* val);
int dc_core_reg_wr(dctx_t* dc, unsigned id, uint32_t val);

int dc_core_reg_rd_list(dctx_t* dc, uint32_t* id, uint32_t* val, unsigned count);

// 0 = no, 1 = yes, < 0 = error
int dc_core_check_halt(dctx_t* dc);


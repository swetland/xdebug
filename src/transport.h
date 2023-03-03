// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#pragma once

#include <stdint.h>

typedef struct debug_context dctx_t;

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
int dc_create(dctx_t** dc);

// attempt to attach to the debug target
int dc_attach(dctx_t* dc, unsigned flags, uint32_t tgt, uint32_t* idcode);

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

#define DC_MULTIDROP 1

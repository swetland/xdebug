// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#pragma once

#include <stdint.h>

typedef struct debug_context dctx_t;

// queue Debug Port reads and writes
// DP.SELECT will be updated as necessary
void dc_q_dp_rd(dctx_t* dc, unsigned dpaddr, uint32_t* val);
void dc_q_dp_wr(dctx_t* dc, unsigned dpaddr, uint32_t val);

// queue Access Port reads and writes
// DP.SELECT will be updated as necessary
void dc_q_ap_rd(dctx_t* dc, unsigned apaddr, uint32_t* val); 
void dc_q_ap_wr(dctx_t* dc, unsigned apaddr, uint32_t val);

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
int dc_attach(dctx_t* dc);


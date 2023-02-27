// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

// see: ARM IHI 0031G
//      Arm Debug Interface Architecture Specification
//      ADIv5.0 to ADIv5.2

#pragma once

// high nybble is banksel
#define DP_DPIDR                  0x00 // RO    Debug Port ID
#define DP_ABORT                  0x00 // WO 
#define DP_CS                     0x04 // RW    CTRL/STAT
#define DP_DLCR                   0x14 // RW    Data Link Control
#define DP_TARGETID               0x24 // RO v2
#define DP_DLPIDR                 0x34 // RO v2 Data Link Protocol ID
#define DP_EVENTSTAT              0x44 // RO v2
#define DP_SELECT                 0x08 // WO
#define DP_RESEND                 0x08 // RO v2 Return last AP or RDBUFF read data
#define DP_RDBUFF                 0x0C // RO
#define DP_TARGETSEL              0x0C // WO v2

#define DP_ABORT_DAPABORT         0x01U // Abort Current AP Txn
#define DP_ABORT_STKCMPCLR        0x02U // clear CS.STICKYCMP
#define DP_ABORT_STKERRCLR        0x04U // clear CS.STICKYERR
#define DP_ABORT_WDERRCLR         0x08U // clear CS.WDATAERR
#define DP_ABORT_ORUNERRCLR       0x10U // clear CS.STICKYORUN

#define DP_CS_ORUNDETECT          0x00000001U // RW
#define DP_CS_STICKYORUN          0x00000002U // RO/WI
#define DP_CS_MODE_MASK           0x0000000CU // RW
#define DP_CS_MODE_NORMAL         0x00000000U
#define DP_CS_MODE_PUSHED_VERIFY  0x00000004U
#define DP_CS_MODE_PUSHED_COMPARE 0x00000008U
#define DP_CS_MODE_RESERVED       0x0000000CU
#define DP_CS_STICKYCMP           0x00000010U // RO/WI for pushed ops
#define DP_CS_STICKYERR           0x00000020U // RO/WI error occurred in AP txn
#define DP_CS_READOK              0x00000040U // RO/WI last AP or RDBUFF RD was OK
#define DP_CS_WDATAERR            0x00000080U // RO/WI
#define DP_CS_MASKLANE(n)         (((n) & 0xFU) << 8) // RW
#define DP_CS_TRNCNT(n)           (((n) & 0xFFFU) << 12)
#define DP_CS_CDBGRSTREQ          0x04000000U // RW or RAZ/WI
#define DP_CS_CDBGRSTACK          0x08000000U // RO
#define DP_CS_CDBGPWRUPREQ        0x10000000U // RW
#define DP_CS_CDBGPWRUPACK        0x20000000U // RO
#define DP_CS_CSYSPWRUPREQ        0x40000000U // RW
#define DP_CS_CSYSPWRUPACK        0x80000000U // RO

#define DP_DLCR_TURNROUND_MASK    0x00000300U
#define DP_DLCR_TURNROUND_1       0x00000000U
#define DP_DLCR_TURNROUND_2       0x00000100U
#define DP_DLCR_TURNROUND_3       0x00000200U
#define DP_DLCR_TURNROUND_4       0x00000300U
#define DP_DLCR_MUST_BE_ONE       0x00000040U

#define DP_EVENTSTAT_EA           0x000000001U

#define DP_SELECT_DPBANK(n)       ((n) & 0xFU)
#define DP_SELECT_APBANK(n)       (((n) & 0xFU) << 4)
#define DP_SELECT_AP(n)           (((n) & 0xFFU) << 24)


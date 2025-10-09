// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

// see: ARM IHI 0031G
//      Arm Debug Interface Architecture Specification
//      ADIv5.0 to ADIv5.2

#pragma once

// high nybble is banksel
// it is ignored for some addresses (marked * below)
#define DP_DPIDR		0x00 //  RO v0 Debug Port ID
#define DP_DPIDR1		0x10 //  RO v3
#define DP_BASEPTR0		0x20 //  RO v3
#define DP_BASEPTR1		0x30 //  RO v3
#define DP_ABORT		0x00 // *WO v0
#define DP_CS			0x04 //  RW v1 CTRL/STAT
#define DP_DLCR			0x14 //  RW v1 Data Link Control
#define DP_TARGETID		0x24 //  RO v2
#define DP_DLPIDR		0x34 //  RO v2 Data Link Protocol ID
#define DP_EVENTSTAT		0x44 //  RO v2
#define DP_SELECT1		0x54 //  WO v3
#define DP_SELECT		0x08 // *WO v0
#define DP_RESEND		0x08 // *RO v2 Return last AP or RDBUFF read data
#define DP_RDBUFF		0x0C // *RO v0
#define DP_TARGETSEL		0x0C // *WO v2

#define DP_ABORT_DAPABORT	0x01U // Abort Current AP Txn
#define DP_ABORT_STKCMPCLR	0x02U // clear CS.STICKYCMP
#define DP_ABORT_STKERRCLR	0x04U // clear CS.STICKYERR
#define DP_ABORT_WDERRCLR	0x08U // clear CS.WDATAERR
#define DP_ABORT_ORUNERRCLR	0x10U // clear CS.STICKYORUN
#define DP_ABORT_ALLCLR		0x1EU

#define DP_CS_ORUNDETECT	0x00000001U // RW
#define DP_CS_STICKYORUN	0x00000002U // RO/WI
#define DP_CS_MODE_MASK		0x0000000CU // RW
#define DP_CS_MODE_NORMAL	0x00000000U
#define DP_CS_MODE_PUSHED_VFY	0x00000004U
#define DP_CS_MODE_PUSHED_CMP	0x00000008U
#define DP_CS_MODE_RESERVED	0x0000000CU
#define DP_CS_STICKYCMP		0x00000010U // RO/WI for pushed ops
#define DP_CS_STICKYERR		0x00000020U // RO/WI error occurred in AP txn
#define DP_CS_READOK		0x00000040U // RO/WI last AP or RDBUFF RD was OK
#define DP_CS_WDATAERR		0x00000080U // RO/WI
#define DP_CS_MASKLANE(n)	(((n) & 0xFU) << 8) // RW
#define DP_CS_TRNCNT(n)		(((n) & 0xFFFU) << 12)
#define DP_CS_CDBGRSTREQ	0x04000000U // RW or RAZ/WI
#define DP_CS_CDBGRSTACK	0x08000000U // RO
#define DP_CS_CDBGPWRUPREQ	0x10000000U // RW
#define DP_CS_CDBGPWRUPACK	0x20000000U // RO
#define DP_CS_CSYSPWRUPREQ	0x40000000U // RW
#define DP_CS_CSYSPWRUPACK	0x80000000U // RO

#define DP_DLCR_TURNROUND_MASK	0x00000300U
#define DP_DLCR_TURNROUND_1	0x00000000U
#define DP_DLCR_TURNROUND_2	0x00000100U
#define DP_DLCR_TURNROUND_3	0x00000200U
#define DP_DLCR_TURNROUND_4	0x00000300U
#define DP_DLCR_MUST_BE_ONE	0x00000040U

#define DP_EVENTSTAT_EA		0x000000001U

#define DP_SELECT_DPBANK(n)	((n) & 0xFU)
#define DP_SELECT_APBANK(n)	(((n) & 0xFU) << 4)
#define DP_SELECT_AP(n)		(((n) & 0xFFU) << 24)

// Memory AP

#define MAP_CSW			0x00 // RW Control/Status Word
#define MAP_TAR			0x04 // RW Transfer Address Reg
#define MAP_TAR_H		0x08 // RW Transfer Address Reg (HI)
#define MAP_DRW			0x0C // RW Data Read/Write
#define MAP_BD0			0x10 // RW Banked Data Reg 0
#define MAP_BD1			0x14
#define MAP_BD2			0x18
#define MAP_BD3			0x1C
#define MAP_MBT			0x20 // ?? Memory Barrier Transfer
#define MAP_T0TR		0x30 // RW Tag 0 Transfer Reg
#define MAP_CFG1		0xE0 // RO Config Register 1
#define MAP_BASE_H		0xF0 // RO Debug Base Addr (HI)
#define MAP_CFG			0xF4 // RO Config Register 0
#define MAP_BASE		0xF8 // RO Debug Base Addr
#define MAP_IDR			0xFC // RO Identification Reg

#define MAP_CFG_LD		0x04 // Large Data Extension (>32bit)
#define MAP_CFG_LA		0x02 // Large Addr Extension (>32bit)
#define MAP_CFG_BE		0x01 // Big Endian (obsolete in 5.2)

#define MAP_CSW_SZ_MASK		0x00000007U
#define MAP_CSW_SZ_8		0x00000000U
#define MAP_CSW_SZ_16		0x00000001U
#define MAP_CSW_SZ_32		0x00000002U // always supported
#define MAP_CSW_SZ_64		0x00000003U
#define MAP_CSW_SZ_128		0x00000004U
#define MAP_CSW_SZ_256		0x00000005U
#define MAP_CSW_INC_MASK	0x00000030U
#define MAP_CSW_INC_OFF		0x00000000U
#define MAP_CSW_INC_SINGLE	0x00000010U
#define MAP_CSW_INC_PACKED	0x00000020U
#define MAP_CSW_DEVICE_EN	0x00000040U // Enable MEM AP
#define MAP_CSW_BUSY		0x00000080U // Transfer In Progress
#define MAP_CSW_MODE_MASK	0x00000F00U
#define MAP_CSW_MODE_BASIC	0x00000000U
#define MAP_CSW_MODE_BARRIER	0x00000100U
#define MAP_CSW_TYPE_MASK	0x00007000U
#define MAP_CSW_MTE		0x00008000U
#define MAP_CSW_SEC_DBG_EN	0x00800000U // Secure Debug Enable
#define MAP_CSW_PROT_MASK	0x7F000000U
#define MAP_CSW_DBG_SW_EN	0x80000000U // Debug SW Access Enable

#define MAP_CSW_KEEP		0xFF00FF00U // preserve mode/type/prot fields

#define AHB_CSW_PROT_PRIV	0x02000000U
#define AHB_CSW_MASTER_DEBUG	0x20000000U
#define MAP_CSW_HNONSEC		0x40000000U

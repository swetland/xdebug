// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#pragma once

// see: ARM v7-M Architecture Reference, C1
// see: ARM v6-M Architecture Reference, C1

#define DHCSR 0xE000EDF0 // RW Debug Halting Control/Status
#define DCRSR 0xE000EDF4 // WO Debug Core Register Selector
#define DCRDR 0xE000EDF8 // RW Debug Core Register Data
#define DEMCR 0xE000EDFC // RW Debug Exception & Monitor Control


// lower 16 bits are RW and have various restrictions
// Enable Halting Debug
// - cannot be modified by software, only DAP
// - if changing from 0 to 1, C_MASKINTS must be written as 0
// - 0 after power on reset
#define DHCSR_C_DEBUGEN   0x00000001

// Halt the Processor
// - state unknown when C_DEBUGEN is 0
#define DHCSR_C_HALT      0x00000002

// Enable Single Step
// - state unknown when C_DEBUGEN is 0
#define DHCSR_C_STEP      0x00000004

// mask PendSV, SysTick, and external interrupts
// - if C_HALT is not 1, writes are unpredictable
// - if C_MASKINTS is modified C_HALT must be written as 1
//   or results are unpredictable
#define DHCSR_C_MASKINTS  0x00000008 // mask PenSV, SysTick, and Ext IRQs

// (v7M) Allow imprecise debug entry
// - can force stalled load/stores to complete
// - unpredictable if C_DEBUGEN and C_HALT are not also set to 1
// - make memory subsystem unpredictable when set:
//   debugger must reset processor before leaving debug
#define DHCSR_C_SNAPSTALL 0x00000020 // (v7M) allow imprecise debug entry

// magic value must be written to modify the above bits
#define DHCSR_DBGKEY      0xA05F0000

// upper 16 bits are RO
#define DHCSR_S_REGRDY    0x00010000 // 0 on write to DCRSR, 1 when xfer done
#define DHCSR_S_HALT      0x00020000 // 1 if cpu halted (in debug)
#define DHCSR_S_SLEEP     0x00040000 // 1 when cpu sleeping
#define DHCSR_S_LOCKUP    0x00080000 // 1 if cpu locked up (cleared on debug entry)
#define DHCSR_S_RETIRE_ST 0x01000000 // 1 if instruction retired since last read
#define DHCSR_S_RESET_ST  0x02000000 // 1 if cpu reset since last read


// bit 16 controls read vs write
#define DCRSR_RD      0x00000000
#define DCRSR_WR      0x00010000
#define DCRSR_ID_MASK 0x0000FFFF

// to write: write value to DCRDR
//           write (regno | DCRSR_WR) to DCRSR
//           poll DHCSR until S_REGRDY is 1
//
// to read:  write (regno | DCRSR_RD) to DCRSR
//           poll DHCSR until S_REGRDY is 1
//           read value from DCRDR

// 0..15  R0..R12,SP,LR,DebugReturnAddr
// 16     xPSR
// 17     MSP
// 18     PSP
// 20     CONTROL | PRIMASK  (v6M)

// 20     CONTROL | FAULTMASK | BASEPRI | PRIMASK (v7M)
// 33     FPSCR   (v7M w/ FPU)
// 64..95 S0..S31 (v7M w/ FPU)


#define DEMCR_VC_CORERESET 0x00000001 // Halt on Reset Vector *
#define DEMCR_VC_MMERR     0x00000010 // Halt on MemManage exception
#define DEMCR_VC_NOCPERR   0x00000020 // Halt on UsageFault for coproc access
#define DEMCR_VC_CHKERR    0x00000040 // Halt on UsageFault for checking errors
#define DEMCR_VC_STATERR   0x00000080 // Halt on UsageFault for state errors
#define DEMCR_VC_BUSERR    0x00000100 // Halt on BusFault
#define DEMCR_VC_INTERR    0x00000200 // Halt on exception entry/return faults
#define DEMCR_VC_HARDERR   0x00000400 // Halt on HardFault *
#define DEMCR_MON_EN       0x00010000
#define DEMCR_MON_PEND     0x00020000
#define DEMCR_MON_STEP     0x00040000 
#define DEMCR_MON_REQ      0x00080000
#define DEMCR_TRCENA       0x01000000 // Enable DWT and ITM *
// v6M only has *'d bits


#define FP_CTRL              0xE0002000
#define FP_REMAP             0xE0002004
#define FP_COMP(n)           (0xE0002008 + 8*(n))

#define FP_CTRL_REV_MASK     0xF0000000
#define FP_CTRL_REV_V1       0x00000000
#define FP_CTRL_REV_V2       0x10000000
#define FP_CTRL_CODE_H_MASK  0x00007000
#define FP_CTRL_CODE_H_SHIFT 8
#define FP_CTRL_LIT_MASK     0x00000F00
#define FP_CTRL_LIT_SHIFT    8
#define FP_CTRL_CODE_L_MASK  0x000000F0
#define FP_CTRL_CODE_L_SHIFT 4
#define FP_CTRL_KEY          0x00000002
#define FP_CTRL_ENABLE       0x00000001

#define FP_REMAP_RMPSPT      0x20000000

#define FP1_COMP_REMAP       0x00000000  // REMAP
#define FP1_COMP_BK_00       0x40000000 // break on 000:COMP:00
#define FP1_COMP_BK_10       0x80000000 // break on 000:COMP:10
#define FP1_COMP_BK_x0       0xC0000000 // break on 000:COMP:x0
#define FP1_COMP_EN          0x00000001

#define FP2_COMP_DISABLE     0x00000000
#define FP2_COMP_BP_EN       0x00000001
#define FP2_COMP_FP_EN       0x80000000
#define FP2_COMP_BP_MASK     0xFFFFFFFE // allowed BP addr bits
#define FP2_COMP_FP_MASK     0x1FFFFFFE // allowed FP addr bits




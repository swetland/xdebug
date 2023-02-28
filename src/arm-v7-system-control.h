// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#pragma once

// see: ARM v7-M Architecture Reference, B3.2
// see: ARM v6-M Architecture Reference, B3.2

#define CPUID 0xE000ED00 // RO CPUID Base *
#define ICSR  0xE000ED04 // RW Interrupt Control/State *
#define VTOR  0xE000ED08 // RW Vector Table Offset *
#define AIRCR 0xE000ED0C // RW App Interrupt/Reset Control *
#define SCR   0xE000ED10 // RW System Control Register *
#define CCR   0xE000ED14 // RW Configuration & Control *
#define SHPR1 0xE000ED18 // RW System Handler Priority 1
#define SHPR2 0xE000ED1C // RW System Handler Priority 2 *
#define SHPR3 0xE000ED20 // RW System Handler Priority 3 *
#define SHCSR 0xE000ED24 // RW System Handler Control/State *
#define CFSR  0xE000ED28 // RW Configurable Fault Status
#define DFSR  0xE000ED30 // RW Debug Fault Status *
#define HFSR  0xE000ED2C // RW HardFault Status
#define MMFAR 0xE000ED34 // RW MemManage Fault Address
#define BFAR  0xE000ED38 // RW BusFault Address
#define AFSR  0xE000ED3C // RW Aux Fault Status
// v6M has only *'d registers

// DFSR bits indicate debug events, are R/W1C
#define DFSR_HALTED   0x00000001
#define DFSR_BKPT     0x00000002
#define DFSR_DWTTRAP  0x00000004
#define DFSR_VCATCH   0x00000008
#define DFSR_EXTERNAL 0x00000010

#define SHCSR_MEMFAULTACT    0x00000001
#define SHCSR_BUSFAULTACT    0x00000002
#define SHCSR_USGFAULTACT    0x00000008
#define SHCSR_SVCALLACT      0x00000080
#define SHCSR_MONITORACT     0x00000100
#define SHCSR_PENDSVACT      0x00000400
#define SHCSR_SYSTICKACT     0x00000800
#define SHCSR_USGFAULTPENDED 0x00001000
#define SHCSR_MEMFAULTPENDED 0x00002000
#define SHCSR_BUSFAULTPENDED 0x00004000
#define SHCSR_SVCALLPENDED   0x00008000 // *
#define SHCSR_MEMFAULTENA    0x00010000
#define SHCSR_BUSFAULTENA    0x00020000
#define SHCSR_USGFAULTENA    0x00040000

#define HFSR_VECTTBL         0x00000002
#define HFSR_FORCED          0x40000000
#define HFSR_DEBUGEVT        0x80000000

#define SCR_SLEEPONEXIT      0x00000002
#define SCR_SLEEPDEEP        0x00000004
#define SCR_SEVONPEND        0x00000010

#define CCR_NONBASETHRDENA   0x00000001 // Enter Thread Mode w/ Exception Active Enable
#define CCR_USESETMPEND      0x00000002 // Unpriv Access to STIR Enable
#define CCR_UNALIGN_TRP      0x00000008 // Unaligned Access Trap Enable *
#define CCR_DIV_0_TRP        0x00000010 // Divide By Zero Trap Enable
#define CCR_BFHFNMIGN        0x00000100 // Ignore Precise Data Faults at Prio -1 & -2
#define CCR_STKALIGN         0x00000200 // 8 byte Stack Alignment *
#define CCR_DC               0x00010000 // Data & Unified Cache Enable
#define CCR_IC               0x00020000 // Instruction Cache Enabel
#define CCR_BP               0x00040000 // Branch Prediction Enable

#define AIRCR_VECTKEY        0x05FA0000 // reads back as 0xFA050000 on v7M
#define AIRCR_PRIGROUP_MASK  0x00000700 // 
#define AIRCR_BIG_ENDIAN     0x00008000 // *
#define AIRCR_SYSRESETREQ    0x00000004 // system reset request *
#define AIRCR_VECTCLRACTIVE  0x00000002 // clear exception state @ *
#define AIRCR_VECTRESET      0x00000001 // request local reset @ 

// @ unpredictable if processor not in debug halt

// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#pragma once

#include <stdint.h>
#include <stdarg.h>

void MSG(uint32_t flags, const char* fmt, ...)
	__attribute__ ((format (printf, 2, 3)));

#define mINFO 1
#define mDEBUG 2
#define mTRACE 3
#define mERROR 4
#define mPANIC 5 

#define DEBUG(fmt...) MSG(mDEBUG, fmt)
#define INFO(fmt...) MSG(mINFO, fmt)
#define TRACE(fmt...) MSG(mTRACE, fmt)
#define ERROR(fmt...) MSG(mERROR, fmt)
#define PANIC(fmt...) MSG(mPANIC, fmt)

#define DBG_OK 0
#define DBG_ERR -1

typedef struct command_context CC;
const char* cmd_name(CC* cc);
int cmd_arg_u32(CC* cc, unsigned nth, uint32_t* out);
int cmd_arg_u32_opt(CC* cc, unsigned nth, uint32_t* out, uint32_t val);
int cmd_arg_str(CC* cc, unsigned nth, const char** out);
int cmd_arg_str_opt(CC* cc, unsigned nth, const char** out, const char* str);

typedef struct debug_context DC;
void debugger_command(DC* dc, CC* cc);
void debugger_exit(void);

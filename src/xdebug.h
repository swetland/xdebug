// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#pragma once

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

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
int cmd_argc(CC* cc);

typedef struct debug_context DC;
void debugger_command(DC* dc, CC* cc);
void debugger_exit(void);

// commands.c
int do_help(DC* dc, CC* cc);
int do_attach(DC* dc, CC* cc);
int do_reset_stop(DC* dc, CC* cc);

// commands-file.c
int do_upload(DC* dc, CC* cc);
int do_download(DC* dc, CC* cc);

// commands-agent.c
int do_setarch(DC* dc, CC* cc);
int do_flash(DC* dc, CC* cc);
int do_erase(DC* dc, CC* cc);
const char* get_arch_name(void);

void *load_file(const char* fn, size_t *sz);
void *get_builtin_file(const char *name, size_t *sz);
const char *get_builtin_filename(unsigned n);


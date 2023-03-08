// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#pragma once

#include <stdarg.h>

void tui_init(void);
void tui_exit(void);
int tui_handle_event(void (*callback)(char* line, unsigned len));

void tui_status_rhs(const char* status);
void tui_status_lhs(const char* status);

// Write a line (or multiple lines separated by '\n') to the TUI log
// Non-printing and non-ascii characters are ignored.
// Lines larger than the TUI log max width (128) are truncated.
void tui_printf(const char* fmt, ...);
void tui_vprintf(const char* fmt, va_list ap);

// TUI Channels provide a way for different entities to use a
// printf() interface to send log lines to the TUI without
// interleaving partial log lines.

// They contain a line assembly buffer and only send lines to
// the TUI log when a newline character is encountered.

// It is safe for different threads to use different channels
// to simultaneously send log lines, but it is NOT safe for
// different threads to simultaneously use the same channel.
typedef struct tui_ch tui_ch_t;

int tui_ch_create(tui_ch_t** ch, unsigned flags);
void tui_ch_destroy(tui_ch_t* ch);
void tui_ch_printf(tui_ch_t* ch, const char* fmt, ...);
void tui_ch_vprintf(tui_ch_t* ch, const char* fmt, va_list ap); 


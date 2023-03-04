// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <string.h>

#include "xdebug.h"
#include "transport.h"
#include "arm-v7-debug.h"
#include "arm-v7-system-control.h"


int do_exit(DC* dc, CC* cc) {
	debugger_exit();
	return 0;
}

int do_help(DC* dc, CC* cc);

struct {
	const char* name;
	int (*func)(DC* dc, CC* cc);
	const char* help;
} CMDS[] = {
	{ "exit", do_exit, "exit debugger" },
	{ "quit", do_exit, NULL },
	{ "help", do_help, "list debugger commands" },
};

int do_help(DC* dc, CC* cc) {
	int w = 0;
	for (int n = 0; n < sizeof(CMDS)/sizeof(CMDS[0]); n++) {
		int len = strlen(CMDS[0].name);
		if (len > w) w = len;
	}
	for (int n = 0; n < sizeof(CMDS)/sizeof(CMDS[0]); n++) {
		if (CMDS[n].help == NULL) {
			continue;
		}
		INFO("%-*s %s", w, CMDS[n].name, CMDS[n].help);
	}
	return 0;
}

void debugger_command(DC* dc, CC* cc) {
	const char* cmd = cmd_name(cc);
	for (int n = 0; n < sizeof(CMDS)/sizeof(CMDS[0]); n++) {
		if (!strcmp(cmd, CMDS[n].name)) {
			CMDS[n].func(dc, cc);
			return;
		}
	}
	ERROR("unknown command '%s'\n", cmd);
}


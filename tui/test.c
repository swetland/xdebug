// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <tui.h>
#include <string.h>

void handle_line(char* line, unsigned len) {
	if (len) {
		tui_printf("? %s\n", line);
	}
	if (!strcmp(line, "exit")) {
		tui_exit();
		exit(-1);
	}
}

int main(int argc, char** argv) {
	tui_init();
	while (tui_handle_event(handle_line) == 0) ;
	tui_exit();
	return 0;
}

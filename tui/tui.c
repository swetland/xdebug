// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#include <tui.h>
#include <termbox.h>

#define MAXWIDTH 128
#define MAXCMD (MAXWIDTH - 1)

typedef struct line LINE;
typedef struct ux UX;

struct line {
	LINE* prev;
	LINE* next;
	uint16_t len;
	uint8_t fg;
	uint8_t bg;
	uint8_t text[MAXWIDTH];
};

struct ux {
	pthread_mutex_t lock;

	int w;
	int h;
	int invalid;
	int running;

	char status_lhs[32];
	char status_rhs[32];

	LINE list;

	// edit buffer and head of the circular history list
	LINE history;

	// points at active edit buffer
	LINE *cmd;

	// points at the line *before* the bottom-most list line
	LINE *display;
};


static void tui_add_cmd(UX* ux, uint8_t* text, unsigned len) {
	LINE* line = malloc(sizeof(LINE));
	if (line == NULL) {
		return;
	}
	line->len = len;
	line->fg = line->bg = TB_DEFAULT;
	memcpy(line->text, text, len);

	line->prev = ux->history.prev;
	line->next = &ux->history;

	line->prev->next = line;
	ux->history.prev = line;
}

static void paint(int x, int y, const char* str) {
	while (*str) {
		tb_change_cell(x++, y, *str++, TB_DEFAULT, TB_DEFAULT);
	}
}

static void paint_titlebar(UX *ux) {
}

static void paint_infobar(UX *ux) {
	int lhw = strlen(ux->status_lhs);
	int rhw = strlen(ux->status_rhs);
	int gap = ux->w - 2 - lhw - rhw;
	if (gap < 0) gap = 0;

	unsigned fg = TB_DEFAULT | TB_REVERSE;
	unsigned bg = TB_DEFAULT;
	int x = 0;
	int y = ux->h - 2;

	tb_change_cell(x++, y, '-', fg, bg);
	char *s = ux->status_lhs;
	while (x < ux->w && lhw-- > 0) {
		tb_change_cell(x++, y, *s++, fg, bg);
	}
	while (x < ux->w && gap-- > 0) {
		tb_change_cell(x++, y, '-', fg, bg);
	}
	s = ux->status_rhs;
	while (x < ux->w && rhw-- > 0) {
		tb_change_cell(x++, y, *s++, fg, bg);
	}
	tb_change_cell(x, y, '-', fg, bg);
}

static void paint_cmdline(UX *ux) {
	uint8_t* ch = ux->cmd->text;
	int len = ux->cmd->len;
	int y = ux->h - 1;
	int w = ux->w;
	for (int x = 0; x < w; x++) {
		if (x < len) {
			tb_change_cell(x, y, *ch++, TB_DEFAULT, TB_DEFAULT);
		} else {
			tb_change_cell(x, y, ' ', TB_DEFAULT, TB_DEFAULT);
		}
	}
	tb_set_cursor(len >= w ? w - 1 : len, y);
}

static void paint_log(UX *ux) {
	LINE* list = &ux->list;
	int y = ux->h - 3;
	int w = ux->w;
	uint8_t c;

	for (LINE* line = ux->display->prev; (line != list) && (y >= 0); line = line->prev) {
		for (int x = 0; x < w; x++) {
			c = (x < line->len) ? line->text[x] : ' ';
			tb_change_cell(x, y, c, line->fg, line->bg);
		}
		y--;
	}
}

static int repaint(UX* ux) {
	// clear entire display and adjust to any resize events
	tb_clear();
	ux->w = tb_width();
	ux->h = tb_height();

	if ((ux->w < 40) || (ux->h < 8)) {
		paint(0, 0, "WINDOW TOO SMALL");
		return 1;
	}

	paint_titlebar(ux);
	paint_infobar(ux);
	paint_cmdline(ux);
	paint_log(ux);

	if (ux->display != &ux->list) {
		int x = ux->w - 8;
		char *s = " SCROLL ";
		while (*s != 0) {
			tb_change_cell(x++, 0, *s++, TB_REVERSE | TB_DEFAULT, TB_DEFAULT);
		}
	}
	return 0;
}

static void tui_scroll(UX* ux, int delta) {
	LINE* list = &ux->list;
	LINE* line = ux->display;

	while (delta > 0) {
		if (line->prev == list) goto done;
		delta--;
		line = line->prev;
	}
	while (delta < 0) {
		if (line == list) goto done;
		delta++;
		line = line->next;
	}
done:
	ux->display = line;
	repaint(ux);
	tb_present();
}

static int handle_event(UX* ux, struct tb_event* ev, char* line, unsigned* len) {
	// always process full repaints due to resize or user request
	if ((ev->type == TB_EVENT_RESIZE) ||
	    (ev->key == TB_KEY_CTRL_L)) {
		ux->invalid = repaint(ux);
		return 0;
	}

	// ignore other input of the window is too small
	if (ux->invalid) {
		return 0;
	}
		
	switch (ev->key) {
	case 0: // printable character
		// ignore fancy unicode characters
		if (ev->ch > 255) {
			break;
		}
		if (ux->cmd->len >= MAXCMD) {
			break;
		}
		ux->cmd->text[ux->cmd->len++] = ev->ch;
		paint_cmdline(ux);
		break;
	case TB_KEY_BACKSPACE:
	case TB_KEY_BACKSPACE2:
		if (ux->cmd->len > 0 ) {
			ux->cmd->len--;
			paint_cmdline(ux);
		}
		break;
	case TB_KEY_ENTER: {
		// pass commandline out to caller
		*len = ux->cmd->len;
		memcpy(line, ux->cmd->text, ux->cmd->len);
		line[ux->cmd->len] = 0;

		// add new command to history
		tui_add_cmd(ux, ux->cmd->text, ux->cmd->len);

		// reset to an empty edit buffer
		ux->cmd = &ux->history;
		ux->cmd->len = 0;

		// update display
		paint_cmdline(ux);
		tb_present();

		return 1;
	}
	case TB_KEY_ARROW_LEFT:
	case TB_KEY_ARROW_RIGHT:
	case TB_KEY_HOME:
	case TB_KEY_END:
	case TB_KEY_INSERT:
	case TB_KEY_DELETE:
		break;
	case TB_KEY_ARROW_UP:
		if (ux->cmd->prev != &ux->history) {
			ux->cmd = ux->cmd->prev;
			paint_cmdline(ux);
			tb_present();
		}
		break;
	case TB_KEY_ARROW_DOWN:
		if (ux->cmd != &ux->history) {
			ux->cmd = ux->cmd->next;
			paint_cmdline(ux);
			tb_present();
		}
		break;
	case TB_KEY_PGUP:
		tui_scroll(ux, ux->h - 3);
		break;
	case TB_KEY_PGDN:
		tui_scroll(ux, -(ux->h - 3));
		break;
	case TB_KEY_ESC: {
		*len = 5;
		memcpy(line, "@ESC@", 6);
		return 1;
	}
	default:
#if 0 // debug unexpected keys
		if (ux->len < (MAXWIDTH - 6)) {
			char tmp[5];
			sprintf(tmp, "%04x", ev->key);
			ux->cmd[ux->len++] = '<';
			ux->cmd[ux->len++] = tmp[0];
			ux->cmd[ux->len++] = tmp[1];
			ux->cmd[ux->len++] = tmp[2];
			ux->cmd[ux->len++] = tmp[3];
			ux->cmd[ux->len++] = '>';
			paint_cmdline(ux);
		}
#endif
		break;
	}
	return 0;
}

static UX ux = {
	.lock = PTHREAD_MUTEX_INITIALIZER,
	.list = {
		.prev = &ux.list,
		.next = &ux.list,
	},
	.history = {
		.prev = &ux.history,
		.next = &ux.history,
	},
	.cmd = &ux.history,
	.display = &ux.list,
	.running = 1,
};

void tui_init(void) {
	if (tb_init()) {
		fprintf(stderr, "termbox init failed\n");
		return;
	}
	tb_select_input_mode(TB_INPUT_ESC | TB_INPUT_SPACE);
	repaint(&ux);
}

void tui_exit(void) {
	pthread_mutex_lock(&ux.lock);
	tb_shutdown();
	ux.running = 0;
	pthread_mutex_unlock(&ux.lock);
}

int tui_handle_event(void (*cb)(char*, unsigned)) {
	struct tb_event ev;
	char line[MAXWIDTH];
	unsigned len;
	int r;

	pthread_mutex_lock(&ux.lock);
	if (ux.running) {
		tb_present();
	}
	pthread_mutex_unlock(&ux.lock);

	if ((tb_poll_event(&ev) < 0) ||
	    (ev.key == TB_KEY_CTRL_C)) {
		return -1;
	}

	pthread_mutex_lock(&ux.lock);
	if (ux.running) {
		r = handle_event(&ux, &ev, line, &len);
	} else {
		r = -1;
	}
	pthread_mutex_unlock(&ux.lock);

	if (r == 1) {
		cb(line, len);
		return 0;
	} else {
		return r;
	}
}

void tui_status_rhs(const char* status) {
	pthread_mutex_lock(&ux.lock);
	if (ux.running) {
		strncpy(ux.status_rhs, status, sizeof(ux.status_rhs) - 1);
		paint_infobar(&ux);
		tb_present();
	}
	pthread_mutex_unlock(&ux.lock);
}

void tui_status_lhs(const char* status) {
	pthread_mutex_lock(&ux.lock);
	if (ux.running) {
		strncpy(ux.status_lhs, status, sizeof(ux.status_lhs) - 1);
		paint_infobar(&ux);
		tb_present();
	}
	pthread_mutex_unlock(&ux.lock);
}

static void tui_logline(uint8_t* text, unsigned len) {
	LINE* line = malloc(sizeof(LINE));
	if (line == NULL) return;
	memcpy(line->text, text, len);
	line->len = len;
	line->fg = TB_DEFAULT;
	line->bg = TB_DEFAULT;

	pthread_mutex_lock(&ux.lock);
	if (ux.running) {
		// add line to the log
		line->prev = ux.list.prev;
		line->next = &ux.list;
		line->prev->next = line;
		ux.list.prev = line;

		// refresh the log
		paint_log(&ux);
		tb_present();
	}
	pthread_mutex_unlock(&ux.lock);
}

struct tui_ch {
	unsigned len;
	uint8_t buffer[MAXWIDTH];
};

int tui_ch_create(tui_ch_t** out, unsigned flags) {
	tui_ch_t* ch = calloc(1, sizeof(tui_ch_t));
	if (ch == NULL) {
		return -1;
	} else {
		*out = ch;
		return 0;
	}
}

void tui_ch_destroy(tui_ch_t* ch) {
	free(ch);
}

void tui_ch_vprintf(tui_ch_t* ch, const char* fmt, va_list ap) {
	char tmp[1024];
	int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
	char *x = tmp;
	while (n > 0) {
		uint8_t c = *x++;
		n--;
		if ((c == '\n') && (ch->len > 0)) {
			tui_logline(ch->buffer, ch->len);
			ch->len = 0;
			continue;
		}
		if ((c < ' ') || (c > 0x7e)) {
			continue;
		}
		if (ch->len < MAXWIDTH) {
			ch->buffer[ch->len++] = c;
		}
	}
}

void tui_ch_printf(tui_ch_t* ch, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	tui_ch_vprintf(ch, fmt, ap);
	va_end(ap);
}

void tui_printf(const char* fmt, ...) {
	va_list ap;
	tui_ch_t ch = { 0 };
	va_start(ap, fmt);
	tui_ch_vprintf(&ch, fmt, ap);
	va_end(ap);
	if (ch.len > 0) {
		tui_logline(ch.buffer, ch.len);
	}
}

void tui_vprintf(const char* fmt, va_list ap) {
	tui_ch_t ch = { 0 };
	tui_ch_vprintf(&ch, fmt, ap);
	if (ch.len > 0) {
		tui_logline(ch.buffer, ch.len);
	}
}


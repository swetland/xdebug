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

	uint8_t cmd[MAXWIDTH + 1];
	int len;

	LINE list;
};

static void paint(int x, int y, const char* str) {
	while (*str) {
		tb_change_cell(x++, y, *str++, TB_DEFAULT, TB_DEFAULT);
	}
}

static void paint_titlebar(UX *ux) {
}

static void paint_infobar(UX *ux) {
	for (int x = 0; x < ux->w; x++) {
		tb_change_cell(x, ux->h - 2, '-', TB_DEFAULT | TB_REVERSE, TB_DEFAULT);
	}
}

static void paint_cmdline(UX *ux) {
	uint8_t* ch = ux->cmd;
	int y = ux->h - 1;
	int w = ux->w;
	for (int x = 0; x < w; x++) {
		if (x < ux->len) {
			tb_change_cell(x, y, *ch++, TB_DEFAULT, TB_DEFAULT);
		} else {
			tb_change_cell(x, y, ' ', TB_DEFAULT, TB_DEFAULT);
		}
	}
	tb_set_cursor(ux->len >= w ? w - 1 : ux->len, y);
}

static void paint_log(UX *ux) {
	LINE* list = &ux->list;
	int y = ux->h - 3;
	int w = ux->w;
	uint8_t c;

	for (LINE* line = list->prev; (line != list) && (y >= 0); line = line->prev) {
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
	return 0;
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
		if (ux->len >= MAXWIDTH) {
			break;
		}
		ux->cmd[ux->len++] = ev->ch;
		paint_cmdline(ux);
		break;
	case TB_KEY_BACKSPACE:
	case TB_KEY_BACKSPACE2:
		if (ux->len > 0 ) {
			ux->len--;
			paint_cmdline(ux);
		}
		break;
	case TB_KEY_ENTER: {
		// pass commandline out to caller
		*len = ux->len;
		memcpy(line, ux->cmd, ux->len);
		line[ux->len] = 0;

		// update display, clearing commandline
		ux->len = 0;
		paint_cmdline(ux);
		tb_present();

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
	tb_shutdown();
}

int tui_handle_event(void (*cb)(char*, unsigned)) {
	struct tb_event ev;
	char line[MAXWIDTH + 1];
	unsigned len;
	int r;

	pthread_mutex_lock(&ux.lock);
	tb_present();
	pthread_mutex_unlock(&ux.lock);

	if ((tb_poll_event(&ev) < 0) ||
	    (ev.key == TB_KEY_CTRL_C)) {
		return -1;
	}

	pthread_mutex_lock(&ux.lock);
	r = handle_event(&ux, &ev, line, &len);
	pthread_mutex_unlock(&ux.lock);

	if (r == 1) {
		cb(line, len);
		return 0;
	} else {
		return r;
	}
}

static void tui_logline(uint8_t* text, unsigned len) {
	LINE* line = malloc(sizeof(LINE));
	if (line == NULL) return;
	memcpy(line->text, text, len);
	line->len = len;
	line->fg = TB_DEFAULT;
	line->bg = TB_DEFAULT;

	pthread_mutex_lock(&ux.lock);

	// add line to the log
	line->prev = ux.list.prev;
	line->next = &ux.list;
	line->prev->next = line;
	ux.list.prev = line;

	// refresh the log
	paint_log(&ux);
	tb_present();

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


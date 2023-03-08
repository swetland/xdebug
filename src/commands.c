// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <string.h>

#include "xdebug.h"
#include "transport.h"
#include "arm-v7-debug.h"
#include "arm-v7-system-control.h"


int do_attach(DC* dc, CC* cc) {
	uint32_t n;
	dc_set_clock(dc, 4000000);
	return dc_attach(dc, 0, 0, &n);
}


static uint32_t reglist[20] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
	10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
};
static uint32_t lastregs[20];

static int read_show_regs(DC* dc) {
	if (dc_core_reg_rd_list(dc, reglist, lastregs, 20)) {
		return DBG_ERR;
	}
	INFO("r0 %08x r4 %08x r8 %08x ip %08x psr %08x\n",
		lastregs[0], lastregs[4], lastregs[8],
		lastregs[12], lastregs[16]);
	INFO("r1 %08x r5 %08x r9 %08x sp %08x msp %08x\n",
		lastregs[1], lastregs[5], lastregs[9],
		lastregs[13], lastregs[17]);
	INFO("r2 %08x r6 %08x 10 %08x lr %08x psp %08x\n",
		lastregs[2], lastregs[6], lastregs[10],
		lastregs[14], lastregs[18]);
	INFO("r3 %08x r7 %08x 11 %08x pc %08x\n",
		lastregs[3], lastregs[7], lastregs[11],
		lastregs[15]);
	INFO("control  %02x faultmsk %02x basepri  %02x primask  %02x\n",
		lastregs[20] >> 24, (lastregs[20] >> 16) & 0xFF,
		(lastregs[20] >> 8) & 0xFF, lastregs[20] & 0xFF);
	return 0;
}

int do_regs(DC* dc, CC* cc) {
	return read_show_regs(dc);
}

static uint32_t lastaddr = 0x20000000;
static uint32_t lastcount = 0x40;

int do_dw(DC* dc, CC* cc) {
	uint32_t data[1024];
	uint32_t addr;
	uint32_t count;
	unsigned n;

	if (cmd_arg_u32_opt(cc, 1, &addr, lastaddr)) return DBG_ERR;
	if (cmd_arg_u32_opt(cc, 2, &count, lastcount)) return DBG_ERR;
	if (count > 1024) count = 1024;
	lastaddr = addr;
	lastcount = count;

	if (addr & 3) {
		ERROR("address is not word-aligned\n");
		return DBG_ERR;
	}
	count /= 4;
	if (count < 1) return 0;

	if (dc_mem_rd_words(dc, addr, count, data)) return DBG_ERR;
	for (n = 0; count > 0; n += 4, addr += 16) {
		switch (count) {
		case 1:
			count = 0;
			INFO("%08x: %08x\n", addr, data[n]);
			break;
		case 2:
			count = 0;
			INFO("%08x: %08x %08x\n",
				addr, data[n], data[n+1]);
			break;
		case 3:
			count = 0;
			INFO("%08x: %08x %08x %08x\n",
				addr, data[n], data[n+1], data[n+2]);
			break;
		default:
			count -= 4;
			INFO("%08x: %08x %08x %08x %08x\n",
				addr, data[n], data[n+1], data[n+2], data[n+3]);
			break;
		}
	}
	return 0;
}

int do_db(DC* dc, CC* cc) {
	uint32_t data[1024 + 8];
	uint32_t addr, count, bytecount;
	uint8_t *x;
	unsigned n;

	if (cmd_arg_u32_opt(cc, 1, &addr, lastaddr)) return DBG_ERR;
	if (cmd_arg_u32_opt(cc, 2, &count, lastcount)) return DBG_ERR;
	if (count > 1024) count = 1024;
	lastaddr = addr;
	lastcount = count;

	bytecount = count;
	x = (void*) data;
	if (addr & 3) {
		x += (addr & 3);
		count += 4 - (addr & 3);
		addr &= 3;
	}
	if (count & 3) {
		count += 4 - (count & 3);
	}
	count /= 4;
	if (count < 1) return 0;

	if (dc_mem_rd_words(dc, addr, count, data)) return DBG_ERR;

	while (bytecount > 0) {
		n = (bytecount > 16) ? 16 : bytecount;
		INFO("%08x:", addr);
		bytecount -= n;
		addr += n;
		while (n-- > 0) {
			INFO(" %02x", *x++);
		}
		INFO("\n");
	}
	return 0;
}

int do_rd(DC* dc, CC* cc) {
	uint32_t addr, val;
	if (cmd_arg_u32(cc, 1, &addr)) return DBG_ERR;
	int r = dc_mem_rd32(dc, addr, &val);
	if (r < 0) {
		INFO("%08x: ????????\n", addr);
	} else {
		INFO("%08x: %08x\n", addr, val);
	}
	return r;
}

int do_wr(DC* dc, CC* cc) {
	uint32_t addr, val;
	if (cmd_arg_u32(cc, 1, &addr)) return DBG_ERR;
	if (cmd_arg_u32(cc, 2, &val)) return DBG_ERR;
	int r;
	if ((r = dc_mem_wr32(dc, addr, val)) == 0) {
		INFO("%08x< %08x\n", addr, val);
	}
	return r;
}

int do_stop(DC* dc, CC* cc) {
	int r;
	if ((r = dc_core_halt(dc)) < 0) {
		return r;
	}
	if ((r = dc_core_wait_halt(dc)) < 0) {
		return r;
	}
	return read_show_regs(dc);
}

int do_resume(DC* dc, CC* cc) {
	return dc_core_resume(dc);
}

int do_step(DC* dc, CC* cc) {
	int r;
	if ((r = dc_core_step(dc)) < 0) {
		return r;
	}
	if ((r = dc_core_wait_halt(dc)) < 0) {
		return r;
	}
	return read_show_regs(dc);
}

static uint32_t vcflags = 0;

int do_reset(DC* dc, CC* cc) {
	int r;
	if ((r = dc_core_halt(dc)) < 0) {
		return r;
	}
	if ((r = dc_mem_wr32(dc, DEMCR, DEMCR_TRCENA | vcflags)) < 0) {
		return r;
	}
	if ((r = dc_mem_wr32(dc, AIRCR, AIRCR_VECTKEY | AIRCR_SYSRESETREQ)) < 0) {
		return r;
	}
	return 0;
}

static int wait_for_stop(DC* dc) {
	unsigned m = 0;
	uint32_t val;
	int r;
	for (unsigned n = 0; n < 100; n++) {
		if ((r = dc_mem_rd32(dc, DHCSR, &val)) < 0) {
			dc_attach(dc, 0, 0, &val);
		} else {
			if (val & DHCSR_S_HALT) {
				INFO("halt: CPU HALTED (%u,%u)\n", n, m);
				uint32_t dfsr = -1, demcr = -1;
				dc_mem_rd32(dc, DFSR, &dfsr);
				dc_mem_rd32(dc, DEMCR, &demcr);
				INFO("halt: DHCSR %08x, DFSR %08x, DEMCR %08x\n", val, dfsr, demcr);
				return 0;
			}
			if (val & DHCSR_S_RESET_ST) {
				m++;
			}
			dc_mem_wr32(dc, DHCSR, DHCSR_DBGKEY | DHCSR_C_HALT | DHCSR_C_DEBUGEN);
		}
	}
	return DC_ERR_FAILED;
}

int do_reset_stop(DC* dc, CC* cc) {
	int r;
	if ((r = dc_core_halt(dc)) < 0) {
		return r;
	}
	if ((r = wait_for_stop(dc)) < 0) {
		return r;
	}
	if ((r = dc_mem_wr32(dc, DEMCR, DEMCR_VC_CORERESET | DEMCR_TRCENA | vcflags)) < 0) {
		return r;
	}
	if ((r = dc_mem_wr32(dc, AIRCR, AIRCR_VECTKEY | AIRCR_SYSRESETREQ)) < 0) {
		return r;
	}
	if ((r = wait_for_stop(dc)) < 0) {
		INFO("reset-stop: CPU DID NOT HALT\n");
		return r;
	}
	return 0;
}

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
{ "attach",     do_attach,     "connect to target" },
{ "stop",       do_stop,       "halt core" },
{ "halt",       do_stop,       NULL },
{ "go",         do_resume,     NULL },
{ "resume",     do_resume,     "resume core" },
{ "step",       do_step,       "single-step core" },
{ "reset",      do_reset,      "reset core" },
{ "reset-stop", do_reset_stop, "reset core and halt" },
{ "dw",         do_dw,         "dump words            dw <addr> [ <count> ]" },
{ "db",         do_db,         "dump bytes            db <addr> [ <count> ]" },
{ "rd",         do_rd,         "read word             rd <addr>" },
{ "dr",         do_rd,         NULL },
{ "wr",         do_wr,         "write word            wr <addr> <val>" },
{ "regs",       do_regs,       "dump registers" },
{ "help",       do_help,       "list commands" },
{ "exit",       do_exit,       "exit debugger" },
{ "quit",       do_exit,       NULL },
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
		INFO("%-*s %s\n", w + 3, CMDS[n].name, CMDS[n].help);
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


// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "xdebug.h"
#include "transport.h"
#include "arm-v7-debug.h"
#include "arm-v7-system-control.h"

#define _AGENT_HOST_
#include <agent/flash.h>

static flash_agent *AGENT = NULL;
static uint32_t AGENT_sz = 0;
static char *AGENT_arch = NULL;

const char* get_arch_name(void) {
	if (AGENT_arch) {
		return AGENT_arch;
	} else {
		return "";
	}
}

static void *load_agent(const char *arch, size_t *_sz) {
	void *data;
	size_t sz;
	char name[1024];

	if (arch == NULL) return NULL;

	name[sizeof(name)-1] = 0;
	snprintf(name, sizeof(name)-1, "%s.bin", arch);
	if ((data = get_builtin_file(name, &sz))) {
		void *copy = malloc(sz + 4);
		if (copy == NULL) return NULL;
		memcpy(copy, data, sz);
		*_sz = sz;
		return copy;
	}

	snprintf(name, sizeof(name)-1, "out/agents/%s.bin", arch);
	return load_file(name, _sz);
}

int do_setarch(DC* dc, CC* cc) {
	char *agent_name = NULL;
	flash_agent *agent = NULL;
	size_t agent_sz = 0;
	const char *name;

	cmd_arg_str_opt(cc, 1, &name, NULL);
	if (name == NULL) {
		if (AGENT_arch) {
			INFO("current flash agent is '%s'\n", AGENT_arch);
		} else {
			INFO("no flash agent selected\n");
		}
fail_load:
		INFO("set architecture with: arch <name> (omit the .bin)\n");
		for (unsigned n = 0; (name = get_builtin_filename(n)) != NULL; n++) {
			INFO("   %s\n", name);
		}
		goto fail;
	}
	if ((agent_name = malloc(strlen(name)+1)) == NULL) {
		goto fail;
	}
	memcpy(agent_name, name, strlen(name)+1);
	if ((agent = load_agent(name, &agent_sz)) == NULL) {
		ERROR("cannot load flash agent for architecture '%s'\n", name);
		goto fail_load;
	}

	// sanity check
	if ((agent_sz < sizeof(flash_agent)) ||
		(agent->magic != AGENT_MAGIC) ||
		(agent->version != AGENT_VERSION)) {
		ERROR("invalid agent image\n");
		goto fail;
	}

	INFO("flash agent '%s' loaded.\n", agent_name);
	if (AGENT) {
		free(AGENT);
		free(AGENT_arch);
	}
	AGENT = agent;
	AGENT_sz = agent_sz;
	AGENT_arch = agent_name;
	return 0;

fail:
	if (agent) {
		free(agent);
	}
	if (agent_name) {
		free(agent_name);
	}
	return DBG_ERR;
}

static int invoke(DC* dc, uint32_t agent, uint32_t func,
	uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3) {
	// TODO: improve error handling
	dc_core_reg_wr(dc, 0, r0);
	dc_core_reg_wr(dc, 1, r1);
	dc_core_reg_wr(dc, 2, r2);
	dc_core_reg_wr(dc, 3, r3);
	dc_core_reg_wr(dc, 13, agent - 4);
	dc_core_reg_wr(dc, 14, agent | 1); // include T bit
	dc_core_reg_wr(dc, 15, func | 1); // include T bit

	// if the target has bogus data at 0, the processor may be in
	// pending-exception state after reset-stop, so we will clear
	// any exceptions and then set the PSR to something reasonable
	dc_mem_wr32(dc, AIRCR, AIRCR_VECTKEY | AIRCR_VECTCLRACTIVE);
	dc_core_reg_wr(dc, 16, 0x01000000);

	// todo: readback and verify?

	INFO("agent: call <func@%08x>(0x%x,0x%x,0x%x,0x%x)\n", func, r0, r1, r2, r3);
	dc_core_resume(dc);
	// todo: timeout after a few seconds?
	if (dc_core_wait_halt(dc)) {
		ERROR("agent: interrupted\n");
		return DBG_ERR;
	}

	uint32_t pc = 0xeeeeeeee, res = 0xeeeeeeee;
	dc_core_reg_rd(dc, 0, &res);
	dc_core_reg_rd(dc, 15, &pc);
	if (pc != agent) {
		ERROR("pc (%08x) is not at %08x\n", pc, agent);
		return -1;
	}
	if (res) {
		if (res == ERR_INVALID) {
			ERROR("agent: unsupported part\n");
		} else {
			ERROR("agent: failure %d\n", res);
		}
		return DBG_ERR;
	}
	return 0;
}

static int run_flash_agent(DC* dc, uint32_t flashaddr, void *data, uint32_t data_sz) {
	uint8_t buffer[4096];
	flash_agent *agent;
	uint32_t agent_sz;
	int r;

	if (AGENT == NULL) {
		ERROR("no flash agent selected\n");
		ERROR("set architecture with: arch <name>\n");
		goto fail;
	}
	if (AGENT_sz > sizeof(buffer)) {
		ERROR("flash agent too large\n");
		goto fail;
	}

	memcpy(buffer, AGENT, AGENT_sz);
	agent_sz = AGENT_sz;
	agent = (void*) buffer;

	// replace magic with bkpt instructions
	agent->magic = 0xbe00be00;

	if (do_attach(dc,0)) {
		ERROR("failed to attach\n");
		goto fail;
	}
	if (do_reset_stop(dc,0)) {
		goto fail;
	}

	if (agent->flags & FLAG_BOOT_ROM_HACK) {
	// TODO: wire this back up
#if 1
		ERROR("agent: BOOT ROM HACK unsupported\n");
		return DBG_ERR;
#else
		xprintf(XCORE, "executing boot rom\n");
		if (swdp_watchpoint_rw(0, 0)) {
			goto fail;
		}
		swdp_core_resume();
		swdp_core_wait_for_halt();
		swdp_watchpoint_disable(0);
		// todo: timeout?
		// todo: confirm halted
#endif
	}

	if (dc_mem_wr_words(dc, agent->load_addr, agent_sz / 4, (void*) agent)) {
		ERROR("failed to download agent\n");
		goto fail;
	}
	INFO("agent: loaded @%08x (%d bytes)\n", agent->load_addr, agent_sz);

	if (invoke(dc, agent->load_addr, agent->setup, agent->load_addr, 0, 0, 0)) {
		goto fail;
	if (r != 0) {
		}
		goto fail;
	}
	if (dc_mem_rd_words(dc, agent->load_addr + 16, 4, (void*) &agent->data_addr)) {
		goto fail;
	}
	INFO("agent: info: buffer %dK @%08x, flash %dK @%08x\n",
		agent->data_size / 1024, agent->data_addr,
		agent->flash_size / 1024, agent->flash_addr);

	if ((flashaddr == 0) && (data == NULL) && (data_sz == 0xFFFFFFFF)) {
		// erase all
		flashaddr = agent->flash_addr;
		data_sz = agent->flash_size;
	}

	if ((flashaddr < agent->flash_addr) ||
		(data_sz > agent->flash_size) ||
		((flashaddr + data_sz) > (agent->flash_addr + agent->flash_size))) {
		ERROR("invalid flash address %08x..%08x\n",
			flashaddr, flashaddr + data_sz);
		goto fail;
	}

	if (data == NULL) {
		// erase
		if (invoke(dc, agent->load_addr, agent->erase, flashaddr, data_sz, 0, 0)) {
			ERROR("failed to erase %d bytes at %08x\n", data_sz, flashaddr);
			goto fail;
		}
		INFO("erase: OK\n");
	} else {
		// write
		uint8_t *ptr = (void*) data;
		uint32_t xfer;
		INFO("flash: writing %d bytes at %08x...\n", data_sz, flashaddr);
		if (invoke(dc, agent->load_addr, agent->erase, flashaddr, data_sz, 0, 0)) {
			ERROR("failed to erase %d bytes at %08x\n", data_sz, flashaddr);
			goto fail;
		}
		while (data_sz > 0) {
			if (data_sz > agent->data_size) {
				xfer = agent->data_size;
			} else {
				xfer = data_sz;
			}
			if (dc_mem_wr_words(dc, agent->data_addr, xfer / 4, (void*) ptr)) {
				ERROR("download to %08x failed\n", agent->data_addr);
				goto fail;
			}
			if (invoke(dc, agent->load_addr, agent->write,
				flashaddr, agent->data_addr, xfer, 0)) {
				ERROR("failed to flash %d bytes to %08x\n", xfer, flashaddr);
				goto fail;
			}
			ptr += xfer;
			data_sz -= xfer;
			flashaddr += xfer;
		}
		INFO("flash: OK\n");
	}

	if (data) free(data);
	return 0;
fail:
	if (data) free(data);
	return -1;
}

int do_flash(DC* dc, CC* cc) {
	void *data = NULL;
	size_t sz;
	const char *fn;
	uint32_t addr;
	uint32_t data_sz;
	if (cmd_arg_str(cc, 1, &fn)) return DBG_ERR;
	if (cmd_arg_u32(cc, 2, &addr)) return DBG_ERR;

	if ((data = load_file(fn, &sz)) == NULL) {
		ERROR("cannot load '%s'\n", fn);
		return -1;
	}
	if (sz > (1024*1024)) {
		ERROR("too large\n");
		return DBG_ERR;
	}

	// word align
	data_sz = (sz + 3) & ~3;
	return run_flash_agent(dc, addr, data, data_sz);
}

int do_erase(DC* dc, CC* cc) {
	const char* s;
	uint32_t addr;
	uint32_t len;
	cmd_arg_str_opt(cc, 1, &s, "");
	if (!strcmp(s, "all")) {
		return run_flash_agent(dc, 0, NULL, 0xFFFFFFFF);
	}
	if (cmd_arg_u32(cc, 1, &addr)) return DBG_ERR;
	if (cmd_arg_u32(cc, 2, &len)) return DBG_ERR;
	return run_flash_agent(dc, addr, NULL, len);
}


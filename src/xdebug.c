// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "usb.h"
#include "cmsis-dap-protocol.h"

struct debug_context {
	usb_handle* usb;
	unsigned connected;

	// last dp select written
	uint32_t dp_select;

	uint32_t max_packet_count;
	uint32_t max_packet_size;
};


typedef struct debug_context DC;


int dap_get_info(DC* dc, unsigned di, void *out, unsigned minlen, unsigned maxlen) {
	uint8_t	buf[256 + 2];
	buf[0] = DAP_Info;
	buf[1] = di;
	if (usb_write(dc->usb, buf, 2) != 2) {
		return -1;
	}
	int sz = usb_read(dc->usb, buf, 256 + 2);
	if ((sz < 2) || (buf[0] != DAP_Info)) {
		return -1;
	}
	//printf("0x%02x > 0x%02x 0x%02x\n", di, buf[0], buf[1]);
	if ((buf[1] < minlen) || (buf[1] > maxlen)) {
		return -1;
	}
	memcpy(out, buf + 2, buf[1]);
	return buf[1];
}

void dump(const char* str, const void* ptr, unsigned len) {
	const uint8_t* x = ptr;
	fprintf(stderr, "%s", str);
	while (len > 0) {
		fprintf(stderr, " %02x", *x++);
		len--;
	}
	fprintf(stderr, "\n");
}

int dap_cmd(DC* dc, const void* tx, unsigned txlen, void* rx, unsigned rxlen) {
	uint8_t cmd = ((const uint8_t*) tx)[0];
	dump("TX>", tx, txlen);
	if (usb_write(dc->usb, tx, txlen) != txlen) {
		fprintf(stderr, "dap_cmd(0x%02x): usb write error\n", cmd);
		return -1;
	}
	int sz = usb_read(dc->usb, rx, rxlen);
	if (sz < 1) {
		fprintf(stderr, "dap_cmd(0x%02x): usb read error\n", cmd);
		return -1;
	}
	dump("RX>", rx, rxlen);
	if (((uint8_t*) rx)[0] != cmd) {
		fprintf(stderr, "dap_cmd(0x%02x): unsupported (0x%02x)\n",
			cmd, ((uint8_t*) rx)[0]);
		return -1;
	}
	fprintf(stderr, "dap_cmd(0x%02x): sz %u\n", cmd, sz);
	return sz;
}

int dap_cmd_std(DC* dc, const char* name, uint8_t* io,
		unsigned txlen, unsigned rxlen) {
	if (dap_cmd(dc, io, txlen, io, rxlen) < 0) {
		return -1;
	}
	if (io[1] != 0) {
		fprintf(stderr, "%s status 0x%02x\n", name, io[1]);
		return -1;
	}
	return 0;
}

int dap_connect(DC* dc) {
	uint8_t io[2] = { DAP_Connect, PORT_SWD };
	return dap_cmd_std(dc, "dap_connect()", io, 2, 2);
}

int dap_swd_configure(DC* dc, unsigned cfg) {
	uint8_t io[2] = { DAP_SWD_Configure, cfg };
	return dap_cmd_std(dc, "dap_swd_configure()", io, 2, 2);
}

int dap_transfer_configure(DC* dc, unsigned idle, unsigned wait, unsigned match) {
	uint8_t io[6] = { DAP_TransferConfigure, idle, wait, wait >> 8, match, match >> 8};
	return dap_cmd_std(dc, "dap_transfer_configure()", io, 6, 2);
}

int dap_xfer_wr1(DC* dc, unsigned cfg, uint32_t val) {
	uint8_t io[8] = {
		DAP_Transfer, 0, 1, cfg & 0x0D,
		val, val >> 8, val >> 16, val >> 24 };
	if (dap_cmd(dc, io, 8, io, 3) < 0) {
		return -1;
	}
	if (io[1] != 0) {
		fprintf(stderr, "dap_xfer_wr1() invalid count %u\n", io[1]);
		return -1;
	}
	if (io[2] == RSP_ACK_OK) {
		return 0;
	}
	fprintf(stderr, "dap_xfer_wr1() status 0x%02x\n", io[2]);
	return -1;
}

int dap_xfer_rd1(DC* dc, unsigned cfg, uint32_t* val) {
	uint8_t io[8] = {
		DAP_Transfer, 0, 1, XFER_Read | (cfg & 0x0D) };
	if (dap_cmd(dc, io, 4, io, 7) < 0) {
		return -1;
	}
	if (io[1] != 1) {
		fprintf(stderr, "dap_xfer_rd1() invalid count %u\n", io[1]);
		return -1;
	}
	if (io[2] == RSP_ACK_OK) {
		memcpy(val, io + 3, 4);
		return 0;
	}
	fprintf(stderr, "dap_xfer_rd1() status 0x%02x\n", io[2]);
	return -1;
}

int swd_init(DC* dc) {
	uint8_t io[23] = {
		DAP_SWD_Sequence, 3,
		0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 64 1s
		0x10, 0x9E, 0xE7, // JTAG to SWD magic sequence
		0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, // 60 1s, 4 0s
	};
	if (dap_cmd(dc, io, 23, io, 2) < 0) {
		return -1;
	}
	if (io[1] != 0) {
		fprintf(stderr, "swd_init() failure 0x%02x\n", io[1]);
		return -1;
	}
	return 0;
}

int dc_init(DC* dc, usb_handle* usb) {
	uint8_t buf[256 + 2];
	uint32_t n32;
	uint16_t n16;
	uint8_t n8;

	memset(dc, 0, sizeof(DC));
	dc->usb = usb;
	dc->max_packet_count = 1;
	dc->max_packet_size = 64;

	buf[0] = DAP_Info;
	for (unsigned n = 0; n < 10; n++) {
		int sz = dap_get_info(dc, n, buf, 0, 255);
		if (sz > 0) {
			buf[sz] = 0;
			printf("0x%02x: '%s'\n", n, (char*) buf);
		}
	}

	buf[0] = 0; buf[1] = 0;
	if (dap_get_info(dc, DI_Capabilities, buf, 1, 2) > 0) {
		printf("Capabilities: 0x%02x 0x%02x\n", buf[0], buf[1]);
		printf("Capabilities:");
		if (buf[0] & I0_SWD) printf(" SWD");
		if (buf[0] & I0_JTAG) printf(" JTAG");
		if (buf[0] & I0_SWO_UART) printf(" SWO(UART)");
		if (buf[0] & I0_SWO_Manchester) printf(" SWO(Manchester)");
		if (buf[0] & I0_Atomic_Commands) printf(" ATOMIC");
		if (buf[0] & I0_Test_Domain_Timer) printf(" TIMER");
		if (buf[0] & I0_SWO_Streaming_Trace) printf(" SWO(Streaming)");
		if (buf[0] & I0_UART_Comm_Port) printf(" UART");
		if (buf[1] & I1_USB_COM_Port) printf(" USBCOM");
		printf("\n");
	}
	if (dap_get_info(dc, DI_UART_RX_Buffer_Size, &n32, 4, 4) == 4) {
		printf("UART RX Buffer Size: %u\n", n32);
	}
	if (dap_get_info(dc, DI_UART_TX_Buffer_Size, &n32, 4, 4) == 4) {
		printf("UART TX Buffer Size: %u\n", n32);
	}
	if (dap_get_info(dc, DI_SWO_Trace_Buffer_Size, &n32, 4, 4) == 4) {
		printf("SWO Trace Buffer Size: %u\n", n32);
	}
	if (dap_get_info(dc, DI_Max_Packet_Count, &n8, 1, 1) == 1) {
		printf("Max Packet Count: %u\n", n8);
		dc->max_packet_count = n8;
	}
	if (dap_get_info(dc, DI_Max_Packet_Size, &n16, 2, 2) == 2) {
		printf("Max Packet Size: %u\n", n16);
		dc->max_packet_size = n16;
	}

	if ((dc->max_packet_count < 1) || (dc->max_packet_size < 64)) {
		fprintf(stderr, "dc_init() impossible packet configuration\n");
		return -1;
	}

	return 0;
}

int main(int argc, char **argv) {
	usb_handle *usb;
	uint32_t n;

	usb = usb_open(0x1fc9, 0x0143, 0);
	if (usb == 0) {
		usb = usb_open(0x2e8a, 0x000c, 42);
		if (usb == 0) {
			fprintf(stderr, "cannot find device\n");
			return -1;
		}
	}

	DC context;
	DC* dc = &context;
	dc_init(dc, usb);

	dap_connect(dc);
	dap_swd_configure(dc, CFG_Turnaround_1);
	dap_transfer_configure(dc, 8, 64, 0);
	//dap_xfer_wr1(dc, XFER_DebugPort | XFER_Addr_04, 0x11553311);
	swd_init(dc);
	n = 0;
	dap_xfer_rd1(dc, XFER_DebugPort, &n);
	printf("IDCODE %08x\n", n);
	
	return 0;
}

	


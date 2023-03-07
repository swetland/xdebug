// Copyright 2014, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <libusb-1.0/libusb.h>

#include "usb.h"

struct usb_handle {
	libusb_device_handle *dev;
	unsigned ei;
	unsigned eo;
};

int get_sysfs_path(libusb_device* dev, char* path, int max) {
	if (max < 0) return -1;
	uint8_t num[8];
	uint8_t bus = libusb_get_bus_number(dev);
	int count = libusb_get_port_numbers(dev, num, 8);
	if (count < 1) return -1;
	int r = snprintf(path, max, "/sys/bus/usb/devices/%u-%u", bus, num[0]);
	if ((r >= max) || (r < 0)) return -1;
	int len = r;
	for (unsigned n = 1; n < count; n++) {
		r = snprintf(path + len, max - len, ".%u", num[n]);
		if ((r >= (max - len)) || (r < 0)) return -1;
		len += r;
	}
	return len;
}

int get_vendor_bulk_ifc(struct libusb_config_descriptor *cd, uint8_t* iifc,
			uint8_t *ino, uint8_t *eptin, uint8_t *eptout) {
	const struct libusb_interface_descriptor *id;
	for (unsigned i = 0; i < cd->bNumInterfaces; i++) {
		if (cd->interface[i].num_altsetting != 1) {
			// keep it simple: skip interfaces with alt settings
			continue;
		}
		id = &cd->interface[i].altsetting[0];
		if (id->bInterfaceClass != 0xFF) {
			// require vendor ifc class
			continue;
		}
		if (id->bNumEndpoints != 2) {
			// must have two endpoints
			continue;
		}
		// look for one bulk in, one bulk out
		const struct libusb_endpoint_descriptor *e0 = id->endpoint + 0;
		const struct libusb_endpoint_descriptor *e1 = id->endpoint + 1;
		if ((e0->bmAttributes & 3) != LIBUSB_ENDPOINT_TRANSFER_TYPE_BULK) {
			continue;
		}
		if ((e1->bmAttributes & 3) != LIBUSB_ENDPOINT_TRANSFER_TYPE_BULK) {
			continue;
		}
		if ((e0->bEndpointAddress & 0x80) &&
		    (!(e1->bEndpointAddress & 0x80))) {
			*eptin = e0->bEndpointAddress;
			*eptout = e1->bEndpointAddress;
			goto match;
		} else if ((e1->bEndpointAddress & 0x80) &&
			   (!(e0->bEndpointAddress & 0x80))) {
			*eptin = e1->bEndpointAddress;
			*eptout = e0->bEndpointAddress;
			goto match;
		} else {
			continue;
		}
	}
	return -1;
match:
	*ino = id->bInterfaceNumber;
	*iifc = id->iInterface;
	return 0;
}

usb_handle *usb_try_open(libusb_device* dev, const char* sn,
			unsigned isn, unsigned iifc,
			unsigned ino, unsigned ei, unsigned eo) {
	unsigned char text[256];
	usb_handle *usb;
	int r;

	usb = malloc(sizeof(usb_handle));
	if (usb == 0) {
		return NULL;
	}

	if (libusb_open(dev, &usb->dev) < 0) {
		goto fail;
	}

	if (sn && (isn != 0)) {
		r = libusb_get_string_descriptor_ascii(usb->dev, isn, text, 256);
		if (r < 0) {
			goto fail;
		}
		if (strlen(sn) != r) {
			goto fail;
		}
		if (memcmp(text, sn, r)) {
			goto fail;
		}
	}
	if (iifc != 0) {
		r = libusb_get_string_descriptor_ascii(usb->dev, iifc, text, 255);
		if (r < 0) {
			goto fail;
		}
		text[r] = 0;
		if (!strstr((void*)text, "CMSIS-DAP")) {
			goto fail;
		}
	}

	usb->ei = ei;
	usb->eo = eo;

	// This causes problems on re-attach.  Maybe need for OSX?
	// On Linux it's completely happy without us explicitly setting a configuration.
	//r = libusb_set_configuration(usb->dev, 1);
	r = libusb_claim_interface(usb->dev, ino);
	if (r < 0) {
		fprintf(stderr, "failed to claim interface #%d\n", ino);
		goto close_fail;
	}

#ifdef __APPLE__
	// make sure everyone's data toggles agree
	// makes things worse on Linux, but happy on OSX
	libusb_clear_halt(usb->dev, usb->ei);
	libusb_clear_halt(usb->dev, usb->eo);
#endif

	return usb;

close_fail:
	libusb_close(usb->dev);
fail:
	free(usb);
	return NULL;
}

static int read_sysfs(const char* path, char* text, int len) {
	int fd;
	if ((fd = open(path, O_RDONLY)) < 0) {
		return -1;
	}
	int r = read(fd, text, len);
	if ((r < 1) || (text[r-1] != '\n')) {
		return -1;
	}
	text[r-1] = 0;
	return 0;
}

static libusb_context *usb_ctx = NULL;

usb_handle *usb_open(unsigned vid, unsigned pid, const char* sn) {
	usb_handle *usb = NULL;

	if (usb_ctx == NULL) {
		if (libusb_init(&usb_ctx) < 0) {
			usb_ctx = NULL;
			return NULL;
		}
	}

	uint8_t ino, eo, ei, iifc;
	libusb_device** list;
	int count = libusb_get_device_list(usb_ctx, &list);
	for (int n = 0; n < count; n++) {
		struct libusb_device_descriptor dd;
		//const struct libusb_interface_descriptor *id;
		if (libusb_get_device_descriptor(list[n], &dd) != 0) {
			continue;
		}
		int isn = dd.iSerialNumber;
		if (vid) {
			// exact match requested
			if ((vid != dd.idVendor) || (pid != dd.idProduct)) {
				continue;
			}
		} else if (dd.bDeviceClass == 0x00) {
			// use interface class: okay
		} else if (dd.bDeviceClass == 0xFF) {
			// vendor class: okay
		} else if ((dd.bDeviceClass == 0xEF) &&
			   (dd.bDeviceSubClass == 0x02) &&
			   (dd.bDeviceProtocol == 0x01)) {
			// interface association descriptor: okay
		} else {
			continue;
		}
		struct libusb_config_descriptor *cd;
		if (libusb_get_active_config_descriptor(list[n], &cd) != 0) {
			continue;
		}
		int r = get_vendor_bulk_ifc(cd, &iifc, &ino, &ei, &eo);
		libusb_free_config_descriptor(cd);
		if (r != 0) {
			continue;
		}
#if 0
		printf("%02x %02x %02x %04x %04x %02x %02x %02x\n",
			dd.bDeviceClass,
			dd.bDeviceSubClass,
			dd.bDeviceProtocol,
			dd.idVendor,
			dd.idProduct,
			ino, ei, eo);
#endif
		// try to validate serialno and interface description
		// using sysfs so we don't need to open the device
		// to rule it out
		char path[512];
		char text[256];
		int len = get_sysfs_path(list[n], path, 512 - 64);
		if (len < 0) {
			// should never happen, but just in case
			continue;
		}
		if (sn) {
			sprintf(path + len, "/serial");
			if (read_sysfs(path, text, sizeof(text)) == 0) {
				if (strcmp(sn, text)) {
					continue;
				}
				// matched here, so don't check after libusb_open()
				isn = 0;
			}
		}
		if (vid == 0) {
			// if we're wildcarding it, check interface
			sprintf(path + len, ":%u.%u/interface", 1, 0);
			if (read_sysfs(path, text, sizeof(text)) == 0) {
				if (strstr(text, "CMSIS-DAP") == 0) {
					continue;
				}
				// matched here, so don't check after libusb_open()
				iifc = 0;
			}
		} else {
			// if not wildcarding, don't enforce this check in usb_try_open()
			iifc = 0;
		}

		if ((usb = usb_try_open(list[n], sn, isn, iifc, ino, ei, eo)) != NULL) {
			break;
		}
	}
	if (count >= 0) {
		libusb_free_device_list(list, 1);
	}
	return usb;
}

void usb_close(usb_handle *usb) {
	libusb_close(usb->dev);
	free(usb);
}

int usb_ctrl(usb_handle *usb, void *data,
	uint8_t typ, uint8_t req, uint16_t val, uint16_t idx, uint16_t len) {
	if (usb == NULL) {
		return LIBUSB_ERROR_NO_DEVICE;
	}
	int r = libusb_control_transfer(usb->dev, typ, req, val, idx, data, len, 5000);
	if (r < 0) {
		return -1;
	} else {
		return r;
	}
}

int usb_read(usb_handle *usb, void *data, int len) {
	if (usb == NULL) {
		return LIBUSB_ERROR_NO_DEVICE;
	}
	int xfer = len;
	int r = libusb_bulk_transfer(usb->dev, usb->ei, data, len, &xfer, 5000);
	if (r < 0) {
		return -1;
	}
	return xfer;
}

int usb_read_forever(usb_handle *usb, void *data, int len) {
	if (usb == NULL) {
		return LIBUSB_ERROR_NO_DEVICE;
	}
	int xfer = len;
	int r = libusb_bulk_transfer(usb->dev, usb->ei, data, len, &xfer, 0);
	if (r < 0) {
		return -1;
	}
	return xfer;
}

int usb_write(usb_handle *usb, const void *data, int len) {
	if (usb == NULL) {
		return LIBUSB_ERROR_NO_DEVICE;
	}
	int xfer = len;
	int r = libusb_bulk_transfer(usb->dev, usb->eo, (void*) data, len, &xfer, 5000);
	if (r < 0) {
		return -1;
	}
	return xfer;
}


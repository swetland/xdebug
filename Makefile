
all: bin/xdebug

SRCS := src/xdebug.c src/usb.c
DEPS := src/arm-debug.h src/cmsis-dap-protocol.h src/usb.h

bin/xdebug: $(SRCS) $(DEPS)
	@mkdir -p $(dir $@)
	gcc -o $@ -Wall -g -O1 $(SRCS) -lusb-1.0

clean:
	rm -f bin/


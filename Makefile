
all: out/xdebug out/xtest

CFLAGS := -Wall -g -O1
CFLAGS += -Itui -Itermbox -D_XOPEN_SOURCE
LIBS := -lusb-1.0

COMMON := src/transport-arm-debug.c src/transport-dap.c src/usb.c
XTESTSRCS := src/xtest.c $(COMMON)
XTESTOBJS := $(addprefix out/,$(patsubst %.c,%.o,$(filter %.c,$(XTESTSRCS))))

XDEBUGSRCS := src/xdebug.c src/commands.c $(COMMON)
XDEBUGSRCS += tui/tui.c termbox/termbox.c termbox/utf8.c
XDEBUGOBJS := $(addprefix out/,$(patsubst %.c,%.o,$(filter %.c,$(XDEBUGSRCS))))

out/xtest: $(XTESTOBJS)
	@mkdir -p $(dir $@)
	gcc -o $@ -Wall -g -O1 $(XTESTOBJS) $(LIBS)

out/xdebug: $(XDEBUGOBJS)
	@mkdir -p $(dir $@)
	gcc -o $@ -Wall -g -O1 $(XDEBUGOBJS) $(LIBS)

# remove dups
OBJS := $(sort $(XTESTOBJS) $(XDEBUGOBJS))

$(OBJS): out/%.o: %.c $(XDEPS)
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -MD -MP -MT $@ -MF $(@:%o=%d) -o $@

-include $(OBJS:%o=%d)

clean:
	rm -rf out/


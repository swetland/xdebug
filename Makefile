
all: out/xdebug

CFLAGS := -Wall -g -O1

SRCS := src/xdebug.c src/transport-arm-debug.c src/transport-dap.c src/usb.c
LIBS := -lusb-1.0

OBJS := $(addprefix out/,$(patsubst %.c,%.o,$(filter %.c,$(SRCS))))

out/xdebug: $(OBJS)
	@mkdir -p $(dir $@)
	gcc -o $@ -Wall -g -O1 $(OBJS) $(LIBS)

$(OBJS): out/%.o: %.c $(XDEPS)
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -MD -MP -MT $@ -MF $(@:%o=%d) -o $@

-include $(OBJS:%o=%d)

clean:
	rm -rf out/


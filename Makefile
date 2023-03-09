
all: out/xdebug out/xtest

CFLAGS := -Wall -g -O1
CFLAGS += -Itui -Itermbox -D_XOPEN_SOURCE
LIBS := -lusb-1.0

# TOOLCHAIN := arm-none-eabi-

ifneq ($(TOOLCHAIN),)
# if there's a cross-compiler, build agents from source

TARGET_CC := $(TOOLCHAIN)gcc
TARGET_OBJCOPY := $(TOOLCHAIN)objcopy
TARGET_OBJDUMP := $(TOOLCHAIN)objdump

ARCH_M3_CFLAGS := -mcpu=cortex-m3 -mthumb
ARCH_M3_LIBS := $(shell $(TARGET_CC) $(ARCH_M3_CFLAGS) -print-libgcc-file-name)

ARCH_M0_CFLAGS := -mcpu=cortex-m0 -mthumb
ARCH_M0_LIBS := $(shell $(TARGET_CC) $(ARCH_M0_CFLAGS) -print-libgcc-file-name)

TARGET_CFLAGS := -g -Os -Wall -I. -Iinclude
TARGET_CFLAGS += -Wno-unused-but-set-variable
TARGET_CFLAGS += -ffunction-sections -fdata-sections
TARGET_CFLAGS += -fno-builtin -nostdlib -ffreestanding

agent = $(eval AGENTS += $(strip $1))\
$(eval ALL += $(patsubst %,out/agents/%.bin,$(strip $1)))\
$(eval ALL += $(patsubst %,out/agents/%.lst,$(strip $1)))\
$(eval out/agents/$(strip $1).elf: LOADADDR := $(strip $2))\
$(eval out/agents/$(strip $1).elf: ARCH := $(strip $3))

$(call agent, lpclink2,  0x10080400, M3)
$(call agent, stm32f4xx, 0x20000400, M3)
$(call agent, stm32f0xx, 0x20000400, M0)
$(call agent, lpc13xx,   0x10000400, M3)
$(call agent, lpc15xx,   0x02000400, M3)
$(call agent, cc13xx,    0x20000400, M3)
$(call agent, nrf528xx,  0x20000400, M3)
$(call agent, efr32bg2x, 0x20000400, M3)
$(call agent, pico,      0x20000400, M0)

out/mkbuiltins: tools/mkbuiltins.c
	@mkdir -p $(dir $@)
	gcc -o $@ $(CFLAGS) $<

AGENT_BINS := $(patsubst %,out/agents/%.bin,$(AGENTS))

gen/builtins.c: $(AGENT_BINS) out/mkbuiltins
	@mkdir -p $(dir $@)
	./out/mkbuiltins $(AGENT_BINS) > $@

out/agents/%.bin: out/agents/%.elf
	@mkdir -p $(dir $@)
	$(TARGET_OBJCOPY) -O binary $< $@

out/agents/%.lst: out/agents/%.elf
	@mkdir -p $(dir $@)
	$(TARGET_OBJDUMP) -d $< > $@

out/agents/%.elf: agents/%.c
	@mkdir -p $(dir $@)
	$(TARGET_CC) $(TARGET_CFLAGS) $(ARCH_$(ARCH)_CFLAGS) -Wl,--script=include/agent.ld -Wl,-Ttext=$(LOADADDR) -o $@ $< $(ARCH_$(ARCH)_LIBS)

endif

COMMON := src/transport-arm-debug.c src/transport-dap.c src/usb.c
XTEST_SRCS := src/xtest.c $(COMMON)
XTEST_OBJS := $(addprefix out/,$(patsubst %.c,%.o,$(filter %.c,$(XTEST_SRCS))))

XDEBUG_SRCS := src/xdebug.c $(COMMON)
XDEBUG_SRCS += src/commands.c src/commands-file.c src/commands-agent.c
XDEBUG_SRCS += tui/tui.c termbox/termbox.c termbox/utf8.c gen/builtins.c
XDEBUG_OBJS := $(addprefix out/,$(patsubst %.c,%.o,$(filter %.c,$(XDEBUG_SRCS))))

out/xtest: $(XTEST_OBJS)
	@mkdir -p $(dir $@)
	gcc -o $@ -Wall -g -O1 $(XTEST_OBJS) $(LIBS)

out/xdebug: $(XDEBUG_OBJS)
	@mkdir -p $(dir $@)
	gcc -o $@ -Wall -g -O1 $(XDEBUG_OBJS) $(LIBS)

# remove dups
OBJS := $(sort $(XTEST_OBJS) $(XDEBUG_OBJS))

$(OBJS): out/%.o: %.c $(XDEPS)
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -MD -MP -MT $@ -MF $(@:%o=%d) -o $@

-include $(OBJS:%o=%d)

clean:
	rm -rf out/


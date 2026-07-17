TARGET      ?= x86_64-elf

CONFIG_FILE := .config

-include $(CONFIG_FILE)

CONFIG_SERIAL ?= y
CONFIG_FRAMEBUFFER ?= y
CONFIG_PIT ?= y
CONFIG_KEYBOARD ?= y
CONFIG_GDT ?= y
CONFIG_INTERRUPTS ?= y
CONFIG_PMM ?= y
CONFIG_VMM ?= y
CONFIG_HEAP ?= y
CONFIG_SCHEDULER ?= y
CONFIG_PROG_HELP ?= y
CONFIG_PROG_WHOAMI ?= y
CONFIG_PROG_USERS ?= y
CONFIG_PROG_ADDUSER ?= y
CONFIG_PROG_UNAME ?= y
CONFIG_PROG_UPTIME ?= y
CONFIG_PROG_MEMINFO ?= y
CONFIG_PROG_REBOOT ?= y
CONFIG_USER_ADDED_PROGRAMS ?= n

HAVE_CROSS_GCC := $(shell command -v $(TARGET)-gcc 2>/dev/null)

ifeq ($(HAVE_CROSS_GCC),)
  CC := clang --target=$(TARGET)
  LD := ld.lld
else
  CC := $(TARGET)-gcc
  LD := $(TARGET)-ld
endif

BUILD_DIR   := build
KERNEL_DIR  := kernel
USERSPACE_DIR := userspace
ISO_DIR     := iso_root
ISO_NAME    := InnovatiOS.iso

CONFIG_OPTIONS := \
	CONFIG_SERIAL \
	CONFIG_FRAMEBUFFER \
	CONFIG_PIT \
	CONFIG_KEYBOARD \
	CONFIG_GDT \
	CONFIG_INTERRUPTS \
	CONFIG_PMM \
	CONFIG_VMM \
	CONFIG_HEAP \
	CONFIG_SCHEDULER \
	CONFIG_PROG_HELP \
	CONFIG_PROG_WHOAMI \
	CONFIG_PROG_USERS \
	CONFIG_PROG_ADDUSER \
	CONFIG_PROG_UNAME \
	CONFIG_PROG_UPTIME \
	CONFIG_PROG_MEMINFO \
	CONFIG_PROG_REBOOT \
	CONFIG_USER_ADDED_PROGRAMS

CFLAGS := -g -O2 -pipe \
          -Wall -Wextra \
          -std=gnu11 \
          -ffreestanding \
          -fno-stack-protector \
          -fno-stack-check \
          -fno-lto \
          -fno-pic -fno-pie \
          -m64 -march=x86-64 \
          -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
          -mno-red-zone \
          -mcmodel=kernel \
          -I$(KERNEL_DIR)/include \
          -I$(USERSPACE_DIR)/include

CFLAGS += $(foreach option,$(CONFIG_OPTIONS),$(if $(filter y,$($(option))),-D$(option)=1,))

LDFLAGS := -nostdlib -static -m elf_x86_64 \
           -z max-page-size=0x1000 \
           -T $(KERNEL_DIR)/linker.ld

CORE_CFILES := $(KERNEL_DIR)/src/main.c $(KERNEL_DIR)/src/lib/lib.c

CFILES := $(CORE_CFILES)
CFILES += $(if $(filter y,$(CONFIG_SERIAL)),$(KERNEL_DIR)/src/drivers/serial.c)
CFILES += $(if $(filter y,$(CONFIG_FRAMEBUFFER)),$(KERNEL_DIR)/src/drivers/framebuffer.c)
CFILES += $(if $(filter y,$(CONFIG_PIT)),$(KERNEL_DIR)/src/drivers/pit.c)
CFILES += $(if $(filter y,$(CONFIG_KEYBOARD)),$(KERNEL_DIR)/src/drivers/keyboard.c)
CFILES += $(if $(filter y,$(CONFIG_GDT)),$(KERNEL_DIR)/src/arch/x86_64/gdt.c)
CFILES += $(if $(filter y,$(CONFIG_INTERRUPTS)),$(KERNEL_DIR)/src/arch/x86_64/idt.c)
CFILES += $(if $(filter y,$(CONFIG_INTERRUPTS)),$(KERNEL_DIR)/src/arch/x86_64/pic.c)
CFILES += $(if $(filter y,$(CONFIG_INTERRUPTS)),$(KERNEL_DIR)/src/arch/x86_64/interrupts.c)
CFILES += $(if $(filter y,$(CONFIG_PMM)),$(KERNEL_DIR)/src/mm/pmm.c)
CFILES += $(if $(filter y,$(CONFIG_VMM)),$(KERNEL_DIR)/src/mm/vmm.c)
CFILES += $(if $(filter y,$(CONFIG_HEAP)),$(KERNEL_DIR)/src/mm/heap.c)
CFILES += $(if $(filter y,$(CONFIG_SCHEDULER)),$(KERNEL_DIR)/src/proc/scheduler.c)

USERSPACE_CFILES :=

ALL_PROG_CFILES := $(wildcard $(USERSPACE_DIR)/programs/*.c)
DEFAULT_PROG_CFILES := $(USERSPACE_DIR)/programs/helpers.c \
	$(USERSPACE_DIR)/programs/help.c \
	$(USERSPACE_DIR)/programs/whoami.c \
	$(USERSPACE_DIR)/programs/users.c \
	$(USERSPACE_DIR)/programs/adduser.c \
	$(USERSPACE_DIR)/programs/uname.c \
	$(USERSPACE_DIR)/programs/uptime.c \
	$(USERSPACE_DIR)/programs/meminfo.c \
	$(USERSPACE_DIR)/programs/reboot.c

USER_ADDED_PROGS_C := $(filter-out $(DEFAULT_PROG_CFILES),$(ALL_PROG_CFILES))

ifeq ($(CONFIG_FRAMEBUFFER),y)
CFILES += $(KERNEL_DIR)/src/drivers/console.c
USERSPACE_CFILES += $(USERSPACE_DIR)/src/user.c
USERSPACE_CFILES += $(USERSPACE_DIR)/src/shell.c
USERSPACE_CFILES += $(USERSPACE_DIR)/programs/helpers.c
USERSPACE_CFILES += $(if $(filter y,$(CONFIG_PROG_HELP)),$(USERSPACE_DIR)/programs/help.c)
USERSPACE_CFILES += $(if $(filter y,$(CONFIG_PROG_WHOAMI)),$(USERSPACE_DIR)/programs/whoami.c)
USERSPACE_CFILES += $(if $(filter y,$(CONFIG_PROG_USERS)),$(USERSPACE_DIR)/programs/users.c)
USERSPACE_CFILES += $(if $(filter y,$(CONFIG_PROG_ADDUSER)),$(USERSPACE_DIR)/programs/adduser.c)
USERSPACE_CFILES += $(if $(filter y,$(CONFIG_PROG_UNAME)),$(USERSPACE_DIR)/programs/uname.c)
USERSPACE_CFILES += $(if $(filter y,$(CONFIG_PROG_UPTIME)),$(USERSPACE_DIR)/programs/uptime.c)
USERSPACE_CFILES += $(if $(filter y,$(CONFIG_PROG_MEMINFO)),$(USERSPACE_DIR)/programs/meminfo.c)
USERSPACE_CFILES += $(if $(filter y,$(CONFIG_PROG_REBOOT)),$(USERSPACE_DIR)/programs/reboot.c)
USERSPACE_CFILES += $(if $(filter y,$(CONFIG_USER_ADDED_PROGRAMS)),$(USER_ADDED_PROGS_C))
endif

SFILES := $(if $(filter y,$(CONFIG_INTERRUPTS)),$(KERNEL_DIR)/src/arch/x86_64/interrupts_asm.S)

OBJS   := $(patsubst $(KERNEL_DIR)/src/%.c,$(BUILD_DIR)/kernel/%.o,$(CFILES))
OBJS   += $(patsubst $(USERSPACE_DIR)/%.c,$(BUILD_DIR)/userspace/%.o,$(USERSPACE_CFILES))
OBJS   += $(patsubst $(KERNEL_DIR)/src/%.S,$(BUILD_DIR)/kernel/%.o,$(SFILES))

KERNEL_ELF := $(BUILD_DIR)/kernel.elf

.PHONY: all clean iso run config menuconfig defconfig FORCE

all: $(USERSPACE_DIR)/include/user_programs.h $(KERNEL_ELF)

GEN_USER_PROGS_H := $(USERSPACE_DIR)/include/user_programs.h

$(GEN_USER_PROGS_H): FORCE
	@mkdir -p $(dir $@)
	@echo "/* Auto-generated */" > $@.tmp
ifeq ($(CONFIG_USER_ADDED_PROGRAMS),y)
	@$(foreach p,$(patsubst $(USERSPACE_DIR)/programs/%.c,%,$(USER_ADDED_PROGS_C)), \
		echo "void prog_$(p)(user_t *user, const char *args);" >> $@.tmp;)
endif
	@cmp -s $@ $@.tmp || mv $@.tmp $@
	@rm -f $@.tmp

FORCE:


config:
	@bash tools/menuconfig.sh

menuconfig: config

defconfig:
	@bash tools/menuconfig.sh --defconfig


$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/src/%.c $(CONFIG_FILE)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/userspace/%.o: $(USERSPACE_DIR)/%.c $(CONFIG_FILE)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/src/%.S $(CONFIG_FILE)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(GEN_USER_PROGS_H) $(OBJS) $(CONFIG_FILE)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

iso: $(KERNEL_ELF)
	mkdir -p $(ISO_DIR)/boot $(ISO_DIR)/EFI/BOOT
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/kernel.elf
	cp limine.conf $(ISO_DIR)/
	cp limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin $(ISO_DIR)/
	cp limine/BOOTX64.EFI $(ISO_DIR)/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		-b limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		-hfsplus \
		-apm-block-size 2048 \
		--efi-boot limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_DIR) -o $(ISO_NAME)
	./limine/limine bios-install $(ISO_NAME)

run: iso
	qemu-system-x86_64 -M q35 -m 512M -cdrom $(ISO_NAME) -serial stdio

clean:
	rm -rf $(BUILD_DIR) $(ISO_NAME) $(ISO_DIR)

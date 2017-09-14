#
# Architecture special makerules for x86 family
# (including x86_32, x86_32y and x86_64).
#

MINIOS_TARGET_ARCHS := x86_32 x86_64

ifeq ($(MINIOS_TARGET_ARCH),x86_32)
ARCH_CFLAGS  := -m32 -march=i686
ARCH_LDFLAGS := -m elf_i386
ARCH_ASFLAGS := -m32
EXTRA_INC += $(TARGET_ARCH_FAM)/$(MINIOS_TARGET_ARCH)
EXTRA_SRC += arch/$(EXTRA_INC)
endif

ifeq ($(MINIOS_TARGET_ARCH),x86_64)
ARCH_CFLAGS := -m64 -mno-red-zone -fno-reorder-blocks
ARCH_CFLAGS += -fno-asynchronous-unwind-tables
ARCH_ASFLAGS := -m64
ARCH_LDFLAGS := -m elf_x86_64
EXTRA_INC += $(TARGET_ARCH_FAM)/$(MINIOS_TARGET_ARCH)
EXTRA_SRC += arch/$(EXTRA_INC)
endif

ifeq ($(CONFIG_PARAVIRT),n)
ARCH_LDFLAGS_FINAL := --oformat=elf32-i386
ARCH_AS_DEPS += x86_hvm.S
endif

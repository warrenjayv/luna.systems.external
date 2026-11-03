CC = gcc
AS = gcc
LD = ld

CFLAGS = -ffreestanding -02 -Wall -Wextra -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -Iinclude
ASFLAGS = -ffreestanding -Iinclude
LDFLAGS = -T linker.ld -nostdlib

# source

BOOT_SRC = boot/multiboot2.S
C_SRCS = kernel/main.c arch/x86/cpu.c arch/x86/vmx.c mm/mm.c


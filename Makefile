# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
CC ?= gcc
QEMU32 ?= qemu-system-i386
QEMU64 ?= qemu-system-x86_64
LD ?= ld
PYTHON ?= python3
FS_PROFILE ?= profiles/filesystems.mk
include $(FS_PROFILE)
COMMON = -std=c11 -Wall -Wextra -Wno-misleading-indentation -Wno-sign-compare -Ilib -Iinclude -I. $(FS_TX_CFLAGS) -DMULTIOS_FS_REGISTRY=\"$(FS_REGISTRY)\"
KFLAGS = $(COMMON) -MMD -MP -m32 -march=i386 -Wa,-march=i386 -Os -ffreestanding -fno-builtin -fno-pie -fno-pic -fstack-protector-strong -mstack-protector-guard=global -fno-asynchronous-unwind-tables -mno-sse -mno-mmx -msoft-float -fcf-protection=none
FS_LIB = lib/fs.c lib/fs_copy.c $(FS_DRIVERS)
# Standalone unit harnesses provide legacy disk/HAL stubs. The TX path is
# exercised separately by test-storage-tx with the real HAL and FS drivers.
FS_UNIT_LIB = $(filter-out lib/undo.c lib/storage_tx.c,$(FS_LIB))
FS_KOBJ = $(patsubst lib/%.c,build/%.o,$(FS_DRIVERS))
FS_EOBJ = $(patsubst lib/%.c,build/efi-%.o,$(FS_DRIVERS))
LIB = lib/util.c $(FS_LIB) lib/shell.c core/hal.c core/osk.c core/modules.c core/cpu.c core/hardware.c core/hid.c core/editor.c
PC_PROFILE ?= profiles/pc-bios.mk
include $(PC_PROFILE)
PC_DRIVER_OBJ = $(patsubst %.c,build/pc-driver/%.o,$(PC_DRIVERS))
KOBJ = build/usb_bot.o build/div64.o build/exception-stubs.o build/exceptions.o build/start.o build/platform.o build/util.o build/fs.o build/fs_copy.o $(FS_KOBJ) build/shell.o build/hal.o build/osk.o build/modules.o build/cpu.o build/hardware.o build/hid.o build/editor.o build/pc-cpu.o $(PC_DRIVER_OBJ)
.PHONY: all image run run64 test clean
all: build/multios.img build/multios-host
build:
	mkdir -p build
build/boot.o: platform/pc-bios/boot.S | build
	$(CC) -m32 -c $< -o $@
build/boot.bin: build/boot.o platform/pc-bios/boot.ld
	$(LD) -m elf_i386 -T platform/pc-bios/boot.ld $< -o $@
	$(PYTHON) -c 'assert len(open("$@","rb").read())==512'
build/start.o: platform/pc-bios/start.S | build
	$(CC) -m32 -c $< -o $@
build/pc-driver/%.o: %.c include/multios/modules.h platform/pc-bios/io.h $(PC_PROFILE) build/pc-profile.stamp | build
	mkdir -p $(dir $@)
	$(CC) $(KFLAGS) -c $< -o $@
build/platform.o: platform/pc-bios/platform.c platform/pc-bios/platform.h $(PC_REGISTRY) $(PC_PROFILE) build/pc-profile.stamp | build
	$(CC) $(KFLAGS) -DMULTIOS_PC_REGISTRY=\"$(PC_REGISTRY)\" -c $< -o $@
build/osk.o: core/osk.c include/multios/osk.h include/multios/hal.h lib/multi.h | build
	$(CC) $(KFLAGS) -c $< -o $@
build/editor.o: core/editor.c include/multios/editor.h | build
	$(CC) $(KFLAGS) -c $< -o $@
build/hardware.o: core/hardware.c include/multios/hardware.h | build
	$(CC) $(KFLAGS) -c $< -o $@
build/hid.o: core/hid.c include/multios/hid.h | build
	$(CC) $(KFLAGS) -c $< -o $@
build/cpu.o: core/cpu.c include/multios/cpu.h | build
	$(CC) $(KFLAGS) -c $< -o $@
build/pc-cpu.o: platform/x86/cpu.c platform/x86/cpu.h include/multios/cpu.h | build
	$(CC) $(KFLAGS) -c $< -o $@
build/modules.o: core/modules.c include/multios/modules.h | build
	$(CC) $(KFLAGS) -c $< -o $@
build/hal.o: core/hal.c include/multios/hal.h lib/multi.h | build
	$(CC) $(KFLAGS) -c $< -o $@
build/%.o: lib/%.c lib/multi.h include/multios/hal.h | build
	$(CC) $(KFLAGS) -c $< -o $@
build/kernel.elf: $(KOBJ) platform/pc-bios/link.ld $(PC_PROFILE) build/pc-profile.stamp
	$(LD) -m elf_i386 $(FS_KLDFLAGS) -T platform/pc-bios/link.ld $(KOBJ) -o $@
build/kernel.bin: build/kernel.elf
	objcopy -O binary $< $@
	$(PYTHON) -c 'assert len(open("$@","rb").read())<=240*512, "kernel exceeds loader limit"'
build/multios-host: platform/posix/host.c $(LIB) $(NTFS_HOST_OBJ) Makefile lib/multi.h $(wildcard include/multios/*.h) | build
	$(CC) $(COMMON) -O2 -Wl,--gc-sections platform/posix/host.c $(LIB) $(NTFS_HOST_OBJ) -o $@
build/multios.img: build/boot.bin build/kernel.bin tools/mkimage.py
	rm -f $@.tmp
	$(PYTHON) tools/mkimage.py $@.tmp --boot build/boot.bin --kernel build/kernel.bin
	mv $@.tmp $@
image: build/multios.img
run: build/multios.img
	$(QEMU32) -machine pc -m 32M -drive file=$<,format=raw,if=ide,index=0 -serial stdio -device isa-debug-exit,iobase=0xf4,iosize=0x04 || test $$? -eq 33
run64: build/multios.img
	$(QEMU64) -machine pc -m 32M -drive file=$<,format=raw,if=ide,index=0 -serial stdio -device isa-debug-exit,iobase=0xf4,iosize=0x04 || test $$? -eq 33
test: all test-hal check-core
	$(PYTHON) tests/test.py
	$(PYTHON) tests/edge_cases.py
	$(PYTHON) tests/faults.py
clean:
	rm -f build/*.o build/*.bin build/*.elf build/*.d build/*.stamp build/multios-host build/multios.img build/BOOTX64.EFI build/multios-uefi.so
	rm -rf build/pc-driver build/efi-driver

build/hal-test: tests/hal_test.c core/hal.c core/osk.c include/multios/hal.h lib/multi.h | build
	$(CC) $(COMMON) -UMULTIOS_STORAGE_TX -O2 tests/hal_test.c core/hal.c core/osk.c -o $@
.PHONY: test-hal check-core
test-hal: build/hal-test
	./build/hal-test
check-core:
	$(PYTHON) tests/check_core.py

# Standalone UEFI live target. Requires GNU-EFI headers/runtime, not an OS in the guest.
# Default to included source: distribution GNU-EFI may require SSE4.1 itself.
USE_SYSTEM_GNUEFI ?= 0
ifeq ($(USE_SYSTEM_GNUEFI),1)
EFIINC ?= /usr/include/efi
EFILIB ?= /usr/lib64
EFIGNUEFILIB ?= $(EFILIB)
EFILDS ?= $(EFILIB)/gnuefi/elf_x86_64_efi.lds
EFICRT ?= $(EFILIB)/gnuefi/crt0-efi-x86_64.o
else
EFIINC ?= build/gnu-efi-sdk/include/efi
EFILIB ?= build/gnu-efi-sdk/lib
EFIGNUEFILIB ?= $(EFILIB)/gnuefi
EFILDS ?= $(EFIGNUEFILIB)/elf_x86_64_efi.lds
EFICRT ?= $(EFIGNUEFILIB)/crt0-efi-x86_64.o
endif
# Do not inherit distribution defaults such as x86-64-v2: SSE4.1 is optional.
EFI_ARCH_FLAGS = -march=x86-64 -mtune=generic -fcf-protection=none
EFIFLAGS = $(EFI_ARCH_FLAGS) $(COMMON) -MMD -MP -I$(EFIINC) -I$(EFIINC)/x86_64 -DEFI_FUNCTION_WRAPPER -fpic -fshort-wchar -mno-red-zone -fno-builtin -fno-stack-protector -maccumulate-outgoing-args -ffreestanding -O2
UEFI_CONFIG ?= profiles/BOOT.CFG
UEFI_PROFILE ?= profiles/uefi.mk
include $(UEFI_PROFILE)
UEFI_DRIVER_OBJ = $(patsubst %.c,build/efi-driver/%.o,$(UEFI_DRIVERS))
EOBJ = build/efi-main.o build/efi-util.o build/efi-fs.o build/efi-fs_copy.o $(FS_EOBJ) build/efi-shell.o build/efi-hal.o build/efi-osk.o build/efi-modules.o build/efi-cpu.o build/efi-hardware.o build/efi-hid.o build/efi-editor.o build/efi-x86-cpu.o $(UEFI_DRIVER_OBJ) $(NTFS_EOBJ)
build/efi-editor.o: core/editor.c include/multios/editor.h | build
	$(CC) $(EFIFLAGS) -c $< -o $@
build/efi-hardware.o: core/hardware.c include/multios/hardware.h | build
	$(CC) $(EFIFLAGS) -c $< -o $@
build/efi-hid.o: core/hid.c include/multios/hid.h | build
	$(CC) $(EFIFLAGS) -c $< -o $@
build/efi-cpu.o: core/cpu.c include/multios/cpu.h | build
	$(CC) $(EFIFLAGS) -c $< -o $@
build/efi-x86-cpu.o: platform/x86/cpu.c platform/x86/cpu.h include/multios/cpu.h | build
	$(CC) $(EFIFLAGS) -c $< -o $@
build/efi-modules.o: core/modules.c include/multios/modules.h | build
	$(CC) $(EFIFLAGS) -c $< -o $@
build/efi-driver/%.o: %.c include/multios/modules.h platform/uefi/platform.h $(UEFI_PROFILE) build/uefi-profile.stamp | build
	mkdir -p $(dir $@)
	$(CC) $(EFIFLAGS) -c $< -o $@
build/efi-main.o: platform/uefi/main.c platform/uefi/platform.h $(UEFI_REGISTRY) $(UEFI_PROFILE) include/multios/modules.h build/uefi-profile.stamp | build
	$(CC) $(EFIFLAGS) -DMULTIOS_UEFI_REGISTRY=\"$(UEFI_REGISTRY)\" -c $< -o $@
build/efi-hal.o: core/hal.c include/multios/hal.h include/multios/osk.h | build
	$(CC) $(EFIFLAGS) -c $< -o $@
build/efi-osk.o: core/osk.c include/multios/hal.h include/multios/osk.h | build
	$(CC) $(EFIFLAGS) -c $< -o $@
build/efi-%.o: lib/%.c lib/multi.h include/multios/hal.h include/multios/osk.h | build
	$(CC) $(EFIFLAGS) -c $< -o $@
build/multios-uefi.so: $(EOBJ) $(EFILDS) $(UEFI_PROFILE) build/uefi-profile.stamp
	$(LD) -nostdlib -znocombreloc $(NTFS_LDFLAGS) -T $(EFILDS) -shared -Bsymbolic $(EFICRT) $(EOBJ) -L$(EFILIB) -L$(EFIGNUEFILIB) -lefi -lgnuefi -o $@
build/BOOTX64.EFI: build/multios-uefi.so
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc --target=efi-app-x86_64 $< $@
.PHONY: uefi usb
uefi: build/BOOTX64.EFI
usb: all uefi
	$(PYTHON) tools/mkusb.py --config $(UEFI_CONFIG)
build/usb-mbr.o: platform/uefi/usb-mbr.S | build
	$(CC) -m32 -c $< -o $@
build/usb-mbr.bin: build/usb-mbr.o platform/pc-bios/boot.ld
	$(LD) -m elf_i386 -T platform/pc-bios/boot.ld $< -o $@
usb: build/usb-mbr.bin
build/osk-test: tests/osk_test.c core/hal.c core/osk.c include/multios/osk.h | build
	$(CC) $(COMMON) -UMULTIOS_STORAGE_TX -O2 tests/osk_test.c core/hal.c core/osk.c -o $@
.PHONY: test-osk
test-osk: build/osk-test
	./build/osk-test
test: test-osk

.PHONY: test-modules
test-modules: build/modules-test
	./build/modules-test
build/modules-test: tests/modules_test.c core/modules.c core/hal.c core/osk.c lib/util.c include/multios/modules.h | build
	$(CC) $(COMMON) -UMULTIOS_STORAGE_TX -O2 tests/modules_test.c core/modules.c core/hal.c core/osk.c lib/util.c -o $@
test: test-modules

# Detect switching to an older profile/registry as well as editing it.
.PHONY: FORCE
FORCE:
build/pc-profile.stamp: FORCE | build
	@printf '%s\n' '$(PC_PROFILE)|$(PC_DRIVERS)|$(PC_REGISTRY)' > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@
build/uefi-profile.stamp: FORCE | build
	@printf '%s\n' '$(UEFI_PROFILE)|$(UEFI_DRIVERS)|$(UEFI_REGISTRY)|$(EFI_ARCH_FLAGS)|$(CC)|$(USE_SYSTEM_GNUEFI)|$(EFIINC)|$(EFILIB)|$(EFIGNUEFILIB)' > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@
-include $(wildcard build/*.d) $(PC_DRIVER_OBJ:.o=.d) $(UEFI_DRIVER_OBJ:.o=.d)

.PHONY: multipulti unified update-unified
build/MultiPulti32.bin: build/kernel.bin
	cp $< $@
multipulti: build/MultiPulti32.bin
	@printf 'MultiPulti32 | vm2 studios | t.me/v2vproj\n'
	@wc -c build/kernel.bin
unified: all uefi multipulti
	@if test -e build/MultiOS-MultiPulti.img; then printf 'Image exists: preserved. Use make update-unified for binary updates.\n'; else $(PYTHON) tools/mkunified.py; fi
update-unified: all uefi multipulti
	$(PYTHON) tools/imagectl.py install build/MultiOS-MultiPulti.img --boot build/boot.bin --kernel build/kernel.bin --efi build/BOOTX64.EFI

.PHONY: test-cpu test-cpu-matrix
build/cpu-test: tests/cpu_test.c core/cpu.c include/multios/cpu.h | build
	$(CC) $(COMMON) -O2 tests/cpu_test.c core/cpu.c -o $@
build/cpu-probe-test: tests/cpu_probe_test.c platform/x86/cpu.c core/cpu.c include/multios/cpu.h | build
	$(CC) $(COMMON) -O2 tests/cpu_probe_test.c platform/x86/cpu.c core/cpu.c -o $@
build/cpu-probe-disabled-test: tests/cpu_probe_test.c platform/x86/cpu.c core/cpu.c include/multios/cpu.h | build
	$(CC) $(COMMON) -O2 -DMULTIOS_X86_NO_CPUID=1 -DEXPECT_NO_CPUID=1 tests/cpu_probe_test.c platform/x86/cpu.c core/cpu.c -o $@
build/pc-cpu-disabled.o: platform/x86/cpu.c include/multios/cpu.h | build
	$(CC) $(KFLAGS) -DMULTIOS_X86_NO_CPUID=1 -c $< -o $@
build/cpu-disabled.elf: $(KOBJ) build/pc-cpu-disabled.o platform/pc-bios/link.ld
	$(LD) -m elf_i386 $(FS_KLDFLAGS) -T platform/pc-bios/link.ld $(filter-out build/pc-cpu.o,$(KOBJ)) build/pc-cpu-disabled.o -o $@
build/cpu-disabled.bin: build/cpu-disabled.elf
	objcopy -O binary $< $@
build/cpu-disabled.img: build/boot.bin build/cpu-disabled.bin tools/mkimage.py
	rm -f $@.tmp
	$(PYTHON) tools/mkimage.py $@.tmp --boot build/boot.bin --kernel build/cpu-disabled.bin
	mv $@.tmp $@
test-cpu: build/cpu-test build/cpu-probe-test build/cpu-probe-disabled-test
	./build/cpu-test
	./build/cpu-probe-test
	./build/cpu-probe-disabled-test
	$(PYTHON) tests/cpu_isa.py
test-cpu-matrix: all build/cpu-disabled.img
	$(PYTHON) tests/cpu_matrix.py
test: test-cpu

# Flag changes must rebuild native objects, not silently reuse older ISA choices.
$(KOBJ) $(EOBJ) build/pc-cpu-disabled.o: Makefile

.PHONY: test-two-buttons
build/two-buttons-test: tests/two_buttons_test.c core/hal.c core/osk.c include/multios/buttons.h include/multios/osk.h | build
	$(CC) $(COMMON) -UMULTIOS_STORAGE_TX -O2 tests/two_buttons_test.c core/hal.c core/osk.c -o $@
build/two-button-shell: tests/two_button_shell.c $(LIB) Makefile lib/multi.h $(wildcard include/multios/*.h) | build
	$(CC) $(COMMON) -O2 tests/two_button_shell.c $(LIB) -o $@
test-two-buttons: build/two-buttons-test build/two-button-shell all
	./build/two-buttons-test
	cp build/multios.img build/two-button-shell-test.img
	./build/two-button-shell build/two-button-shell-test.img > build/two-button-shell.log
	$(PYTHON) -c 'from pathlib import Path; s=Path("build/two-button-shell.log").read_text(); assert "Error:" not in s and "Hello Two Buttons!" in s and "reopened" in s'
test: test-two-buttons

.PHONY: test-hid test-editor
build/hid-test: tests/hid_test.c core/hid.c include/multios/hid.h include/multios/osk.h | build
	$(CC) $(COMMON) -O2 tests/hid_test.c core/hid.c -o $@
build/editor-test: tests/editor_test.c core/editor.c lib/util.c lib/unicode.h include/multios/editor.h include/multios/osk.h | build
	$(CC) $(COMMON) -O2 tests/editor_test.c core/editor.c lib/util.c -o $@
test-hid: build/hid-test
	./build/hid-test
test-editor: build/editor-test
	./build/editor-test
test: test-hid test-editor
.PHONY: test-ps2
build/ps2-test: tests/ps2_test.c drivers/pc-bios/keys.c platform/pc-bios/io.h | build
	$(CC) $(COMMON) -O2 tests/ps2_test.c -o $@
test-ps2: build/ps2-test
	./build/ps2-test
test: test-ps2

# MultiPulti 0.9 fault boundary: production build contains no fault trigger.
build/exception-stubs.o: platform/pc-bios/exceptions.S | build
	$(CC) -m32 -march=i386 -c $< -o $@
build/exceptions.o: platform/pc-bios/exceptions.c platform/pc-bios/exceptions.h platform/pc-bios/io.h | build
	$(CC) $(KFLAGS) -fno-stack-protector -c $< -o $@
build/fault-%.o: platform/pc-bios/exceptions.c platform/pc-bios/exceptions.h | build
	$(CC) $(KFLAGS) -fno-stack-protector -DMULTIOS_FAULT_TEST=$* -c $< -o $@
build/fault-%.elf: $(KOBJ) build/fault-%.o build/fault-stack-test.o platform/pc-bios/link.ld
	$(LD) -m elf_i386 $(FS_KLDFLAGS) -T platform/pc-bios/link.ld $(filter-out build/exceptions.o,$(KOBJ)) build/fault-$*.o build/fault-stack-test.o -o $@
build/fault-%.bin: build/fault-%.elf
	objcopy -O binary $< $@
build/fault-%.img: build/boot.bin build/fault-%.bin tools/mkimage.py
	rm -f $@.tmp
	$(PYTHON) tools/mkimage.py $@.tmp --boot build/boot.bin --kernel build/fault-$*.bin
	mv $@.tmp $@
.PHONY: test-kernel-faults
test-kernel-faults: all $(addprefix build/fault-,$(addsuffix .img,1 2 3 4 5 6))
	$(PYTHON) tests/kernel_faults.py

build/fault-stack-test.o: tests/kernel_stack_fault.c | build
	$(CC) $(KFLAGS) -c $< -o $@
.SECONDARY: $(addprefix build/fault-,$(addsuffix .elf,1 2 3 4 5 6)) $(addprefix build/fault-,$(addsuffix .bin,1 2 3 4 5 6))

# 0.10 fail-closed filesystem / provider error regression.
.PHONY: test-io-failure test-exfat-bounds test-hardening
build/io-failure-test: tests/io_failure_test.c core/hal.c core/osk.c $(wildcard include/multios/*.h) lib/multi.h | build
	$(CC) $(COMMON) -UMULTIOS_STORAGE_TX -O2 tests/io_failure_test.c core/hal.c core/osk.c -o $@
build/exfat-bounds-test: tests/exfat_bounds_test.c $(FS_LIB) lib/util.c lib/multi.h | build
	$(CC) $(COMMON) -UMULTIOS_STORAGE_TX -O2 tests/exfat_bounds_test.c $(FS_UNIT_LIB) lib/util.c -o $@
test-io-failure: build/io-failure-test
	./build/io-failure-test
test-exfat-bounds: build/exfat-bounds-test
	./build/exfat-bounds-test
test: test-io-failure test-exfat-bounds
test-hardening: all test-io-failure test-exfat-bounds
	$(PYTHON) tests/adverse_volumes.py
	$(PYTHON) tests/write_audit_limits.py
	$(PYTHON) tests/host_image_safety.py
	$(PYTHON) tests/exfat_mutation_fuzz.py
	$(PYTHON) tests/flush_faults.py

$(KOBJ) $(EOBJ): lib/fs_backend.h

.PHONY: test-filesystems test-filesystems-sanitize test-filesystems-boot
build/multios-sanitize-host: $(NTFS_HOST_SAN_OBJ) platform/posix/host.c $(LIB) Makefile lib/multi.h lib/fs_backend.h | build
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie platform/posix/host.c $(LIB) $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@
test-filesystems: all
	$(PYTHON) tests/filesystems.py
test-filesystems-sanitize: all build/multios-sanitize-host
	MULTIOS_HOST=$(CURDIR)/build/multios-sanitize-host $(PYTHON) tests/filesystems.py
	MULTIOS_HOST=$(CURDIR)/build/multios-sanitize-host $(PYTHON) tests/filesystems_fuzz.py
	MULTIOS_HOST=$(CURDIR)/build/multios-sanitize-host $(PYTHON) tests/fat32_large.py
test-filesystems-boot: all uefi
	$(PYTHON) tests/filesystems_boot.py

.PHONY: test-filesystems-faults
build/multios-fault-host: platform/posix/host.c $(LIB) Makefile lib/multi.h lib/fs_backend.h | build
	$(CC) $(COMMON) -O1 -DMULTIOS_FAULT_INJECTION platform/posix/host.c $(LIB) $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@
test-filesystems-faults: build/multios-fault-host
	$(PYTHON) tests/filesystems_faults.py

# Backend ABI changes also invalidate non-MMD host tools.
build/multios-host build/two-button-shell build/exfat-bounds-test: lib/fs_backend.h

# Switching a filesystem profile invalidates static dispatch and host binaries.
build/fs-profile.stamp: FORCE | build
	@printf '%s\n' '$(FS_PROFILE)|$(FS_DRIVERS)|$(FS_REGISTRY)|$(FS_TX_CFLAGS)' > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@
$(KOBJ) $(EOBJ) build/multios-host build/multios-sanitize-host build/multios-fault-host build/two-button-shell build/exfat-bounds-test: build/fs-profile.stamp $(FS_REGISTRY) include/multios/fs_driver.h

build/fs-registry-test: tests/fs_registry_test.c $(FS_LIB) lib/util.c lib/fs_backend.h include/multios/fs_driver.h | build
	$(CC) $(COMMON) -UMULTIOS_STORAGE_TX -O2 tests/fs_registry_test.c $(FS_UNIT_LIB) lib/util.c -o $@
test-fs-registry: build/fs-registry-test
	./build/fs-registry-test
.PHONY: test-fs-registry

test-fat32-model: all
	$(PYTHON) tests/fat32_model.py
test-studio:
	$(PYTHON) tests/studio_test.py
.PHONY: test-fat32-model test-studio

# First NTFS write-foundation stage: codecs only, not permission to modify NTFS.
build/ntfs-meta-test: tests/ntfs_meta_test.c lib/ntfs_meta.c lib/ntfs_meta.h lib/multi.h | build
	$(CC) $(COMMON) -O2 tests/ntfs_meta_test.c lib/ntfs_meta.c -o $@
build/ntfs-meta-sanitize: tests/ntfs_meta_test.c lib/ntfs_meta.c lib/ntfs_meta.h lib/multi.h | build
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer tests/ntfs_meta_test.c lib/ntfs_meta.c -o $@
test-ntfs-meta: build/ntfs-meta-test build/ntfs-meta-sanitize
	./build/ntfs-meta-test
	./build/ntfs-meta-sanitize
.PHONY: test-ntfs-meta

build/block64-test: tests/block64_test.c lib/block64.c include/multios/block64.h | build
	$(CC) $(COMMON) -O2 tests/block64_test.c lib/block64.c -o $@
build/block64-sanitize: tests/block64_test.c lib/block64.c include/multios/block64.h | build
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer tests/block64_test.c lib/block64.c -o $@
test-block64: build/block64-test build/block64-sanitize
	./build/block64-test
	./build/block64-sanitize
.PHONY: test-block64

# Header changes must invalidate metadata helpers and their users.
$(KOBJ) $(EOBJ) build/multios-host build/multios-sanitize-host build/multios-fault-host: lib/ntfs_meta.h include/multios/block64.h

# Stage 2: checked attributes / bounded extension-record DATA assembly.
$(KOBJ) $(EOBJ) build/multios-host build/multios-sanitize-host build/multios-fault-host: lib/ntfs_attr.h
build/ntfs-attr-test: tests/ntfs_attr_test.c lib/ntfs_attr.c lib/ntfs_attr.h lib/multi.h | build
	$(CC) $(COMMON) -O2 tests/ntfs_attr_test.c lib/ntfs_attr.c -o $@
build/ntfs-attr-sanitize: tests/ntfs_attr_test.c lib/ntfs_attr.c lib/ntfs_attr.h lib/multi.h | build
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/ntfs_attr_test.c lib/ntfs_attr.c -o $@
test-ntfs-attr: build/ntfs-attr-test build/ntfs-attr-sanitize
	./build/ntfs-attr-test
	./build/ntfs-attr-sanitize
build/ntfs-split-fixture: tests/ntfs_split_fixture.c | build
	$(CC) -Wall -Wextra $< -lntfs-3g -o $@
test-ntfs-attrlist: build/multios-host build/multios-sanitize-host build/ntfs-split-fixture
	$(PYTHON) tests/ntfs_attrlist_test.py
.PHONY: test-ntfs-attr test-ntfs-attrlist
build/ntfs-stream-test: tests/ntfs_stream_test.c $(FS_LIB) lib/util.c lib/ntfs_attr.h lib/fs_backend.h | build
	$(CC) $(COMMON) -UMULTIOS_STORAGE_TX -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/ntfs_stream_test.c $(FS_UNIT_LIB) lib/util.c -o $@
test-ntfs-stream: build/ntfs-stream-test
	./build/ntfs-stream-test build/ntfs2-tests/attributes.ntfs
.PHONY: test-ntfs-stream

# Standalone VFS harnesses also follow attribute/header changes.
build/fs-registry-test build/two-button-shell build/exfat-bounds-test build/ntfs-stream-test: lib/ntfs_attr.h lib/ntfs_meta.h

# Wide read ABI 2, independent sparse-volume fixtures. No wide write API.
build/file64-test: tests/file64_test.c $(FS_LIB) lib/util.c lib/multi.h lib/fs_backend.h include/multios/fs_driver.h | build
	$(CC) $(COMMON) -UMULTIOS_STORAGE_TX -g -O1 -D_FILE_OFFSET_BITS=64 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/file64_test.c $(FS_UNIT_LIB) lib/util.c -o $@
test-file64: build/file64-test
	$(PYTHON) tests/file64_fixture.py
	./build/file64-test build/fs64-tests/fat32.img fat32
	./build/file64-test build/fs64-tests/exfat.img exfat
	./build/file64-test build/fs64-tests/exfat-chain.img exfat-chain
	./build/file64-test build/fs64-tests/exfat-uninit.img exfat-uninit
	./build/file64-test build/fs64-tests/ntfs.img ntfs
.PHONY: test-file64
build/file64-epoch-test: tests/file64_epoch_test.c tests/fs64_mock.registry.h lib/fs.c lib/util.c lib/multi.h include/multios/fs_driver.h | build
	$(CC) $(COMMON) -UMULTIOS_STORAGE_TX -UMULTIOS_FS_REGISTRY -DMULTIOS_FS_REGISTRY='"../tests/fs64_mock.registry.h"' -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/file64_epoch_test.c lib/fs.c lib/util.c -o $@
test-file64-epoch: build/file64-epoch-test
	./build/file64-epoch-test
.PHONY: test-file64-epoch

build/file64-plain: tests/file64_test.c $(FS_LIB) lib/util.c lib/multi.h lib/fs_backend.h include/multios/fs_driver.h | build
	$(CC) $(COMMON) -UMULTIOS_STORAGE_TX -O2 -D_FILE_OFFSET_BITS=64 tests/file64_test.c $(FS_UNIT_LIB) lib/util.c -o $@

# Unicode codec/case-data changes must invalidate all filesystem consumers.
$(KOBJ) $(EOBJ) build/multios-host: lib/unicode.h lib/unicode_upcase.h

build/unicode-test: tests/unicode_test.c lib/util.c lib/unicode.h lib/unicode_upcase.h | build
	$(CC) $(COMMON) -O2 tests/unicode_test.c lib/util.c -o $@
build/unicode-sanitize: tests/unicode_test.c lib/util.c lib/unicode.h lib/unicode_upcase.h | build
	$(CC) $(COMMON) -O1 -g -fsanitize=address,undefined -fno-pie -no-pie tests/unicode_test.c lib/util.c -o $@
test-unicode: build/unicode-test build/unicode-sanitize
	./build/unicode-test
	./build/unicode-sanitize
.PHONY: test-unicode

build/uefi-unicode-test: tests/uefi_unicode_test.c drivers/uefi/text.c drivers/uefi/plain.c drivers/uefi/keys.c lib/util.c lib/unicode.h | build
	$(CC) $(COMMON) -I$(EFIINC) -I$(EFIINC)/x86_64 -O1 -g -fsanitize=address,undefined -fno-pie -no-pie tests/uefi_unicode_test.c lib/util.c -o $@
test-uefi-unicode: build/uefi-unicode-test
	./build/uefi-unicode-test
.PHONY: test-uefi-unicode

# Experimental standalone private undo journal; NOT enabled in filesystem writers.
build/undo-test: tests/undo_test.c lib/undo.c lib/block64.c include/multios/undo.h include/multios/block64.h lib/multi.h | build
	$(CC) $(COMMON) -O2 tests/undo_test.c lib/undo.c lib/block64.c -o $@
build/undo-sanitize: tests/undo_test.c lib/undo.c lib/block64.c include/multios/undo.h include/multios/block64.h lib/multi.h | build
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/undo_test.c lib/undo.c lib/block64.c -o $@
test-undo: build/undo-test build/undo-sanitize
	./build/undo-test
	./build/undo-sanitize
.PHONY: test-undo

# Opt-in transactional profile must invalidate consumers after adapter API changes.
$(KOBJ) $(EOBJ) build/multios-host build/multios-sanitize-host build/multios-fault-host: include/multios/storage_tx.h include/multios/undo.h

# These targets require FS_PROFILE=profiles/filesystems-journal.mk.
build/storage-tx-test: $(NTFS_HOST_OBJ) tests/storage_tx_test.c core/hal.c $(FS_LIB) lib/util.c include/multios/storage_tx.h include/multios/undo.h build/fs-profile.stamp | build
	$(CC) $(COMMON) -O2 tests/storage_tx_test.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
build/storage-tx-sanitize: $(NTFS_HOST_SAN_OBJ) tests/storage_tx_test.c core/hal.c $(FS_LIB) lib/util.c include/multios/storage_tx.h include/multios/undo.h build/fs-profile.stamp | build
	$(CC) $(COMMON) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/storage_tx_test.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@
.PHONY: test-storage-tx
test-storage-tx:
	$(MAKE) FS_PROFILE=profiles/filesystems-journal.mk build/multios-host build/storage-tx-test build/storage-tx-sanitize
	$(PYTHON) tests/storage_tx.py
TX_OUTPUT ?= build/MultiOS-TX-EXPERIMENTAL.img
TX_DATA_FS ?= exfat
.PHONY: journal-image
journal-image:
	$(MAKE) FS_PROFILE=profiles/filesystems-journal.mk all uefi
	$(PYTHON) tools/build_journal_image.py --data-fs $(TX_DATA_FS) --output $(TX_OUTPUT)

include ports/ntfs3g/build.mk

build/upstream-growth: $(NTFS_HOST_OBJ) tests/upstream_growth.c core/hal.c $(FS_LIB) lib/util.c build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/upstream_growth.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
.PHONY: test-fatfs
# Distinct target so the older journal suite does not silently switch backends.
test-fatfs:
	$(MAKE) FS_PROFILE=profiles/filesystems-fatfs.mk build/multios-host build/storage-tx-test build/storage-tx-sanitize build/upstream-growth
	$(PYTHON) tests/storage_tx.py
	$(PYTHON) tests/upstream_growth.py
	$(PYTHON) tests/upstream_upcase.py

UPSTREAM_OUTPUT ?= build/MultiOS-0.21-dev-ntfsstreams.img
UPSTREAM_DATA_FS ?= exfat
.PHONY: upstream-image
upstream-image:
	$(MAKE) FS_PROFILE=profiles/filesystems-fatfs.mk build/multios.img
	$(MAKE) FS_PROFILE=profiles/filesystems-ntfs3g.mk uefi build/multios-host
	$(PYTHON) tools/build_upstream_image.py --data-fs $(UPSTREAM_DATA_FS) --output $(UPSTREAM_OUTPUT)

# Full upstream read-path probe; runtime refuses every write.
build/fs-read-probe: tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
build/fs-mutation-probe: tests/fs_mutation_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/fs_mutation_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
build/fs-capacity-probe: tests/fs_capacity_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/fs_capacity_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
build/fs-read-probe-sanitize: tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@
build/fs-mutation-probe-sanitize: tests/fs_mutation_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/fs_mutation_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@

# Staged copy public API and full-library sanitizer probes.
build/fs-copy-probe: tests/fs_copy_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) include/multios/copy.h build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/fs_copy_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
build/fs-copy-probe-sanitize: tests/fs_copy_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) include/multios/copy.h build/fs-profile.stamp
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/fs_copy_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@
$(KOBJ) $(EOBJ) build/multios-host: include/multios/copy.h
build/copy64-model: tests/copy64_model.c lib/fs_copy.c lib/util.c include/multios/copy.h
	$(CC) -std=c11 -O2 -Ilib -Iinclude -DMULTIOS_STORAGE_TX tests/copy64_model.c lib/fs_copy.c lib/util.c -o $@
.PHONY: test-copy64-model
test-copy64-model: build/copy64-model
	./build/copy64-model

# Changing compiler ISA policy must invalidate every firmware object, including NTFS.
$(EOBJ): build/uefi-profile.stamp

ifeq ($(USE_SYSTEM_GNUEFI),0)
build/gnuefi-toolchain.stamp: FORCE | build
	@printf '%s\n' '$(CC)|x86-64|generic|no-cf-protection' > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@
build/gnuefi.stamp: build/gnuefi-toolchain.stamp third_party/gnu-efi/gnu-efi-3.0.11.tar.gz tools/build_uefi_runtime.py | build
	$(PYTHON) tools/build_uefi_runtime.py --cc "$(CC)"
$(EOBJ) build/uefi-unicode-test: build/gnuefi.stamp
$(EFICRT) $(EFILIB)/libefi.a $(EFIGNUEFILIB)/libgnuefi.a build/gnu-efi-sdk/lib/gnuefi/elf_x86_64_efi.lds: build/gnuefi.stamp
	@test -f $@
build/multios-uefi.so: $(EFICRT) $(EFILIB)/libefi.a $(EFIGNUEFILIB)/libgnuefi.a
endif
.PHONY: test-uefi-isa
test-uefi-isa: uefi
	$(PYTHON) tests/uefi_isa.py

build/fs-random-probe: tests/fs_random_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) include/multios/copy.h build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/fs_random_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
build/fs-random-probe-sanitize: tests/fs_random_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) include/multios/copy.h build/fs-profile.stamp
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/fs_random_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@

# NTFS named streams/hardlinks through modular VFS.
build/ntfs-stream-probe: tests/ntfs_stream_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) include/multios/copy.h build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/ntfs_stream_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
build/ntfs-stream-probe-sanitize: tests/ntfs_stream_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) include/multios/copy.h build/fs-profile.stamp
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/ntfs_stream_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@

# Extended BIOS NTFS RW profile. Historical compact `all` remains separate.
.PHONY: bios-rw bios-rw-image
bios-rw:
	$(PYTHON) tools/build_bios_ntfs.py
bios-rw-image: bios-rw
	@test -n "$(BIOS_RW_SOURCE)" -a -n "$(BIOS_RW_OUTPUT)" || (echo 'Set BIOS_RW_SOURCE and NEW BIOS_RW_OUTPUT'; exit 1)
	$(MAKE) FS_PROFILE=profiles/filesystems-ntfs3g.mk uefi
	$(PYTHON) tools/build_bios_rw_image.py "$(BIOS_RW_SOURCE)" "$(BIOS_RW_OUTPUT)" --efi build/BOOTX64.EFI

# Write-integrity guards: persistence model + ASan/UBSan + port-level ATA model.
build/write-guard-sanitize: tests/write_guard_test.c tests/undo_test.c lib/undo.c lib/block64.c include/multios/undo.h | build
	$(CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -I. -Ilib -Iinclude tests/write_guard_test.c lib/undo.c lib/block64.c -o $@
build/ata-write-guard-test: tests/ata_write_guard_test.c drivers/pc-bios/ata.c platform/pc-bios/io.h | build
	$(CC) -std=c11 -O2 -ffunction-sections -fdata-sections -I. -Ilib -Iinclude tests/ata_write_guard_test.c -Wl,--gc-sections -o $@
.PHONY: test-write-guard
test-write-guard: build/write-guard-sanitize build/ata-write-guard-test build/undo-test build/undo-sanitize
	./build/write-guard-sanitize
	./build/ata-write-guard-test
	./build/undo-test
	./build/undo-sanitize

build/repeated-crash-test: tests/repeated_crash_test.c tests/undo_test.c lib/undo.c lib/block64.c include/multios/undo.h | build
	$(CC) -std=c11 -O2 -I. -Ilib -Iinclude tests/repeated_crash_test.c lib/undo.c lib/block64.c -o $@
build/repeated-crash-sanitize: tests/repeated_crash_test.c tests/undo_test.c lib/undo.c lib/block64.c include/multios/undo.h | build
	$(CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -I. -Ilib -Iinclude tests/repeated_crash_test.c lib/undo.c lib/block64.c -o $@
.PHONY: test-repeated-crash
test-repeated-crash: build/repeated-crash-test build/repeated-crash-sanitize
	./build/repeated-crash-test
	./build/repeated-crash-sanitize

# Native USB 1.1/2.0 boot HID tests. No physical device certification.
build/usb-enum-test: tests/usb_enum_test.c drivers/pc-bios/usb.c platform/pc-bios/usb-host.h | build
	$(CC) -std=c11 -O1 -g -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-pie -no-pie -I. -Ilib -Iinclude tests/usb_enum_test.c drivers/pc-bios/usb.c -Wl,--gc-sections -o $@
.PHONY: test-usb-enum
test-usb-enum: build/usb-enum-test
	./build/usb-enum-test

build/usb-bot-test: tests/usb_bot_test.c lib/usb_bot.c include/multios/usb_bot.h | build
	$(CC) -std=c11 -Ilib -Iinclude -I. -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer tests/usb_bot_test.c lib/usb_bot.c -o $@
.PHONY: test-usb-bot
test-usb-bot: build/usb-bot-test
	./build/usb-bot-test

# RO-view cache sits above transaction/read-back IO, never below it.
build/read-cache-test: tests/read_cache_test.c include/multios/read_cache.h | build
	$(CC) -std=c11 -Ilib -Iinclude -I. -g -O1 -fsanitize=address,undefined -fno-pie -no-pie tests/read_cache_test.c -o $@
.PHONY: test-read-cache
test-read-cache: build/read-cache-test
	./build/read-cache-test

.PHONY: test-usb-bulk-hosts
test-usb-bulk-hosts: | build
	@for host in 1 2 3; do $(CC) -std=c11 -DHOST=$$host -I. -Ilib -Iinclude -g -O1 -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-pie -no-pie tests/usb_bulk_host_test.c -Wl,--gc-sections -o build/usb-bulk-host-$$host && ./build/usb-bulk-host-$$host || exit 1; done

# Changes to the header-only NTFS RO-view cache must invalidate consumers.
$(KOBJ) $(EOBJ) build/multios-host build/multios-sanitize-host build/storage-tx-test build/storage-tx-sanitize: include/multios/read_cache.h

# Test actual FatFs bridge callbacks; fake HAL, no third-party engine stubs in production.
build/fatfs-bridge-test: tests/fatfs_bridge_test.c lib/fatfs_adapter.c | build
	$(CC) -std=c11 -I. -Iinclude -Ilib -Ivendor/fatfs/port -O1 -g -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-pie -no-pie $< -Wl,--gc-sections -o $@
.PHONY: test-fatfs-bridge
test-fatfs-bridge: build/fatfs-bridge-test
	./build/fatfs-bridge-test

build/fs-unlock-probe: tests/fs_unlock_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/fs_unlock_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@

build/fs-full-probe: tests/fs_full_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/fs_full_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
build/fs-full-probe-sanitize: tests/fs_full_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/fs_full_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@


build/unlock-contract-test: tests/unlock_contract_test.c lib/fs.c tests/fs64_mock.registry.h | build
	$(CC) -std=c11 -Ilib -Iinclude -I. -DMULTIOS_STORAGE_TX -DMULTIOS_FS_REGISTRY='"../tests/fs64_mock.registry.h"' -O1 -g -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-pie -no-pie tests/unlock_contract_test.c lib/fs.c -Wl,--gc-sections -o $@
.PHONY: test-unlock-contract
test-unlock-contract: build/unlock-contract-test
	./build/unlock-contract-test

build/fat32-active-probe: tests/fat32_active_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/fat32_active_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
build/fat32-active-probe-sanitize: tests/fat32_active_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/fat32_active_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@

# Write semantics use the actual production adapters, not a mock filesystem.
build/fs-writeops-probe: tests/fs_writeops_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/fs_writeops_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
build/fs-writeops-probe-sanitize: tests/fs_writeops_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/fs_writeops_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@
build/writeops-tx-test: tests/writeops_tx_test.c tests/writeops_actions.h tests/storage_tx_test.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/writeops_tx_test.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
build/writeops-tx-sanitize: tests/writeops_tx_test.c tests/writeops_actions.h tests/storage_tx_test.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/writeops_tx_test.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@

build/ntfs-case-probe: tests/ntfs_case_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/ntfs_case_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
build/ntfs-case-probe-sanitize: tests/ntfs_case_probe.c tests/fs_read_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/ntfs_case_probe.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@

build/ntfs-case-tx-test: tests/ntfs_case_tx_test.c tests/ntfs_case_actions.h tests/storage_tx_test.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -O2 tests/ntfs_case_tx_test.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_OBJ) -Wl,--gc-sections -o $@
build/ntfs-case-tx-sanitize: tests/ntfs_case_tx_test.c tests/ntfs_case_actions.h tests/storage_tx_test.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) build/fs-profile.stamp
	$(CC) $(COMMON) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie tests/ntfs_case_tx_test.c core/hal.c $(FS_LIB) lib/util.c $(NTFS_HOST_SAN_OBJ) -Wl,--gc-sections -o $@

.PHONY: test-pc-console
build/pc-console-test: tests/pc_console_test.c drivers/pc-bios/text.c lib/util.c | build
	$(CC) $(COMMON) -O1 -g -fsanitize=address,undefined -fno-pie -no-pie tests/pc_console_test.c lib/util.c -o $@
test-pc-console: build/pc-console-test
	./build/pc-console-test
test: test-pc-console

.PHONY: test-input-loss
build/input-loss-test: tests/input_loss_test.c core/hid.c core/editor.c core/osk.c lib/util.c include/multios/osk.h | build
	$(CC) $(COMMON) -O1 -g -fsanitize=address,undefined -fno-pie -no-pie tests/input_loss_test.c core/hid.c core/editor.c core/osk.c lib/util.c -o $@
test-input-loss: build/input-loss-test
	./build/input-loss-test
test: test-input-loss

.PHONY: test-input-loss-shell
build/input-loss-shell: tests/input_loss_shell.c $(LIB) Makefile lib/multi.h $(wildcard include/multios/*.h) | build
	$(CC) $(COMMON) -O2 tests/input_loss_shell.c $(LIB) -o $@
test-input-loss-shell: build/input-loss-shell build/multios.img
	./build/input-loss-shell build/multios.img
test: test-input-loss-shell

.PHONY: test-input-sanitize
build/hid-sanitize: tests/hid_test.c core/hid.c include/multios/hid.h include/multios/osk.h | build
	$(CC) $(COMMON) -O1 -g -fsanitize=address,undefined -fno-pie -no-pie tests/hid_test.c core/hid.c -o $@
build/editor-sanitize: tests/editor_test.c core/editor.c lib/util.c lib/unicode.h include/multios/editor.h include/multios/osk.h | build
	$(CC) $(COMMON) -O1 -g -fsanitize=address,undefined -fno-pie -no-pie tests/editor_test.c core/editor.c lib/util.c -o $@
test-input-sanitize: build/hid-sanitize build/editor-sanitize
	./build/hid-sanitize
	./build/editor-sanitize
test: test-input-sanitize

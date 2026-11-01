#!/bin/sh
# vm2 studios | t.me/v2vproj — never opens a physical disk.
set -eu
cd "$(dirname "$0")"
mode="${1:-uefi}"
case "$mode" in bios|uefi);; *) echo 'Usage: ./run-unified.sh [bios|uefi]' >&2; exit 2;; esac
if [ -n "${MULTIOS_IMAGE:-}" ]; then
 image="$MULTIOS_IMAGE" # explicit direct image: caller must stop other writers
else
 image=build/unified-work.img
 [ -f "$image" ] || cp build/MultiOS-MultiPulti.img "$image"
fi
[ -f "$image" ] && [ ! -L "$image" ] || { echo 'Expected regular image file' >&2; exit 1; }
if [ "$mode" = bios ]; then
 "${QEMU:-qemu-system-x86_64}" -machine pc -m 32M -drive "file=$image,format=raw,if=ide,index=0" -serial stdio -monitor none -device isa-debug-exit,iobase=0xf4,iosize=0x04 || test "$?" -eq 33
else
 code="${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}"
 vars="${OVMF_VARS:-/usr/share/OVMF/OVMF_VARS.fd}"
 [ -f "$code" ] && [ -f "$vars" ] || { echo 'Set OVMF_CODE/OVMF_VARS' >&2; exit 1; }
 [ -f build/unified-vars.fd ] || cp "$vars" build/unified-vars.fd
 exec "${QEMU_UEFI:-qemu-system-x86_64}" -machine q35 -m 256M \
   -drive "if=pflash,format=raw,readonly=on,file=$code" \
   -drive if=pflash,format=raw,file=build/unified-vars.fd \
   -device qemu-xhci,id=xhci -drive "if=none,format=raw,id=stick,file=$image" \
   -device usb-storage,drive=stick,bus=xhci.0,bootindex=1 -device usb-kbd,bus=xhci.0 \
   -serial stdio -monitor none
fi

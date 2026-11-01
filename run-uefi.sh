#!/bin/sh
# vm2 studios | t.me/v2vproj - MultiOS / MultiPulti
# Safe launcher: persistent COPY of the image, never a physical device.
set -eu
cd "$(dirname "$0")"
QEMU="${QEMU_UEFI:-qemu-system-x86_64}"
CODE="${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}"
VARS="${OVMF_VARS:-/usr/share/OVMF/OVMF_VARS.fd}"
[ -f "$CODE" ] && [ -f "$VARS" ] || { echo 'Set OVMF_CODE and OVMF_VARS to matching firmware files.' >&2; exit 1; }
[ -f build/usb-work.img ] || cp build/multios-usb-uefi.img build/usb-work.img
[ -f build/usb-vars.fd ] || cp "$VARS" build/usb-vars.fd
exec "$QEMU" -machine q35 -m 256M \
 -drive "if=pflash,format=raw,readonly=on,file=$CODE" \
 -drive if=pflash,format=raw,file=build/usb-vars.fd \
 -device qemu-xhci,id=xhci \
 -drive if=none,format=raw,id=stick,file=build/usb-work.img \
 -device usb-storage,drive=stick,bus=xhci.0,bootindex=1 \
 -device usb-kbd,bus=xhci.0 -serial stdio -monitor none

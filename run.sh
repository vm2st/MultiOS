#!/bin/sh
# vm2 studios | t.me/v2vproj - MultiOS / MultiPulti
# Safe convenience launcher: use a persistent COPY, never the release image.
set -eu
cd "$(dirname "$0")"
QEMU="${QEMU:-qemu-system-x86_64}"
command -v "$QEMU" >/dev/null 2>&1 || { echo 'Install qemu-system-x86 or set QEMU=/path/to/qemu-system-x86_64' >&2; exit 1; }
[ -f build/work.img ] || cp build/multios.img build/work.img
if [ "${1:-}" = '--headless' ]; then
  set -- -display none -monitor none
else
  set --
fi
set +e
"$QEMU" -machine pc -cpu qemu64 -m 32M -drive file=build/work.img,format=raw,if=ide,index=0 -serial stdio -device isa-debug-exit,iobase=0xf4,iosize=0x04 "$@"
rc=$?
[ "$rc" -eq 33 ] && exit 0
exit "$rc"

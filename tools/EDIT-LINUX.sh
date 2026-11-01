#!/bin/sh
# vm2 studios | t.me/v2vproj
cd "$(dirname "$0")" || exit 1
exec python3 ./customize.py --gui

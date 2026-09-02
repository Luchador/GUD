#!/bin/bash
set -e
find build/u -name '*.o' -delete 2>/dev/null
rm -f build/u/GUD.z64 build/u/GUD.elf build/u/GUD.bin build/u/GUD.map
make VERSION=US "$@"

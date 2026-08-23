#!/bin/bash
set -e
find build/u -name '*.o' -delete 2>/dev/null
rm -f build/u/ge007.u.z64 build/u/ge007.u.elf build/u/ge007.u.bin build/u/ge007.u.map
make VERSION=US "$@"

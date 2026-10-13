#!/bin/bash
set -euo pipefail
exec docker run --rm -i --network=host --user 0:0 --memory=64m --memory-swap=64m --cpuset-cpus=20 --privileged --pid=host --cgroupns=host -v /:/host uwvm3-implementation-checkpoint:20261002-kvm-r1 chroot /host /usr/bin/python3 -B /home/macromodel/Documents/uwvm-gc-perf-20261011-r73/control/entry.py "$@"

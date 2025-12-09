#!/usr/bin/env bash

set -e

clang++ -o RWSpinLock RWSpinLock.cc -std=c++26 -Ofast -s -flto -march=native -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables -I ../../src -I ../../third-parties/fast_io/include -I ../../third-parties/bizwen/include -I ../../third-parties/boost_unordered/include -fuse-ld=lld


/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#if defined(__linux__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && \
    (defined(__aarch64__) || defined(__i386__) || defined(__powerpc__) || \
     (defined(__mips__) && __SIZEOF_POINTER__ == 8) || (defined(__riscv) && __riscv_xlen == 64) || \
     defined(__loongarch64) || (defined(__sparc__) && defined(__arch64__)) || defined(__s390x__) || \
     defined(__arm__))
# define UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE 1
#else
# define UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE 0
#endif

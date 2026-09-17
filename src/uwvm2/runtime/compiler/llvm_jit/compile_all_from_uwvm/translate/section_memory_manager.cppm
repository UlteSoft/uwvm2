/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        section_memory_manager.cppm
 * @brief       Owning module partition for the LLVM JIT section memory manager.
 * @copyright   APL-2.0 License
 */

module;

// std
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
// backend configuration
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
// platform
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <llvm/Config/llvm-config.h>
# include <llvm/ExecutionEngine/SectionMemoryManager.h>
# if defined(__APPLE__) && defined(__aarch64__)
#  include "macho_headers.h"
# endif
# if defined(__linux__) && defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
#  include <sys/mman.h>
#  include <unistd.h>
#  ifndef MAP_FIXED_NOREPLACE
#   define MAP_FIXED_NOREPLACE 0x100000
#  endif
# endif
# if !defined(_WIN32) && !defined(__arm__) && !defined(__thumb__) && __has_include(<unwind.h>)
#  include <unwind.h>
#  include "dwarf_eh_frame_registration.h"
# endif
#endif

export module uwvm2.runtime.compiler.llvm_jit.compile_all_from_uwvm:section_memory_manager;

import fast_io;
import uwvm2.utils.container;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "section_memory_manager.h"

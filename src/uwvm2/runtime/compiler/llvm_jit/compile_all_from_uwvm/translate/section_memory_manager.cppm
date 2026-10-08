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
#include <uwvm2/runtime/lib/uwvm_runtime_posix_abi.h>
#include "arm_ehabi_registration.h"
#if defined(_WIN32) && !defined(__CYGWIN__)
// Fast_io Win32 unwind declarations live in its global module fragment.
// A plain module import does not make those private declarations visible.
#include <fast_io.h>
#endif
// Compiler exception macros are translation-unit local; module imports
// cannot supply them to the registered CFI observer.
#include <uwvm2/utils/macro/push_macros.h>
// backend configuration
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
// platform
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <llvm/Config/llvm-config.h>
#if defined(LLVM_VERSION_MAJOR) && LLVM_VERSION_MAJOR >= 23 && !defined(_WIN32) && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
# include <array>
# include <bit>
# include <span>
# include <vector>
# include <llvm/DebugInfo/DWARF/DWARFDataExtractor.h>
# include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>
# include <llvm/Object/ObjectFile.h>
# include <llvm/Object/ELFObjectFile.h>
# include <llvm/BinaryFormat/ELF.h>
# include <llvm/ExecutionEngine/RuntimeDyld.h>
# include <llvm/ExecutionEngine/ExecutionEngine.h>
# include <llvm/ExecutionEngine/JITEventListener.h>
#endif

#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
# include <bit>
# include <vector>
# include <llvm/DebugInfo/DWARF/DWARFDataExtractor.h>
# include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>
#endif
# if defined(_WIN64)
#  include "coff_headers.h"
# endif
# include <llvm/ExecutionEngine/SectionMemoryManager.h>
# if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35))
#  include <llvm/ExecutionEngine/ExecutionEngine.h>
#  include <llvm/ExecutionEngine/JITEventListener.h>
# endif
# if defined(_WIN64)
#  include <llvm/ExecutionEngine/ExecutionEngine.h>
#  include <llvm/ExecutionEngine/JITEventListener.h>
# endif
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
# if !defined(_WIN32) && (!(defined(__arm__) || defined(__thumb__)) || defined(__ARM_DWARF_EH__)) && __has_include(<unwind.h>)
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

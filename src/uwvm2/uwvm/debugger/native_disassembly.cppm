/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <uwvm2/utils/macro/push_macros.h>
#include <array>
#include <atomic>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#if defined(__APPLE__)
# include <TargetConditionals.h>
#endif
#if defined(UWVM_USE_LLVM_JIT)
# include <llvm-c/Disassembler.h>
# include <llvm-c/Target.h>
# include "native_disassembly_abi.h"
#endif
export module uwvm2.uwvm.debugger:native_disassembly;
import fast_io;
import :native_target_metadata;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "native_disassembly.h"

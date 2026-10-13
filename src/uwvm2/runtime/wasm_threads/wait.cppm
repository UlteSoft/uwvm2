/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <uwvm2/utils/macro/push_macros.h>
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# include <stop_token>
#endif
export module uwvm2.runtime.wasm_threads:wait;
import uwvm2.utils.thread;
import uwvm2.object.memory.linear;
import uwvm2.runtime.compiler.shared.wasm_threads;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "wait.h"

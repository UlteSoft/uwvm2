/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <atomic>
#include <bit>
#include <cstdint>
#include <type_traits>
#include <cstddef>
#include <cstring>
#include <concepts>
#include <memory>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
export module uwvm2.runtime.compiler.uwvm_int.optable:threads;
import uwvm2.utils.container;
import :define;
import :memory;
import uwvm2.runtime.compiler.shared.wasm_threads;
import uwvm2.runtime.wasm_threads;
import :storage;
import fast_io;
import uwvm2.object;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "threads.h"

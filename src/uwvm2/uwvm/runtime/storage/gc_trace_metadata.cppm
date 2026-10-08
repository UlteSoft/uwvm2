/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <utility>
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
#include <bit>
#include <cstdint>
#include <type_traits>
#endif

export module uwvm2.uwvm.runtime.storage:gc_trace_metadata;
import uwvm2.parser.wasm.standard.wasm3.type;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "gc_trace_metadata.h"

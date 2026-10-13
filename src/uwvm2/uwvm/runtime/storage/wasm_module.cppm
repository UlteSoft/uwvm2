/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

module;

// std
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1
#include <bit>
#if defined(UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY) && UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY == 1
#include <algorithm>
#endif
#endif
#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <type_traits>
#include <limits>
#include <memory>
#include <span>
#include <vector>
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
#include <algorithm>
#include <functional>
#include <span>
#endif
#include <new>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>

export module uwvm2.uwvm.runtime.storage:wasm_module;

#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1
import :compact_numeric;
#endif
import :gc_trace_metadata;
import :tag_instance_identity;
import fast_io;
import uwvm2.runtime.gc.instance_phase;
import uwvm2.utils.container;
import uwvm2.utils.allocator.fast_io_strict;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard.wasm1.type;
import uwvm2.parser.wasm.standard.wasm1p1.type;
import uwvm2.parser.wasm.standard.wasm3.type;
import uwvm2.runtime.exception.value;
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
import uwvm2.runtime.exception.roots;
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
import uwvm2.runtime.exception.native_roots;
#endif
#endif
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1 && defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
import uwvm2.runtime.exception.external_handle;
#endif
import uwvm2.validation.standard.wasm3.recursive_type_validation;
import uwvm2.validation.standard.wasm3.recursive_type_registry;
import uwvm2.parser.wasm.standard.wasm1.features;
import uwvm2.object;
import uwvm2.uwvm.wasm;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "wasm_module.h"

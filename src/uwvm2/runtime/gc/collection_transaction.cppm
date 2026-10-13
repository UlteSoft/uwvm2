/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <chrono>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#if defined(UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION) && UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION == 1
#include <utility>
#endif
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.runtime.gc.collection_transaction;
import uwvm2.utils.thread;
import uwvm2.runtime.gc.frame_roots;
import uwvm2.uwvm.runtime.storage;
#if defined(UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION) && UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION == 1
import uwvm2.runtime.exception.native_roots;
#endif

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "collection_transaction.h"

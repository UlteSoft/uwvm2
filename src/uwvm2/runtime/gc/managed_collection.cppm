/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <utility>
#include <span>
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.runtime.gc.managed_collection;
// These names are used directly by managed_collection.h. The transaction
// module's non-exported imports make definitions reachable, not names visible.
import uwvm2.utils.thread;
import uwvm2.runtime.gc.frame_roots;
import uwvm2.runtime.gc.entry_admission;
import uwvm2.runtime.gc.instance_phase;
import uwvm2.runtime.gc.allocation_policy;
import uwvm2.runtime.gc.collection_transaction;
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
import uwvm2.runtime.gc.managed_exception_cohort;
#endif
import uwvm2.uwvm.runtime.storage;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "managed_collection.h"

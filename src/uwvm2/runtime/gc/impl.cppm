/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
export module uwvm2.runtime.gc;
export import uwvm2.runtime.gc.frame_roots;
export import uwvm2.runtime.gc.entry_admission;
export import uwvm2.runtime.gc.instance_phase;
export import uwvm2.runtime.gc.allocation_policy;
export import uwvm2.runtime.gc.collection_transaction;
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
export import uwvm2.runtime.gc.managed_exception_cohort;
#endif
export import uwvm2.runtime.gc.managed_collection;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "impl.h"

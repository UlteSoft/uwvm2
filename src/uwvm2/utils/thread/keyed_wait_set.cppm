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
#include <memory>
#include <exception>
#include <span>
#include <vector>
#include <utility>
#include <uwvm2/utils/macro/push_macros.h>
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# include <condition_variable>
# include <mutex>
# include <stop_token>
#endif
export module uwvm2.utils.thread:keyed_wait_set;
import fast_io;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "keyed_wait_set.h"

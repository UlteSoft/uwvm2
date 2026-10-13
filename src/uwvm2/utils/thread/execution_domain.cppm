/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <chrono>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <uwvm2/utils/macro/push_macros.h>
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# include <mutex>
#endif
export module uwvm2.utils.thread:execution_domain;
import :execution_lifetime;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "execution_domain.h"

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <cstddef>
#include <exception>
#include <limits>
#include <memory>
export module uwvm2.utils.thread:deferred_owner;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "deferred_owner.h"

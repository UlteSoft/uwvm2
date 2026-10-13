/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
export module uwvm2.parser.wasm.standard.wasm3.type.section_details;
import fast_io;
import uwvm2.utils.container;
import uwvm2.parser.wasm.standard.wasm3.type.recursive_type;
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "section_details.h"

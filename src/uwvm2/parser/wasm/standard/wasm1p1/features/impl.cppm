/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

module;

export module uwvm2.parser.wasm.standard.wasm1p1.features;
export import :parser_limit;
export import :def;
export import :feature_def;
export import :types;
export import :data_count_section;
export import :data_section;
export import :element_section;
export import :sequence;
export import :final_check;
export import :binfmt;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "impl.h"

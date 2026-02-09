/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-05-07
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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <concepts>
#include <type_traits>
#include <utility>
#include <memory>
#include <bit>
#include <numeric>
// macro
#include <uwvm2/utils/macro/push_macros.h>
// platform
#if defined(_MSC_VER) && !defined(__clang__)
# if !defined(_KERNEL_MODE) && defined(_M_AMD64) && !defined(_M_ARM64EC)
#  include <emmintrin.h>  // MSVC x86_64-SSE2
# endif
# if !defined(_KERNEL_MODE) && (defined(_M_ARM64) || defined(_M_ARM64EC))
#  include <arm_neon.h>  // MSVC aarch64-NEON
# endif
#endif

export module uwvm2.parser.wasm.standard.wasm1.features:code_section;

import fast_io;
import uwvm2.utils.container;
import uwvm2.utils.debug;
import uwvm2.utils.intrinsics;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.utils;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard.wasm1.type;
import uwvm2.parser.wasm.standard.wasm1.section;
import uwvm2.parser.wasm.standard.wasm1.opcode;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import :def;
import :feature_def;
import :parser_limit;
import :types;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "code_section.h"

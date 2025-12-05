/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-01
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

// #pragma once

// macro
#include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
/// @todo add wasip2, wasip3 ...

/// @brief This option automatically excludes unsupported systems.
/// @note  UWVM_DISABLE_LOCAL_IMPORTED_WASIP1 controls imports within UWVM rather than this implementation.
#pragma push_macro("UWVM_IMPORT_WASI")
#undef UWVM_IMPORT_WASI
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)
#  define UWVM_IMPORT_WASI
# endif
#endif
/// @todo add wasip2, wasip3 ...

// macro
/// @todo add wasip2, wasip3 ...
#include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
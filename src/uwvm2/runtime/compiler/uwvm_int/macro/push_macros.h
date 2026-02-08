/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author MacroModel
 * @version 2.0.0
 * @date 2025-03-23
 * @copyright APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

/// @brief The following are the macros used by uwvm.
/// @details Use `push_macro` to avoid side effects on existing macros. Please use `pop_macro` in conjunction.

// #pragma once

// macro checkers
#pragma push_macro("UWVM_INTERPRETER_OPFUNC_TYPE_MACRO")
#undef UWVM_INTERPRETER_OPFUNC_TYPE_MACRO
#if defined(_WIN32) && (defined(__x86_64__) || defined(_M_AMD64)) && (defined(__GNUC__) || defined(__clang__))
# define UWVM_INTERPRETER_OPFUNC_TYPE_MACRO [[__gnu__::__sysv_abi__]]
#else
# define UWVM_INTERPRETER_OPFUNC_TYPE_MACRO
#endif

#pragma push_macro("UWVM_INTERPRETER_OPFUNC_MACRO")
#undef UWVM_INTERPRETER_OPFUNC_MACRO
#define UWVM_INTERPRETER_OPFUNC_MACRO UWVM_INTERPRETER_OPFUNC_TYPE_MACRO UWVM_GNU_HOT

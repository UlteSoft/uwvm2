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
#define UWVM_INTERPRETER_OPFUNC_TYPE_MACRO 

#pragma push_macro("UWVM_INTERPRETER_OPFUNC_MACRO")
#undef UWVM_INTERPRETER_OPFUNC_MACRO
#define UWVM_INTERPRETER_OPFUNC_MACRO UWVM_INTERPRETER_OPFUNC_TYPE_MACRO UWVM_GNU_HOT

// =========================================================
// Wasm strict floating-point control (override -ffast-math)
// =========================================================

// Notes:
// - UWVM (u2 core) may be built with `-ffast-math`, but Wasm interpreter float semantics must remain strict.
// - Use push/pop pragmas so the effects do not leak outside a single function definition block.
// - Clang must be checked first: it may define both `__GNUC__` and `_MSC_VER`.

#pragma push_macro("UWVM_UWVM_INT_PRAGMA")
#undef UWVM_UWVM_INT_PRAGMA
#if defined(__clang__) || defined(__GNUC__)
# define UWVM_UWVM_INT_PRAGMA(x) _Pragma(#x)
#elif defined(_MSC_VER)
# define UWVM_UWVM_INT_PRAGMA(x) __pragma(x)
#else
# define UWVM_UWVM_INT_PRAGMA(x)
#endif

#pragma push_macro("UWVM_UWVM_INT_STRICT_FP_BEGIN")
#undef UWVM_UWVM_INT_STRICT_FP_BEGIN

#pragma push_macro("UWVM_UWVM_INT_STRICT_FP_END")
#undef UWVM_UWVM_INT_STRICT_FP_END

#if defined(__clang__)
// Clang: `float_control(push/pop)` provides state save/restore; `exceptions(maytrap)` prevents unsafe -ffast-math assumptions (NaN/signed-zero).
# define UWVM_UWVM_INT_STRICT_FP_BEGIN                                                                                             \
    UWVM_UWVM_INT_PRAGMA(float_control(push))                                                                                      \
    UWVM_UWVM_INT_PRAGMA(float_control(precise, on))                                                                               \
    UWVM_UWVM_INT_PRAGMA(clang fp contract(off) reassociate(off) reciprocal(off) exceptions(maytrap))
# define UWVM_UWVM_INT_STRICT_FP_END UWVM_UWVM_INT_PRAGMA(float_control(pop))
#elif defined(__GNUC__) && !defined(__clang__)
// GCC: disable fast-math suboptions that break Wasm float strictness.
# define UWVM_UWVM_INT_STRICT_FP_BEGIN                \
    UWVM_UWVM_INT_PRAGMA(GCC push_options)            \
    UWVM_UWVM_INT_PRAGMA(GCC optimize("no-associative-math")) \
    UWVM_UWVM_INT_PRAGMA(GCC optimize("no-reciprocal-math"))  \
    UWVM_UWVM_INT_PRAGMA(GCC optimize("signed-zeros"))        \
    UWVM_UWVM_INT_PRAGMA(GCC optimize("no-finite-math-only"))
# define UWVM_UWVM_INT_STRICT_FP_END UWVM_UWVM_INT_PRAGMA(GCC pop_options)
#elif defined(_MSC_VER) && !defined(__clang__)
// MSVC: precise mode + disable FP contraction.
# define UWVM_UWVM_INT_STRICT_FP_BEGIN                 \
    UWVM_UWVM_INT_PRAGMA(float_control(push))          \
    UWVM_UWVM_INT_PRAGMA(float_control(precise, on))   \
    UWVM_UWVM_INT_PRAGMA(fp_contract(off))
# define UWVM_UWVM_INT_STRICT_FP_END UWVM_UWVM_INT_PRAGMA(float_control(pop))
#else
# define UWVM_UWVM_INT_STRICT_FP_BEGIN
# define UWVM_UWVM_INT_STRICT_FP_END
#endif

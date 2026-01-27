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

/*
 * ============================================================================
 * UWVM_INT_STRICT_FP_BEGIN / END
 * ============================================================================
 *
 * Purpose
 * -------
 * uwvm2 is generally built under aggressive floating-point optimization
 * flags (e.g. -ffast-math) for performance.  However, WebAssembly defines
 * *strict, instruction-level IEEE-754 floating-point semantics*:
 *
 *   - No reassociation of FP expressions
 *   - No implicit contraction (e.g. mul+add -> FMA)
 *   - NaN propagation and signed-zero must be observable
 *   - No assumptions of finiteness (NaN / Inf must be handled correctly)
 *   - Conversions must follow precise, per-instruction semantics
 *
 * In particular, integer conversion instructions (e.g. trunc_f32_to_i32)
 * are required to trap *semantically* when the input is NaN or out of range.
 * These traps are implemented explicitly in the interpreter/JIT and must
 * not be optimized away or altered by host compiler assumptions.
 *
 * This macro pair locally *overrides fast-math assumptions* and restores
 * strict IEEE behavior for the enclosed code, without affecting the rest
 * of the translation unit.
 *
 * IMPORTANT:
 * ----------
 * - Wasm traps are *language-level semantic traps*, NOT hardware FP exceptions.
 * - We therefore explicitly DISABLE compiler assumptions, rather than relying
 *   on FP exception trapping.
 * - Correctness must not depend on inlining or undefined behavior.
 *
 * ============================================================================
 * Compiler-specific rationale
 * ============================================================================
 *
 * --- Clang ---
 *
 *  float_control(push/pop)
 *      Saves and restores the FP environment so the strict mode does not
 *      leak outside the intended scope.
 *
 *  float_control(precise, on)
 *      Enforces strict evaluation order and per-operation rounding.
 *      Prevents expression reordering and value speculation.
 *
 *  clang fp contract(off)
 *      Disables implicit FMA generation.
 *      Required because Wasm distinguishes mul+add from fma.
 *
 *  clang fp reassociate(off)
 *      Prevents algebraic reassociation such as (a+b)+c -> a+(b+c),
 *      which would change rounding, NaN propagation, and signed-zero behavior.
 *
 *  clang fp reciprocal(off)
 *      Prevents transforming division into multiplication by a reciprocal,
 *      which would alter rounding and exceptional behavior.
 *
 *  clang fp exceptions(ignore)
 *      Explicitly tells the compiler that floating-point operations should
 *      NOT be treated as potentially trapping via hardware FP exceptions.
 *
 *      Rationale:
 *        - Wasm traps are implemented explicitly in uwvm2, not via SIGFPE.
 *        - Allowing the compiler to assume "may trap" would unnecessarily
 *          restrict optimization and could introduce unintended interactions
 *          with hardware FP exception modes.
 *        - All required trap points are expressed as explicit control flow
 *          (e.g. trap_invalid_conversion_to_integer()).
 *
 * --- GCC (non-Clang) ---
 *
 *  GCC push_options / pop_options
 *      Save and restore optimization state.
 *
 *  no-associative-math
 *      Disables reassociation of FP expressions.
 *
 *  no-reciprocal-math
 *      Disables reciprocal-based division optimizations.
 *
 *  signed-zeros
 *      Preserves the distinction between +0 and -0.
 *
 *  no-finite-math-only
 *      Prevents assuming all FP values are finite (allows NaN/Inf).
 *
 *  Note:
 *      GCC does not provide a single "precise FP" switch equivalent to
 *      Clang's float_control(precise), so we explicitly disable the
 *      fast-math suboptions that violate Wasm semantics.
 *
 * --- MSVC (non-Clang frontend) ---
 *
 *  float_control(push/pop)
 *      Saves/restores FP state.
 *
 *  float_control(precise, on)
 *      Enforces precise IEEE semantics and evaluation order.
 *
 *  fp_contract(off)
 *      Disables FMA contraction.
 *
 * ============================================================================
 * Summary
 * ============================================================================
 *
 * These macros create a *strict IEEE-754 island* inside an otherwise
 * -ffast-math build, ensuring that:
 *
 *   - Wasm floating-point semantics are preserved exactly
 *   - Integer conversion traps are correct and explicit
 *   - No undefined behavior is relied upon
 *   - Performance-critical code outside this region remains unaffected
 *
 * This design allows uwvm2 to combine high performance with strict
 * WebAssembly conformance.
 */

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
// Clang:
// Use float_control(push/pop) to save and restore the floating-point environment.
// float_control(precise, on) disables fast-math assumptions.
# define UWVM_UWVM_INT_STRICT_FP_BEGIN                                                                                                                         \
     UWVM_UWVM_INT_PRAGMA(float_control(push))                                                                                                                 \
     UWVM_UWVM_INT_PRAGMA(float_control(precise, on))                                                                                                          \
     UWVM_UWVM_INT_PRAGMA(clang fp contract(off) reassociate(off) reciprocal(off) exceptions(ignore))
# define UWVM_UWVM_INT_STRICT_FP_END UWVM_UWVM_INT_PRAGMA(float_control(pop))

#elif defined(__GNUC__) && !defined(__clang__)
// GCC:
// Disable fast-math suboptions that would violate WebAssembly strict
// floating-point semantics (reassociation, reciprocal transforms,
// finite-only assumptions, etc.).
# define UWVM_UWVM_INT_STRICT_FP_BEGIN                                                                                                                         \
     UWVM_UWVM_INT_PRAGMA(GCC push_options)                                                                                                                    \
     UWVM_UWVM_INT_PRAGMA(GCC optimize("no-associative-math"))                                                                                                 \
     UWVM_UWVM_INT_PRAGMA(GCC optimize("no-reciprocal-math"))                                                                                                  \
     UWVM_UWVM_INT_PRAGMA(GCC optimize("signed-zeros"))                                                                                                        \
     UWVM_UWVM_INT_PRAGMA(GCC optimize("no-finite-math-only"))                                                                                                 \
     UWVM_UWVM_INT_PRAGMA(GCC optimize("fp-contract=off"))
# define UWVM_UWVM_INT_STRICT_FP_END UWVM_UWVM_INT_PRAGMA(GCC pop_options)

#elif defined(_MSC_VER) && !defined(__clang__)
# define UWVM_UWVM_INT_STRICT_FP_BEGIN                                                                                                                         \
     UWVM_UWVM_INT_PRAGMA(float_control(push))                                                                                                                 \
     UWVM_UWVM_INT_PRAGMA(float_control(precise, on))                                                                                                          \
     UWVM_UWVM_INT_PRAGMA(fp_contract(off))
# define UWVM_UWVM_INT_STRICT_FP_END UWVM_UWVM_INT_PRAGMA(float_control(pop))
#else
# define UWVM_UWVM_INT_STRICT_FP_BEGIN
# define UWVM_UWVM_INT_STRICT_FP_END
#endif

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-14
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

#pragma once

#ifndef UWVM_MODULE
// std
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
#if defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
    namespace details
    {
        inline constexpr ::uwvm2::utils::container::u8string_view runtime_llvm_jit_cache_no_sign_alias{u8"-Rllvm-cache-nosign"};
    }  // namespace details

# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wbraced-scalar-init"
# endif
    inline constexpr ::uwvm2::utils::cmdline::parameter runtime_llvm_jit_cache_no_sign{
        .name{u8"--runtime-llvm-jit-cache-no-sign"},
        .describe{u8"Do not generate Ed25519 signatures when writing LLVM JIT cache objects."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::runtime_llvm_jit_cache_no_sign_alias), 1uz}},
        .is_exist{::std::addressof(::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_cache_no_sign)},
        .cate{::uwvm2::utils::cmdline::categorization::runtime}};
# if defined(__clang__)
#  pragma clang diagnostic pop
# endif
#endif
}  // namespace uwvm2::uwvm::cmdline::params

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

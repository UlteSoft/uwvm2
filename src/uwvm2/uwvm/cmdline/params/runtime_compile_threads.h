/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-03-25
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
    namespace details
    {
        inline constexpr ::uwvm2::utils::container::u8string_view runtime_compile_threads_alias{u8"-Rct"};
#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            void runtime_compile_threads_pretreatment(char8_t const* const*&,
                                                      char8_t const* const*,
                                                      ::uwvm2::utils::container::vector<::uwvm2::utils::cmdline::parameter_parsing_results>&) noexcept;
#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type runtime_compile_threads_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                            ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                            ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
    }  // namespace details

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wbraced-scalar-init"
#endif
    inline constexpr ::uwvm2::utils::cmdline::parameter runtime_compile_threads{
        .name{u8"--runtime-compile-threads"},
        .describe{u8"Set the runtime compile thread count."},
        .usage{u8"<count:ssize_t>"},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::runtime_compile_threads_alias), 1uz}},
        .handle{::std::addressof(details::runtime_compile_threads_callback)},
        .pretreatment{::std::addressof(details::runtime_compile_threads_pretreatment)},
        .is_exist{::std::addressof(::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_existed)},
        .cate{::uwvm2::utils::cmdline::categorization::runtime}};
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
}  // namespace uwvm2::uwvm::cmdline::params

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

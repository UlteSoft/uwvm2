/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-01-03
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
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#if defined(UWVM_RUNTIME_LLVM_JIT)

# if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
# else
    UWVM_GNU_COLD inline constexpr
# endif
        ::uwvm2::utils::cmdline::parameter_return_type runtime_aot_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
                                                                            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
                                                                            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results *
                                                                                para_end) noexcept
    {
        bool const conflicting_runtime_shortcut{
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            ::uwvm2::uwvm::runtime::runtime_mode::is_runtime_mode_code_int_existed ||
# endif
            false};
        if(conflicting_runtime_shortcut) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                u8"uwvm: [error] only one runtime backend shortcut may be selected (-Rint or -Raot).\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile;
        ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only;
        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }

#endif
}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

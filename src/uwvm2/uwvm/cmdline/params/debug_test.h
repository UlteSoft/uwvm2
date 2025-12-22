/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-07
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
// import
# include <fast_io.h>
# include <uwvm2/utils/cmdline/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#ifdef _DEBUG
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
    namespace details
    {
        inline bool debug_test_is_exist{};  // [global]
        extern "C++"  ::uwvm2::utils::cmdline::parameter_return_type debug_test_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                            ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                            ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

    }  // namespace details

# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wbraced-scalar-init"
# endif
    inline constexpr ::uwvm2::utils::cmdline::parameter debug_test{.name{u8"--debug-test"},
                                                                   .describe{u8"Custom Debug Test (Only exists in debug mode)."},
                                                                   .handle{::std::addressof(details::debug_test_callback)},
                                                                   .is_exist{::std::addressof(details::debug_test_is_exist)},
                                                                   .cate{::uwvm2::utils::cmdline::categorization::debug}};
# if defined(__clang__)
#  pragma clang diagnostic pop
# endif
}
#endif

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif


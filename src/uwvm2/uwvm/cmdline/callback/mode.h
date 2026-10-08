/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
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
# include <limits>
# include <cstdint>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        ::uwvm2::utils::cmdline::parameter_return_type runtime_debug_jit_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_begin,
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept
    {
        // No operand is consumed and no input pointer is read or advanced.
        // Preserve explicitly selected runtime settings: prepare_debug_jit_mode
        // supplies defaults or reports the same unsupported-mode fatal as -m.
        ::uwvm2::uwvm::wasm::storage::execute_wasm_mode = ::uwvm2::uwvm::wasm::base::mode::debug_jit;
        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }

    // The decimal argument is a launch-owned host descriptor number, never a
    // guest FD. Runtime startup verifies the connected socket and its peer.
#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline
#endif
        ::uwvm2::utils::cmdline::parameter_return_type debug_jit_control_fd_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
            ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept
    {
        // [parameter, optional argument) | para_end
        // [safe                         ] | unsafe (one-past)
        //                                 ^^ next: formation only; check before dereference.
        auto const next{para_curr + 1u};
        bool valid{next != para_end && next->type == ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg};
        int parsed{};
        if(valid)
        {
            auto const digits{next->str};
            valid = !digits.empty();
            if(valid)
            {
                // [digits.cbegin(), digits.cend()) contains the whole argument.
                // [safe                        ] unsafe (one-past)
                //          ^^ no pointer is advanced manually; the bounded scanner
                // returns its cursor, which must equal end before accepting it.
                auto const first{digits.cbegin()};
                auto const last{digits.cend()};
                // A signed FD must still start with a digit: neither "+3" nor
                // "-3" may introduce a launch capability. Overflow, whitespace
                // and suffixes are rejected by fast_io's decimal scanner.
                valid = ::fast_io::char_category::is_c_digit(*first);
                if(valid)
                {
                    auto const result{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::dec_get<true, true>(parsed))};
                    valid = result.code == ::fast_io::parse_code::ok && result.iter == last && parsed >= 3;
                }
            }
        }
        if(!valid)
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE), u8"uwvm: ",
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED), u8"[error] ",
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                u8"Usage: ", ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::debug_jit_control_fd), u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }
        next->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        debug_jit_control_fd = parsed;
        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
    // Host launcher passes only a decimal inherited HANDLE value; no guest
    // command can create this launch-time authority or name a pipe path.
#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline
#endif
        ::uwvm2::utils::cmdline::parameter_return_type debug_jit_control_handle_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
            ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept
    {
        // [parameter, optional argument) | para_end
        // [safe                         ] | unsafe (one-past)
        //                                 ^^ next: form first, check before dereference.
        auto const next{para_curr + 1u};
        bool valid{next != para_end && next->type == ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg};
        ::std::uintptr_t parsed{};
        if(valid)
        {
            auto const digits{next->str};
            valid = !digits.empty();
            if(valid)
            {
                // [digits.cbegin(), digits.cend()) contains the whole argument.
                // [safe                        ] unsafe (one-past)
                //          ^^ first is dereferenced only after the nonempty check;
                // the scanner cannot read or move its cursor beyond last.
                auto const first{digits.cbegin()};
                auto const last{digits.cend()};
                valid = ::fast_io::char_category::is_c_digit(*first);
                if(valid)
                {
                    auto const result{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::dec_get<true, true>(parsed))};
                    valid = result.code == ::fast_io::parse_code::ok && result.iter == last &&
                            parsed >= 4u && parsed != (::std::numeric_limits<::std::uintptr_t>::max)();
                }
            }
        }
        if(!valid)
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE), u8"uwvm: ",
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED), u8"[error] ",
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                u8"Usage: ", ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::debug_jit_control_handle), u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }
        next->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        debug_jit_control_handle = parsed;
        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        ::uwvm2::utils::cmdline::parameter_return_type mode_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
                                                                     ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
                                                                     ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        // [... curr] ...
        // [  safe  ] unsafe (could be the module_end)
        //      ^^ para_curr

        auto currp1{para_curr + 1u};

        // [... curr] ...
        // [  safe  ] unsafe (could be the module_end)
        //            ^^ currp1

        // Check for out-of-bounds and not-argument
        if(currp1 == para_end || currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            // (currp1 == para_end):
            // [... curr] ...
            // [  safe  ] unsafe (could be the module_end)
            //            ^^ currp1

            // (currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg):
            // [... curr para] ...
            // [     safe    ] unsafe (could be the module_end)
            //           ^^ currp1

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::mode),
                                // print_usage comes with UWVM_COLOR_U8_RST_ALL
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        // [... curr arg] ...
        // [     safe   ] unsafe (could be the module_end)
        //           ^^ currp1

        // Setting the argument is already taken
        currp1->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;

        if(auto const currp1_str{currp1->str}; currp1_str == u8"section-details")
        {
            ::uwvm2::uwvm::wasm::storage::execute_wasm_mode = ::uwvm2::uwvm::wasm::base::mode::section_details;
        }
        else if(auto const currp1_str{currp1->str}; currp1_str == u8"run")
        {
            ::uwvm2::uwvm::wasm::storage::execute_wasm_mode = ::uwvm2::uwvm::wasm::base::mode::run;
        }
        else if(auto const currp1_str{currp1->str}; currp1_str == u8"debug-jit")
        {
            ::uwvm2::uwvm::wasm::storage::execute_wasm_mode = ::uwvm2::uwvm::wasm::base::mode::debug_jit;
        }
        else if(auto const currp1_str{currp1->str}; currp1_str == u8"validation")
        {
            ::uwvm2::uwvm::wasm::storage::execute_wasm_mode = ::uwvm2::uwvm::wasm::base::mode::validation;
        }
        else [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"Invalid mode \"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                currp1_str,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\". Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::mode),
                                // print_usage comes with UWVM_COLOR_U8_RST_ALL
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

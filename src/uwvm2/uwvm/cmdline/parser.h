/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-27
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
# include <cstdint>
# include <cstddef>
# include <memory>
# include <bit>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include "params.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline
{
    enum class parsing_return_val : unsigned
    {
        def,      // Continue with the main program
        return0,  // Instructs the main function to return 0
        returnm1  // Instructs the main function to return -1
    };

    // globals: There is no data contention because command line parsing is always done when the main thread is created.
    inline ::uwvm2::utils::container::vector<::uwvm2::utils::cmdline::parameter_parsing_results> parsing_result{};  // [global]
    inline char8_t const* argv0_dir{};                                                                              // [global]
    inline ::uwvm2::utils::cmdline::parameter_parsing_results* wasm_file_ppos{};                                    // [global]

    /// @brief parsing cmdline
    inline constexpr parsing_return_val parsing(::std::size_t argc, char8_t const* const* argv) noexcept
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parsing parameters"};
#endif

        auto& pr{parsing_result};
        auto const& ht{::uwvm2::uwvm::cmdline::hash_table};

        // It is assumed that they are all initialized by default and no checks will be made
#if 0 
        pr.clear();
#endif

        pr.reserve(argc);

        // If argc is 0uz, prohibit running
        if(!argc) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"No Parameters.",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                u8"\n\n");
            return parsing_return_val::returnm1;
        }

        // argv_end > argv_begin

        auto curr_argv{argv};

        // Use argv0 as the program runtime path
        if(*curr_argv != nullptr) [[likely]]
        {
            pr.emplace_back_unchecked(::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(*curr_argv)},
                                      nullptr,
                                      ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir);
            // storage argv0
            argv0_dir = *curr_argv;
        }

        // preprocessing
        auto const argv_end{argv + argc};
        for(++curr_argv; curr_argv != argv_end;)
        {
            if(*curr_argv == nullptr) [[unlikely]]
            {
                // This doesn't happen, but just in case

                // Next parameter
                ++curr_argv;
            }
            else if(::uwvm2::utils::container::u8cstring_view const argv_str{::fast_io::mnp::os_c_str(*curr_argv)}; argv_str.empty()) [[unlikely]]
            {
                // No characters, may appear on windows, the first value of this parameter is u8'\0'

                // Next parameter
                ++curr_argv;
            }
            else if(argv_str.front_unchecked() == u8'-')
            {
                // this is a parameter
                // Special treatment for run
                constexpr auto run_para{::std::addressof(::uwvm2::uwvm::cmdline::params::run)};

                // find hash table
                auto const para{::uwvm2::utils::cmdline::find_from_hash_table(ht, argv_str)};

                if(para == nullptr) [[unlikely]]
                {
                    // invalid parameter
                    pr.emplace_back_unchecked(argv_str, nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::invalid_parameter);

                    // Next parameter
                    ++curr_argv;
                }
                else if(para == run_para)
                {
                    // All subsequent parameters are runtime parameters and need not be parsed further, jumping directly out of the
                    pr.emplace_back_unchecked(argv_str, run_para, ::uwvm2::utils::cmdline::parameter_parsing_results_type::parameter);  // -- run

                    // grammatical error
                    if(++curr_argv == argv_end) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                            u8"[error] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"Usage: ",
                                            ::uwvm2::utils::cmdline::print_usage(*run_para),
                                            // print_usage comes with UWVM_COLOR_U8_RST_ALL
                                            u8"\n\n");

                        return parsing_return_val::returnm1;
                    }

                    wasm_file_ppos =
                        ::std::addressof(pr.emplace_back_unchecked(fast_io::u8cstring_view{::fast_io::mnp::os_c_str(*curr_argv)},
                                                                   nullptr,
                                                                   ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg));  // wasm file

                    for(++curr_argv; curr_argv != argv_end; ++curr_argv)
                    {
                        pr.emplace_back_unchecked(::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(*curr_argv)},
                                                  nullptr,
                                                  ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg);  // wasm parameters
                    }

                    break;  // End preprocessing
                }
                else [[likely]]
                {
                    if(para->is_exist)  // Check for null pointer
                    {
                        // not nullptr
                        if(auto& para_is_exist{*para->is_exist}; para_is_exist)
                        {
                            pr.emplace_back_unchecked(argv_str, para, ::uwvm2::utils::cmdline::parameter_parsing_results_type::duplicate_parameter);
                        }
                        else
                        {
                            para_is_exist = true;  // Marker already exists
                            pr.emplace_back_unchecked(argv_str, para, ::uwvm2::utils::cmdline::parameter_parsing_results_type::parameter);
                        }
                    }
                    else
                    {
                        pr.emplace_back_unchecked(argv_str, para, ::uwvm2::utils::cmdline::parameter_parsing_results_type::parameter);
                    }

                    // The preprocessor that calls this function
                    // Preprocessing is used to jump directly to the backmarked section.

                    // If a signed integer is required for a subsequent argument,
                    // marking one bit back as occupied_arg prevents the negative sign from being misidentified as an argument prefix.

                    if(para->pretreatment)
                    {
                        para->pretreatment(curr_argv, argv_end, pr);
                        // For pretreat, the result of pretreat is the next parameter.
                    }
                    else
                    {
                        // Next parameter
                        ++curr_argv;
                    }
                }
            }
            else
            {
                pr.emplace_back_unchecked(argv_str, nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg);

                // Next parameter
                ++curr_argv;
            }
        }

        // wasm_file_ppos - 1u is always valid and not memory safe
        // ...   "--run" (end)         "wasm file name" ...
        // ... wasm_file_ppos - 1u      wasm_file_ppos  ...
        auto const end_pos{wasm_file_ppos ? wasm_file_ppos - 1u : pr.end()};

        // Checking for wrong parameters vs. duplicates
        {
            // Here, as an entire output, the mutex needs to be controlled uniformly.
            // There are no unspecified external calls that make the mutex deadlock.

            // No copies will be made here.
            auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
            // Add raii locks while unlocking operations
            ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
            // No copies will be made here.
            auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

            bool shouldreturn{};

            for(auto curr_pr{pr.begin() + 1u}; curr_pr != end_pos; ++curr_pr)
            {
                if(curr_pr->type == ::uwvm2::utils::cmdline::parameter_parsing_results_type::invalid_parameter) [[unlikely]]
                {
                    shouldreturn = true;
                    ::fast_io::io::perr(u8log_output_ul,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                        u8"[error] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"invalid parameter: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                        curr_pr->str);

                    // Finding the most similar parameters
                    ::uwvm2::utils::container::u8string_view f_test_str{};

                    ::std::size_t const str_size{curr_pr->str.size()};

                    // Assumed size facilitates multiplicative optimization
                    constexpr ::std::size_t smax{::std::numeric_limits<::std::size_t>::max() / 4uz};
                    [[assume(str_size < smax)]];

                    // First time variance requirement within 40%
                    ::std::size_t const test_size{str_size * 4uz / 10uz};
                    ::std::size_t f_test_size{test_size};

                    for(auto const& j: ::uwvm2::uwvm::cmdline::parameter_lookup_table)
                    {
                        if(j.str.size() < str_size - f_test_size || j.str.size() > str_size + f_test_size)
                        {
                            // When the string size difference is already greater than the editable distance difference, just skip the
                            continue;
                        }

                        // Maximum stack size that will hit
                        constexpr ::std::size_t shortest_path_stack_size{parameter_max_name_size + parameter_max_name_size * 4uz / 10uz + 1uz};

                        // shortest_path_stack_size is larger than the maximum test size to ensure that there is no stack overflow.

                        // Addition may overflow, need to check size
                        static_assert(shortest_path_stack_size > parameter_max_name_size, "addition overflow");

                        // Calculate the shortest path distance
                        if(auto const dp_res{::uwvm2::utils::cmdline::shortest_path<shortest_path_stack_size>(curr_pr->str.data(),
                                                                                                              curr_pr->str.size(),
                                                                                                              j.str.data(),
                                                                                                              j.str.size())};
                           dp_res <= test_size)
                        {
                            f_test_str = j.str;
                            f_test_size = dp_res;
                        }
                    }

                    if(f_test_str.empty())
                    {
                        // The most similar parameters were not found
                        ::fast_io::io::perr(u8log_output_ul, ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL), u8"\n");
                    }
                    else
                    {
                        // Output the most similar parameter
                        ::fast_io::io::perr(u8log_output_ul,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8" (did you mean: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                            f_test_str,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8")",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                            u8"\n");
                    }
                }
                else if(curr_pr->type == ::uwvm2::utils::cmdline::parameter_parsing_results_type::duplicate_parameter) [[unlikely]]
                {
                    shouldreturn = true;
                    ::fast_io::io::perr(u8log_output_ul,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                        u8"[error] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"duplicate parameter: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                        curr_pr->str,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                        u8"\n");
                }
            }

            if(shouldreturn) [[unlikely]]
            {
                ::fast_io::io::perrln(u8log_output_ul);
                return parsing_return_val::returnm1;
            }

            // Here, guard will perform destructors.
        }

        // Call the function corresponding to each parameter
        {
            bool needexit{};
            bool needterminal{};

            for(auto curr_pr{pr.begin() + 1u}; curr_pr != end_pos; ++curr_pr)
            {
                auto const cp{curr_pr->para};

                if(cp == nullptr)
                {
                    // nonparametric
                    continue;
                }

                if(auto const cb{cp->handle}; cb != nullptr)
                {
                    ::uwvm2::utils::cmdline::parameter_return_type res;  // No initialization necessary

                    res = cb(pr.begin(), curr_pr, pr.end());

                    switch(res)
                    {
                        case ::uwvm2::utils::cmdline::parameter_return_type::def:
                        {
                            break;
                        }
                        case ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme:
                        {
                            return parsing_return_val::returnm1;
                        }
                        case ::uwvm2::utils::cmdline::parameter_return_type::return_imme:
                        {
                            return parsing_return_val::return0;
                        }
                        case ::uwvm2::utils::cmdline::parameter_return_type::return_soon:
                        {
                            needexit = true;
                            break;
                        }
                        case ::uwvm2::utils::cmdline::parameter_return_type::err_imme:
                        {
                            ::fast_io::fast_terminate();
                        }
                        case ::uwvm2::utils::cmdline::parameter_return_type::err_soon:
                        {
                            needterminal = true;
                            break;
                        }
                        [[unlikely]] default:
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::std::unreachable();  // A correct implementation will not show any other results, and if it does, the behavior is undefined.
                        }
                    }
                }
            }

            if(needterminal) [[unlikely]] { ::fast_io::fast_terminate(); }

            if(needexit) [[unlikely]] { return parsing_return_val::returnm1; }
        }

        // Check for unmarked arg
        {
            // Here, as an entire output, the mutex needs to be controlled uniformly.
            // There are no unspecified external calls that make the mutex deadlock.

            // No copies will be made here.
            auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
            // Add raii locks while unlocking operations
            ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
            // No copies will be made here.
            auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

            bool shouldreturn{};

            for(auto curr_pr{pr.begin() + 1u}; curr_pr != end_pos; ++curr_pr)
            {
                if(curr_pr->type == ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
                {
                    shouldreturn = true;

                    ::fast_io::io::perr(u8log_output_ul,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                        u8"[error] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"invalid option: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                        curr_pr->str,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                        u8"\n");
                }
            }

            if(shouldreturn) [[unlikely]]
            {
                ::fast_io::io::perrln(u8log_output_ul);
                return parsing_return_val::returnm1;
            }
        }

        return parsing_return_val::def;
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

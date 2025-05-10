/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-27
 * @copyright   APL-2 License
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

#ifdef UWVM_MODULE
import fast_io;
import uwvm2.uwvm.io;
import uwvm2.utils.ansies;
# ifdef UWVM_TIMER
import uwvm2.utils.debug;
# endif
import uwvm2.utils.madvise;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard;
import uwvm2.parser.wasm.binfmt.base;
import uwvm2.uwvm.utils.ansies;
import uwvm2.uwvm.utils.memory;
import uwvm2.uwvm.cmdline;
import uwvm2.uwvm.wasm.base;
import uwvm2.uwvm.wasm.storage;
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.wasm.custom;
#else
// std
# include <cstddef>
# include <cstdint>
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# ifdef UWVM_TIMER
#  include <uwvm2/utils/debug/impl.h>
# endif
# include <uwvm2/utils/madvise/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/parser/wasm/binfmt/base/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/utils/memory/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/custom/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::run
{
    inline int run() noexcept
    {
        if(!::uwvm2::uwvm::cmdline::wasm_file_ppos) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"No input file.\nTry \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                u8"--help",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" for more information.\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            return -2;  // The specified file is not available or cannot be opened
        }

        // storage module name
        auto const module_name{::uwvm2::uwvm::cmdline::wasm_file_ppos->str};

#ifdef __cpp_exceptions
        try
#endif
        {
#ifdef UWVM_TIMER
            ::uwvm2::utils::debug::timer parsing_timer{u8"file loader"};
#endif
            ::uwvm2::uwvm::wasm::storage::execute_wasm_file = ::fast_io::native_file_loader{module_name};
        }
#ifdef __cpp_exceptions
        catch(::fast_io::error e)
        {
            ::fast_io::io::perr(::uwvm2::uwvm::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Unable to open WASM file \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                module_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\": ",
                                e,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                u8"\n"
# ifndef _WIN32  // Win32 automatically adds a newline (winnt and win9x)
                                u8"\n"
# endif
            );
            return -2;  // The specified file is not available or cannot be opened
        }
#endif

        // If not specified by the user, detect it
        if(::uwvm2::uwvm::wasm::storage::execute_wasm_binfmt_ver == static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(0u))
        {
            ::uwvm2::uwvm::wasm::storage::execute_wasm_binfmt_ver = ::uwvm2::parser::wasm::binfmt::detect_wasm_binfmt_version(
                reinterpret_cast<::std::byte const*>(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cbegin()),
                reinterpret_cast<::std::byte const*>(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cend()));
        }

        // After detect
        // Instructs to read the file all the way into memory
        ::uwvm2::utils::madvise::my_madvise(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cbegin(),
                                            ::uwvm2::uwvm::wasm::storage::execute_wasm_file.size(),
                                            ::uwvm2::utils::madvise::madvise_flag::willneed);

        switch(::uwvm2::uwvm::wasm::storage::execute_wasm_binfmt_ver)
        {
            case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(0u):
            {
#ifndef UWVM_DISABLE_OUTPUT_WHEN_PARSE
                ::fast_io::io::perr(
                    ::uwvm2::uwvm::u8log_output,
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                    u8"uwvm: ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                    u8"[error] ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8"(offset=",
                    ::fast_io::mnp::addrvw(nullptr),
                    u8") Illegal WebAssembly file format.",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8"\nuwvm: ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_CYAN),
                    u8"[parser]",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8" -> ",
                    ::uwvm2::uwvm::utils::memory::print_memory{reinterpret_cast<::std::byte const*>(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cbegin()),
                                                               reinterpret_cast<::std::byte const*>(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cbegin()),
                                                               reinterpret_cast<::std::byte const*>(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cend())},
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                    u8"\n\n");
#endif
                return -3;  // wasm parsing error
            }
            case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u):
            {
                // handle exec module
                {
                    // set module name
                    ::uwvm2::uwvm::wasm::storage::execute_wasm_binfmt_ver1_storage.module_name = ::fast_io::u8string_view{module_name};

                    // storage wasm err
                    ::uwvm2::parser::wasm::base::error_impl execute_wasm_binfmt_ver1_storage_wasm_err{};

                    // parse wasm 1
#if defined(__cpp_exceptions) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
                    try
#endif
                    {
#ifdef UWVM_TIMER
                        ::uwvm2::utils::debug::timer parsing_timer{u8"parse binfmt ver1"};
#endif
                        ::uwvm2::uwvm::wasm::storage::execute_wasm_binfmt_ver1_storage = ::uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(
                            reinterpret_cast<::std::byte const*>(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cbegin()),
                            reinterpret_cast<::std::byte const*>(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cend()),
                            execute_wasm_binfmt_ver1_storage_wasm_err,
                            ::uwvm2::uwvm::wasm::storage::global_wasm_binfmt_ver1_paramaters);
                    }
#if defined(__cpp_exceptions) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
                    catch(::fast_io::error e)
                    {
# ifndef UWVM_DISABLE_OUTPUT_WHEN_PARSE
                        if(execute_wasm_binfmt_ver1_storage_wasm_err.err_code != ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok) [[likely]]
                        {
                            // print error
                            ::uwvm2::parser::wasm::base::error_output_t errout;
                            errout.module_begin = reinterpret_cast<::std::byte const*>(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cbegin());
                            errout.err = execute_wasm_binfmt_ver1_storage_wasm_err;
                            errout.flag.enable_ansi = static_cast<::std::uint_least8_t>(::uwvm2::uwvm::utils::ansies::put_color);
#  if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                            errout.flag.win32_use_text_attr = static_cast<::std::uint_least8_t>(!::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b);
#  endif
                            ::fast_io::io::perr(::uwvm2::uwvm::u8log_output,
                                                errout,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\nuwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_CYAN),
                                                u8"[parser]",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8" -> ",
                                                ::uwvm2::uwvm::utils::memory::print_memory{
                                                    reinterpret_cast<::std::byte const*>(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cbegin()),
                                                    execute_wasm_binfmt_ver1_storage_wasm_err.err_curr,
                                                    reinterpret_cast<::std::byte const*>(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cend())},
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                                u8"\n\n");
                        }
# endif
                        return -3;  // wasm parsing error
                    }
#endif
                    // handle custom
                    auto const custom_res{
                        ::uwvm2::uwvm::wasm::custom::handle_binfmt1_custom_section(::uwvm2::uwvm::wasm::storage::execute_wasm_binfmt_ver1_storage,
                                                                                   ::uwvm2::uwvm::wasm::custom::custom_handle_funcs)};
                    // 0: success, others: fault
                    if(custom_res) [[unlikely]]
                    {
                        return -4;  // custom handling error
                    }
                }

                /// @todo handle import modules

                break;
            }
            default: [[unlikely]] {
#ifndef UWVM_DISABLE_OUTPUT_WHEN_PARSE
                    ::fast_io::io::perr(::uwvm2::uwvm::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                        u8"[error] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"(offset=",
                                        ::fast_io::mnp::addrvw(4uz),
                                        u8") Unknown Binary Format Version of WebAssembly: \"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                        ::uwvm2::uwvm::wasm::storage::execute_wasm_binfmt_ver,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\".",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\nuwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_CYAN),
                                        u8"[parser]",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8" -> ",
                                        ::uwvm2::uwvm::utils::memory::print_memory{
                                            reinterpret_cast<::std::byte const*>(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cbegin()),
                                            reinterpret_cast<::std::byte const*>(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cbegin()) + 4uz,
                                            reinterpret_cast<::std::byte const*>(::uwvm2::uwvm::wasm::storage::execute_wasm_file.cend())},
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                        u8"\n\n");
#endif
                    return -3;  // wasm parsing error
                }
        }

        // run vm
        switch(::uwvm2::uwvm::wasm::storage::execute_wasm_mode)
        {
            case ::uwvm2::uwvm::wasm::base::mode::objdump:
            {
                /// @todo objdump
                break;
            }
            default: ::fast_io::unreachable();
        }

        return 0;
    }
}  // namespace uwvm2::uwvm::run

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

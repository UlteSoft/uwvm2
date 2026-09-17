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

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <concepts>
#include <memory>

#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include <fast_io_dsal/tuple.h>
# include <uwvm2/parser/wasm_custom/impl.h>
# include <uwvm2/uwvm/io/impl.h>
#else
# error "Module testing is not currently supported"
#endif

#include "error_output_test_stream.h"

int main()
{
    {
        auto obuf_u8err{error_test_u8err()};

        error_test_output_file<char> cf{u8"name_error_code_test_c.log", ::fast_io::open_mode::out};
        error_test_output_file<wchar_t> wcf{u8"name_error_code_test_wc.log", ::fast_io::open_mode::out};
        error_test_output_file<char8_t> u8cf{u8"name_error_code_test_u8c.log", ::fast_io::open_mode::out};
        error_test_output_file<char16_t> u16cf{u8"name_error_code_test_u16c.log", ::fast_io::open_mode::out};
        error_test_output_file<char32_t> u32f{u8"name_error_code_test_u32c.log", ::fast_io::open_mode::out};
        ::uwvm2::parser::wasm_custom::customs::name_error_output_t errout{};
        ::std::byte name_bytes[64]{};
        errout.name_begin = name_bytes;

        for(::std::uint_least32_t i{};
            i != static_cast<::std::uint_least32_t>(::uwvm2::parser::wasm_custom::customs::name_err_type_t::exceed_the_max_name_parser_limit) + 1u;
            ++i)
        {
            errout.name_err.curr = name_bytes + i % sizeof(name_bytes);
            switch(static_cast<::uwvm2::parser::wasm_custom::customs::name_err_type_t>(i))
            {
                case ::uwvm2::parser::wasm_custom::customs::name_err_type_t::illegal_char_sequence:
                {
                    errout.name_err.err.u32 = 0x00;
                    break;
                }
                case ::uwvm2::parser::wasm_custom::customs::name_err_type_t::exceed_the_max_name_parser_limit:
                {
                    errout.name_err.err.exceed_the_max_name_parser_limit.name = u8"function_names";
                    errout.name_err.err.exceed_the_max_name_parser_limit.value = 0u;
                    errout.name_err.err.exceed_the_max_name_parser_limit.maxval = 0u;
                    break;
                }
                default:
                {
                    errout.name_err.err.u64 = 0xcdcdcdcdcdcdcdcd;
                    break;
                }
            }

            errout.name_err.type = static_cast<::uwvm2::parser::wasm_custom::customs::name_err_type_t>(i);

            {
                ::uwvm2::parser::wasm_custom::customs::name_error_output_t obuf_u8err_errout{errout};
                obuf_u8err_errout.flag.enable_ansi = static_cast<::std::uint_least8_t>(::uwvm2::uwvm::utils::ansies::put_color);
#  if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                obuf_u8err_errout.flag.win32_use_text_attr = static_cast<::std::uint_least8_t>(!::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b);
#  endif
#if !defined(UWVM_TEST_ERROR_CHAR) || UWVM_TEST_ERROR_CHAR == 3
                ::fast_io::io::perrln(obuf_u8err, obuf_u8err_errout);
#endif
            }

#if !defined(UWVM_TEST_ERROR_CHAR) || UWVM_TEST_ERROR_CHAR == 1
            ::fast_io::io::perrln(cf, errout);
#endif
#if !defined(UWVM_TEST_ERROR_CHAR) || UWVM_TEST_ERROR_CHAR == 2
            ::fast_io::io::perrln(wcf, errout);
#endif
#if !defined(UWVM_TEST_ERROR_CHAR) || UWVM_TEST_ERROR_CHAR == 3
            ::fast_io::io::perrln(u8cf, errout);
#endif
#if !defined(UWVM_TEST_ERROR_CHAR) || UWVM_TEST_ERROR_CHAR == 4
            ::fast_io::io::perrln(u16cf, errout);
#endif
#if !defined(UWVM_TEST_ERROR_CHAR) || UWVM_TEST_ERROR_CHAR == 5
            ::fast_io::io::perrln(u32f, errout);
#endif
        }
    }
}

/*

(stderr)

sec1:
test1

sec2:
test2

sec3:
test3

sec4:
test4, not found

*/

// macro
#include <uwvm2/utils/macro/pop_macros.h>

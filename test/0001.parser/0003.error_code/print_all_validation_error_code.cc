/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-01-06
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
# include <uwvm2/compiler/validation/error/error_code_output.h>
# include <uwvm2/uwvm/io/impl.h>
#else
# error "Module testing is not currently supported"
#endif

int main()
{
    ::fast_io::basic_obuf<::fast_io::u8native_io_observer> obuf_u8err{::fast_io::u8err()};

    ::fast_io::obuf_file cf{u8"validation_error_code_test_c.log"};
    ::fast_io::wobuf_file wcf{u8"validation_error_code_test_wc.log"};
    ::fast_io::u8obuf_file u8cf{u8"validation_error_code_test_u8c.log"};
    ::fast_io::u16obuf_file u16cf{u8"validation_error_code_test_u16c.log"};
    ::fast_io::u32obuf_file u32f{u8"validation_error_code_test_u32c.log"};

    ::std::byte module_bytes[64]{};

    ::uwvm2::compiler::validation::error::error_output_t errout{};
    errout.module_begin = module_bytes;

    auto const last_ec{static_cast<::std::uint_least32_t>(::uwvm2::compiler::validation::error::code_validation_error_code::illegal_local_index)};

    for(::std::uint_least32_t i{}; i != last_ec + 1u; ++i)
    {
        errout.err.err_curr = module_bytes + (i % (sizeof(module_bytes) / sizeof(module_bytes[0])));
        errout.err.err_selectable.u64 = 0xcdcdcdcdcdcdcdcdULL;

        switch(static_cast<::uwvm2::compiler::validation::error::code_validation_error_code>(i))
        {
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_opbase:
            {
                errout.err.err_selectable.u8 = 0xFFu;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow:
            {
                errout.err.err_selectable.operand_stack_underflow.op_code_name = u8"select";
                errout.err.err_selectable.operand_stack_underflow.stack_size_actual = 2uz;
                errout.err.err_selectable.operand_stack_underflow.stack_size_required = 3uz;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::select_type_mismatch:
            {
                errout.err.err_selectable.select_type_mismatch.type_v1 = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32;
                errout.err.err_selectable.select_type_mismatch.type_v2 = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::select_cond_type_not_i32:
            {
                errout.err.err_selectable.select_cond_type_not_i32.cond_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::local_set_type_mismatch:
            {
                errout.err.err_selectable.local_variable_type_mismatch.local_index = 3u;
                errout.err.err_selectable.local_variable_type_mismatch.expected_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32;
                errout.err.err_selectable.local_variable_type_mismatch.actual_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::local_tee_type_mismatch:
            {
                errout.err.err_selectable.local_variable_type_mismatch.local_index = 7u;
                errout.err.err_selectable.local_variable_type_mismatch.expected_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32;
                errout.err.err_selectable.local_variable_type_mismatch.actual_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::not_local_function:
            {
                errout.err.err_selectable.not_local_function.function_index = 0uz;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_function_index:
            {
                errout.err.err_selectable.invalid_function_index.function_index = 10uz;
                errout.err.err_selectable.invalid_function_index.all_function_size = 5uz;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_local_index:
            {
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_local_index:
            {
                errout.err.err_selectable.illegal_local_index.local_index = 10u;
                errout.err.err_selectable.illegal_local_index.all_local_count = 5u;
                break;
            }
            default: break;
        }

        errout.err.err_code = static_cast<::uwvm2::compiler::validation::error::code_validation_error_code>(i);

        {
            ::uwvm2::compiler::validation::error::error_output_t obuf_u8err_errout{errout};
            obuf_u8err_errout.flag.enable_ansi = static_cast<::std::uint_least8_t>(::uwvm2::uwvm::utils::ansies::put_color);
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
            obuf_u8err_errout.flag.win32_use_text_attr = static_cast<::std::uint_least8_t>(!::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b);
#endif
            ::fast_io::io::perrln(obuf_u8err, obuf_u8err_errout);
        }

        ::fast_io::io::perrln(cf, errout);
        ::fast_io::io::perrln(wcf, errout);
        ::fast_io::io::perrln(u8cf, errout);
        ::fast_io::io::perrln(u16cf, errout);
        ::fast_io::io::perrln(u32f, errout);
    }
}

// macro
#include <uwvm2/utils/macro/pop_macros.h>

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
            case ::uwvm2::compiler::validation::error::code_validation_error_code::missing_block_type:
            {
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_block_type:
            {
                errout.err.err_selectable.u8 = 0x7Fu;
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
            case ::uwvm2::compiler::validation::error::code_validation_error_code::if_cond_type_not_i32:
            {
                errout.err.err_selectable.if_cond_type_not_i32.cond_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_else:
            {
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::if_then_result_mismatch:
            {
                errout.err.err_selectable.if_then_result_mismatch.expected_count = 1uz;
                errout.err.err_selectable.if_then_result_mismatch.actual_count = 1uz;
                errout.err.err_selectable.if_then_result_mismatch.expected_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32;
                errout.err.err_selectable.if_then_result_mismatch.actual_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_label_index:
            {
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_label_index:
            {
                errout.err.err_selectable.illegal_label_index.label_index = 7u;
                errout.err.err_selectable.illegal_label_index.all_label_count = 3u;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::br_value_type_mismatch:
            {
                errout.err.err_selectable.br_value_type_mismatch.op_code_name = u8"br_if";
                errout.err.err_selectable.br_value_type_mismatch.expected_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32;
                errout.err.err_selectable.br_value_type_mismatch.actual_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::br_cond_type_not_i32:
            {
                errout.err.err_selectable.br_cond_type_not_i32.op_code_name = u8"br_table";
                errout.err.err_selectable.br_cond_type_not_i32.cond_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::br_table_target_type_mismatch:
            {
                errout.err.err_selectable.br_table_target_type_mismatch.expected_label_index = 0u;
                errout.err.err_selectable.br_table_target_type_mismatch.mismatched_label_index = 2u;
                errout.err.err_selectable.br_table_target_type_mismatch.expected_arity = 1u;
                errout.err.err_selectable.br_table_target_type_mismatch.actual_arity = 1u;
                errout.err.err_selectable.br_table_target_type_mismatch.expected_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32;
                errout.err.err_selectable.br_table_target_type_mismatch.actual_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64;
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
            case ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_global_index:
            {
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_global_index:
            {
                errout.err.err_selectable.illegal_global_index.global_index = 10u;
                errout.err.err_selectable.illegal_global_index.all_global_count = 5u;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::immutable_global_set:
            {
                errout.err.err_selectable.immutable_global_set.global_index = 2u;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::global_set_type_mismatch:
            {
                errout.err.err_selectable.global_variable_type_mismatch.global_index = 9u;
                errout.err.err_selectable.global_variable_type_mismatch.expected_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64;
                errout.err.err_selectable.global_variable_type_mismatch.actual_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory:
            {
                errout.err.err_selectable.no_memory.op_code_name = u8"i32.load8_s";
                errout.err.err_selectable.no_memory.align = 0u;
                errout.err.err_selectable.no_memory.offset = 123u;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align:
            {
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset:
            {
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment:
            {
                errout.err.err_selectable.illegal_memarg_alignment.op_code_name = u8"i32.load";
                errout.err.err_selectable.illegal_memarg_alignment.align = 6u;
                errout.err.err_selectable.illegal_memarg_alignment.max_align = 2u;
                break;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32:
            {
                errout.err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"f64.load";
                errout.err.err_selectable.memarg_address_type_not_i32.addr_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64;
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

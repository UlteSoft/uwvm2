/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-27
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
# include <cstddef>
# include <cstdint>
# include <type_traits>
# include <concepts>
# include <utility>
# include <limits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/utils/ansies/ansi_push_macro.h>
# include <uwvm2/utils/ansies/win32_text_attr_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include "error.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::compiler::validation::error
{
    /// @brief Structured error handling is independent of the parser concept system, allowing for modular use.

    /// @brief Define the flag used for output
    struct error_output_flag_t
    {
        ::std::uint_least8_t enable_ansi : 1;
        ::std::uint_least8_t win32_use_text_attr : 1;  // on win95 - win7
    };

    /// @brief Provide information for output
    struct error_output_t
    {
        ::std::byte const* module_begin{};
        ::uwvm2::compiler::validation::error::code_validation_error_impl err{};
        error_output_flag_t flag{};
    };

    /// @brief Output, support: char, wchar_t, char8_t, char16_t, char32_t
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, error_output_t>, Stm && stream, error_output_t const& errout)
    {
        bool const enable_ansi{static_cast<bool>(errout.flag.enable_ansi)};
        switch(errout.err.err_code)
        {
            default:
            {
#include "error_code_outputs/eco_default.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::ok:
            {
#include "error_code_outputs/eco_ok.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::missing_end:
            {
#include "error_code_outputs/eco_missing_end.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::missing_block_type:
            {
#include "error_code_outputs/eco_missing_block_type.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_block_type:
            {
#include "error_code_outputs/eco_illegal_block_type.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_opbase:
            {
#include "error_code_outputs/eco_illegal_opbase.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow:
            {
#include "error_code_outputs/eco_operand_stack_underflow.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::select_type_mismatch:
            {
#include "error_code_outputs/eco_select_type_mismatch.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::select_cond_type_not_i32:
            {
#include "error_code_outputs/eco_select_cond_type_not_i32.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::if_cond_type_not_i32:
            {
#include "error_code_outputs/eco_if_cond_type_not_i32.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_else:
            {
#include "error_code_outputs/eco_illegal_else.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::if_then_result_mismatch:
            {
#include "error_code_outputs/eco_if_then_result_mismatch.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_label_index:
            {
#include "error_code_outputs/eco_invalid_label_index.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_label_index:
            {
#include "error_code_outputs/eco_illegal_label_index.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::br_value_type_mismatch:
            {
#include "error_code_outputs/eco_br_value_type_mismatch.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::br_cond_type_not_i32:
            {
#include "error_code_outputs/eco_br_cond_type_not_i32.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::br_table_target_type_mismatch:
            {
#include "error_code_outputs/eco_br_table_target_type_mismatch.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::local_set_type_mismatch:
            {
#include "error_code_outputs/eco_local_set_type_mismatch.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::local_tee_type_mismatch:
            {
#include "error_code_outputs/eco_local_tee_type_mismatch.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_global_index:
            {
#include "error_code_outputs/eco_invalid_global_index.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_global_index:
            {
#include "error_code_outputs/eco_illegal_global_index.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::immutable_global_set:
            {
#include "error_code_outputs/eco_immutable_global_set.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::global_set_type_mismatch:
            {
#include "error_code_outputs/eco_global_set_type_mismatch.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory:
            {
#include "error_code_outputs/eco_no_memory.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align:
            {
#include "error_code_outputs/eco_invalid_memarg_align.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset:
            {
#include "error_code_outputs/eco_invalid_memarg_offset.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment:
            {
#include "error_code_outputs/eco_illegal_memarg_alignment.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32:
            {
#include "error_code_outputs/eco_memarg_address_type_not_i32.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::not_local_function:
            {
#include "error_code_outputs/eco_not_local_function.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_function_index:
            {
#include "error_code_outputs/eco_invalid_function_index.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_local_index:
            {
#include "error_code_outputs/eco_invalid_local_index.h"
                return;
            }
            case ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_local_index:
            {
#include "error_code_outputs/eco_illegal_local_index.h"
                return;
            }
        }
    }
}
#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/ansies/win32_text_attr_pop_macro.h>
# include <uwvm2/utils/ansies/ansi_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

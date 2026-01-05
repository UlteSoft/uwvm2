/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-07-07
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

#include "uwvm2/compiler/validation/error/error.h"
#include "uwvm2/parser/wasm/standard/wasm1/opcode/mvp.h"
#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
# include <limits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/intrinsics/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/utils/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/compiler/validation/error/impl.h>
# include <uwvm2/compiler/validation/concepts/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::compiler::validation::standard::wasm1
{
    using wasm1_code = ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic;

    using wasm1_code_version_type = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline void validate_code(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version,
                              ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
                              ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...> const& code_type,
                              ::std::byte const* code_begin,
                              ::std::byte const* code_end,
                              ::uwvm2::compiler::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        auto code_curr{code_begin};

        // [before_section ... ] | opbase opextent
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ code_curr

        // a WebAssembly function with type '() -> ()' (often written as returning “nil”) can have no meaningful code, but it still must have a valid
        // instruction sequence—at minimum an end.

        for(;;)
        {
            if(code_curr == code_end) [[unlikely]]
            {
                // [... ] | (end)
                // [safe] | unsafe (could be the section_end)
                //          ^^ code_curr

                // Validation completes when the end is reached, so this condition can never be met. If it were met, it would indicate a missing end.
                err.err_curr = code_curr;
                err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::missing_end;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // opbase [opextent|code] ...
            // [safe] unsafe (could be the section_end)
            // ^^ code_curr

            // switch the code
            wasm1_code check_end;  // no initialize necessary
            ::std::memcpy(::std::addressof(check_end), code_curr, sizeof(wasm1_code));

            switch(check_end)
            {
                case wasm1_code::unreachable:
                {
                    break;
                }
                case wasm1_code::nop:
                {
                    break;
                }
                case wasm1_code::block:
                {
                    break;
                }
                case wasm1_code::loop:
                {
                    break;
                }
                case wasm1_code::if_:
                {
                    break;
                }
                case wasm1_code::else_:
                {
                    break;
                }
                case wasm1_code::end:
                {
                    break;
                }
                case wasm1_code::br:
                {
                    break;
                }
                case wasm1_code::br_if:
                {
                    break;
                }
                case wasm1_code::br_table:
                {
                    break;
                }
                case wasm1_code::return_:
                {
                    break;
                }
                case wasm1_code::call:
                {
                    break;
                }
                case wasm1_code::call_indirect:
                {
                    break;
                }
                case wasm1_code::drop:
                {
                    break;
                }
                case wasm1_code::select:
                {
                    break;
                }
                case wasm1_code::local_get:
                {
                    break;
                }
                case wasm1_code::local_set:
                {
                    break;
                }
                case wasm1_code::local_tee:
                {
                    break;
                }
                case wasm1_code::global_get:
                {
                    break;
                }
                case wasm1_code::global_set:
                {
                    break;
                }
                case wasm1_code::i32_load:
                {
                    break;
                }
                case wasm1_code::i64_load:
                {
                    break;
                }
                case wasm1_code::f32_load:
                {
                    break;
                }
                case wasm1_code::f64_load:
                {
                    break;
                }
                case wasm1_code::i32_load8_s:
                {
                    break;
                }
                case wasm1_code::i32_load8_u:
                {
                    break;
                }
                case wasm1_code::i32_load16_s:
                {
                    break;
                }
                case wasm1_code::i32_load16_u:
                {
                    break;
                }
                case wasm1_code::i64_load8_s:
                {
                    break;
                }
                case wasm1_code::i64_load8_u:
                {
                    break;
                }
                case wasm1_code::i64_load16_s:
                {
                    break;
                }
                case wasm1_code::i64_load16_u:
                {
                    break;
                }
                case wasm1_code::i64_load32_s:
                {
                    break;
                }
                case wasm1_code::i64_load32_u:
                {
                    break;
                }
                case wasm1_code::i32_store:
                {
                    break;
                }
                case wasm1_code::i64_store:
                {
                    break;
                }
                case wasm1_code::f32_store:
                {
                    break;
                }
                case wasm1_code::f64_store:
                {
                    break;
                }
                case wasm1_code::i32_store8:
                {
                    break;
                }
                case wasm1_code::i32_store16:
                {
                    break;
                }
                case wasm1_code::i64_store8:
                {
                    break;
                }
                case wasm1_code::i64_store16:
                {
                    break;
                }
                case wasm1_code::i64_store32:
                {
                    break;
                }
                case wasm1_code::memory_size:
                {
                    break;
                }
                case wasm1_code::memory_grow:
                {
                    break;
                }
                case wasm1_code::i32_const:
                {
                    break;
                }
                case wasm1_code::i64_const:
                {
                    break;
                }
                case wasm1_code::f32_const:
                {
                    break;
                }
                case wasm1_code::f64_const:
                {
                    break;
                }
                case wasm1_code::i32_eqz:
                {
                    break;
                }
                case wasm1_code::i32_eq:
                {
                    break;
                }
                case wasm1_code::i32_ne:
                {
                    break;
                }
                case wasm1_code::i32_lt_s:
                {
                    break;
                }
                case wasm1_code::i32_lt_u:
                {
                    break;
                }
                case wasm1_code::i32_gt_s:
                {
                    break;
                }
                case wasm1_code::i32_gt_u:
                {
                    break;
                }
                case wasm1_code::i32_le_s:
                {
                    break;
                }
                case wasm1_code::i32_le_u:
                {
                    break;
                }
                case wasm1_code::i32_ge_s:
                {
                    break;
                }
                case wasm1_code::i32_ge_u:
                {
                    break;
                }
                case wasm1_code::i64_eqz:
                {
                    break;
                }
                case wasm1_code::i64_eq:
                {
                    break;
                }
                case wasm1_code::i64_ne:
                {
                    break;
                }
                case wasm1_code::i64_lt_s:
                {
                    break;
                }
                case wasm1_code::i64_lt_u:
                {
                    break;
                }
                case wasm1_code::i64_gt_s:
                {
                    break;
                }
                case wasm1_code::i64_gt_u:
                {
                    break;
                }
                case wasm1_code::i64_le_s:
                {
                    break;
                }
                case wasm1_code::i64_le_u:
                {
                    break;
                }
                case wasm1_code::i64_ge_s:
                {
                    break;
                }
                case wasm1_code::i64_ge_u:
                {
                    break;
                }
                case wasm1_code::f32_eq:
                {
                    break;
                }
                case wasm1_code::f32_ne:
                {
                    break;
                }
                case wasm1_code::f32_lt:
                {
                    break;
                }
                case wasm1_code::f32_gt:
                {
                    break;
                }
                case wasm1_code::f32_le:
                {
                    break;
                }
                case wasm1_code::f32_ge:
                {
                    break;
                }
                case wasm1_code::f64_eq:
                {
                    break;
                }
                case wasm1_code::f64_ne:
                {
                    break;
                }
                case wasm1_code::f64_lt:
                {
                    break;
                }
                case wasm1_code::f64_gt:
                {
                    break;
                }
                case wasm1_code::f64_le:
                {
                    break;
                }
                case wasm1_code::f64_ge:
                {
                    break;
                }
                case wasm1_code::i32_clz:
                {
                    break;
                }
                case wasm1_code::i32_ctz:
                {
                    break;
                }
                case wasm1_code::i32_popcnt:
                {
                    break;
                }
                case wasm1_code::i32_add:
                {
                    break;
                }
                case wasm1_code::i32_sub:
                {
                    break;
                }
                case wasm1_code::i32_mul:
                {
                    break;
                }
                case wasm1_code::i32_div_s:
                {
                    break;
                }
                case wasm1_code::i32_div_u:
                {
                    break;
                }
                case wasm1_code::i32_rem_s:
                {
                    break;
                }
                case wasm1_code::i32_rem_u:
                {
                    break;
                }
                case wasm1_code::i32_and:
                {
                    break;
                }
                case wasm1_code::i32_or:
                {
                    break;
                }
                case wasm1_code::i32_xor:
                {
                    break;
                }
                case wasm1_code::i32_shl:
                {
                    break;
                }
                case wasm1_code::i32_shr_s:
                {
                    break;
                }
                case wasm1_code::i32_shr_u:
                {
                    break;
                }
                case wasm1_code::i32_rotl:
                {
                    break;
                }
                case wasm1_code::i32_rotr:
                {
                    break;
                }
                case wasm1_code::i64_clz:
                {
                    break;
                }
                case wasm1_code::i64_ctz:
                {
                    break;
                }
                case wasm1_code::i64_popcnt:
                {
                    break;
                }
                case wasm1_code::i64_add:
                {
                    break;
                }
                case wasm1_code::i64_sub:
                {
                    break;
                }
                case wasm1_code::i64_mul:
                {
                    break;
                }
                case wasm1_code::i64_div_s:
                {
                    break;
                }
                case wasm1_code::i64_div_u:
                {
                    break;
                }
                case wasm1_code::i64_rem_s:
                {
                    break;
                }
                case wasm1_code::i64_rem_u:
                {
                    break;
                }
                case wasm1_code::i64_and:
                {
                    break;
                }
                case wasm1_code::i64_or:
                {
                    break;
                }
                case wasm1_code::i64_xor:
                {
                    break;
                }
                case wasm1_code::i64_shl:
                {
                    break;
                }
                case wasm1_code::i64_shr_s:
                {
                    break;
                }
                case wasm1_code::i64_shr_u:
                {
                    break;
                }
                case wasm1_code::i64_rotl:
                {
                    break;
                }
                case wasm1_code::i64_rotr:
                {
                    break;
                }
                case wasm1_code::f32_abs:
                {
                    break;
                }
                case wasm1_code::f32_neg:
                {
                    break;
                }
                case wasm1_code::f32_ceil:
                {
                    break;
                }
                case wasm1_code::f32_floor:
                {
                    break;
                }
                case wasm1_code::f32_trunc:
                {
                    break;
                }
                case wasm1_code::f32_nearest:
                {
                    break;
                }
                case wasm1_code::f32_sqrt:
                {
                    break;
                }
                case wasm1_code::f32_add:
                {
                    break;
                }
                case wasm1_code::f32_sub:
                {
                    break;
                }
                case wasm1_code::f32_mul:
                {
                    break;
                }
                case wasm1_code::f32_div:
                {
                    break;
                }
                case wasm1_code::f32_min:
                {
                    break;
                }
                case wasm1_code::f32_max:
                {
                    break;
                }
                case wasm1_code::f32_copysign:
                {
                    break;
                }
                case wasm1_code::f64_abs:
                {
                    break;
                }
                case wasm1_code::f64_neg:
                {
                    break;
                }
                case wasm1_code::f64_ceil:
                {
                    break;
                }
                case wasm1_code::f64_floor:
                {
                    break;
                }
                case wasm1_code::f64_trunc:
                {
                    break;
                }
                case wasm1_code::f64_nearest:
                {
                    break;
                }
                case wasm1_code::f64_sqrt:
                {
                    break;
                }
                case wasm1_code::f64_add:
                {
                    break;
                }
                case wasm1_code::f64_sub:
                {
                    break;
                }
                case wasm1_code::f64_mul:
                {
                    break;
                }
                case wasm1_code::f64_div:
                {
                    break;
                }
                case wasm1_code::f64_min:
                {
                    break;
                }
                case wasm1_code::f64_max:
                {
                    break;
                }
                case wasm1_code::f64_copysign:
                {
                    break;
                }
                case wasm1_code::i32_wrap_i64:
                {
                    break;
                }
                case wasm1_code::i32_trunc_f32_s:
                {
                    break;
                }
                case wasm1_code::i32_trunc_f32_u:
                {
                    break;
                }
                case wasm1_code::i32_trunc_f64_s:
                {
                    break;
                }
                case wasm1_code::i32_trunc_f64_u:
                {
                    break;
                }
                case wasm1_code::i64_extend_i32_s:
                {
                    break;
                }
                case wasm1_code::i64_extend_i32_u:
                {
                    break;
                }
                case wasm1_code::i64_trunc_f32_s:
                {
                    break;
                }
                case wasm1_code::i64_trunc_f32_u:
                {
                    break;
                }
                case wasm1_code::i64_trunc_f64_s:
                {
                    break;
                }
                case wasm1_code::i64_trunc_f64_u:
                {
                    break;
                }
                case wasm1_code::f32_convert_i32_s:
                {
                    break;
                }
                case wasm1_code::f32_convert_i32_u:
                {
                    break;
                }
                case wasm1_code::f32_convert_i64_s:
                {
                    break;
                }
                case wasm1_code::f32_convert_i64_u:
                {
                    break;
                }
                case wasm1_code::f32_demote_f64:
                {
                    break;
                }
                case wasm1_code::f64_convert_i32_s:
                {
                    break;
                }
                case wasm1_code::f64_convert_i32_u:
                {
                    break;
                }
                case wasm1_code::f64_convert_i64_s:
                {
                    break;
                }
                case wasm1_code::f64_convert_i64_u:
                {
                    break;
                }
                case wasm1_code::f64_promote_f32:
                {
                    break;
                }
                case wasm1_code::i32_reinterpret_f32:
                {
                    break;
                }
                case wasm1_code::i64_reinterpret_f64:
                {
                    break;
                }
                case wasm1_code::f32_reinterpret_i32:
                {
                    break;
                }
                case wasm1_code::f64_reinterpret_i64:
                {
                    break;
                }
                [[unlikely]] default:
                {
                    break;
                }
            }
        }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif

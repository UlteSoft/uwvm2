/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <concepts>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/object/impl.h>
# include "define.h"
#endif

#if !(__cpp_pack_indexing >= 202311L)
# error "UWVM requires at least C++26 standard compiler. See https://en.cppreference.com/w/cpp/feature_test#cpp_pack_indexing"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    /// @note This function cannot be forced to [[noreturn]] because some implementations embedded as plugins may need to perform cleanup operations instead of
    /// immediately terminating the program. The default implementation terminates the program.
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_unreachable(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        // curr_unreachable_opfunc unreachable_func ...
        // safe
        // ^^ type...[0]

        type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        // curr_unreachable_opfunc unreachable_func ...
        // safe
        //                         ^^ type...[0]

        ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t unreachable_func_p;  // no init
        ::std::memcpy(::std::addressof(unreachable_func_p), type...[0], sizeof(unreachable_func_t));

        if(unreachable_func_p) { unreachable_func_p(); }
        else
        {
            // default crash the vm
            ::fast_io::fast_terminate();
        }
    }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_br(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        // curr_uwvmint_br jmp_ip (jmp to next_opfunc) ...
        // safe
        // ^^ type...[0]

        type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        // curr_uwvmint_br jmp_ip (jmp to next_opfunc) ...
        // safe
        //                 ^^ type...[0]

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));

        type...[0] = jmp_ip;

        // next_opfunc (*jmp_ip) ...
        // safe
        // ^^ type...[0]

        if constexpr(CompileOption.is_tail_call)
        {
            // next_opfunc (*jmp_ip) ...
            // safe
            // ^^ type...[0]

            // `jmp_ip` may not be aligned for a function-pointer slot; always load via memcpy.
            ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

            UWVM_MUSTTAIL return next_interpreter(type...);
        }
    }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i32_stack_top,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_br_if(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        // curr_uwvmint_br_if jmp_ip next_op_false
        // safe
        // ^^ type...[0]

        type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        // curr_uwvmint_br_if jmp_ip next_op_false
        // safe
        //                    ^^ type...[0]

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));

        type...[0] += sizeof(jmp_ip);

        // curr_uwvmint_br_if jmp_ip next_op_false
        // safe
        //                           ^^ type...[0]

        auto const cond{
            ::uwvm2::runtime::compiler::uwvm_int::optable::
                get_curr_val_from_operand_stack_top<CompileOption, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32, curr_i32_stack_top>(type...)};

        if(cond)
        {
            type...[0] = jmp_ip;

            // next_op_true (*jmp_ip) ...
            // safe
            // ^^ type...[0]
        }

        // next_opfunc ...
        // safe
        // ^^ type...[0]

        if constexpr(CompileOption.is_tail_call)
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

            UWVM_MUSTTAIL return next_interpreter(type...);
        }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

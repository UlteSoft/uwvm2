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

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    namespace manipulate
    {
        inline ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_call_func_t call_func{};  // [global]

        UWVM_ALWAYS_INLINE inline constexpr void
            call(::std::size_t curr_module_id, ::std::size_t call_function, ::std::byte** uwvm_int_operand_stack_top_ptr) UWVM_THROWS
        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(call_func == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

            call_func(curr_module_id, call_function, uwvm_int_operand_stack_top_ptr);
        }
    }  // namespace manipulate

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_call(Type... type) UWVM_THROWS
    {
        // Due to the variability of types, the call function strictly requires all arguments to be on the stack rather than in the top-of-stack register.
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        // This prevents binary bloat caused by differing template options.
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        // ^^ type...[0]

        type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        //                   ^^ type...[0]

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), type...[0], sizeof(curr_module_id));

        type...[0] += sizeof(curr_module_id);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        //                                  ^^ type...[0]

        ::std::size_t call_function;  // no init
        ::std::memcpy(::std::addressof(call_function), type...[0], sizeof(call_function));

        type...[0] += sizeof(call_function);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        //                                                ^^ type...[0]

        // call function
        manipulate::call(curr_module_id, call_function, ::std::addressof(type...[1]));

        // next op
        ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_call(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 1uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        // ^^ type...[0]

        typeref...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        //                   ^^ type...[0]

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), typeref...[0], sizeof(curr_module_id));

        typeref...[0] += sizeof(curr_module_id);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        //                                  ^^ type...[0]

        ::std::size_t call_function;  // no init
        ::std::memcpy(::std::addressof(call_function), typeref...[0], sizeof(call_function));

        typeref...[0] += sizeof(call_function);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        //                                                ^^ type...[0]

        // call function
        manipulate::call(curr_module_id, call_function, ::std::addressof(typeref...[1]));

        // Function calls are initiated by higher-level functions.
    }

    namespace translate
    {
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_call_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            // Because there is no top-of-stack dependency, there is only a single version here.
            return uwvmint_call<CompileOption, Type...>;
        }

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_call_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_call_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            // Because there is no top-of-stack dependency, there is only a single version here.
            return uwvmint_call<CompileOption, Type...>;
        }

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_call_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

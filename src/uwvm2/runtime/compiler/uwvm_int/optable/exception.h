/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <concepts>
# include <cstddef>
# include <cstring>
# include <memory>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/runtime/exception/impl.h>
# include "call.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) && defined(UWVM_CPP_EXCEPTIONS)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    struct exception_continuation
    {
        ::std::byte const* ip{};
        ::std::byte* top{};
    };

    // A compiler-owned immutable call-site descriptor. Only calls protected by nonempty try_table
    // handlers use this opcode; ordinary calls, memory operations and arithmetic remain unchanged.
    // dispatch selects the first matching lexical handler and returns a checked bytecode/stack target.
    // A null IP means "no handler": the typed native exception is rethrown to the caller.
    //
    // The compiler must spill every live cached operand before this protected call. Cached ABI
    // parameters are forwarded unchanged; they do not contain the new payload. The selected handler
    // must resume with empty logical caches or reload its required cache slots from the restored stack.
    //
    // Before returning a handler target, dispatch must transfer any reference roots into the resumed
    // activation. The caught value can be destroyed immediately afterwards. Payload bits alone are
    // insufficient ownership for catch_ref or reference-valued arguments. The borrowed value_ref lets
    // a catch_ref handler retain the whole exception instance before the native catch ends.
    struct exception_call_site
    {
        exception_continuation (*dispatch)(void const*, ::uwvm2::runtime::exception::value_ref const&,
                                           ::std::byte const*, ::std::byte*) noexcept{};
        void const* context{};
    };

    namespace details
    {
        template<bool Indirect, bool Reference = false>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline exception_continuation invoke_with_exception_handler(
            ::std::byte const* immediates, ::std::byte* top, ::std::byte const* locals) UWVM_THROWS
        {
            constexpr ::std::size_t argument_count{Indirect ? 3uz : 2uz};
            ::std::size_t arguments[argument_count];
            // [module, function/type] OR [module, type, table] [call-site pointer] [next opfunc]
            // [safe                                    ] compiler reserves the entire immediate tuple.
            // ^^ immediates; memcpy accepts bytecode with unaligned integer/pointer fields.
            ::std::memcpy(arguments, immediates, sizeof(arguments));
            auto const site_slot{immediates + sizeof(arguments)};
            // [checked arguments][call-site pointer][next opfunc]
            // [safe              ^^^^^^^^^^^^^^^^^^] site_slot points to a full emitted pointer slot.
            auto const next{site_slot + sizeof(exception_call_site const*)};
            // [checked arguments][call-site pointer][next opfunc]
            // [safe                                  ^^^^^^^^^^^] next is readable and owned by this function.
            try
            {
                if constexpr(Indirect)
                { return {next, details::call_indirect(arguments[0], arguments[1], arguments[2], top)}; }
                else if constexpr(Reference) { return {next, details::call_ref(arguments[0], arguments[1], top)}; }
                else { return {next, details::call(arguments[0], arguments[1], top)}; }
            }
            catch(::uwvm2::runtime::exception::guest_exception const& caught)
            {
                exception_call_site const* site;
                // [call-site pointer] compiler-owned target remains live with the executing bytecode.
                // [safe             ] pointer bytes are loaded only on the exceptional path.
                // ^^ site receives that complete, possibly unaligned pointer representation.
                ::std::memcpy(::std::addressof(site), site_slot, sizeof(site));
                if(site == nullptr || site->dispatch == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const resumed{site->dispatch(site->context, caught.instance(), locals, top)};
                if(resumed.ip == nullptr) { throw; }
                // The dispatcher proves resumed.ip names a complete opfunc and resumed.top belongs
                // to this caller's still-live frame; any escaped payload references have been rooted.
                return resumed;
            }
            // Leaving the catch balances the C++ activation BEFORE the outer opcode's musttail.
            // Foreign host exceptions and traps do not match guest_exception and are never dispatched.
        }
    }

    template<bool Indirect, bool Reference, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_catching(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0], ::std::byte const*>);
        // [opfunc][complete protected-call immediates][next opfunc]
        // [safe  ] dispatch already loaded this opfunc; advancing skips its exact pointer width.
        auto const immediates{type...[0] + sizeof(uwvm_interpreter_opfunc_t<Type...>)};
        auto const resumed{details::invoke_with_exception_handler<Indirect, Reference>(immediates, type...[1], type...[2])};
        // [caller-owned bytecode target][...]  [caller operand frame ...] top
        // [safe                          ]    [safe                        ]
        // ^^ IP/top are either the normal continuation or the checked cold dispatcher result.
        type...[0] = resumed.ip;
        type...[1] = resumed.top;
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }

    template<bool Indirect, bool Reference, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_catching(Type&... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0], ::std::byte const*>);
        static_assert(Option.i32_stack_top_begin_pos == SIZE_MAX && Option.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(Option.i64_stack_top_begin_pos == SIZE_MAX && Option.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(Option.f32_stack_top_begin_pos == SIZE_MAX && Option.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(Option.f64_stack_top_begin_pos == SIZE_MAX && Option.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(Option.v128_stack_top_begin_pos == SIZE_MAX && Option.v128_stack_top_end_pos == SIZE_MAX);
        // [opfunc][complete protected-call immediates][next opfunc]
        // [safe  ] the by-reference dispatcher proved the current opfunc slot readable.
        auto const immediates{type...[0] + sizeof(uwvm_interpreter_opfunc_byref_t<Type...>)};
        auto const resumed{details::invoke_with_exception_handler<Indirect, Reference>(immediates, type...[1], type...[2])};
        // [caller-owned next/handler opfunc] [caller-owned operand frame ...] top
        // [safe                          ] [safe                             ]
        // ^^ only complete targets returned by the normal or exception dispatch path are published.
        type...[0] = resumed.ip;
        type...[1] = resumed.top;
    }

    namespace translate
    {
        template<bool Indirect, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_call_catching_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_call_catching<Indirect, false, Option, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_call_catching<Indirect, false, Option, Type...>); }
        }

        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_call_ref_catching_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_call_catching<false, true, Option, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_call_catching<false, true, Option, Type...>); }
        }
    }
}
#endif
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

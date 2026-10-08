/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <memory>
# include <type_traits>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/utils/container/impl.h>
# include "memory64.h"
# include "threads.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    namespace details::memory64
    {
        template<atomic_rmw_operation Operation, bool Wide, ::std::size_t Bytes, auto Bounds,
                 uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos,
                 uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr void execute_rmw(Type&... args) UWVM_THROWS
        {
            constexpr bool compare{Operation == atomic_rmw_operation::compare_exchange};
            using layout = atomic_rmw_ring_layout<Wide, compare, Option, ValuePos, AddressPos, true>;
            using value_type = typename layout::value_type;
            static_assert(Bytes <= sizeof(value_type));
            // [handler][memory*][u64 offset][successor] compiler-owned bytecode
            // [safe                                  ]
            // ^^ IP: all slots and the complete typed operand frame were proved.
            if constexpr(Option.is_tail_call) { args...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>); }
            else { args...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); }
            // [handler][memory*][u64 offset][successor]
            //          ^^ IP after the complete dispatch slot.
            auto const memory{read_imm<native_memory_t*>(args...[0])};
            // [handler][memory*][u64 offset][successor]
            //                   ^^ IP after the complete owner-pointer slot.
            auto const offset{read_imm<::std::uint64_t>(args...[0])};
            // [handler][memory*][u64 offset][successor]
            //                               ^^ IP: complete successor remains.
            // [older operands][i64 address][expected?][replacement] <- SP or ring
            // [safe                                               ] typed frame
            auto const inputs{pop_atomic_rmw_inputs_impl<true, Wide, compare, Option, ValuePos, AddressPos>(args...)};
            // [older operands] <- SP; exactly layout::popped_bytes were consumed.
            // Each cached input retains its original ring slot until the result.
            auto const effective{wasm64_effective_offset(inputs.address, offset)};
            if((effective.offset & (Bytes - 1uz)) != 0u) [[unlikely]] { unaligned_atomic(); }
            enter_memory_operation_memory_lock(*memory); // mmap: no pin/lock code
            Bounds(*memory, 0uz, offset, effective, Bytes);
            // [live pinned/stable reservation ... naturally aligned Bytes ...]
            //                                    ^^ pointer: complete u65 bounds
            // proof precedes native narrowing. An aligned atomic cannot straddle
            // a protection page; custom subword pages use software bounds.
            auto const pointer{ptr_add_u64(memory->memory_begin, effective.offset)};
            auto const old{static_cast<value_type>(::uwvm2::runtime::compiler::shared::wasm_threads::atomic_rmw_le<Operation, Bytes>(
                pointer, static_cast<::std::uint64_t>(inputs.value), static_cast<::std::uint64_t>(inputs.expected)))};
            exit_memory_operation_memory_lock(*memory);
            if constexpr(layout::value_slots != 0uz)
            { set_curr_val_to_stacktop_cache<Option, value_type, layout::output_pos>(old, args...); }
            else
            {
                // [older operands][reserved result slot] ... frame end
                // [safe                               ]
                //                  ^^ SP; translation reserved sizeof(old) bytes.
                ::std::memcpy(args...[1], ::std::addressof(old), sizeof(old));
                args...[1] += sizeof(old);
                // [older operands][old value] <- SP, within the proved allocation.
            }
        }
    }

    template<details::atomic_rmw_operation Operation, bool Wide, ::std::size_t Bytes, auto Bounds,
             uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos,
             uwvm_int_stack_top_type... Type>
        requires(Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_rmw(Type... args) UWVM_THROWS
    {
        details::memory64::execute_rmw<Operation, Wide, Bytes, Bounds, Option, ValuePos, AddressPos>(args...);
        // [consumed instruction][successor] ... compiler-owned bytecode
        // [safe                ][safe     ]
        //                        ^^ IP, unchanged while reading the next handler.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), args...[0], sizeof(next));
        UWVM_MUSTTAIL return next(args...);
    }

    template<details::atomic_rmw_operation Operation, bool Wide, ::std::size_t Bytes, auto Bounds,
             uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos,
             uwvm_int_stack_top_type... Type>
        requires(!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_rmw(Type&... args) UWVM_THROWS
    { details::memory64::execute_rmw<Operation, Wide, Bytes, Bounds, Option, ValuePos, AddressPos>(args...); }

    // Wait/notify may suspend: translation spills cached operands first and
    // arranges refill afterward, matching the existing VM-managed wait boundary.
    template<unsigned Operation, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires(Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_memory64_wait_notify(Type... args) UWVM_THROWS
    {
        details::atomic_wait_notify_body_impl<true, Operation, Option>(args...);
        // [consumed wait instruction][successor] ... compiler-owned bytecode
        // [safe                     ][safe     ]
        //                             ^^ IP: complete successor, unchanged below.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), args...[0], sizeof(next));
        UWVM_MUSTTAIL return next(args...);
    }
    template<unsigned Operation, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires(!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_memory64_wait_notify(Type&... args) UWVM_THROWS
    { details::atomic_wait_notify_body_impl<true, Operation, Option>(args...); }

    namespace translate
    {
        template<unsigned Operation, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_memory64_wait_notify_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_memory64_wait_notify<Operation, Option, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_memory64_wait_notify<Operation, Option, Type...>); }
        }
        namespace details::memory64
        {
            template<op_details::atomic_rmw_operation Operation, bool Wide, ::std::size_t Bytes> struct rmw
            {
                struct one
                {
                    using writes_memory = void;
                    template<auto Bounds, auto, uwvm_interpreter_translate_option_t O, ::std::size_t Pos, uwvm_int_stack_top_type... T>
                    inline static constexpr uwvm_interpreter_opfunc_t<T...> fptr() noexcept
                    { return uwvmint_memory64_rmw<Operation, Wide, Bytes, Bounds, O, Pos, Pos, T...>; }
                };
                struct two
                {
                    using writes_memory = void;
                    template<auto Bounds, auto, uwvm_interpreter_translate_option_t O, ::std::size_t V, ::std::size_t A, uwvm_int_stack_top_type... T>
                    inline static constexpr uwvm_interpreter_opfunc_t<T...> fptr() noexcept
                    { return uwvmint_memory64_rmw<Operation, Wide, Bytes, Bounds, O, V, A, T...>; }
                };
                struct address_only
                {
                    using writes_memory = void;
                    template<auto Bounds, auto, uwvm_interpreter_translate_option_t O, ::std::size_t Pos, uwvm_int_stack_top_type... T>
                    inline static constexpr uwvm_interpreter_opfunc_t<T...> fptr() noexcept
                    { return uwvmint_memory64_rmw<Operation, Wide, Bytes, Bounds, O, 0uz, Pos, T...>; }
                };
            };
        }
        template<details::op_details::atomic_rmw_operation Operation, bool Wide, ::std::size_t Bytes,
                 uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_memory64_rmw_fptr_from_tuple(
            uwvm_interpreter_stacktop_currpos_t const& positions, details::op_details::native_memory_t const& memory,
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            namespace op = details::op_details;
            if constexpr(!Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_memory64_rmw<Operation, Wide, Bytes, op::bounds_check_generic, Option, 0uz, 0uz, Type...>); }
            else
            {
                using value_type = ::std::conditional_t<Wide, op::wasm_i64, op::wasm_i32>;
                using operations = details::memory64::rmw<Operation, Wide, Bytes>;
                return details::select_binary_mem_fptr<Option, value_type, op::wasm_i64,
                    op::stacktop_range_begin_pos<Option, value_type>(), op::stacktop_range_end_pos<Option, value_type>(),
                    Option.i64_stack_top_begin_pos, Option.i64_stack_top_end_pos,
                    typename operations::one, typename operations::two, typename operations::address_only, 0u, Type...>(positions, memory);
            }
        }
    }
}
#endif
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <bit>
# include <cstdint>
# include <type_traits>
# include <cstddef>
# include <cstring>
# include <concepts>
# include <memory>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/utils/container/impl.h>
# include "define.h"
# include "memory.h"
# include <uwvm2/runtime/compiler/shared/wasm_threads.h>
# include <uwvm2/runtime/wasm_threads/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    // No memory operand or register-ring rotation. Preserve every cached value
    // across the fence; the translator flushes pending instruction combinations
    // before emitting it. The hardware fence is required by this guest opcode.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_atomic_fence(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0], ::std::byte const*>);
        // [atomic.fence handler] [next handler] ...
        // [safe               ] [safe        ] compiler-owned validated bytecode
        // ^^ type...[0]; every nonterminal handler has a complete successor slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        // [atomic.fence handler] [next handler] ...
        // [safe               ] [safe        ]
        //                       ^^ type...[0]
        ::std::atomic_thread_fence(::std::memory_order_seq_cst);
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_atomic_fence(Type&... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0], ::std::byte const*>);
        // [atomic.fence handler] [next handler] ...
        // [safe               ] [safe        ] compiler-owned validated bytecode
        // ^^ type...[0]; byref dispatch consumes one complete handler slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>);
        // [atomic.fence handler] [next handler] ...
        // [safe               ] [safe        ]
        //                       ^^ type...[0]; outer loop dispatches the successor.
        ::std::atomic_thread_fence(::std::memory_order_seq_cst);
    }


    namespace details
    {
        UWVM_NOINLINE UWVM_GNU_COLD [[noreturn]] inline void atomic_unaligned_terminate() UWVM_THROWS
        {
            auto const callback{::uwvm2::runtime::compiler::uwvm_int::optable::trap_unaligned_atomic_func};
            if(callback != nullptr) { callback(); }
            ::fast_io::fast_terminate();
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_COLD_MACRO inline void trap_unaligned_atomic(Type...) UWVM_THROWS
        { atomic_unaligned_terminate(); }

        template <bool Wide, ::std::size_t Bytes, auto BoundsCheckFn,
                  uwvm_interpreter_translate_option_t CompileOption, ::std::size_t I32Pos, ::std::size_t I64Pos,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void atomic_load(Type... type) UWVM_THROWS
        {
            using result_type = ::std::conditional_t<Wide, wasm_i64, wasm_i32>;
            // [handler][memory pointer][offset][successor] ... compiler-owned stream
            // [safe                                      ]
            // ^^ op_begin borrows the complete instruction proved by translation.
            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
            // [handler][memory pointer][offset][successor]
            //          ^^ IP: the handler slot is complete.
            auto const memory_p{read_imm<native_memory_t*>(type...[0])};
            // [handler][memory pointer][offset][successor]
            //                          ^^ IP: read_imm consumed the pointer slot.
            auto const offset{read_memarg_offset(type...[0])};
            // [handler][memory pointer][offset][successor]
            //                                  ^^ IP: offset decoding consumed its slot.
            // [older operands][i32 address] <- SP if uncached; otherwise ring slot I32Pos.
            auto const address{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, I32Pos>(type...)};
            // [older operands] <- SP when uncached: one validated i32 was popped.
            auto const effective{wasm32_effective_offset(address, offset)};
            if((effective.offset & (Bytes - 1uz)) != 0u) [[unlikely]]
            {
                type...[0] = op_begin;
                // [handler][memory pointer][offset][successor]
                // ^^ IP restored to the proved instruction before the cold tail trap.
                if constexpr(!stacktop_enabled_for<CompileOption, wasm_i32>())
                {
                    type...[1] += sizeof(wasm_i32);
                    // [older operands][i32 address] <- SP restored to its entry position.
                }
                UWVM_MUSTTAIL return trap_unaligned_atomic<CompileOption, Type...>(type...);
            }
            auto const& memory{*memory_p};
            enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == bounds_check_generic)
            {
                if(should_trap_oob_unlocked(memory, effective, Bytes)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    // [handler][memory pointer][offset][successor]
                    // ^^ IP restored for the existing memory-owner diagnostic helper.
                    if constexpr(!stacktop_enabled_for<CompileOption, wasm_i32>())
                    {
                        type...[1] += sizeof(wasm_i32);
                        // [older operands][i32 address] <- SP restored; no value was lost.
                    }
                    UWVM_MUSTTAIL return memop::trap_oob_i32addr<Bytes, CompileOption, I32Pos, Type...>(type...);
                }
            }
            else { BoundsCheckFn(memory, 0uz, offset, effective, Bytes); }
            // [memory reservation ... effective: Bytes ...]
            //                          ^^ bounded/guarded and naturally aligned address.
            auto const pointer{ptr_add_u64(memory.memory_begin, effective.offset)};
            result_type const value{static_cast<result_type>(::uwvm2::runtime::compiler::shared::wasm_threads::atomic_load_le<Bytes>(pointer))};
            exit_memory_operation_memory_lock(memory);
            if constexpr(stacktop_enabled_for<CompileOption, result_type>())
            {
                if constexpr(!Wide || i32_i64_ranges_merged<CompileOption>())
                { set_curr_val_to_stacktop_cache<CompileOption, result_type, I32Pos>(value, type...); }
                else
                {
                    constexpr auto output_pos{ring_prev_pos(I64Pos, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos)};
                    set_curr_val_to_stacktop_cache<CompileOption, result_type, output_pos>(value, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1], ::std::addressof(value), sizeof(value));
                type...[1] += sizeof(value);
                // [older operands][result] <- SP; compiler reserved the result slot.
            }
            uwvm_interpreter_opfunc_t<Type...> next;
            ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
            UWVM_MUSTTAIL return next(type...);
        }
    }

    template <bool Wide, ::std::size_t Bytes, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_atomic_load(Type&... type) UWVM_THROWS
    {
        using result_type = ::std::conditional_t<Wide, details::wasm_i64, details::wasm_i32>;
        // [handler][memory pointer][u32 offset][successor] ... validated stream
        // ^^ IP; consume a complete byref handler slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>);
        // [handler][memory pointer][u32 offset][successor]
        //          ^^ IP: beginning of the validated memory-pointer slot.
        auto const memory_p{details::read_imm<details::native_memory_t*>(type...[0])};
        // [handler][memory pointer][u32 offset][successor]
        //                          ^^ IP: memory-pointer slot consumed.
        auto const offset{details::read_imm<details::wasm_u32>(type...[0])};
        // [handler][memory pointer][u32 offset][successor]
        //                                      ^^ IP: complete successor slot remains.
        auto const address{get_curr_val_from_operand_stack_cache<details::wasm_i32>(type...)};
        // [older operands] <- SP after popping the validated i32 address slot.
        auto const effective{details::wasm32_effective_offset(address, offset)};
        if((effective.offset & (Bytes - 1uz)) != 0u) [[unlikely]] { details::atomic_unaligned_terminate(); }
        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)}; // mmap policy is a compile-time no-op.
        details::check_memory_bounds_unlocked(memory, 0uz, offset, effective, Bytes);
        // [memory reservation ... effective: Bytes ...]
        //                          ^^ bounded/guarded and naturally aligned address.
        auto const pointer{details::ptr_add_u64(memory.memory_begin, effective.offset)};
        result_type const value{static_cast<result_type>(::uwvm2::runtime::compiler::shared::wasm_threads::atomic_load_le<Bytes>(pointer))};
        ::std::memcpy(type...[1], ::std::addressof(value), sizeof(value));
        type...[1] += sizeof(value);
        // [older operands][result] <- SP; caller's frame has a validated result slot.
    }

    namespace details
    {
        template <bool Wide, ::std::size_t Bytes, auto BoundsCheckFn,
                  uwvm_interpreter_translate_option_t CompileOption, ::std::size_t ValuePos, ::std::size_t AddressPos,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void atomic_store(Type... type) UWVM_THROWS
        {
            using value_type = ::std::conditional_t<Wide, wasm_i64, wasm_i32>;
            constexpr bool shared_ring{stacktop_enabled_for<CompileOption, value_type>() && stacktop_enabled_for<CompileOption, wasm_i32>() &&
                (!Wide || i32_i64_ranges_merged<CompileOption>())};
            constexpr auto ring_size{CompileOption.i32_stack_top_end_pos - CompileOption.i32_stack_top_begin_pos};
            // [handler][memory pointer][offset][successor] ... compiler-owned stream
            // [safe                                      ]
            // ^^ op_begin borrows the complete instruction proved by translation.
            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
            // [handler][memory pointer][offset][successor]
            //          ^^ IP: one complete handler slot consumed.
            auto const memory_p{read_imm<native_memory_t*>(type...[0])};
            // [handler][memory pointer][offset][successor]
            //                          ^^ IP: read_imm consumed the complete pointer.
            auto const offset{read_memarg_offset(type...[0])};
            // [handler][memory pointer][offset][successor]
            //                                  ^^ IP: bounded offset slot consumed.
            auto const value{get_curr_val_from_operand_stack_top<CompileOption, value_type, ValuePos>(type...)};
            // [older][address] <- SP if value was uncached; otherwise its ring slot was read.
            wasm_i32 address{};
            if constexpr(shared_ring)
            {
                if constexpr(ring_size >= 2uz)
                {
                    constexpr auto address_pos{ring_next_pos(ValuePos, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos)};
                    address = get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, address_pos>(type...);
                    // SP unchanged: address follows the value in their common ring.
                }
                else
                {
                    address = get_curr_val_from_operand_stack_cache<wasm_i32>(type...);
                    // [older] <- SP: tiny ring spilled this validated i32 address.
                }
            }
            else
            {
                address = get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, AddressPos>(type...);
                // [older] <- SP if address uncached; independent address ring otherwise.
            }
            constexpr auto restore_bytes{(stacktop_enabled_for<CompileOption, value_type>() ? 0uz : sizeof(value_type)) +
                ((!stacktop_enabled_for<CompileOption, wasm_i32>() || (shared_ring && ring_size < 2uz)) ? sizeof(wasm_i32) : 0uz)};
            auto const effective{wasm32_effective_offset(address, offset)};
            if((effective.offset & (Bytes - 1uz)) != 0u) [[unlikely]]
            {
                type...[0] = op_begin;
                // [handler][memory pointer][offset][successor]
                // ^^ IP restored to the validated instruction before the cold trap.
                type...[1] += restore_bytes;
                // [older][uncached inputs] <- SP restored to entry; no memory write occurred.
                UWVM_MUSTTAIL return trap_unaligned_atomic<CompileOption, Type...>(type...);
            }
            auto const& memory{*memory_p};
            enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == bounds_check_generic)
            {
                if(should_trap_oob_unlocked(memory, effective, Bytes)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    // [handler][memory pointer][offset][successor]
                    // ^^ IP restored for the existing memory-owner diagnostic helper.
                    type...[1] += restore_bytes;
                    // [older][uncached inputs] <- SP restored before the cold tail helper.
                    if constexpr(Wide)
                    { UWVM_MUSTTAIL return memop::trap_oob_i64_store<Bytes, CompileOption, ValuePos, AddressPos, Type...>(type...); }
                    else
                    { UWVM_MUSTTAIL return memop::trap_oob_i32_store<Bytes, CompileOption, ValuePos, Type...>(type...); }
                }
            }
            else { BoundsCheckFn(memory, 0uz, offset, effective, Bytes); }
            // [live memory ... effective: Bytes ...]
            //                 ^^ aligned; normal mmap pages use hardware guards.
            // Existing store policy also covers custom page sizes smaller than Bytes.
            auto const pointer{prepare_memory_store_pointer_with_policy<BoundsCheckFn, Bytes>(memory, static_cast<::std::size_t>(effective.offset))};
            ::uwvm2::runtime::compiler::shared::wasm_threads::atomic_store_le<Bytes>(pointer, static_cast<::std::uint64_t>(value));
            exit_memory_operation_memory_lock(memory);
            uwvm_interpreter_opfunc_t<Type...> next;
            ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
            UWVM_MUSTTAIL return next(type...);
        }
    }

    template <bool Wide, ::std::size_t Bytes, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_atomic_store(Type&... type) UWVM_THROWS
    {
        using value_type = ::std::conditional_t<Wide, details::wasm_i64, details::wasm_i32>;
        // [handler][memory pointer][offset][successor] ... validated stream
        // ^^ IP; a complete byref instruction and two typed operands were proved.
        type...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>);
        // [handler][memory pointer][offset][successor]
        //          ^^ IP: complete handler slot consumed.
        auto const memory_p{details::read_imm<details::native_memory_t*>(type...[0])};
        // [handler][memory pointer][offset][successor]
        //                          ^^ IP: complete pointer slot consumed.
        auto const offset{details::read_imm<details::wasm_u32>(type...[0])};
        // [handler][memory pointer][offset][successor]
        //                                  ^^ IP: complete offset slot consumed.
        auto const value{get_curr_val_from_operand_stack_cache<value_type>(type...)};
        // [older][address] <- SP after popping the validated value.
        auto const address{get_curr_val_from_operand_stack_cache<details::wasm_i32>(type...)};
        // [older] <- SP after popping the validated i32 address.
        auto const effective{details::wasm32_effective_offset(address, offset)};
        if((effective.offset & (Bytes - 1uz)) != 0u) [[unlikely]] { details::atomic_unaligned_terminate(); }
        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)}; // mmap policy is a compile-time no-op.
        details::check_memory_bounds_unlocked(memory, 0uz, offset, effective, Bytes);
        // [live memory ... effective: Bytes ...]
        //                 ^^ full store range checked/preflighted before any write.
        auto const pointer{details::prepare_memory_store_pointer_with_policy<details::bounds_check_generic, Bytes>(memory, static_cast<::std::size_t>(effective.offset))};
        ::uwvm2::runtime::compiler::shared::wasm_threads::atomic_store_le<Bytes>(pointer, static_cast<::std::uint64_t>(value));
    }

    namespace details
    {
        using atomic_rmw_operation = ::uwvm2::runtime::compiler::shared::wasm_threads::atomic_rmw_operation;
        template <bool Wide, bool Compare, uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos, bool Address64 = false>
        struct atomic_rmw_ring_layout
        {
            using value_type = ::std::conditional_t<Wide, wasm_i64, wasm_i32>;
            using address_type = ::std::conditional_t<Address64, wasm_i64, wasm_i32>;
            static constexpr auto value_begin{Wide ? Option.i64_stack_top_begin_pos : Option.i32_stack_top_begin_pos};
            static constexpr auto value_end{Wide ? Option.i64_stack_top_end_pos : Option.i32_stack_top_end_pos};
            static constexpr auto value_slots{value_end - value_begin};
            static constexpr bool shared_ring{value_slots != 0uz && stacktop_enabled_for<Option, address_type>() && (Wide == Address64 || i32_i64_ranges_merged<Option>())};
            static constexpr ::std::size_t value_count{Compare ? 2uz : 1uz};
            static constexpr bool address_cached{stacktop_enabled_for<Option, address_type>() && (!shared_ring || value_slots > value_count)};
            static constexpr auto popped_bytes{(value_count - (value_slots < value_count ? value_slots : value_count)) * sizeof(value_type) +
                (address_cached ? 0uz : sizeof(address_type))};
            static constexpr auto advance(::std::size_t depth) noexcept
            {
                if constexpr(value_slots != 0uz) { return value_begin + (ValuePos - value_begin + depth) % value_slots; }
                else { return 0uz; }
            }
            static constexpr auto output_pos{advance(shared_ring ? value_count : value_count - 1uz)};
        };

        template <bool Address64, bool Wide, bool Compare, uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos,
                  uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr auto pop_atomic_rmw_inputs_impl(Type&... type) UWVM_THROWS
        {
            using layout = atomic_rmw_ring_layout<Wide, Compare, Option, ValuePos, AddressPos, Address64>;
            using value_type = typename layout::value_type;
            using address_type = typename layout::address_type;
            struct inputs { value_type value{}; value_type expected{}; address_type address{}; } result{};
            result.value = get_curr_val_from_operand_stack_top<Option, value_type, ValuePos>(type...);
            // [older][address][expected?] <- SP if the replacement/value was uncached.
            if constexpr(Compare)
            {
                if constexpr(layout::value_slots >= 2uz)
                { result.expected = get_curr_val_from_operand_stack_top<Option, value_type, layout::advance(1uz)>(type...); }
                // SP unchanged above: expected is the next slot of the value ring.
                else { result.expected = get_curr_val_from_operand_stack_cache<value_type>(type...); }
                // [older][address] <- SP if expected was spilled/uncached.
            }
            if constexpr(layout::shared_ring)
            {
                if constexpr(layout::address_cached)
                { result.address = get_curr_val_from_operand_stack_top<Option, address_type, layout::advance(layout::value_count)>(type...); }
                // SP unchanged above: address is in the shared ring after all value inputs.
                else { result.address = get_curr_val_from_operand_stack_cache<address_type>(type...); }
                // [older] <- SP if the shared ring was too small for this address.
            }
            else
            {
                result.address = get_curr_val_from_operand_stack_top<Option, address_type, AddressPos>(type...);
                // [older] <- SP if uncached; otherwise the independent address ring was read.
            }
            return result;
        }

        // Keep the existing memory32 entry point and its compiler-owned u32
        // immediate layout; only the register layout machinery is shared.
        template <bool Wide, bool Compare, uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos,
                  uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr auto pop_atomic_rmw_inputs(Type&... type) UWVM_THROWS
        { return pop_atomic_rmw_inputs_impl<false, Wide, Compare, Option, ValuePos, AddressPos>(type...); }

        template <bool Wide, bool Compare, ::std::size_t Bytes, uwvm_interpreter_translate_option_t Option,
                  ::std::size_t ValuePos, ::std::size_t AddressPos, uwvm_int_stack_top_type... Type>
            requires (Option.is_tail_call)
        UWVM_NOINLINE UWVM_INTERPRETER_OPFUNC_COLD_MACRO inline constexpr void trap_atomic_rmw_oob(Type... type) UWVM_THROWS
        {
            // [handler][memory pointer][offset][successor] proved by translation;
            // entry IP/SP were restored by the hot handler before this cold tail.
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
            // [handler][memory pointer][offset][successor]
            //          ^^ IP: complete handler consumed.
            auto const memory{read_imm<native_memory_t*>(type...[0])};
            // [handler][memory pointer][offset][successor]
            //                          ^^ IP: complete pointer consumed.
            auto const offset{read_memarg_offset(type...[0])};
            // [handler][memory pointer][offset][successor]
            //                                  ^^ IP: complete offset consumed.
            auto const inputs{pop_atomic_rmw_inputs<Wide, Compare, Option, ValuePos, AddressPos>(type...)};
            // [older] <- SP after replaying precisely the validated input pops.
            auto const effective{wasm32_effective_offset(inputs.address, offset)};
            memory_oob_terminate(*memory, offset, effective, load_memory_length_for_oob_unlocked(*memory), Bytes);
        }

        template <atomic_rmw_operation Operation, bool Wide, ::std::size_t Bytes, auto BoundsCheckFn,
                  uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos, uwvm_int_stack_top_type... Type>
            requires (Option.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void atomic_rmw(Type... type) UWVM_THROWS
        {
            constexpr bool compare{Operation == atomic_rmw_operation::compare_exchange};
            using layout = atomic_rmw_ring_layout<Wide, compare, Option, ValuePos, AddressPos>;
            using value_type = typename layout::value_type;
            // [handler][memory pointer][offset][successor] ... validated compiler stream
            // ^^ op_begin borrows the complete instruction; all typed inputs exist.
            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
            // [handler][memory pointer][offset][successor]
            //          ^^ IP: one complete handler slot consumed.
            auto const memory{read_imm<native_memory_t*>(type...[0])};
            // [handler][memory pointer][offset][successor]
            //                          ^^ IP: one complete pointer slot consumed.
            auto const offset{read_memarg_offset(type...[0])};
            // [handler][memory pointer][offset][successor]
            //                                  ^^ IP: one complete offset slot consumed.
            auto const inputs{pop_atomic_rmw_inputs<Wide, compare, Option, ValuePos, AddressPos>(type...)};
            // [older] <- SP: layout::popped_bytes consumed; cached inputs unchanged.
            auto const effective{wasm32_effective_offset(inputs.address, offset)};
            if((effective.offset & (Bytes - 1uz)) != 0u) [[unlikely]]
            {
                type...[0] = op_begin;
                // [handler][memory pointer][offset][successor]
                // ^^ IP restored to the complete validated instruction.
                type...[1] += layout::popped_bytes;
                // [older][uncached inputs] <- SP restored before the cold tail trap.
                UWVM_MUSTTAIL return trap_unaligned_atomic<Option, Type...>(type...);
            }
            enter_memory_operation_memory_lock(*memory);
            if constexpr(BoundsCheckFn == bounds_check_generic)
            {
                if(should_trap_oob_unlocked(*memory, effective, Bytes)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    // [handler][memory pointer][offset][successor]
                    // ^^ IP restored to the complete validated instruction.
                    type...[1] += layout::popped_bytes;
                    // [older][uncached inputs] <- SP restored for the diagnostic replay.
                    UWVM_MUSTTAIL return trap_atomic_rmw_oob<Wide, compare, Bytes, Option, ValuePos, AddressPos, Type...>(type...);
                }
            }
            else { BoundsCheckFn(*memory, 0uz, offset, effective, Bytes); }
            // [live memory ... effective: Bytes ...] full range/alignment proved;
            // native mmap guards or software bounds prevent an out-of-range write.
            auto const pointer{prepare_memory_store_pointer_with_policy<BoundsCheckFn, Bytes>(*memory, static_cast<::std::size_t>(effective.offset))};
            auto const old{static_cast<value_type>(::uwvm2::runtime::compiler::shared::wasm_threads::atomic_rmw_le<Operation, Bytes>(
                pointer, static_cast<::std::uint64_t>(inputs.value), static_cast<::std::uint64_t>(inputs.expected)))};
            exit_memory_operation_memory_lock(*memory);
            if constexpr(layout::value_slots != 0uz)
            { set_curr_val_to_stacktop_cache<Option, value_type, layout::output_pos>(old, type...); }
            else
            {
                ::std::memcpy(type...[1], ::std::addressof(old), sizeof(old));
                type...[1] += sizeof(old);
                // [older][old value] <- SP: compiler reserved this typed result slot.
            }
            uwvm_interpreter_opfunc_t<Type...> next;
            ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
            UWVM_MUSTTAIL return next(type...);
        }
    }

    template <details::atomic_rmw_operation Operation, bool Wide, ::std::size_t Bytes,
              uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_atomic_rmw(Type&... type) UWVM_THROWS
    {
        using value_type = ::std::conditional_t<Wide, details::wasm_i64, details::wasm_i32>;
        // [handler][memory pointer][offset][successor] ... complete validated stream
        // ^^ IP; compiler proved all typed input and output slots.
        type...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>);
        // [handler][memory pointer][offset][successor]
        //          ^^ IP: complete handler slot consumed.
        auto const memory{details::read_imm<details::native_memory_t*>(type...[0])};
        // [handler][memory pointer][offset][successor]
        //                          ^^ IP: complete pointer slot consumed.
        auto const offset{details::read_imm<details::wasm_u32>(type...[0])};
        // [handler][memory pointer][offset][successor]
        //                                  ^^ IP: complete offset slot consumed.
        auto const value{get_curr_val_from_operand_stack_cache<value_type>(type...)};
        // [older][address][expected?] <- SP after popping the validated value.
        value_type expected{};
        if constexpr(Operation == details::atomic_rmw_operation::compare_exchange)
        { expected = get_curr_val_from_operand_stack_cache<value_type>(type...); }
        // [older][address] <- SP after the optional validated expected-value pop.
        auto const address{get_curr_val_from_operand_stack_cache<details::wasm_i32>(type...)};
        // [older] <- SP after popping the validated address.
        auto const effective{details::wasm32_effective_offset(address, offset)};
        if((effective.offset & (Bytes - 1uz)) != 0u) [[unlikely]] { details::atomic_unaligned_terminate(); }
        [[maybe_unused]] auto guard{details::lock_memory(*memory)}; // mmap policy is a compile-time no-op.
        details::check_memory_bounds_unlocked(*memory, 0uz, offset, effective, Bytes);
        // [live memory ... effective: Bytes ...] bounded/guarded and naturally aligned.
        auto const pointer{details::prepare_memory_store_pointer_with_policy<details::bounds_check_generic, Bytes>(*memory, static_cast<::std::size_t>(effective.offset))};
        auto const old{static_cast<value_type>(::uwvm2::runtime::compiler::shared::wasm_threads::atomic_rmw_le<Operation, Bytes>(
            pointer, static_cast<::std::uint64_t>(value), static_cast<::std::uint64_t>(expected)))};
        ::std::memcpy(type...[1], ::std::addressof(old), sizeof(old));
        type...[1] += sizeof(old);
        // [older][old value] <- SP: compiler proved the typed result slot.
    }

    namespace details
    {
        // The translator spills the register ring before this blocking operation.
        // No ring position or borrowed native-memory pointer survives a suspension
        // outside the VM host-entry lease. Refill handlers follow this instruction.
        template<bool Address64, unsigned Operation, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline void atomic_wait_notify_body_impl(Type&... type) UWVM_THROWS
        {
            static_assert(Operation <= 2u);
            // [handler][native owner pointer][address-sized offset][successor] compiler-owned
            // [safe                                               ]
            // ^^ entry_ip; SP names the end of the proved, fully spilled operands.
            auto const entry_ip{type...[0]};
            auto const entry_sp{type...[1]};
            // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
            //           [complete pointer slot ] safe to its end; unsafe past stream_end
            // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
            if constexpr(Option.is_tail_call) { type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>); }
            else { type...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); }
            // [handler][native owner pointer][address-sized offset][successor]
            // [safe                                               ]
            //          ^^ IP after the complete handler slot.
            auto const memory{read_imm<native_memory_t*>(type...[0])};
            // [handler][native owner pointer][address-sized offset][successor]
            // [safe                                               ]
            //                                ^^ IP after checked owner slot.
            auto const offset{[&]() constexpr noexcept
            {
                if constexpr(Address64) { return read_imm<::std::uint64_t>(type...[0]); }
                else { return read_memarg_offset(type...[0]); }
            }()};
            // [handler][native owner pointer][address-sized offset][successor]
            // [safe                                               ]
            //                                            ^^ IP, complete next slot.
            auto pop{[&]<typename Value>() noexcept
            {
                type...[1] -= sizeof(Value);
                // [older operands][Value] end (old SP)
                // [safe                  ]
                //                  ^^ SP; typed validation proved this operand exists.
                Value value;
                ::std::memcpy(::std::addressof(value), type...[1], sizeof(value));
                return value;
            }};
            ::std::int64_t timeout{};
            ::std::uint64_t expected{};
            if constexpr(Operation != 0u)
            {
                timeout = pop.template operator()<wasm_i64>();
                if constexpr(Operation == 2u) { expected = static_cast<::std::uint64_t>(pop.template operator()<wasm_i64>()); }
                else { expected = static_cast<::std::uint32_t>(pop.template operator()<wasm_i32>()); }
            }
            else { expected = static_cast<::std::uint32_t>(pop.template operator()<wasm_i32>()); }
            using address_type = ::std::conditional_t<Address64, wasm_i64, wasm_i32>;
            auto const address{pop.template operator()<address_type>()};
            // All guest inputs are now scalars; no movable linear-memory address
            // is retained while waiting. Preserve the complete address sum for
            // diagnostics; memory64 checks its carry before consulting a wait key.
            auto const effective{[&]() constexpr noexcept
            {
                if constexpr(Address64) { return wasm64_effective_offset(address, offset); }
                else { return wasm32_effective_offset(address, offset); }
            }()};
            namespace waiting = ::uwvm2::runtime::wasm_threads;
            auto const result{[&]
            {
                if constexpr(Address64) { return waiting::memory_wait_notify64<Operation>(*memory, offset,
                    static_cast<::std::uint64_t>(address), expected, timeout); }
                else if constexpr(Operation == 0u) { return waiting::memory_notify(*memory, static_cast<::std::uint64_t>(static_cast<::std::uint32_t>(address)) + offset, static_cast<::std::uint32_t>(expected)); }
                else { return waiting::memory_wait<Operation == 1u ? 4uz : 8uz>(*memory, waiting::is_shared(*memory), static_cast<::std::uint64_t>(static_cast<::std::uint32_t>(address)) + offset, expected, timeout); }
            }()};
            if(result.status > waiting::wait_status::timed_out) [[unlikely]]
            {
                type...[0] = entry_ip;
                // [handler][owner][offset][successor] all safe; IP restores entry.
                type...[1] = entry_sp;
                // [older operands][validated arguments] end; SP restores entry.
                if(result.status == waiting::wait_status::unaligned) { atomic_unaligned_terminate(); }
                if(result.status == waiting::wait_status::out_of_bounds)
                { memory_oob_terminate(*memory, offset, effective, result.memory_length, Operation == 2u ? 8uz : 4uz); }
                auto const callback{trap_atomic_wait_func};
                if(callback != nullptr) { callback(static_cast<unsigned>(result.status)); }
                ::fast_io::fast_terminate();
            }
            auto const value{Operation == 0u ? result.notified : static_cast<::std::uint_least32_t>(result.status)};
            wasm_i32 const guest_value{static_cast<wasm_i32>(value)};
            ::std::memcpy(type...[1], ::std::addressof(guest_value), sizeof(guest_value));
            type...[1] += sizeof(guest_value);
            // [older operands][i32 result] end
            // [safe                      ]
            //                              ^^ SP, translator reserved this slot.
        }
        template<unsigned Operation, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline void atomic_wait_notify_body(Type&... type) UWVM_THROWS
        { atomic_wait_notify_body_impl<false, Operation, Option>(type...); }

    }

    template<unsigned Operation, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires(Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_atomic_wait_notify(Type... type) UWVM_THROWS
    {
        details::atomic_wait_notify_body<Operation, Option>(type...);
        // [successor handler] ... [safe]; body consumed the complete instruction.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }

    template<unsigned Operation, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires(!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_atomic_wait_notify(Type&... type) UWVM_THROWS
    { details::atomic_wait_notify_body<Operation, Option>(type...); }

    namespace translate
    {
        namespace details
        {
            template <op_details::atomic_rmw_operation Operation, bool Wide, ::std::size_t Bytes> struct atomic_rmw_op
            {
                using writes_memory = void;
                template <auto Bounds, auto, uwvm_interpreter_translate_option_t Option, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::atomic_rmw<Operation, Wide, Bytes, Bounds, Option, Pos, Pos, Type...>; }
            };
            template <op_details::atomic_rmw_operation Operation, ::std::size_t Bytes> struct atomic_rmw_op_2d
            {
                using writes_memory = void;
                template <auto Bounds, auto, uwvm_interpreter_translate_option_t Option, ::std::size_t Value, ::std::size_t Address, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::atomic_rmw<Operation, true, Bytes, Bounds, Option, Value, Address, Type...>; }
            };
            template <op_details::atomic_rmw_operation Operation, ::std::size_t Bytes> struct atomic_rmw_op_address
            {
                using writes_memory = void;
                template <auto Bounds, auto, uwvm_interpreter_translate_option_t Option, ::std::size_t Address, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::atomic_rmw<Operation, true, Bytes, Bounds, Option, 0uz, Address, Type...>; }
            };
        }
        template <details::op_details::atomic_rmw_operation Operation, bool Wide, ::std::size_t Bytes,
                  uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_atomic_rmw_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& position,
            details::op_details::native_memory_t const& memory, ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(!Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_atomic_rmw<Operation, Wide, Bytes, Option, Type...>); }
            else if constexpr(Wide)
            {
                return details::select_binary_mem_fptr<Option, details::op_details::wasm_i64, details::op_details::wasm_i32,
                    Option.i64_stack_top_begin_pos, Option.i64_stack_top_end_pos, Option.i32_stack_top_begin_pos, Option.i32_stack_top_end_pos,
                    details::atomic_rmw_op<Operation, true, Bytes>, details::atomic_rmw_op_2d<Operation, Bytes>,
                    details::atomic_rmw_op_address<Operation, Bytes>, 0u, Type...>(position, memory);
            }
            else
            {
                return details::select_mem_fptr_or_default<Option, Option.i32_stack_top_begin_pos, Option.i32_stack_top_end_pos,
                    details::atomic_rmw_op<Operation, false, Bytes>, 0u, Type...>(position.i32_stack_top_curr_pos, memory);
            }
        }
    }

    namespace translate
    {
        namespace details
        {
            template <bool Wide, ::std::size_t Bytes> struct atomic_store_op
            {
                using writes_memory = void;
                template <auto Bounds, auto, uwvm_interpreter_translate_option_t Option, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::atomic_store<Wide, Bytes, Bounds, Option, Pos, Pos, Type...>; }
            };
            template <::std::size_t Bytes> struct atomic_store_op_2d
            {
                using writes_memory = void;
                template <auto Bounds, auto, uwvm_interpreter_translate_option_t Option, ::std::size_t Value, ::std::size_t Address, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::atomic_store<true, Bytes, Bounds, Option, Value, Address, Type...>; }
            };
            template <::std::size_t Bytes> struct atomic_store_op_address
            {
                using writes_memory = void;
                template <auto Bounds, auto, uwvm_interpreter_translate_option_t Option, ::std::size_t Address, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::atomic_store<true, Bytes, Bounds, Option, 0uz, Address, Type...>; }
            };
        }
        template <bool Wide, ::std::size_t Bytes, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_atomic_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& position,
            details::op_details::native_memory_t const& memory, ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(!Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_atomic_store<Wide, Bytes, Option, Type...>); }
            else if constexpr(Wide)
            {
                return details::select_binary_mem_fptr<Option, details::op_details::wasm_i64, details::op_details::wasm_i32,
                    Option.i64_stack_top_begin_pos, Option.i64_stack_top_end_pos,
                    Option.i32_stack_top_begin_pos, Option.i32_stack_top_end_pos,
                    details::atomic_store_op<true, Bytes>, details::atomic_store_op_2d<Bytes>, details::atomic_store_op_address<Bytes>, 0u, Type...>(position, memory);
            }
            else
            {
                return details::select_mem_fptr_or_default<Option, Option.i32_stack_top_begin_pos, Option.i32_stack_top_end_pos,
                    details::atomic_store_op<false, Bytes>, 0u, Type...>(position.i32_stack_top_curr_pos, memory);
            }
        }
    }

    namespace translate
    {

        namespace details
        {
            template <bool Wide, ::std::size_t Bytes> struct atomic_load_op
            {
                template <auto Bounds, auto, uwvm_interpreter_translate_option_t Option, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::atomic_load<Wide, Bytes, Bounds, Option, Pos, Pos, Type...>; }
            };
            template <::std::size_t Bytes> struct atomic_load_op_2d
            {
                template <auto Bounds, auto, uwvm_interpreter_translate_option_t Option, ::std::size_t Out, ::std::size_t In, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::atomic_load<true, Bytes, Bounds, Option, In, Out, Type...>; }
            };
            template <::std::size_t Bytes> struct atomic_load_op_out
            {
                template <auto Bounds, auto, uwvm_interpreter_translate_option_t Option, ::std::size_t Out, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::atomic_load<true, Bytes, Bounds, Option, 0uz, Out, Type...>; }
            };
        }
        template <bool Wide, ::std::size_t Bytes, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_atomic_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& position,
            details::op_details::native_memory_t const& memory, ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(!Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_atomic_load<Wide, Bytes, Option, Type...>); }
            else if constexpr(Wide)
            {
                return details::select_unary_mem_fptr<Option, details::op_details::wasm_i32, details::op_details::wasm_i64,
                    Option.i32_stack_top_begin_pos, Option.i32_stack_top_end_pos,
                    Option.i64_stack_top_begin_pos, Option.i64_stack_top_end_pos,
                    details::atomic_load_op<true, Bytes>, details::atomic_load_op_2d<Bytes>, details::atomic_load_op_out<Bytes>, 0uz, Type...>(position, memory);
            }
            else
            {
                return details::select_mem_fptr_or_default<Option, Option.i32_stack_top_begin_pos, Option.i32_stack_top_end_pos,
                    details::atomic_load_op<false, Bytes>, 0uz, Type...>(position.i32_stack_top_curr_pos, memory);
            }
        }
        template <unsigned Operation, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_atomic_wait_notify_fptr_from_tuple(::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_atomic_wait_notify<Operation, Option, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_atomic_wait_notify<Operation, Option, Type...>); }
        }
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_atomic_fence_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const&,
                                                                     ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(CompileOption.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_atomic_fence<CompileOption, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_atomic_fence<CompileOption, Type...>); }
        }
    }
}
#endif
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

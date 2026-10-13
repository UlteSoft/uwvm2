/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <bit>
# include <concepts>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <memory>
# include <type_traits>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/runtime/compiler/shared/wasm_memory64.h>
# include <uwvm2/runtime/compiler/shared/wasm_threads.h>
# include "define.h"
# include "register_ring.h"
# include "memory.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    namespace details::memory64
    {
        UWVM_NOINLINE UWVM_GNU_COLD [[noreturn]] inline void unaligned_atomic() UWVM_THROWS
        {
            auto const callback{trap_unaligned_atomic_func};
            if(callback != nullptr) { callback(); }
            ::fast_io::fast_terminate();
        }

#if defined(UWVM_SUPPORT_MMAP)
        // A distinct function identity carries the translation-time page proof.
        // Bounds still use the complete memory64 partial-reservation/carry check.
        UWVM_ALWAYS_INLINE inline constexpr void bounds_check_mmap_path_standard_page(native_memory_t const& memory,
            ::std::size_t index, ::std::uint_least64_t offset, memory_offset_t effective, ::std::size_t width) noexcept
        { bounds_check_mmap_path(memory, index, offset, effective, width); }
#endif

        template<auto Bounds, ::std::size_t Bytes>
        UWVM_ALWAYS_INLINE inline constexpr ::std::byte* prepare_store(native_memory_t const& memory, ::std::uint_least64_t offset) noexcept
        {
#if defined(UWVM_SUPPORT_MMAP)
            if constexpr(Bounds == bounds_check_mmap_path_standard_page)
            {
                static_assert(Bytes >= 1uz && Bytes <= 16uz);
                // Translation selected a stable mmap base, standard 64 KiB pages
                // and hardware protection. The preceding partial-reservation or
                // native-length proof includes the entire access plus guard tail.
                // [committed pages][protected suffix] | outside reservation
                //          ^^ offset: checked before pointer formation.
                if constexpr(Bytes > 1uz)
                {
                    if((offset & 65535u) > 65536u - Bytes) [[unlikely]]
                    {
                        // [first byte ... last byte] within the proven reservation
                        //                 ^^ last; a fault precedes every store.
                        auto const last{ptr_add_u64(memory.memory_begin, offset + Bytes - 1uz)};
                        auto const probe{*static_cast<::std::byte const volatile*>(last)};
                        static_cast<void>(probe);
                        ::std::atomic_signal_fence(::std::memory_order_seq_cst);
                    }
                }
                // [complete checked/probed store span] reservation end
                //  ^^ result: the prefix cannot be modified before the last-byte proof.
                return ptr_add_u64(memory.memory_begin, offset);
            }
            else
#endif
            { return prepare_memory_store_pointer_with_policy<Bounds, Bytes>(memory, offset); }
        }

        // The offset is a native u64 slot in compiler-owned bytecode, not a LEB.
        // Memory32 keeps its separate four-byte slot and all existing fusions.
        template <typename Value, ::std::size_t Bytes, bool Signed>
        UWVM_ALWAYS_INLINE inline constexpr Value load_integer(::std::byte const* pointer) noexcept
        {
            static_assert(::std::same_as<Value, wasm_i32> || ::std::same_as<Value, wasm_i64>);
            static_assert(Bytes == 1uz || Bytes == 2uz || Bytes == 4uz || Bytes == 8uz);
            static_assert(Bytes <= sizeof(Value));
            ::std::uint_least64_t raw{};
            if constexpr(Bytes == 1uz) { raw = load_u8(pointer); }
            else if constexpr(Bytes == 2uz)
            {
                ::std::uint_least16_t word{};
                ::std::memcpy(::std::addressof(word), pointer, sizeof(word));
                raw = ::fast_io::little_endian(word);
            }
            else if constexpr(Bytes == 4uz) { raw = static_cast<::std::uint_least32_t>(load_i32_le(pointer)); }
            else { raw = static_cast<::std::uint_least64_t>(load_i64_le(pointer)); }
            if constexpr(Signed && Bytes < sizeof(Value))
            {
                // Interpret the narrow two's-complement field, then widen its
                // signed value. This exposes a single sign-extending load to the
                // compiler without signed shifts, overflow, or a conditional mask.
                using unsigned_field = ::std::conditional_t<Bytes == 1uz, ::std::uint_least8_t,
                    ::std::conditional_t<Bytes == 2uz, ::std::uint_least16_t, ::std::uint_least32_t>>;
                using signed_field = ::std::make_signed_t<unsigned_field>;
                return static_cast<Value>(::std::bit_cast<signed_field>(static_cast<unsigned_field>(raw)));
            }
            if constexpr(sizeof(Value) == 4uz)
            { return ::std::bit_cast<Value>(static_cast<::std::uint_least32_t>(raw)); }
            else { return ::std::bit_cast<Value>(raw); }
        }

        template <::std::size_t Bytes, typename Value>
        UWVM_ALWAYS_INLINE inline constexpr void store_integer(::std::byte* pointer, Value value) noexcept
        {
            static_assert(Bytes == 1uz || Bytes == 2uz || Bytes == 4uz || Bytes == 8uz);
            static_assert(Bytes <= sizeof(Value));
            using unsigned_type = ::std::conditional_t<Bytes == 1uz, ::std::uint_least8_t,
                ::std::conditional_t<Bytes == 2uz, ::std::uint_least16_t,
                ::std::conditional_t<Bytes == 4uz, ::std::uint_least32_t, ::std::uint_least64_t>>>;
            auto const bits{::fast_io::little_endian(static_cast<unsigned_type>(value))};
            ::std::memcpy(pointer, ::std::addressof(bits), Bytes);
        }

        template <typename Value, ::std::size_t Bytes, bool Signed, bool Store, bool Atomic, auto BoundsCheckFn,
                  uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos,
                  uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr void execute(Type&... args) UWVM_THROWS
        {
            constexpr bool integer{::std::same_as<Value, wasm_i32> || ::std::same_as<Value, wasm_i64>};
            constexpr bool float32{::std::same_as<Value, wasm_f32>};
            constexpr bool float64{::std::same_as<Value, wasm_f64>};
            constexpr bool vector{::std::same_as<Value, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>};
            static_assert(integer || float32 || float64 || vector);
            static_assert(!Atomic || (integer && !Signed));
            static_assert(integer || (!Signed && Bytes == sizeof(Value)));
            static_assert(::std::same_as<Type...[0], ::std::byte const*> && ::std::same_as<Type...[1], ::std::byte*>);
            constexpr auto value_begin{stacktop_range_begin_pos<Option, Value>()};
            constexpr auto value_end{stacktop_range_end_pos<Option, Value>()};
            constexpr auto address_begin{Option.i64_stack_top_begin_pos};
            constexpr auto address_end{Option.i64_stack_top_end_pos};
            constexpr bool merged{value_begin != value_end && value_begin == address_begin && value_end == address_end};
            // [handler][native memory pointer][u64 offset][successor handler] ...
            // [safe                                                       ] owned bytecode
            // ^^ IP; translation proved every immediate and successor slot.
            if constexpr(Option.is_tail_call) { args...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>); }
            else { args...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); }
            // [handler][native memory pointer][u64 offset][successor handler]
            //          ^^ IP after consuming the complete dispatch slot.
            auto const memory{read_imm<native_memory_t*>(args...[0])};
            // [handler][native memory pointer][u64 offset][successor handler]
            //                                 ^^ IP; read_imm copied its complete pointer slot.
            auto const offset{read_imm<::std::uint_least64_t>(args...[0])};
            // [handler][native memory pointer][u64 offset][successor handler]
            //                                             ^^ IP after the complete u64 immediate.
            Value value{};
            if constexpr(Store)
            {
                // [older values][i64 address][Value] <- SP for uncached values;
                // ValuePos supplies the top value when its ring is enabled.
                if constexpr((float32 || float64) && value_begin == value_end)
                {
                    // [older values][i64 address][Value] <- SP
                    // [safe                           ] validated operand allocation
                    args...[1] -= sizeof(Value);
                    // [older values][i64 address][Value]
                    //                            ^^ SP at the complete value slot.
                    // Copy bits directly: a native FP return may quiet an sNaN.
                    ::std::memcpy(::std::addressof(value), args...[1], sizeof(Value));
                }
                else { value = get_curr_val_from_operand_stack_top<Option, Value, ValuePos>(args...); }
                // [older values][i64 address] <- SP if the value was uncached.
            }
            wasm_i64 address{};
            if constexpr(Store && merged)
            {
                static_assert(ValuePos == AddressPos);
                if constexpr(address_end - address_begin >= 2uz)
                {
                    constexpr auto position{ring_next_pos(AddressPos, address_begin, address_end)};
                    address = get_curr_val_from_operand_stack_top<Option, wasm_i64, position>(args...);
                }
                else
                {
                    // [older values][i64 address] <- SP; a one-slot shared ring
                    // held only the store value, so translation spilled the address.
                    address = get_curr_val_from_operand_stack_cache<wasm_i64>(args...);
                    // [older values] <- SP, after consuming the proved eight bytes.
                }
            }
            else
            {
                // [older values][i64 address] <- SP, or AddressPos in the i64 ring.
                address = get_curr_val_from_operand_stack_top<Option, wasm_i64, AddressPos>(args...);
                // [older values] <- SP if the address was uncached.
            }
            auto const effective{wasm64_effective_offset(address, offset)};
            if constexpr(Atomic)
            {
                // Atomic memarg alignment is only a declaration. The effective
                // i64+u64 address must itself be naturally aligned at execution.
                if((effective.offset & (Bytes - 1uz)) != 0u) [[unlikely]] { unaligned_atomic(); }
            }
            // mmap bases never move: these enter/exit operations compile away.
            // Moving allocator memory is pinned through the entire checked access.
            enter_memory_operation_memory_lock(*memory);
            BoundsCheckFn(*memory, 0uz, offset, effective, Bytes);
            // [committed/guarded reservation] | outside host object
            //          ^^ effective: the complete 65-bit sum passed the selected
            // native bounds proof, including on a host whose size_t is 32 bits.
            if constexpr(Store)
            {
                auto const pointer{[&]() constexpr noexcept
                {
                    if constexpr(Atomic)
                    {
                        // [naturally aligned Bytes] lies in one protection page,
                        // or the software bounds policy proved the complete range.
                        // ^^ result: no redundant cross-page probe for atomic stores.
                        return ptr_add_u64(memory->memory_begin, effective.offset);
                    }
                    else { return prepare_store<BoundsCheckFn, Bytes>(*memory, effective.offset); }
                }()};
                // [complete proved store span] protected partial-page stores probe
                // [safe                      ] their last byte before the first write.
                if constexpr(Atomic)
                { ::uwvm2::runtime::compiler::shared::wasm_threads::atomic_store_le<Bytes>(pointer, static_cast<::std::uint64_t>(value)); }
                else if constexpr(integer) { store_integer<Bytes>(pointer, value); }
                else if constexpr(float32) { store_f32_le(pointer, value); }
                else if constexpr(float64) { store_f64_le(pointer, value); }
                else { ::std::memcpy(pointer, ::std::addressof(value), sizeof(value)); }
            }
            else
            {
                auto const pointer{ptr_add_u64(memory->memory_begin, effective.offset)};
                // [complete proved load span] mmap may fault within its reservation;
                // [safe                     ] no host pointer was formed before checking.
                if constexpr(Atomic)
                { value = static_cast<Value>(::uwvm2::runtime::compiler::shared::wasm_threads::atomic_load_le<Bytes>(pointer)); }
                else if constexpr(integer) { value = load_integer<Value, Bytes, Signed>(pointer); }
                else if constexpr(float32) { load_f32_le(pointer, value); }
                else if constexpr(float64) { load_f64_le(pointer, value); }
                else { ::std::memcpy(::std::addressof(value), pointer, sizeof(value)); }
            }
            exit_memory_operation_memory_lock(*memory);
            if constexpr(!Store)
            {
                if constexpr(value_begin != value_end)
                {
                    if constexpr(merged)
                    {
                        static_assert(ValuePos == AddressPos);
                        set_curr_val_to_stacktop_cache<Option, Value, ValuePos>(value, args...);
                    }
                    else
                    {
                        constexpr auto position{ring_prev_pos(ValuePos, value_begin, value_end)};
                        set_curr_val_to_stacktop_cache<Option, Value, position>(value, args...);
                    }
                }
                else
                {
                    // [older values][reserved result space] ... operand allocation
                    //                ^^ SP; translator reserved the output width.
                    ::std::memcpy(args...[1], ::std::addressof(value), sizeof(value));
                    args...[1] += sizeof(value);
                    // [older values][Value] <- SP, still within the operand allocation.
                }
            }
        }
        // Size bytecode: [handler][memory*][successor]. Grow additionally owns a
        // native maximum-byte slot between memory* and successor. This native
        // resource limit is distinct from the declared u64 Wasm page limit.
        template<bool Grow, uwvm_interpreter_translate_option_t Option, ::std::size_t AddressPos,
                 uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr void execute_pages(Type&... args) UWVM_THROWS
        {
            // [handler][memory*][maximum bytes?][successor] ... owned bytecode
            // [safe                                       ]
            // ^^ IP; translator reserved every slot including the next dispatch.
            if constexpr(Option.is_tail_call) { args...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>); }
            else { args...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); }
            // [handler][memory*][maximum bytes?][successor]
            //          ^^ IP at the complete pointer immediate.
            auto const memory{read_imm<native_memory_t*>(args...[0])};
            // [handler][memory*][maximum bytes?][successor]
            //                   ^^ IP after bounded owned-pointer decoding.
            wasm_i64 result{};
            if constexpr(Grow)
            {
                auto const maximum{read_imm<::std::size_t>(args...[0])};
                // [handler][memory*][maximum bytes][successor]
                //                                  ^^ IP at the proved next dispatch.
                // [older values][i64 delta] <- SP, or AddressPos in its register ring.
                auto const delta{get_curr_val_from_operand_stack_top<Option, wasm_i64, AddressPos>(args...)};
                // [older values] <- SP when uncached; the complete eight-byte
                // operand was consumed within the validated operand allocation.
                result = ::std::bit_cast<wasm_i64>(::uwvm2::runtime::compiler::shared::wasm_memory64::grow(
                    *memory, maximum, static_cast<::std::uint_least64_t>(delta), ::uwvm2::object::memory::flags::grow_strict));
            }
            else { result = static_cast<wasm_i64>(memory->get_page_size()); }
            if constexpr(Option.i64_stack_top_begin_pos != Option.i64_stack_top_end_pos)
            {
                constexpr auto result_position{Grow ? AddressPos :
                    ring_prev_pos(AddressPos, Option.i64_stack_top_begin_pos, Option.i64_stack_top_end_pos)};
                set_curr_val_to_stacktop_cache<Option, wasm_i64, result_position>(result, args...);
            }
            else
            {
                // [older values][reserved i64 result slot] ... operand allocation
                //                ^^ SP; translation reserved all eight bytes.
                ::std::memcpy(args...[1], ::std::addressof(result), sizeof(result));
                args...[1] += sizeof(result);
                // [older values][i64 result] <- SP within the same allocation.
            }
        }

    }

    template <typename Value, ::std::size_t Bytes, bool Signed, bool Store, auto BoundsCheckFn,
              uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos,
              uwvm_int_stack_top_type... Type>
        requires(Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_load_store(Type... args) UWVM_THROWS
    {
        details::memory64::execute<Value, Bytes, Signed, Store, false, BoundsCheckFn, Option, ValuePos, AddressPos>(args...);
        // [consumed memory64 instruction][successor handler] ... owned bytecode
        // [safe                         ][safe             ]
        //                                 ^^ IP from execute; no pointer advance here.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), args...[0], sizeof(next));
        UWVM_MUSTTAIL return next(args...);
    }

    template <typename Value, ::std::size_t Bytes, bool Signed, bool Store, auto BoundsCheckFn,
              uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos,
              uwvm_int_stack_top_type... Type>
        requires(!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_load_store(Type&... args) UWVM_THROWS
    { details::memory64::execute<Value, Bytes, Signed, Store, false, BoundsCheckFn, Option, ValuePos, AddressPos>(args...); }


    template<typename Value, ::std::size_t Bytes, bool Store, auto BoundsCheckFn,
             uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos,
             uwvm_int_stack_top_type... Type>
        requires(Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_atomic(Type... args) UWVM_THROWS
    {
        details::memory64::execute<Value, Bytes, false, Store, true, BoundsCheckFn, Option, ValuePos, AddressPos>(args...);
        // [consumed atomic instruction][successor] ... compiler-owned bytecode
        // [safe                       ][safe     ]
        //                               ^^ IP from execute; unchanged below.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), args...[0], sizeof(next));
        UWVM_MUSTTAIL return next(args...);
    }

    template<typename Value, ::std::size_t Bytes, bool Store, auto BoundsCheckFn,
             uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos,
             uwvm_int_stack_top_type... Type>
        requires(!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_atomic(Type&... args) UWVM_THROWS
    { details::memory64::execute<Value, Bytes, false, Store, true, BoundsCheckFn, Option, ValuePos, AddressPos>(args...); }

    template<bool Grow, uwvm_interpreter_translate_option_t Option, ::std::size_t AddressPos,
             uwvm_int_stack_top_type... Type>
        requires(Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_pages(Type... args) UWVM_THROWS
    {
        details::memory64::execute_pages<Grow, Option, AddressPos>(args...);
        // [consumed size/grow slots][successor] ... owned bytecode
        // [safe                   ][safe     ]
        //                           ^^ IP from execute_pages, unchanged below.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), args...[0], sizeof(next));
        UWVM_MUSTTAIL return next(args...);
    }

    template<bool Grow, uwvm_interpreter_translate_option_t Option, ::std::size_t AddressPos,
             uwvm_int_stack_top_type... Type>
        requires(!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_pages(Type&... args) UWVM_THROWS
    { details::memory64::execute_pages<Grow, Option, AddressPos>(args...); }

    namespace translate
    {
        namespace details::memory64
        {
            template<bool Grow> struct pages
            {
                template<uwvm_interpreter_translate_option_t Option, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_memory64_pages<Grow, Option, Pos, Type...>; }
            };
            template <bool Store> struct write_marker {};
            template <> struct write_marker<true> { using writes_memory = void; };
            template <typename Value, ::std::size_t Bytes, bool Signed, bool Store, bool StandardPage, bool Atomic>
            struct operation
            {
                template<auto Bounds> inline static constexpr auto selected_bounds{[]() constexpr noexcept
                {
#if defined(UWVM_SUPPORT_MMAP)
                    if constexpr(StandardPage && Bounds == op_details::bounds_check_mmap_path)
                    { return op_details::memory64::bounds_check_mmap_path_standard_page; }
                    else
#endif
                    { return Bounds; }
                }()};
                template<auto Bounds, uwvm_interpreter_translate_option_t O, ::std::size_t ValuePos, ::std::size_t AddrPos, uwvm_int_stack_top_type... T>
                inline static constexpr uwvm_interpreter_opfunc_t<T...> choose() noexcept
                {
                    if constexpr(Atomic) { return uwvmint_memory64_atomic<Value, Bytes, Store, selected_bounds<Bounds>, O, ValuePos, AddrPos, T...>; }
                    else { return uwvmint_memory64_load_store<Value, Bytes, Signed, Store, selected_bounds<Bounds>, O, ValuePos, AddrPos, T...>; }
                }
                struct one : write_marker<Store>
                {
                    template <auto Bounds, auto, uwvm_interpreter_translate_option_t O, ::std::size_t Pos, uwvm_int_stack_top_type... T>
                    inline static constexpr uwvm_interpreter_opfunc_t<T...> fptr() noexcept
                    { return operation::template choose<Bounds, O, Pos, Pos, T...>(); }
                };
                struct two : write_marker<Store>
                {
                    template <auto Bounds, auto, uwvm_interpreter_translate_option_t O, ::std::size_t ValuePos, ::std::size_t AddrPos, uwvm_int_stack_top_type... T>
                    inline static constexpr uwvm_interpreter_opfunc_t<T...> fptr() noexcept
                    { return operation::template choose<Bounds, O, ValuePos, AddrPos, T...>(); }
                };
                struct value_only : write_marker<Store>
                {
                    template <auto Bounds, auto, uwvm_interpreter_translate_option_t O, ::std::size_t Pos, uwvm_int_stack_top_type... T>
                    inline static constexpr uwvm_interpreter_opfunc_t<T...> fptr() noexcept
                    { return operation::template choose<Bounds, O, Pos, 0uz, T...>(); }
                };
                struct address_only : write_marker<Store>
                {
                    template <auto Bounds, auto, uwvm_interpreter_translate_option_t O, ::std::size_t Pos, uwvm_int_stack_top_type... T>
                    inline static constexpr uwvm_interpreter_opfunc_t<T...> fptr() noexcept
                    { return operation::template choose<Bounds, O, 0uz, Pos, T...>(); }
                };
            };
        }

        template <typename Value, ::std::size_t Bytes, bool Signed, bool Store, bool Atomic,
                  uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto select_memory64_load_store_fptr(
            uwvm_interpreter_stacktop_currpos_t const& positions,
            details::op_details::native_memory_t const& memory, ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            namespace op = details::op_details;
            if constexpr(!Option.is_tail_call)
            {
                if constexpr(Atomic)
                { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_memory64_atomic<Value, Bytes, Store, op::bounds_check_generic, Option, 0uz, 0uz, Type...>); }
                else
                { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_memory64_load_store<Value, Bytes, Signed, Store, op::bounds_check_generic, Option, 0uz, 0uz, Type...>); }
            }
            else
            {
                auto const select{[&]<bool StandardPage>() constexpr noexcept
                {
                    using operations = details::memory64::operation<Value, Bytes, Signed, Store, StandardPage, Atomic>;
                    constexpr auto begin{op::stacktop_range_begin_pos<Option, Value>()};
                    constexpr auto end{op::stacktop_range_end_pos<Option, Value>()};
                    if constexpr(Store)
                    {
                        return details::select_binary_mem_fptr<Option, Value, op::wasm_i64, begin, end,
                            Option.i64_stack_top_begin_pos, Option.i64_stack_top_end_pos,
                            typename operations::one, typename operations::two, typename operations::address_only, 0u, Type...>(positions, memory);
                    }
                    else
                    {
                        return details::select_unary_mem_fptr<Option, op::wasm_i64, Value,
                            Option.i64_stack_top_begin_pos, Option.i64_stack_top_end_pos, begin, end,
                            typename operations::one, typename operations::two, typename operations::value_only, 0u, Type...>(positions, memory);
                    }
                }};
#if defined(UWVM_SUPPORT_MMAP)
                if constexpr(Store)
                {
                    // Immutable memory layout is checked once during translation,
                    // never reloaded on each standard-page memory64 store.
                    if(memory.custom_page_size_log2 == 16u && !memory.require_dynamic_determination_memory_size())
                    { return select.template operator()<true>(); }
                }
#endif
                return select.template operator()<false>();
            }
        }

        template<typename Value, ::std::size_t Bytes, bool Signed, bool Store,
                 uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_memory64_load_store_fptr_from_tuple(
            uwvm_interpreter_stacktop_currpos_t const& positions, details::op_details::native_memory_t const& memory,
            ::uwvm2::utils::container::tuple<Type...> const& tuple) noexcept
        { return select_memory64_load_store_fptr<Value, Bytes, Signed, Store, false, Option>(positions, memory, tuple); }

        template<typename Value, ::std::size_t Bytes, bool Store,
                 uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_memory64_atomic_fptr_from_tuple(
            uwvm_interpreter_stacktop_currpos_t const& positions, details::op_details::native_memory_t const& memory,
            ::uwvm2::utils::container::tuple<Type...> const& tuple) noexcept
        { return select_memory64_load_store_fptr<Value, Bytes, false, Store, true, Option>(positions, memory, tuple); }

        template<bool Grow, uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_memory64_pages_fptr_from_tuple(
            uwvm_interpreter_stacktop_currpos_t const& positions, ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            {
                return details::select_stacktop_fptr_or_default<Option, Option.i64_stack_top_begin_pos,
                    Option.i64_stack_top_end_pos, details::memory64::pages<Grow>, Type...>(positions.i64_stack_top_curr_pos);
            }
            else
            {
                return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_memory64_pages<Grow, Option, 0uz, Type...>);
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

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
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/runtime/compiler/shared/wasm1p1_simd.h>
# include "define.h"
# include "memory64.h"
# include "register_ring.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    namespace details::memory64_simd
    {
        namespace simd = ::uwvm2::runtime::compiler::shared::wasm1p1_simd_details;
        template<simd::simd_code Op> inline constexpr bool lane_load{
            Op == simd::simd_code::v128_load8_lane || Op == simd::simd_code::v128_load16_lane ||
            Op == simd::simd_code::v128_load32_lane || Op == simd::simd_code::v128_load64_lane};
        template<simd::simd_code Op> inline constexpr bool lane_store{
            Op == simd::simd_code::v128_store8_lane || Op == simd::simd_code::v128_store16_lane ||
            Op == simd::simd_code::v128_store32_lane || Op == simd::simd_code::v128_store64_lane};
        template<simd::simd_code Op> inline constexpr bool store{lane_store<Op> || Op == simd::simd_code::v128_store};
        template<simd::simd_code Op> inline constexpr bool consumes_vector{store<Op> || lane_load<Op>};

        template<simd::simd_code Op, auto Bounds, uwvm_interpreter_translate_option_t Option,
                 ::std::size_t ValuePos, ::std::size_t AddressPos, uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr void execute(Type&... args) UWVM_THROWS
        {
            using Value = simd::wasm_v128;
            constexpr auto width{simd::simd_memory_access_size<Op>()};
            constexpr auto vb{Option.v128_stack_top_begin_pos}, ve{Option.v128_stack_top_end_pos};
            constexpr auto ab{Option.i64_stack_top_begin_pos}, ae{Option.i64_stack_top_end_pos};
            constexpr bool merged{vb != ve && vb == ab && ve == ae};
            // [handler][memory*][u64 offset][u8 lane?][successor] ... owned bytecode
            // [safe                                            ]
            // ^^ IP: translation reserved every immediate and the next handler.
            if constexpr(Option.is_tail_call) { args...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>); }
            else { args...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); }
            // [handler][memory*][u64 offset][u8 lane?][successor]
            //          ^^ IP at a complete owned pointer slot.
            auto const memory{read_imm<native_memory_t*>(args...[0])};
            // [handler][memory*][u64 offset][u8 lane?][successor]
            //                   ^^ IP: complete native pointer consumed.
            auto const offset{read_imm<::std::uint_least64_t>(args...[0])};
            // [handler][memory*][u64 offset][u8 lane?][successor]
            //                               ^^ IP after the complete wide offset.
            simd::u8 lane{};
            if constexpr(lane_load<Op> || lane_store<Op>)
            {
                lane = read_imm<simd::u8>(args...[0]);
                // [handler][memory*][u64 offset][u8 lane][successor]
                //                                         ^^ IP; validation proved lane < 16/width.
            }
            Value value{};
            if constexpr(consumes_vector<Op>)
            {
                // [older operands][i64 address][v128] <- SP, unless v128 is cached.
                value = get_curr_val_from_operand_stack_top<Option, Value, ValuePos>(args...);
                // [older operands][i64 address] <- SP when the v128 was uncached.
            }
            wasm_i64 address{};
            if constexpr(consumes_vector<Op> && merged)
            {
                static_assert(ValuePos == AddressPos);
                if constexpr(ae - ab >= 2uz)
                { address = get_curr_val_from_operand_stack_top<Option, wasm_i64, ring_next_pos(AddressPos, ab, ae)>(args...); }
                else
                {
                    // [older operands][spilled i64 address] <- SP; the only
                    // shared ring slot held the consumed vector, not this address.
                    address = get_curr_val_from_operand_stack_cache<wasm_i64>(args...);
                    // [older operands] <- SP after consuming the proved eight bytes.
                }
            }
            else
            {
                // [older operands][i64 address] <- SP, or AddressPos in the i64 ring.
                address = get_curr_val_from_operand_stack_top<Option, wasm_i64, AddressPos>(args...);
                // [older operands] <- SP if uncached; the full address stays i64.
            }
            auto const effective{wasm64_effective_offset(address, offset)};
            enter_memory_operation_memory_lock(*memory); // Eliminated for stable mmap memory.
            Bounds(*memory, 0uz, offset, effective, width);
            if constexpr(store<Op>)
            {
                // [proved destination span] within native reservation
                // [safe                   ] all 65 address bits were checked.
                // ^^ pointer; cross-page stores probe their final byte before mutation.
                auto const pointer{memory64::prepare_store<Bounds, width>(*memory, effective.offset)};
                simd::eval_memory_store<Op>(pointer, value, lane);
            }
            else
            {
                // [proved source span] within native reservation
                // [safe              ] including native-width bounds on ISA32.
                // ^^ pointer is formed only after the complete address proof.
                auto const pointer{ptr_add_u64(memory->memory_begin, effective.offset)};
                value = simd::eval_memory_load<Op>(pointer, value, lane);
            }
            exit_memory_operation_memory_lock(*memory);
            if constexpr(!store<Op>)
            {
                if constexpr(vb != ve)
                {
                    constexpr auto output_position{merged ? (consumes_vector<Op> ? ring_next_pos(ValuePos, vb, ve) : ValuePos) :
                        (consumes_vector<Op> ? ValuePos : ring_prev_pos(ValuePos, vb, ve))};
                    set_curr_val_to_stacktop_cache<Option, Value, output_position>(value, args...);
                }
                else
                {
                    // [older operands][reserved v128 result] ... operand allocation
                    //                  ^^ SP; translator reserved all sixteen bytes.
                    ::std::memcpy(args...[1], ::std::addressof(value), sizeof(value));
                    args...[1] += sizeof(value);
                    // [older operands][v128 result] <- SP, within that same allocation.
                }
            }
        }
    }
    template<details::memory64_simd::simd::simd_code Op, auto Bounds,
             uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos,
             uwvm_int_stack_top_type... Type> requires(Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_simd(Type... args) UWVM_THROWS
    {
        details::memory64_simd::execute<Op, Bounds, Option, ValuePos, AddressPos>(args...);
        // [consumed instruction][successor] ... owned bytecode
        // [safe                ][safe     ]
        //                       ^^ IP is already at the proved successor slot.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), args...[0], sizeof(next));
        UWVM_MUSTTAIL return next(args...);
    }
    template<details::memory64_simd::simd::simd_code Op, auto Bounds,
             uwvm_interpreter_translate_option_t Option, ::std::size_t ValuePos, ::std::size_t AddressPos,
             uwvm_int_stack_top_type... Type> requires(!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_simd(Type&... args) UWVM_THROWS
    { details::memory64_simd::execute<Op, Bounds, Option, ValuePos, AddressPos>(args...); }
    namespace translate
    {
        namespace details::memory64_simd
        {
            template <op_details::memory64_simd::simd::simd_code Op, bool StandardPage>
            struct operation
            {
                inline static constexpr bool Store{op_details::memory64_simd::store<Op>};
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
                    return uwvmint_memory64_simd<Op, selected_bounds<Bounds>, O, ValuePos, AddrPos, T...>;
                }
                struct one : memory64::write_marker<Store>
                {
                    template <auto Bounds, auto, uwvm_interpreter_translate_option_t O, ::std::size_t Pos, uwvm_int_stack_top_type... T>
                    inline static constexpr uwvm_interpreter_opfunc_t<T...> fptr() noexcept
                    { return operation::template choose<Bounds, O, Pos, Pos, T...>(); }
                };
                struct two : memory64::write_marker<Store>
                {
                    template <auto Bounds, auto, uwvm_interpreter_translate_option_t O, ::std::size_t ValuePos, ::std::size_t AddrPos, uwvm_int_stack_top_type... T>
                    inline static constexpr uwvm_interpreter_opfunc_t<T...> fptr() noexcept
                    { return operation::template choose<Bounds, O, ValuePos, AddrPos, T...>(); }
                };
                struct value_only : memory64::write_marker<Store>
                {
                    template <auto Bounds, auto, uwvm_interpreter_translate_option_t O, ::std::size_t Pos, uwvm_int_stack_top_type... T>
                    inline static constexpr uwvm_interpreter_opfunc_t<T...> fptr() noexcept
                    { return operation::template choose<Bounds, O, Pos, 0uz, T...>(); }
                };
                struct address_only : memory64::write_marker<Store>
                {
                    template <auto Bounds, auto, uwvm_interpreter_translate_option_t O, ::std::size_t Pos, uwvm_int_stack_top_type... T>
                    inline static constexpr uwvm_interpreter_opfunc_t<T...> fptr() noexcept
                    { return operation::template choose<Bounds, O, 0uz, Pos, T...>(); }
                };
            };
        }

        template <details::op_details::memory64_simd::simd::simd_code Op,
                  uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_memory64_simd_fptr_from_tuple(
            uwvm_interpreter_stacktop_currpos_t const& positions,
            details::op_details::native_memory_t const& memory, ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            namespace op = details::op_details;
            using Value = op::memory64_simd::simd::wasm_v128;
            constexpr bool Store{op::memory64_simd::store<Op>};
            if constexpr(!Option.is_tail_call)
            {
                return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_memory64_simd<Op, op::bounds_check_generic, Option, 0uz, 0uz, Type...>);
            }
            else
            {
                auto const select{[&]<bool StandardPage>() constexpr noexcept
                {
                    using operations = details::memory64_simd::operation<Op, StandardPage>;
                    constexpr auto begin{op::stacktop_range_begin_pos<Option, Value>()};
                    constexpr auto end{op::stacktop_range_end_pos<Option, Value>()};
                    if constexpr(op::memory64_simd::consumes_vector<Op>)
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

    }
}
#endif
#ifndef UWVM_MODULE
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

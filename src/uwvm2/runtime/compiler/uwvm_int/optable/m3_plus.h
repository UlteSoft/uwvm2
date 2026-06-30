/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
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
# include <bit>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
# include "define.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
# if !(__cpp_pack_indexing >= 202311L)
#  error "UWVM requires at least C++26 standard compiler. See https://en.cppreference.com/w/cpp/feature_test#cpp_pack_indexing"
# endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    namespace m3_plus
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_u64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_v128 = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;

        using local_offset_t = ::std::size_t;
        using m3_register_t = wasm_i64;

        inline constexpr ::std::size_t max_fv_ring_size{8uz};

        enum class int_src : ::std::uint_least8_t
        {
            r0,
            r1,
            local,
            stack_slot,
            immediate
        };

        enum class int_dst : ::std::uint_least8_t
        {
            r0,
            r1,
            local,
            stack_slot
        };

        enum class int_binop : ::std::uint_least8_t
        {
            add,
            sub,
            mul,
            and_,
            or_,
            xor_,
            lt_u,
            lt_s,
            eq,
            ne
        };

        enum class int_unop : ::std::uint_least8_t
        {
            eqz
        };

        enum class float_binop : ::std::uint_least8_t
        {
            add,
            sub,
            mul,
            div
        };

        enum class float_unop : ::std::uint_least8_t
        {
            neg,
            abs
        };

        enum class v128_binop : ::std::uint_least8_t
        {
            f32x4_add,
            f32x4_mul,
            f64x2_add,
            f64x2_mul
        };

        enum class fv_kind : ::std::uint_least8_t
        {
            empty,
            f32,
            f64,
            v128
        };

        template <::std::size_t M3Top, ::std::size_t FVRing>
        struct profile
        {
            static_assert(M3Top <= 2uz, "the initial m3-plus landing supports the hybrid_i2 integer window");
            static_assert(FVRing <= max_fv_ring_size);
            static_assert(FVRing == 0uz || ((FVRing & (FVRing - 1uz)) == 0uz), "FV ring size must be a power of two");

            static constexpr ::std::size_t m3_top{M3Top};
            static constexpr ::std::size_t fv_ring{FVRing};
            static constexpr bool has_integer_register_ring{};
            static constexpr ::std::size_t theoretical_state_count{FVRing == 0uz ? 1uz : FVRing};
        };

        using hybrid_i2_fvring_profile = profile<2uz, 8uz>;

        struct compile_counters
        {
            ::std::uint_least64_t input_ops{};
            ::std::uint_least64_t emitted_ops{};
            ::std::uint_least64_t fused_ops{};
            ::std::uint_least64_t local_get_total{};
            ::std::uint_least64_t local_get_folded{};
            ::std::uint_least64_t local_get_materialized{};
            ::std::uint_least64_t local_set_total{};
            ::std::uint_least64_t local_set_folded{};
            ::std::uint_least64_t opstream_bytes{};
            ::std::uint_least64_t unique_opfuncs{};
            ::std::uint_least64_t theoretical_state_count{};
        };

        struct runtime_counters
        {
            ::std::uint_least64_t dispatch_count{};
            ::std::uint_least64_t slot_load_count{};
            ::std::uint_least64_t slot_store_count{};
            ::std::uint_least64_t local_load_count{};
            ::std::uint_least64_t local_store_count{};
            ::std::uint_least64_t fv_ring_fill_count{};
            ::std::uint_least64_t fv_ring_spill_count{};
            ::std::uint_least64_t fv_logical_head_move_count{};
            ::std::uint_least64_t fv_physical_rotate_count{};
            ::std::uint_least64_t fv_top_update_count{};
            ::std::uint_least64_t branch_count{};
            ::std::uint_least64_t branch_taken_count{};
        };

        struct alignas(16uz) fv_value
        {
            fv_kind kind{};
            ::std::byte storage[16]{};
        };

        struct state
        {
            ::std::byte* local_base{};
            ::std::byte* operand_stack_base{};
            ::std::byte* memory_begin{};
            ::std::size_t memory_size{};

            fv_value fv_ring[max_fv_ring_size]{};
            ::std::uint_least8_t fv_head{};
            ::std::uint_least8_t fv_depth{};

            runtime_counters counters{};
        };

        using opfunc_t = void(UWVM_INTERPRETER_OPFUNC_TYPE_MACRO*)(::std::byte const*, ::std::byte*, ::std::byte*, m3_register_t, m3_register_t) UWVM_THROWS;

        namespace details
        {
            template <typename T>
            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr T read_imm(::std::byte const*& ip) noexcept
            {
                static_assert(::std::is_trivially_copyable_v<T>);
                T value;  // no init
                ::std::memcpy(::std::addressof(value), ip, sizeof(value));
                ip += sizeof(value);
                return value;
            }

            template <typename T>
            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr T load_unaligned(::std::byte const* p) noexcept
            {
                static_assert(::std::is_trivially_copyable_v<T>);
                T value;  // no init
                ::std::memcpy(::std::addressof(value), p, sizeof(value));
                return value;
            }

            template <typename T>
            UWVM_ALWAYS_INLINE inline constexpr void store_unaligned(::std::byte* p, T value) noexcept
            {
                static_assert(::std::is_trivially_copyable_v<T>);
                ::std::memcpy(p, ::std::addressof(value), sizeof(value));
            }

            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr state& state_from_raw(::std::byte* state_raw) noexcept
            { return *reinterpret_cast<state*>(state_raw); }

            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr wasm_i32 reg_to_i32(m3_register_t v) noexcept
            {
                auto const u{static_cast<wasm_u32>(static_cast<wasm_u64>(v))};
                return ::std::bit_cast<wasm_i32>(u);
            }

            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr m3_register_t i32_to_reg(wasm_i32 v) noexcept
            {
                auto const u{::std::bit_cast<wasm_u32>(v)};
                return static_cast<m3_register_t>(static_cast<wasm_u64>(u));
            }

            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr wasm_u32 i32_to_u32(wasm_i32 v) noexcept { return ::std::bit_cast<wasm_u32>(v); }

            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr wasm_i32 u32_to_i32(wasm_u32 v) noexcept { return ::std::bit_cast<wasm_i32>(v); }

            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr wasm_i32 eval_int_binop(int_binop op, wasm_i32 lhs, wasm_i32 rhs) noexcept
            {
                auto const lu{i32_to_u32(lhs)};
                auto const ru{i32_to_u32(rhs)};
                switch(op)
                {
                    case int_binop::add: return u32_to_i32(static_cast<wasm_u32>(lu + ru));
                    case int_binop::sub: return u32_to_i32(static_cast<wasm_u32>(lu - ru));
                    case int_binop::mul: return u32_to_i32(static_cast<wasm_u32>(lu * ru));
                    case int_binop::and_: return u32_to_i32(static_cast<wasm_u32>(lu & ru));
                    case int_binop::or_: return u32_to_i32(static_cast<wasm_u32>(lu | ru));
                    case int_binop::xor_: return u32_to_i32(static_cast<wasm_u32>(lu ^ ru));
                    case int_binop::lt_u: return static_cast<wasm_i32>(lu < ru);
                    case int_binop::lt_s: return static_cast<wasm_i32>(lhs < rhs);
                    case int_binop::eq: return static_cast<wasm_i32>(lu == ru);
                    case int_binop::ne: return static_cast<wasm_i32>(lu != ru);
                }

                ::fast_io::fast_terminate();
            }

            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr wasm_f64 eval_float_binop(float_binop op, wasm_f64 lhs, wasm_f64 rhs) noexcept
            {
                switch(op)
                {
                    case float_binop::add: return static_cast<wasm_f64>(lhs + rhs);
                    case float_binop::sub: return static_cast<wasm_f64>(lhs - rhs);
                    case float_binop::mul: return static_cast<wasm_f64>(lhs * rhs);
                    case float_binop::div: return static_cast<wasm_f64>(lhs / rhs);
                }

                ::fast_io::fast_terminate();
            }

            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr wasm_f64 eval_float_unop(float_unop op, wasm_f64 v) noexcept
            {
                switch(op)
                {
                    case float_unop::neg: return static_cast<wasm_f64>(-v);
                    case float_unop::abs:
                    {
                        auto bits{::std::bit_cast<wasm_u64>(v)};
                        bits &= static_cast<wasm_u64>(0x7fffffffffffffffull);
                        return ::std::bit_cast<wasm_f64>(bits);
                    }
                }

                ::fast_io::fast_terminate();
            }

            template <typename T>
            UWVM_ALWAYS_INLINE inline constexpr void fv_set(fv_value& slot, fv_kind kind, T value) noexcept
            {
                static_assert(sizeof(T) <= sizeof(slot.storage));
                slot.kind = kind;
                ::std::memcpy(slot.storage, ::std::addressof(value), sizeof(value));
            }

            template <typename T>
            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr T fv_get(fv_value const& slot) noexcept
            {
                static_assert(sizeof(T) <= sizeof(slot.storage));
                T value;  // no init
                ::std::memcpy(::std::addressof(value), slot.storage, sizeof(value));
                return value;
            }

            template <typename Profile>
            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr ::std::size_t fv_prev_pos(::std::size_t pos) noexcept
            {
                static_assert(Profile::fv_ring != 0uz);
                return (pos - 1uz) & (Profile::fv_ring - 1uz);
            }

            template <typename Profile>
            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr ::std::size_t fv_next_pos(::std::size_t pos) noexcept
            {
                static_assert(Profile::fv_ring != 0uz);
                return (pos + 1uz) & (Profile::fv_ring - 1uz);
            }

            template <typename Profile, typename T>
            UWVM_ALWAYS_INLINE inline constexpr void fv_push(state& st, fv_kind kind, T value) noexcept
            {
                static_assert(Profile::fv_ring != 0uz);
                auto const next_head{fv_prev_pos<Profile>(st.fv_head)};
                st.fv_head = static_cast<::std::uint_least8_t>(next_head);
                fv_set(st.fv_ring[next_head], kind, value);
                if(st.fv_depth != Profile::fv_ring) [[likely]] { ++st.fv_depth; }
                else
                {
                    // The initial landing records overflow pressure, but does not physically rotate the ring.
                    ++st.counters.fv_ring_spill_count;
                }
                ++st.counters.fv_ring_fill_count;
                ++st.counters.fv_logical_head_move_count;
            }

            template <typename Profile>
            UWVM_ALWAYS_INLINE inline constexpr void fv_pop(state& st) noexcept
            {
                static_assert(Profile::fv_ring != 0uz);
                if(st.fv_depth == 0u) [[unlikely]] { ::fast_io::fast_terminate(); }
                st.fv_head = static_cast<::std::uint_least8_t>(fv_next_pos<Profile>(st.fv_head));
                --st.fv_depth;
                ++st.counters.fv_logical_head_move_count;
            }

            template <typename Profile>
            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr fv_value& fv_top(state& st) noexcept
            {
                static_assert(Profile::fv_ring != 0uz);
                if(st.fv_depth == 0u) [[unlikely]] { ::fast_io::fast_terminate(); }
                return st.fv_ring[st.fv_head];
            }

            template <typename Profile>
            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr fv_value& fv_next(state& st) noexcept
            {
                static_assert(Profile::fv_ring != 0uz);
                if(st.fv_depth < 2u) [[unlikely]] { ::fast_io::fast_terminate(); }
                return st.fv_ring[fv_next_pos<Profile>(st.fv_head)];
            }

            template <int_src Src>
            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr wasm_i32
                read_i32_src(::std::byte const*& ip, ::std::byte*, state& st, m3_register_t r0, m3_register_t r1) noexcept
            {
                if constexpr(Src == int_src::r0) { return reg_to_i32(r0); }
                else if constexpr(Src == int_src::r1) { return reg_to_i32(r1); }
                else if constexpr(Src == int_src::local)
                {
                    auto const off{read_imm<local_offset_t>(ip)};
                    ++st.counters.local_load_count;
                    return load_unaligned<wasm_i32>(st.local_base + off);
                }
                else if constexpr(Src == int_src::stack_slot)
                {
                    auto const off{read_imm<local_offset_t>(ip)};
                    ++st.counters.slot_load_count;
                    return load_unaligned<wasm_i32>(st.operand_stack_base + off);
                }
                else if constexpr(Src == int_src::immediate) { return read_imm<wasm_i32>(ip); }
                else
                {
                    static_assert(Src != Src, "unhandled m3-plus integer source");
                }
            }

            template <int_dst Dst>
            UWVM_ALWAYS_INLINE inline constexpr void
                write_i32_dst(::std::byte const*& ip, ::std::byte*, state& st, m3_register_t& r0, m3_register_t& r1, wasm_i32 value) noexcept
            {
                if constexpr(Dst == int_dst::r0) { r0 = i32_to_reg(value); }
                else if constexpr(Dst == int_dst::r1) { r1 = i32_to_reg(value); }
                else if constexpr(Dst == int_dst::local)
                {
                    auto const off{read_imm<local_offset_t>(ip)};
                    store_unaligned(st.local_base + off, value);
                    ++st.counters.local_store_count;
                }
                else if constexpr(Dst == int_dst::stack_slot)
                {
                    auto const off{read_imm<local_offset_t>(ip)};
                    store_unaligned(st.operand_stack_base + off, value);
                    ++st.counters.slot_store_count;
                }
                else
                {
                    static_assert(Dst != Dst, "unhandled m3-plus integer destination");
                }
            }

            UWVM_ALWAYS_INLINE inline constexpr void check_memory(state const& st, wasm_i32 addr, wasm_u32 offset, ::std::size_t bytes) noexcept
            {
                auto const base{static_cast<::std::uint_least64_t>(i32_to_u32(addr))};
                auto const eff{base + static_cast<::std::uint_least64_t>(offset)};
                if(st.memory_begin == nullptr || eff + bytes > st.memory_size) [[unlikely]] { ::fast_io::fast_terminate(); }
            }

            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr ::std::byte* memory_ptr(state& st, wasm_i32 addr, wasm_u32 offset, ::std::size_t bytes) noexcept
            {
                check_memory(st, addr, offset, bytes);
                auto const eff{static_cast<::std::uint_least64_t>(i32_to_u32(addr)) + static_cast<::std::uint_least64_t>(offset)};
                return st.memory_begin + static_cast<::std::size_t>(eff);
            }

            template <typename LaneT, ::std::size_t N>
            struct lane_array
            {
                LaneT lane[N];
            };

            template <typename UIntT, ::std::size_t N>
            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr lane_array<UIntT, N> load_uint_lanes(wasm_v128 v) noexcept
            {
                static_assert(sizeof(UIntT) * N == sizeof(wasm_v128));
                lane_array<UIntT, N> out{};              // init
                ::std::byte bytes[sizeof(wasm_v128)]{};  // init
                ::std::memcpy(bytes, ::std::addressof(v), sizeof(bytes));
                for(::std::size_t i{}; i != N; ++i)
                {
                    UIntT lane{};  // init
                    ::std::memcpy(::std::addressof(lane), bytes + i * sizeof(UIntT), sizeof(UIntT));
                    out.lane[i] = ::fast_io::little_endian(lane);
                }
                return out;
            }

            template <typename UIntT, ::std::size_t N>
            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr wasm_v128 store_uint_lanes(lane_array<UIntT, N> const& lanes) noexcept
            {
                static_assert(sizeof(UIntT) * N == sizeof(wasm_v128));
                ::std::byte bytes[sizeof(wasm_v128)]{};  // init
                for(::std::size_t i{}; i != N; ++i)
                {
                    auto lane{::fast_io::little_endian(lanes.lane[i])};
                    ::std::memcpy(bytes + i * sizeof(UIntT), ::std::addressof(lane), sizeof(UIntT));
                }

                wasm_v128 out;  // no init
                ::std::memcpy(::std::addressof(out), bytes, sizeof(out));
                return out;
            }

            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr lane_array<wasm_f32, 4uz> load_f32x4_lanes(wasm_v128 v) noexcept
            {
                auto const bits{load_uint_lanes<wasm_u32, 4uz>(v)};
                lane_array<wasm_f32, 4uz> out{};  // init
                for(::std::size_t i{}; i != 4uz; ++i) { out.lane[i] = ::std::bit_cast<wasm_f32>(bits.lane[i]); }
                return out;
            }

            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr wasm_v128 store_f32x4_lanes(lane_array<wasm_f32, 4uz> const& lanes) noexcept
            {
                lane_array<wasm_u32, 4uz> bits{};  // init
                for(::std::size_t i{}; i != 4uz; ++i) { bits.lane[i] = ::std::bit_cast<wasm_u32>(lanes.lane[i]); }
                return store_uint_lanes<wasm_u32, 4uz>(bits);
            }

            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr lane_array<wasm_f64, 2uz> load_f64x2_lanes(wasm_v128 v) noexcept
            {
                auto const bits{load_uint_lanes<wasm_u64, 2uz>(v)};
                lane_array<wasm_f64, 2uz> out{};  // init
                for(::std::size_t i{}; i != 2uz; ++i) { out.lane[i] = ::std::bit_cast<wasm_f64>(bits.lane[i]); }
                return out;
            }

            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr wasm_v128 store_f64x2_lanes(lane_array<wasm_f64, 2uz> const& lanes) noexcept
            {
                lane_array<wasm_u64, 2uz> bits{};  // init
                for(::std::size_t i{}; i != 2uz; ++i) { bits.lane[i] = ::std::bit_cast<wasm_u64>(lanes.lane[i]); }
                return store_uint_lanes<wasm_u64, 2uz>(bits);
            }

            template <v128_binop Op>
            UWVM_ALWAYS_INLINE [[nodiscard]] inline constexpr wasm_v128 eval_v128_binop(wasm_v128 lhs, wasm_v128 rhs) noexcept
            {
                if constexpr(Op == v128_binop::f32x4_add || Op == v128_binop::f32x4_mul)
                {
                    auto l{load_f32x4_lanes(lhs)};
                    auto const r{load_f32x4_lanes(rhs)};
                    for(::std::size_t i{}; i != 4uz; ++i)
                    {
                        if constexpr(Op == v128_binop::f32x4_add) { l.lane[i] = static_cast<wasm_f32>(l.lane[i] + r.lane[i]); }
                        else
                        {
                            l.lane[i] = static_cast<wasm_f32>(l.lane[i] * r.lane[i]);
                        }
                    }
                    return store_f32x4_lanes(l);
                }
                else if constexpr(Op == v128_binop::f64x2_add || Op == v128_binop::f64x2_mul)
                {
                    auto l{load_f64x2_lanes(lhs)};
                    auto const r{load_f64x2_lanes(rhs)};
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        if constexpr(Op == v128_binop::f64x2_add) { l.lane[i] = static_cast<wasm_f64>(l.lane[i] + r.lane[i]); }
                        else
                        {
                            l.lane[i] = static_cast<wasm_f64>(l.lane[i] * r.lane[i]);
                        }
                    }
                    return store_f64x2_lanes(l);
                }
                else
                {
                    static_assert(Op != Op, "unhandled m3-plus v128 binary opcode");
                }
            }
        }  // namespace details

        template <typename Profile, int_binop Op, int_src SrcA, int_src SrcB, int_dst Dst>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_i32_binop(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            static_assert(Profile::m3_top >= 1uz);
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto const lhs{details::read_i32_src<SrcA>(ip, sp, st, r0, r1)};
            auto const rhs{details::read_i32_src<SrcB>(ip, sp, st, r0, r1)};
            auto const out{details::eval_int_binop(Op, lhs, rhs)};
            details::write_i32_dst<Dst>(ip, sp, st, r0, r1, out);
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile, int_unop Op, int_src Src, int_dst Dst>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_i32_unop(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            static_assert(Profile::m3_top >= 1uz);
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto const v{details::read_i32_src<Src>(ip, sp, st, r0, r1)};
            wasm_i32 out{};
            if constexpr(Op == int_unop::eqz) { out = static_cast<wasm_i32>(details::i32_to_u32(v) == wasm_u32{}); }
            else
            {
                static_assert(Op != Op, "unhandled m3-plus integer unary opcode");
            }
            details::write_i32_dst<Dst>(ip, sp, st, r0, r1, out);
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile, int_src Src>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_local_set_i32(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            static_assert(Profile::m3_top >= 1uz);
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto const value{details::read_i32_src<Src>(ip, sp, st, r0, r1)};
            auto const off{details::read_imm<local_offset_t>(ip)};
            details::store_unaligned(st.local_base + off, value);
            ++st.counters.local_store_count;
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile, int_src CondSrc>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_br_if(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            static_assert(Profile::m3_top >= 1uz);
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto const cond{details::read_i32_src<CondSrc>(ip, sp, st, r0, r1)};
            ::std::byte const* jmp_ip;  // no init
            ::std::memcpy(::std::addressof(jmp_ip), ip, sizeof(jmp_ip));
            ip += sizeof(jmp_ip);
            ++st.counters.branch_count;
            if(details::i32_to_u32(cond) != wasm_u32{}) [[likely]]
            {
                ip = jmp_ip;
                ++st.counters.branch_taken_count;
            }
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile, int_src AddrSrc, int_dst Dst>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_i32_load(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            static_assert(Profile::m3_top >= 1uz);
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto const offset{details::read_imm<wasm_u32>(ip)};
            auto const addr{details::read_i32_src<AddrSrc>(ip, sp, st, r0, r1)};
            auto const out{details::load_unaligned<wasm_i32>(details::memory_ptr(st, addr, offset, sizeof(wasm_i32)))};
            details::write_i32_dst<Dst>(ip, sp, st, r0, r1, out);
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile, int_src AddrSrc, int_src ValueSrc>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_i32_store(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            static_assert(Profile::m3_top >= 1uz);
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto const offset{details::read_imm<wasm_u32>(ip)};
            auto const addr{details::read_i32_src<AddrSrc>(ip, sp, st, r0, r1)};
            auto const value{details::read_i32_src<ValueSrc>(ip, sp, st, r0, r1)};
            details::store_unaligned(details::memory_ptr(st, addr, offset, sizeof(wasm_i32)), value);
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_f64_fill_local(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto const off{details::read_imm<local_offset_t>(ip)};
            auto const value{details::load_unaligned<wasm_f64>(st.local_base + off)};
            ++st.counters.local_load_count;
            details::fv_push<Profile>(st, fv_kind::f64, value);
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile, float_binop Op>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_f64_binop_top_local(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto const off{details::read_imm<local_offset_t>(ip)};
            auto const rhs{details::load_unaligned<wasm_f64>(st.local_base + off)};
            auto& top{details::fv_top<Profile>(st)};
            if(top.kind != fv_kind::f64) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const lhs{details::fv_get<wasm_f64>(top)};
            details::fv_set(top, fv_kind::f64, details::eval_float_binop(Op, lhs, rhs));
            ++st.counters.local_load_count;
            ++st.counters.fv_top_update_count;
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile, float_binop Op>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_f64_binop_top_prev(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto& rhs_slot{details::fv_top<Profile>(st)};
            auto& lhs_slot{details::fv_next<Profile>(st)};
            if(rhs_slot.kind != fv_kind::f64 || lhs_slot.kind != fv_kind::f64) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const rhs{details::fv_get<wasm_f64>(rhs_slot)};
            auto const lhs{details::fv_get<wasm_f64>(lhs_slot)};
            details::fv_set(lhs_slot, fv_kind::f64, details::eval_float_binop(Op, lhs, rhs));
            details::fv_pop<Profile>(st);
            ++st.counters.fv_top_update_count;
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile, float_unop Op>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_f64_unop_top(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto& top{details::fv_top<Profile>(st)};
            if(top.kind != fv_kind::f64) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const value{details::fv_get<wasm_f64>(top)};
            details::fv_set(top, fv_kind::f64, details::eval_float_unop(Op, value));
            ++st.counters.fv_top_update_count;
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_f64_local_set_top(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto const off{details::read_imm<local_offset_t>(ip)};
            auto& top{details::fv_top<Profile>(st)};
            if(top.kind != fv_kind::f64) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const value{details::fv_get<wasm_f64>(top)};
            details::store_unaligned(st.local_base + off, value);
            details::fv_pop<Profile>(st);
            ++st.counters.local_store_count;
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile, int_src AddrSrc>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_f64_load(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto const offset{details::read_imm<wasm_u32>(ip)};
            auto const addr{details::read_i32_src<AddrSrc>(ip, sp, st, r0, r1)};
            auto const value{details::load_unaligned<wasm_f64>(details::memory_ptr(st, addr, offset, sizeof(wasm_f64)))};
            details::fv_push<Profile>(st, fv_kind::f64, value);
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile, int_src AddrSrc>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_f64_store(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto const offset{details::read_imm<wasm_u32>(ip)};
            auto const addr{details::read_i32_src<AddrSrc>(ip, sp, st, r0, r1)};
            auto& top{details::fv_top<Profile>(st)};
            if(top.kind != fv_kind::f64) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const value{details::fv_get<wasm_f64>(top)};
            details::store_unaligned(details::memory_ptr(st, addr, offset, sizeof(wasm_f64)), value);
            details::fv_pop<Profile>(st);
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile, int_src AddrSrc>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_v128_load(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto const offset{details::read_imm<wasm_u32>(ip)};
            auto const addr{details::read_i32_src<AddrSrc>(ip, sp, st, r0, r1)};
            auto const value{details::load_unaligned<wasm_v128>(details::memory_ptr(st, addr, offset, sizeof(wasm_v128)))};
            details::fv_push<Profile>(st, fv_kind::v128, value);
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile, int_src AddrSrc>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_v128_store(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto const offset{details::read_imm<wasm_u32>(ip)};
            auto const addr{details::read_i32_src<AddrSrc>(ip, sp, st, r0, r1)};
            auto& top{details::fv_top<Profile>(st)};
            if(top.kind != fv_kind::v128) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const value{details::fv_get<wasm_v128>(top)};
            details::store_unaligned(details::memory_ptr(st, addr, offset, sizeof(wasm_v128)), value);
            details::fv_pop<Profile>(st);
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }

        template <typename Profile, v128_binop Op>
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void
            uwvmint_m3p_v128_binop_top_prev(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3_register_t r0, m3_register_t r1) UWVM_THROWS
        {
            ip += sizeof(opfunc_t);
            auto& st{details::state_from_raw(state_raw)};
            auto& rhs_slot{details::fv_top<Profile>(st)};
            auto& lhs_slot{details::fv_next<Profile>(st)};
            if(rhs_slot.kind != fv_kind::v128 || lhs_slot.kind != fv_kind::v128) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const rhs{details::fv_get<wasm_v128>(rhs_slot)};
            auto const lhs{details::fv_get<wasm_v128>(lhs_slot)};
            details::fv_set(lhs_slot, fv_kind::v128, details::eval_v128_binop<Op>(lhs, rhs));
            details::fv_pop<Profile>(st);
            ++st.counters.fv_top_update_count;
            ++st.counters.dispatch_count;

            opfunc_t next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), ip, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(ip, sp, state_raw, r0, r1);
        }
    }  // namespace m3_plus
}
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

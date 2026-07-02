#pragma once

#include "../strict/uwvm_int_translate_strict_common.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace uwvm2test::uwvm_int_wasm1p1_full
{
    namespace strict = ::uwvm2test::uwvm_int_strict;

    using strict::byte_vec;
    using strict::func_body;
    using strict::func_type;
    using strict::module_builder;
    using strict::runtime_module_t;
    using strict::u8;
    using strict::wasm1p1_numeric_op;
    using strict::wasm1p1_op;
    using strict::wasm1p1_simd_op;
    using strict::wasm_op;

    enum : ::std::size_t
    {
        k_fn_target_a = 0uz,
        k_fn_target_b = 1uz,
        k_fn_sign_extension = 2uz,
        k_fn_trunc_sat_i32 = 3uz,
        k_fn_trunc_sat_i64 = 4uz,
        k_fn_bulk_memory = 5uz,
        k_fn_reference_table = 6uz,
        k_fn_simd_i32 = 7uz,
        k_fn_simd_memory_select = 8uz,
        k_fn_simd_all_forms = 9uz,
        k_fn_cross_feature_mix = 10uz,
        k_fn_count = 11uz,
    };

    [[nodiscard]] inline byte_vec pack_i32_i64(::std::int32_t a, ::std::int64_t b)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] inline byte_vec pack_f32_f64(float a, double b)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 8);
        return out;
    }

    template <typename SignedT, typename UnsignedT, typename OutT>
    [[nodiscard]] inline OutT sign_extend_low(OutT v) noexcept
    {
        auto const low{static_cast<UnsignedT>(v)};
        auto const signed_low{::std::bit_cast<SignedT>(low)};
        return static_cast<OutT>(signed_low);
    }

    [[nodiscard]] inline ::std::int64_t expected_sign_extension(::std::int32_t i32v, ::std::int64_t i64v) noexcept
    {
        auto const a{static_cast<::std::int64_t>(sign_extend_low<::std::int8_t, ::std::uint8_t>(i32v))};
        auto const b{static_cast<::std::int64_t>(sign_extend_low<::std::int16_t, ::std::uint16_t>(i32v))};
        auto const c{sign_extend_low<::std::int8_t, ::std::uint8_t>(i64v)};
        auto const d{sign_extend_low<::std::int16_t, ::std::uint16_t>(i64v)};
        auto const e{sign_extend_low<::std::int32_t, ::std::uint32_t>(i64v)};
        return a + b + c + d + e;
    }

    inline void append_u32_le(byte_vec& out, ::std::uint32_t v)
    {
        strict::append_u8(out, static_cast<::std::uint8_t>(v & 0xffu));
        strict::append_u8(out, static_cast<::std::uint8_t>((v >> 8u) & 0xffu));
        strict::append_u8(out, static_cast<::std::uint8_t>((v >> 16u) & 0xffu));
        strict::append_u8(out, static_cast<::std::uint8_t>((v >> 24u) & 0xffu));
    }

    inline void append_op(byte_vec& c, wasm_op o)
    {
        strict::append_u8(c, u8(o));
    }

    inline void append_wasm1p1_op(byte_vec& c, wasm1p1_op o)
    {
        strict::append_u8(c, u8(o));
    }

    inline void append_numeric_op(byte_vec& c, wasm1p1_numeric_op o)
    {
        append_wasm1p1_op(c, wasm1p1_op::numeric_prefix);
        strict::append_u32_leb(c, strict::u32(o));
    }

    inline void append_simd_op(byte_vec& c, wasm1p1_simd_op o)
    {
        append_wasm1p1_op(c, wasm1p1_op::simd_prefix);
        strict::append_u32_leb(c, strict::u32(o));
    }

    inline void append_i32_const(byte_vec& c, ::std::int32_t v)
    {
        append_op(c, wasm_op::i32_const);
        strict::append_i32_leb(c, v);
    }

    inline void append_i64_const(byte_vec& c, ::std::int64_t v)
    {
        append_op(c, wasm_op::i64_const);
        strict::append_i64_leb(c, v);
    }

    inline void append_f32_const(byte_vec& c, float v)
    {
        append_op(c, wasm_op::f32_const);
        strict::append_f32_ieee(c, v);
    }

    inline void append_f64_const(byte_vec& c, double v)
    {
        append_op(c, wasm_op::f64_const);
        strict::append_f64_ieee(c, v);
    }

    inline void append_v128_zero(byte_vec& c)
    {
        append_simd_op(c, wasm1p1_simd_op::v128_const);
        for(::std::size_t i{}; i != 16uz; ++i) { strict::append_u8(c, 0u); }
    }

    inline void append_i32_triple(byte_vec& c, ::std::int32_t a, ::std::int32_t b, ::std::int32_t d)
    {
        append_i32_const(c, a);
        append_i32_const(c, b);
        append_i32_const(c, d);
    }

    [[nodiscard]] inline byte_vec finish_single_func_module(module_builder mb, func_type ty, func_body fb)
    {
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] inline byte_vec build_full_wasm1p1_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        mb.has_table = true;
        mb.table_min = 5u;
        mb.table_has_max = true;
        mb.table_max = 8u;

        mb.passive_datas.push_back(
            strict::passive_data_segment{.bytes = byte_vec{::std::byte{1}, ::std::byte{2}, ::std::byte{3}, ::std::byte{4},
                                                           ::std::byte{5}, ::std::byte{6}, ::std::byte{7}, ::std::byte{8}}});

        auto op = [](byte_vec& c, wasm_op o) { strict::append_u8(c, u8(o)); };
        auto op1p1 = [](byte_vec& c, wasm1p1_op o) { strict::append_u8(c, u8(o)); };
        auto ext = [&](byte_vec& c, wasm1p1_numeric_op o)
        {
            op1p1(c, wasm1p1_op::numeric_prefix);
            strict::append_u32_leb(c, strict::u32(o));
        };
        auto simd = [&](byte_vec& c, wasm1p1_simd_op o)
        {
            op1p1(c, wasm1p1_op::simd_prefix);
            strict::append_u32_leb(c, strict::u32(o));
        };
        auto u32 = [](byte_vec& c, ::std::uint32_t v) { strict::append_u32_leb(c, v); };
        auto i32 = [](byte_vec& c, ::std::int32_t v) { strict::append_i32_leb(c, v); };
        auto i64 = [](byte_vec& c, ::std::int64_t v) { strict::append_i64_leb(c, v); };
        auto f32 = [](byte_vec& c, float v) { strict::append_f32_ieee(c, v); };
        auto f64 = [](byte_vec& c, double v) { strict::append_f64_ieee(c, v); };

        auto v128_i32x4_const = [&](byte_vec& c, ::std::uint32_t a, ::std::uint32_t b, ::std::uint32_t d, ::std::uint32_t e)
        {
            simd(c, wasm1p1_simd_op::v128_const);
            append_u32_le(c, a);
            append_u32_le(c, b);
            append_u32_le(c, d);
            append_u32_le(c, e);
        };

        {
            func_type ty{{}, {strict::k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 31);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {strict::k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 47);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        mb.passive_elements.push_back(strict::passive_element_segment{.func_indices = {static_cast<::std::uint32_t>(k_fn_target_a),
                                                                                       static_cast<::std::uint32_t>(k_fn_target_b)}});

        {
            func_type ty{{strict::k_val_i32, strict::k_val_i64}, {strict::k_val_i64}};
            func_body fb{};
            auto& c{fb.code};
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op1p1(c, wasm1p1_op::i32_extend8_s);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op1p1(c, wasm1p1_op::i32_extend16_s);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op1p1(c, wasm1p1_op::i64_extend8_s);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op1p1(c, wasm1p1_op::i64_extend16_s);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op1p1(c, wasm1p1_op::i64_extend32_s);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{strict::k_val_f32, strict::k_val_f64},
                         {strict::k_val_i32, strict::k_val_i32, strict::k_val_i32, strict::k_val_i32}};
            func_body fb{};
            auto& c{fb.code};
            op(c, wasm_op::local_get);
            u32(c, 0u);
            ext(c, wasm1p1_numeric_op::i32_trunc_sat_f32_s);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            ext(c, wasm1p1_numeric_op::i32_trunc_sat_f32_u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            ext(c, wasm1p1_numeric_op::i32_trunc_sat_f64_s);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            ext(c, wasm1p1_numeric_op::i32_trunc_sat_f64_u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{strict::k_val_f32, strict::k_val_f64},
                         {strict::k_val_i64, strict::k_val_i64, strict::k_val_i64, strict::k_val_i64}};
            func_body fb{};
            auto& c{fb.code};
            op(c, wasm_op::local_get);
            u32(c, 0u);
            ext(c, wasm1p1_numeric_op::i64_trunc_sat_f32_s);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            ext(c, wasm1p1_numeric_op::i64_trunc_sat_f32_u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            ext(c, wasm1p1_numeric_op::i64_trunc_sat_f64_s);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            ext(c, wasm1p1_numeric_op::i64_trunc_sat_f64_u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {strict::k_val_i32}};
            func_body fb{};
            auto& c{fb.code};
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_const);
            i32(c, 16);
            ext(c, wasm1p1_numeric_op::memory_fill);
            strict::append_u8(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 4);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_const);
            i32(c, 4);
            ext(c, wasm1p1_numeric_op::memory_init);
            u32(c, 0u);
            strict::append_u8(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 8);
            op(c, wasm_op::i32_const);
            i32(c, 4);
            op(c, wasm_op::i32_const);
            i32(c, 4);
            ext(c, wasm1p1_numeric_op::memory_copy);
            strict::append_u8(c, 0u);
            strict::append_u8(c, 0u);

            ext(c, wasm1p1_numeric_op::data_drop);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 12);
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::i32_const);
            i32(c, 2);
            ext(c, wasm1p1_numeric_op::memory_fill);
            strict::append_u8(c, 0u);

            for(::std::uint32_t offset{4u}; offset != 14u; ++offset)
            {
                op(c, wasm_op::i32_const);
                i32(c, 0);
                op(c, wasm_op::i32_load8_u);
                u32(c, 0u);
                u32(c, offset);
                if(offset != 4u) { op(c, wasm_op::i32_add); }
            }
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {strict::k_val_i32}};
            func_body fb{};
            auto& c{fb.code};

            op(c, wasm_op::i32_const);
            i32(c, 1);
            op1p1(c, wasm1p1_op::ref_func);
            u32(c, static_cast<::std::uint32_t>(k_fn_target_b));
            op1p1(c, wasm1p1_op::table_set);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 1);
            op1p1(c, wasm1p1_op::table_get);
            u32(c, 0u);
            op1p1(c, wasm1p1_op::ref_is_null);

            op(c, wasm_op::i32_const);
            i32(c, 2);
            op1p1(c, wasm1p1_op::ref_null);
            strict::append_u8(c, strict::k_ref_funcref);
            op1p1(c, wasm1p1_op::table_set);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 2);
            op1p1(c, wasm1p1_op::table_get);
            u32(c, 0u);
            op1p1(c, wasm1p1_op::ref_is_null);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::i32_const);
            i32(c, 3);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_const);
            i32(c, 2);
            ext(c, wasm1p1_numeric_op::table_init);
            u32(c, 0u);
            u32(c, 0u);

            ext(c, wasm1p1_numeric_op::elem_drop);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_const);
            i32(c, 3);
            op(c, wasm_op::i32_const);
            i32(c, 2);
            ext(c, wasm1p1_numeric_op::table_copy);
            u32(c, 0u);
            u32(c, 0u);

            op1p1(c, wasm1p1_op::ref_null);
            strict::append_u8(c, strict::k_ref_funcref);
            op(c, wasm_op::i32_const);
            i32(c, 2);
            ext(c, wasm1p1_numeric_op::table_grow);
            u32(c, 0u);
            op(c, wasm_op::i32_add);

            ext(c, wasm1p1_numeric_op::table_size);
            u32(c, 0u);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::i32_const);
            i32(c, 5);
            op1p1(c, wasm1p1_op::ref_func);
            u32(c, static_cast<::std::uint32_t>(k_fn_target_a));
            op(c, wasm_op::i32_const);
            i32(c, 2);
            ext(c, wasm1p1_numeric_op::table_fill);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 6);
            op1p1(c, wasm1p1_op::table_get);
            u32(c, 0u);
            op1p1(c, wasm1p1_op::ref_is_null);
            op(c, wasm_op::i32_add);

            op1p1(c, wasm1p1_op::ref_func);
            u32(c, static_cast<::std::uint32_t>(k_fn_target_a));
            op1p1(c, wasm1p1_op::ref_null);
            strict::append_u8(c, strict::k_ref_funcref);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op1p1(c, wasm1p1_op::select_t);
            u32(c, 1u);
            strict::append_u8(c, strict::k_ref_funcref);
            op1p1(c, wasm1p1_op::ref_is_null);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {strict::k_val_i32}};
            func_body fb{};
            auto& c{fb.code};
            v128_i32x4_const(c, 1u, 2u, 3u, 4u);
            v128_i32x4_const(c, 10u, 20u, 30u, 40u);
            simd(c, wasm1p1_simd_op::i32x4_add);
            op(c, wasm_op::i32_const);
            i32(c, 99);
            simd(c, wasm1p1_simd_op::i32x4_replace_lane);
            strict::append_u8(c, 2u);
            simd(c, wasm1p1_simd_op::i32x4_extract_lane);
            strict::append_u8(c, 2u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {strict::k_val_i32}};
            func_body fb{};
            auto& c{fb.code};
            op(c, wasm_op::i32_const);
            i32(c, 16);
            v128_i32x4_const(c, 5u, 6u, 7u, 8u);
            simd(c, wasm1p1_simd_op::v128_store);
            u32(c, 4u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 16);
            simd(c, wasm1p1_simd_op::v128_load);
            u32(c, 4u);
            u32(c, 0u);
            v128_i32x4_const(c, 1u, 1u, 1u, 1u);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op1p1(c, wasm1p1_op::select_t);
            u32(c, 1u);
            strict::append_u8(c, 0x7bu);
            simd(c, wasm1p1_simd_op::i32x4_extract_lane);
            strict::append_u8(c, 3u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            struct mem_case
            {
                wasm1p1_simd_op op{};
                ::std::uint32_t align{};
            };

            struct lane_mem_case
            {
                wasm1p1_simd_op op{};
                ::std::uint32_t align{};
                ::std::uint8_t lane{};
            };

            func_type ty{{}, {strict::k_val_i32}};
            func_body fb{};
            auto& c{fb.code};

            auto emit_i32_const = [&](::std::int32_t v)
            {
                op(c, wasm_op::i32_const);
                i32(c, v);
            };
            auto emit_i64_const = [&](::std::int64_t v)
            {
                op(c, wasm_op::i64_const);
                i64(c, v);
            };
            auto emit_f32_const = [&](float v)
            {
                op(c, wasm_op::f32_const);
                f32(c, v);
            };
            auto emit_f64_const = [&](double v)
            {
                op(c, wasm_op::f64_const);
                f64(c, v);
            };
            auto emit_drop = [&] { op(c, wasm_op::drop); };
            auto emit_v128 = [&] { v128_i32x4_const(c, 0x01020304u, 0x11121314u, 0x21222324u, 0x31323334u); };
            auto emit_v128_rhs = [&] { v128_i32x4_const(c, 0x05060708u, 0x15161718u, 0x25262728u, 0x35363738u); };

            for(auto const mc: {mem_case{wasm1p1_simd_op::v128_load, 4u},
                                mem_case{wasm1p1_simd_op::v128_load8x8_s, 3u},
                                mem_case{wasm1p1_simd_op::v128_load8x8_u, 3u},
                                mem_case{wasm1p1_simd_op::v128_load16x4_s, 3u},
                                mem_case{wasm1p1_simd_op::v128_load16x4_u, 3u},
                                mem_case{wasm1p1_simd_op::v128_load32x2_s, 3u},
                                mem_case{wasm1p1_simd_op::v128_load32x2_u, 3u},
                                mem_case{wasm1p1_simd_op::v128_load8_splat, 0u},
                                mem_case{wasm1p1_simd_op::v128_load16_splat, 1u},
                                mem_case{wasm1p1_simd_op::v128_load32_splat, 2u},
                                mem_case{wasm1p1_simd_op::v128_load64_splat, 3u},
                                mem_case{wasm1p1_simd_op::v128_load32_zero, 2u},
                                mem_case{wasm1p1_simd_op::v128_load64_zero, 3u}})
            {
                emit_i32_const(64);
                simd(c, mc.op);
                u32(c, mc.align);
                u32(c, 0u);
                emit_drop();
            }

            emit_i32_const(64);
            emit_v128();
            simd(c, wasm1p1_simd_op::v128_store);
            u32(c, 4u);
            u32(c, 0u);

            for(auto const mc: {lane_mem_case{wasm1p1_simd_op::v128_load8_lane, 0u, 0u},
                                lane_mem_case{wasm1p1_simd_op::v128_load16_lane, 1u, 1u},
                                lane_mem_case{wasm1p1_simd_op::v128_load32_lane, 2u, 2u},
                                lane_mem_case{wasm1p1_simd_op::v128_load64_lane, 3u, 1u}})
            {
                emit_i32_const(64);
                emit_v128();
                simd(c, mc.op);
                u32(c, mc.align);
                u32(c, 0u);
                strict::append_u8(c, mc.lane);
                emit_drop();
            }

            for(auto const mc: {lane_mem_case{wasm1p1_simd_op::v128_store8_lane, 0u, 0u},
                                lane_mem_case{wasm1p1_simd_op::v128_store16_lane, 1u, 1u},
                                lane_mem_case{wasm1p1_simd_op::v128_store32_lane, 2u, 2u},
                                lane_mem_case{wasm1p1_simd_op::v128_store64_lane, 3u, 1u}})
            {
                emit_i32_const(80);
                emit_v128();
                simd(c, mc.op);
                u32(c, mc.align);
                u32(c, 0u);
                strict::append_u8(c, mc.lane);
            }

            emit_v128();
            emit_v128_rhs();
            simd(c, wasm1p1_simd_op::i8x16_shuffle);
            for(::std::uint8_t lane{}; lane != 16u; ++lane) { strict::append_u8(c, lane); }
            emit_drop();

            emit_v128();
            emit_v128_rhs();
            simd(c, wasm1p1_simd_op::i8x16_swizzle);
            emit_drop();

            emit_i32_const(7);
            simd(c, wasm1p1_simd_op::i8x16_splat);
            emit_drop();
            emit_i32_const(7);
            simd(c, wasm1p1_simd_op::i16x8_splat);
            emit_drop();
            emit_i32_const(7);
            simd(c, wasm1p1_simd_op::i32x4_splat);
            emit_drop();
            emit_i64_const(7);
            simd(c, wasm1p1_simd_op::i64x2_splat);
            emit_drop();
            emit_f32_const(1.25f);
            simd(c, wasm1p1_simd_op::f32x4_splat);
            emit_drop();
            emit_f64_const(2.5);
            simd(c, wasm1p1_simd_op::f64x2_splat);
            emit_drop();

            for(auto const so: {wasm1p1_simd_op::i8x16_extract_lane_s,
                                wasm1p1_simd_op::i8x16_extract_lane_u,
                                wasm1p1_simd_op::i16x8_extract_lane_s,
                                wasm1p1_simd_op::i16x8_extract_lane_u,
                                wasm1p1_simd_op::i32x4_extract_lane,
                                wasm1p1_simd_op::i64x2_extract_lane,
                                wasm1p1_simd_op::f32x4_extract_lane,
                                wasm1p1_simd_op::f64x2_extract_lane})
            {
                emit_v128();
                simd(c, so);
                strict::append_u8(c, 0u);
                emit_drop();
            }

            emit_v128();
            emit_i32_const(5);
            simd(c, wasm1p1_simd_op::i8x16_replace_lane);
            strict::append_u8(c, 0u);
            emit_drop();
            emit_v128();
            emit_i32_const(5);
            simd(c, wasm1p1_simd_op::i16x8_replace_lane);
            strict::append_u8(c, 0u);
            emit_drop();
            emit_v128();
            emit_i32_const(5);
            simd(c, wasm1p1_simd_op::i32x4_replace_lane);
            strict::append_u8(c, 0u);
            emit_drop();
            emit_v128();
            emit_i64_const(5);
            simd(c, wasm1p1_simd_op::i64x2_replace_lane);
            strict::append_u8(c, 0u);
            emit_drop();
            emit_v128();
            emit_f32_const(5.0f);
            simd(c, wasm1p1_simd_op::f32x4_replace_lane);
            strict::append_u8(c, 0u);
            emit_drop();
            emit_v128();
            emit_f64_const(5.0);
            simd(c, wasm1p1_simd_op::f64x2_replace_lane);
            strict::append_u8(c, 0u);
            emit_drop();

            for(auto const so: {wasm1p1_simd_op::i8x16_abs,
                                wasm1p1_simd_op::i8x16_neg,
                                wasm1p1_simd_op::i8x16_popcnt,
                                wasm1p1_simd_op::f32x4_ceil,
                                wasm1p1_simd_op::f32x4_floor,
                                wasm1p1_simd_op::f32x4_trunc,
                                wasm1p1_simd_op::f32x4_nearest,
                                wasm1p1_simd_op::i16x8_extadd_pairwise_i8x16_s,
                                wasm1p1_simd_op::i16x8_extadd_pairwise_i8x16_u,
                                wasm1p1_simd_op::i32x4_extadd_pairwise_i16x8_s,
                                wasm1p1_simd_op::i32x4_extadd_pairwise_i16x8_u,
                                wasm1p1_simd_op::i16x8_abs,
                                wasm1p1_simd_op::i16x8_neg,
                                wasm1p1_simd_op::i16x8_extend_low_i8x16_s,
                                wasm1p1_simd_op::i16x8_extend_high_i8x16_s,
                                wasm1p1_simd_op::i16x8_extend_low_i8x16_u,
                                wasm1p1_simd_op::i16x8_extend_high_i8x16_u,
                                wasm1p1_simd_op::f64x2_ceil,
                                wasm1p1_simd_op::f64x2_floor,
                                wasm1p1_simd_op::f64x2_trunc,
                                wasm1p1_simd_op::f64x2_nearest,
                                wasm1p1_simd_op::i32x4_abs,
                                wasm1p1_simd_op::i32x4_neg,
                                wasm1p1_simd_op::i32x4_extend_low_i16x8_s,
                                wasm1p1_simd_op::i32x4_extend_high_i16x8_s,
                                wasm1p1_simd_op::i32x4_extend_low_i16x8_u,
                                wasm1p1_simd_op::i32x4_extend_high_i16x8_u,
                                wasm1p1_simd_op::i64x2_abs,
                                wasm1p1_simd_op::i64x2_neg,
                                wasm1p1_simd_op::i64x2_extend_low_i32x4_s,
                                wasm1p1_simd_op::i64x2_extend_high_i32x4_s,
                                wasm1p1_simd_op::i64x2_extend_low_i32x4_u,
                                wasm1p1_simd_op::i64x2_extend_high_i32x4_u,
                                wasm1p1_simd_op::f32x4_abs,
                                wasm1p1_simd_op::f32x4_neg,
                                wasm1p1_simd_op::f32x4_sqrt,
                                wasm1p1_simd_op::f64x2_abs,
                                wasm1p1_simd_op::f64x2_neg,
                                wasm1p1_simd_op::f64x2_sqrt,
                                wasm1p1_simd_op::i32x4_trunc_sat_f32x4_s,
                                wasm1p1_simd_op::i32x4_trunc_sat_f32x4_u,
                                wasm1p1_simd_op::f32x4_convert_i32x4_s,
                                wasm1p1_simd_op::f32x4_convert_i32x4_u,
                                wasm1p1_simd_op::i32x4_trunc_sat_f64x2_s_zero,
                                wasm1p1_simd_op::i32x4_trunc_sat_f64x2_u_zero,
                                wasm1p1_simd_op::f64x2_convert_low_i32x4_s,
                                wasm1p1_simd_op::f64x2_convert_low_i32x4_u})
            {
                emit_v128();
                simd(c, so);
                emit_drop();
            }

            for(auto const so: {wasm1p1_simd_op::i8x16_eq,
                                wasm1p1_simd_op::i8x16_ne,
                                wasm1p1_simd_op::i8x16_lt_s,
                                wasm1p1_simd_op::i8x16_lt_u,
                                wasm1p1_simd_op::i8x16_gt_s,
                                wasm1p1_simd_op::i8x16_gt_u,
                                wasm1p1_simd_op::i8x16_le_s,
                                wasm1p1_simd_op::i8x16_le_u,
                                wasm1p1_simd_op::i8x16_ge_s,
                                wasm1p1_simd_op::i8x16_ge_u,
                                wasm1p1_simd_op::i16x8_eq,
                                wasm1p1_simd_op::i16x8_ne,
                                wasm1p1_simd_op::i16x8_lt_s,
                                wasm1p1_simd_op::i16x8_lt_u,
                                wasm1p1_simd_op::i16x8_gt_s,
                                wasm1p1_simd_op::i16x8_gt_u,
                                wasm1p1_simd_op::i16x8_le_s,
                                wasm1p1_simd_op::i16x8_le_u,
                                wasm1p1_simd_op::i16x8_ge_s,
                                wasm1p1_simd_op::i16x8_ge_u,
                                wasm1p1_simd_op::i32x4_eq,
                                wasm1p1_simd_op::i32x4_ne,
                                wasm1p1_simd_op::i32x4_lt_s,
                                wasm1p1_simd_op::i32x4_lt_u,
                                wasm1p1_simd_op::i32x4_gt_s,
                                wasm1p1_simd_op::i32x4_gt_u,
                                wasm1p1_simd_op::i32x4_le_s,
                                wasm1p1_simd_op::i32x4_le_u,
                                wasm1p1_simd_op::i32x4_ge_s,
                                wasm1p1_simd_op::i32x4_ge_u,
                                wasm1p1_simd_op::f32x4_eq,
                                wasm1p1_simd_op::f32x4_ne,
                                wasm1p1_simd_op::f32x4_lt,
                                wasm1p1_simd_op::f32x4_gt,
                                wasm1p1_simd_op::f32x4_le,
                                wasm1p1_simd_op::f32x4_ge,
                                wasm1p1_simd_op::f64x2_eq,
                                wasm1p1_simd_op::f64x2_ne,
                                wasm1p1_simd_op::f64x2_lt,
                                wasm1p1_simd_op::f64x2_gt,
                                wasm1p1_simd_op::f64x2_le,
                                wasm1p1_simd_op::f64x2_ge,
                                wasm1p1_simd_op::v128_and,
                                wasm1p1_simd_op::v128_andnot,
                                wasm1p1_simd_op::v128_or,
                                wasm1p1_simd_op::v128_xor,
                                wasm1p1_simd_op::i8x16_narrow_i16x8_s,
                                wasm1p1_simd_op::i8x16_narrow_i16x8_u,
                                wasm1p1_simd_op::i8x16_add,
                                wasm1p1_simd_op::i8x16_add_sat_s,
                                wasm1p1_simd_op::i8x16_add_sat_u,
                                wasm1p1_simd_op::i8x16_sub,
                                wasm1p1_simd_op::i8x16_sub_sat_s,
                                wasm1p1_simd_op::i8x16_sub_sat_u,
                                wasm1p1_simd_op::i8x16_min_s,
                                wasm1p1_simd_op::i8x16_min_u,
                                wasm1p1_simd_op::i8x16_max_s,
                                wasm1p1_simd_op::i8x16_max_u,
                                wasm1p1_simd_op::i8x16_avgr_u,
                                wasm1p1_simd_op::i16x8_q15mulr_sat_s,
                                wasm1p1_simd_op::i16x8_narrow_i32x4_s,
                                wasm1p1_simd_op::i16x8_narrow_i32x4_u,
                                wasm1p1_simd_op::i16x8_add,
                                wasm1p1_simd_op::i16x8_add_sat_s,
                                wasm1p1_simd_op::i16x8_add_sat_u,
                                wasm1p1_simd_op::i16x8_sub,
                                wasm1p1_simd_op::i16x8_sub_sat_s,
                                wasm1p1_simd_op::i16x8_sub_sat_u,
                                wasm1p1_simd_op::i16x8_mul,
                                wasm1p1_simd_op::i16x8_min_s,
                                wasm1p1_simd_op::i16x8_min_u,
                                wasm1p1_simd_op::i16x8_max_s,
                                wasm1p1_simd_op::i16x8_max_u,
                                wasm1p1_simd_op::i16x8_avgr_u,
                                wasm1p1_simd_op::i16x8_extmul_low_i8x16_s,
                                wasm1p1_simd_op::i16x8_extmul_high_i8x16_s,
                                wasm1p1_simd_op::i16x8_extmul_low_i8x16_u,
                                wasm1p1_simd_op::i16x8_extmul_high_i8x16_u,
                                wasm1p1_simd_op::i32x4_add,
                                wasm1p1_simd_op::i32x4_sub,
                                wasm1p1_simd_op::i32x4_mul,
                                wasm1p1_simd_op::i32x4_min_s,
                                wasm1p1_simd_op::i32x4_min_u,
                                wasm1p1_simd_op::i32x4_max_s,
                                wasm1p1_simd_op::i32x4_max_u,
                                wasm1p1_simd_op::i32x4_dot_i16x8_s,
                                wasm1p1_simd_op::i32x4_extmul_low_i16x8_s,
                                wasm1p1_simd_op::i32x4_extmul_high_i16x8_s,
                                wasm1p1_simd_op::i32x4_extmul_low_i16x8_u,
                                wasm1p1_simd_op::i32x4_extmul_high_i16x8_u,
                                wasm1p1_simd_op::i64x2_add,
                                wasm1p1_simd_op::i64x2_sub,
                                wasm1p1_simd_op::i64x2_mul,
                                wasm1p1_simd_op::i64x2_eq,
                                wasm1p1_simd_op::i64x2_ne,
                                wasm1p1_simd_op::i64x2_lt_s,
                                wasm1p1_simd_op::i64x2_gt_s,
                                wasm1p1_simd_op::i64x2_le_s,
                                wasm1p1_simd_op::i64x2_ge_s,
                                wasm1p1_simd_op::i64x2_extmul_low_i32x4_s,
                                wasm1p1_simd_op::i64x2_extmul_high_i32x4_s,
                                wasm1p1_simd_op::i64x2_extmul_low_i32x4_u,
                                wasm1p1_simd_op::i64x2_extmul_high_i32x4_u,
                                wasm1p1_simd_op::f32x4_add,
                                wasm1p1_simd_op::f32x4_sub,
                                wasm1p1_simd_op::f32x4_mul,
                                wasm1p1_simd_op::f32x4_div,
                                wasm1p1_simd_op::f32x4_min,
                                wasm1p1_simd_op::f32x4_max,
                                wasm1p1_simd_op::f32x4_pmin,
                                wasm1p1_simd_op::f32x4_pmax,
                                wasm1p1_simd_op::f64x2_add,
                                wasm1p1_simd_op::f64x2_sub,
                                wasm1p1_simd_op::f64x2_mul,
                                wasm1p1_simd_op::f64x2_div,
                                wasm1p1_simd_op::f64x2_min,
                                wasm1p1_simd_op::f64x2_max,
                                wasm1p1_simd_op::f64x2_pmin,
                                wasm1p1_simd_op::f64x2_pmax})
            {
                emit_v128();
                emit_v128_rhs();
                simd(c, so);
                emit_drop();
            }

            emit_v128();
            simd(c, wasm1p1_simd_op::v128_not);
            emit_drop();
            emit_v128();
            emit_v128_rhs();
            emit_v128();
            simd(c, wasm1p1_simd_op::v128_bitselect);
            emit_drop();

            for(auto const so: {wasm1p1_simd_op::v128_any_true,
                                wasm1p1_simd_op::i8x16_all_true,
                                wasm1p1_simd_op::i8x16_bitmask,
                                wasm1p1_simd_op::i16x8_all_true,
                                wasm1p1_simd_op::i16x8_bitmask,
                                wasm1p1_simd_op::i32x4_all_true,
                                wasm1p1_simd_op::i32x4_bitmask,
                                wasm1p1_simd_op::i64x2_all_true,
                                wasm1p1_simd_op::i64x2_bitmask})
            {
                emit_v128();
                simd(c, so);
                emit_drop();
            }

            for(auto const so: {wasm1p1_simd_op::i8x16_shl,
                                wasm1p1_simd_op::i8x16_shr_s,
                                wasm1p1_simd_op::i8x16_shr_u,
                                wasm1p1_simd_op::i16x8_shl,
                                wasm1p1_simd_op::i16x8_shr_s,
                                wasm1p1_simd_op::i16x8_shr_u,
                                wasm1p1_simd_op::i32x4_shl,
                                wasm1p1_simd_op::i32x4_shr_s,
                                wasm1p1_simd_op::i32x4_shr_u,
                                wasm1p1_simd_op::i64x2_shl,
                                wasm1p1_simd_op::i64x2_shr_s,
                                wasm1p1_simd_op::i64x2_shr_u})
            {
                emit_v128();
                emit_i32_const(1);
                simd(c, so);
                emit_drop();
            }

            emit_i32_const(123);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {strict::k_val_i32}};
            func_body fb{};
            auto& c{fb.code};

            op(c, wasm_op::i32_const);
            i32(c, 96);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_const);
            i32(c, 64);
            ext(c, wasm1p1_numeric_op::memory_fill);
            strict::append_u8(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 96);
            v128_i32x4_const(c, 11u, 22u, 33u, 44u);
            simd(c, wasm1p1_simd_op::v128_store);
            u32(c, 4u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 128);
            op(c, wasm_op::i32_const);
            i32(c, 96);
            op(c, wasm_op::i32_const);
            i32(c, 16);
            ext(c, wasm1p1_numeric_op::memory_copy);
            strict::append_u8(c, 0u);
            strict::append_u8(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op1p1(c, wasm1p1_op::ref_func);
            u32(c, static_cast<::std::uint32_t>(k_fn_target_b));
            op1p1(c, wasm1p1_op::table_set);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 128);
            simd(c, wasm1p1_simd_op::v128_load);
            u32(c, 4u);
            u32(c, 0u);
            v128_i32x4_const(c, 99u, 88u, 77u, 66u);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op1p1(c, wasm1p1_op::select_t);
            u32(c, 1u);
            strict::append_u8(c, 0x7bu);
            simd(c, wasm1p1_simd_op::i32x4_extract_lane);
            strict::append_u8(c, 2u);

            op(c, wasm_op::i32_const);
            i32(c, 240);
            op1p1(c, wasm1p1_op::i32_extend8_s);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::f32_const);
            f32(c, -1.5f);
            ext(c, wasm1p1_numeric_op::i32_trunc_sat_f32_s);
            op(c, wasm_op::i32_add);

            op1p1(c, wasm1p1_op::ref_func);
            u32(c, static_cast<::std::uint32_t>(k_fn_target_a));
            op1p1(c, wasm1p1_op::ref_null);
            strict::append_u8(c, strict::k_ref_funcref);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op1p1(c, wasm1p1_op::select_t);
            u32(c, 1u);
            strict::append_u8(c, strict::k_ref_funcref);
            op1p1(c, wasm1p1_op::ref_is_null);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op1p1(c, wasm1p1_op::table_get);
            u32(c, 0u);
            op1p1(c, wasm1p1_op::ref_is_null);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] inline byte_vec build_invalid_ref_feature_module()
    {
        module_builder mb{};
        auto op = [](byte_vec& c, wasm_op o) { strict::append_u8(c, u8(o)); };
        auto op1p1 = [](byte_vec& c, wasm1p1_op o) { strict::append_u8(c, u8(o)); };

        func_type ty{{}, {strict::k_val_i32}};
        func_body fb{};
        op1p1(fb.code, wasm1p1_op::ref_null);
        strict::append_u8(fb.code, strict::k_ref_funcref);
        op1p1(fb.code, wasm1p1_op::ref_is_null);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] inline byte_vec build_invalid_sign_extension_type_module()
    {
        module_builder mb{};
        auto op = [](byte_vec& c, wasm_op o) { strict::append_u8(c, u8(o)); };
        auto op1p1 = [](byte_vec& c, wasm1p1_op o) { strict::append_u8(c, u8(o)); };

        func_type ty{{}, {strict::k_val_i32}};
        func_body fb{};
        op(fb.code, wasm_op::i64_const);
        strict::append_i64_leb(fb.code, 1);
        op1p1(fb.code, wasm1p1_op::i32_extend8_s);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] inline byte_vec build_invalid_ref_null_type_module()
    {
        module_builder mb{};
        func_type ty{{}, {strict::k_val_i32}};
        func_body fb{};
        append_wasm1p1_op(fb.code, wasm1p1_op::ref_null);
        strict::append_u8(fb.code, strict::k_val_i32);
        append_wasm1p1_op(fb.code, wasm1p1_op::ref_is_null);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_ref_is_null_i32_module()
    {
        module_builder mb{};
        func_type ty{{}, {strict::k_val_i32}};
        func_body fb{};
        append_i32_const(fb.code, 0);
        append_wasm1p1_op(fb.code, wasm1p1_op::ref_is_null);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_ref_func_undeclared_module()
    {
        module_builder mb{};
        func_type ty{{}, {strict::k_val_i32}};
        func_body fb{};
        append_wasm1p1_op(fb.code, wasm1p1_op::ref_func);
        strict::append_u32_leb(fb.code, 0u);
        append_op(fb.code, wasm_op::drop);
        append_i32_const(fb.code, 0);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_select_t_count_module()
    {
        module_builder mb{};
        func_type ty{{}, {strict::k_val_i32}};
        func_body fb{};
        append_i32_triple(fb.code, 1, 2, 0);
        append_wasm1p1_op(fb.code, wasm1p1_op::select_t);
        strict::append_u32_leb(fb.code, 2u);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_select_t_cond_type_module()
    {
        module_builder mb{};
        func_type ty{{}, {strict::k_val_i32}};
        func_body fb{};
        append_i32_const(fb.code, 1);
        append_i32_const(fb.code, 2);
        append_i64_const(fb.code, 0);
        append_wasm1p1_op(fb.code, wasm1p1_op::select_t);
        strict::append_u32_leb(fb.code, 1u);
        strict::append_u8(fb.code, strict::k_val_i32);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_select_t_result_type_mismatch_module()
    {
        module_builder mb{};
        func_type ty{{}, {strict::k_val_i32}};
        func_body fb{};
        append_i64_const(fb.code, 1);
        append_i64_const(fb.code, 2);
        append_i32_const(fb.code, 0);
        append_wasm1p1_op(fb.code, wasm1p1_op::select_t);
        strict::append_u32_leb(fb.code, 1u);
        strict::append_u8(fb.code, strict::k_val_i32);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_numeric_prefix_module()
    {
        module_builder mb{};
        func_type ty{{}, {}};
        func_body fb{};
        append_wasm1p1_op(fb.code, wasm1p1_op::numeric_prefix);
        strict::append_u32_leb(fb.code, 0x12u);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_simd_prefix_module()
    {
        module_builder mb{};
        func_type ty{{}, {}};
        func_body fb{};
        append_wasm1p1_op(fb.code, wasm1p1_op::simd_prefix);
        strict::append_u32_leb(fb.code, 0x100u);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_nontrapping_feature_module()
    {
        module_builder mb{};
        func_type ty{{}, {strict::k_val_i32}};
        func_body fb{};
        append_f32_const(fb.code, 1.0f);
        append_numeric_op(fb.code, wasm1p1_numeric_op::i32_trunc_sat_f32_s);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_bulk_feature_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        func_type ty{{}, {}};
        func_body fb{};
        append_i32_triple(fb.code, 0, 0, 1);
        append_numeric_op(fb.code, wasm1p1_numeric_op::memory_fill);
        strict::append_u8(fb.code, 0u);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_memory_init_data_index_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        func_type ty{{}, {}};
        func_body fb{};
        append_i32_triple(fb.code, 0, 0, 1);
        append_numeric_op(fb.code, wasm1p1_numeric_op::memory_init);
        strict::append_u32_leb(fb.code, 0u);
        strict::append_u8(fb.code, 0u);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_memory_copy_memidx_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        func_type ty{{}, {}};
        func_body fb{};
        append_i32_triple(fb.code, 0, 0, 1);
        append_numeric_op(fb.code, wasm1p1_numeric_op::memory_copy);
        strict::append_u8(fb.code, 1u);
        strict::append_u8(fb.code, 0u);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_memory_fill_type_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        func_type ty{{}, {}};
        func_body fb{};
        append_i32_const(fb.code, 0);
        append_i32_const(fb.code, 0);
        append_i64_const(fb.code, 1);
        append_numeric_op(fb.code, wasm1p1_numeric_op::memory_fill);
        strict::append_u8(fb.code, 0u);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_table_get_index_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 1u;
        func_type ty{{}, {}};
        func_body fb{};
        append_i32_const(fb.code, 0);
        append_wasm1p1_op(fb.code, wasm1p1_op::table_get);
        strict::append_u32_leb(fb.code, 1u);
        append_op(fb.code, wasm_op::drop);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_table_set_value_type_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 1u;
        func_type ty{{}, {}};
        func_body fb{};
        append_i32_const(fb.code, 0);
        append_i32_const(fb.code, 1);
        append_wasm1p1_op(fb.code, wasm1p1_op::table_set);
        strict::append_u32_leb(fb.code, 0u);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_table_fill_len_type_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 1u;
        func_type ty{{}, {}};
        func_body fb{};
        append_i32_const(fb.code, 0);
        append_wasm1p1_op(fb.code, wasm1p1_op::ref_null);
        strict::append_u8(fb.code, strict::k_ref_funcref);
        append_f64_const(fb.code, 1.0);
        append_numeric_op(fb.code, wasm1p1_numeric_op::table_fill);
        strict::append_u32_leb(fb.code, 0u);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_table_grow_value_type_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 1u;
        func_type ty{{}, {}};
        func_body fb{};
        append_i32_const(fb.code, 1);
        append_i32_const(fb.code, 1);
        append_numeric_op(fb.code, wasm1p1_numeric_op::table_grow);
        strict::append_u32_leb(fb.code, 0u);
        append_op(fb.code, wasm_op::drop);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_table_init_elem_index_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 1u;
        func_type ty{{}, {}};
        func_body fb{};
        append_i32_triple(fb.code, 0, 0, 1);
        append_numeric_op(fb.code, wasm1p1_numeric_op::table_init);
        strict::append_u32_leb(fb.code, 0u);
        strict::append_u32_leb(fb.code, 0u);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_simd_feature_module()
    {
        module_builder mb{};
        func_type ty{{}, {}};
        func_body fb{};
        append_v128_zero(fb.code);
        append_op(fb.code, wasm_op::drop);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_simd_lane_module()
    {
        module_builder mb{};
        func_type ty{{}, {}};
        func_body fb{};
        append_v128_zero(fb.code);
        append_simd_op(fb.code, wasm1p1_simd_op::i32x4_extract_lane);
        strict::append_u8(fb.code, 4u);
        append_op(fb.code, wasm_op::drop);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_simd_align_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        func_type ty{{}, {}};
        func_body fb{};
        append_i32_const(fb.code, 0);
        append_simd_op(fb.code, wasm1p1_simd_op::v128_load);
        strict::append_u32_leb(fb.code, 5u);
        strict::append_u32_leb(fb.code, 0u);
        append_op(fb.code, wasm_op::drop);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_simd_no_memory_module()
    {
        module_builder mb{};
        func_type ty{{}, {}};
        func_body fb{};
        append_i32_const(fb.code, 0);
        append_simd_op(fb.code, wasm1p1_simd_op::v128_load);
        strict::append_u32_leb(fb.code, 4u);
        strict::append_u32_leb(fb.code, 0u);
        append_op(fb.code, wasm_op::drop);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_ref_feature_masks_ref_type_module()
    {
        module_builder mb{};
        func_type ty{{}, {strict::k_val_i32}};
        func_body fb{};
        append_wasm1p1_op(fb.code, wasm1p1_op::ref_null);
        strict::append_u8(fb.code, strict::k_val_i32);
        append_wasm1p1_op(fb.code, wasm1p1_op::ref_is_null);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_bulk_feature_masks_data_index_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        func_type ty{{}, {}};
        func_body fb{};
        append_i32_triple(fb.code, 0, 0, 1);
        append_numeric_op(fb.code, wasm1p1_numeric_op::memory_init);
        strict::append_u32_leb(fb.code, 0u);
        strict::append_u8(fb.code, 0u);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_simd_feature_masks_lane_module()
    {
        module_builder mb{};
        func_type ty{{}, {}};
        func_body fb{};
        append_simd_op(fb.code, wasm1p1_simd_op::i32x4_extract_lane);
        strict::append_u8(fb.code, 4u);
        append_op(fb.code, wasm_op::drop);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_invalid_table_fill_feature_masks_type_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 1u;
        func_type ty{{}, {}};
        func_body fb{};
        append_i32_const(fb.code, 0);
        append_i32_const(fb.code, 1);
        append_f64_const(fb.code, 1.0);
        append_numeric_op(fb.code, wasm1p1_numeric_op::table_fill);
        strict::append_u32_leb(fb.code, 0u);
        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    [[nodiscard]] inline byte_vec build_valid_polymorphic_wasm1p1_combo_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.has_table = true;
        mb.table_min = 1u;

        func_type ty{{}, {}};
        func_body fb{};
        append_op(fb.code, wasm_op::unreachable);

        append_wasm1p1_op(fb.code, wasm1p1_op::select_t);
        strict::append_u32_leb(fb.code, 1u);
        strict::append_u8(fb.code, strict::k_ref_funcref);
        append_wasm1p1_op(fb.code, wasm1p1_op::ref_is_null);
        append_op(fb.code, wasm_op::drop);

        append_simd_op(fb.code, wasm1p1_simd_op::v128_load);
        strict::append_u32_leb(fb.code, 4u);
        strict::append_u32_leb(fb.code, 0u);
        append_op(fb.code, wasm_op::drop);

        append_numeric_op(fb.code, wasm1p1_numeric_op::memory_fill);
        strict::append_u8(fb.code, 0u);

        append_numeric_op(fb.code, wasm1p1_numeric_op::table_fill);
        strict::append_u32_leb(fb.code, 0u);

        append_op(fb.code, wasm_op::end);
        return finish_single_func_module(::std::move(mb), ::std::move(ty), ::std::move(fb));
    }

    template <strict::optable::uwvm_interpreter_translate_option_t Opt, typename RunFunc>
    [[nodiscard]] inline int run_full_wasm1p1_suite(runtime_module_t const& rt, RunFunc run_func) noexcept
    {
        {
            auto rr{run_func(k_fn_target_a, strict::pack_no_params())};
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results) == 31);
        }

        {
            constexpr ::std::int32_t i32v{static_cast<::std::int32_t>(0x000080f0u)};
            constexpr ::std::int64_t i64v{static_cast<::std::int64_t>(0x00000000800080f0ull)};
            auto rr{run_func(k_fn_sign_extension, pack_i32_i64(i32v, i64v))};
            UWVM2TEST_REQUIRE(strict::load_i64(rr.results) == expected_sign_extension(i32v, i64v));
        }

        {
            auto rr{run_func(k_fn_trunc_sat_i32, pack_f32_f64(::std::numeric_limits<float>::quiet_NaN(), 4294967296.0))};
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results, 0uz) == 0);
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results, 4uz) == 0);
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results, 8uz) == (::std::numeric_limits<::std::int32_t>::max)());
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(strict::load_i32(rr.results, 12uz)) == 0xffffffffu);
        }

        {
            auto rr{run_func(k_fn_trunc_sat_i32, pack_f32_f64(-129.75f, 42.9))};
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results, 0uz) == -129);
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results, 4uz) == 0);
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results, 8uz) == 42);
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results, 12uz) == 42);
        }

        {
            auto rr{run_func(k_fn_trunc_sat_i64, pack_f32_f64(-1.5f, 9223372036854775808.0))};
            UWVM2TEST_REQUIRE(strict::load_i64(rr.results, 0uz) == -1);
            UWVM2TEST_REQUIRE(strict::load_i64(rr.results, 8uz) == 0);
            UWVM2TEST_REQUIRE(strict::load_i64(rr.results, 16uz) == (::std::numeric_limits<::std::int64_t>::max)());
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(strict::load_i64(rr.results, 24uz)) == 0x8000000000000000ull);
        }

        {
            auto rr{run_func(k_fn_bulk_memory, strict::pack_no_params())};
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results) == 42);
        }

        {
            auto rr{run_func(k_fn_reference_table, strict::pack_no_params())};
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results) == 14);
        }

        {
            auto rr{run_func(k_fn_simd_i32, strict::pack_no_params())};
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results) == 99);
        }

        {
            auto rr{run_func(k_fn_simd_memory_select, strict::pack_no_params())};
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results) == 8);
        }

        {
            auto rr{run_func(k_fn_simd_all_forms, strict::pack_no_params())};
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results) == 123);
        }

        {
            auto rr{run_func(k_fn_cross_feature_mix, strict::pack_no_params())};
            UWVM2TEST_REQUIRE(strict::load_i32(rr.results) == 17);
        }

        (void)rt;
        (void)Opt;
        return 0;
    }
}  // namespace uwvm2test::uwvm_int_wasm1p1_full

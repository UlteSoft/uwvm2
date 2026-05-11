#include "uwvm_int_lazy_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_lazy;

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i64x2(::std::int64_t a, ::std::int64_t b)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] byte_vec pack_f32x2(float a, float b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_f64x2(double a, double b)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        return out;
    }

    enum : ::std::size_t
    {
        k_fn_nop = 0uz,
        k_fn_const7 = 1uz,
        k_fn_control_sampler = 2uz,
        k_fn_call_sampler = 3uz,
        k_fn_locals_globals_select_sampler = 4uz,
        k_fn_memory_sampler = 5uz,
        k_fn_memory_size_grow_sampler = 6uz,
        k_fn_i32_sampler = 7uz,
        k_fn_i64_sampler = 8uz,
        k_fn_f32_sampler = 9uz,
        k_fn_f64_sampler = 10uz,
        k_fn_convert_sampler = 11uz,
        k_fn_i32_add = 12uz,
        k_fn_i64_rem_s = 13uz,
        k_fn_f32_add = 14uz,
        k_fn_f64_mul = 15uz,
        k_fn_i32_trunc_f64_s = 16uz,
        k_fn_if_else = 17uz,
        k_fn_memory_roundtrip = 18uz,
        k_fn_count = 19uz,
    };

    [[nodiscard]] byte_vec build_lazy_full_mvp_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        mb.has_table = true;
        mb.table_min = 1u;
        mb.table_has_max = true;
        mb.table_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { strict::append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { strict::append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { strict::append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { strict::append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { strict::append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { strict::append_f64_ieee(c, v); };

        {
            strict::global_entry g{};
            g.valtype = strict::k_val_i32;
            g.mut = true;
            op(g.init_expr, wasm_op::i32_const);
            i32(g.init_expr, 0);
            op(g.init_expr, wasm_op::end);
            mb.globals.push_back(::std::move(g));
        }

        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::nop);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {strict::k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 7);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c{fb.code};

            op(c, wasm_op::block);
            strict::append_u8(c, k_block_empty);
            op(c, wasm_op::end);

            op(c, wasm_op::loop);
            strict::append_u8(c, k_block_empty);
            op(c, wasm_op::end);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::if_);
            strict::append_u8(c, k_block_empty);
            op(c, wasm_op::else_);
            op(c, wasm_op::end);

            op(c, wasm_op::block);
            strict::append_u8(c, k_block_empty);
            op(c, wasm_op::br);
            u32(c, 0u);
            op(c, wasm_op::end);

            op(c, wasm_op::block);
            strict::append_u8(c, k_block_empty);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::br_if);
            u32(c, 0u);
            op(c, wasm_op::end);

            op(c, wasm_op::block);
            strict::append_u8(c, k_block_empty);
            op(c, wasm_op::block);
            strict::append_u8(c, k_block_empty);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::br_table);
            u32(c, 2u);
            u32(c, 0u);
            u32(c, 1u);
            u32(c, 0u);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            op(c, wasm_op::unreachable);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c{fb.code};

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::call_indirect);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::call);
            u32(c, k_fn_nop);

            op(c, wasm_op::call);
            u32(c, k_fn_const7);
            op(c, wasm_op::drop);

            op(c, wasm_op::return_);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {}};
            func_body fb{};
            fb.locals.push_back({1u, strict::k_val_i32});
            auto& c{fb.code};

            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::global_set);
            u32(c, 0u);

            op(c, wasm_op::global_get);
            u32(c, 0u);
            op(c, wasm_op::local_tee);
            u32(c, 0u);
            op(c, wasm_op::drop);

            op(c, wasm_op::i32_const);
            i32(c, 10);
            op(c, wasm_op::i32_const);
            i32(c, 20);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::select);
            op(c, wasm_op::drop);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c{fb.code};

            auto emit_load_drop = [&](wasm_op load_op, ::std::uint32_t align_pow2, ::std::uint32_t offset)
            {
                op(c, wasm_op::i32_const);
                i32(c, 0);
                op(c, load_op);
                u32(c, align_pow2);
                u32(c, offset);
                op(c, wasm_op::drop);
            };

            auto emit_store = [&](wasm_op store_op, wasm_op const_op, auto imm, ::std::uint32_t align_pow2, ::std::uint32_t offset)
            {
                op(c, wasm_op::i32_const);
                i32(c, 0);
                op(c, const_op);
                imm(c);
                op(c, store_op);
                u32(c, align_pow2);
                u32(c, offset);
            };

            emit_load_drop(wasm_op::i32_load, 2u, 0u);
            emit_load_drop(wasm_op::i64_load, 3u, 0u);
            emit_load_drop(wasm_op::f32_load, 2u, 0u);
            emit_load_drop(wasm_op::f64_load, 3u, 0u);

            emit_load_drop(wasm_op::i32_load8_s, 0u, 0u);
            emit_load_drop(wasm_op::i32_load8_u, 0u, 0u);
            emit_load_drop(wasm_op::i32_load16_s, 1u, 0u);
            emit_load_drop(wasm_op::i32_load16_u, 1u, 0u);
            emit_load_drop(wasm_op::i64_load8_s, 0u, 0u);
            emit_load_drop(wasm_op::i64_load8_u, 0u, 0u);
            emit_load_drop(wasm_op::i64_load16_s, 1u, 0u);
            emit_load_drop(wasm_op::i64_load16_u, 1u, 0u);
            emit_load_drop(wasm_op::i64_load32_s, 2u, 0u);
            emit_load_drop(wasm_op::i64_load32_u, 2u, 0u);

            emit_store(wasm_op::i32_store, wasm_op::i32_const, [&](byte_vec& x) { i32(x, 1); }, 2u, 0u);
            emit_store(wasm_op::i64_store, wasm_op::i64_const, [&](byte_vec& x) { i64(x, 2); }, 3u, 0u);
            emit_store(wasm_op::f32_store, wasm_op::f32_const, [&](byte_vec& x) { f32(x, 3.0f); }, 2u, 0u);
            emit_store(wasm_op::f64_store, wasm_op::f64_const, [&](byte_vec& x) { f64(x, 4.0); }, 3u, 0u);
            emit_store(wasm_op::i32_store8, wasm_op::i32_const, [&](byte_vec& x) { i32(x, 5); }, 0u, 0u);
            emit_store(wasm_op::i32_store16, wasm_op::i32_const, [&](byte_vec& x) { i32(x, 6); }, 1u, 0u);
            emit_store(wasm_op::i64_store8, wasm_op::i64_const, [&](byte_vec& x) { i64(x, 7); }, 0u, 0u);
            emit_store(wasm_op::i64_store16, wasm_op::i64_const, [&](byte_vec& x) { i64(x, 8); }, 1u, 0u);
            emit_store(wasm_op::i64_store32, wasm_op::i64_const, [&](byte_vec& x) { i64(x, 9); }, 2u, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c{fb.code};

            op(c, wasm_op::memory_size);
            u32(c, 0u);
            op(c, wasm_op::drop);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::memory_grow);
            u32(c, 0u);
            op(c, wasm_op::drop);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c{fb.code};

            auto i32_unop_drop = [&](wasm_op o)
            {
                op(c, wasm_op::i32_const);
                i32(c, 1);
                op(c, o);
                op(c, wasm_op::drop);
            };
            auto i32_binop_drop = [&](wasm_op o)
            {
                op(c, wasm_op::i32_const);
                i32(c, 7);
                op(c, wasm_op::i32_const);
                i32(c, 1);
                op(c, o);
                op(c, wasm_op::drop);
            };

            i32_unop_drop(wasm_op::i32_eqz);
            i32_unop_drop(wasm_op::i32_clz);
            i32_unop_drop(wasm_op::i32_ctz);
            i32_unop_drop(wasm_op::i32_popcnt);

            i32_binop_drop(wasm_op::i32_add);
            i32_binop_drop(wasm_op::i32_sub);
            i32_binop_drop(wasm_op::i32_mul);
            i32_binop_drop(wasm_op::i32_div_s);
            i32_binop_drop(wasm_op::i32_div_u);
            i32_binop_drop(wasm_op::i32_rem_s);
            i32_binop_drop(wasm_op::i32_rem_u);
            i32_binop_drop(wasm_op::i32_and);
            i32_binop_drop(wasm_op::i32_or);
            i32_binop_drop(wasm_op::i32_xor);
            i32_binop_drop(wasm_op::i32_shl);
            i32_binop_drop(wasm_op::i32_shr_s);
            i32_binop_drop(wasm_op::i32_shr_u);
            i32_binop_drop(wasm_op::i32_rotl);
            i32_binop_drop(wasm_op::i32_rotr);
            i32_binop_drop(wasm_op::i32_eq);
            i32_binop_drop(wasm_op::i32_ne);
            i32_binop_drop(wasm_op::i32_lt_s);
            i32_binop_drop(wasm_op::i32_lt_u);
            i32_binop_drop(wasm_op::i32_gt_s);
            i32_binop_drop(wasm_op::i32_gt_u);
            i32_binop_drop(wasm_op::i32_le_s);
            i32_binop_drop(wasm_op::i32_le_u);
            i32_binop_drop(wasm_op::i32_ge_s);
            i32_binop_drop(wasm_op::i32_ge_u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c{fb.code};

            auto i64_unop_drop = [&](wasm_op o)
            {
                op(c, wasm_op::i64_const);
                i64(c, 1);
                op(c, o);
                op(c, wasm_op::drop);
            };
            auto i64_binop_drop = [&](wasm_op o)
            {
                op(c, wasm_op::i64_const);
                i64(c, 7);
                op(c, wasm_op::i64_const);
                i64(c, 1);
                op(c, o);
                op(c, wasm_op::drop);
            };

            i64_unop_drop(wasm_op::i64_eqz);
            i64_unop_drop(wasm_op::i64_clz);
            i64_unop_drop(wasm_op::i64_ctz);
            i64_unop_drop(wasm_op::i64_popcnt);

            i64_binop_drop(wasm_op::i64_add);
            i64_binop_drop(wasm_op::i64_sub);
            i64_binop_drop(wasm_op::i64_mul);
            i64_binop_drop(wasm_op::i64_div_s);
            i64_binop_drop(wasm_op::i64_div_u);
            i64_binop_drop(wasm_op::i64_rem_s);
            i64_binop_drop(wasm_op::i64_rem_u);
            i64_binop_drop(wasm_op::i64_and);
            i64_binop_drop(wasm_op::i64_or);
            i64_binop_drop(wasm_op::i64_xor);
            i64_binop_drop(wasm_op::i64_shl);
            i64_binop_drop(wasm_op::i64_shr_s);
            i64_binop_drop(wasm_op::i64_shr_u);
            i64_binop_drop(wasm_op::i64_rotl);
            i64_binop_drop(wasm_op::i64_rotr);
            i64_binop_drop(wasm_op::i64_eq);
            i64_binop_drop(wasm_op::i64_ne);
            i64_binop_drop(wasm_op::i64_lt_s);
            i64_binop_drop(wasm_op::i64_lt_u);
            i64_binop_drop(wasm_op::i64_gt_s);
            i64_binop_drop(wasm_op::i64_gt_u);
            i64_binop_drop(wasm_op::i64_le_s);
            i64_binop_drop(wasm_op::i64_le_u);
            i64_binop_drop(wasm_op::i64_ge_s);
            i64_binop_drop(wasm_op::i64_ge_u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c{fb.code};

            auto f32_unop_drop = [&](wasm_op o, float x = 1.25f)
            {
                op(c, wasm_op::f32_const);
                f32(c, x);
                op(c, o);
                op(c, wasm_op::drop);
            };
            auto f32_binop_drop = [&](wasm_op o, float a = 3.0f, float b = 1.5f)
            {
                op(c, wasm_op::f32_const);
                f32(c, a);
                op(c, wasm_op::f32_const);
                f32(c, b);
                op(c, o);
                op(c, wasm_op::drop);
            };

            f32_unop_drop(wasm_op::f32_abs, -1.0f);
            f32_unop_drop(wasm_op::f32_neg, 1.0f);
            f32_unop_drop(wasm_op::f32_ceil, 1.25f);
            f32_unop_drop(wasm_op::f32_floor, 1.25f);
            f32_unop_drop(wasm_op::f32_trunc, 1.25f);
            f32_unop_drop(wasm_op::f32_nearest, 1.25f);
            f32_unop_drop(wasm_op::f32_sqrt, 4.0f);

            f32_binop_drop(wasm_op::f32_add);
            f32_binop_drop(wasm_op::f32_sub);
            f32_binop_drop(wasm_op::f32_mul);
            f32_binop_drop(wasm_op::f32_div);
            f32_binop_drop(wasm_op::f32_min);
            f32_binop_drop(wasm_op::f32_max);
            f32_binop_drop(wasm_op::f32_copysign, -2.0f, 1.0f);
            f32_binop_drop(wasm_op::f32_eq);
            f32_binop_drop(wasm_op::f32_ne);
            f32_binop_drop(wasm_op::f32_lt);
            f32_binop_drop(wasm_op::f32_gt);
            f32_binop_drop(wasm_op::f32_le);
            f32_binop_drop(wasm_op::f32_ge);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c{fb.code};

            auto f64_unop_drop = [&](wasm_op o, double x = 1.25)
            {
                op(c, wasm_op::f64_const);
                f64(c, x);
                op(c, o);
                op(c, wasm_op::drop);
            };
            auto f64_binop_drop = [&](wasm_op o, double a = 3.0, double b = 1.5)
            {
                op(c, wasm_op::f64_const);
                f64(c, a);
                op(c, wasm_op::f64_const);
                f64(c, b);
                op(c, o);
                op(c, wasm_op::drop);
            };

            f64_unop_drop(wasm_op::f64_abs, -1.0);
            f64_unop_drop(wasm_op::f64_neg, 1.0);
            f64_unop_drop(wasm_op::f64_ceil, 1.25);
            f64_unop_drop(wasm_op::f64_floor, 1.25);
            f64_unop_drop(wasm_op::f64_trunc, 1.25);
            f64_unop_drop(wasm_op::f64_nearest, 1.25);
            f64_unop_drop(wasm_op::f64_sqrt, 4.0);

            f64_binop_drop(wasm_op::f64_add);
            f64_binop_drop(wasm_op::f64_sub);
            f64_binop_drop(wasm_op::f64_mul);
            f64_binop_drop(wasm_op::f64_div);
            f64_binop_drop(wasm_op::f64_min);
            f64_binop_drop(wasm_op::f64_max);
            f64_binop_drop(wasm_op::f64_copysign, -2.0, 1.0);
            f64_binop_drop(wasm_op::f64_eq);
            f64_binop_drop(wasm_op::f64_ne);
            f64_binop_drop(wasm_op::f64_lt);
            f64_binop_drop(wasm_op::f64_gt);
            f64_binop_drop(wasm_op::f64_le);
            f64_binop_drop(wasm_op::f64_ge);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c{fb.code};

            op(c, wasm_op::i64_const);
            i64(c, 1);
            op(c, wasm_op::i32_wrap_i64);
            op(c, wasm_op::drop);

            op(c, wasm_op::i32_const);
            i32(c, -1);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, -1);
            op(c, wasm_op::i64_extend_i32_u);
            op(c, wasm_op::drop);

            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::drop);
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::i32_trunc_f32_u);
            op(c, wasm_op::drop);
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::drop);
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::i32_trunc_f64_u);
            op(c, wasm_op::drop);

            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::i64_trunc_f32_s);
            op(c, wasm_op::drop);
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::i64_trunc_f32_u);
            op(c, wasm_op::drop);
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::i64_trunc_f64_s);
            op(c, wasm_op::drop);
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::i64_trunc_f64_u);
            op(c, wasm_op::drop);

            op(c, wasm_op::i32_const);
            i32(c, -1);
            op(c, wasm_op::f32_convert_i32_s);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, -1);
            op(c, wasm_op::f32_convert_i32_u);
            op(c, wasm_op::drop);
            op(c, wasm_op::i64_const);
            i64(c, -1);
            op(c, wasm_op::f32_convert_i64_s);
            op(c, wasm_op::drop);
            op(c, wasm_op::i64_const);
            i64(c, -1);
            op(c, wasm_op::f32_convert_i64_u);
            op(c, wasm_op::drop);
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::f32_demote_f64);
            op(c, wasm_op::drop);

            op(c, wasm_op::i32_const);
            i32(c, -1);
            op(c, wasm_op::f64_convert_i32_s);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, -1);
            op(c, wasm_op::f64_convert_i32_u);
            op(c, wasm_op::drop);
            op(c, wasm_op::i64_const);
            i64(c, -1);
            op(c, wasm_op::f64_convert_i64_s);
            op(c, wasm_op::drop);
            op(c, wasm_op::i64_const);
            i64(c, -1);
            op(c, wasm_op::f64_convert_i64_u);
            op(c, wasm_op::drop);
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::f64_promote_f32);
            op(c, wasm_op::drop);

            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::i32_reinterpret_f32);
            op(c, wasm_op::drop);

            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::i64_reinterpret_f64);
            op(c, wasm_op::drop);

            op(c, wasm_op::i32_const);
            i32(c, 0x3f800000);
            op(c, wasm_op::f32_reinterpret_i32);
            op(c, wasm_op::drop);

            op(c, wasm_op::i64_const);
            i64(c, 0x3ff0000000000000ll);
            op(c, wasm_op::f64_reinterpret_i64);
            op(c, wasm_op::drop);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{strict::k_val_i32, strict::k_val_i32}, {strict::k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{strict::k_val_i64, strict::k_val_i64}, {strict::k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i64_rem_s);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{strict::k_val_f32, strict::k_val_f32}, {strict::k_val_f32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::f32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{strict::k_val_f64, strict::k_val_f64}, {strict::k_val_f64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::f64_mul);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{strict::k_val_f64}, {strict::k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_trunc_f64_s);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{strict::k_val_i32}, {strict::k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::if_);
            strict::append_u8(fb.code, strict::k_val_i32);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 11);
            op(fb.code, wasm_op::else_);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 22);
            op(fb.code, wasm_op::end);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {strict::k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 8);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, static_cast<::std::int32_t>(0x11223344u));
            op(fb.code, wasm_op::i32_store);
            u32(fb.code, 2u);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 8);
            op(fb.code, wasm_op::i32_load);
            u32(fb.code, 2u);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_lazy_function(prepared_runtime const& prep,
                                            lazy_module_t& storage,
                                            ::uwvm2::utils::container::u8string_view module_name,
                                            lazy_validation_mode_t validation_mode,
                                            ::std::size_t local_function_index,
                                            ::uwvm2::utils::thread::lazy_compile_scheduler& scheduler)
    {
        auto const& fn{storage.functions.index_unchecked(local_function_index)};
        UWVM2TEST_REQUIRE(fn.primary_cu_index != SIZE_MAX);

        auto options{make_lazy_options(module_name, validation_mode)};
        if(validation_mode == lazy_validation_mode_t::validate_on_lazy_compile)
        {
            UWVM2TEST_REQUIRE(options.validator_module_storage != nullptr);
        }
        else
        {
            UWVM2TEST_REQUIRE(options.validator_module_storage == nullptr);
        }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        lazy::lazy_compile_request_context ctx{.curr_module = prep.mod,
                                               .lazy_storage = ::std::addressof(storage),
                                               .options = options,
                                               .compile_unit_index = fn.primary_cu_index,
                                               .err = ::std::addressof(err),
                                               .module_name = module_name};

        auto request{make_lazy_compile_request<Opt>(ctx, 1u)};
        UWVM2TEST_REQUIRE(request.unit != nullptr);
        UWVM2TEST_REQUIRE(request.compile != nullptr);
        UWVM2TEST_REQUIRE(scheduler.ensure_ready(request));
        UWVM2TEST_REQUIRE(scheduler.ensure_ready(request));
        UWVM2TEST_REQUIRE(request.unit->state.load(::std::memory_order_acquire) == lazy_compile_state_t::compiled);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(compiled_local_func_ready(storage, local_function_index));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_executable_checks(prepared_runtime const& prep, lazy_module_t const& storage)
    {
        {
            auto rr{run_compiled_local_func<Opt>(
                storage, k_fn_const7, prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_const7), strict::pack_no_params())};
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 7);
        }

        {
            auto rr{run_compiled_local_func<Opt>(
                storage, k_fn_i32_add, prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_i32_add), pack_i32x2(17, 25))};
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 42);
        }

        {
            auto rr{run_compiled_local_func<Opt>(
                storage, k_fn_i64_rem_s, prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_i64_rem_s), pack_i64x2(-17, 5))};
            UWVM2TEST_REQUIRE(strict::load_i64(rr.results) == (-17 % 5));
        }

        {
            auto rr{run_compiled_local_func<Opt>(
                storage, k_fn_f32_add, prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_f32_add), pack_f32x2(1.5f, 2.25f))};
            UWVM2TEST_REQUIRE(strict::load_f32(rr.results) == 3.75f);
        }

        {
            auto rr{run_compiled_local_func<Opt>(
                storage, k_fn_f64_mul, prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_f64_mul), pack_f64x2(1.25, 4.0))};
            UWVM2TEST_REQUIRE(strict::load_f64(rr.results) == 5.0);
        }

        {
            auto rr{run_compiled_local_func<Opt>(
                storage, k_fn_i32_trunc_f64_s, prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_i32_trunc_f64_s), strict::pack_f64(42.0))};
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 42);
        }

        {
            auto rr_true{run_compiled_local_func<Opt>(
                storage, k_fn_if_else, prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_if_else), strict::pack_i32(1))};
            UWVM2TEST_REQUIRE(load_i32(rr_true.results) == 11);

            auto rr_false{run_compiled_local_func<Opt>(
                storage, k_fn_if_else, prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_if_else), strict::pack_i32(0))};
            UWVM2TEST_REQUIRE(load_i32(rr_false.results) == 22);
        }

        {
            auto rr{run_compiled_local_func<Opt>(
                storage, k_fn_memory_roundtrip, prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_memory_roundtrip), strict::pack_no_params())};
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == 0x11223344u);
        }

        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_all_functions_and_run(::uwvm2::utils::container::u8string_view module_name,
                                                    lazy_validation_mode_t validation_mode)
    {
        auto wasm{build_lazy_full_mvp_module()};
        auto prep{prepare_runtime_from_wasm(wasm, module_name)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        UWVM2TEST_REQUIRE(storage.functions.size() == k_fn_count);

        ::uwvm2::utils::thread::lazy_compile_scheduler scheduler{};
        scheduler.start({.worker_count = 1uz});
        for(::std::size_t i{}; i != k_fn_count; ++i)
        {
            UWVM2TEST_REQUIRE(compile_lazy_function<Opt>(prep, storage, module_name, validation_mode, i, scheduler) == 0);
        }
        scheduler.stop();

        for(::std::size_t i{}; i != k_fn_count; ++i)
        {
            UWVM2TEST_REQUIRE(storage.functions.index_unchecked(i).primary_cu_index != SIZE_MAX);
            UWVM2TEST_REQUIRE(compiled_local_func_ready(storage, i));
        }

        UWVM2TEST_REQUIRE(run_executable_checks<Opt>(prep, storage) == 0);
        return 0;
    }

    [[nodiscard]] int test_lazy_full_mvp()
    {
        configure_unexpected_traps();

        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
        UWVM2TEST_REQUIRE(
            compile_all_functions_and_run<opt>(u8"uwvm2test_lazy_full_mvp_validate", lazy_validation_mode_t::validate_on_lazy_compile) == 0);
        UWVM2TEST_REQUIRE(
            compile_all_functions_and_run<opt>(u8"uwvm2test_lazy_full_mvp_assume", lazy_validation_mode_t::assume_full_code_verified) == 0);

        return 0;
    }
}  // namespace

int main()
{
#if defined(__APPLE__) && (defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_UNDEFINED__) || defined(__SANITIZE_LEAK__))
    return 0;
#else
    return test_lazy_full_mvp();
#endif
}

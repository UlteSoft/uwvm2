#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_runtime_log_full_mvp_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        mb.has_table = true;
        mb.table_min = 1u;
        mb.table_has_max = true;
        mb.table_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // global0: (mut i32) = 0
        {
            global_entry g{};
            g.valtype = k_val_i32;
            g.mut = true;
            op(g.init_expr, wasm_op::i32_const);
            i32(g.init_expr, 0);
            op(g.init_expr, wasm_op::end);
            mb.globals.push_back(::std::move(g));
        }

        // f0: () -> () : nop ; end
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::nop);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: () -> (i32) : i32.const 7 ; end
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 7);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: control-flow opcode sampler (block/loop/if/else/br/br_if/br_table/unreachable).
        // Note: never executed at runtime; used for translator/runtime-log coverage only.
        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c = fb.code;

            // block
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::end);

            // loop
            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);
            op(c, wasm_op::end);

            // if/else
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::if_);
            append_u8(c, k_block_empty);
            op(c, wasm_op::else_);
            op(c, wasm_op::end);

            // br
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::br);
            u32(c, 0u);
            op(c, wasm_op::end);

            // br_if
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::br_if);
            u32(c, 0u);
            op(c, wasm_op::end);

            // br_table (two nested empty blocks)
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);  // outer
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);  // inner
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::br_table);
            u32(c, 2u);  // vec len
            u32(c, 0u);  // label0 = inner
            u32(c, 1u);  // label1 = outer
            u32(c, 0u);  // default = inner
            op(c, wasm_op::end);  // end inner
            op(c, wasm_op::end);  // end outer

            // unreachable (trap if executed)
            op(c, wasm_op::unreachable);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: direct call + call_indirect + drop + return (not executed).
        // - call 0: ()->()
        // - call 1: ()->(i32)
        // - call_indirect: typeidx=0 (()->()), tableidx=0
        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::call_indirect);
            u32(c, 0u);  // typeidx
            u32(c, 0u);  // tableidx

            op(c, wasm_op::call);
            u32(c, 0u);

            op(c, wasm_op::call);
            u32(c, 1u);
            op(c, wasm_op::drop);

            op(c, wasm_op::return_);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: locals/globals/select sampler (not executed).
        {
            func_type ty{{}, {}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local0: i32
            auto& c = fb.code;

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

            // select (i32)
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

        // f5: memory load/store variants (not executed).
        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c = fb.code;

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

            // Full-width loads.
            emit_load_drop(wasm_op::i32_load, 2u, 0u);
            emit_load_drop(wasm_op::i64_load, 3u, 0u);
            emit_load_drop(wasm_op::f32_load, 2u, 0u);
            emit_load_drop(wasm_op::f64_load, 3u, 0u);

            // Narrow loads.
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

            // Stores.
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

        // f6: memory.size/memory.grow (not executed).
        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c = fb.code;

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

        // f7: i32 arithmetic/comparison sampler (not executed).
        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c = fb.code;

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

            // comparisons (all return i32)
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

        // f8: i64 arithmetic/comparison sampler (not executed).
        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c = fb.code;

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

            // comparisons (all return i32)
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

        // f9: f32 arithmetic/comparison sampler (not executed).
        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c = fb.code;

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

            // comparisons (all return i32)
            f32_binop_drop(wasm_op::f32_eq);
            f32_binop_drop(wasm_op::f32_ne);
            f32_binop_drop(wasm_op::f32_lt);
            f32_binop_drop(wasm_op::f32_gt);
            f32_binop_drop(wasm_op::f32_le);
            f32_binop_drop(wasm_op::f32_ge);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f10: f64 arithmetic/comparison sampler (not executed).
        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c = fb.code;

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

            // comparisons (all return i32)
            f64_binop_drop(wasm_op::f64_eq);
            f64_binop_drop(wasm_op::f64_ne);
            f64_binop_drop(wasm_op::f64_lt);
            f64_binop_drop(wasm_op::f64_gt);
            f64_binop_drop(wasm_op::f64_le);
            f64_binop_drop(wasm_op::f64_ge);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f11: conversions + reinterpret sampler (not executed).
        {
            func_type ty{{}, {}};
            func_body fb{};
            auto& c = fb.code;

            // i32.wrap_i64
            op(c, wasm_op::i64_const);
            i64(c, 1);
            op(c, wasm_op::i32_wrap_i64);
            op(c, wasm_op::drop);

            // i64.extend_i32_{s,u}
            op(c, wasm_op::i32_const);
            i32(c, -1);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, -1);
            op(c, wasm_op::i64_extend_i32_u);
            op(c, wasm_op::drop);

            // int<->float conversions (pick safe in-range values).
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

            // reinterpret
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::i32_reinterpret_f32);
            op(c, wasm_op::drop);

            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::i64_reinterpret_f64);
            op(c, wasm_op::drop);

            op(c, wasm_op::i32_const);
            i32(c, 0x3f800000);  // 1.0f bits
            op(c, wasm_op::f32_reinterpret_i32);
            op(c, wasm_op::drop);

            op(c, wasm_op::i64_const);
            i64(c, 0x3ff0000000000000ll);  // 1.0 double bits
            op(c, wasm_op::f64_reinterpret_i64);
            op(c, wasm_op::drop);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_only_with_runtime_log(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        (void)compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        return 0;
    }

    [[nodiscard]] int test_translate_runtime_log_full_mvp()
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto const wasm = build_runtime_log_full_mvp_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_runtime_log_full_mvp");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Enable translator runtime-log and discard output to avoid spam.
#if defined(_WIN32) || defined(_WIN64)
        ::uwvm2::uwvm::io::u8runtime_log_output.reopen(u8"NUL", ::fast_io::open_mode::out);
#else
        ::uwvm2::uwvm::io::u8runtime_log_output.reopen(u8"/dev/null", ::fast_io::open_mode::out);
#endif
        ::uwvm2::uwvm::io::enable_runtime_log = true;

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        // tailcall
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_runtime_log_full_mvp();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}


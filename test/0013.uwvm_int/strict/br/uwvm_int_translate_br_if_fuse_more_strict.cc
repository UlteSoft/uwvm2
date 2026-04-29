#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i64(::std::int64_t v)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(v), 8);
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

    [[nodiscard]] byte_vec pack_mem_f32_lt_args(::std::int32_t addr, float rhs)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(addr), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(rhs), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_mem_i32_eqimm_args(::std::int32_t addr, ::std::int32_t v)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(addr), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(v), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_br_if_fuse_more_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        auto add_brif_i32_cmp2 = [&](wasm_op cmp_op, ::std::int32_t taken_ret, ::std::int32_t fall_ret) -> void
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, taken_ret);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, cmp_op);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, fall_ret);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f0: i32.ne + br_if fusion candidate.
        add_brif_i32_cmp2(wasm_op::i32_ne, 111, 222);
        // f1: i32.gt_s + br_if fusion candidate.
        add_brif_i32_cmp2(wasm_op::i32_gt_s, 123, 234);
        // f2: i32.le_u + br_if fusion candidate.
        add_brif_i32_cmp2(wasm_op::i32_le_u, 345, 456);

        // f3: local.get0; local.get1; i32.and; br_if
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 567);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            // Barrier: avoid `i32_and_2localget` (extra-heavy) so `i32.and ; br_if` fusion can trigger consistently.
            op(c, wasm_op::nop);
            op(c, wasm_op::i32_and);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 678);

            op(c, wasm_op::end);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: i64.eqz + br_if
        {
            func_type ty{{k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 901);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i64_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 902);

            op(c, wasm_op::end);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: i64.gt_u + br_if
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 903);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i64_gt_u);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 904);

            op(c, wasm_op::end);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: f32.eq + br_if
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 911);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_eq);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 922);

            op(c, wasm_op::end);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: f32.lt_localget_rhs (const lhs, local.get rhs) + br_if
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 933);

            op(c, wasm_op::f32_const);
            f32(c, 1.5f);
            // Barrier: prevent forming `const_f32_localget` pending kind; we want `local.get` to be delayed alone (localget_rhs cmp).
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_lt);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 944);

            op(c, wasm_op::end);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: f64.ge_localget_rhs (const lhs, local.get rhs) + br_if
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 955);

            op(c, wasm_op::f64_const);
            f64(c, 2.0);
            // Barrier: prevent forming `const_f64_localget` pending kind; we want `local.get` to be delayed alone (localget_rhs cmp).
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_ge);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 966);

            op(c, wasm_op::end);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f9: f32.load(local.get addr) ; local.get rhs ; f32.lt ; br_if
        {
            func_type ty{{k_val_i32, k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // store 1.5f at [addr]
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 1.5f);
            op(c, wasm_op::f32_store);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 977);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_load);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_lt);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 988);

            op(c, wasm_op::end);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f10: i32.load(local.get addr) == 7 ; br_if
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // store v at [addr]
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_store);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 999);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_load);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::i32_eq);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 1000);

            op(c, wasm_op::end);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f11: f64.lt ; i32.eqz ; br_if  (eqz-skip fusion, non-NaN => fallthrough sets out=111)
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local0 out
            auto& c = fb.code;

            // out = 222
            op(c, wasm_op::i32_const);
            i32(c, 222);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);

            op(c, wasm_op::f64_const);
            f64(c, 0.0);
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::f64_lt);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            // fallthrough: out = 111
            op(c, wasm_op::i32_const);
            i32(c, 111);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            op(c, wasm_op::end);  // end block

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f12: f64.lt ; i32.eqz ; br_if  (eqz-skip fusion, NaN => taken, out stays 222)
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local0 out
            auto& c = fb.code;

            double const qnan = ::std::bit_cast<double>(0x7ff8000000000001ull);

            // out = 222
            op(c, wasm_op::i32_const);
            i32(c, 222);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);

            op(c, wasm_op::f64_const);
            f64(c, qnan);
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::f64_lt);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            // fallthrough: out = 111 (should be skipped due to NaN)
            op(c, wasm_op::i32_const);
            i32(c, 111);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            op(c, wasm_op::end);  // end block

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f13: peephole boolean-normalize: i32.lt_s; i32.const 1; i32.and; i32.eqz; br_if  (should fuse to br_if_i32_ge_s)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 700);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_lt_s);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_and);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 701);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f14: peephole boolean-normalize: i32.lt_u; i32.const 1; i32.and; i32.eqz; br_if  (should fuse to br_if_i32_ge_u)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 702);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_lt_u);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_and);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 703);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_brif_fuse_more_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0 i32.ne
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32x2(1, 2),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32x2(3, 3),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f1 i32.gt_s
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32x2(5, 3),
                                              nullptr,
                                              nullptr)
                                       .results) == 123);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32x2(-1, 10),
                                              nullptr,
                                              nullptr)
                                       .results) == 234);

        // f2 i32.le_u
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32x2(9, 10),
                                              nullptr,
                                              nullptr)
                                       .results) == 345);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32x2(11, 10),
                                              nullptr,
                                              nullptr)
                                       .results) == 456);

        // f3 i32.and_nz
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32x2(0x10, 0x01),
                                              nullptr,
                                              nullptr)
                                       .results) == 678);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32x2(0x10, 0x11),
                                              nullptr,
                                              nullptr)
                                       .results) == 567);

        // f4 i64.eqz
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i64(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 901);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i64(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 902);

        // f5 i64.gt_u
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_i64x2(10, 9),
                                              nullptr,
                                              nullptr)
                                       .results) == 903);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_i64x2(9, 10),
                                              nullptr,
                                              nullptr)
                                       .results) == 904);

        // f6 f32.eq
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_f32x2(1.5f, 1.5f),
                                              nullptr,
                                              nullptr)
                                       .results) == 911);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_f32x2(1.5f, 2.25f),
                                              nullptr,
                                              nullptr)
                                       .results) == 922);

        // f7 f32.lt_localget_rhs: 1.5 < rhs ?
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_f32(2.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 933);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_f32(1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 944);

        // f8 f64.ge_localget_rhs: 2.0 >= rhs ?
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(8),
                                              rt.local_defined_function_vec_storage.index_unchecked(8),
                                              pack_f64(1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 955);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(8),
                                              rt.local_defined_function_vec_storage.index_unchecked(8),
                                              pack_f64(3.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 966);

        // f9 memory f32.load < rhs ?
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(9),
                                              rt.local_defined_function_vec_storage.index_unchecked(9),
                                              pack_mem_f32_lt_args(0, 2.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 977);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(9),
                                              rt.local_defined_function_vec_storage.index_unchecked(9),
                                              pack_mem_f32_lt_args(0, 1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 988);

        // f10 memory i32.load == 7 ?
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(10),
                                              rt.local_defined_function_vec_storage.index_unchecked(10),
                                              pack_mem_i32_eqimm_args(0, 7),
                                              nullptr,
                                              nullptr)
                                       .results) == 999);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(10),
                                              rt.local_defined_function_vec_storage.index_unchecked(10),
                                              pack_mem_i32_eqimm_args(0, 8),
                                              nullptr,
                                              nullptr)
                                      .results) == 1000);

        // f11 f64.lt+eqz (non-NaN) => fallthrough sets out=111
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(11),
                                              rt.local_defined_function_vec_storage.index_unchecked(11),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);

        // f12 f64.lt+eqz (NaN) => taken, out stays 222
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(12),
                                              rt.local_defined_function_vec_storage.index_unchecked(12),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f13 peephole lt_s->ge_s
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(13),
                                              rt.local_defined_function_vec_storage.index_unchecked(13),
                                              pack_i32x2(5, 3),
                                              nullptr,
                                              nullptr)
                                       .results) == 700);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(13),
                                              rt.local_defined_function_vec_storage.index_unchecked(13),
                                              pack_i32x2(-1, 10),
                                              nullptr,
                                              nullptr)
                                       .results) == 701);

        // f14 peephole lt_u->ge_u
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(14),
                                              rt.local_defined_function_vec_storage.index_unchecked(14),
                                              pack_i32x2(static_cast<::std::int32_t>(static_cast<::std::uint32_t>(0xffffffffu)), 0),
                                              nullptr,
                                              nullptr)
                                       .results) == 702);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(14),
                                              rt.local_defined_function_vec_storage.index_unchecked(14),
                                              pack_i32x2(0, 1),
                                              nullptr,
                                              nullptr)
                                       .results) == 703);

        return 0;
    }

    [[nodiscard]] int test_translate_br_if_fuse_more() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_if_fuse_more_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_if_fuse_more");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall mode: check fused br_if opfuncs when combine is enabled.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            constexpr auto exp_i32_ne = optable::translate::get_uwvmint_br_if_i32_ne_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_i32_gt_s = optable::translate::get_uwvmint_br_if_i32_gt_s_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_i32_le_u = optable::translate::get_uwvmint_br_if_i32_le_u_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_i32_and_nz = optable::translate::get_uwvmint_br_if_i32_and_nz_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_i64_eqz = optable::translate::get_uwvmint_br_if_i64_local_eqz_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_i64_gt_u = optable::translate::get_uwvmint_br_if_i64_gt_u_fptr_from_tuple<opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_ne));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i32_gt_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_i32_le_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_i32_and_nz));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_i64_eqz));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_i64_gt_u));
# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
            constexpr auto exp_i32_ge_s = optable::translate::get_uwvmint_br_if_i32_ge_s_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_i32_ge_u = optable::translate::get_uwvmint_br_if_i32_ge_u_fptr_from_tuple<opt>(curr, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(13).op.operands, exp_i32_ge_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(14).op.operands, exp_i32_ge_u));
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
	            {
	                constexpr optable::uwvm_interpreter_stacktop_currpos_t curr2{};
	                constexpr auto tuple2 =
	                    compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
	                constexpr auto exp_f32_eq = optable::translate::get_uwvmint_br_if_f32_eq_fptr_from_tuple<opt>(curr2, tuple2);
# else
	                constexpr auto exp_f32_eq_rhs = optable::translate::get_uwvmint_br_if_f32_eq_localget_rhs_fptr_from_tuple<opt>(curr2, tuple2);
# endif
	                constexpr auto exp_f32_lt_rhs = optable::translate::get_uwvmint_br_if_f32_lt_localget_rhs_fptr_from_tuple<opt>(curr2, tuple2);
	                constexpr auto exp_f64_ge_rhs = optable::translate::get_uwvmint_br_if_f64_ge_localget_rhs_fptr_from_tuple<opt>(curr2, tuple2);
	                constexpr auto exp_f64_lt_eqz = optable::translate::get_uwvmint_br_if_f64_lt_eqz_fptr_from_tuple<opt>(curr2, tuple2);

# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
	                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_f32_eq));
# else
	                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_f32_eq_rhs));
# endif
	                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_f32_lt_rhs));
	                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_f64_ge_rhs));
	                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(11).op.operands, exp_f64_lt_eqz));
	                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(12).op.operands, exp_f64_lt_eqz));

                // Memory mega-fusions are tailcall-only and memory-specialized.
                auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;

                auto const exp_mem_f32_lt_rhs =
                    optable::translate::get_uwvmint_br_if_f32_load_localget_off_lt_localget_rhs_fptr_from_tuple<opt>(curr2, mem, tuple2);
                auto const exp_mem_i32_eq_imm =
                    optable::translate::get_uwvmint_br_if_i32_load_localget_off_eq_imm_fptr_from_tuple<opt>(curr2, mem, tuple2);

                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(9).op.operands, exp_mem_f32_lt_rhs));
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(10).op.operands, exp_mem_i32_eq_imm));
            }
#endif

            UWVM2TEST_REQUIRE(run_brif_fuse_more_suite<opt>(rt) == 0);
        }

        // Byref mode: semantics.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_brif_fuse_more_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_br_if_fuse_more();
}

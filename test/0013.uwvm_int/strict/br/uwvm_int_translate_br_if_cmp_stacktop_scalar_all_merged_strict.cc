#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_br_if_cmp_stacktop_scalar_all_merged_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // Pattern: `cmp; br_if` with stacktop caching enabled and ALL scalar rings merged (i32/i64/f32/f64 share the same range).
        // Each function returns:
        // - taken  => 111
        // - fallthrough => 222

        // f0: (param i64) -> i64.eqz; br_if
        {
            func_type ty{{k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i64_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (param i64) -> (x <u 10) ; br_if
        {
            func_type ty{{k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i64_const);
            i64(c, 10);
            op(c, wasm_op::i64_lt_u);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: (param i64) -> (x >u 10) ; br_if
        {
            func_type ty{{k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i64_const);
            i64(c, 10);
            op(c, wasm_op::i64_gt_u);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: (param f32) -> (x < 0.0) ; br_if
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 0.0f);
            op(c, wasm_op::f32_lt);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: (param f32) -> (x == 0.0) ; br_if
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 0.0f);
            op(c, wasm_op::f32_eq);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: (param f64) -> (x < 0.0) ; br_if
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            f64(c, 0.0);
            op(c, wasm_op::f64_lt);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: (param f32) -> (x > 0.0) ; br_if
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 0.0f);
            op(c, wasm_op::f32_gt);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: (param f32) -> (x != 0.0) ; br_if
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 0.0f);
            op(c, wasm_op::f32_ne);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: (param f32) -> (x <= 0.0) ; br_if
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 0.0f);
            op(c, wasm_op::f32_le);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f9: (param f32) -> (x >= 0.0) ; br_if
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 0.0f);
            op(c, wasm_op::f32_ge);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f10: (param f64) -> (x == 0.0) ; br_if
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            f64(c, 0.0);
            op(c, wasm_op::f64_eq);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f11: (param f64) -> (x != 0.0) ; br_if
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            f64(c, 0.0);
            op(c, wasm_op::f64_ne);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f12: (param f64) -> (x > 0.0) ; br_if
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            f64(c, 0.0);
            op(c, wasm_op::f64_gt);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f13: (param f64) -> (x <= 0.0) ; br_if
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            f64(c, 0.0);
            op(c, wasm_op::f64_le);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f14: (param f64) -> (x >= 0.0) ; br_if
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            f64(c, 0.0);
            op(c, wasm_op::f64_ge);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_br_if_cmp_stacktop_scalar_all_merged_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0: i64.eqz
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i64(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i64(7),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f1: i64.lt_u (x < 10)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i64(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i64(20),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f2: i64.gt_u (x > 10)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i64(20),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i64(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f3: f32.lt (x < 0.0)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_f32(-1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_f32(1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f4: f32.eq (x == 0.0)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_f32(0.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_f32(1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f5: f64.lt (x < 0.0)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_f64(-1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_f64(1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f6: f32.gt (x > 0.0)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_f32(1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_f32(-1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f7: f32.ne (x != 0.0)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_f32(1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_f32(0.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f8: f32.le (x <= 0.0)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(8),
                                              rt.local_defined_function_vec_storage.index_unchecked(8),
                                              pack_f32(-1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(8),
                                              rt.local_defined_function_vec_storage.index_unchecked(8),
                                              pack_f32(1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f9: f32.ge (x >= 0.0)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(9),
                                              rt.local_defined_function_vec_storage.index_unchecked(9),
                                              pack_f32(1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(9),
                                              rt.local_defined_function_vec_storage.index_unchecked(9),
                                              pack_f32(-1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f10: f64.eq (x == 0.0)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(10),
                                              rt.local_defined_function_vec_storage.index_unchecked(10),
                                              pack_f64(0.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(10),
                                              rt.local_defined_function_vec_storage.index_unchecked(10),
                                              pack_f64(1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f11: f64.ne (x != 0.0)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(11),
                                              rt.local_defined_function_vec_storage.index_unchecked(11),
                                              pack_f64(1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(11),
                                              rt.local_defined_function_vec_storage.index_unchecked(11),
                                              pack_f64(0.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f12: f64.gt (x > 0.0)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(12),
                                              rt.local_defined_function_vec_storage.index_unchecked(12),
                                              pack_f64(1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(12),
                                              rt.local_defined_function_vec_storage.index_unchecked(12),
                                              pack_f64(-1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f13: f64.le (x <= 0.0)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(13),
                                              rt.local_defined_function_vec_storage.index_unchecked(13),
                                              pack_f64(-1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(13),
                                              rt.local_defined_function_vec_storage.index_unchecked(13),
                                              pack_f64(1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f14: f64.ge (x >= 0.0)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(14),
                                              rt.local_defined_function_vec_storage.index_unchecked(14),
                                              pack_f64(1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(14),
                                              rt.local_defined_function_vec_storage.index_unchecked(14),
                                              pack_f64(-1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        return 0;
    }

    [[nodiscard]] int test_translate_br_if_cmp_stacktop_scalar_all_merged() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_if_cmp_stacktop_scalar_all_merged_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_if_cmp_stacktop_scalar_all_merged");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Byref mode: semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_br_if_cmp_stacktop_scalar_all_merged_suite<opt>(rt) == 0);
        }

        // Tailcall: baseline.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_br_if_cmp_stacktop_scalar_all_merged_suite<opt>(rt) == 0);
        }

        // Tailcall + stacktop caching: fully merged scalar ring (i32/i64/f32/f64).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 5uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 5uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_br_if_cmp_stacktop_scalar_all_merged_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_br_if_cmp_stacktop_scalar_all_merged();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}

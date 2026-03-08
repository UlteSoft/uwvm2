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

    [[nodiscard]] byte_vec pack_i32_i64(::std::int32_t a, ::std::int64_t b)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] byte_vec pack_i32_f32(::std::int32_t a, float b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i32_f64(::std::int32_t a, double b)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] byte_vec build_memory_localget_fuse_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: local.get addr ; i32.load  (tests i32_load_localget_off)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_const);
            i32(c, static_cast<::std::int32_t>(0x11223344u));
            op(c, wasm_op::i32_store);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_load);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: local.get addr ; i32.load ; i32.const 7 ; i32.add  (tests i32_load_add_imm)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_const);
            i32(c, 100);
            op(c, wasm_op::i32_store);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_load);
            u32(c, 2u);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: local.get addr ; i32.load ; i32.const 0xff ; i32.and  (tests i32_load_and_imm)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 4);
            op(c, wasm_op::i32_const);
            i32(c, static_cast<::std::int32_t>(0xffff00ffu));
            op(c, wasm_op::i32_store);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_load);
            u32(c, 2u);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 0xff);
            op(c, wasm_op::i32_and);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: local.get base ; i32.const 4 ; i32.add ; i32.load  (tests i32_load_local_plus_imm)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 16);
            op(c, wasm_op::i32_const);
            i32(c, 55);
            op(c, wasm_op::i32_store);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 4);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_load);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: local.get addr ; i64.load  (tests i64_load_localget_off)
        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 32);
            op(c, wasm_op::i64_const);
            i64(c, static_cast<::std::int64_t>(0x0123456789abcdefull));
            op(c, wasm_op::i64_store);
            u32(c, 3u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i64_load);
            u32(c, 3u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: local.get addr ; i64.load ; local.set 1  (tests i64_load_localget_set_local)
        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});  // local1
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 40);
            op(c, wasm_op::i64_const);
            i64(c, static_cast<::std::int64_t>(0x0f1e2d3c4b5a6978ull));
            op(c, wasm_op::i64_store);
            u32(c, 3u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i64_load);
            u32(c, 3u);
            u32(c, 0u);
            op(c, wasm_op::local_set);
            u32(c, 1u);
            op(c, wasm_op::local_get);
            u32(c, 1u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: local.get addr ; f32.load  (tests f32_load_localget_off)
        {
            func_type ty{{k_val_i32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 48);
            op(c, wasm_op::f32_const);
            f32(c, 1.5f);
            op(c, wasm_op::f32_store);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_load);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: local.get base ; i32.const 4 ; i32.add ; f32.load  (tests f32_load_local_plus_imm)
        {
            func_type ty{{k_val_i32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 64);
            op(c, wasm_op::f32_const);
            f32(c, 2.25f);
            op(c, wasm_op::f32_store);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 4);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::f32_load);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: local.get addr ; f64.load  (tests f64_load_localget_off)
        {
            func_type ty{{k_val_i32}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 72);
            op(c, wasm_op::f64_const);
            f64(c, 6.5);
            op(c, wasm_op::f64_store);
            u32(c, 3u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_load);
            u32(c, 3u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f9: local.get base ; i32.const 8 ; i32.add ; f64.load  (tests f64_load_local_plus_imm)
        {
            func_type ty{{k_val_i32}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 88);
            op(c, wasm_op::f64_const);
            f64(c, -3.25);
            op(c, wasm_op::f64_store);
            u32(c, 3u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 8);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::f64_load);
            u32(c, 3u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f10: local.get addr ; local.get v ; i32.store  (tests i32_store_localget_off)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_store);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_load);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f11: local.get addr ; i32.const imm ; i32.store  (tests i32_store_imm_localget_off)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, static_cast<::std::int32_t>(0x12345678u));
            op(c, wasm_op::i32_store);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_load);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f12: local.get base ; i32.const 8 ; i32.add ; local.get v ; i32.store  (tests i32_store_local_plus_imm)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 8);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_store);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 8);
            op(c, wasm_op::i32_load);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f13: local.get base ; i32.const 4 ; i32.add ; local.get v ; f32.store  (tests f32_store_local_plus_imm)
        {
            func_type ty{{k_val_i32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 4);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_store);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 4);
            op(c, wasm_op::f32_load);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f14: local.get base ; i32.const 8 ; i32.add ; local.get v ; f64.store  (tests f64_store_local_plus_imm)
        {
            func_type ty{{k_val_i32, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 8);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_store);
            u32(c, 3u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 8);
            op(c, wasm_op::f64_load);
            u32(c, 3u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f15: local.get addr ; local.get v ; i64.store  (tests i64_store_localget_off)
        {
            func_type ty{{k_val_i32, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i64_store);
            u32(c, 3u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i64_load);
            u32(c, 3u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_memory_localget_fuse_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        using Runner = interpreter_runner<Opt>;

        // f0: i32_load_localget_off
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(0),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == 0x11223344u);
        }

        // f1: i32_load_add_imm
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32(0),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 107);
        }

        // f2: i32_load_and_imm
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_i32(4),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == 0xffu);
        }

        // f3: i32_load_local_plus_imm
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(3),
                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                  pack_i32(12),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 55);
        }

        // f4: i64_load_localget_off
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(4),
                                  rt.local_defined_function_vec_storage.index_unchecked(4),
                                  pack_i32(32),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == 0x0123456789abcdefull);
        }

        // f5: i64_load_localget_set_local
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(5),
                                  rt.local_defined_function_vec_storage.index_unchecked(5),
                                  pack_i32(40),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == 0x0f1e2d3c4b5a6978ull);
        }

        // f6: f32_load_localget_off
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(6),
                                  rt.local_defined_function_vec_storage.index_unchecked(6),
                                  pack_i32(48),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr.results) == 1.5f);
        }

        // f7: f32_load_local_plus_imm
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(7),
                                  rt.local_defined_function_vec_storage.index_unchecked(7),
                                  pack_i32(60),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr.results) == 2.25f);
        }

        // f8: f64_load_localget_off
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(8),
                                  rt.local_defined_function_vec_storage.index_unchecked(8),
                                  pack_i32(72),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr.results) == 6.5);
        }

        // f9: f64_load_local_plus_imm
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(9),
                                  rt.local_defined_function_vec_storage.index_unchecked(9),
                                  pack_i32(80),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr.results) == -3.25);
        }

        // f10: i32_store_localget_off
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(10),
                                  rt.local_defined_function_vec_storage.index_unchecked(10),
                                  pack_i32x2(0, static_cast<::std::int32_t>(0x55667788u)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == 0x55667788u);
        }

        // f11: i32_store_imm_localget_off
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(11),
                                  rt.local_defined_function_vec_storage.index_unchecked(11),
                                  pack_i32(0),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == 0x12345678u);
        }

        // f12: i32_store_local_plus_imm
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(12),
                                  rt.local_defined_function_vec_storage.index_unchecked(12),
                                  pack_i32x2(0, 77),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 77);
        }

        // f13: f32_store_local_plus_imm
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(13),
                                  rt.local_defined_function_vec_storage.index_unchecked(13),
                                  pack_i32_f32(0, 1.25f),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr.results) == 1.25f);
        }

        // f14: f64_store_local_plus_imm
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(14),
                                  rt.local_defined_function_vec_storage.index_unchecked(14),
                                  pack_i32_f64(0, 9.5),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr.results) == 9.5);
        }

        // f15: i64_store_localget_off
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(15),
                                  rt.local_defined_function_vec_storage.index_unchecked(15),
                                  pack_i32_i64(0, static_cast<::std::int64_t>(0x1122334455667788ull)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == 0x1122334455667788ull);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_memory_localget_fuse() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_localget_fuse_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_localget_fuse");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
        [[maybe_unused]] auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr auto curr{make_initial_stacktop_currpos<opt>()};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            auto const exp_i32_load_localget_off =
                optable::translate::get_uwvmint_i32_load_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_i32_load_add_imm = optable::translate::get_uwvmint_i32_load_add_imm_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_i32_load_and_imm = optable::translate::get_uwvmint_i32_load_and_imm_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_i32_load_local_plus_imm =
                optable::translate::get_uwvmint_i32_load_local_plus_imm_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_i64_load_localget_off = optable::translate::get_uwvmint_i64_load_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_i64_load_localget_set_local =
                optable::translate::get_uwvmint_i64_load_localget_set_local_fptr_from_tuple<opt>(curr, mem, tuple);

            auto const exp_f32_load_localget_off = optable::translate::get_uwvmint_f32_load_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_f64_load_localget_off = optable::translate::get_uwvmint_f64_load_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);

            auto const exp_i32_store_localget_off =
                optable::translate::get_uwvmint_i32_store_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_i32_store_imm_localget_off =
                optable::translate::get_uwvmint_i32_store_imm_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_i32_store_local_plus_imm =
                optable::translate::get_uwvmint_i32_store_local_plus_imm_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_i64_store_localget_off =
                optable::translate::get_uwvmint_i64_store_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_load_localget_off));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i32_load_add_imm));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_i32_load_and_imm));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_i32_load_local_plus_imm));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_i64_load_localget_off));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_i64_load_localget_set_local));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_f32_load_localget_off));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_f64_load_localget_off));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(10).op.operands, exp_i32_store_localget_off));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(11).op.operands, exp_i32_store_imm_localget_off));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(12).op.operands, exp_i32_store_local_plus_imm));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(15).op.operands, exp_i64_store_localget_off));

# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
            auto const exp_f32_load_local_plus_imm =
                optable::translate::get_uwvmint_f32_load_local_plus_imm_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_f64_load_local_plus_imm =
                optable::translate::get_uwvmint_f64_load_local_plus_imm_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_f32_store_local_plus_imm =
                optable::translate::get_uwvmint_f32_store_local_plus_imm_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_f64_store_local_plus_imm =
                optable::translate::get_uwvmint_f64_store_local_plus_imm_fptr_from_tuple<opt>(curr, mem, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_f32_load_local_plus_imm));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(9).op.operands, exp_f64_load_local_plus_imm));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(13).op.operands, exp_f32_store_local_plus_imm));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(14).op.operands, exp_f64_store_local_plus_imm));
# endif
#endif

            UWVM2TEST_REQUIRE(run_memory_localget_fuse_suite<opt>(cm, rt) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE(run_memory_localget_fuse_suite<opt>(cm, rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE(run_memory_localget_fuse_suite<opt>(cm, rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE(run_memory_localget_fuse_suite<opt>(cm, rt) == 0);
        }

        if(legacy_layouts_enabled())
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
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE(run_memory_localget_fuse_suite<opt>(cm, rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_memory_localget_fuse();
}

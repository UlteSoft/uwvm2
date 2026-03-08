#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_uint_float_convert_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: f32.convert_i32_u
        {
            func_type ty{{k_val_i32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_convert_i32_u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: f64.convert_i32_u
        {
            func_type ty{{k_val_i32}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_convert_i32_u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: f32.convert_i64_u
        {
            func_type ty{{k_val_i64}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_convert_i64_u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: f64.convert_i64_u
        {
            func_type ty{{k_val_i64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_convert_i64_u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: i32.trunc_f32_u
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_trunc_f32_u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: i32.trunc_f64_u
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_trunc_f64_u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: i64.trunc_f32_u
        {
            func_type ty{{k_val_f32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_trunc_f32_u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: i64.trunc_f64_u
        {
            func_type ty{{k_val_f64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_trunc_f64_u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: f32.convert_i64_s
        {
            func_type ty{{k_val_i64}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_convert_i64_s);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f9: f64.convert_i64_s
        {
            func_type ty{{k_val_i64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_convert_i64_s);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f10: f64.convert_i32_s
        {
            func_type ty{{k_val_i32}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_convert_i32_s);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f11: i64.trunc_f64_s
        {
            func_type ty{{k_val_f64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_trunc_f64_s);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0: f32.convert_i32_u
        {
            ::std::int32_t x = static_cast<::std::int32_t>(0x80000000u);  // unsigned = 2147483648
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(x),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr.results) == 2147483648.0f);
        }

        // f1: f64.convert_i32_u
        {
            ::std::int32_t x = -1;  // unsigned = 4294967295
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32(x),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr.results) == 4294967295.0);
        }

        // f2: f32.convert_i64_u
        {
            ::std::int64_t x = static_cast<::std::int64_t>(0x8000000000000000ull);  // unsigned = 2^63
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_i64(x),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr.results) == 9223372036854775808.0f);
        }

        // f3: f64.convert_i64_u
        {
            ::std::int64_t x = static_cast<::std::int64_t>(0x8000000000000000ull);  // unsigned = 2^63
            auto rr = Runner::run(cm.local_funcs.index_unchecked(3),
                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                  pack_i64(x),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr.results) == 9223372036854775808.0);
        }

        // f4: i32.trunc_f32_u
        {
            float v = 2147483648.0f;  // 2^31
            auto rr = Runner::run(cm.local_funcs.index_unchecked(4),
                                  rt.local_defined_function_vec_storage.index_unchecked(4),
                                  pack_f32(v),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == 0x80000000u);
        }

        // f5: i32.trunc_f64_u
        {
            double v = 4294967295.0;  // 2^32-1 (exact in f64)
            auto rr = Runner::run(cm.local_funcs.index_unchecked(5),
                                  rt.local_defined_function_vec_storage.index_unchecked(5),
                                  pack_f64(v),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == 0xffffffffu);
        }

        // f6: i64.trunc_f32_u
        {
            float v = 9223372036854775808.0f;  // 2^63
            auto rr = Runner::run(cm.local_funcs.index_unchecked(6),
                                  rt.local_defined_function_vec_storage.index_unchecked(6),
                                  pack_f32(v),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == 0x8000000000000000ull);
        }

        // f7: i64.trunc_f64_u
        {
            double v = 9223372036854775808.0;  // 2^63
            auto rr = Runner::run(cm.local_funcs.index_unchecked(7),
                                  rt.local_defined_function_vec_storage.index_unchecked(7),
                                  pack_f64(v),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == 0x8000000000000000ull);
        }

        // f8/f9: i64 signed conversions
        for(::std::int64_t x : {0ll, 1ll, -1ll, 7ll, -7ll})
        {
            auto rr8 = Runner::run(cm.local_funcs.index_unchecked(8),
                                   rt.local_defined_function_vec_storage.index_unchecked(8),
                                   pack_i64(x),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr8.results) == static_cast<float>(x));

            auto rr9 = Runner::run(cm.local_funcs.index_unchecked(9),
                                   rt.local_defined_function_vec_storage.index_unchecked(9),
                                   pack_i64(x),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr9.results) == static_cast<double>(x));
        }

        // f10: f64.convert_i32_s
        for(::std::int32_t x : {0, 1, -1, 12345, -12345})
        {
            auto rr10 = Runner::run(cm.local_funcs.index_unchecked(10),
                                    rt.local_defined_function_vec_storage.index_unchecked(10),
                                    pack_i32(x),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr10.results) == static_cast<double>(x));
        }

        // f11: i64.trunc_f64_s
        for(double v : {0.0, 1.0, -1.0, 7.0, -7.0, 12345.0, -12345.0})
        {
            auto rr11 = Runner::run(cm.local_funcs.index_unchecked(11),
                                    rt.local_defined_function_vec_storage.index_unchecked(11),
                                    pack_f64(v),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i64(rr11.results) == static_cast<::std::int64_t>(v));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_uint_float_convert() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_uint_float_convert_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_uint_float_convert");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr auto opt{make_tailcall_hardfloat_abi_opt<2uz, 2uz>()};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_uint_float_convert();
}

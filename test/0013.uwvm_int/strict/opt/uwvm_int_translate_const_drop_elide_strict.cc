#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    constexpr ::std::size_t k_run = 256uz;

    [[nodiscard]] byte_vec build_const_drop_elide_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        auto drops = [&](byte_vec& c, ::std::size_t n)
        {
            for(::std::size_t i{}; i != n; ++i) { op(c, wasm_op::drop); }
        };

        // f0: i32.const* + drop* => elided, then return 77
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            for(::std::size_t i{}; i != k_run; ++i)
            {
                op(c, wasm_op::i32_const);
                i32(c, static_cast<::std::int32_t>(i));
            }
            drops(c, k_run);

            op(c, wasm_op::i32_const);
            i32(c, 77);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i64.const* + drop* => elided, then return 88
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            for(::std::size_t i{}; i != k_run; ++i)
            {
                op(c, wasm_op::i64_const);
                i64(c, static_cast<::std::int64_t>(i) * 7);
            }
            drops(c, k_run);

            op(c, wasm_op::i32_const);
            i32(c, 88);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: f32.const* + drop* => elided, then return 99
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            for(::std::size_t i{}; i != k_run; ++i)
            {
                op(c, wasm_op::f32_const);
                f32(c, static_cast<float>(i) + 0.25f);
            }
            drops(c, k_run);

            op(c, wasm_op::i32_const);
            i32(c, 99);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: f64.const* + drop* => elided, then return 111
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            for(::std::size_t i{}; i != k_run; ++i)
            {
                op(c, wasm_op::f64_const);
                f64(c, static_cast<double>(i) + 0.5);
            }
            drops(c, k_run);

            op(c, wasm_op::i32_const);
            i32(c, 111);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: local.get 0; (i32.const* + drop*) should be elided, leaving one extra drop for the param.
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);

            for(::std::size_t i{}; i != k_run; ++i)
            {
                op(c, wasm_op::i32_const);
                i32(c, 1000 + static_cast<::std::int32_t>(i));
            }
            drops(c, k_run);

            // extra drop: pops the param (should remain after const/drop elide).
            op(c, wasm_op::drop);

            op(c, wasm_op::i32_const);
            i32(c, 222);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_const_drop_elide_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        // Const+drop elide is a translation-time peephole. Without it, bytecode would scale with O(N).
        // With it, each function should be tiny.
        for(::std::size_t i{}; i != 5uz; ++i)
        {
            auto const& bc = cm.local_funcs.index_unchecked(i).op.operands;
            UWVM2TEST_REQUIRE(bc.size() < 1024uz);
        }

        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                               rt.local_defined_function_vec_storage.index_unchecked(0),
                                               pack_no_params(),
                                               nullptr,
                                               nullptr)
                                        .results) == 77);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                               rt.local_defined_function_vec_storage.index_unchecked(1),
                                               pack_no_params(),
                                               nullptr,
                                               nullptr)
                                        .results) == 88);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                               rt.local_defined_function_vec_storage.index_unchecked(2),
                                               pack_no_params(),
                                               nullptr,
                                               nullptr)
                                        .results) == 99);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                               rt.local_defined_function_vec_storage.index_unchecked(3),
                                               pack_no_params(),
                                               nullptr,
                                               nullptr)
                                        .results) == 111);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                               rt.local_defined_function_vec_storage.index_unchecked(4),
                                               pack_i32(7),
                                               nullptr,
                                               nullptr)
                                        .results) == 222);

        return 0;
    }

    [[nodiscard]] int test_translate_const_drop_elide() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        // No calls expected in this test.
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_const_drop_elide_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_const_drop_elide");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_const_drop_elide_suite<opt>(rt) == 0);
        }

        // Mode B: tailcall
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_const_drop_elide_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_const_drop_elide();
}

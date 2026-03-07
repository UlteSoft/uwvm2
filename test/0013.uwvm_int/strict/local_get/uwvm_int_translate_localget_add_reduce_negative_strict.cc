#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32x8(::std::int32_t a0,
                                      ::std::int32_t a1,
                                      ::std::int32_t a2,
                                      ::std::int32_t a3,
                                      ::std::int32_t a4,
                                      ::std::int32_t a5,
                                      ::std::int32_t a6,
                                      ::std::int32_t a7)
    {
        byte_vec out(32);
        ::std::int32_t const v[8]{a0, a1, a2, a3, a4, a5, a6, a7};
        ::std::memcpy(out.data(), v, sizeof(v));
        return out;
    }

    [[nodiscard]] byte_vec pack_i64x8(::std::int64_t a0,
                                      ::std::int64_t a1,
                                      ::std::int64_t a2,
                                      ::std::int64_t a3,
                                      ::std::int64_t a4,
                                      ::std::int64_t a5,
                                      ::std::int64_t a6,
                                      ::std::int64_t a7)
    {
        byte_vec out(64);
        ::std::int64_t const v[8]{a0, a1, a2, a3, a4, a5, a6, a7};
        ::std::memcpy(out.data(), v, sizeof(v));
        return out;
    }

    [[nodiscard]] byte_vec pack_f64x12(double a0,
                                       double a1,
                                       double a2,
                                       double a3,
                                       double a4,
                                       double a5,
                                       double a6,
                                       double a7,
                                       double a8,
                                       double a9,
                                       double a10,
                                       double a11)
    {
        byte_vec out(96);
        double const v[12]{a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11};
        ::std::memcpy(out.data(), v, sizeof(v));
        return out;
    }

    [[nodiscard]] byte_vec build_localget_add_reduce_negative_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: i32 local.get x8 + i32.add x6 + drop
        // Intended to enter heavy add-reduce lookahead, then fail at trailing-add parse (op != add_op).
        // Returns the first parameter (p0).
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            for(::std::uint32_t i{}; i != 8u; ++i)
            {
                op(c, wasm_op::local_get);
                u32(c, i);
            }
            for(int i{}; i != 6; ++i) { op(c, wasm_op::i32_add); }
            op(c, wasm_op::drop);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i64 local.get x8 + i64.add x6 + drop => p0
        {
            func_type ty{{k_val_i64, k_val_i64, k_val_i64, k_val_i64, k_val_i64, k_val_i64, k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            for(::std::uint32_t i{}; i != 8u; ++i)
            {
                op(c, wasm_op::local_get);
                u32(c, i);
            }
            for(int i{}; i != 6; ++i) { op(c, wasm_op::i64_add); }
            op(c, wasm_op::drop);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: f64 local.get x12 + f64.add x10 + drop => p0
        // Also forces f64 add-reduce lookahead attempts (12-local then 7-local), both should fail at trailing-add parse.
        {
            func_type ty{
                {k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64},
                {k_val_f64},
            };
            func_body fb{};
            auto& c = fb.code;

            for(::std::uint32_t i{}; i != 12u; ++i)
            {
                op(c, wasm_op::local_get);
                u32(c, i);
            }
            for(int i{}; i != 10; ++i) { op(c, wasm_op::f64_add); }
            op(c, wasm_op::drop);
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
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 3uz);

        using Runner = interpreter_runner<Opt>;

        // f0: i32 => p0
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32x8(111, 1, 2, 3, 4, 5, 6, 7),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 111);
        }

        // f1: i64 => p0
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i64x8(0x123456789ll, 1, 2, 3, 4, 5, 6, 7),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i64(rr.results) == 0x123456789ll);
        }

        // f2: f64 => p0
        {
            double const p0 = 1.25;
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_f64x12(p0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(::std::bit_cast<::std::uint64_t>(load_f64(rr.results)) == ::std::bit_cast<::std::uint64_t>(p0));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_localget_add_reduce_negative() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        // No calls expected.
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_localget_add_reduce_negative_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_localget_add_reduce_negative");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // tailcall
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_localget_add_reduce_negative();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}


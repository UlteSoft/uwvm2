#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    inline constexpr ::std::uint32_t k_loop_end = 97u;

    [[nodiscard]] byte_vec build_round_f64_dense_loop_run_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: matches the extra-heavy `round_f64_dense` mega-fusion pattern (when enabled).
        // Returns: acc + x, so both hot locals remain observable.
        {
            constexpr ::std::uint32_t k_i = 0u;
            constexpr ::std::uint32_t k_x = 1u;
            constexpr ::std::uint32_t k_acc = 2u;

            func_type ty{{}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local0 i
            fb.locals.push_back({2u, k_val_f64});  // local1 x, local2 acc
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::local_set);
            u32(c, k_i);

            op(c, wasm_op::f64_const);
            f64(c, 1.23456789);
            op(c, wasm_op::local_set);
            u32(c, k_x);

            op(c, wasm_op::f64_const);
            f64(c, 0.0);
            op(c, wasm_op::local_set);
            u32(c, k_acc);

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);

            op(c, wasm_op::local_get);
            u32(c, k_i);
            op(c, wasm_op::i32_const);
            i32(c, static_cast<::std::int32_t>(k_loop_end));
            op(c, wasm_op::i32_ge_u);
            op(c, wasm_op::br_if);
            u32(c, 1u);

            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::f64_const);
            f64(c, 0.000001);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, k_x);

            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::f64_const);
            f64(c, 1.0000001);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::local_set);
            u32(c, k_x);

            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::f64_const);
            f64(c, 0.5);
            op(c, wasm_op::f64_sub);
            op(c, wasm_op::local_set);
            u32(c, k_x);

            op(c, wasm_op::local_get);
            u32(c, k_acc);
            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::f64_floor);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, k_acc);

            op(c, wasm_op::local_get);
            u32(c, k_acc);
            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::f64_ceil);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, k_acc);

            op(c, wasm_op::local_get);
            u32(c, k_acc);
            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::f64_trunc);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, k_acc);

            op(c, wasm_op::local_get);
            u32(c, k_acc);
            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::f64_nearest);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, k_acc);

            op(c, wasm_op::local_get);
            u32(c, k_acc);
            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::f64_abs);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, k_acc);

            op(c, wasm_op::local_get);
            u32(c, k_acc);
            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::f64_const);
            f64(c, -1.0);
            op(c, wasm_op::f64_copysign);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, k_acc);

            op(c, wasm_op::local_get);
            u32(c, k_i);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set);
            u32(c, k_i);

            op(c, wasm_op::br);
            u32(c, 0u);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            op(c, wasm_op::local_get);
            u32(c, k_acc);
            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::f64_add);

            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] double expected_f0() noexcept
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        namespace num = ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details;

        ::std::uint32_t i = 0u;
        wasm_f64 x = static_cast<wasm_f64>(1.23456789);
        wasm_f64 acc = static_cast<wasm_f64>(0.0);

        constexpr wasm_f64 add_k{static_cast<wasm_f64>(0.000001)};
        constexpr wasm_f64 mul_k{static_cast<wasm_f64>(1.0000001)};
        constexpr wasm_f64 sub_k{static_cast<wasm_f64>(0.5)};
        constexpr wasm_f64 neg_one{static_cast<wasm_f64>(-1.0)};

        while(i < k_loop_end)
        {
            x = num::eval_float_binop<num::float_binop::add, wasm_f64>(x, add_k);
            x = num::eval_float_binop<num::float_binop::mul, wasm_f64>(x, mul_k);
            x = num::eval_float_binop<num::float_binop::sub, wasm_f64>(x, sub_k);

            acc = num::eval_float_binop<num::float_binop::add, wasm_f64>(acc, num::eval_float_unop<num::float_unop::floor, wasm_f64>(x));
            acc = num::eval_float_binop<num::float_binop::add, wasm_f64>(acc, num::eval_float_unop<num::float_unop::ceil, wasm_f64>(x));
            acc = num::eval_float_binop<num::float_binop::add, wasm_f64>(acc, num::eval_float_unop<num::float_unop::trunc, wasm_f64>(x));
            acc = num::eval_float_binop<num::float_binop::add, wasm_f64>(acc, num::eval_float_unop<num::float_unop::nearest, wasm_f64>(x));
            acc = num::eval_float_binop<num::float_binop::add, wasm_f64>(acc, num::eval_float_unop<num::float_unop::abs, wasm_f64>(x));
            acc = num::eval_float_binop<num::float_binop::add, wasm_f64>(
                acc,
                num::eval_float_binop<num::float_binop::copysign, wasm_f64>(x, neg_one));

            i = static_cast<::std::uint32_t>(i + 1u);
        }

        return num::eval_float_binop<num::float_binop::add, wasm_f64>(acc, x);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_round_f64_dense_loop_run_suite(runtime_module_t const& rt, double expected) noexcept
    {
        if constexpr(Opt.is_tail_call) { static_assert(compiler::details::interpreter_tuple_has_no_holes<Opt>()); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;
        double const got = load_f64(Runner::run(cm.local_funcs.index_unchecked(0),
                                                rt.local_defined_function_vec_storage.index_unchecked(0),
                                                pack_no_params(),
                                                nullptr,
                                                nullptr)
                                        .results);
        UWVM2TEST_REQUIRE(::std::bit_cast<::std::uint64_t>(got) == ::std::bit_cast<::std::uint64_t>(expected));
        return 0;
    }

    [[nodiscard]] int test_translate_round_f64_dense_loop_run() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        double const expected = expected_f0();

        auto wasm = build_round_f64_dense_loop_run_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_round_f64_dense_loop_run");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
            constexpr auto exp_loop_run =
                optable::translate::get_uwvmint_round_f64_dense_loop_run_fptr_from_tuple<opt>(curr, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_loop_run));
#endif

            using Runner = interpreter_runner<opt>;
            double const got = load_f64(Runner::run(cm.local_funcs.index_unchecked(0),
                                                    rt.local_defined_function_vec_storage.index_unchecked(0),
                                                    pack_no_params(),
                                                    nullptr,
                                                    nullptr)
                                            .results);
            UWVM2TEST_REQUIRE(::std::bit_cast<::std::uint64_t>(got) == ::std::bit_cast<::std::uint64_t>(expected));
        }

        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_round_f64_dense_loop_run_suite<opt>(rt, expected) == 0);
        }

        {
            constexpr auto opt{make_tailcall_scalar4_merged_opt<2uz>()};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_round_f64_dense_loop_run_suite<opt>(rt, expected) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_round_f64_dense_loop_run();
}

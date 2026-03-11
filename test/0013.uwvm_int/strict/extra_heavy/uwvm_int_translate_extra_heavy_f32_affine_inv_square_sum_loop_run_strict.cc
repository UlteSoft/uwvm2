#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_f32_affine_inv_square_sum_loop_run_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };

        // f0: matches the extra-heavy f32 affine inv-square sum-loop mega-fusion pattern (when enabled).
        // Returns: sum_out + sum + f32(i) + f32(i1)
        {
            constexpr ::std::uint32_t k_sum = 0u;      // f32
            constexpr ::std::uint32_t k_i = 1u;        // i32
            constexpr ::std::uint32_t k_sum_out = 2u;  // f32
            constexpr ::std::uint32_t k_i1 = 3u;       // i32

            constexpr ::std::int32_t k_end = 10;  // even (loop ends when i+1 == end)
            constexpr ::std::int32_t k_i_init = 1;

            // f32.const 0x1.2dfd6ap-17
            constexpr ::std::uint32_t k_bits = 0x3716feb5u;
            float const k = ::std::bit_cast<float>(k_bits);

            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});  // local0 sum
            fb.locals.push_back({1u, k_val_i32});  // local1 i
            fb.locals.push_back({1u, k_val_f32});  // local2 sum_out
            fb.locals.push_back({1u, k_val_i32});  // local3 i1
            auto& c = fb.code;

            // sum = 0
            op(c, wasm_op::f32_const);
            f32(c, 0.0f);
            op(c, wasm_op::local_set);
            u32(c, k_sum);

            // i = 1 (odd)
            op(c, wasm_op::i32_const);
            i32(c, k_i_init);
            op(c, wasm_op::local_set);
            u32(c, k_i);

            // sum_out = 0
            op(c, wasm_op::f32_const);
            f32(c, 0.0f);
            op(c, wasm_op::local_set);
            u32(c, k_sum_out);

            // i1 = 0
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::local_set);
            u32(c, k_i1);

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);

            // 1 / (1 + i*k)^2 + sum -> sum_out
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::local_get);
            u32(c, k_i);
            op(c, wasm_op::f32_convert_i32_u);
            op(c, wasm_op::f32_const);
            f32(c, k);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_tee);
            u32(c, k_sum_out);
            op(c, wasm_op::local_get);
            u32(c, k_sum_out);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::f32_div);
            op(c, wasm_op::local_get);
            u32(c, k_sum);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, k_sum_out);

            // i1 = i+1; if(i1 == end) break;
            op(c, wasm_op::local_get);
            u32(c, k_i);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee);
            u32(c, k_i1);
            op(c, wasm_op::i32_const);
            i32(c, k_end);
            op(c, wasm_op::i32_eq);
            op(c, wasm_op::br_if);
            u32(c, 1u);

            // i = i1+1
            op(c, wasm_op::local_get);
            u32(c, k_i1);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set);
            u32(c, k_i);

            // 1 / (1 + i1*k)^2 + sum_out -> sum
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::local_get);
            u32(c, k_i1);
            op(c, wasm_op::f32_convert_i32_u);
            op(c, wasm_op::f32_const);
            f32(c, k);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_tee);
            u32(c, k_sum);
            op(c, wasm_op::local_get);
            u32(c, k_sum);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::f32_div);
            op(c, wasm_op::local_get);
            u32(c, k_sum_out);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, k_sum);

            // back-edge
            op(c, wasm_op::br);
            u32(c, 0u);

            op(c, wasm_op::end);  // end loop
            op(c, wasm_op::end);  // end block

            // return sum_out + sum + f32(i) + f32(i1)
            op(c, wasm_op::local_get);
            u32(c, k_sum_out);
            op(c, wasm_op::local_get);
            u32(c, k_sum);
            op(c, wasm_op::f32_add);

            op(c, wasm_op::local_get);
            u32(c, k_i);
            op(c, wasm_op::f32_convert_i32_u);
            op(c, wasm_op::f32_add);

            op(c, wasm_op::local_get);
            u32(c, k_i1);
            op(c, wasm_op::f32_convert_i32_u);
            op(c, wasm_op::f32_add);

            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] float expected_f0() noexcept
    {
        constexpr ::std::uint32_t k_bits = 0x3716feb5u;
        float const k = ::std::bit_cast<float>(k_bits);

        float sum = 0.0f;
        float sum_out = 0.0f;
        ::std::uint32_t i_u = 1u;
        ::std::uint32_t i1_u = 0u;
        constexpr ::std::uint32_t end_u = 10u;

        for(;;)
        {
            float const fi = static_cast<float>(i_u);
            float const x0 = 1.0f + (fi * k);
            float const term0 = 1.0f / (x0 * x0);
            sum_out = sum + term0;

            i1_u = i_u + 1u;
            if(i1_u == end_u) { break; }

            float const fi1 = static_cast<float>(i1_u);
            float const x1 = 1.0f + (fi1 * k);
            float const term1 = 1.0f / (x1 * x1);
            sum = sum_out + term1;

            i_u = i1_u + 1u;
        }

        return ((sum_out + sum) + static_cast<float>(i_u)) + static_cast<float>(i1_u);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_f32_affine_inv_square_sum_loop_run_suite(runtime_module_t const& rt, float expected) noexcept
    {
        if constexpr(Opt.is_tail_call) { static_assert(compiler::details::interpreter_tuple_has_no_holes<Opt>()); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;
        float const got = load_f32(Runner::run(cm.local_funcs.index_unchecked(0),
                                               rt.local_defined_function_vec_storage.index_unchecked(0),
                                               pack_no_params(),
                                               nullptr,
                                               nullptr)
                                       .results);
        UWVM2TEST_REQUIRE(::std::bit_cast<::std::uint32_t>(got) == ::std::bit_cast<::std::uint32_t>(expected));

        return 0;
    }

    [[nodiscard]] int test_translate_f32_affine_inv_square_sum_loop_run() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        float const expected = expected_f0();

        auto wasm = build_f32_affine_inv_square_sum_loop_run_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_f32_affine_inv_square_sum_loop_run");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall mode: bytecode should contain the mega-op when extra-heavy combine is enabled.
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
                optable::translate::get_uwvmint_f32_affine_inv_square_sum_loop_run_fptr_from_tuple<opt>(curr, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_loop_run));
#endif

            using Runner = interpreter_runner<opt>;
            float const got = load_f32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                                   pack_no_params(),
                                                   nullptr,
                                                   nullptr)
                                           .results);
            UWVM2TEST_REQUIRE(::std::bit_cast<::std::uint32_t>(got) == ::std::bit_cast<::std::uint32_t>(expected));
        }

        // Byref mode: semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_f32_affine_inv_square_sum_loop_run_suite<opt>(rt, expected) == 0);
        }

        // Tailcall + stacktop caching: semantics.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 5uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 5uz,
                .f64_stack_top_end_pos = 7uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_f32_affine_inv_square_sum_loop_run_suite<opt>(rt, expected) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_f32_affine_inv_square_sum_loop_run();
}


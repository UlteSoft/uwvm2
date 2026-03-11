#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_loop_fusion_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        auto add_for_loop = [&](::std::int32_t step, ::std::int32_t end) -> void
        {
            // (result i32) (local i32 counter)
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            auto& c = fb.code;

            // counter = 0
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            // block/loop
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);

            // counter += step
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, step);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            // if (counter < end) continue
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, end);
            op(c, wasm_op::i32_lt_u);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            // end loop + block
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            // return counter
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_ptr_inc_loop = [&](::std::int32_t step, ::std::int32_t pend) -> void
        {
            // (result i32) (local i32 p, i32 pend)
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // p
            fb.locals.push_back({1u, k_val_i32});  // pend
            auto& c = fb.code;

            // p = 0
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            // pend = const
            op(c, wasm_op::i32_const);
            i32(c, pend);
            op(c, wasm_op::local_set);
            u32(c, 1u);

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);

            // Canonical ptr loop skeleton (for_ptr_inc_ne_br_if):
            // local.get p; i32.const step; i32.add; local.tee p; local.get pend; i32.ne; br_if 0
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, step);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_ne);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::end);  // end loop
            op(c, wasm_op::end);  // end block

            // return p
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_i32_inc_f64_lt_u_eqz_loop = [&](double n, ::std::int32_t step) -> void
        {
            // (result i32) (local f64 n, i32 i)
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});  // n
            fb.locals.push_back({1u, k_val_i32});  // i
            auto& c = fb.code;

            // n = const
            op(c, wasm_op::f64_const);
            f64(c, n);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            // i = 0
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::local_set);
            u32(c, 1u);

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);

            // Canonical test8 loop skeleton (for_i32_inc_f64_lt_u_eqz_br_if):
            // local.get n; local.get i; i32.const step; i32.add; local.tee i; f64.convert_i32_u; f64.lt; i32.eqz; br_if 0
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_const);
            i32(c, step);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee);
            u32(c, 1u);
            op(c, wasm_op::f64_convert_i32_u);
            op(c, wasm_op::f64_lt);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::end);  // end loop
            op(c, wasm_op::end);  // end block

            // return i
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f0: step=1, end=10 => 10
        add_for_loop(1, 10);
        // f1: step=2, end=11 => 12 (overshoot)
        add_for_loop(2, 11);
        // f2: ptr loop step=4, pend=64 => 64
        add_ptr_inc_loop(4, 64);
        // f3: i32-inc + f64.lt + eqz hot loop: n=10.0, step=1 => 11
        add_i32_inc_f64_lt_u_eqz_loop(10.0, 1);

        return mb.build();
    }

    [[nodiscard]] int test_translate_loop_fusion() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_loop_fusion_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_loop_fusion");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall mode: expect loop-skeleton fusion when heavy combine ops are enabled.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
            constexpr auto exp_for_i32_inc_lt_u =
                optable::translate::get_uwvmint_for_i32_inc_lt_u_br_if_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_for_i32_inc_f64_lt_u_eqz =
                optable::translate::get_uwvmint_for_i32_inc_f64_lt_u_eqz_br_if_fptr_from_tuple<opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_for_i32_inc_lt_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_for_i32_inc_lt_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_for_i32_inc_f64_lt_u_eqz));
#endif

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            {
                constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
                constexpr auto tuple =
                    compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
                constexpr auto exp_for_ptr_inc_ne =
                    optable::translate::get_uwvmint_for_ptr_inc_ne_br_if_fptr_from_tuple<opt>(curr, tuple);
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_for_ptr_inc_ne));
            }
#endif

            using Runner = interpreter_runner<opt>;
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr0.results) == 10);

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr1.results) == 12);

            auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                                   rt.local_defined_function_vec_storage.index_unchecked(2),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr2.results) == 64);

            auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3),
                                   rt.local_defined_function_vec_storage.index_unchecked(3),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr3.results) == 11);
        }

        // Byref mode: semantics smoke (combine pending is intentionally disabled in byref mode).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr0.results) == 10);

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr1.results) == 12);

            auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                                   rt.local_defined_function_vec_storage.index_unchecked(2),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr2.results) == 64);

            auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3),
                                   rt.local_defined_function_vec_storage.index_unchecked(3),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr3.results) == 11);
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

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_no_params(),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 10);
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_no_params(),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 12);
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                                  pack_no_params(),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 64);
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                                  pack_no_params(),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 11);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_loop_fusion();
}

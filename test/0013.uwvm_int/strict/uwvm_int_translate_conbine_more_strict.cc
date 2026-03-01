#include "uwvm_int_translate_strict_common.h"

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

    [[nodiscard]] byte_vec build_conbine_more_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: local.get 0 ; i32.const 7 ; i32.add ; local.tee 0 ; drop ; local.get 0 ; end
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: local.get 0 ; local.get 1 ; i32.add ; local.tee 2 ; drop ; local.get 2 ; end
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local2: i32 out
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: local.get 0 ; local.get 1 ; i32.sub ; end
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_sub);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: rotl(x, 13) stored via local.tee (tests i32_binop_imm_stack_local_tee when enabled)
        // local.get 0 ; nop ; i32.const 13 ; i32.rotl ; local.tee 0 ; drop ; local.get 0 ; end
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);  // barrier so the const becomes a standalone pending provider (const_i32), not local_get_const_i32
            op(c, wasm_op::i32_const); i32(c, 13);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::drop);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_conbine_more() noexcept
    {
        // Install optable hooks (unexpected traps/calls should terminate the test process).
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_more_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_more");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        runtime_module_t const& rt = *prep.mod;

        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<opt>;
        constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
        constexpr auto tuple = compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        constexpr auto exp_add_imm_local_tee_same =
            optable::translate::get_uwvmint_i32_add_imm_local_tee_same_fptr_from_tuple<opt>(curr, tuple);
# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        constexpr auto exp_rotl_imm_stack_local_tee =
            optable::translate::get_uwvmint_i32_binop_imm_stack_local_tee_fptr_from_tuple<
                opt,
                optable::numeric_details::int_binop::rotl>(curr, tuple);
# endif
# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        constexpr auto exp_add_2localget_local_tee =
            optable::translate::get_uwvmint_i32_add_2localget_local_tee_fptr_from_tuple<opt>(curr, tuple);
        constexpr auto exp_sub_2localget =
            optable::translate::get_uwvmint_i32_sub_2localget_fptr_from_tuple<opt>(curr, tuple);
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT)
        constexpr auto exp_delay_add_tee =
            optable::translate::get_uwvmint_i32_binop_localget_rhs_local_tee_fptr_from_tuple<
                opt,
                optable::numeric_details::int_binop::add>(curr, tuple);
#endif
#if defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY)
        constexpr auto exp_delay_sub =
            optable::translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<
                opt,
                optable::numeric_details::int_binop::sub>(curr, tuple);
#endif

        // f0: x -> x+7, should use a combined opcode when enabled.
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(5),
                                  nullptr,
                                  nullptr
            );
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 12);
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_add_imm_local_tee_same));
#endif
        }

        // f1: a+b, should hit either the direct 2localget fusion or delay_local (depending on build).
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32x2(3, 4),
                                  nullptr,
                                  nullptr
            );
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 7);
#if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS) || defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT)
            bool ok{};
# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            ok = ok || bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_add_2localget_local_tee);
# endif
# if defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT)
            ok = ok || bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_delay_add_tee);
# endif
            UWVM2TEST_REQUIRE(ok);
#endif
        }

        // f2: a-b, should hit either sub_2localget or delay_local (depending on build).
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_i32x2(10, 3),
                                  nullptr,
                                  nullptr
            );
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 7);
#if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS) || defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY)
            bool ok{};
# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            ok = ok || bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_sub_2localget);
# endif
# if defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY)
            ok = ok || bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_delay_sub);
# endif
            UWVM2TEST_REQUIRE(ok);
#endif
        }

        // f3: rotl(x, 13) stored via local.tee, should hit i32_binop_imm_stack_local_tee when enabled.
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(3),
                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                  pack_i32(1),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == (1 << 13));
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_rotl_imm_stack_local_tee));
#endif
        }

        // Byref mode: semantics smoke (combine/delay pending is not expected to persist across opcodes).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt_byref{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err2{};
            optable::compile_option cop2{};
            auto cm2 = compiler::compile_all_from_uwvm_single_func<opt_byref>(rt, cop2, err2);
            UWVM2TEST_REQUIRE(err2.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner2 = interpreter_runner<opt_byref>;

            UWVM2TEST_REQUIRE(load_i32(Runner2::run(cm2.local_funcs.index_unchecked(0),
                                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                                   pack_i32(5),
                                                   nullptr,
                                                   nullptr)
                                            .results) == 12);

            UWVM2TEST_REQUIRE(load_i32(Runner2::run(cm2.local_funcs.index_unchecked(1),
                                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                                   pack_i32x2(3, 4),
                                                   nullptr,
                                                   nullptr)
                                            .results) == 7);

            UWVM2TEST_REQUIRE(load_i32(Runner2::run(cm2.local_funcs.index_unchecked(2),
                                                   rt.local_defined_function_vec_storage.index_unchecked(2),
                                                   pack_i32x2(10, 3),
                                                   nullptr,
                                                   nullptr)
                                            .results) == 7);

            UWVM2TEST_REQUIRE(load_i32(Runner2::run(cm2.local_funcs.index_unchecked(3),
                                                   rt.local_defined_function_vec_storage.index_unchecked(3),
                                                   pack_i32(1),
                                                   nullptr,
                                                   nullptr)
                                            .results) == (1 << 13));
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_conbine_more();
}

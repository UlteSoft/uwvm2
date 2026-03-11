#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_br_if_cmp_imm_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        auto add_brif_cmp_imm = [&](wasm_op cmp_op, ::std::int32_t imm, ::std::int32_t taken_ret, ::std::int32_t fall_ret) -> void
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // block (result i32)
            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            // value for taken-branch
            op(c, wasm_op::i32_const);
            i32(c, taken_ret);

            // local.get 0 ; i32.const imm ; cmp ; br_if 0
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, imm);
            op(c, cmp_op);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            // fallthrough: drop taken_ret; return fall_ret
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, fall_ret);

            // end block + end function
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f0: eq imm
        add_brif_cmp_imm(wasm_op::i32_eq, 7, 111, 222);
        // f1: ne imm
        add_brif_cmp_imm(wasm_op::i32_ne, 7, 112, 223);
        // f2: lt_s imm
        add_brif_cmp_imm(wasm_op::i32_lt_s, 0, 123, 234);
        // f3: lt_u imm
        add_brif_cmp_imm(wasm_op::i32_lt_u, 10, 345, 456);
        // f4: ge_s imm
        add_brif_cmp_imm(wasm_op::i32_ge_s, 0, 567, 678);
        // f5: ge_u imm
        add_brif_cmp_imm(wasm_op::i32_ge_u, 10, 789, 890);
        // f6: gt_s imm
        add_brif_cmp_imm(wasm_op::i32_gt_s, 0, 901, 902);
        // f7: gt_u imm
        add_brif_cmp_imm(wasm_op::i32_gt_u, 10, 903, 904);
        // f8: le_s imm
        add_brif_cmp_imm(wasm_op::i32_le_s, 0, 905, 906);
        // f9: le_u imm
        add_brif_cmp_imm(wasm_op::i32_le_u, 10, 907, 908);

        // f10: local.get 0 ; i32.eqz ; br_if 0
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 333);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 444);

            op(c, wasm_op::end);
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

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        if constexpr(Opt.is_tail_call && Opt.i32_stack_top_begin_pos == SIZE_MAX && Opt.i64_stack_top_begin_pos == SIZE_MAX &&
                     Opt.f32_stack_top_begin_pos == SIZE_MAX && Opt.f64_stack_top_begin_pos == SIZE_MAX && Opt.v128_stack_top_begin_pos == SIZE_MAX)
        {
            constexpr auto curr{make_initial_stacktop_currpos<Opt>()};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            constexpr auto exp_eq = optable::translate::get_uwvmint_br_if_i32_eq_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_ne = optable::translate::get_uwvmint_br_if_i32_ne_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_lt_s = optable::translate::get_uwvmint_br_if_i32_lt_s_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_lt_u = optable::translate::get_uwvmint_br_if_i32_lt_u_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_ge_s = optable::translate::get_uwvmint_br_if_i32_ge_s_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_ge_u = optable::translate::get_uwvmint_br_if_i32_ge_u_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_gt_s = optable::translate::get_uwvmint_br_if_i32_gt_s_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_gt_u = optable::translate::get_uwvmint_br_if_i32_gt_u_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_le_s = optable::translate::get_uwvmint_br_if_i32_le_s_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_le_u = optable::translate::get_uwvmint_br_if_i32_le_u_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_eqz = optable::translate::get_uwvmint_br_if_local_eqz_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_eq));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_ne));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_lt_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_lt_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_ge_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_ge_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_gt_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_gt_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_le_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(9).op.operands, exp_le_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(10).op.operands, exp_eqz));
        }
#endif

        using Runner = interpreter_runner<Opt>;

        // f0 eq
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32(7),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32(8),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // f1 ne
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(8),
                                              nullptr,
                                              nullptr)
                                       .results) == 112);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(7),
                                              nullptr,
                                              nullptr)
                                       .results) == 223);

        // f2 lt_s 0
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32(-1),
                                              nullptr,
                                              nullptr)
                                       .results) == 123);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 234);

        // f3 lt_u 10
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32(9),
                                              nullptr,
                                              nullptr)
                                       .results) == 345);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32(10),
                                              nullptr,
                                              nullptr)
                                       .results) == 456);

        // f4 ge_s 0
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i32(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 567);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i32(-1),
                                              nullptr,
                                              nullptr)
                                       .results) == 678);

        // f5 ge_u 10
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_i32(10),
                                              nullptr,
                                              nullptr)
                                       .results) == 789);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_i32(9),
                                              nullptr,
                                              nullptr)
                                       .results) == 890);

        // f6 gt_s 0
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_i32(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 901);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_i32(-1),
                                              nullptr,
                                              nullptr)
                                       .results) == 902);

        // f7 gt_u 10
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_i32(11),
                                              nullptr,
                                              nullptr)
                                       .results) == 903);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_i32(10),
                                              nullptr,
                                              nullptr)
                                       .results) == 904);

        // f8 le_s 0
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(8),
                                              rt.local_defined_function_vec_storage.index_unchecked(8),
                                              pack_i32(-1),
                                              nullptr,
                                              nullptr)
                                       .results) == 905);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(8),
                                              rt.local_defined_function_vec_storage.index_unchecked(8),
                                              pack_i32(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 906);

        // f9 le_u 10
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(9),
                                              rt.local_defined_function_vec_storage.index_unchecked(9),
                                              pack_i32(10),
                                              nullptr,
                                              nullptr)
                                       .results) == 907);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(9),
                                              rt.local_defined_function_vec_storage.index_unchecked(9),
                                              pack_i32(11),
                                              nullptr,
                                              nullptr)
                                       .results) == 908);

        // f10 eqz
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(10),
                                              rt.local_defined_function_vec_storage.index_unchecked(10),
                                              pack_i32(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 333);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(10),
                                              rt.local_defined_function_vec_storage.index_unchecked(10),
                                              pack_i32(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 444);

        return 0;
    }

    [[nodiscard]] int test_translate_br_if_cmp_imm() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_if_cmp_imm_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_if_cmp_imm");
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

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_br_if_cmp_imm();
}

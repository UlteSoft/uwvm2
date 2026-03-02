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
        // f1: lt_s imm
        add_brif_cmp_imm(wasm_op::i32_lt_s, 0, 123, 234);
        // f2: lt_u imm
        add_brif_cmp_imm(wasm_op::i32_lt_u, 10, 345, 456);
        // f3: ge_s imm
        add_brif_cmp_imm(wasm_op::i32_ge_s, 0, 567, 678);
        // f4: ge_u imm
        add_brif_cmp_imm(wasm_op::i32_ge_u, 10, 789, 890);

        // f5: local.get 0 ; i32.eqz ; br_if 0
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

        // Tailcall mode: strict assertions on fused br_if opfuncs when combine is enabled.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            constexpr auto exp_eq = optable::translate::get_uwvmint_br_if_i32_eq_imm_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_lt_s = optable::translate::get_uwvmint_br_if_i32_lt_s_imm_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_lt_u = optable::translate::get_uwvmint_br_if_i32_lt_u_imm_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_ge_s = optable::translate::get_uwvmint_br_if_i32_ge_s_imm_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_ge_u = optable::translate::get_uwvmint_br_if_i32_ge_u_imm_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_eqz = optable::translate::get_uwvmint_br_if_local_eqz_fptr_from_tuple<opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_eq));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_lt_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_lt_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_ge_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_ge_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_eqz));
#endif

            using Runner = interpreter_runner<opt>;

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

            // f1 lt_s 0
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_i32(-1),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 123);
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_i32(0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 234);

            // f2 lt_u 10
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                                  pack_i32(9),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 345);
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                                  pack_i32(10),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 456);

            // f3 ge_s 0
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                                  pack_i32(1),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 567);
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                                  pack_i32(-1),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 678);

            // f4 ge_u 10
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                                  rt.local_defined_function_vec_storage.index_unchecked(4),
                                                  pack_i32(10),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 789);
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                                  rt.local_defined_function_vec_storage.index_unchecked(4),
                                                  pack_i32(9),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 890);

            // f5 eqz
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(5),
                                                  rt.local_defined_function_vec_storage.index_unchecked(5),
                                                  pack_i32(0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 333);
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(5),
                                                  rt.local_defined_function_vec_storage.index_unchecked(5),
                                                  pack_i32(1),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 444);
        }

        // Byref mode: semantics smoke (fusions are not expected).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(7),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 111);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_br_if_cmp_imm();
}


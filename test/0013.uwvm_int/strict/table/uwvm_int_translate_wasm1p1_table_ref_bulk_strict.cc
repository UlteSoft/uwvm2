#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;
    using wasm_funcref_t = ::uwvm2::object::global::wasm_funcref_t;

    [[nodiscard]] byte_vec build_wasm1p1_table_ref_bulk_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 8u;
        mb.table_has_max = true;
        mb.table_max = 10u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto op1p1 = [&](byte_vec& c, wasm1p1_op o) { append_u8(c, u8(o)); };
        auto ext = [&](byte_vec& c, wasm1p1_numeric_op o)
        {
            op1p1(c, wasm1p1_op::numeric_prefix);
            append_u32_leb(c, u32(o));
        };
        auto u32_leb = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        auto make_const_func = [&](::std::int32_t value)
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const); i32(fb.code, value);
            op(fb.code, wasm_op::end);
            return mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto const f0{make_const_func(11)};
        auto const f1{make_const_func(22)};
        mb.passive_elements.push_back(passive_element_segment{.func_indices = {f0, f1}});

        auto emit_i32_const = [&](byte_vec& c, ::std::int32_t v)
        {
            op(c, wasm_op::i32_const);
            i32(c, v);
        };
        auto emit_table_size = [&](byte_vec& c)
        {
            ext(c, wasm1p1_numeric_op::table_size);
            u32_leb(c, 0u);
        };
        auto emit_table_get = [&](byte_vec& c, ::std::int32_t index)
        {
            emit_i32_const(c, index);
            op1p1(c, wasm1p1_op::table_get);
            u32_leb(c, 0u);
        };
        auto emit_ref_is_null = [&](byte_vec& c)
        {
            op1p1(c, wasm1p1_op::ref_is_null);
        };
        auto emit_add = [&](byte_vec& c)
        {
            op(c, wasm_op::i32_add);
        };
        auto emit_null = [&](byte_vec& c)
        {
            op1p1(c, wasm1p1_op::ref_null);
            append_u8(c, k_ref_funcref);
        };
        auto emit_ref_func = [&](byte_vec& c, ::std::uint32_t func_index)
        {
            op1p1(c, wasm1p1_op::ref_func);
            u32_leb(c, func_index);
        };
        auto emit_table_set = [&](byte_vec& c, ::std::int32_t index, ::std::uint32_t func_index)
        {
            emit_i32_const(c, index);
            emit_ref_func(c, func_index);
            op1p1(c, wasm1p1_op::table_set);
            u32_leb(c, 0u);
        };
        auto emit_table_fill_null = [&](byte_vec& c, ::std::int32_t index, ::std::int32_t len)
        {
            emit_i32_const(c, index);
            emit_null(c);
            emit_i32_const(c, len);
            ext(c, wasm1p1_numeric_op::table_fill);
            u32_leb(c, 0u);
        };
        auto emit_table_init = [&](byte_vec& c, ::std::int32_t dst, ::std::int32_t src, ::std::int32_t len)
        {
            emit_i32_const(c, dst);
            emit_i32_const(c, src);
            emit_i32_const(c, len);
            ext(c, wasm1p1_numeric_op::table_init);
            u32_leb(c, 0u);
            u32_leb(c, 0u);
        };
        auto emit_elem_drop = [&](byte_vec& c)
        {
            ext(c, wasm1p1_numeric_op::elem_drop);
            u32_leb(c, 0u);
        };
        auto emit_table_copy = [&](byte_vec& c, ::std::int32_t dst, ::std::int32_t src, ::std::int32_t len)
        {
            emit_i32_const(c, dst);
            emit_i32_const(c, src);
            emit_i32_const(c, len);
            ext(c, wasm1p1_numeric_op::table_copy);
            u32_leb(c, 0u);
            u32_leb(c, 0u);
        };
        auto emit_table_grow = [&](byte_vec& c, ::std::uint32_t func_index, ::std::int32_t delta)
        {
            emit_ref_func(c, func_index);
            emit_i32_const(c, delta);
            ext(c, wasm1p1_numeric_op::table_grow);
            u32_leb(c, 0u);
        };
        auto add_null_check = [&](byte_vec& c, ::std::int32_t index)
        {
            emit_table_get(c, index);
            emit_ref_is_null(c);
            emit_add(c);
        };

        func_type main_ty{{}, {k_val_i32}};
        func_body main_fb{};
        auto& c = main_fb.code;

        emit_table_size(c);                  // 8
        add_null_check(c, 0);                // +1, initial table[0] is null
        emit_table_set(c, 0, f0);
        add_null_check(c, 0);                // +0
        emit_table_fill_null(c, 1, 2);
        add_null_check(c, 1);                // +1
        emit_table_init(c, 3, 0, 2);
        add_null_check(c, 3);                // +0
        add_null_check(c, 4);                // +0
        emit_elem_drop(c);
        emit_table_copy(c, 5, 3, 2);
        add_null_check(c, 5);                // +0
        add_null_check(c, 6);                // +0
        emit_table_grow(c, f1, 1);
        emit_add(c);                         // +8
        emit_table_size(c);
        emit_add(c);                         // +9
        emit_table_fill_null(c, 8, 1);
        add_null_check(c, 8);                // +1
        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(main_ty), ::std::move(main_fb));
        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] bool bytecode_contains_ref_is_null_funcref_fptr(compiled_local_func_t const& lf) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        if constexpr(Opt.is_tail_call && Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            for(::std::size_t pos{Opt.i32_stack_top_begin_pos}; pos != Opt.i32_stack_top_end_pos; ++pos)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{};
                curr.i32_stack_top_curr_pos = pos;
                if(bytecode_contains_fptr(lf.op.operands,
                                           optable::translate::get_uwvmint_ref_is_null_typed_fptr_from_tuple<Opt, wasm_funcref_t>(curr, tuple)))
                {
                    return true;
                }
            }
            return false;
        }
        else
        {
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            return bytecode_contains_fptr(lf.op.operands,
                                          optable::translate::get_uwvmint_ref_is_null_typed_fptr_from_tuple<Opt, wasm_funcref_t>(curr, tuple));
        }
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_bytecode(compiled_module_t const& cm) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};

        auto const& lf = cm.local_funcs.index_unchecked(2);
        auto const& bc = lf.op.operands;
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_get_funcref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_set_funcref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(
            bytecode_contains_fptr(bc, optable::translate::get_uwvmint_ref_null_typed_fptr_from_tuple<Opt, wasm_funcref_t>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_ref_is_null_funcref_fptr<Opt>(lf));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_ref_func_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_init_funcref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_elem_drop_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_copy_funcref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_grow_funcref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_size_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_fill_funcref_fptr_from_tuple<Opt>(curr, tuple)));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(byte_vec const& wasm,
                                            ::uwvm2::utils::container::u8string_view name,
                                            wasm_feature_parameter_t const& features) noexcept
    {
        auto prep = prepare_runtime_from_wasm(wasm, name, {}, features);
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(features));
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(check_bytecode<Opt>(cm) == 0);

        using Runner = interpreter_runner<Opt>;
        auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                              rt.local_defined_function_vec_storage.index_unchecked(2),
                              pack_no_params(),
                              nullptr,
                              nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr.results) == 28);
        return 0;
    }

    [[nodiscard]] int test_translate_wasm1p1_table_ref_bulk() noexcept
    {
        install_unexpected_traps();
        optable::call_func = strict_terminate_call;
        optable::call_indirect_func = strict_terminate_call_indirect;

        auto wasm = build_wasm1p1_table_ref_bulk_module();
        auto features = make_wasm1p1_feature_parameter();

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_table_ref_bulk_byref", features) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_table_ref_bulk_tailmin", features) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_table_ref_bulk_sysv", features) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_table_ref_bulk_aapcs64", features) == 0);
        }

        return 0;
    }
}

int main()
{
    return test_translate_wasm1p1_table_ref_bulk();
}

#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;
    using wasm_externref_t = ::uwvm2::object::global::wasm_externref_t;

    [[nodiscard]] byte_vec build_wasm2_externref_table_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_elem_type = k_ref_externref;
        mb.table_min = 6u;
        mb.table_has_max = true;
        mb.table_max = 8u;
        mb.extra_tables.push_back(local_table_entry{.elem_type = k_ref_externref, .min = 4u, .max = 8u, .has_max = true});

        byte_vec null_extern_expr{};
        append_u8(null_extern_expr, u8(wasm1p1_op::ref_null));
        append_u8(null_extern_expr, k_ref_externref);
        append_u8(null_extern_expr, u8(wasm_op::end));
        mb.passive_element_exprs.push_back(
            passive_element_expr_segment{.ref_type = k_ref_externref, .init_exprs = {::std::move(null_extern_expr)}});

        auto op = [](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto op1p1 = [](byte_vec& c, wasm1p1_op o) { append_u8(c, u8(o)); };
        auto ext = [&](byte_vec& c, wasm1p1_numeric_op o)
        {
            op1p1(c, wasm1p1_op::numeric_prefix);
            append_u32_leb(c, u32(o));
        };
        auto u32_leb = [](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32_const = [&](byte_vec& c, ::std::int32_t v)
        {
            op(c, wasm_op::i32_const);
            append_i32_leb(c, v);
        };
        auto local_externref = [&](byte_vec& c)
        {
            op(c, wasm_op::local_get);
            u32_leb(c, 0u);
        };
        auto table_get = [&](byte_vec& c, ::std::int32_t index)
        {
            i32_const(c, index);
            op1p1(c, wasm1p1_op::table_get);
            u32_leb(c, 1u);
        };

        func_type ty{{k_ref_externref}, {k_val_i32}};
        func_body fb{};
        auto& c{fb.code};

        // Preserve an accumulator below the side-effecting table operations.
        ext(c, wasm1p1_numeric_op::table_size);
        u32_leb(c, 1u);  // 4

        i32_const(c, 0);
        local_externref(c);
        op1p1(c, wasm1p1_op::table_set);
        u32_leb(c, 1u);
        table_get(c, 0);
        op1p1(c, wasm1p1_op::ref_is_null);
        op(c, wasm_op::i32_add);  // +0 for the non-null host reference

        local_externref(c);
        i32_const(c, 2);
        ext(c, wasm1p1_numeric_op::table_grow);
        u32_leb(c, 1u);
        op(c, wasm_op::i32_add);  // +4 (old size)

        ext(c, wasm1p1_numeric_op::table_size);
        u32_leb(c, 1u);
        op(c, wasm_op::i32_add);  // +6 (new size)

        i32_const(c, 1);
        local_externref(c);
        i32_const(c, 2);
        ext(c, wasm1p1_numeric_op::table_fill);
        u32_leb(c, 1u);

        i32_const(c, 3);
        i32_const(c, 1);
        i32_const(c, 2);
        ext(c, wasm1p1_numeric_op::table_copy);
        u32_leb(c, 0u);
        u32_leb(c, 1u);
        i32_const(c, 3);
        op1p1(c, wasm1p1_op::table_get);
        u32_leb(c, 0u);
        op1p1(c, wasm1p1_op::ref_is_null);
        op(c, wasm_op::i32_add);  // +0 after copying a non-null externref

        i32_const(c, 5);
        i32_const(c, 0);
        i32_const(c, 1);
        ext(c, wasm1p1_numeric_op::table_init);
        u32_leb(c, 0u);
        u32_leb(c, 1u);
        table_get(c, 5);
        op1p1(c, wasm1p1_op::ref_is_null);
        op(c, wasm_op::i32_add);  // +1 from the passive ref.null extern segment

        ext(c, wasm1p1_numeric_op::elem_drop);
        u32_leb(c, 0u);
        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] bool contains_externref_is_null(compiled_local_func_t const& lf) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        if constexpr(Opt.is_tail_call && Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            for(::std::size_t pos{Opt.i32_stack_top_begin_pos}; pos != Opt.i32_stack_top_end_pos; ++pos)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{};
                curr.i32_stack_top_curr_pos = pos;
                if(bytecode_contains_fptr(
                       lf.op.operands,
                       optable::translate::get_uwvmint_ref_is_null_typed_fptr_from_tuple<Opt, wasm_externref_t>(curr, tuple)))
                {
                    return true;
                }
            }
            return false;
        }
        else
        {
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            return bytecode_contains_fptr(
                lf.op.operands,
                optable::translate::get_uwvmint_ref_is_null_typed_fptr_from_tuple<Opt, wasm_externref_t>(curr, tuple));
        }
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(byte_vec const& wasm,
                                            ::uwvm2::utils::container::u8string_view name,
                                            wasm_feature_parameter_t const& features) noexcept
    {
        auto prep{prepare_runtime_from_wasm(wasm, name, {}, features)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt{*prep.mod};
        UWVM2TEST_REQUIRE(rt.local_defined_table_vec_storage.size() == 2uz);
        UWVM2TEST_REQUIRE(rt.local_defined_table_vec_storage.front_unchecked().elems.front_unchecked().type ==
                          ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::extern_ref);

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm{compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(features))};
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 1uz);

        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
        auto const& lf{cm.local_funcs.front_unchecked()};
        auto const& bc{lf.op.operands};
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_get_externref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_set_externref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_init_externref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_grow_externref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_fill_externref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_copy_funcref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(contains_externref_is_null<Opt>(lf));

        int host_value{42};
        wasm_externref_t ref{};
        ref.ref.storage.ptr = ::std::addressof(host_value);
        ref.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_extern;
        byte_vec packed(sizeof(ref));
        ::std::memcpy(packed.data(), ::std::addressof(ref), sizeof(ref));

        using Runner = interpreter_runner<Opt>;
        auto rr{Runner::run(lf, rt.local_defined_function_vec_storage.front_unchecked(), packed, nullptr, nullptr)};
        UWVM2TEST_REQUIRE(load_i32(rr.results) == 15);
        return 0;
    }

    [[nodiscard]] int test_wasm2_externref_table() noexcept
    {
        install_unexpected_traps();
        optable::call_func = strict_terminate_call;
        optable::call_indirect_func = strict_terminate_call_indirect;

        auto const wasm{build_wasm2_externref_table_module()};
        auto const features{make_wasm2_feature_parameter()};

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm2_externref_byref", features) == 0);
        }
        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm2_externref_tail_min", features) == 0);
        }
        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm2_externref_tail_sysv", features) == 0);
        }
        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm2_externref_tail_aapcs64", features) == 0);
        }
        return 0;
    }
}

int main()
{
    return test_wasm2_externref_table();
}

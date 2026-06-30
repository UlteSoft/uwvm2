#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;
    using errc = ::uwvm2::validation::error::code_validation_error_code;

    constexpr ::std::uint32_t k_large_block_type_index{260u};

    void add_large_block_type(module_builder& mb, func_type ty)
    {
        while(mb.types.size() < k_large_block_type_index) { mb.types.push_back({}); }
        mb.types.push_back(::std::move(ty));
    }

    void append_large_block_typeidx(byte_vec& c)
    {
        append_i32_leb(c, static_cast<::std::int32_t>(k_large_block_type_index));
    }

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] wasm_feature_parameter_t make_alignment_feature_parameter(bool multi_value, bool reference_types, bool bulk_memory) noexcept
    {
        auto out = make_wasm1p1_feature_parameter();
        using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
        auto& para = ::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(out);
        para.enable_multi_value = multi_value;
        para.enable_reference_types = reference_types;
        para.enable_bulk_memory = bulk_memory;
        return out;
    }

    [[nodiscard]] byte_vec build_select_t_empty_result_types_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto op1p1 = [&](byte_vec& c, wasm1p1_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::i32_const); i32(c, 7);
        op(c, wasm_op::i32_const); i32(c, 9);
        op(c, wasm_op::i32_const); i32(c, 1);
        op1p1(c, wasm1p1_op::select_t);
        append_u32_leb(c, 0u);
        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_table_fill_bulk_feature_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 2u;
        mb.table_has_max = true;
        mb.table_max = 2u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto op1p1 = [&](byte_vec& c, wasm1p1_op o) { append_u8(c, u8(o)); };
        auto ext = [&](byte_vec& c, wasm1p1_numeric_op o)
        {
            op1p1(c, wasm1p1_op::numeric_prefix);
            append_u32_leb(c, u32(o));
        };

        func_type ty{{}, {}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::unreachable);
        ext(c, wasm1p1_numeric_op::table_fill);
        append_u32_leb(c, 0u);
        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_ref_is_null_unknown_operand_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto op1p1 = [&](byte_vec& c, wasm1p1_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::unreachable);
        op1p1(c, wasm1p1_op::ref_is_null);
        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_typeidx_block_multivalue_repair_module()
    {
        module_builder mb{};
        add_large_block_type(mb, func_type{{}, {k_val_i32, k_val_i32}});

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        func_type ty{{}, {k_val_i32, k_val_i32}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::block);
        append_large_block_typeidx(c);
        op(c, wasm_op::i32_const); i32(c, 99);
        op(c, wasm_op::i32_const); i32(c, 11);
        op(c, wasm_op::i32_const); i32(c, 22);
        op(c, wasm_op::br); append_u32_leb(c, 0u);
        op(c, wasm_op::end);
        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_typeidx_loop_param_run_module()
    {
        module_builder mb{};
        add_large_block_type(mb, func_type{{k_val_i32}, {k_val_i32}});

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        func_type ty{{k_val_i32}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::local_get); u32(c, 0u);
        op(c, wasm_op::loop);
        append_large_block_typeidx(c);
        op(c, wasm_op::i32_const); i32(c, 1);
        op(c, wasm_op::i32_sub);
        op(c, wasm_op::local_tee); u32(c, 0u);
        op(c, wasm_op::local_get); u32(c, 0u);
        op(c, wasm_op::i32_const); i32(c, 0);
        op(c, wasm_op::i32_gt_s);
        op(c, wasm_op::br_if); u32(c, 0u);
        op(c, wasm_op::end);
        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_typeidx_if_param_run_module()
    {
        module_builder mb{};
        add_large_block_type(mb, func_type{{k_val_i32}, {k_val_i32}});

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::local_get); u32(c, 0u);
        op(c, wasm_op::local_get); u32(c, 1u);
        op(c, wasm_op::if_);
        append_large_block_typeidx(c);
        op(c, wasm_op::i32_const); i32(c, 1);
        op(c, wasm_op::i32_add);
        op(c, wasm_op::else_);
        op(c, wasm_op::i32_const); i32(c, 2);
        op(c, wasm_op::i32_add);
        op(c, wasm_op::end);
        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_typeidx_loop_label_mismatch_module()
    {
        module_builder mb{};
        add_large_block_type(mb, func_type{{k_val_i64}, {k_val_i32}});

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::i64_const); i64(c, 1);
        op(c, wasm_op::loop);
        append_large_block_typeidx(c);
        op(c, wasm_op::drop);
        op(c, wasm_op::i32_const); i32(c, 123);
        op(c, wasm_op::i32_const); i32(c, 0);
        op(c, wasm_op::br_if); u32(c, 0u);
        op(c, wasm_op::drop);
        op(c, wasm_op::i32_const); i32(c, 7);
        op(c, wasm_op::end);
        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_select_empty_and_run(byte_vec const& wasm, wasm_feature_parameter_t const& features) noexcept
    {
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_wasm1p1_select_t_empty_nomv", {}, features);
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(features));
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;
        auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                              rt.local_defined_function_vec_storage.index_unchecked(0),
                              pack_no_params(),
                              nullptr,
                              nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr.results) == 7);
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_table_fill_bulk_only(byte_vec const& wasm, wasm_feature_parameter_t const& features) noexcept
    {
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_wasm1p1_table_fill_bulk_only", {}, features);
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(features));
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
        auto const& bc = cm.local_funcs.index_unchecked(0).op.operands;
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_fill_funcref_fptr_from_tuple<Opt>(curr, tuple)));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_ref_is_null_unknown_operand(byte_vec const& wasm, wasm_feature_parameter_t const& features) noexcept
    {
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_wasm1p1_ref_is_null_unknown", {}, features);
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(features));
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 1uz);
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_typeidx_block_multivalue_and_run(byte_vec const& wasm, wasm_feature_parameter_t const& features) noexcept
    {
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_wasm1p1_typeidx_block_mv", {}, features);
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(features));
        UWVM2TEST_REQUIRE(err.err_code == errc::ok);

        using Runner = interpreter_runner<Opt>;
        auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                              rt.local_defined_function_vec_storage.index_unchecked(0),
                              pack_no_params(),
                              nullptr,
                              nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr.results, 0uz) == 11);
        UWVM2TEST_REQUIRE(load_i32(rr.results, 4uz) == 22);
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_typeidx_loop_param_and_run(byte_vec const& wasm, wasm_feature_parameter_t const& features) noexcept
    {
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_wasm1p1_typeidx_loop_param", {}, features);
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(features));
        UWVM2TEST_REQUIRE(err.err_code == errc::ok);

        using Runner = interpreter_runner<Opt>;
        auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                              rt.local_defined_function_vec_storage.index_unchecked(0),
                              pack_i32(3),
                              nullptr,
                              nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr.results) == 0);
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_typeidx_if_param_and_run(byte_vec const& wasm, wasm_feature_parameter_t const& features) noexcept
    {
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_wasm1p1_typeidx_if_param", {}, features);
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(features));
        UWVM2TEST_REQUIRE(err.err_code == errc::ok);

        using Runner = interpreter_runner<Opt>;
        auto rr_true = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_i32x2(10, 1),
                                   nullptr,
                                   nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr_true.results) == 11);

        auto rr_false = Runner::run(cm.local_funcs.index_unchecked(0),
                                    rt.local_defined_function_vec_storage.index_unchecked(0),
                                    pack_i32x2(10, 0),
                                    nullptr,
                                    nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr_false.results) == 12);
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_expect_error_with_features(byte_vec const& wasm,
                                                         ::uwvm2::utils::container::u8string_view module_name,
                                                         wasm_feature_parameter_t const& prepare_features,
                                                         wasm_feature_parameter_t const& compile_features,
                                                         errc expected) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        bool started_compile{};
        try
        {
            auto prep = prepare_runtime_from_wasm(wasm, module_name, {}, prepare_features);
            UWVM2TEST_REQUIRE(prep.mod != nullptr);
            runtime_module_t const& rt = *prep.mod;

            optable::compile_option cop{};
            started_compile = true;
            (void)compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(compile_features));
        }
        catch(::fast_io::error const&)
        {}
        catch(...)
        {
            return fail(__LINE__, "unexpected exception type");
        }

        UWVM2TEST_REQUIRE(started_compile);
        if(err.err_code != expected)
        {
            char buf[128]{};
            ::std::snprintf(buf, sizeof(buf), "err_code mismatch: expected=%u actual=%u", static_cast<unsigned>(expected), static_cast<unsigned>(err.err_code));
            return fail(__LINE__, buf);
        }
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_expect_error(byte_vec const& wasm,
                                           ::uwvm2::utils::container::u8string_view module_name,
                                           wasm_feature_parameter_t const& features,
                                           errc expected) noexcept
    {
        return compile_expect_error_with_features<Opt>(wasm, module_name, features, features, expected);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_alignment_suite_for_opt() noexcept
    {
        auto select_features = make_alignment_feature_parameter(false, false, true);
        UWVM2TEST_REQUIRE(compile_select_empty_and_run<Opt>(build_select_t_empty_result_types_module(), select_features) == 0);

        auto table_fill_features = make_alignment_feature_parameter(true, false, true);
        UWVM2TEST_REQUIRE(compile_table_fill_bulk_only<Opt>(build_table_fill_bulk_feature_module(), table_fill_features) == 0);

        auto ref_is_null_features = make_alignment_feature_parameter(false, true, false);
        UWVM2TEST_REQUIRE(compile_ref_is_null_unknown_operand<Opt>(build_ref_is_null_unknown_operand_module(), ref_is_null_features) == 0);

        auto typeidx_features = make_alignment_feature_parameter(true, false, false);
        auto typeidx_disabled_features = make_alignment_feature_parameter(false, false, false);
        UWVM2TEST_REQUIRE(compile_typeidx_block_multivalue_and_run<Opt>(build_typeidx_block_multivalue_repair_module(), typeidx_features) == 0);
        UWVM2TEST_REQUIRE(compile_typeidx_loop_param_and_run<Opt>(build_typeidx_loop_param_run_module(), typeidx_features) == 0);
        UWVM2TEST_REQUIRE(compile_typeidx_if_param_and_run<Opt>(build_typeidx_if_param_run_module(), typeidx_features) == 0);
        UWVM2TEST_REQUIRE(compile_expect_error_with_features<Opt>(build_typeidx_block_multivalue_repair_module(),
                                                                  u8"uwvm2test_wasm1p1_typeidx_block_mv_feature_disabled",
                                                                  typeidx_features,
                                                                  typeidx_disabled_features,
                                                                  errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(compile_expect_error<Opt>(build_typeidx_loop_label_mismatch_module(),
                                                    u8"uwvm2test_wasm1p1_typeidx_loop_label_mismatch",
                                                    typeidx_features,
                                                    errc::br_value_type_mismatch) == 0);
        return 0;
    }

    [[nodiscard]] int test_validator_alignment() noexcept
    {
        install_unexpected_traps();
        optable::call_func = strict_terminate_call;
        optable::call_indirect_func = strict_terminate_call_indirect;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_alignment_suite_for_opt<opt>() == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_alignment_suite_for_opt<opt>() == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_alignment_suite_for_opt<opt>() == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_alignment_suite_for_opt<opt>() == 0);
        }

        return 0;
    }
}

int main()
{
    return test_validator_alignment();
}

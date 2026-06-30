#include "uwvm_int_lazy_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_lazy;
    using errc = ::uwvm2::validation::error::code_validation_error_code;

    template <::std::size_t N>
    [[nodiscard]] constexpr ::uwvm2::utils::container::u8string_view literal_view(char8_t const (&literal)[N]) noexcept
    {
        return ::uwvm2::utils::container::u8string_view{literal, N - 1uz};
    }

    [[nodiscard]] strict::wasm_feature_parameter_t make_alignment_feature_parameter(bool multi_value,
                                                                                   bool reference_types,
                                                                                   bool bulk_memory) noexcept
    {
        auto out{strict::make_wasm1p1_feature_parameter()};
        using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
        auto& para{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(out)};
        para.enable_multi_value = multi_value;
        para.enable_reference_types = reference_types;
        para.enable_bulk_memory = bulk_memory;
        return out;
    }

    [[nodiscard]] byte_vec build_select_t_empty_result_types_module()
    {
        module_builder mb{};

        auto op = [](byte_vec& c, wasm_op o) { strict::append_u8(c, u8(o)); };
        auto op1p1 = [](byte_vec& c, strict::wasm1p1_op o) { strict::append_u8(c, strict::u8(o)); };
        auto i32 = [](byte_vec& c, ::std::int32_t v) { strict::append_i32_leb(c, v); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        auto& c{fb.code};
        op(c, wasm_op::i32_const);
        i32(c, 7);
        op(c, wasm_op::i32_const);
        i32(c, 9);
        op(c, wasm_op::i32_const);
        i32(c, 1);
        op1p1(c, strict::wasm1p1_op::select_t);
        strict::append_u32_leb(c, 0u);
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

        auto op = [](byte_vec& c, wasm_op o) { strict::append_u8(c, u8(o)); };
        auto op1p1 = [](byte_vec& c, strict::wasm1p1_op o) { strict::append_u8(c, strict::u8(o)); };
        auto ext = [&](byte_vec& c, strict::wasm1p1_numeric_op o)
        {
            op1p1(c, strict::wasm1p1_op::numeric_prefix);
            strict::append_u32_leb(c, strict::u32(o));
        };

        func_type ty{{}, {}};
        func_body fb{};
        auto& c{fb.code};
        op(c, wasm_op::unreachable);
        ext(c, strict::wasm1p1_numeric_op::table_fill);
        strict::append_u32_leb(c, 0u);
        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_ref_is_null_unknown_operand_module()
    {
        module_builder mb{};

        auto op = [](byte_vec& c, wasm_op o) { strict::append_u8(c, u8(o)); };
        auto op1p1 = [](byte_vec& c, strict::wasm1p1_op o) { strict::append_u8(c, strict::u8(o)); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        auto& c{fb.code};
        op(c, wasm_op::unreachable);
        op1p1(c, strict::wasm1p1_op::ref_is_null);
        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_primary_lazy_function(prepared_runtime const& prep,
                                                    lazy_module_t& storage,
                                                    ::uwvm2::utils::container::u8string_view module_name,
                                                    lazy_validation_mode_t validation_mode)
    {
        UWVM2TEST_REQUIRE(storage.functions.size() == 1uz);
        auto const& fn{storage.functions.index_unchecked(0)};
        UWVM2TEST_REQUIRE(fn.primary_cu_index != SIZE_MAX);

        auto options{make_lazy_options(module_name, validation_mode)};
        UWVM2TEST_REQUIRE(options.validator_feature_parameter != nullptr);
        if(validation_mode == lazy_validation_mode_t::validate_on_lazy_compile) { UWVM2TEST_REQUIRE(options.validator_module_storage != nullptr); }
        else { UWVM2TEST_REQUIRE(options.validator_module_storage == nullptr); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        compile_lazy_cu<Opt>(*prep.mod, storage, options, fn.primary_cu_index, err);
        UWVM2TEST_REQUIRE(err.err_code == errc::ok);
        UWVM2TEST_REQUIRE(storage.functions.index_unchecked(0).materialization_state.state.load(::std::memory_order_acquire) ==
                          lazy_compile_state_t::compiled);
        UWVM2TEST_REQUIRE(compiled_local_func_ready(storage, 0uz));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_select_empty_and_run(lazy_validation_mode_t validation_mode)
    {
        auto const module_name{validation_mode == lazy_validation_mode_t::validate_on_lazy_compile
                                   ? literal_view(u8"uwvm2test_lazy_wasm1p1_select_validate")
                                   : literal_view(u8"uwvm2test_lazy_wasm1p1_select_assume")};
        auto features{make_alignment_feature_parameter(false, false, true)};
        auto wasm{build_select_t_empty_result_types_module()};
        auto prep{prepare_runtime_from_wasm(wasm, module_name, {}, features)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        UWVM2TEST_REQUIRE(compile_primary_lazy_function<Opt>(prep, storage, module_name, validation_mode) == 0);

        auto rr{run_compiled_local_func<Opt>(storage,
                                             0uz,
                                             prep.mod->local_defined_function_vec_storage.index_unchecked(0),
                                             strict::pack_no_params())};
        UWVM2TEST_REQUIRE(load_i32(rr.results) == 7);
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_table_fill_bulk_only(lazy_validation_mode_t validation_mode)
    {
        auto const module_name{validation_mode == lazy_validation_mode_t::validate_on_lazy_compile
                                   ? literal_view(u8"uwvm2test_lazy_wasm1p1_table_fill_validate")
                                   : literal_view(u8"uwvm2test_lazy_wasm1p1_table_fill_assume")};
        auto features{make_alignment_feature_parameter(true, false, true)};
        auto wasm{build_table_fill_bulk_feature_module()};
        auto prep{prepare_runtime_from_wasm(wasm, module_name, {}, features)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        UWVM2TEST_REQUIRE(compile_primary_lazy_function<Opt>(prep, storage, module_name, validation_mode) == 0);
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_ref_is_null_unknown_operand(lazy_validation_mode_t validation_mode)
    {
        auto const module_name{validation_mode == lazy_validation_mode_t::validate_on_lazy_compile
                                   ? literal_view(u8"uwvm2test_lazy_wasm1p1_ref_is_null_validate")
                                   : literal_view(u8"uwvm2test_lazy_wasm1p1_ref_is_null_assume")};
        auto features{make_alignment_feature_parameter(false, true, false)};
        auto wasm{build_ref_is_null_unknown_operand_module()};
        auto prep{prepare_runtime_from_wasm(wasm, module_name, {}, features)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        UWVM2TEST_REQUIRE(compile_primary_lazy_function<Opt>(prep, storage, module_name, validation_mode) == 0);
        return 0;
    }

    [[nodiscard]] int test_lazy_wasm1p1_alignment()
    {
        configure_unexpected_traps();

        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
        UWVM2TEST_REQUIRE(compile_select_empty_and_run<opt>(lazy_validation_mode_t::validate_on_lazy_compile) == 0);
        UWVM2TEST_REQUIRE(compile_select_empty_and_run<opt>(lazy_validation_mode_t::assume_full_code_verified) == 0);
        UWVM2TEST_REQUIRE(compile_table_fill_bulk_only<opt>(lazy_validation_mode_t::validate_on_lazy_compile) == 0);
        UWVM2TEST_REQUIRE(compile_table_fill_bulk_only<opt>(lazy_validation_mode_t::assume_full_code_verified) == 0);
        UWVM2TEST_REQUIRE(compile_ref_is_null_unknown_operand<opt>(lazy_validation_mode_t::validate_on_lazy_compile) == 0);
        UWVM2TEST_REQUIRE(compile_ref_is_null_unknown_operand<opt>(lazy_validation_mode_t::assume_full_code_verified) == 0);
        return 0;
    }
}  // namespace

int main()
{
#if defined(__APPLE__) && (defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_UNDEFINED__) || defined(__SANITIZE_LEAK__))
    return 0;
#else
    return test_lazy_wasm1p1_alignment();
#endif
}

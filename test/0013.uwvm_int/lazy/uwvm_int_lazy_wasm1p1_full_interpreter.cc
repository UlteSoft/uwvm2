#include "uwvm_int_lazy_common.h"
#include "../wasm1p1/uwvm_int_wasm1p1_full_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_lazy;
    using namespace ::uwvm2test::uwvm_int_wasm1p1_full;
    using errc = ::uwvm2::validation::error::code_validation_error_code;
    using feature_parameter_t = strict::wasm_feature_parameter_t;

    template <::std::size_t N>
    [[nodiscard]] constexpr ::uwvm2::utils::container::u8string_view literal_view(char8_t const (&literal)[N]) noexcept
    {
        return ::uwvm2::utils::container::u8string_view{literal, N - 1uz};
    }

    [[nodiscard]] feature_parameter_t make_reference_feature_parameter(bool enabled) noexcept
    {
        auto out{strict::make_wasm1p1_feature_parameter()};
        using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
        auto& para{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(out)};
        para.disable_reference_types = !enabled;
        para.controllable_allow_multi_table = para.disable_reference_types;
        return out;
    }

    [[nodiscard]] feature_parameter_t make_sign_extension_feature_parameter(bool enabled) noexcept
    {
        auto out{strict::make_wasm1p1_feature_parameter()};
        using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
        auto& para{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(out)};
        para.disable_sign_extension = !enabled;
        return out;
    }

    [[nodiscard]] feature_parameter_t make_bulk_memory_feature_parameter(bool enabled) noexcept
    {
        auto out{strict::make_wasm1p1_feature_parameter()};
        using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
        auto& para{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(out)};
        para.disable_bulk_memory = !enabled;
        return out;
    }

    [[nodiscard]] feature_parameter_t make_nontrapping_feature_parameter(bool enabled) noexcept
    {
        auto out{strict::make_wasm1p1_feature_parameter()};
        using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
        auto& para{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(out)};
        para.disable_nontrapping_float_to_int = !enabled;
        return out;
    }

    [[nodiscard]] feature_parameter_t make_simd_feature_parameter(bool enabled) noexcept
    {
        auto out{strict::make_wasm1p1_feature_parameter()};
        using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
        auto& para{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(out)};
        para.disable_simd = !enabled;
        return out;
    }

    [[nodiscard]] int fail_err_mismatch(int line, char const* label, errc expected, errc actual) noexcept
    {
        char buf[192]{};
        ::std::snprintf(buf,
                        sizeof(buf),
                        "%s err_code mismatch: expected=%u actual=%u",
                        label,
                        static_cast<unsigned>(expected),
                        static_cast<unsigned>(actual));
        return strict::fail(line, buf);
    }

    [[nodiscard]] errc capture_standard_validator_error(prepared_runtime const& prep,
                                                        ::uwvm2::utils::container::u8string_view module_name,
                                                        feature_parameter_t const& features,
                                                        ::std::size_t local_function_index) noexcept
    {
        if(prep.mod == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const* const validator_storage{find_validator_module_storage(module_name)};
        if(validator_storage == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const& rt{*prep.mod};
        if(local_function_index >= rt.local_defined_function_vec_storage.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const& fn{rt.local_defined_function_vec_storage.index_unchecked(local_function_index)};
        if(fn.wasm_code_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const import_count{rt.imported_function_vec_storage.size()};
        auto const function_index{import_count + local_function_index};
        auto const& code{*fn.wasm_code_ptr};
        auto const code_begin{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
        auto const code_end{reinterpret_cast<::std::byte const*>(code.body.code_end)};

        ::uwvm2::validation::error::code_validation_error_impl err{};
        try
        {
            ::uwvm2::validation::standard::wasm1p1::validate_code(::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
                                                                  *validator_storage,
                                                                  function_index,
                                                                  code_begin,
                                                                  code_end,
                                                                  err,
                                                                  features);
        }
        catch(::fast_io::error const&)
        {}
        catch(...)
        {
            ::fast_io::fast_terminate();
        }

        return err.err_code;
    }

    [[nodiscard]] int standard_validator_accepts_all(prepared_runtime const& prep,
                                                     ::uwvm2::utils::container::u8string_view module_name,
                                                     feature_parameter_t const& features) noexcept
    {
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        for(::std::size_t i{}; i != prep.mod->local_defined_function_vec_storage.size(); ++i)
        {
            auto const actual{capture_standard_validator_error(prep, module_name, features, i)};
            if(actual != errc::ok) { return fail_err_mismatch(__LINE__, "standard-validator-valid", errc::ok, actual); }
        }
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] errc capture_full_compile_error(prepared_runtime const& prep, feature_parameter_t const& features) noexcept
    {
        if(prep.mod == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        try
        {
            (void)strict::compiler::compile_all_from_uwvm_single_func<Opt>(*prep.mod, cop, err, ::std::addressof(features));
        }
        catch(::fast_io::error const&)
        {}
        catch(...)
        {
            ::fast_io::fast_terminate();
        }

        return err.err_code;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] errc capture_lazy_compile_error(prepared_runtime const& prep,
                                                  ::uwvm2::utils::container::u8string_view module_name,
                                                  feature_parameter_t const& features,
                                                  lazy_validation_mode_t validation_mode,
                                                  ::std::size_t local_function_index) noexcept
    {
        if(prep.mod == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        backend_compile_option_t cop{};
        lazy_module_t storage{};
        try
        {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
            storage = lazy::initialize_lazy_module_storage(*prep.mod, cop, err, small_code_size_split_config());
#else
            storage = lazy::initialize_lazy_module_storage(*prep.mod, cop, err, small_code_size_split_config(), ::std::addressof(features));
#endif
        }
        catch(::fast_io::error const&)
        {}
        catch(...)
        {
            ::fast_io::fast_terminate();
        }

        if(err.err_code != errc::ok) { return err.err_code; }
        if(local_function_index >= storage.functions.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const& fn{storage.functions.index_unchecked(local_function_index)};
        if(fn.primary_cu_index == SIZE_MAX) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto options{make_lazy_options(module_name, validation_mode)};
        options.validator_feature_parameter = ::std::addressof(features);
        if(validation_mode == lazy_validation_mode_t::validate_on_lazy_compile && options.validator_module_storage == nullptr) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }
        if(validation_mode == lazy_validation_mode_t::assume_full_code_verified && options.validator_module_storage != nullptr) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }

        try
        {
            compile_lazy_cu<Opt>(*prep.mod, storage, options, fn.primary_cu_index, err);
        }
        catch(::fast_io::error const&)
        {}
        catch(...)
        {
            ::fast_io::fast_terminate();
        }

        return err.err_code;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int expect_validator_alignment(byte_vec const& wasm,
                                                 ::uwvm2::utils::container::u8string_view direct_name,
                                                 ::uwvm2::utils::container::u8string_view full_name,
                                                 ::uwvm2::utils::container::u8string_view lazy_validate_name,
                                                 ::uwvm2::utils::container::u8string_view lazy_assume_name,
                                                 feature_parameter_t const& parse_features,
                                                 feature_parameter_t const& validation_features,
                                                 errc expected,
                                                 ::std::size_t local_function_index = 0uz) noexcept
    {
        {
            auto prep{prepare_runtime_from_wasm(wasm, direct_name, {}, parse_features)};
            UWVM2TEST_REQUIRE(prep.mod != nullptr);
            auto const actual{capture_standard_validator_error(prep, direct_name, validation_features, local_function_index)};
            if(actual != expected) { return fail_err_mismatch(__LINE__, "standard-validator", expected, actual); }
        }

        {
            auto prep{prepare_runtime_from_wasm(wasm, full_name, {}, parse_features)};
            UWVM2TEST_REQUIRE(prep.mod != nullptr);
            auto const actual{capture_full_compile_error<Opt>(prep, validation_features)};
            if(actual != expected) { return fail_err_mismatch(__LINE__, "full-compiler-validator", expected, actual); }
        }

        {
            auto prep{prepare_runtime_from_wasm(wasm, lazy_validate_name, {}, parse_features)};
            UWVM2TEST_REQUIRE(prep.mod != nullptr);
            auto const actual{
                capture_lazy_compile_error<Opt>(prep, lazy_validate_name, validation_features, lazy_validation_mode_t::validate_on_lazy_compile, local_function_index)};
            if(actual != expected) { return fail_err_mismatch(__LINE__, "lazy-validate-validator", expected, actual); }
        }

        {
            auto prep{prepare_runtime_from_wasm(wasm, lazy_assume_name, {}, parse_features)};
            UWVM2TEST_REQUIRE(prep.mod != nullptr);
            auto const actual{
                capture_lazy_compile_error<Opt>(prep, lazy_assume_name, validation_features, lazy_validation_mode_t::assume_full_code_verified, local_function_index)};
            if(actual != expected) { return fail_err_mismatch(__LINE__, "lazy-assume-validator", expected, actual); }
        }

        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int expect_validator_alignment_case(byte_vec const& wasm,
                                                      ::uwvm2::utils::container::u8string_view module_name,
                                                      feature_parameter_t const& parse_features,
                                                      feature_parameter_t const& validation_features,
                                                      errc expected,
                                                      ::std::size_t local_function_index = 0uz) noexcept
    {
        return expect_validator_alignment<Opt>(wasm,
                                               module_name,
                                               module_name,
                                               module_name,
                                               module_name,
                                               parse_features,
                                               validation_features,
                                               expected,
                                               local_function_index);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_all_lazy_functions_and_run(::uwvm2::utils::container::u8string_view module_name,
                                                         lazy_validation_mode_t validation_mode)
    {
        auto wasm{build_full_wasm1p1_module()};
        auto features{strict::make_wasm1p1_feature_parameter()};
        auto prep{prepare_runtime_from_wasm(wasm, module_name, {}, features)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        UWVM2TEST_REQUIRE(prep.mod->local_defined_function_vec_storage.size() == k_fn_count);
        UWVM2TEST_REQUIRE(standard_validator_accepts_all(prep, module_name, features) == 0);

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        UWVM2TEST_REQUIRE(storage.functions.size() == k_fn_count);

        auto options{make_lazy_options(module_name, validation_mode)};
        UWVM2TEST_REQUIRE(options.validator_feature_parameter != nullptr);
        if(validation_mode == lazy_validation_mode_t::validate_on_lazy_compile) { UWVM2TEST_REQUIRE(options.validator_module_storage != nullptr); }
        else { UWVM2TEST_REQUIRE(options.validator_module_storage == nullptr); }

        for(::std::size_t i{}; i != k_fn_count; ++i)
        {
            auto const& fn{storage.functions.index_unchecked(i)};
            UWVM2TEST_REQUIRE(fn.primary_cu_index != SIZE_MAX);

            if(fn.materialization_state.state.load(::std::memory_order_acquire) != lazy_compile_state_t::compiled)
            {
                ::uwvm2::validation::error::code_validation_error_impl err{};
                compile_lazy_cu<Opt>(*prep.mod, storage, options, fn.primary_cu_index, err);
                UWVM2TEST_REQUIRE(err.err_code == errc::ok);
            }

            UWVM2TEST_REQUIRE(storage.functions.index_unchecked(i).materialization_state.state.load(::std::memory_order_acquire) ==
                              lazy_compile_state_t::compiled);
            UWVM2TEST_REQUIRE(compiled_local_func_ready(storage, i));
        }

        auto run_func = [&](::std::size_t local_index, byte_vec const& params)
        {
            return run_compiled_local_func<Opt>(storage,
                                                local_index,
                                                prep.mod->local_defined_function_vec_storage.index_unchecked(local_index),
                                                params);
        };

        UWVM2TEST_REQUIRE((run_full_wasm1p1_suite<Opt>(*prep.mod, run_func) == 0));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_alignment_error_suite_for_opt() noexcept
    {
        auto all_features{strict::make_wasm1p1_feature_parameter()};
        auto ref_disabled_features{make_reference_feature_parameter(false)};
        auto sign_disabled_features{make_sign_extension_feature_parameter(false)};
        auto bulk_disabled_features{make_bulk_memory_feature_parameter(false)};
        auto nontrapping_disabled_features{make_nontrapping_feature_parameter(false)};
        auto simd_disabled_features{make_simd_feature_parameter(false)};

        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_ref_feature_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_ref_feature"),
                                                               all_features,
                                                               ref_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_ref_feature_masks_ref_type_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_ref_feature_masks_type"),
                                                               all_features,
                                                               ref_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_ref_null_type_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_ref_null_type"),
                                                               all_features,
                                                               all_features,
                                                               errc::wasm1p1_invalid_reference_type) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_ref_is_null_i32_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_ref_is_null_i32"),
                                                               all_features,
                                                               all_features,
                                                               errc::wasm1p1_invalid_reference_type) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_ref_func_undeclared_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_ref_func_undeclared"),
                                                               all_features,
                                                               all_features,
                                                               errc::wasm1p1_undeclared_ref_func) == 0);

        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_sign_extension_type_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_sign_feature"),
                                                               all_features,
                                                               sign_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_sign_extension_type_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_sign_type"),
                                                               all_features,
                                                               all_features,
                                                               errc::numeric_operand_type_mismatch) == 0);

        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_select_t_count_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_select_count"),
                                                               all_features,
                                                               all_features,
                                                               errc::invalid_const_immediate) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_select_t_cond_type_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_select_cond"),
                                                               all_features,
                                                               all_features,
                                                               errc::select_cond_type_not_i32) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_select_t_result_type_mismatch_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_select_type"),
                                                               all_features,
                                                               all_features,
                                                               errc::select_type_mismatch) == 0);

        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_numeric_prefix_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_numeric_prefix"),
                                                               all_features,
                                                               all_features,
                                                               errc::illegal_opbase) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_nontrapping_feature_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_nontrapping_feature"),
                                                               all_features,
                                                               nontrapping_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_bulk_feature_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_bulk_feature"),
                                                               all_features,
                                                               bulk_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_bulk_feature_masks_data_index_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_bulk_feature_masks_data"),
                                                               all_features,
                                                               bulk_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_memory_init_data_index_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_memory_init_data"),
                                                               all_features,
                                                               all_features,
                                                               errc::illegal_data_index) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_memory_copy_memidx_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_memory_copy_memidx"),
                                                               all_features,
                                                               all_features,
                                                               errc::illegal_memory_index) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_memory_fill_type_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_memory_fill_type"),
                                                               all_features,
                                                               all_features,
                                                               errc::numeric_operand_type_mismatch) == 0);

        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_table_get_index_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_table_get_index"),
                                                               all_features,
                                                               all_features,
                                                               errc::illegal_table_index) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_table_set_value_type_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_table_set_type"),
                                                               all_features,
                                                               all_features,
                                                               errc::br_value_type_mismatch) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_table_fill_len_type_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_table_fill_len"),
                                                               all_features,
                                                               all_features,
                                                               errc::numeric_operand_type_mismatch) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_table_fill_feature_masks_type_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_table_fill_feature_masks_type"),
                                                               all_features,
                                                               bulk_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_table_grow_value_type_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_table_grow_value"),
                                                               all_features,
                                                               all_features,
                                                               errc::br_value_type_mismatch) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_table_init_elem_index_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_table_init_elem"),
                                                               all_features,
                                                               all_features,
                                                               errc::illegal_element_index) == 0);

        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_simd_feature_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_simd_feature"),
                                                               all_features,
                                                               simd_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_simd_feature_masks_lane_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_simd_feature_masks_lane"),
                                                               all_features,
                                                               simd_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_simd_prefix_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_simd_prefix"),
                                                               all_features,
                                                               all_features,
                                                               errc::illegal_opbase) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_simd_lane_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_simd_lane"),
                                                               all_features,
                                                               all_features,
                                                               errc::invalid_const_immediate) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_simd_align_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_simd_align"),
                                                               all_features,
                                                               all_features,
                                                               errc::illegal_memarg_alignment) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_simd_no_memory_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_simd_no_memory"),
                                                               all_features,
                                                               all_features,
                                                               errc::no_memory) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_polymorphic_simd_lane_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_poly_simd_feature_masks_lane"),
                                                               all_features,
                                                               simd_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_polymorphic_simd_lane_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_poly_simd_lane"),
                                                               all_features,
                                                               all_features,
                                                               errc::invalid_const_immediate) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_polymorphic_simd_no_memory_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_poly_simd_no_memory"),
                                                               all_features,
                                                               all_features,
                                                               errc::no_memory) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_polymorphic_ref_func_undeclared_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_poly_ref_func_feature"),
                                                               all_features,
                                                               ref_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_polymorphic_ref_func_undeclared_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_poly_ref_func_undeclared"),
                                                               all_features,
                                                               all_features,
                                                               errc::wasm1p1_undeclared_ref_func) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_polymorphic_memory_init_data_index_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_poly_memory_init_feature"),
                                                               all_features,
                                                               bulk_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_polymorphic_memory_init_data_index_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_poly_memory_init_data"),
                                                               all_features,
                                                               all_features,
                                                               errc::illegal_data_index) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_polymorphic_table_init_elem_index_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_poly_table_init_feature"),
                                                               all_features,
                                                               bulk_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_invalid_polymorphic_table_init_elem_index_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_poly_table_init_elem"),
                                                               all_features,
                                                               all_features,
                                                               errc::illegal_element_index) == 0);

        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_valid_polymorphic_wasm1p1_combo_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_polymorphic_ref_feature"),
                                                               all_features,
                                                               ref_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_valid_polymorphic_wasm1p1_combo_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_polymorphic_simd_feature"),
                                                               all_features,
                                                               simd_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_valid_polymorphic_wasm1p1_combo_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_polymorphic_bulk_feature"),
                                                               all_features,
                                                               bulk_disabled_features,
                                                               errc::wasm1p1_feature_required) == 0);
        UWVM2TEST_REQUIRE(expect_validator_alignment_case<Opt>(build_valid_polymorphic_wasm1p1_combo_module(),
                                                               literal_view(u8"uwvm2test_lazy_full_polymorphic_combo"),
                                                               all_features,
                                                               all_features,
                                                               errc::ok) == 0);

        return 0;
    }

    [[nodiscard]] int test_lazy_wasm1p1_full_interpreter()
    {
        configure_unexpected_traps();

        {
            constexpr auto opt{strict::k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_all_lazy_functions_and_run<opt>(
                                  literal_view(u8"uwvm2test_lazy_wasm1p1_full_byref_validate"),
                                  lazy_validation_mode_t::validate_on_lazy_compile) == 0);
            UWVM2TEST_REQUIRE(compile_all_lazy_functions_and_run<opt>(
                                  literal_view(u8"uwvm2test_lazy_wasm1p1_full_byref_assume"),
                                  lazy_validation_mode_t::assume_full_code_verified) == 0);
            UWVM2TEST_REQUIRE(run_alignment_error_suite_for_opt<opt>() == 0);
        }

        {
            constexpr auto opt{strict::k_test_tail_sysv_v128_opt};
            static_assert(strict::compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(compile_all_lazy_functions_and_run<opt>(
                                  literal_view(u8"uwvm2test_lazy_wasm1p1_full_tail_v128_validate"),
                                  lazy_validation_mode_t::validate_on_lazy_compile) == 0);
            UWVM2TEST_REQUIRE(compile_all_lazy_functions_and_run<opt>(
                                  literal_view(u8"uwvm2test_lazy_wasm1p1_full_tail_v128_assume"),
                                  lazy_validation_mode_t::assume_full_code_verified) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
#if defined(__APPLE__) && (defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_UNDEFINED__) || defined(__SANITIZE_LEAK__))
    return 0;
#else
    return test_lazy_wasm1p1_full_interpreter();
#endif
}

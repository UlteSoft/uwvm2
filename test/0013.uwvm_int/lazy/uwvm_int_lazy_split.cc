#include "uwvm_int_lazy_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_lazy;
    namespace feature = ::uwvm2::parser::wasm::standard::wasm1p1::features;
    using errc = ::uwvm2::validation::error::code_validation_error_code;

    [[nodiscard]] byte_vec build_lazy_split_module()
    {
        module_builder mb{};

        auto op = [](byte_vec& c, wasm_op o) { strict::append_u8(c, u8(o)); };
        auto u32 = [](byte_vec& c, ::std::uint32_t v) { strict::append_u32_leb(c, v); };
        auto i32 = [](byte_vec& c, ::std::int32_t v) { strict::append_i32_leb(c, v); };

        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c{fb.code};

            op(c, wasm_op::local_get);
            u32(c, 0u);
            for(::std::int32_t i{}; i != 12; ++i)
            {
                op(c, wasm_op::i32_const);
                i32(c, i);
                op(c, wasm_op::drop);
            }
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c{fb.code};

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::if_);
            strict::append_u8(c, k_val_i32);
            op(c, wasm_op::i32_const);
            i32(c, 11);
            op(c, wasm_op::else_);
            op(c, wasm_op::block);
            strict::append_u8(c, k_val_i32);
            op(c, wasm_op::i32_const);
            i32(c, 22);
            op(c, wasm_op::end);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] byte_vec build_lazy_call_indirect_immediate_module(::std::initializer_list<::std::uint8_t> bytes_after_opcode,
                                                                     bool append_function_end = true)
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 1u;

        func_type ty{{}, {}};
        func_body fb{};
        auto& c{fb.code};

        strict::append_u8(c, u8(wasm_op::i32_const));
        strict::append_i32_leb(c, 0);
        strict::append_u8(c, u8(wasm_op::call_indirect));
        for(auto const byte: bytes_after_opcode) { strict::append_u8(c, byte); }
        if(append_function_end) { strict::append_u8(c, u8(wasm_op::end)); }

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_lazy_multicu_invalid_table_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 1u;

        func_type ty{{}, {}};
        func_body fb{};
        auto& c{fb.code};

        // Two sibling blocks force the execution-unit policy to create at least two compile units.  The structured decoder accepts
        // call_indirect's well-formed tableidx 1 without performing the module-table cardinality lookup; materialization later
        // rejects it because the module contains only table 0.
        strict::append_u8(c, u8(wasm_op::block));
        strict::append_u8(c, k_block_empty);
        strict::append_u8(c, u8(wasm_op::nop));
        strict::append_u8(c, u8(wasm_op::end));

        strict::append_u8(c, u8(wasm_op::block));
        strict::append_u8(c, k_block_empty);
        strict::append_u8(c, u8(wasm_op::i32_const));
        strict::append_i32_leb(c, 0);
        strict::append_u8(c, u8(wasm_op::call_indirect));
        strict::append_u32_leb(c, 0u);
        strict::append_u32_leb(c, 1u);
        strict::append_u8(c, u8(wasm_op::end));
        strict::append_u8(c, u8(wasm_op::end));

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] strict::wasm_feature_parameter_t make_direct_version_policy(feature::wasm_feature_cli_mode const mode) noexcept
    {
        auto out{strict::make_wasm1p1_feature_parameter()};
        auto& para{feature::get_wasm1p1_parameter(out)};
        para.cli_mode = mode;
        para.explicit_feature_mvp = mode == feature::wasm_feature_cli_mode::direct_wasmmvp;
        para.explicit_feature_wasm1p1 = mode == feature::wasm_feature_cli_mode::direct_wasm1p1;
        return out;
    }

    [[nodiscard]] lazy_split_config_t function_only_split_config() noexcept
    {
        return {.eu_policy = lazy::lazy_execution_unit_split_policy_t::function_only,
                .cu_policy = lazy::lazy_compile_unit_split_policy_t::function,
                .cu_code_size = 0uz};
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int expect_lazy_call_indirect_compile(::std::initializer_list<::std::uint8_t> bytes_after_opcode,
                                                        bool append_function_end,
                                                        ::uwvm2::utils::container::u8string_view module_name,
                                                        strict::wasm_feature_parameter_t const& policy,
                                                        errc const expected) noexcept
    {
        auto wasm{build_lazy_call_indirect_immediate_module(bytes_after_opcode, append_function_end)};
        auto prep{prepare_runtime_from_wasm(wasm, module_name, {}, policy)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto const& runtime_function{prep.mod->local_defined_function_vec_storage.index_unchecked(0uz)};
        auto const* const code_begin{reinterpret_cast<::std::byte const*>(runtime_function.wasm_code_ptr->body.expr_begin)};
        auto const* const call_indirect_begin{code_begin + 2uz};

        // Exercise the production structured splitter as well as materialization.  On a trailing-immediate failure it must diagnose
        // the call_indirect opcode, and on success it must leave a usable lazy function index.
        ::uwvm2::validation::error::code_validation_error_impl split_err{};
        bool split_caught_parse_error{};
        try
        {
            backend_compile_option_t split_compile_option{};
            auto split_storage{lazy::initialize_lazy_module_storage(
                *prep.mod, split_compile_option, split_err, small_code_size_split_config(), ::std::addressof(policy))};
            if(expected == errc::ok) { UWVM2TEST_REQUIRE(split_storage.functions.size() == 1uz); }
        }
        catch(::fast_io::error const&)
        {
            split_caught_parse_error = true;
        }
        catch(...)
        {
            return strict::fail(__LINE__, "unexpected splitter exception type");
        }
        UWVM2TEST_REQUIRE(split_err.err_code == expected);
        UWVM2TEST_REQUIRE(split_caught_parse_error == (expected != errc::ok));
        if(expected != errc::ok) { UWVM2TEST_REQUIRE(split_err.err_curr == call_indirect_begin); }

        // Function-only execution-unit splitting deliberately does not run the structured immediate decoder.  Therefore the result
        // below can only come from the real synchronous lazy materialization entry point, not from that decoder alone.
        ::uwvm2::validation::error::code_validation_error_impl err{};
        backend_compile_option_t compile_option{};
        auto storage{lazy::initialize_lazy_module_storage(
            *prep.mod, compile_option, err, function_only_split_config(), ::std::addressof(policy))};
        UWVM2TEST_REQUIRE(err.err_code == errc::ok);
        UWVM2TEST_REQUIRE(storage.functions.size() == 1uz);

        auto const& fn{storage.functions.index_unchecked(0uz)};
        UWVM2TEST_REQUIRE(fn.primary_cu_index != SIZE_MAX);
        UWVM2TEST_REQUIRE(fn.materialization_state.state.load(::std::memory_order_acquire) == lazy_compile_state_t::uncompiled);

        auto options{make_lazy_options(module_name, lazy_validation_mode_t::assume_full_code_verified)};
        options.validator_feature_parameter = ::std::addressof(policy);
        UWVM2TEST_REQUIRE(options.validator_module_storage == nullptr);

        bool caught_parse_error{};
        try
        {
            // This is the API under test: it must apply the selected Core grammar while materializing the whole owning function.
            lazy::compile_cu_from_lazy_validator<Opt>(*prep.mod, storage, options, fn.primary_cu_index, err);
        }
        catch(::fast_io::error const&)
        {
            caught_parse_error = true;
        }
        catch(...)
        {
            return strict::fail(__LINE__, "unexpected exception type");
        }

        UWVM2TEST_REQUIRE(err.err_code == expected);
        UWVM2TEST_REQUIRE(caught_parse_error == (expected != errc::ok));
        if(expected != errc::ok) { UWVM2TEST_REQUIRE(err.err_curr == call_indirect_begin); }
        if(expected == errc::ok)
        {
            UWVM2TEST_REQUIRE(fn.materialization_state.state.load(::std::memory_order_acquire) == lazy_compile_state_t::compiled);
            UWVM2TEST_REQUIRE(compiled_local_func_ready(storage, 0uz));
        }
        else
        {
            UWVM2TEST_REQUIRE(fn.materialization_state.state.load(::std::memory_order_acquire) == lazy_compile_state_t::failed);
            for(::std::size_t i{fn.first_cu_index}; i != fn.first_cu_index + fn.cu_count; ++i)
            {
                UWVM2TEST_REQUIRE(storage.compile_units.index_unchecked(i).state.state.load(::std::memory_order_acquire) ==
                                  lazy_compile_state_t::failed);
            }
        }
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int test_multicu_failure_publication() noexcept
    {
        auto const policy{strict::make_wasm2_feature_parameter()};
        auto wasm{build_lazy_multicu_invalid_table_module()};
        auto prep{prepare_runtime_from_wasm(wasm, u8"uwvm2test_lazy_multicu_invalid_table", {}, policy)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto const& runtime_function{prep.mod->local_defined_function_vec_storage.index_unchecked(0uz)};
        auto const* const code_begin{reinterpret_cast<::std::byte const*>(runtime_function.wasm_code_ptr->body.expr_begin)};
        auto const* const call_indirect_begin{code_begin + 8uz};

        lazy_split_config_t split_config{.eu_policy = lazy::lazy_execution_unit_split_policy_t::structured_control,
                                         .cu_policy = lazy::lazy_compile_unit_split_policy_t::execution_unit,
                                         .cu_code_size = 0uz};
        ::uwvm2::validation::error::code_validation_error_impl err{};
        backend_compile_option_t compile_option{};
        auto storage{lazy::initialize_lazy_module_storage(
            *prep.mod, compile_option, err, split_config, ::std::addressof(policy))};
        UWVM2TEST_REQUIRE(err.err_code == errc::ok);
        UWVM2TEST_REQUIRE(storage.functions.size() == 1uz);

        auto& fn{storage.functions.index_unchecked(0uz)};
        UWVM2TEST_REQUIRE(fn.primary_cu_index != SIZE_MAX);
        UWVM2TEST_REQUIRE(fn.cu_count >= 2uz);
        UWVM2TEST_REQUIRE(fn.materialization_state.state.load(::std::memory_order_acquire) == lazy_compile_state_t::uncompiled);
        for(::std::size_t i{fn.first_cu_index}; i != fn.first_cu_index + fn.cu_count; ++i)
        {
            UWVM2TEST_REQUIRE(storage.compile_units.index_unchecked(i).state.state.load(::std::memory_order_acquire) ==
                              lazy_compile_state_t::uncompiled);
        }

        auto options{make_lazy_options(u8"uwvm2test_lazy_multicu_invalid_table", lazy_validation_mode_t::assume_full_code_verified)};
        options.validator_feature_parameter = ::std::addressof(policy);
        UWVM2TEST_REQUIRE(options.validator_module_storage == nullptr);

        bool caught_parse_error{};
        try
        {
            lazy::compile_cu_from_lazy_validator<Opt>(*prep.mod, storage, options, fn.primary_cu_index, err);
        }
        catch(::fast_io::error const&)
        {
            caught_parse_error = true;
        }
        catch(...)
        {
            return strict::fail(__LINE__, "unexpected exception type");
        }

        UWVM2TEST_REQUIRE(caught_parse_error);
        UWVM2TEST_REQUIRE(err.err_code == errc::illegal_table_index);
        UWVM2TEST_REQUIRE(err.err_curr == call_indirect_begin);
        UWVM2TEST_REQUIRE(err.err_selectable.illegal_table_index.table_index == 1u);
        UWVM2TEST_REQUIRE(err.err_selectable.illegal_table_index.all_table_count == 1u);
        UWVM2TEST_REQUIRE(fn.materialization_state.state.load(::std::memory_order_acquire) == lazy_compile_state_t::failed);
        for(::std::size_t i{fn.first_cu_index}; i != fn.first_cu_index + fn.cu_count; ++i)
        {
            UWVM2TEST_REQUIRE(storage.compile_units.index_unchecked(i).state.state.load(::std::memory_order_acquire) ==
                              lazy_compile_state_t::failed);
        }

        return 0;
    }

    [[nodiscard]] int test_versioned_call_indirect_lazy_compile() noexcept
    {
        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};

        auto const mvp_policy{make_direct_version_policy(feature::wasm_feature_cli_mode::direct_wasmmvp)};
        UWVM2TEST_REQUIRE(expect_lazy_call_indirect_compile<opt>({0x00u, 0x00u},
                                                                 true,
                                                                 u8"uwvm2test_lazy_call_indirect_mvp_zero",
                                                                 mvp_policy,
                                                                 errc::ok) == 0);
        UWVM2TEST_REQUIRE(expect_lazy_call_indirect_compile<opt>({0x00u, 0x80u, 0x00u},
                                                                 true,
                                                                 u8"uwvm2test_lazy_call_indirect_mvp_padded_zero",
                                                                 mvp_policy,
                                                                 errc::invalid_table_index) == 0);

        // Core 1.1 and Core 2.0 encode tableidx as u32, so the width-bounded padded spelling 0x80 0x00 is valid zero.
        auto const wasm1p1_policy{make_direct_version_policy(feature::wasm_feature_cli_mode::direct_wasm1p1)};
        UWVM2TEST_REQUIRE(expect_lazy_call_indirect_compile<opt>({0x00u, 0x80u, 0x00u},
                                                                 true,
                                                                 u8"uwvm2test_lazy_call_indirect_wasm1p1_padded_zero",
                                                                 wasm1p1_policy,
                                                                 errc::ok) == 0);

        auto const wasm2_policy{strict::make_wasm2_feature_parameter()};
        UWVM2TEST_REQUIRE(expect_lazy_call_indirect_compile<opt>({0x00u, 0x80u, 0x00u},
                                                                 true,
                                                                 u8"uwvm2test_lazy_call_indirect_wasm2_padded_zero",
                                                                 wasm2_policy,
                                                                 errc::ok) == 0);

        auto wasm2_single_table_policy{wasm2_policy};
        auto& single_table_para{feature::get_wasm1p1_parameter(wasm2_single_table_policy)};
        single_table_para.cli_mode = feature::wasm_feature_cli_mode::scoped;
        single_table_para.disable_multiple_tables = true;
        UWVM2TEST_REQUIRE(expect_lazy_call_indirect_compile<opt>({0x00u, 0x01u},
                                                                 true,
                                                                 u8"uwvm2test_lazy_call_indirect_single_table_nonzero",
                                                                 wasm2_single_table_policy,
                                                                 errc::wasm2_feature_required) == 0);

        // Both immediates are syntactically valid, but typeidx 1 is out of range and tableidx 1 requires multiple tables.
        // The structured splitter must consume the complete instruction, then report the type error at the opcode before
        // considering the feature policy, exactly like eager uwvm-int, LLVM, and AOT materialization.
        UWVM2TEST_REQUIRE(expect_lazy_call_indirect_compile<opt>({0x01u, 0x01u},
                                                                 true,
                                                                 u8"uwvm2test_lazy_call_indirect_invalid_type_nonzero_table",
                                                                 wasm2_single_table_policy,
                                                                 errc::illegal_type_index) == 0);

        // u32 permits at most five bytes and only the low four payload bits in byte five.  Overflow must fail before semantic lookup.
        UWVM2TEST_REQUIRE(expect_lazy_call_indirect_compile<opt>({0x00u, 0xffu, 0xffu, 0xffu, 0xffu, 0x1fu},
                                                                 true,
                                                                 u8"uwvm2test_lazy_call_indirect_tableidx_overflow",
                                                                 wasm2_policy,
                                                                 errc::invalid_table_index) == 0);

        // The section parser only requires the last body byte to be 0x0b.  Here that byte completes typeidx=11 and is consumed by the
        // compiler, leaving the trailing table immediate exactly at code_end; lazy materialization must fail closed at that boundary.
        UWVM2TEST_REQUIRE(expect_lazy_call_indirect_compile<opt>({0x0bu},
                                                                 false,
                                                                 u8"uwvm2test_lazy_call_indirect_missing_tableidx",
                                                                 wasm2_policy,
                                                                 errc::invalid_table_index) == 0);
        return 0;
    }

    [[nodiscard]] int test_lazy_split()
    {
        auto wasm{build_lazy_split_module()};
        auto prep{prepare_runtime_from_wasm(wasm, u8"uwvm2test_lazy_split")};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        {
            lazy_split_config_t function_cfg{.eu_policy = lazy::lazy_execution_unit_split_policy_t::function_only,
                                             .cu_policy = lazy::lazy_compile_unit_split_policy_t::function,
                                             .cu_code_size = 0uz};
            auto function_storage{initialize_lazy_storage(*prep.mod, function_cfg)};
            UWVM2TEST_REQUIRE(function_storage.functions.size() == 2uz);

            auto const& fn{function_storage.functions.index_unchecked(0)};
            UWVM2TEST_REQUIRE(fn.eu_count == 1uz);
            UWVM2TEST_REQUIRE(fn.cu_count == 1uz);
            UWVM2TEST_REQUIRE(fn.primary_cu_index == fn.first_cu_index);

            auto const& eu{function_storage.execution_units.index_unchecked(fn.first_eu_index)};
            UWVM2TEST_REQUIRE(eu.kind == lazy::lazy_execution_unit_kind::function);
            UWVM2TEST_REQUIRE(eu.parent_eu_index == SIZE_MAX);

            auto const& cu{function_storage.compile_units.index_unchecked(fn.primary_cu_index)};
            UWVM2TEST_REQUIRE(cu.kind == lazy::lazy_compile_unit_kind::function);
            UWVM2TEST_REQUIRE(cu.begin_eu_index == fn.first_eu_index);
            UWVM2TEST_REQUIRE(cu.end_eu_index == fn.first_eu_index + 1uz);
            UWVM2TEST_REQUIRE(cu.materialization_scope == lazy::lazy_materialization_scope::whole_function);
        }

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        UWVM2TEST_REQUIRE(storage.functions.size() == 2uz);

        auto const& fn0{storage.functions.index_unchecked(0)};
        UWVM2TEST_REQUIRE(fn0.eu_count == 1uz);
        UWVM2TEST_REQUIRE(fn0.cu_count == 1uz);
        UWVM2TEST_REQUIRE(fn0.primary_cu_index == fn0.first_cu_index);

        for(::std::size_t i{fn0.first_eu_index}; i != fn0.first_eu_index + fn0.eu_count; ++i)
        {
            auto const& eu{storage.execution_units.index_unchecked(i)};
            UWVM2TEST_REQUIRE(eu.kind == lazy::lazy_execution_unit_kind::function);
            UWVM2TEST_REQUIRE(eu.function_index == fn0.function_index);
            UWVM2TEST_REQUIRE(eu.local_function_index == 0uz);
            UWVM2TEST_REQUIRE(eu.code_begin <= eu.code_end);
        }

        auto const& fn1{storage.functions.index_unchecked(1)};
        ::std::size_t if_eu{SIZE_MAX};
        ::std::size_t block_eu{SIZE_MAX};

        for(::std::size_t i{fn1.first_eu_index}; i != fn1.first_eu_index + fn1.eu_count; ++i)
        {
            auto const& eu{storage.execution_units.index_unchecked(i)};
            if(eu.kind == lazy::lazy_execution_unit_kind::if_) { if_eu = i; }
            if(eu.kind == lazy::lazy_execution_unit_kind::block) { block_eu = i; }
        }

        UWVM2TEST_REQUIRE(if_eu != SIZE_MAX);
        UWVM2TEST_REQUIRE(block_eu != SIZE_MAX);
        UWVM2TEST_REQUIRE(fn1.eu_count == 3uz);
        UWVM2TEST_REQUIRE(fn1.cu_count == 2uz);

        auto const& if_unit{storage.execution_units.index_unchecked(if_eu)};
        auto const& block_unit{storage.execution_units.index_unchecked(block_eu)};
        UWVM2TEST_REQUIRE(block_unit.parent_eu_index == if_eu);
        UWVM2TEST_REQUIRE(block_unit.depth == if_unit.depth + 1uz);
        UWVM2TEST_REQUIRE(if_unit.code_end > block_unit.code_end);

        for(::std::size_t i{fn1.first_cu_index}; i != fn1.first_cu_index + fn1.cu_count; ++i)
        {
            auto const& cu{storage.compile_units.index_unchecked(i)};
            UWVM2TEST_REQUIRE(cu.function_index == fn1.function_index);
            UWVM2TEST_REQUIRE(cu.local_function_index == 1uz);
            UWVM2TEST_REQUIRE(cu.begin_eu_index < cu.end_eu_index);
            UWVM2TEST_REQUIRE(cu.code_begin <= cu.code_end);
            UWVM2TEST_REQUIRE(cu.kind == lazy::lazy_compile_unit_kind::code_size_group);
            UWVM2TEST_REQUIRE(cu.materialization_scope == lazy::lazy_materialization_scope::whole_function);
        }

        {
            lazy_split_config_t eu_cfg{.eu_policy = lazy::lazy_execution_unit_split_policy_t::structured_control,
                                       .cu_policy = lazy::lazy_compile_unit_split_policy_t::execution_unit,
                                       .cu_code_size = 0uz};
            auto eu_storage{initialize_lazy_storage(*prep.mod, eu_cfg)};
            auto const& fn{eu_storage.functions.index_unchecked(1)};

            ::std::size_t structured_eu_count{};
            for(::std::size_t i{fn.first_eu_index}; i != fn.first_eu_index + fn.eu_count; ++i)
            {
                auto const& eu{eu_storage.execution_units.index_unchecked(i)};
                if(eu.kind != lazy::lazy_execution_unit_kind::function) { ++structured_eu_count; }
            }

            UWVM2TEST_REQUIRE(structured_eu_count == 2uz);
            UWVM2TEST_REQUIRE(fn.cu_count == structured_eu_count);

            for(::std::size_t i{fn.first_cu_index}; i != fn.first_cu_index + fn.cu_count; ++i)
            {
                auto const& cu{eu_storage.compile_units.index_unchecked(i)};
                UWVM2TEST_REQUIRE(cu.kind == lazy::lazy_compile_unit_kind::execution_unit);
                UWVM2TEST_REQUIRE(cu.end_eu_index == cu.begin_eu_index + 1uz);
                UWVM2TEST_REQUIRE(eu_storage.execution_units.index_unchecked(cu.begin_eu_index).kind != lazy::lazy_execution_unit_kind::function);
                UWVM2TEST_REQUIRE(cu.materialization_scope == lazy::lazy_materialization_scope::whole_function);
            }
        }

        return 0;
    }
}  // namespace

int main()
{
    if(auto const result{test_lazy_split()}; result != 0) { return result; }
    if(auto const result{test_versioned_call_indirect_lazy_compile()}; result != 0) { return result; }

    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
    return test_multicu_failure_publication<opt>();
}

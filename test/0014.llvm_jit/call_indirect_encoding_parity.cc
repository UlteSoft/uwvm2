/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief Version-correct call_indirect immediate parity across all retained ROS backends.
 * @details Core 1.0 uses a literal reserved 0x00 byte. Reference Types and Core 2.0
 *          use tableidx ::= u32; a single-table policy constrains the decoded value.
 */

#if defined(UWVM_DISABLE_INT) && !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
# define UWVM2TEST_STRICT_NO_INTERPRETER 1
#endif

#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"

#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

#include <cstddef>
#include <cstdint>

namespace
{
    namespace strict = ::uwvm2test::uwvm_int_strict;
#ifndef UWVM_DISABLE_INT
    namespace int_compiler = ::uwvm2::runtime::compiler::uwvm_int;
#endif
    namespace llvm_compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    namespace llvm_details = llvm_compiler::details;

    using validation_error = ::uwvm2::validation::error::code_validation_error_impl;
    using error_code = ::uwvm2::validation::error::code_validation_error_code;
    using feature_parameter = strict::wasm_feature_parameter_t;
    using cli_mode = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode;

    inline constexpr ::std::uint8_t canonical_zero_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x41, 0x00, 0x11,
        0x00, 0x00, 0x0b};

    inline constexpr ::std::uint8_t nonminimal_zero_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x0a, 0x01, 0x08, 0x00, 0x41, 0x00, 0x11,
        0x00, 0x80, 0x00, 0x0b};

    inline constexpr ::std::uint8_t nonzero_table_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x41, 0x00, 0x11,
        0x00, 0x01, 0x0b};

    inline constexpr ::std::uint8_t invalid_type_nonzero_table_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x41, 0x00, 0x11,
        0x01, 0x01, 0x0b};

    inline constexpr ::std::uint8_t invalid_type_overflowing_table_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x0d, 0x01, 0x0b, 0x00, 0x41, 0x00, 0x11,
        0x01, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x0b};

    [[nodiscard]] feature_parameter make_policy(cli_mode mode, bool disable_multiple_tables = false) noexcept
    {
        auto policy{strict::make_wasm1p1_feature_parameter()};
        auto& parameter{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(policy)};
        parameter.cli_mode = mode;
        parameter.disable_multiple_tables = disable_multiple_tables;
        parameter.controllable_allow_multi_table = disable_multiple_tables;
        return policy;
    }

    [[nodiscard]] validation_error validate_standard(llvm_details::validation_module_storage_t const& validation_module,
                                                     llvm_compiler::local_func_storage_t const& local_function,
                                                     feature_parameter const& policy) noexcept
    {
        validation_error error{};
        try
        {
            ::uwvm2::validation::standard::wasm2::validate_code_with_runtime_policy(validation_module,
                                                                                     local_function.function_index,
                                                                                     local_function.code_begin,
                                                                                     local_function.code_end,
                                                                                     error,
                                                                                     policy);
        }
        catch(::fast_io::error const&)
        {}
        return error;
    }

#ifndef UWVM_DISABLE_INT
    [[nodiscard]] validation_error validate_int(strict::runtime_module_t const& runtime_module,
                                                feature_parameter const& policy) noexcept
    {
        validation_error error{};
        int_compiler::optable::compile_option options{};
        int_compiler::optable::uwvm_interpreter_full_function_symbol_t storage{};
        int_compiler::compile_all_from_uwvm::details::initialize_local_defined_call_info(runtime_module, options, storage);
        try
        {
            int_compiler::compile_all_from_uwvm::details::compile_all_from_uwvm_local_func<
                int_compiler::optable::uwvm_interpreter_translate_option_t{}>(runtime_module,
                                                                              options,
                                                                              storage,
                                                                              0uz,
                                                                              ::std::addressof(policy),
                                                                              error);
        }
        catch(::fast_io::error const&)
        {}
        return error;
    }
#endif

    struct llvm_compile_result
    {
        validation_error error{};
        bool materialized{};
    };

    /// Exercise the real serial AOT pipeline. Valid encodings must survive IR verification and retain a finalized
    /// module; malformed or disabled immediates must return their error through the compiler-owned validation path.
    [[nodiscard]] llvm_compile_result compile_llvm(strict::runtime_module_t const& runtime_module,
                                                   feature_parameter const& policy) noexcept
    {
        llvm_compile_result result{};
        llvm_compiler::compile_option options{};
        options.validator_feature_parameter = ::std::addressof(policy);
        options.verify_llvm_jit_ir = true;
        options.emit_call_stack_frames = false;
        try
        {
            auto compiled{llvm_compiler::compile_all_from_uwvm(runtime_module, options, result.error, 0uz)};
            result.materialized = compiled.llvm_jit_module.emitted && compiled.llvm_jit_module.llvm_module != nullptr;
        }
        catch(::fast_io::error const&)
        {}
        return result;
    }

    template <::std::size_t N>
    [[nodiscard]] int check_case(::std::uint8_t const (&input)[N],
                                 feature_parameter const& policy,
                                 ::uwvm2::utils::container::u8string_view module_name,
                                 error_code expected) noexcept
    {
        strict::byte_vec bytes{};
        bytes.reserve(N);
        for(auto byte: input) { bytes.push_back(static_cast<::std::byte>(byte)); }

        auto prepared{strict::prepare_runtime_from_wasm(bytes, module_name, {}, policy)};
        UWVM2TEST_REQUIRE(prepared.mod != nullptr);
        UWVM2TEST_REQUIRE(prepared.mod->local_defined_function_vec_storage.size() == 1uz);

        validation_error local_storage_error{};
        auto const validation_module{llvm_details::build_runtime_validation_module(*prepared.mod)};
        auto const local_function{llvm_details::get_runtime_local_func_storage(*prepared.mod, 0uz, local_storage_error)};
        constexpr ::std::size_t call_indirect_opcode_offset{2uz};
        UWVM2TEST_REQUIRE(static_cast<::std::size_t>(local_function.code_end - local_function.code_begin) >
                          call_indirect_opcode_offset);
        UWVM2TEST_REQUIRE(local_function.code_begin[call_indirect_opcode_offset] == ::std::byte{0x11u});
        auto const* const call_indirect_opcode{local_function.code_begin + call_indirect_opcode_offset};
        auto const standard_error{validate_standard(validation_module, local_function, policy)};
#ifndef UWVM_DISABLE_INT
        auto const int_error{validate_int(*prepared.mod, policy)};
#endif
        auto const llvm_result{compile_llvm(*prepared.mod, policy)};

        UWVM2TEST_REQUIRE(standard_error.err_code == expected);
#ifndef UWVM_DISABLE_INT
        UWVM2TEST_REQUIRE(int_error.err_code == expected);
#endif
        UWVM2TEST_REQUIRE(llvm_result.error.err_code == expected);
        UWVM2TEST_REQUIRE(expected != error_code::ok || llvm_result.materialized);
        if(expected != error_code::ok)
        {
            UWVM2TEST_REQUIRE(standard_error.err_curr == call_indirect_opcode);
#ifndef UWVM_DISABLE_INT
            UWVM2TEST_REQUIRE(int_error.err_curr == call_indirect_opcode);
#endif
            UWVM2TEST_REQUIRE(llvm_result.error.err_curr == call_indirect_opcode);
        }
        if(expected == error_code::wasm2_feature_required)
        {
            constexpr auto feature{::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables};
            UWVM2TEST_REQUIRE(standard_error.err_selectable.wasm2_feature_required.feature == feature);
#ifndef UWVM_DISABLE_INT
            UWVM2TEST_REQUIRE(int_error.err_selectable.wasm2_feature_required.feature == feature);
#endif
            UWVM2TEST_REQUIRE(llvm_result.error.err_selectable.wasm2_feature_required.feature == feature);
            UWVM2TEST_REQUIRE(standard_error.err_selectable.wasm2_feature_required.value == 0x11u);
#ifndef UWVM_DISABLE_INT
            UWVM2TEST_REQUIRE(int_error.err_selectable.wasm2_feature_required.value == 0x11u);
#endif
            UWVM2TEST_REQUIRE(llvm_result.error.err_selectable.wasm2_feature_required.value == 0x11u);
        }
        return 0;
    }
}

int main()
{
    auto const wasm1p1_policy{make_policy(cli_mode::direct_wasm1p1)};
    UWVM2TEST_REQUIRE(check_case(canonical_zero_module, wasm1p1_policy, u8"call_indirect_wasm1p1_zero", error_code::ok) == 0);
    UWVM2TEST_REQUIRE(check_case(nonminimal_zero_module, wasm1p1_policy, u8"call_indirect_wasm1p1_nonminimal", error_code::ok) == 0);

    auto const mvp_policy{make_policy(cli_mode::direct_mvp, true)};
    UWVM2TEST_REQUIRE(check_case(canonical_zero_module, mvp_policy, u8"call_indirect_mvp_zero", error_code::ok) == 0);
    UWVM2TEST_REQUIRE(
        check_case(nonminimal_zero_module, mvp_policy, u8"call_indirect_mvp_nonminimal", error_code::invalid_table_index) == 0);
    UWVM2TEST_REQUIRE(check_case(invalid_type_nonzero_table_module,
                                 mvp_policy,
                                 u8"call_indirect_mvp_invalid_type_bad_reserved",
                                 error_code::invalid_table_index) == 0);

    auto const wasm2_policy{make_policy(cli_mode::direct_wasm2)};
    UWVM2TEST_REQUIRE(check_case(nonzero_table_module,
                                 wasm2_policy,
                                 u8"call_indirect_wasm2_table_out_of_bounds",
                                 error_code::illegal_table_index) == 0);

    auto const wasm2_single_table_policy{make_policy(cli_mode::direct_wasm2, true)};
    UWVM2TEST_REQUIRE(
        check_case(nonminimal_zero_module, wasm2_single_table_policy, u8"call_indirect_wasm2_nonminimal_zero", error_code::ok) == 0);
    UWVM2TEST_REQUIRE(check_case(nonzero_table_module,
                                 wasm2_single_table_policy,
                                 u8"call_indirect_wasm2_nonzero_table",
                                 error_code::wasm2_feature_required) == 0);
    UWVM2TEST_REQUIRE(check_case(invalid_type_overflowing_table_module,
                                 wasm2_single_table_policy,
                                 u8"call_indirect_wasm2_invalid_type_bad_table_encoding",
                                 error_code::invalid_table_index) == 0);
    UWVM2TEST_REQUIRE(check_case(invalid_type_nonzero_table_module,
                                 wasm2_single_table_policy,
                                 u8"call_indirect_wasm2_invalid_type_nonzero_table",
                                 error_code::illegal_type_index) == 0);
    return 0;
}

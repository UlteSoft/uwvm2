/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       Exact feature-policy parity for the Wasm2 standard, uwvm-int, and LLVM-JIT validators.
 * @details     Each Release 2.0 feature is disabled independently against one minimal module that requires it.
 */

#if defined(UWVM_DISABLE_INT) && !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
# define UWVM2TEST_STRICT_NO_INTERPRETER 1
#endif

#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"

#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

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

    enum class feature_id : ::std::uint_least8_t
    {
        sign_extension,
        nontrapping_float_to_int,
        multi_value,
        reference_types,
        table_instructions,
        multiple_tables,
        bulk_memory,
        simd
    };

    inline constexpr ::std::uint8_t sign_extension_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
        0x03, 0x02, 0x01, 0x00, 0x0a, 0x08, 0x01, 0x06, 0x00, 0x41, 0x00, 0xc0, 0x1a, 0x0b};

    inline constexpr ::std::uint8_t nontrapping_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x0a, 0x0c, 0x01, 0x0a, 0x00, 0x43, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x1a, 0x0b};

    inline constexpr ::std::uint8_t multi_value_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x09, 0x02, 0x60, 0x00, 0x02, 0x7f, 0x7e,
        0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x01, 0x0a, 0x0d, 0x01, 0x0b, 0x00, 0x02, 0x00, 0x41,
        0x01, 0x42, 0x02, 0x0b, 0x1a, 0x1a, 0x0b};

    inline constexpr ::std::uint8_t reference_types_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
        0x03, 0x02, 0x01, 0x00, 0x0a, 0x07, 0x01, 0x05, 0x00, 0xd0, 0x70, 0x1a, 0x0b};

    inline constexpr ::std::uint8_t table_instruction_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x41, 0x00, 0x25,
        0x00, 0x1a, 0x0b};

    inline constexpr ::std::uint8_t multiple_tables_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x07, 0x02, 0x70, 0x00, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x41,
        0x00, 0x25, 0x01, 0x1a, 0x0b};

    inline constexpr ::std::uint8_t bulk_memory_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x05, 0x03, 0x01, 0x00, 0x01, 0x0a, 0x0d, 0x01, 0x0b, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41,
        0x00, 0xfc, 0x0b, 0x00, 0x0b};

    inline constexpr ::std::uint8_t simd_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x0a, 0x17, 0x01, 0x15, 0x00, 0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x0b};

    // (module (type (func)) (table 1 funcref) (func i32.const 0 call_indirect (type 0)))
    inline constexpr ::std::uint8_t call_indirect_reserved_zero_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x41, 0x00, 0x11,
        0x00, 0x00, 0x0b};

    // Same function with the table immediate encoded as the permitted non-minimal u32 zero 0x80 0x00.
    inline constexpr ::std::uint8_t call_indirect_nonminimal_zero_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x0a, 0x01, 0x08, 0x00, 0x41, 0x00, 0x11,
        0x00, 0x80, 0x00, 0x0b};

    // Same function with a decoded table index of one.  A single-table feature
    // policy must reject the value after ULEB128 decoding, not reinterpret the
    // field as MVP's literal reserved byte.
    inline constexpr ::std::uint8_t call_indirect_nonzero_table_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x41, 0x00, 0x11,
        0x00, 0x01, 0x0b};

    // A syntactically valid nonzero table index paired with an out-of-range type index.
    inline constexpr ::std::uint8_t call_indirect_invalid_type_nonzero_table_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x41, 0x00, 0x11,
        0x01, 0x01, 0x0b};

    // The type index is also out of range, but the overflowing u32 table encoding is the first malformed field.
    inline constexpr ::std::uint8_t call_indirect_invalid_type_overflowing_table_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x0d, 0x01, 0x0b, 0x00, 0x41, 0x00, 0x11,
        0x01, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x0b};

    struct feature_case
    {
        ::std::uint8_t const* bytes{};
        ::std::size_t size{};
        feature_id feature{};
        ::uwvm2::utils::container::u8string_view module_name{};
        ::std::string_view cli_suffix{};
    };

    inline constexpr feature_case cases[]{
        {sign_extension_module, sizeof(sign_extension_module), feature_id::sign_extension, u8"wasm2_parity_sign", "sign-extension"},
        {nontrapping_module, sizeof(nontrapping_module), feature_id::nontrapping_float_to_int, u8"wasm2_parity_sat", "nontrapping-float-to-int"},
        {multi_value_module, sizeof(multi_value_module), feature_id::multi_value, u8"wasm2_parity_multi", "multi-value"},
        {reference_types_module, sizeof(reference_types_module), feature_id::reference_types, u8"wasm2_parity_ref", "reference-types"},
        {table_instruction_module, sizeof(table_instruction_module), feature_id::table_instructions, u8"wasm2_parity_table", "table-instructions"},
        {multiple_tables_module, sizeof(multiple_tables_module), feature_id::multiple_tables, u8"wasm2_parity_multitable", "multiple-tables"},
        {bulk_memory_module, sizeof(bulk_memory_module), feature_id::bulk_memory, u8"wasm2_parity_bulk", "bulk-memory"},
        {simd_module, sizeof(simd_module), feature_id::simd, u8"wasm2_parity_simd", "simd"},
    };

    [[nodiscard]] strict::byte_vec make_bytes(feature_case const& test_case)
    {
        strict::byte_vec bytes{};
        bytes.reserve(test_case.size);
        for(::std::size_t i{}; i != test_case.size; ++i) { bytes.push_back(static_cast<::std::byte>(test_case.bytes[i])); }
        return bytes;
    }

    [[nodiscard]] feature_parameter make_disabled_policy(feature_id const feature) noexcept
    {
        feature_parameter policy{strict::make_wasm1p1_feature_parameter()};
        auto& para{::uwvm2::parser::wasm::standard::wasm2::features::get_wasm2_parameter(policy)};
        para.cli_mode = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode::scoped;
        switch(feature)
        {
            case feature_id::sign_extension: para.disable_sign_extension = true; break;
            case feature_id::nontrapping_float_to_int: para.disable_nontrapping_float_to_int = true; break;
            case feature_id::multi_value: para.disable_multi_value = true; break;
            case feature_id::reference_types: para.disable_reference_types = true; break;
            case feature_id::table_instructions: para.disable_table_instructions = true; break;
            case feature_id::multiple_tables: para.disable_multiple_tables = true; break;
            case feature_id::bulk_memory: para.disable_bulk_memory = true; break;
            case feature_id::simd: para.disable_simd = true; break;
        }
        return policy;
    }

    [[nodiscard]] bool expected_feature(validation_error const& err, feature_id const feature) noexcept
    {
        using wasm1p1_feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
        using wasm2_feature = ::uwvm2::parser::wasm::base::wasm2_feature_kind;
        switch(feature)
        {
            case feature_id::sign_extension:
                return err.err_code == error_code::wasm1p1_feature_required &&
                       err.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature::sign_extension;
            case feature_id::nontrapping_float_to_int:
                return err.err_code == error_code::wasm1p1_feature_required &&
                       err.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature::nontrapping_float_to_int;
            case feature_id::multi_value:
                return err.err_code == error_code::wasm1p1_feature_required &&
                       err.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature::multi_value;
            case feature_id::reference_types:
                return err.err_code == error_code::wasm1p1_feature_required &&
                       err.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature::reference_types;
            case feature_id::table_instructions:
                return err.err_code == error_code::wasm2_feature_required &&
                       err.err_selectable.wasm2_feature_required.feature == wasm2_feature::table_instructions;
            case feature_id::multiple_tables:
                return err.err_code == error_code::wasm2_feature_required &&
                       err.err_selectable.wasm2_feature_required.feature == wasm2_feature::multiple_tables;
            case feature_id::bulk_memory:
                return err.err_code == error_code::wasm1p1_feature_required &&
                       err.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature::bulk_memory;
            case feature_id::simd:
                return err.err_code == error_code::wasm1p1_feature_required &&
                       err.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature::simd;
        }
        return false;
    }

    [[nodiscard]] bool same_feature_diagnostic(validation_error const& left, validation_error const& right) noexcept
    {
        if(left.err_code != right.err_code) { return false; }
        if(left.err_code == error_code::wasm1p1_feature_required)
        {
            auto const& lhs{left.err_selectable.wasm1p1_feature_required};
            auto const& rhs{right.err_selectable.wasm1p1_feature_required};
            return lhs.value == rhs.value && lhs.feature == rhs.feature && lhs.subject == rhs.subject;
        }
        if(left.err_code == error_code::wasm2_feature_required)
        {
            auto const& lhs{left.err_selectable.wasm2_feature_required};
            auto const& rhs{right.err_selectable.wasm2_feature_required};
            return lhs.value == rhs.value && lhs.feature == rhs.feature && lhs.subject == rhs.subject;
        }
        return true;
    }

    [[nodiscard]] validation_error validate_standard(llvm_details::validation_module_storage_t const& validation_module,
                                                     llvm_compiler::local_func_storage_t const& local_function,
                                                     feature_parameter const& policy) noexcept
    {
        validation_error err{};
        try
        {
            ::uwvm2::validation::standard::wasm2::validate_code_with_runtime_policy(validation_module,
                                                                                     local_function.function_index,
                                                                                     local_function.code_begin,
                                                                                     local_function.code_end,
                                                                                     err,
                                                                                     policy);
        }
        catch(::fast_io::error const&)
        {}
        return err;
    }

#ifndef UWVM_DISABLE_INT
    [[nodiscard]] validation_error validate_uwvm_int(strict::runtime_module_t const& runtime_module,
                                                     feature_parameter const& policy) noexcept
    {
        validation_error err{};
        int_compiler::optable::compile_option options{};
        int_compiler::optable::uwvm_interpreter_full_function_symbol_t storage{};
        int_compiler::compile_all_from_uwvm::details::initialize_local_defined_call_info(runtime_module, options, storage);
        try
        {
            for(::std::size_t local_index{}; local_index != runtime_module.local_defined_function_vec_storage.size(); ++local_index)
            {
                int_compiler::compile_all_from_uwvm::details::compile_all_from_uwvm_local_func<
                    int_compiler::optable::uwvm_interpreter_translate_option_t{}>(runtime_module,
                                                                                  options,
                                                                                  storage,
                                                                                  local_index,
                                                                                  ::std::addressof(policy),
                                                                                  err);
                if(err.err_code != error_code::ok) { break; }
            }
        }
        catch(::fast_io::error const&)
        {}
        return err;
    }
#endif

    [[nodiscard]] validation_error validate_llvm_internal(llvm_details::validation_module_storage_t const& validation_module,
                                                          llvm_compiler::local_func_storage_t const& local_function,
                                                          feature_parameter const& policy) noexcept
    {
        validation_error err{};
        try
        {
            llvm_details::validate_runtime_local_func(validation_module,
                                                      local_function,
                                                      err,
                                                      nullptr,
                                                      false,
                                                      false,
                                                      0u,
                                                      0uz,
                                                      0u,
                                                      0uz,
                                                      false,
                                                      false,
                                                      false,
                                                      false,
                                                      ::std::addressof(policy));
        }
        catch(::fast_io::error const&)
        {}
        return err;
    }

    struct llvm_compile_result
    {
        validation_error error{};
        bool materialized{};
    };

    /// Exercise the same serial LLVM module pipeline used by the AOT runtime.  Successful cases must survive IR
    /// verification and retain a finalized module; malformed immediates must report through the compiler-owned error.
    [[nodiscard]] llvm_compile_result compile_llvm_call_indirect(strict::runtime_module_t const& runtime_module,
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

    [[nodiscard]] int fail(::std::size_t const case_index, char const* message) noexcept
    {
        ::std::fprintf(stderr, "wasm2_feature_validator_parity[%zu]: %s\n", case_index, message);
        return 1;
    }

    [[nodiscard]] ::std::filesystem::path find_uwvm_binary(::std::filesystem::path directory)
    {
        for(;;)
        {
            auto const candidate{directory / "uwvm"};
            if(::std::filesystem::exists(candidate)) { return candidate; }
#ifdef _WIN32
            auto const windows_candidate{directory / "uwvm.exe"};
            if(::std::filesystem::exists(windows_candidate)) { return windows_candidate; }
#endif
            if(directory == directory.root_path()) { return {}; }
            directory = directory.parent_path();
        }
    }

    [[nodiscard]] ::std::string quote(::std::filesystem::path const& path)
    { return ::std::string{"\""} + path.string() + "\""; }

    [[nodiscard]] ::std::vector<::std::uint8_t> make_cli_module(feature_case const& test_case)
    {
        // The validator fixtures intentionally contain no exports. Insert `(export "_start" (func 0))` immediately
        // before the code section so the real runner reaches backend validation instead of stopping at entry lookup.
        ::std::size_t section_offset{8uz};
        while(section_offset < test_case.size)
        {
            auto const section_begin{section_offset};
            auto const section_id{test_case.bytes[section_offset++]};
            ::std::size_t payload_size{};
            unsigned shift{};
            for(;;)
            {
                if(section_offset == test_case.size || shift >= 35u) { return {}; }
                auto const byte{test_case.bytes[section_offset++]};
                payload_size |= static_cast<::std::size_t>(byte & 0x7fu) << shift;
                if((byte & 0x80u) == 0u) { break; }
                shift += 7u;
            }
            if(payload_size > test_case.size - section_offset) { return {}; }
            if(section_id == 0x0au)
            {
                constexpr ::std::uint8_t start_export[]{
                    0x07, 0x0a, 0x01, 0x06, 0x5f, 0x73, 0x74, 0x61, 0x72, 0x74, 0x00, 0x00};
                ::std::vector<::std::uint8_t> result{};
                result.reserve(test_case.size + sizeof(start_export));
                result.insert(result.end(), test_case.bytes, test_case.bytes + section_begin);
                result.insert(result.end(), ::std::begin(start_export), ::std::end(start_export));
                result.insert(result.end(), test_case.bytes + section_begin, test_case.bytes + test_case.size);
                return result;
            }
            section_offset += payload_size;
        }
        return {};
    }

    [[nodiscard]] bool run_cli_rejection(::std::filesystem::path const& uwvm_path,
                                         ::std::filesystem::path const& artifact_directory,
                                         feature_case const& test_case,
                                         ::std::size_t const case_index,
                                         ::std::string_view const backend)
    {
        auto const wasm_path{artifact_directory / ("feature-" + ::std::to_string(case_index) + ".wasm")};
        auto const log_path{artifact_directory / ("feature-" + ::std::to_string(case_index) + "-" + ::std::string{backend} + ".log")};
        auto const cli_module{make_cli_module(test_case)};
        if(cli_module.empty()) { return false; }
        {
            ::std::ofstream output(wasm_path, ::std::ios::binary | ::std::ios::trunc);
            if(!output) { return false; }
            output.write(reinterpret_cast<char const*>(cli_module.data()), static_cast<::std::streamsize>(cli_module.size()));
            if(!output) { return false; }
        }

        auto command{quote(uwvm_path) + " -Rcm full -Rcc " + ::std::string{backend}};
        if(backend == "jit") { command += " -Rllvm-cache-path disable"; }
        command += " --wasm-feature-disable-" + ::std::string{test_case.cli_suffix} + " --run " + quote(wasm_path) + " > " + quote(log_path) + " 2>&1";
        auto const status{::std::system(command.c_str())};
        if(status == 0) { return false; }

        ::std::ifstream input(log_path, ::std::ios::binary);
        if(!input) { return false; }
        ::std::string log{::std::istreambuf_iterator<char>{input}, ::std::istreambuf_iterator<char>{}};
        if(log.find(test_case.cli_suffix) != ::std::string::npos) { return true; }
        // These two complete-module constraints are rejected by the shared structural decoder before either backend
        // receives a function. Their established diagnostics predate the named feature switches.
        if(test_case.feature == feature_id::multi_value)
        { return log.find("length of the result type vector") != ::std::string::npos; }
        if(test_case.feature == feature_id::multiple_tables)
        { return log.find("at most one table may be defined or imported") != ::std::string::npos; }
        return false;
    }

    [[nodiscard]] int validate_call_indirect_encoding(::std::uint8_t const* const input,
                                                       ::std::size_t const input_size,
                                                       feature_parameter const& policy,
                                                       error_code const expected,
                                                       char const* const case_name)
    {
        strict::byte_vec bytes{};
        bytes.reserve(input_size);
        for(::std::size_t i{}; i != input_size; ++i) { bytes.push_back(static_cast<::std::byte>(input[i])); }

        auto prepared{strict::prepare_runtime_from_wasm(bytes, u8"call_indirect_reserved_encoding", {}, policy)};
        if(prepared.mod == nullptr || prepared.mod->local_defined_function_vec_storage.size() != 1uz)
        {
            ::std::fprintf(stderr, "call_indirect_reserved_encoding[%s]: runtime preparation failed\n", case_name);
            return 1;
        }

        validation_error local_storage_error{};
        auto const validation_module{llvm_details::build_runtime_validation_module(*prepared.mod)};
        auto const local_function{llvm_details::get_runtime_local_func_storage(*prepared.mod, 0uz, local_storage_error)};
        constexpr ::std::size_t call_indirect_opcode_offset{2uz};
        if(static_cast<::std::size_t>(local_function.code_end - local_function.code_begin) <= call_indirect_opcode_offset ||
           local_function.code_begin[call_indirect_opcode_offset] != ::std::byte{0x11u})
        {
            ::std::fprintf(stderr, "call_indirect_reserved_encoding[%s]: fixture opcode offset mismatch\n", case_name);
            return 1;
        }
        auto const* const call_indirect_opcode{local_function.code_begin + call_indirect_opcode_offset};
        auto const standard_error{validate_standard(validation_module, local_function, policy)};
#ifndef UWVM_DISABLE_INT
        auto const int_error{validate_uwvm_int(*prepared.mod, policy)};
#endif
        auto const llvm_result{compile_llvm_call_indirect(*prepared.mod, policy)};

        if(standard_error.err_code != expected || llvm_result.error.err_code != expected
#ifndef UWVM_DISABLE_INT
           || int_error.err_code != expected
#endif
        )
        {
            ::std::fprintf(stderr, "call_indirect_reserved_encoding[%s]: validator result mismatch\n", case_name);
            return 1;
        }
        auto error_cursor_matches{standard_error.err_curr == call_indirect_opcode &&
                                  llvm_result.error.err_curr == call_indirect_opcode};
#ifndef UWVM_DISABLE_INT
        error_cursor_matches = error_cursor_matches && int_error.err_curr == call_indirect_opcode;
#endif
        if(expected != error_code::ok && !error_cursor_matches)
        {
            ::std::fprintf(stderr, "call_indirect_reserved_encoding[%s]: error cursor is not the opcode\n", case_name);
            return 1;
        }
        if(expected == error_code::ok && !llvm_result.materialized)
        {
            ::std::fprintf(stderr, "call_indirect_reserved_encoding[%s]: LLVM module materialization failed\n", case_name);
            return 1;
        }
        if(expected == error_code::wasm2_feature_required && !same_feature_diagnostic(standard_error, llvm_result.error))
        {
            ::std::fprintf(stderr, "call_indirect_reserved_encoding[%s]: feature diagnostic mismatch\n", case_name);
            return 1;
        }
#ifndef UWVM_DISABLE_INT
        if(expected == error_code::wasm2_feature_required && !same_feature_diagnostic(standard_error, int_error))
        {
            ::std::fprintf(stderr, "call_indirect_reserved_encoding[%s]: uwvm-int feature diagnostic mismatch\n", case_name);
            return 1;
        }
#endif
        return 0;
    }
}

int main(int argc, char** argv)
{
    if(argc == 0 || argv == nullptr || argv[0] == nullptr) { return 1; }
    auto const executable{::std::filesystem::absolute(argv[0])};
    auto const uwvm_path{find_uwvm_binary(executable.parent_path())};
    if(uwvm_path.empty()) { return 1; }
    auto const artifact_directory{executable.parent_path() / "test-artifacts" / "0014.llvm_jit" / "wasm2-feature-validator-parity"};
    ::std::error_code filesystem_error{};
    ::std::filesystem::create_directories(artifact_directory, filesystem_error);
    if(filesystem_error) { return 1; }

    for(::std::size_t case_index{}; case_index != ::std::size(cases); ++case_index)
    {
        auto const& test_case{cases[case_index]};
        auto const bytes{make_bytes(test_case)};
        auto const all_enabled{strict::make_wasm2_feature_parameter()};
        auto prepared{strict::prepare_runtime_from_wasm(bytes, test_case.module_name, {}, all_enabled)};
        if(prepared.mod == nullptr) { return fail(case_index, "runtime preparation failed"); }
        if(prepared.mod->local_defined_function_vec_storage.size() != 1uz) { return fail(case_index, "fixture must contain one local function"); }

        validation_error local_storage_error{};
        auto const validation_module{llvm_details::build_runtime_validation_module(*prepared.mod)};
        auto const local_function{llvm_details::get_runtime_local_func_storage(*prepared.mod, 0uz, local_storage_error)};
        auto const policy{make_disabled_policy(test_case.feature)};

        auto const standard_error{validate_standard(validation_module, local_function, policy)};
#ifndef UWVM_DISABLE_INT
        auto const int_error{validate_uwvm_int(*prepared.mod, policy)};
#endif
        auto const llvm_error{validate_llvm_internal(validation_module, local_function, policy)};

        if(!expected_feature(standard_error, test_case.feature)) { return fail(case_index, "standard validator selected the wrong feature error"); }
#ifndef UWVM_DISABLE_INT
        if(!same_feature_diagnostic(standard_error, int_error)) { return fail(case_index, "uwvm-int feature diagnostic differs from the standard validator"); }
#endif
        if(!same_feature_diagnostic(standard_error, llvm_error)) { return fail(case_index, "LLVM feature diagnostic differs from the standard validator"); }

        if(standard_error.err_curr == nullptr || llvm_error.err_curr == nullptr
#ifndef UWVM_DISABLE_INT
           || int_error.err_curr == nullptr
#endif
        )
        {
            return fail(case_index, "validator did not report an instruction location");
        }
        auto const standard_offset{standard_error.err_curr - local_function.code_begin};
#ifndef UWVM_DISABLE_INT
        auto const int_offset{int_error.err_curr - local_function.code_begin};
#endif
        auto const llvm_offset{llvm_error.err_curr - local_function.code_begin};
        if(standard_offset != llvm_offset
#ifndef UWVM_DISABLE_INT
           || standard_offset != int_offset
#endif
        )
        {
            return fail(case_index, "validator first-error offsets differ");
        }

        // Exercise the real command parser and loader with both standalone backends. The process must fail specifically
        // with the disabled feature name, not merely later because these compact validation fixtures have no entry export.
#ifndef UWVM_DISABLE_INT
        if(!run_cli_rejection(uwvm_path, artifact_directory, test_case, case_index, "int"))
        {
            return fail(case_index, "uwvm-int CLI did not reject the disabled feature");
        }
#endif
        if(!run_cli_rejection(uwvm_path, artifact_directory, test_case, case_index, "jit"))
        {
            return fail(case_index, "LLVM-JIT CLI did not reject the disabled feature");
        }
    }

    auto const wasm1p1_policy{strict::make_wasm1p1_feature_parameter()};
    if(validate_call_indirect_encoding(call_indirect_reserved_zero_module,
                                       sizeof(call_indirect_reserved_zero_module),
                                       wasm1p1_policy,
                                       error_code::ok,
                                       "wasm1p1-canonical") != 0)
    { return 1; }
    if(validate_call_indirect_encoding(call_indirect_nonminimal_zero_module,
                                       sizeof(call_indirect_nonminimal_zero_module),
                                       wasm1p1_policy,
                                       error_code::ok,
                                       "wasm1p1-nonminimal") != 0)
    { return 1; }

    auto mvp_policy{wasm1p1_policy};
    ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(mvp_policy).cli_mode =
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode::direct_wasmmvp;
    if(validate_call_indirect_encoding(call_indirect_reserved_zero_module,
                                       sizeof(call_indirect_reserved_zero_module),
                                       mvp_policy,
                                       error_code::ok,
                                       "mvp-canonical-reserved-byte") != 0)
    { return 1; }
    if(validate_call_indirect_encoding(call_indirect_nonminimal_zero_module,
                                       sizeof(call_indirect_nonminimal_zero_module),
                                       mvp_policy,
                                       error_code::invalid_table_index,
                                       "mvp-nonminimal-reserved-byte") != 0)
    { return 1; }
    if(validate_call_indirect_encoding(call_indirect_invalid_type_nonzero_table_module,
                                       sizeof(call_indirect_invalid_type_nonzero_table_module),
                                       mvp_policy,
                                       error_code::invalid_table_index,
                                       "mvp-invalid-type-bad-reserved-byte") != 0)
    { return 1; }

    auto const wasm2_policy{strict::make_wasm2_feature_parameter()};
    if(validate_call_indirect_encoding(call_indirect_nonminimal_zero_module,
                                       sizeof(call_indirect_nonminimal_zero_module),
                                       wasm2_policy,
                                       error_code::ok,
                                       "wasm2-multiple-tables-enabled") != 0)
    { return 1; }

    auto wasm2_single_table_policy{wasm2_policy};
    auto& wasm2_single_table_para{::uwvm2::parser::wasm::standard::wasm2::features::get_wasm2_parameter(wasm2_single_table_policy)};
    wasm2_single_table_para.cli_mode = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode::scoped;
    wasm2_single_table_para.disable_multiple_tables = true;
    if(validate_call_indirect_encoding(call_indirect_nonminimal_zero_module,
                                       sizeof(call_indirect_nonminimal_zero_module),
                                       wasm2_single_table_policy,
                                       error_code::ok,
                                       "wasm2-nonminimal-zero-single-table") != 0)
    { return 1; }
    if(validate_call_indirect_encoding(call_indirect_nonzero_table_module,
                                       sizeof(call_indirect_nonzero_table_module),
                                       wasm2_single_table_policy,
                                       error_code::wasm2_feature_required,
                                       "wasm2-nonzero-single-table") != 0)
    { return 1; }
    if(validate_call_indirect_encoding(call_indirect_invalid_type_overflowing_table_module,
                                       sizeof(call_indirect_invalid_type_overflowing_table_module),
                                       wasm2_single_table_policy,
                                       error_code::invalid_table_index,
                                       "wasm2-invalid-type-bad-table-encoding") != 0)
    { return 1; }
    if(validate_call_indirect_encoding(call_indirect_invalid_type_nonzero_table_module,
                                       sizeof(call_indirect_invalid_type_nonzero_table_module),
                                       wasm2_single_table_policy,
                                       error_code::illegal_type_index,
                                       "wasm2-invalid-type-nonzero-table") != 0)
    { return 1; }
    return 0;
}

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#ifndef UWVM
# define UWVM 2
#endif
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/uwvm/cmdline/callback/wasm_feature.h>
#include <fstream>

namespace
{
    namespace strict = uwvm2test::uwvm_int_strict;
    using bytes = strict::byte_vec;
    using feature_parameter = strict::wasm_feature_parameter_t;

    void require(bool value) { if(!value) { std::abort(); } }

    void section(bytes& out, unsigned id, std::initializer_list<unsigned> payload)
    {
        require(payload.size() < 128);
        out.push_back(static_cast<std::byte>(id));
        out.push_back(static_cast<std::byte>(payload.size()));
        for(auto b: payload) { out.push_back(static_cast<std::byte>(b)); }
    }

    bytes header() { return bytes{std::byte{0}, std::byte{97}, std::byte{115}, std::byte{109}, std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}}; }

    bytes module(std::initializer_list<unsigned> globals)
    {
        bytes result{};
        for(auto b: {0u, 97u, 115u, 109u, 1u, 0u, 0u, 0u, 6u, static_cast<unsigned>(globals.size())})
        {
            result.push_back(static_cast<std::byte>(b));
        }
        for(auto b: globals) { result.push_back(static_cast<std::byte>(b)); }
        return result;
    }

    feature_parameter features(bool enabled)
    {
        feature_parameter result{};
        uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(result).disable_extended_const = !enabled;
        return result;
    }

    bool valid(bytes const& wasm, bool enabled, bool table_initializer = false)
    {
        uwvm2::parser::wasm::base::error_impl error{};
        try
        {
            auto parameters = features(enabled);
            uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(parameters).disable_table_initializer = !table_initializer;
            auto parsed = uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(wasm.data(), wasm.data() + wasm.size(), error, parameters);
            return true;
        }
        catch(...) { return false; }
    }
}

int main(int argc, char** argv)
{
    // Small parser entry point for binary modules extracted from the official specification suite.
    if(argc == 3 && std::string_view{argv[1]} == "--parse")
    {
        std::ifstream file(argv[2], std::ios::binary);
        if(!file) { return 2; }
        bytes wasm{};
        for(char c; file.get(c);) { wasm.push_back(static_cast<std::byte>(static_cast<unsigned char>(c))); }
        if(!file.eof()) { return 2; }
        return valid(wasm, true, true) ? 0 : 1;
    }
    // CLI ownership and independence: one switch cannot silently enable another feature group.
    namespace cli = uwvm2::uwvm::cmdline::params::details;
    using result = uwvm2::utils::cmdline::parameter_return_type;
    auto& cli_features = cli::wasm_feature_details::wasm1p1_parameter();
    cli_features = {};
    uwvm2::utils::cmdline::parameter_parsing_results argument{};
    argument.str = u8"--wasm-feature-enable-extended-const";
    require(cli::wasm_feature_enable_extended_const_callback(nullptr, &argument, nullptr) == result::def);
    require(!cli_features.disable_extended_const);
    require(!cli_features.disable_simd && !cli_features.disable_multi_value);
    argument.str = u8"--wasm-feature-disable-simd";
    require(cli::wasm_feature_disable_simd_callback(nullptr, &argument, nullptr) == result::def);
    require(cli_features.disable_simd && !cli_features.disable_extended_const);
    argument.str = u8"--wasm-feature-disable-extended-const";
    require(cli::wasm_feature_disable_extended_const_callback(nullptr, &argument, nullptr) == result::return_m1_imme);
    argument.str = u8"--wasm-feature-wasm2";
    require(cli::wasm_feature_wasm2_callback(nullptr, &argument, nullptr) == result::return_m1_imme);
    cli_features = {};
    require(cli::wasm_feature_wasm2_callback(nullptr, &argument, nullptr) == result::def);
    require(cli_features.disable_extended_const);
    argument.str = u8"--wasm-feature-enable-extended-const";
    require(cli::wasm_feature_enable_extended_const_callback(nullptr, &argument, nullptr) == result::return_m1_imme);
    cli_features = {};
    require(cli_features.disable_table_initializer && cli_features.disable_extended_const);
    argument.str = u8"--wasm-feature-enable-table-initializer";
    require(cli::wasm_feature_enable_table_initializer_callback(nullptr, &argument, nullptr) == result::def);
    require(!cli_features.disable_table_initializer && cli_features.disable_extended_const);
    argument.str = u8"--wasm-feature-disable-table-initializer";
    require(cli::wasm_feature_disable_table_initializer_callback(nullptr, &argument, nullptr) == result::return_m1_imme);
    cli_features = {};

    // Independent gates, nesting, modulo arithmetic, order of subtraction, and prior local globals.
    auto wasm = module({3,
        0x7f, 0, 0x41, 0x7f, 0x41, 2, 0x6a, 0x41, 3, 0x6c, 0x0b,
        0x7f, 1, 0x23, 0, 0x41, 7, 0x6b, 0x0b,
        0x7e, 0, 0x42, 0x7f, 0x42, 2, 0x7c, 0x42, 7, 0x7e, 0x42, 9, 0x7d, 0x0b});
    require(valid(wasm, true));
    require(!valid(wasm, false));
#if defined(__unix__) || defined(__APPLE__)
    // Receiving an already parsed module must not bypass the initializer's independent feature policy.
    require(strict::run_in_child_expect_trap_message("requires --wasm-feature-enable-extended-const", [&]
    {
        uwvm2::uwvm::io::u8log_output.reopen(::fast_io::io_dup, ::fast_io::u8err());
        uwvm2::parser::wasm::base::error_impl error{};
        auto parsed = uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(wasm.data(), wasm.data() + wasm.size(), error, features(true));
        uwvm2::uwvm::runtime::initializer::details::enforce_wasm1p1_initializer_feature_parameters(parsed, features(false));
    }) == 0);
#endif
    auto runtime = strict::prepare_runtime_from_wasm(wasm, u8"extended_const", {}, features(true));
    auto const& globals = runtime.mod->local_defined_global_vec_storage;
    require(globals.size() == 3);
    require(globals.index_unchecked(0).global.storage.i32 == 3);
    require(globals.index_unchecked(1).global.storage.i32 == -4);
    require(globals.index_unchecked(1).global.is_mutable);
    require(globals.index_unchecked(2).global.storage.i64 == -2);

    require(valid(module({1, 0x7f, 0, 0x41, 1, 0x0b}), false));
    require(!valid(module({1, 0x7f, 0, 0x0b}), true));
    require(!valid(module({1, 0x7f, 0, 0x6a, 0x0b}), true));
    require(!valid(module({1, 0x7f, 0, 0x41, 1, 0x6a, 0x0b}), true));
    require(!valid(module({1, 0x7f, 0, 0x41, 1, 0x41, 2, 0x0b}), true));
    require(!valid(module({1, 0x7f, 0, 0x41, 1, 0x42, 2, 0x6a, 0x0b}), true));
    require(!valid(module({1, 0x7f, 0, 0x41, 1, 0x41, 2, 0x6d, 0x0b}), true));
    require(!valid(module({1, 0x7f, 0, 0x23, 0, 0x0b}), true));
    require(!valid(module({2, 0x7f, 0, 0x23, 1, 0x0b, 0x7f, 0, 0x41, 1, 0x0b}), true));
    require(!valid(module({2, 0x7f, 1, 0x41, 1, 0x0b, 0x7f, 0, 0x23, 0, 0x0b}), true));
    // Every truncated prefix must fail without accessing the missing bytes.
    for(std::size_t n = 10; n < wasm.size(); ++n)
    {
        bytes truncated(wasm.begin(), wasm.begin() + n);
        // Keep the outer section length correct, so the decoder actually reaches the truncated expression.
        truncated[9] = static_cast<std::byte>(n - 10);
        require(!valid(truncated, true));
    }

    // Local reads of non-integer values must preserve their complete representation.
    auto floating = module({2, 0x7d, 0, 0x43, 1, 0, 0x80, 0x7f, 0x0b, 0x7d, 0, 0x23, 0, 0x0b});
    auto float_runtime = strict::prepare_runtime_from_wasm(floating, u8"float_const", {}, features(true));
    std::uint32_t bits{};
    std::memcpy(&bits, &float_runtime.mod->local_defined_global_vec_storage.index_unchecked(1).global.storage.f32, sizeof(bits));
    require(bits == 0x7f800001u);

    // Active segment offsets share the extended-expression grammar, including local globals.
    auto segments = header();
    section(segments, 4, {1, 0x70, 0, 8});
    section(segments, 5, {1, 0, 1});
    section(segments, 6, {2, 0x7f, 0, 0x41, 3, 0x0b, 0x70, 0, 0xd0, 0x70, 0x0b});
    section(segments, 9, {1, 4, 0x23, 0, 0x41, 2, 0x6a, 0x0b, 1, 0x23, 1, 0x0b});
    section(segments, 11, {1, 0, 0x23, 0, 0x41, 7, 0x6c, 0x0b, 1, 0xab});
    require(valid(segments, true));
    require(!valid(segments, false));
    auto segment_runtime = strict::prepare_runtime_from_wasm(segments, u8"segment_const", {}, features(true));
    require(segment_runtime.mod->local_defined_element_vec_storage.index_unchecked(0).element.offset == 5);
    require(segment_runtime.mod->local_defined_data_vec_storage.index_unchecked(0).data.offset == 21);
    require(segment_runtime.mod->local_defined_memory_vec_storage.index_unchecked(0).memory.memory_begin[21] == std::byte{0xab});

    // Link before evaluation: imported global values are not available during parsing.
    auto provider = module({1, 0x7f, 0, 0x41, 5, 0x0b});
    section(provider, 7, {1, 1, 'x', 3, 0});
    auto consumer = header();
    section(consumer, 2, {1, 1, 'p', 1, 'x', 3, 0x7f, 0});
    section(consumer, 6, {1, 0x7f, 0, 0x23, 0, 0x41, 7, 0x6c, 0x0b});
    auto import_runtime = strict::prepare_runtime_from_wasm(consumer, u8"import_const", {{&provider, u8"p"}}, features(true));
    require(import_runtime.mod->local_defined_global_vec_storage.index_unchecked(0).global.storage.i32 == 35);
    // Core 3 explicit table initializers have their own opt-in gate, independent of integer expressions.
    auto table_parameters = features(false);
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(table_parameters).disable_table_initializer = false;
    auto table_module = header();
    section(table_module, 1, {1, 0x60, 0, 1, 0x7f});
    section(table_module, 3, {1, 0});
    section(table_module, 4, {3,
        0x40, 0, 0x70, 0, 3, 0xd2, 0, 0x0b,
        0x40, 0, 0x6f, 0, 2, 0xd0, 0x6f, 0x0b,
        0x70, 0, 1});
    // Active element segments overwrite the broadcast initializer, including with ref.null.
    section(table_module, 9, {1, 4, 0x41, 1, 0x0b, 1, 0xd0, 0x70, 0x0b});
    section(table_module, 10, {1, 4, 0, 0x41, 7, 0x0b});
    require(valid(table_module, false, true));
    require(!valid(table_module, true, false));
#if defined(__unix__) || defined(__APPLE__)
    require(strict::run_in_child_expect_trap_message("requires --wasm-feature-enable-table-initializer", [&]
    {
        uwvm2::uwvm::io::u8log_output.reopen(::fast_io::io_dup, ::fast_io::u8err());
        uwvm2::parser::wasm::base::error_impl error{};
        auto parsed = uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(
            table_module.data(), table_module.data() + table_module.size(), error, table_parameters);
        uwvm2::uwvm::runtime::initializer::details::enforce_wasm1p1_initializer_feature_parameters(parsed, features(false));
    }) == 0);
#endif
    auto table_runtime = strict::prepare_runtime_from_wasm(table_module, u8"table_init", {}, table_parameters);
    auto const& tables = table_runtime.mod->local_defined_table_vec_storage;
    using element_kind = uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
    require(tables.size() == 3);
    auto const& function = table_runtime.mod->local_defined_function_vec_storage.index_unchecked(0);
    require(tables.index_unchecked(0).elems.index_unchecked(0).type == element_kind::func_ref_defined);
    require(tables.index_unchecked(0).elems.index_unchecked(0).storage.defined_ptr == &function);
    require(tables.index_unchecked(0).elems.index_unchecked(2).storage.defined_ptr == &function);
    require(tables.index_unchecked(0).elems.index_unchecked(1).type == element_kind{});
    require(tables.index_unchecked(1).elems.index_unchecked(0).type == element_kind::extern_ref);
    require(tables.index_unchecked(1).elems.index_unchecked(1).storage.extern_ptr == nullptr);
    require(tables.index_unchecked(2).elems.index_unchecked(0).type == element_kind{});
    require(table_runtime.mod->declared_ref_funcidx_vec_storage.size() == 1);
    require(table_runtime.mod->declared_ref_funcidx_vec_storage.front_unchecked() == 0);

    for(auto bad_payload: {std::initializer_list<unsigned>{1, 0x40},
            {1, 0x40, 1, 0x70, 0, 0, 0xd0, 0x70, 0x0b},
            {1, 0x40, 0, 0x70, 0, 0, 0x0b},
            {1, 0x40, 0, 0x70, 0, 0, 0xd0, 0x6f, 0x0b},
            {1, 0x40, 0, 0x70, 0, 0, 0xd2, 0, 0x0b},
            {1, 0x40, 0, 0x70, 0, 0, 0x23, 0, 0x0b}})
    {
        auto malformed = header();
        section(malformed, 4, bad_payload);
        require(!valid(malformed, true, true));
    }
    auto table_prefix = header();
    section(table_prefix, 4, {1, 0x40, 0, 0x70, 0, 2, 0xd0, 0x70, 0x0b});
    for(std::size_t n = 11; n < table_prefix.size(); ++n)
    {
        bytes truncated(table_prefix.begin(), table_prefix.begin() + n);
        truncated[9] = static_cast<std::byte>(n - 10);
        require(!valid(truncated, true, true));
    }
    auto invalid_import = header();
    section(invalid_import, 2, {1, 1, 'p', 1, 't', 1, 0x40, 0, 0x70, 0, 0, 0xd0, 0x70, 0x0b});
    require(!valid(invalid_import, true, true));

    // A table initializer's imported funcref retains the provider's identity, not the consumer's function index.
    auto ref_provider = header();
    section(ref_provider, 1, {1, 0x60, 0, 0});
    section(ref_provider, 3, {1, 0});
    section(ref_provider, 6, {1, 0x70, 0, 0xd2, 0, 0x0b});
    section(ref_provider, 7, {1, 1, 'r', 3, 0});
    section(ref_provider, 10, {1, 2, 0, 0x0b});
    auto ref_consumer = header();
    section(ref_consumer, 2, {1, 1, 'p', 1, 'r', 3, 0x70, 0});
    section(ref_consumer, 4, {1, 0x40, 0, 0x70, 0, 2, 0x23, 0, 0x0b});
    require(valid(ref_consumer, false, true));
    auto linked_tables = strict::prepare_runtime_from_wasm(ref_consumer, u8"table_import", {{&ref_provider, u8"p"}}, table_parameters);
    auto const& linked_element = linked_tables.mod->local_defined_table_vec_storage.index_unchecked(0).elems.index_unchecked(0);
    require(linked_element.type == element_kind::func_ref_defined);
    auto provider_runtime = uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.find(u8"p");
    require(provider_runtime != uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.end());
    require(linked_element.storage.defined_ptr == &provider_runtime->second.local_defined_function_vec_storage.index_unchecked(0));
    std::puts("wasm3 extended constants and table initializers: parser and initializer passed");
}

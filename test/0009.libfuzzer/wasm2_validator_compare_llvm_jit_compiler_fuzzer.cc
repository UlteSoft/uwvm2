/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       Differential fuzzing for wasm2 standard validation and the LLVM JIT internal validator.
 * @author      GPT
 * @version     2.0.0
 */

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <utility>

#ifndef UWVM_MODULE
# include <fast_io.h>

# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/validation/error/error.h>
# include <uwvm2/validation/standard/wasm2/impl.h>

# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/runtime/initializer/init.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/loader/load_and_check_modules.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>

# include <uwvm2/uwvm/cmdline/callback/impl.h>

# include "wasm1_code_section_module_builder.h"
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using validation_error_t = ::uwvm2::validation::error::code_validation_error_impl;

    [[nodiscard]] inline constexpr bool same_feature_diagnostic(validation_error_t const& left,
                                                                 validation_error_t const& right) noexcept
    {
        using error_code = ::uwvm2::validation::error::code_validation_error_code;
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

    [[maybe_unused]] inline constexpr void fuzz_trap() noexcept
    {
#if defined(__clang__) || defined(__GNUC__)
        __builtin_trap();
#else
        ::fast_io::fast_terminate();
#endif
    }
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    using code_validation_error_code = ::uwvm2::validation::error::code_validation_error_code;
    using feature_parameter_t = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t;

    try
    {
        if(data == nullptr || size == 0uz || size > (1u << 20u)) { return 0; }

        // The first byte independently controls all eight Release 2.0 feature groups. Marking the policy scoped is
        // essential: binary-format version 1 is shared with wasm1p1, so the runtime validator must not infer wasm2 from
        // the compile-time storage tuple.
        auto const feature_mask{data[0]};
        ++data;
        --size;

        feature_parameter_t runtime_feature_parameter{};
        auto& wasm2_parameter{::uwvm2::parser::wasm::standard::wasm2::features::get_wasm2_parameter(runtime_feature_parameter)};
        using cli_mode = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode;
        wasm2_parameter.cli_mode = cli_mode::scoped;
        wasm2_parameter.disable_multi_value = (feature_mask & 0x01u) == 0u;
        wasm2_parameter.disable_reference_types = (feature_mask & 0x02u) == 0u;
        wasm2_parameter.disable_table_instructions = (feature_mask & 0x04u) == 0u;
        wasm2_parameter.disable_multiple_tables = (feature_mask & 0x08u) == 0u;
        wasm2_parameter.disable_bulk_memory = (feature_mask & 0x10u) == 0u;
        wasm2_parameter.disable_sign_extension = (feature_mask & 0x20u) == 0u;
        wasm2_parameter.disable_nontrapping_float_to_int = (feature_mask & 0x40u) == 0u;
        wasm2_parameter.disable_simd = (feature_mask & 0x80u) == 0u;
        wasm2_parameter.controllable_allow_multi_result_vector = false;
        wasm2_parameter.controllable_allow_multi_table = false;

        // Keep decoding/runtime construction independent from the randomized validation policy. Parsing with disabled
        // features would hide precisely the typeidx/multiple-table cases this target must feed to LLVM's internal
        // validator, because the section parser can reject them before the backend comparison begins.
        feature_parameter_t parser_feature_parameter{};

        auto const mod{::test::wasm1_code_section_module_builder::build_module_from_code_validation_bytes(data, size)};
        auto const* begin{reinterpret_cast<::std::byte const*>(mod.data())};
        auto const* end{begin + mod.size()};

        ::uwvm2::parser::wasm::base::error_impl parse_err{};
        ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t parsed_module{};
        try
        {
            parsed_module = ::uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(begin, end, parse_err, parser_feature_parameter);
        }
        catch(::fast_io::error const&)
        {
            return 0;
        }

        constexpr auto get_importsec{
            []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,
                                                                     ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept
                -> decltype(auto)
            {
                return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(sections);
            }};
        constexpr auto get_codesec{
            []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,
                                                                     ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept
                -> decltype(auto)
            {
                return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections);
            }};
        constexpr auto get_tablesec{
            []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,
                                                                     ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept
                -> decltype(auto)
            {
                return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(sections);
            }};
        constexpr auto get_memorysec{
            []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,
                                                                     ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept
                -> decltype(auto)
            {
                return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>(sections);
            }};

        auto const& importsec{get_importsec(parsed_module.sections, ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features)};
        auto const& codesec{get_codesec(parsed_module.sections, ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features)};
        auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};

        validation_error_t standard_err{};
        for(::std::size_t local_index{}; local_index != codesec.codes.size(); ++local_index)
        {
            auto const& code{codesec.codes.index_unchecked(local_index)};
            auto const* code_begin{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
            auto const* code_end{reinterpret_cast<::std::byte const*>(code.body.code_end)};
            standard_err = {};
            try
            {
                ::uwvm2::validation::standard::wasm2::validate_code_with_runtime_policy(parsed_module,
                                                                                         import_func_count + local_index,
                                                                                         code_begin,
                                                                                         code_end,
                                                                                         standard_err,
                                                                                         runtime_feature_parameter);
            }
            catch(::fast_io::error const&)
            {
                break;
            }
            if(standard_err.err_code != code_validation_error_code::ok) { break; }
        }

        // Avoid turning valid large-limit inputs into allocator/OOM fuzzing. Imported memories/tables do not allocate in
        // this locally constructed runtime record; only local definitions need the guard.
        auto const& tablesec{get_tablesec(parsed_module.sections, ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features)};
        auto const& memorysec{get_memorysec(parsed_module.sections, ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features)};
        for(auto const& table_type: tablesec.tables)
        {
            if(static_cast<::std::size_t>(table_type.limits.min) > 65536uz) { return 0; }
        }
        for(auto const& memory_type: memorysec.memories)
        {
            if(static_cast<::std::size_t>(memory_type.limits.min) > 256uz) { return 0; }
        }

        ::uwvm2::uwvm::io::show_verbose = false;
        ::uwvm2::uwvm::io::show_depend_warning = false;
        ::uwvm2::uwvm::wasm::storage::all_module.clear();
        ::uwvm2::uwvm::wasm::storage::all_module_export.clear();
        ::uwvm2::uwvm::wasm::storage::preloaded_wasm.clear();
#if defined(UWVM_SUPPORT_PRELOAD_DL)
        ::uwvm2::uwvm::wasm::storage::preloaded_dl.clear();
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        ::uwvm2::uwvm::wasm::storage::weak_symbol.clear();
#endif
        ::uwvm2::uwvm::wasm::storage::preload_local_imported.clear();

        ::uwvm2::uwvm::wasm::storage::execute_wasm = ::uwvm2::uwvm::wasm::type::wasm_file_t{1u};
        ::uwvm2::uwvm::wasm::storage::execute_wasm.file_name = u8"fuzz.wasm";
        ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"fuzz";
        ::uwvm2::uwvm::wasm::storage::execute_wasm.binfmt_ver = 1u;
        ::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_parameter.binfmt1_para = parser_feature_parameter;
        ::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage = ::std::move(parsed_module);

        if(::uwvm2::uwvm::wasm::loader::construct_all_module_and_check_duplicate_module() !=
           ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok)
        {
            return 0;
        }
        if(::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles() !=
           ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok)
        {
            return 0;
        }

        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.clear();
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.reserve(1uz);
        ::uwvm2::uwvm::runtime::initializer::details::import_alias_sanity_checked = false;
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t runtime_module{};
        ::uwvm2::uwvm::runtime::initializer::details::current_initializing_module_name = u8"fuzz";
        ::uwvm2::uwvm::runtime::initializer::details::initialize_from_wasm_file(
            ::uwvm2::uwvm::wasm::storage::execute_wasm, runtime_module);
        ::uwvm2::uwvm::runtime::initializer::details::current_initializing_module_name = {};
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.try_emplace(u8"fuzz", ::std::move(runtime_module));

        auto runtime_it{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.find(u8"fuzz")};
        if(runtime_it == ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.end()) { return 0; }

        // Do not use either public LLVM entry here: both perform the canonical standard-validation prepass before
        // reaching LLVM's independent validator. Build its synthetic runtime validation module and invoke the internal
        // per-function validator directly so this comparison measures the first error selected by each implementation.
        namespace llvm_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
        ::uwvm2::validation::error::code_validation_error_impl llvm_internal_err{};
        try
        {
            auto const validation_module{llvm_details::build_runtime_validation_module(runtime_it->second)};
            auto const local_func_count{runtime_it->second.local_defined_function_vec_storage.size()};

            for(::std::size_t local_index{}; local_index != local_func_count; ++local_index)
            {
                auto const local_func_storage{
                    llvm_details::get_runtime_local_func_storage(runtime_it->second, local_index, llvm_internal_err)};
                llvm_details::validate_runtime_local_func(validation_module,
                                                          local_func_storage,
                                                          llvm_internal_err,
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
                                                          ::std::addressof(runtime_feature_parameter));
                if(llvm_internal_err.err_code != code_validation_error_code::ok) { break; }
            }
        }
        catch(::fast_io::error const&)
        {
        }

        // Emission is deliberately omitted from this validator differential target. If emission coverage is added later,
        // it must run only after both independent validators accepted the input, so a standard prepass cannot mask an
        // LLVM-internal validation discrepancy.
        if(!same_feature_diagnostic(standard_err, llvm_internal_err)) [[unlikely]]
        {
            ::std::fprintf(stderr,
                           "wasm2 LLVM validator mismatch: mask=0x%02x standard=%u compiler=%u input_size=%zu\n",
                           static_cast<unsigned>(feature_mask),
                           static_cast<unsigned>(standard_err.err_code),
                           static_cast<unsigned>(llvm_internal_err.err_code),
                           size);
            ::std::fflush(stderr);
            fuzz_trap();
        }
        return 0;
    }
    catch(...)
    {
        return 0;
    }
}

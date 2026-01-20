/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
 * @version     2.0.0
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#include <cstddef>
#include <cstdint>
#include <utility>

#ifndef UWVM_MODULE
# include <fast_io.h>

# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/validation/error/error.h>
# include <uwvm2/validation/standard/wasm1/impl.h>

# include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/wasm1.h>

# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/runtime/initializer/init.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/loader/load_and_check_modules.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>

// callback
# include <uwvm2/uwvm/cmdline/callback/impl.h>

#else
# error "Module testing is not currently supported"
#endif

namespace
{
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
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using code_validation_error_code = ::uwvm2::validation::error::code_validation_error_code;

    try
    {
        if(size > (1u << 20u)) { return 0; }
        if(data == nullptr) { return 0; }

        auto const* begin = reinterpret_cast<::std::byte const*>(data);
        auto const* end = begin + size;

        // Phase 1: parser check (must pass before running validators).
        ::uwvm2::parser::wasm::base::error_impl parse_err{};
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Feature> module_storage{};
        try
        {
            module_storage = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<Feature>(begin, end, parse_err, {});
        }
        catch(::fast_io::error const&)
        {
            return 0;
        }

        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Feature>>(module_storage.sections)};
        auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};

        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Feature>>(module_storage.sections)};

        // Temporary limitation: the compiler-side validator needs type section pointers for call_indirect.
        // Skip inputs that might contain call_indirect (0x11) anywhere in function bodies.
        for(::std::size_t local_idx{}; local_idx < codesec.codes.size(); ++local_idx)
        {
            auto const& code{codesec.codes.index_unchecked(local_idx)};
            auto const* p = reinterpret_cast<::std::byte const*>(code.body.expr_begin);
            auto const* q = reinterpret_cast<::std::byte const*>(code.body.code_end);
            for(; p != q; ++p)
            {
                if(*p == ::std::byte{0x11}) { return 0; }
            }
        }

        // Phase 2 (standard validation): find the first code-validation error (or ok).
        code_validation_error_code std_code{code_validation_error_code::ok};
        for(::std::size_t local_idx{}; local_idx < codesec.codes.size(); ++local_idx)
        {
            auto const& code{codesec.codes.index_unchecked(local_idx)};
            auto const* const code_begin_ptr{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
            auto const* const code_end_ptr{reinterpret_cast<::std::byte const*>(code.body.code_end)};

            ::uwvm2::validation::error::code_validation_error_impl v_err{};
            try
            {
                ::uwvm2::validation::standard::wasm1::validate_code<Feature>(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version{},
                                                                             module_storage,
                                                                             import_func_count + local_idx,
                                                                             code_begin_ptr,
                                                                             code_end_ptr,
                                                                             v_err);
            }
            catch(::fast_io::error const&)
            {
                std_code = v_err.err_code;
                break;
            }
        }

        // Phase 3 (runtime init for compiler path): parser -> init -> compile (captures compiler-side code-validation error).
        ::uwvm2::parser::wasm::base::error_impl rt_parse_err{};
        ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t rt_parsed_module_storage{};
        rt_parsed_module_storage =
            ::uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(begin,
                                                              end,
                                                              rt_parse_err,
                                                              ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t{});

        // Resource guards for fuzzing: a valid wasm can still request enormous initial table/memory sizes.
        // Building the runtime record would then attempt huge allocations and OOM the fuzzer process.
        {
            auto const& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Feature>>(rt_parsed_module_storage.sections)};
            auto const& memorysec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Feature>>(rt_parsed_module_storage.sections)};

            constexpr ::std::size_t max_table_min_elems{65536uz};
            constexpr ::std::size_t max_memory_min_pages{256uz};  // 256 * 64KiB = 16MiB

            for(auto const& table_type: tablesec.tables)
            {
                if(static_cast<::std::size_t>(table_type.limits.min) > max_table_min_elems) { return 0; }
            }
            for(auto const& memory_type: memorysec.memories)
            {
                if(static_cast<::std::size_t>(memory_type.limits.min) > max_memory_min_pages) { return 0; }
            }
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
        ::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage = ::std::move(rt_parsed_module_storage);

        if(::uwvm2::uwvm::wasm::loader::construct_all_module_and_check_duplicate_module() != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok)
        {
            return 0;
        }

        if(::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles() != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok) { return 0; }

        // `initialize_runtime()` applies active element/data segments and treats out-of-bounds initialization
        // as a fatal error (process termination). This fuzzer only compares *code* validation results, so
        // build the per-module runtime record for the compiler path and skip full runtime initialization.
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.clear();
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.reserve(1uz);
        ::uwvm2::uwvm::runtime::initializer::details::import_alias_sanity_checked = false;

        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t rt{};
        ::uwvm2::uwvm::runtime::initializer::details::current_initializing_module_name = u8"fuzz";
        ::uwvm2::uwvm::runtime::initializer::details::initialize_from_wasm_file(::uwvm2::uwvm::wasm::storage::execute_wasm, rt);
        ::uwvm2::uwvm::runtime::initializer::details::current_initializing_module_name = {};
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.try_emplace(u8"fuzz", ::std::move(rt));

        auto it{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.find(u8"fuzz")};
        if(it == ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.end()) { return 0; }

        ::uwvm2::validation::error::code_validation_error_impl compiler_err{};
        ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option op{};
        (void)::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_all_from_uwvm_single_func<
            ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t{}>(it->second, op, compiler_err);

        if(std_code != compiler_err.err_code) [[unlikely]] { fuzz_trap(); }

        return 0;
    }
    catch(...)
    {
        return 0;
    }
}

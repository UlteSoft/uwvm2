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
# include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/wasm1.h>
# include <uwvm2/uwvm/cmdline/callback/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/runtime/initializer/init.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/loader/load_and_check_modules.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#else
# error "Module testing is not currently supported"
#endif

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    try
    {
        // Keep the input size bounded even when running the target directly.
        if(size > (1u << 20u)) { return 0; }
        if(data == nullptr) { return 0; }

        auto const* begin = reinterpret_cast<::std::byte const*>(data);
        auto const* end = begin + size;

        // Parse (must succeed) -> runtime init -> compiler path.
        ::uwvm2::parser::wasm::base::error_impl err{};
        ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t module_storage{};
        module_storage =
            ::uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(begin, end, err, ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t{});

        ::uwvm2::uwvm::io::show_verbose = false;
        ::uwvm2::uwvm::io::show_depend_warning = false;

        // Reset global storages between fuzz iterations.
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

        // Install the parsed module as the exec wasm module, then run the same pre-checks as uwvm::run.
        ::uwvm2::uwvm::wasm::storage::execute_wasm = ::uwvm2::uwvm::wasm::type::wasm_file_t{1u};
        ::uwvm2::uwvm::wasm::storage::execute_wasm.file_name = u8"fuzz.wasm";
        ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"fuzz";
        ::uwvm2::uwvm::wasm::storage::execute_wasm.binfmt_ver = 1u;
        ::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage = ::std::move(module_storage);

        if(::uwvm2::uwvm::wasm::loader::construct_all_module_and_check_duplicate_module() != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok)
        {
            return 0;
        }

        if(::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles() != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok) { return 0; }

        // `initialize_runtime()` applies active element/data segments and treats out-of-bounds initialization
        // as a fatal error (process termination). For this compiler fuzzer we only need the per-module runtime
        // record to drive the compiler pipeline, so build it directly and skip full runtime initialization.
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.clear();
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.reserve(1uz);

        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t rt{};
        ::uwvm2::uwvm::runtime::initializer::details::current_initializing_module_name = u8"fuzz";
        ::uwvm2::uwvm::runtime::initializer::details::initialize_from_wasm_file(::uwvm2::uwvm::wasm::storage::execute_wasm, rt);
        ::uwvm2::uwvm::runtime::initializer::details::current_initializing_module_name = {};
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.try_emplace(u8"fuzz", ::std::move(rt));

        auto it{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.find(u8"fuzz")};
        if(it == ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.end()) { return 0; }

        ::uwvm2::validation::error::code_validation_error_impl compiler_err{};
        (void)::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_all_from_uwvm_single_func<
            ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::uwvm_interpreter_translate_option_t{}>(it->second, compiler_err);

        return 0;
    }
    catch(...)
    {
        return 0;
    }
}

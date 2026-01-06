/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/initializer/init.h>
# include <uwvm2/uwvm/cmdline/callback/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    inline void disable_noisy_logs_once() noexcept
    {
        static bool done{};
        if(done) { return; }
        done = true;

        ::uwvm2::uwvm::io::show_verbose = false;
        ::uwvm2::uwvm::io::show_vm_warning = false;
        ::uwvm2::uwvm::io::show_parser_warning = false;
        ::uwvm2::uwvm::io::show_untrusted_dl_warning = false;
        ::uwvm2::uwvm::io::show_depend_warning = false;

#if defined(UWVM_SUPPORT_PRELOAD_DL)
        ::uwvm2::uwvm::io::show_dl_warning = false;
#endif

#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        ::uwvm2::uwvm::io::show_weak_symbol_warning = false;
#endif

#if defined(_WIN32) && !defined(_WIN32_WINDOWS)
        ::uwvm2::uwvm::io::show_nt_path_warning = false;
#endif

#if defined(_WIN32) && defined(_WIN32_WINDOWS)
        ::uwvm2::uwvm::io::show_toctou_warning = false;
#endif
    }

    template <typename ModuleStorage>
    inline constexpr auto const& get_import_section(ModuleStorage const& module_storage) noexcept
    {
        return [&module_storage]<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(
                   ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept -> decltype(auto)
        {
            return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(module_storage.sections);
        }(::uwvm2::uwvm::wasm::feature::all_features);
    }

    template <typename ModuleStorage>
    inline constexpr bool module_requests_too_many_resources(ModuleStorage const& module_storage) noexcept
    {
        // Keep fuzzing stable: avoid huge allocations in initializer (e.g., memory min/max pages).
        constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 kMaxMemoryPages{16u};
        constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 kMaxTableElems{1024u};

        auto const& memsec = [&module_storage]<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(
                                 ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept -> decltype(auto)
        {
            return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>(module_storage.sections);
        }(::uwvm2::uwvm::wasm::feature::all_features);

        for(auto const& mem : memsec.memories)
        {
            if(mem.limits.min > kMaxMemoryPages) { return true; }
            if(mem.limits.present_max && mem.limits.max > kMaxMemoryPages) { return true; }
        }

        auto const& tablesec = [&module_storage]<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(
                                   ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept -> decltype(auto)
        {
            return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage.sections);
        }(::uwvm2::uwvm::wasm::feature::all_features);

        for(auto const& tab : tablesec.tables)
        {
            if(tab.limits.min > kMaxTableElems) { return true; }
            if(tab.limits.present_max && tab.limits.max > kMaxTableElems) { return true; }
        }

        return false;
    }
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    disable_noisy_logs_once();

    if(data == nullptr || size == 0) { return 0; }

    // Copy input to stable storage: the parser stores pointers/spans into the module bytes.
    ::std::vector<::std::byte> module_bytes(size);
    ::std::memcpy(module_bytes.data(), data, size);

    auto const* begin = module_bytes.data();
    auto const* end = begin + module_bytes.size();

    ::uwvm2::parser::wasm::base::error_impl err{};

    ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t fs_para{};
    ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t parsed{};
    try
    {
        parsed = ::uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(begin, end, err, fs_para);
    }
    catch(...)
    {
        // Expected on invalid inputs; libFuzzer will keep mutating.
        return 0;
    }

    // Avoid expected fatal paths in initializer: unresolved imports are treated as fatal.
    if(!get_import_section(parsed).imports.empty()) { return 0; }

    if(module_requests_too_many_resources(parsed)) { return 0; }

    // Clean global state for this fuzz iteration.
    ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.clear();
    ::uwvm2::uwvm::wasm::storage::all_module_export.clear();
    ::uwvm2::uwvm::wasm::storage::all_module.clear();

    // Build a minimal module record that matches initializer expectations (parser output -> all_module -> init).
    static constexpr char8_t kModuleName[]{u8"fuzz"};
    static constexpr char8_t kFileName[]{u8"fuzz.wasm"};

    ::uwvm2::uwvm::wasm::type::wasm_file_t wf{};
    wf.change_binfmt_ver(1u);
    wf.file_name = kFileName;
    wf.module_name = kModuleName;
    wf.wasm_module_storage.wasm_binfmt_ver1_storage = ::std::move(parsed);

    (void)::uwvm2::uwvm::wasm::storage::all_module.try_emplace(
        wf.module_name,
        ::uwvm2::uwvm::wasm::type::all_module_t{.module_storage_ptr = {.wf = ::std::addressof(wf)}, .type = ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm});

    ::uwvm2::uwvm::runtime::initializer::initialize_runtime();

    // Drop runtime state to avoid carrying pointers across fuzz iterations.
    ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.clear();
    ::uwvm2::uwvm::wasm::storage::all_module_export.clear();
    ::uwvm2::uwvm::wasm::storage::all_module.clear();

    return 0;
}


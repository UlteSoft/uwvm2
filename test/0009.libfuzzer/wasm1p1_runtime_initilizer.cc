/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
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
# include <uwvm2/parser/wasm/standard/wasm1p1/features/impl.h>
# include <uwvm2/uwvm/cmdline/callback/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/runtime/initializer/init.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/loader/load_and_check_modules.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using fs_para_t = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t;

    inline constexpr void configure_wasm1p1_features(fs_para_t& fs_para, ::std::uint8_t mode) noexcept
    {
        auto& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};

        if((mode & 0x40u) != 0u)
        {
            para.disable_multi_value = false;
            para.disable_reference_types = false;
            para.disable_bulk_memory = false;
            para.disable_sign_extension = false;
            para.disable_nontrapping_float_to_int = false;
            para.disable_simd = false;
            para.controllable_allow_multi_result_vector = false;
            para.controllable_allow_multi_table = false;
            return;
        }

        para.disable_multi_value = (mode & 0x01u) == 0u;
        para.disable_reference_types = (mode & 0x02u) == 0u;
        para.disable_bulk_memory = (mode & 0x04u) == 0u;
        para.disable_sign_extension = (mode & 0x08u) == 0u;
        para.disable_nontrapping_float_to_int = (mode & 0x10u) == 0u;
        para.disable_simd = (mode & 0x20u) == 0u;

        para.controllable_allow_multi_result_vector = para.disable_multi_value;
        para.controllable_allow_multi_table = para.disable_reference_types;
    }

    [[nodiscard]] inline constexpr bool check_resource_limits(auto const& module_storage) noexcept
    {
        constexpr auto check_impl{
            []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& module_storage_in,
                                                                    ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept
            {
                auto const& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage_in.sections)};
                auto const& memorysec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>(module_storage_in.sections)};

                constexpr ::std::size_t max_table_min_elems{65536uz};
                constexpr ::std::size_t max_memory_min_pages{256uz};

                for(auto const& table_type: tablesec.tables)
                {
                    if(static_cast<::std::size_t>(table_type.limits.min) > max_table_min_elems) { return false; }
                }
                for(auto const& memory_type: memorysec.memories)
                {
                    if(static_cast<::std::size_t>(memory_type.limits.min) > max_memory_min_pages) { return false; }
                }

                return true;
            }};

        return check_impl(module_storage, ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
    }

    inline void reset_global_storages()
    {
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
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.clear();
    }
}

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    try
    {
        if(data == nullptr) { return 0; }
        if(size <= 1uz || size > (1u << 20u)) { return 0; }

        auto const mode{data[0]};
        auto const* begin = reinterpret_cast<::std::byte const*>(data + 1uz);
        auto const* end = reinterpret_cast<::std::byte const*>(data + size);

        fs_para_t fs_para{};
        configure_wasm1p1_features(fs_para, mode);

        ::uwvm2::parser::wasm::base::error_impl err{};
        ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t module_storage{};
        try
        {
            module_storage = ::uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(begin, end, err, fs_para);
        }
        catch(::fast_io::error const&)
        {
            return 0;
        }

        if(err.err_code != ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok) { return 0; }
        if(!check_resource_limits(module_storage)) { return 0; }

        ::uwvm2::uwvm::io::show_verbose = false;
        ::uwvm2::uwvm::io::show_depend_warning = false;

        reset_global_storages();

        ::uwvm2::uwvm::wasm::storage::execute_wasm = ::uwvm2::uwvm::wasm::type::wasm_file_t{1u};
        ::uwvm2::uwvm::wasm::storage::execute_wasm.file_name = u8"fuzz.wasm";
        ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"fuzz";
        ::uwvm2::uwvm::wasm::storage::execute_wasm.binfmt_ver = 1u;
        ::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_parameter.binfmt1_para = fs_para;
        ::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage = ::std::move(module_storage);

        if(::uwvm2::uwvm::wasm::loader::construct_all_module_and_check_duplicate_module() != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok)
        {
            return 0;
        }

        if(::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles() != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok) { return 0; }

        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.reserve(1uz);

        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t rt{};
        ::uwvm2::uwvm::runtime::initializer::details::current_initializing_module_name = u8"fuzz";
        ::uwvm2::uwvm::runtime::initializer::details::initialize_from_wasm_file(::uwvm2::uwvm::wasm::storage::execute_wasm, rt);
        ::uwvm2::uwvm::runtime::initializer::details::current_initializing_module_name = {};
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.try_emplace(u8"fuzz", ::std::move(rt));

        return 0;
    }
    catch(...)
    {
        ::uwvm2::uwvm::runtime::initializer::details::current_initializing_module_name = {};
        return 0;
    }
}

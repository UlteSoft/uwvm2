/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-27
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <climits>
# include <type_traits>
# include <memory>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/madvise/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/parser/wasm/binfmt/base/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/depend/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/utils/memory/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/custom/impl.h>
# include <uwvm2/uwvm/wasm/warning/impl.h>
# include "wasm_file.h"
# include "dl.h"
# include "detect_cycle.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::loader
{
    enum class load_and_check_modules_rtl
    {
        ok,
        module_dependency_error,
        duplicate_module_name
    };

    inline load_and_check_modules_rtl contruct_all_module_and_check_duplicate_module() noexcept
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"contruct all module and check duplicate module"};
#endif

        // preloaded wasm
        for(auto const& lwc: ::uwvm2::uwvm::wasm::storage::preloaded_wasm)
        {
            if(!::uwvm2::uwvm::wasm::storage::all_module
                    .try_emplace(lwc.module_name,
                                 ::uwvm2::uwvm::wasm::type::all_module_t{.module_storage_ptr = {.wf = ::std::addressof(lwc)},
                                                                         .type = ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm})
                    .second) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Duplicate WASM module names: \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    lwc.module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\".\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                return load_and_check_modules_rtl::duplicate_module_name;
            }
        }

#if defined(UWVM_SUPPORT_PRELOAD_DL)

        // preloaded dl
        for(auto const& ldc: ::uwvm2::uwvm::wasm::storage::preloaded_dl)
        {
            if(!::uwvm2::uwvm::wasm::storage::all_module
                    .try_emplace(ldc.module_name,
                                 ::uwvm2::uwvm::wasm::type::all_module_t{.module_storage_ptr = {.wd = ::std::addressof(ldc)},
                                                                         .type = ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_dl})
                    .second) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Duplicate WASM module names: \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    ldc.module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\".\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                return load_and_check_modules_rtl::duplicate_module_name;
            }
        }
#endif

#if defined(UWVM_SUPPORT_WEEK_SYMBOL)
        // week symbol
        for(auto const& lwws: ::uwvm2::uwvm::wasm::storage::week_symbol)
        {
            if(!::uwvm2::uwvm::wasm::storage::all_module
                    .try_emplace(lwws.module_name,
                                 ::uwvm2::uwvm::wasm::type::all_module_t{.module_storage_ptr = {.wws = ::std::addressof(lwws)},
                                                                         .type = ::uwvm2::uwvm::wasm::type::module_type_t::week_symbol})
                    .second) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Duplicate WASM module names: \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    lwws.module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\".\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                return load_and_check_modules_rtl::duplicate_module_name;
            }
        }
#endif

        /// @todo local modules

        // exec wasm
        {
            if(!::uwvm2::uwvm::wasm::storage::all_module
                    .try_emplace(
                        ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name,
                        ::uwvm2::uwvm::wasm::type::all_module_t{.module_storage_ptr = {.wf = ::std::addressof(::uwvm2::uwvm::wasm::storage::execute_wasm)},
                                                                .type = ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm})
                    .second) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Duplicate WASM module names: \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\".\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                return load_and_check_modules_rtl::duplicate_module_name;
            }
        }

        // All modules loaded
        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"All modules have been loaded. ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(verbose)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
        }

        return load_and_check_modules_rtl::ok;
    }

    // this struct is utilized for temporary variables on the stack and doesnt store the global variables
    struct build_dependency_graph_and_check_import_exist_ret_t
    {
        using module_name_t = ::uwvm2::utils::container::u8string_view;
        using import_export_name_t = ::uwvm2::utils::container::u8string_view;
        using adjacency_list_t = ::uwvm2::utils::container::unordered_flat_map<module_name_t, ::uwvm2::utils::container::vector<module_name_t>>;

        load_and_check_modules_rtl ok;
        adjacency_list_t adj;
    };

    /// @brief      Build module dependency graph
    /// @details    In the WASM standard, importing module A name B type C and importing module A name B type D simultaneously satisfies syntactic validity
    ///             (binary format validity) but fails validation.
    /// @return     Adjacency list representation of the dependency graph
    inline constexpr build_dependency_graph_and_check_import_exist_ret_t
        build_dependency_graph_and_check_import_exist_and_construct_all_module_export() noexcept
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer build_dependency_graph_and_check_import_exist_timer{u8"build dependency graph and check import exist"};
#endif

        using module_name_t = ::uwvm2::utils::container::u8string_view;
        using adjacency_list_t = ::uwvm2::utils::container::unordered_flat_map<module_name_t, ::uwvm2::utils::container::vector<module_name_t>>;

        auto const all_module_size{::uwvm2::uwvm::wasm::storage::all_module.size()};

        adjacency_list_t adjacency_list{};
        adjacency_list.reserve(all_module_size);  // Reserve space for all modules

        // Initialize all module nodes
        for(auto const& module: ::uwvm2::uwvm::wasm::storage::all_module)
        {
            adjacency_list.emplace(module.first, ::uwvm2::utils::container::vector<module_name_t>{});
        }

        // Used to record the contents exported by each module. Persist globally.
        ::uwvm2::uwvm::wasm::storage::all_module_export.reserve(all_module_size);  // Reserve space for all modules to avoid reallocations

        // Build dependency relationships
        for(auto const& curr_module: ::uwvm2::uwvm::wasm::storage::all_module)
        {
            auto const curr_module_name{curr_module.first};

            switch(curr_module.second.type)
            {
                case ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm: [[fallthrough]];
                case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm:
                {
                    auto const exec_wasm_ptr{curr_module.second.module_storage_ptr.wf};
                    auto const exec_wasm_binfmt_ver{exec_wasm_ptr->binfmt_ver};

                    switch(exec_wasm_binfmt_ver)
                    {
                        case 1u:
                        {
                            auto const& exec_wasm_module_storage{exec_wasm_ptr->wasm_module_storage.wasm_binfmt_ver1_storage};
                            auto get_exec_wasm_module_storage_importsec_from_feature_tuple{
                                [&exec_wasm_module_storage]<::uwvm2::parser::wasm::concepts::wasm_feature... Fs> UWVM_ALWAYS_INLINE(
                                    ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept
                                {
                                    return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                        ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(exec_wasm_module_storage.sections);
                                }};
                            auto const& exec_wasm_module_storage_importsec{
                                get_exec_wasm_module_storage_importsec_from_feature_tuple(::uwvm2::uwvm::wasm::feature::all_features)};

                            for(auto const& imports: exec_wasm_module_storage_importsec.imports)
                            {
                                auto const import_module_name{imports.module_name};
                                auto const import_extern_name{imports.extern_name};

                                // Add dependency edge
                                if(adjacency_list.contains(import_module_name))
                                {
                                    auto& deps{adjacency_list[curr_module_name]};
                                    if(deps.empty())
                                    {
                                        deps.reserve(exec_wasm_module_storage_importsec.imports.size());  // Reserve space for all imports
                                    }
                                    // Safe: we just reserved space for all imports
                                    deps.push_back_unchecked(import_module_name);
                                }

                                // Check dependencies
                                auto const import_module{::uwvm2::uwvm::wasm::storage::all_module.find(import_module_name)};
                                if(import_module == ::uwvm2::uwvm::wasm::storage::all_module.end()) [[unlikely]]
                                {
                                    // Output missing module dependency error
                                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                        u8"uwvm: ",
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                                        u8"[error] ",
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                        u8"Missing module dependency: \"",
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                                        import_module_name,
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                        u8"\" required by \"",
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                                        curr_module_name,
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                        u8"\".\n\n",
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                                    return {load_and_check_modules_rtl::module_dependency_error, ::std::move(adjacency_list)};
                                }

                                // export -> import
                                auto const& imported_module{*import_module};

                                using exported_iterator = decltype(::uwvm2::uwvm::wasm::storage::all_module_export)::iterator;

                                exported_iterator curr_exported;  // No init

                                switch(imported_module.second.type)
                                {
                                    case ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm: [[fallthrough]];
                                    case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm:
                                    {
                                        auto const imported_wasm_ptr{imported_module.second.module_storage_ptr.wf};
                                        auto const imported_wasm_binfmt_ver{imported_wasm_ptr->binfmt_ver};

                                        switch(imported_wasm_binfmt_ver)
                                        {
                                            case 1u:
                                            {
                                                auto const& imported_wasm_module_storage{imported_wasm_ptr->wasm_module_storage.wasm_binfmt_ver1_storage};
                                                auto get_imported_wasm_module_storage_exportsec_from_feature_tuple{
                                                    [&imported_wasm_module_storage]<::uwvm2::parser::wasm::concepts::wasm_feature... Fs> UWVM_ALWAYS_INLINE(
                                                        ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept
                                                    {
                                                        return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                                            ::uwvm2::parser::wasm::standard::wasm1::features::export_section_storage_t<Fs...>>(
                                                            imported_wasm_module_storage.sections);
                                                    }};
                                                auto const& imported_wasm_module_storage_exportsec{
                                                    get_imported_wasm_module_storage_exportsec_from_feature_tuple(::uwvm2::uwvm::wasm::feature::all_features)};

                                                // Check if there is an exported map. If not, build one.
                                                auto [curr_exported_module,
                                                      inserted]{::uwvm2::uwvm::wasm::storage::all_module_export.try_emplace(import_module_name)};
                                                if(inserted) [[unlikely]]
                                                {
                                                    curr_exported_module->second.reserve(
                                                        imported_wasm_module_storage_exportsec.exports.size());  // Reserve space for exports
                                                    for(auto const& exports: imported_wasm_module_storage_exportsec.exports)
                                                    {
                                                        ::uwvm2::uwvm::wasm::type::all_module_export_t export_record{};
                                                        export_record.type = imported_module.second.type;
                                                        auto& file_export{export_record.storage.wasm_file_export_storage_ptr};
                                                        file_export.binfmt_ver = imported_wasm_binfmt_ver;
                                                        file_export.storage.wasm_binfmt_ver1_export_storage_ptr = ::std::addressof(exports.exports);
                                                        static_assert(::std::is_trivially_copy_constructible_v<decltype(export_record)>);
                                                        // No duplication, because a check was performed during loading.
                                                        curr_exported_module->second.emplace(exports.export_name, export_record);
                                                    }
                                                }

                                                curr_exported = curr_exported_module;

                                                break;
                                            }
                                            [[unlikely]] default:
                                            {
                                                static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u,
                                                              "missing implementation of other binfmt version");
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                                ::std::unreachable();
                                            }
                                        }

                                        break;
                                    }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                    case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_dl:
                                    {
                                        auto const imported_dl_ptr{imported_module.second.module_storage_ptr.wd};

                                        // Check if there is an exported map. If not, build one.
                                        auto [curr_exported_module, inserted]{::uwvm2::uwvm::wasm::storage::all_module_export.try_emplace(import_module_name)};
                                        if(inserted) [[unlikely]]
                                        {
                                            auto const dl_func{imported_dl_ptr->wasm_dl_storage.capi_function_vec};
                                            auto const dl_func_ptr{dl_func.function_begin};
                                            auto const dl_func_sz{dl_func.function_size};
                                            curr_exported_module->second.reserve(dl_func_sz);  // Reserve space for dl functions
                                            for(auto dl_func_curr{dl_func_ptr}; dl_func_curr != dl_func_ptr + dl_func_sz; ++dl_func_curr)
                                            {
                                                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                auto const dl_func_curr_name{
                                                    ::uwvm2::utils::container::u8string_view{
                                                                                             reinterpret_cast<char8_t_const_may_alias_ptr>(dl_func_curr->func_name_ptr),
                                                                                             dl_func_curr->func_name_length}
                                                };
                                                ::uwvm2::uwvm::wasm::type::all_module_export_t export_record{};
                                                export_record.type = ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_dl;
                                                export_record.storage.wasm_dl_export_storage_ptr.storage = dl_func_curr;
                                                static_assert(::std::is_trivially_copy_constructible_v<decltype(export_record)>);
                                                // No duplication, because a check was performed during loading.
                                                curr_exported_module->second.emplace(dl_func_curr_name, export_record);
                                            }
                                        }

                                        curr_exported = curr_exported_module;

                                        break;
                                    }
#endif

#if defined(UWVM_SUPPORT_WEEK_SYMBOL)
                                    case ::uwvm2::uwvm::wasm::type::module_type_t::week_symbol:
                                    {
                                        auto const week_symbol_ptr{imported_module.second.module_storage_ptr.wws};

                                        // Check if there is an exported map. If not, build one.
                                        auto [curr_exported_module, inserted]{::uwvm2::uwvm::wasm::storage::all_module_export.try_emplace(import_module_name)};
                                        if(inserted) [[unlikely]]
                                        {
                                            auto const wws_func{week_symbol_ptr->wasm_wws_storage.capi_function_vec};
                                            auto const wws_func_ptr{wws_func.function_begin};
                                            auto const wws_func_sz{wws_func.function_size};
                                            curr_exported_module->second.reserve(wws_func_sz);  // Reserve space for dl functions
                                            for(auto wws_func_curr{wws_func_ptr}; wws_func_curr != wws_func_ptr + wws_func_sz; ++wws_func_curr)
                                            {
                                                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                auto const wws_func_curr_name{
                                                    ::uwvm2::utils::container::u8string_view{
                                                                                             reinterpret_cast<char8_t_const_may_alias_ptr>(wws_func_curr->func_name_ptr),
                                                                                             wws_func_curr->func_name_length}
                                                };
                                                ::uwvm2::uwvm::wasm::type::all_module_export_t export_record{};
                                                export_record.type = ::uwvm2::uwvm::wasm::type::module_type_t::week_symbol;
                                                export_record.storage.wasm_week_symbol_export_storage_ptr.storage = wws_func_curr;
                                                static_assert(::std::is_trivially_copy_constructible_v<decltype(export_record)>);
                                                // No duplication, because a check was performed during loading.
                                                curr_exported_module->second.emplace(wws_func_curr_name, export_record);
                                            }
                                        }

                                        curr_exported = curr_exported_module;

                                        break;
                                    }
#endif

                                        /// @todo local_import

                                    [[unlikely]] default:
                                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                        ::std::unreachable();
                                    }
                                }

// curr_exported should never be end() here
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                if(curr_exported == ::uwvm2::uwvm::wasm::storage::all_module_export.end()) [[unlikely]]
                                {
                                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
                                }
#endif

                                auto const curr_exported_module_exported{curr_exported->second.find(import_extern_name)};
                                if(curr_exported_module_exported == curr_exported->second.end()) [[unlikely]]
                                {
                                    // Output missing export error in module a, dependency from module b
                                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                        u8"uwvm: ",
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                                        u8"[error] ",
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                        u8"Missing export \"",
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                                        import_extern_name,
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                        u8"\" in module \"",
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                                        import_module_name,
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                        u8"\" required by \"",
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                                        curr_module_name,
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                        u8"\".\n\n",
                                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                                    return {load_and_check_modules_rtl::module_dependency_error, ::std::move(adjacency_list)};
                                }

                                // Subsequent internal state checks are left to the initializer.
                            }

                            break;
                        }
                        [[unlikely]] default:
                        {
                            static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::std::unreachable();
                        }
                    }
                    break;
                }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_dl:
                {
                    // Since dl only allows importing functions, there is no possibility of cyclic dependencies or similar issues, so no check is needed
                    break;
                }
#endif
#if defined(UWVM_SUPPORT_WEEK_SYMBOL)
                case ::uwvm2::uwvm::wasm::type::module_type_t::week_symbol:
                {
                    // Since week_symbol only allows importing functions, there is no possibility of cyclic dependencies or similar issues, so no check is
                    // needed
                    break;
                }
#endif

                    /// @todo local_import

                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }
        }

        return {load_and_check_modules_rtl::ok, ::std::move(adjacency_list)};
    }

    inline load_and_check_modules_rtl check_import_exist_and_detect_cycles() noexcept
    {
        // Build dependency graph and detect cyclic dependencies
        // verbose
        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Start checking whether the import exists and the type matches. ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(verbose)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
        }

        auto const [ok, dependency_graph]{build_dependency_graph_and_check_import_exist_and_construct_all_module_export()};

        if(ok != load_and_check_modules_rtl::ok) [[unlikely]] { return ok; }

        // Detecting Cycle
        if(::uwvm2::uwvm::io::show_depend_warning)
        {
            // verbose
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Start Detecting Cycles. ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(verbose)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }

            auto const cycles{::uwvm2::uwvm::wasm::loader::detect_cycles(dependency_graph)};

            // Output cyclic dependency warnings
            if(!cycles.empty()) [[unlikely]]
            {
                {
                    // Here, as an entire output, the mutex needs to be controlled uniformly.
                    // There are no unspecified external calls that make the mutex deadlock.

                    // No copies will be made here.
                    auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                    // Add raii locks while unlocking operations
                    ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                        ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                    // No copies will be made here.
                    auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                    // output warning
                    ::fast_io::io::perr(u8log_output_ul,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        u8"[warn]  ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Detected ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                        cycles.size(),
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8" cyclic dependencies: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(depend)\n");

                    ::std::size_t counter{};

                    for(auto const& cycle: cycles)
                    {
                        ::fast_io::io::perr(u8log_output_ul,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"              ",
                                            ++counter,
                                            u8": ");

                        bool is_not_first{};
                        for(auto const cycle_curr: cycle)
                        {
                            if(is_not_first) [[likely]]
                            {
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                    u8" -> ");
                            }
                            is_not_first = true;

                            ::fast_io::io::perr(u8log_output_ul, ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN), cycle_curr);
                        }

                        ::fast_io::io::perrln(u8log_output_ul, ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    }

                    // Here, guard will perform destructors.
                }

                if(::uwvm2::uwvm::io::depend_warning_fatal) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Convert warnings to fatal errors. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(depend)\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }
        }

        return load_and_check_modules_rtl::ok;
    }
}  // namespace uwvm2::uwvm::run

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

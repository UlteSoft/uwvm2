/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-03-08
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
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include "preloaded_dl.h"
# include "weak_symbol.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::storage
{
    using configured_preload_module_attribute_map_t =
        ::uwvm2::utils::container::unordered_flat_map<::uwvm2::utils::container::u8string,
                                                      ::uwvm2::uwvm::wasm::type::preload_module_memory_attribute_t,
                                                      ::uwvm2::utils::container::pred::u8string_view_hash,
                                                      ::uwvm2::utils::container::pred::u8string_view_equal>;

    inline configured_preload_module_attribute_map_t configured_preload_module_attribute{};  // [global]

    [[nodiscard]] inline auto find_configured_preload_module_attribute(::uwvm2::utils::container::u8string_view module_name) noexcept
        -> ::uwvm2::uwvm::wasm::type::preload_module_memory_attribute_t*
    {
        if(auto it{configured_preload_module_attribute.find(module_name)}; it != configured_preload_module_attribute.end()) [[likely]]
        {
            return ::std::addressof(it->second);
        }
        return nullptr;
    }

    [[nodiscard]] inline auto find_configured_preload_module_attribute_const(::uwvm2::utils::container::u8string_view module_name) noexcept
        -> ::uwvm2::uwvm::wasm::type::preload_module_memory_attribute_t const*
    {
        if(auto const it{configured_preload_module_attribute.find(module_name)}; it != configured_preload_module_attribute.cend()) [[likely]]
        {
            return ::std::addressof(it->second);
        }
        return nullptr;
    }

    [[nodiscard]] inline ::uwvm2::uwvm::wasm::type::preload_module_memory_attribute_t& upsert_configured_preload_module_attribute(
        ::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        auto const [it, inserted]{configured_preload_module_attribute.try_emplace(::uwvm2::utils::container::u8string{module_name})};
        static_cast<void>(inserted);
        return it->second;
    }

    [[nodiscard]] inline ::uwvm2::uwvm::wasm::type::preload_module_memory_attribute_t resolve_preload_module_memory_attribute(
        ::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        if(auto const attribute{find_configured_preload_module_attribute_const(module_name)}; attribute != nullptr) [[likely]] { return *attribute; }
        return {};
    }

    enum class preload_capi_function_owner_kind_t : unsigned
    {
        preloaded_dl = 0u,
        weak_symbol
    };

    struct preload_capi_function_owner_t
    {
        preload_capi_function_owner_kind_t kind{preload_capi_function_owner_kind_t::preloaded_dl};
        ::std::size_t module_index{};
    };

    using preload_capi_function_owner_map_t =
        ::uwvm2::utils::container::unordered_flat_map<::uwvm2::uwvm::wasm::type::capi_function_t const*, preload_capi_function_owner_t>;

    inline preload_capi_function_owner_map_t preload_capi_function_owner{};  // [global]

    [[nodiscard]] inline preload_capi_function_owner_t const* find_preload_capi_function_owner(
        ::uwvm2::uwvm::wasm::type::capi_function_t const* function) noexcept
    {
        if(function == nullptr) [[unlikely]] { return nullptr; }

        auto const it{preload_capi_function_owner.find(function)};
        if(it == preload_capi_function_owner.cend()) [[unlikely]] { return nullptr; }
        return ::std::addressof(it->second);
    }

#if defined(UWVM_SUPPORT_PRELOAD_DL)
    inline void register_preloaded_dl_capi_functions(::std::size_t module_index) noexcept
    {
        if(module_index >= preloaded_dl.size()) [[unlikely]] { return; }

        auto const& module{preloaded_dl.index_unchecked(module_index)};
        auto const begin{module.wasm_dl_storage.capi_function_vec.function_begin};
        auto const size{module.wasm_dl_storage.capi_function_vec.function_size};
        if(begin == nullptr || size == 0uz) [[unlikely]] { return; }

        preload_capi_function_owner.reserve(preload_capi_function_owner.size() + size);

        for(::std::size_t i{}; i != size; ++i)
        {
            preload_capi_function_owner.insert_or_assign(
                begin + i,
                preload_capi_function_owner_t{.kind = preload_capi_function_owner_kind_t::preloaded_dl, .module_index = module_index});
        }
    }
#endif

#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
    inline void register_weak_symbol_capi_functions(::std::size_t module_index) noexcept
    {
        if(module_index >= weak_symbol.size()) [[unlikely]] { return; }

        auto const& module{weak_symbol.index_unchecked(module_index)};
        auto const begin{module.wasm_wws_storage.capi_function_vec.function_begin};
        auto const size{module.wasm_wws_storage.capi_function_vec.function_size};
        if(begin == nullptr || size == 0uz) [[unlikely]] { return; }

        preload_capi_function_owner.reserve(preload_capi_function_owner.size() + size);

        for(::std::size_t i{}; i != size; ++i)
        {
            preload_capi_function_owner.insert_or_assign(
                begin + i,
                preload_capi_function_owner_t{.kind = preload_capi_function_owner_kind_t::weak_symbol, .module_index = module_index});
        }
    }
#endif

    [[nodiscard]] inline auto find_loaded_preload_module_memory_attribute(::uwvm2::uwvm::wasm::type::capi_function_t const* function) noexcept
        -> ::uwvm2::uwvm::wasm::type::preload_module_memory_attribute_t const*
    {
        if(function == nullptr) [[unlikely]] { return nullptr; }

        auto const it{preload_capi_function_owner.find(function)};
        if(it == preload_capi_function_owner.cend()) [[unlikely]] { return nullptr; }

        switch(it->second.kind)
        {
#if defined(UWVM_SUPPORT_PRELOAD_DL)
            case preload_capi_function_owner_kind_t::preloaded_dl:
            {
                if(it->second.module_index >= preloaded_dl.size()) [[unlikely]] { return nullptr; }
                return ::std::addressof(preloaded_dl.index_unchecked(it->second.module_index).preload_module_memory_attribute);
            }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
            case preload_capi_function_owner_kind_t::weak_symbol:
            {
                if(it->second.module_index >= weak_symbol.size()) [[unlikely]] { return nullptr; }
                return ::std::addressof(weak_symbol.index_unchecked(it->second.module_index).preload_module_memory_attribute);
            }
#endif
            default:
            {
                return nullptr;
            }
        }
    }

    inline constexpr void apply_preload_module_memory_attribute_to_loaded_modules(
        ::uwvm2::utils::container::u8string_view module_name,
        ::uwvm2::uwvm::wasm::type::preload_module_memory_attribute_t const& attribute) noexcept
    {
#if defined(UWVM_SUPPORT_PRELOAD_DL)
        for(auto& curr_module: preloaded_dl)
        {
            if(curr_module.module_name == module_name) { curr_module.preload_module_memory_attribute = attribute; }
        }
#endif

#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        for(auto& curr_module: weak_symbol)
        {
            if(curr_module.module_name == module_name) { curr_module.preload_module_memory_attribute = attribute; }
        }
#endif
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif

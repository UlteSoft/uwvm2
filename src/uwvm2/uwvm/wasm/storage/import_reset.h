/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-04-28
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
# include <memory>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::storage
{
    struct configured_import_reset_t
    {
        ::uwvm2::utils::container::u8string import_module_name{};
        ::uwvm2::utils::container::u8string import_extern_name{};
        ::uwvm2::utils::container::u8string new_import_module_name{};
        ::uwvm2::utils::container::u8string new_import_extern_name{};
        ::std::size_t matched_count{};
    };

    using configured_import_reset_vec_t = ::uwvm2::utils::container::vector<configured_import_reset_t>;

    using configured_module_import_reset_map_t = ::uwvm2::utils::container::unordered_flat_map<::uwvm2::utils::container::u8string,
                                                                                               configured_import_reset_vec_t,
                                                                                               ::uwvm2::utils::container::pred::u8string_view_hash,
                                                                                               ::uwvm2::utils::container::pred::u8string_view_equal>;

    inline configured_module_import_reset_map_t configured_module_import_reset{};  // [global]

    [[nodiscard]] inline configured_import_reset_vec_t* find_configured_module_import_reset(
        ::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        if(auto it{configured_module_import_reset.find(module_name)}; it != configured_module_import_reset.end()) [[likely]]
        {
            return ::std::addressof(it->second);
        }
        return nullptr;
    }

    [[nodiscard]] inline configured_import_reset_vec_t const* find_configured_module_import_reset_const(
        ::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        if(auto const it{configured_module_import_reset.find(module_name)}; it != configured_module_import_reset.cend()) [[likely]]
        {
            return ::std::addressof(it->second);
        }
        return nullptr;
    }

    [[nodiscard]] inline configured_import_reset_vec_t& upsert_configured_module_import_reset(
        ::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        auto const [it, inserted]{configured_module_import_reset.try_emplace(::uwvm2::utils::container::u8string{module_name})};
        static_cast<void>(inserted);
        return it->second;
    }

    [[nodiscard]] inline configured_import_reset_t const* find_configured_import_reset_const(
        ::uwvm2::utils::container::u8string_view module_name,
        ::uwvm2::utils::container::u8string_view import_module_name,
        ::uwvm2::utils::container::u8string_view import_extern_name) noexcept
    {
        auto const rules{find_configured_module_import_reset_const(module_name)};
        if(rules == nullptr) [[likely]] { return nullptr; }

        for(auto const& rule: *rules)
        {
            if(rule.import_module_name == import_module_name && rule.import_extern_name == import_extern_name) [[unlikely]]
            {
                return ::std::addressof(rule);
            }
        }

        return nullptr;
    }

    [[nodiscard]] inline configured_import_reset_t& upsert_configured_import_reset(
        ::uwvm2::utils::container::u8string_view module_name,
        ::uwvm2::utils::container::u8string_view import_module_name,
        ::uwvm2::utils::container::u8string_view import_extern_name,
        ::uwvm2::utils::container::u8string_view new_import_module_name,
        ::uwvm2::utils::container::u8string_view new_import_extern_name) noexcept
    {
        auto& rules{upsert_configured_module_import_reset(module_name)};

        for(auto& rule: rules)
        {
            if(rule.import_module_name == import_module_name && rule.import_extern_name == import_extern_name) [[unlikely]]
            {
                rule.new_import_module_name = ::uwvm2::utils::container::u8string{new_import_module_name};
                rule.new_import_extern_name = ::uwvm2::utils::container::u8string{new_import_extern_name};
                rule.matched_count = 0uz;
                return rule;
            }
        }

        configured_import_reset_t rule{};
        rule.import_module_name = ::uwvm2::utils::container::u8string{import_module_name};
        rule.import_extern_name = ::uwvm2::utils::container::u8string{import_extern_name};
        rule.new_import_module_name = ::uwvm2::utils::container::u8string{new_import_module_name};
        rule.new_import_extern_name = ::uwvm2::utils::container::u8string{new_import_extern_name};
        rules.push_back(::std::move(rule));
        return rules.back_unchecked();
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-04-23
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
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::storage
{
    struct configured_module_memory_limit_t
    {
        ::uwvm2::utils::container::unordered_flat_map<::std::size_t, ::uwvm2::uwvm::wasm::type::module_memory_limit_t> local_defined_memory_limits{};
        ::uwvm2::uwvm::wasm::type::module_memory_limit_t all_limits{};
        bool apply_to_all_memories{};
    };

    using configured_module_memory_limit_map_t = ::uwvm2::utils::container::unordered_flat_map<::uwvm2::utils::container::u8string,
                                                                                               configured_module_memory_limit_t,
                                                                                               ::uwvm2::utils::container::pred::u8string_view_hash,
                                                                                               ::uwvm2::utils::container::pred::u8string_view_equal>;

    inline configured_module_memory_limit_map_t configured_module_memory_limit{};  // [global]

    [[nodiscard]] inline configured_module_memory_limit_t* find_configured_module_memory_limit(::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        if(auto it{configured_module_memory_limit.find(module_name)}; it != configured_module_memory_limit.end()) [[likely]]
        {
            return ::std::addressof(it->second);
        }
        return nullptr;
    }

    [[nodiscard]] inline configured_module_memory_limit_t const* find_configured_module_memory_limit_const(
        ::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        if(auto const it{configured_module_memory_limit.find(module_name)}; it != configured_module_memory_limit.cend()) [[likely]]
        {
            return ::std::addressof(it->second);
        }
        return nullptr;
    }

    [[nodiscard]] inline configured_module_memory_limit_t& upsert_configured_module_memory_limit(::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        auto const [it, inserted]{configured_module_memory_limit.try_emplace(::uwvm2::utils::container::u8string{module_name})};
        static_cast<void>(inserted);
        return it->second;
    }

    [[nodiscard]] inline ::uwvm2::uwvm::wasm::type::module_memory_limit_t const* find_configured_local_defined_memory_limit(
        configured_module_memory_limit_t const& config,
        ::std::size_t local_memory_index) noexcept
    {
        if(auto const it{config.local_defined_memory_limits.find(local_memory_index)}; it != config.local_defined_memory_limits.cend()) [[likely]]
        {
            return ::std::addressof(it->second);
        }

        if(config.apply_to_all_memories) [[likely]] { return ::std::addressof(config.all_limits); }

        return nullptr;
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
#endif

namespace uwvm2::runtime::lib::details
{
    template <typename Cache, typename Environment>
    [[nodiscard]] inline constexpr bool wasip1_default_context_fast_path(
        Cache const& cache, ::std::size_t module, Environment const* default_environment,
        bool configured_overrides) noexcept
    {
        if(configured_overrides) { return false; }
        // The actual cache owns the module selection. A privately restored
        // environment must not fall back to the original global environment
        // merely because the consumed initializer configuration is empty.
        return module >= cache.size() || cache.index_unchecked(module).env == default_environment;
    }

    template <typename DefaultEnvironment, typename GroupStorage>
    inline constexpr void clear_wasip1_memory_bindings(DefaultEnvironment& default_environment, GroupStorage& configured_groups) noexcept
    {
        default_environment.wasip1_memory = nullptr;
        for(auto& state: configured_groups) { state.env.wasip1_memory = nullptr; }
    }
}

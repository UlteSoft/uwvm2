/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

namespace uwvm2::runtime::lib::details
{
    template <typename DefaultEnvironment, typename GroupStorage>
    inline constexpr void clear_wasip1_memory_bindings(DefaultEnvironment& default_environment, GroupStorage& configured_groups) noexcept
    {
        default_environment.wasip1_memory = nullptr;
        for(auto& state: configured_groups) { state.env.wasip1_memory = nullptr; }
    }
}

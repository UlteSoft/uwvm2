/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       Runtime initializer limits
 * @details     Used to cap internal `reserve()` calls during instantiation to avoid excessive allocations.
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-01-19
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

#if !(__cpp_pack_indexing >= 202311L)
# error "UWVM requires at least C++26 standard compiler. See https://en.cppreference.com/w/cpp/feature_test#cpp_pack_indexing"
#endif

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <concepts>
# include <limits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::initializer
{
    template <::std::unsigned_integral From>
    inline constexpr ::std::size_t saturating_cast_size_t(From value) noexcept
    {
        if constexpr(::std::numeric_limits<From>::max() <= ::std::numeric_limits<::std::size_t>::max()) { return static_cast<::std::size_t>(value); }
        else
        {
            constexpr auto max_val{static_cast<From>(::std::numeric_limits<::std::size_t>::max())};
            return value > max_val ? ::std::numeric_limits<::std::size_t>::max() : static_cast<::std::size_t>(value);
        }
    }

    // Defaults are intentionally conservative for containers that can lead to large allocations.
    inline constexpr ::std::size_t default_max_runtime_modules{saturating_cast_size_t(65536u)};

    inline constexpr ::std::size_t default_max_imported_functions{saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_imported_tables{saturating_cast_size_t(1024u)};
    inline constexpr ::std::size_t default_max_imported_memories{saturating_cast_size_t(1024u)};
    inline constexpr ::std::size_t default_max_imported_globals{saturating_cast_size_t(262144u)};

    inline constexpr ::std::size_t default_max_local_defined_functions{saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_local_defined_codes{saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_local_defined_tables{saturating_cast_size_t(1024u)};
    inline constexpr ::std::size_t default_max_local_defined_memories{saturating_cast_size_t(1024u)};
    inline constexpr ::std::size_t default_max_local_defined_globals{saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_local_defined_elements{saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_local_defined_datas{saturating_cast_size_t(262144u)};

    struct initializer_limit_t
    {
        ::std::size_t max_runtime_modules{default_max_runtime_modules};

        ::std::size_t max_imported_functions{default_max_imported_functions};
        ::std::size_t max_imported_tables{default_max_imported_tables};
        ::std::size_t max_imported_memories{default_max_imported_memories};
        ::std::size_t max_imported_globals{default_max_imported_globals};

        ::std::size_t max_local_defined_functions{default_max_local_defined_functions};
        ::std::size_t max_local_defined_codes{default_max_local_defined_codes};
        ::std::size_t max_local_defined_tables{default_max_local_defined_tables};
        ::std::size_t max_local_defined_memories{default_max_local_defined_memories};
        ::std::size_t max_local_defined_globals{default_max_local_defined_globals};
        ::std::size_t max_local_defined_elements{default_max_local_defined_elements};
        ::std::size_t max_local_defined_datas{default_max_local_defined_datas};
    };

    inline initializer_limit_t initializer_limit{};
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif

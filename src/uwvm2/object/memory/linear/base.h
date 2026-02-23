/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
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
# include <version>
# include <limits>
# include <memory>
# include <new>
# include <atomic>
# include <bit>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/mutex/impl.h>
# include <uwvm2/object/memory/wasm_page/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::linear
{
    // Provide a dummy structure to facilitate a unified API.
    struct dummy_memory_operation_guard_t
    {
        inline constexpr dummy_memory_operation_guard_t() noexcept = default;

        inline constexpr dummy_memory_operation_guard_t(dummy_memory_operation_guard_t const& other) noexcept = delete;
        inline constexpr dummy_memory_operation_guard_t& operator= (dummy_memory_operation_guard_t const& other) noexcept = delete;

        // Provide mobile semantic convenience functions for return
        inline constexpr dummy_memory_operation_guard_t(dummy_memory_operation_guard_t&& other) noexcept = default;
        inline constexpr dummy_memory_operation_guard_t& operator= (dummy_memory_operation_guard_t&& other) noexcept = default;

        inline constexpr ~dummy_memory_operation_guard_t() noexcept = default;

        inline constexpr void enter_operation() noexcept {}

        inline constexpr void exit_operation() noexcept {}
    };

    // Defined in `allocator.h` when the standard library provides C++20 atomic wait/notify primitives.
    struct memory_operation_guard_t;
}  // namespace uwvm2::object::memory::linear

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif

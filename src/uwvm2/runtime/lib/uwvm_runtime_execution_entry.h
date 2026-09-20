/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <cstddef>
#include <limits>

namespace uwvm2::runtime::lib::details
{
    enum class runtime_execution_entry_reentry : bool
    {
        reject,
        allow_public_llvm_raw
    };

    enum class runtime_execution_entry_leave_result : unsigned char
    {
        invalid,
        nested,
        outermost
    };

    // Full/lazy host execution is deliberately non-reentrant. The public LLVM raw API is the sole supported
    // callback re-entry surface; its nested exit must leave the outer entry's per-thread state intact.
    [[nodiscard]] inline constexpr bool runtime_execution_entry_enter(
        ::std::size_t& depth,
        runtime_execution_entry_reentry reentry = runtime_execution_entry_reentry::reject) noexcept
    {
        if(depth == (::std::numeric_limits<::std::size_t>::max)()) { return false; }
        if(depth != 0u && reentry == runtime_execution_entry_reentry::reject) { return false; }
        ++depth;
        return true;
    }

    [[nodiscard]] inline constexpr runtime_execution_entry_leave_result runtime_execution_entry_leave(::std::size_t& depth) noexcept
    {
        if(depth == 0u) { return runtime_execution_entry_leave_result::invalid; }
        --depth;
        return depth == 0u ? runtime_execution_entry_leave_result::outermost : runtime_execution_entry_leave_result::nested;
    }

    [[nodiscard]] inline constexpr bool runtime_execution_entry_reset_allowed(::std::size_t depth) noexcept
    { return depth == 0u; }

    // Provider metadata can be queried while a lazy compilation unit is materializing without holding the global
    // publication lock. A public raw/reset re-entry from that callback could wait on the compilation unit that is
    // currently invoking it, so track this narrower phase independently from ordinary execution callbacks.
    [[nodiscard]] inline constexpr bool runtime_compilation_metadata_callback_access_allowed(::std::size_t depth) noexcept
    { return depth == 0u; }

    [[nodiscard]] inline constexpr bool runtime_compilation_metadata_callback_enter(::std::size_t& depth) noexcept
    {
        if(depth == (::std::numeric_limits<::std::size_t>::max)()) { return false; }
        ++depth;
        return true;
    }

    [[nodiscard]] inline constexpr bool runtime_compilation_metadata_callback_leave(::std::size_t& depth) noexcept
    {
        if(depth == 0u) { return false; }
        --depth;
        return true;
    }

    // A map-backed metadata scope may remove a node only when that exact scope created the node and its own leave
    // completed the outermost callback. Pre-existing execution/main-thread nodes remain owned by their original entry.
    [[nodiscard]] inline constexpr bool runtime_compilation_metadata_callback_owned_node_should_erase(
        bool owns_inserted_node,
        ::std::size_t remaining_depth) noexcept
    {
        return owns_inserted_node && remaining_depth == 0u;
    }

    // The publication lock is intentionally non-recursive. Track its current-thread ownership separately from
    // execution depth so provider callbacks cannot re-enter a public raw call or reset and spin forever on the
    // lock already owned by that same thread.
    [[nodiscard]] inline constexpr bool runtime_state_publication_access_allowed(::std::size_t depth) noexcept
    { return depth == 0u; }

    [[nodiscard]] inline constexpr bool runtime_state_publication_enter(::std::size_t& depth) noexcept
    {
        if(depth != 0u) { return false; }
        depth = 1u;
        return true;
    }

    [[nodiscard]] inline constexpr bool runtime_state_publication_leave(::std::size_t& depth) noexcept
    {
        if(depth != 1u) { return false; }
        depth = 0u;
        return true;
    }
}  // namespace uwvm2::runtime::lib::details

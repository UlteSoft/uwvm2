/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::llvm_jit_cache
{
    struct source_provenance_policy_inputs
    {
        bool has_git_commit{};
        bool has_verified_build_source_id{};
        bool git_worktree_is_dirty{};
        bool allow_unsafe_dirty_cache{};
        bool allow_unsafe_unprovenanced_cache{};
    };

    [[nodiscard]] inline constexpr bool source_provenance_allows_persistent_cache(source_provenance_policy_inputs const policy) noexcept
    {
        // A dirty worktree is rejected even when it also has a commit id: the commit does not identify the modified
        // host bridge/runtime sources. Its escape hatch is intentionally independent from missing-provenance handling.
        if(policy.git_worktree_is_dirty && !policy.allow_unsafe_dirty_cache) { return false; }

        // Native objects can embed calls and ABI assumptions outside Wasm-derived LLVM IR. A deterministic cache
        // signature authenticates bytes against a context; it cannot create source provenance for that context.
        if(!policy.has_git_commit && !policy.has_verified_build_source_id && !policy.allow_unsafe_unprovenanced_cache) { return false; }
        return true;
    }
}  // namespace uwvm2::runtime::llvm_jit_cache

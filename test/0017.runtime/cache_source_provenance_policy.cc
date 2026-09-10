#include <uwvm2/runtime/llvm_jit_cache/source_provenance_policy.h>

namespace
{
    using ::uwvm2::runtime::llvm_jit_cache::source_provenance_allows_persistent_cache;
    using ::uwvm2::runtime::llvm_jit_cache::source_provenance_policy_inputs;

    static_assert(source_provenance_allows_persistent_cache({.has_git_commit = true}));
    static_assert(source_provenance_allows_persistent_cache({.has_verified_build_source_id = true}));
    static_assert(!source_provenance_allows_persistent_cache({}));
    static_assert(!source_provenance_allows_persistent_cache({.has_git_commit = true, .git_worktree_is_dirty = true}));
    static_assert(source_provenance_allows_persistent_cache(
        {.has_git_commit = true, .git_worktree_is_dirty = true, .allow_unsafe_dirty_cache = true}));
    static_assert(source_provenance_allows_persistent_cache({.allow_unsafe_unprovenanced_cache = true}));
    static_assert(!source_provenance_allows_persistent_cache(
        {.git_worktree_is_dirty = true, .allow_unsafe_unprovenanced_cache = true}));
    static_assert(source_provenance_allows_persistent_cache(
        {.git_worktree_is_dirty = true, .allow_unsafe_dirty_cache = true, .allow_unsafe_unprovenanced_cache = true}));
}

int main() {}

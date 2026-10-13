// Cold dependency contract only; never stop, file, cache or restore authority.
#pragma once
#ifndef UWVM_MODULE
# include <type_traits>
# include <uwvm2/runtime/checkpoint/materialization.h>
# if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
#  include <uwvm2/runtime/llvm_jit_cache/format.h>
# endif
# include "checkpoint_binding.h"
# include "checkpoint_codec.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::checkpoint::binding
{
    // Prevent silently keeping a private binding reader on stale actual cache,
    // state-body or per-engine profile contracts after source changes. A format
    // migration must update its explicit wire version and compatibility policy.
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
    static_assert(cache_format_revision == ::uwvm2::runtime::llvm_jit_cache::cache_format_version);
#endif
    static_assert(state_version == 6u && state_version == ::uwvm2::uwvm::debugger::checkpoint::database_format_version);
    static_assert(::uwvm2::runtime::checkpoint::state_schema_revision == state_version);
    using binding_profile_identity_type = decltype(manifest{}.checkpoint_profile);
    static_assert(::std::is_same_v<binding_profile_identity_type,
        ::uwvm2::runtime::checkpoint::compilation_profile::cache_identity_type>);
}

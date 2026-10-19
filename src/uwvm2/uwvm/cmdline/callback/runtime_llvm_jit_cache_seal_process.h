// Pure argument callback: never changes the Host process policy.
#pragma once
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#if defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
# if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
# else
    UWVM_GNU_COLD inline constexpr
# endif
    ::uwvm2::utils::cmdline::parameter_return_type runtime_llvm_jit_cache_seal_process_callback(
        [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_begin,
        [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
        [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept
    {
        // A Host CLI request is not a Source/origin ticket or permission grant.
        ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_cache_seal_process_requested = true;
        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

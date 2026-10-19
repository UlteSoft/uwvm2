// Explicit Host process-policy request. SOURCE ONLY; qualification pending.
#pragma once
#ifndef UWVM_MODULE
# include <memory>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
#if defined(UWVM_RUNTIME_LLVM_JIT)
    namespace details
    {
        inline constexpr ::uwvm2::utils::container::u8string_view runtime_llvm_jit_cache_seal_process_alias{u8"-Rllvm-cache-seal-process"};
# if defined(UWVM_MODULE)
        extern "C++"
# else
        inline constexpr
# endif
        ::uwvm2::utils::cmdline::parameter_return_type runtime_llvm_jit_cache_seal_process_callback(
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
    }
# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wbraced-scalar-init"
# endif
    inline constexpr ::uwvm2::utils::cmdline::parameter runtime_llvm_jit_cache_seal_process{
        .name{u8"--runtime-llvm-jit-cache-seal-process"},
        .describe{u8"Explicit Linux-only process seal (PR_SET_DUMPABLE=0) before execution for authenticated LLVM disk cache eligibility. Disables normal core dumps and usual same-UID ptrace attachment; DBG/VTune compatibility requires separate qualification. Default process policy is unchanged; no automatic restoration. A seal does not guarantee a cache hit."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::runtime_llvm_jit_cache_seal_process_alias), 1uz}},
        .handle{::std::addressof(details::runtime_llvm_jit_cache_seal_process_callback)},
        .is_exist{::std::addressof(::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_cache_seal_process_existed)},
        .cate{::uwvm2::utils::cmdline::categorization::runtime}};
# if defined(__clang__)
#  pragma clang diagnostic pop
# endif
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

// Private SOURCE candidate: full exception dispatch request, not call-stack policy.
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
#if defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
    namespace details
    {
        inline constexpr ::uwvm2::utils::container::u8string_view runtime_llvm_jit_exception_dispatch_alias{u8"-Rllvm-exception-dispatch"};
# if defined(UWVM_MODULE)
        extern "C++"
# else
        inline constexpr
# endif
            ::uwvm2::utils::cmdline::parameter_return_type runtime_llvm_jit_exception_dispatch_callback(
                ::uwvm2::utils::cmdline::parameter_parsing_results*,
                ::uwvm2::utils::cmdline::parameter_parsing_results*,
                ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
    }
    
# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wbraced-scalar-init"
# endif
    inline constexpr ::uwvm2::utils::cmdline::parameter runtime_llvm_jit_exception_dispatch{
        .name{u8"--runtime-llvm-jit-exception-dispatch"},
        .describe{u8"Request full LLVM exception dispatch; ineligible pending modules retain native dispatch."},
        .usage{u8"[auto|native-unwind|pending-numeric]"},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::runtime_llvm_jit_exception_dispatch_alias), 1uz}},
        .handle{::std::addressof(details::runtime_llvm_jit_exception_dispatch_callback)},
        .is_exist{::std::addressof(::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_exception_dispatch_existed)},
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

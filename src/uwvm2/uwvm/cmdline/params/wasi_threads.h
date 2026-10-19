#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <memory>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/runtime/lib/uwvm_runtime_threaded_profile_api.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
#if defined(UWVM2_HAS_THREADED_PROFILE_HOST_API) && UWVM2_HAS_THREADED_PROFILE_HOST_API
    namespace details
    {
        inline bool wasi_threads_existed{};
        inline void wasi_threads_pretreatment(char8_t const* const*&,char8_t const* const*,
            ::uwvm2::utils::container::vector<::uwvm2::utils::cmdline::parameter_parsing_results>&) noexcept;
        inline ::uwvm2::utils::cmdline::parameter_return_type wasi_threads_callback(
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
    }
# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wbraced-scalar-init"
# endif
    inline constexpr ::uwvm2::utils::cmdline::parameter wasi_threads{
        .name{u8"--wasi-threads"},
        .describe{u8"Select finite WASI Threads quotas; the stack value bounds each Guest AND compiler mapping, including guards."},
        .usage{u8"<max-active> <max-spawns> <native-stack-bytes> <per-child-storage> <per-child-code> <shared-memory-bytes>"},
        .handle{::std::addressof(details::wasi_threads_callback)},
        .pretreatment{::std::addressof(details::wasi_threads_pretreatment)},
        .is_exist{::std::addressof(details::wasi_threads_existed)},
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

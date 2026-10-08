/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <chrono>
# include <concepts>
# include <fast_io.h>
# include <uwvm2/utils/macro/push_macros.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::utils::thread
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    struct physical_join_result
    {
        ::fast_io::thread_join_result actual{};
        bool timed_out{};
    };
    // Synchronous HOST owner borrow. The caller pins the exact native_thread
    // and every VM/controller/guest-code owner until actual.status == joined.
    // No callback or native handle escapes. Pending/error/unsupported never
    // detach, destroy, cancel the thread or manufacture a physical-retirement ACK.
    namespace details
    {
        template<typename NativeThread>
        [[nodiscard]] inline physical_join_result join_native_thread_until_impl(
            NativeThread& owner, ::std::chrono::steady_clock::time_point deadline) noexcept
        {
            if constexpr(requires(NativeThread& actual_owner)
                { { actual_owner.try_join() } noexcept -> ::std::same_as<::fast_io::thread_join_result>; })
            {
                for(;;)
                {
                    auto const actual{owner.try_join()};
                    if(actual.status != ::fast_io::thread_join_status::pending) { return {actual, false}; }
                    if(::std::chrono::steady_clock::now() >= deadline) { return {actual, true}; }
                    // Cold HOST manager only. FastIO yield is nonthrowing on
                    // each selected provider; no guest/cancellation callback.
                    ::fast_io::this_thread::yield();
                }
            }
            else
            {
                // Native-thread presence alone does not qualify bounded join,
                // for example the distinct WASI-thread provider. Retain owner.
                static_cast<void>(owner); static_cast<void>(deadline);
                return {{::fast_io::thread_join_status::unsupported, 0u}, false};
            }
        }
    }
    [[nodiscard]] inline physical_join_result join_native_thread_until(
        ::fast_io::native_thread& owner, ::std::chrono::steady_clock::time_point deadline) noexcept
    { return details::join_native_thread_until_impl(owner, deadline); }

#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif

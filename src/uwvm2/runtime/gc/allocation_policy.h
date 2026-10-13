/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <cstddef>
# include <cstdint>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
// Translator helpers use the complete capability definition before the runtime
// implementation is included. Private experiment only; no default include cost.
# include "managed_numeric_page.h"
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::gc
{
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    // Runtime validates actual entry/lease before returning a typed capability.
    // No guest address or public bool can mint one.
    using managed_page_borrow_function = ::uwvm2::uwvm::runtime::storage::managed_numeric_entry_page*(*)(::std::uintptr_t) noexcept;
    inline ::std::atomic<managed_page_borrow_function> managed_page_borrow_callback{};
    using managed_page_boundary_function = void(*)(bool) noexcept;
    inline ::std::atomic<managed_page_boundary_function> managed_page_boundary_callback{};
    inline void managed_page_boundary(bool revoke) noexcept
    {
        auto callback{managed_page_boundary_callback.load(::std::memory_order_acquire)};
        if(callback != nullptr) { callback(revoke); }
    }
    [[nodiscard]] inline auto borrow_actual_managed_page(::std::uintptr_t module) noexcept
    {
        auto callback{managed_page_borrow_callback.load(::std::memory_order_acquire)};
        return callback == nullptr ? nullptr : callback(module);
    }
#endif
    using managed_allocation_poll_function = void(*)(::std::uintptr_t) noexcept;
    // Host-only publication. The runtime installs a process-lifetime callback
    // before roots-enabled code materializes and clears it only after execution
    // drain. A native helper outside an admitted eligible guest still returns
    // without collection; no guest address can install or change this callback.
    inline ::std::atomic<managed_allocation_poll_function> managed_allocation_poll_callback{};
    inline void poll_before_managed_aggregate_allocation(::std::uintptr_t module_address) noexcept
    {
        auto const callback{managed_allocation_poll_callback.load(::std::memory_order_acquire)};
        if(callback != nullptr) { callback(module_address); }
    }
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE) && UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE == 1
    using managed_array_allocation_poll_function = void(*)(::std::uintptr_t, ::std::size_t) noexcept;
    inline ::std::atomic<managed_array_allocation_poll_function> managed_array_allocation_poll_callback{};
    inline void poll_before_managed_array_allocation(::std::uintptr_t module_address,
        ::std::size_t length) noexcept
    {
        // A scheduling hint only. It admits no module, reference, byte window,
        // allocation, or GC root. Small arrays retain the original count poll.
        if(length >= 256uz)
        {
            auto const callback{managed_array_allocation_poll_callback.load(::std::memory_order_acquire)};
            if(callback != nullptr) { callback(module_address,length); return; }
        }
        poll_before_managed_aggregate_allocation(module_address);
    }
#endif

}

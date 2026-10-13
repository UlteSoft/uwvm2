/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <cstdint>
# include <exception>
# include <limits>
# include "entry_admission.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::gc
{
    inline ::std::atomic_uint_least64_t initializer_serial_source{};
    inline ::std::atomic_uint_least64_t published_initializer_serial{};
    [[nodiscard]] inline ::std::uint_least64_t begin_owned_initializer() noexcept
    {
        published_initializer_serial.store(0u, ::std::memory_order_release);
        auto const previous{initializer_serial_source.fetch_add(1u, ::std::memory_order_acq_rel)};
        if(previous == (::std::numeric_limits<::std::uint_least64_t>::max)()) { ::std::terminate(); }
        return previous + 1u;
    }
    inline void publish_owned_initializer(::std::uint_least64_t serial) noexcept
    {
        if(serial == 0u || initializer_serial_source.load(::std::memory_order_acquire) != serial)
        { ::std::terminate(); }
        published_initializer_serial.store(serial, ::std::memory_order_release);
    }
    struct instance_collection_phase
    {
        alignas(::std::atomic_ref<::std::uint_least64_t>::required_alignment) ::std::uint_least64_t initializer_serial{};
        alignas(::std::atomic_ref<bool>::required_alignment) bool native_storage_owned{};
        alignas(::std::atomic_ref<bool>::required_alignment) bool initialized{};
        alignas(::std::atomic_ref<bool>::required_alignment) bool active_segments_ready{};
        // Actual initializer writes only after all globals/table expressions
        // are complete. Direct native/test-created modules remain unowned.
        void publish_initialized(::std::uint_least64_t serial) noexcept
        {
            ::std::atomic_ref<::std::uint_least64_t>{initializer_serial}.store(serial, ::std::memory_order_release);
            ::std::atomic_ref<bool>{native_storage_owned}.store(true, ::std::memory_order_release);
            ::std::atomic_ref<bool>{initialized}.store(true, ::std::memory_order_release);
        }
        void publish_active_segments(bool ready) noexcept
        { ::std::atomic_ref<bool>{active_segments_ready}.store(ready, ::std::memory_order_release); }
        [[nodiscard]] bool initialized_for(::std::uint_least64_t serial) const noexcept
        {
            // Native fields are initialized objects. Integer/guest addresses
            // never select this phase record; every caller proves module identity.
            return serial != 0u &&
                ::std::atomic_ref<::std::uint_least64_t>{const_cast<::std::uint_least64_t&>(initializer_serial)}.load(::std::memory_order_acquire) == serial &&
                ::std::atomic_ref<bool>{const_cast<bool&>(native_storage_owned)}.load(::std::memory_order_acquire) &&
                ::std::atomic_ref<bool>{const_cast<bool&>(initialized)}.load(::std::memory_order_acquire);
        }
        [[nodiscard]] bool ready_for(::std::uint_least64_t serial) const noexcept
        {
            return initialized_for(serial) &&
                ::std::atomic_ref<bool>{const_cast<bool&>(active_segments_ready)}.load(::std::memory_order_acquire);
        }
    };
}

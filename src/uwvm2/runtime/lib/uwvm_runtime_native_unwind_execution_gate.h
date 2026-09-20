/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <mutex>

namespace uwvm2::runtime::lib::details
{
    // Native-unwind diagnostics read compact mutable address maps. Serialize an unwind-backed execute/reset lifetime so a
    // reset cannot destroy those maps while a trap reader is walking them. Recursive locking preserves same-thread host re-entry.
    class native_unwind_execution_gate
    {
    public:
        class guard
        {
            native_unwind_execution_gate* owner{};

        public:
            inline explicit guard(native_unwind_execution_gate& gate, bool enabled) noexcept : owner(enabled ? &gate : nullptr)
            {
                if(owner != nullptr) { owner->mutex.lock(); }
            }

            guard(guard const&) = delete;
            guard& operator= (guard const&) = delete;
            guard(guard&&) = delete;
            guard& operator= (guard&&) = delete;

            inline ~guard()
            {
                if(owner != nullptr) { owner->mutex.unlock(); }
            }

            [[nodiscard]] inline bool owns_lock() const noexcept { return owner != nullptr; }
        };

        [[nodiscard]] inline guard enter_if(bool enabled) noexcept { return guard{*this, enabled}; }

    private:
        ::std::recursive_mutex mutex{};
    };
}

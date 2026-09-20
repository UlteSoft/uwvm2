/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <mutex>

namespace uwvm2::runtime::lib::details
{
    // Native-unwind metadata is intentionally a compact, mutable address table.
    // When execution-time LLVM materialization is enabled, serialize the whole
    // materialize/execute lifetime so trap readers never overlap a table writer.
    // A recursive mutex preserves same-thread host re-entry from imported code.
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

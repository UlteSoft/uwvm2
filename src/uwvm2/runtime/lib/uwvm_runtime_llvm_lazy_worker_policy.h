/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <cstddef>

namespace uwvm2::runtime::lib::details
{
    [[nodiscard]] inline constexpr bool llvm_lazy_urgent_worker_allowed(::std::size_t extra_compile_threads,
                                                                       bool native_unwind_requested) noexcept
    {
        // The resolved CLI budget applies to on-demand workers too: -Rct 0 means
        // no extra compile thread, not merely no initial/background worker.
        // Native unwind independently requires materialization on the execution
        // thread that holds the registry gate, even with a positive budget.
        return extra_compile_threads != 0uz && !native_unwind_requested;
    }
}

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger
{
    // Synchronous HOST management borrow only. Never parsed from Wasm/wire
    // bytes and never stored in a guest, controller or handler. A reserved
    // input adapter owns context for the full execute() call; requested must
    // be a bounded, nonthrowing flag query, without locks, IO or recursion.
    // Requesting interruption creates no frame/stop/memory/native authority.
    struct management_wait_interrupt
    {
        void* context{};
        bool (*requested)(void*) noexcept{};
        [[nodiscard]] bool pending() const noexcept
        { return requested != nullptr && requested(context); }
    };

    // One normal-management execute() owns this observation on its stack.
    // A trusted input adapter may clear/change its flag after the first query;
    // that cannot erase a cancellation already observed by THIS operation.
    // No OS handler accesses this object and no controller stores its borrow.
    class management_wait_interrupt_observation
    {
        management_wait_interrupt original_{};
        bool observed_{};
        [[nodiscard]] static bool query(void* context) noexcept
        {
            // [actual execute-owned observation object] observation_end
            // [safe                                   ] no guest/wire pointer
            //  ^^ borrow() passes this exact live object to synchronous queries;
            //     it cannot outlive execute() or be retained by its callee.
            auto& self{*static_cast<management_wait_interrupt_observation*>(context)};
            if(!self.observed_) { self.observed_ = self.original_.pending(); }
            return self.observed_;
        }
    public:
        explicit management_wait_interrupt_observation(management_wait_interrupt original) noexcept
            : original_{original} {}
        management_wait_interrupt_observation(management_wait_interrupt_observation const&) = delete;
        management_wait_interrupt_observation& operator=(management_wait_interrupt_observation const&) = delete;
        management_wait_interrupt_observation(management_wait_interrupt_observation&&) = delete;
        management_wait_interrupt_observation& operator=(management_wait_interrupt_observation&&) = delete;
        // Inspect THIS operation's already-delivered HOST event without a new
        // flag query. UI acknowledgement must not consume an unseen interrupt.
        [[nodiscard]] bool observed() const noexcept { return observed_; }
        [[nodiscard]] management_wait_interrupt borrow() noexcept { return {this, &query}; }
    };
}

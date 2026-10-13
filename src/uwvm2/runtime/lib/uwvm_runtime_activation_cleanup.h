/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <atomic>

namespace uwvm2::runtime::lib::details
{
    // One optional interpreter-frame scratch mark. The allocator belongs to this execution thread and must outlive
    // the scope; nested calls restore their own marks before this mark is released. No memory-access instrumentation.
    template<typename Allocator>
    class runtime_scratch_mark_scope
    {
        Allocator* allocator_{};
        typename Allocator::mark_t saved_{};

    public:
        explicit inline constexpr runtime_scratch_mark_scope(Allocator* allocator) noexcept : allocator_{allocator}
        {
            // [thread-owned allocator and blocks: live through this scope]
            //  ^^ allocator_ borrows the owner; nullptr selects the alloca path without touching TLS/map storage.
            if(allocator_ != nullptr) { saved_ = allocator_->mark(); }
        }

        runtime_scratch_mark_scope(runtime_scratch_mark_scope const&) = delete;
        runtime_scratch_mark_scope& operator=(runtime_scratch_mark_scope const&) = delete;

        inline constexpr ~runtime_scratch_mark_scope() noexcept
        {
            // Every reference into this frame's scratch suffix must be dead before this destructor runs. Restoring
            // the mark releases precisely that suffix on normal return, a tail-transfer return, or exception unwinding.
            // [older live frames | this frame and any completed nested frames]
            //                    ^^ saved_ restores the owned allocator cursor here; older frames remain valid.
            if(allocator_ != nullptr) { allocator_->release(saved_); }
        }
    };

    // Publish one fully initialized stack record to an already aligned, thread-owned atomic pointer slot. The slot,
    // previous record, and published record all outlive this scope. A signal-time reader may inspect the chain on the
    // owning thread; this is not a cross-thread reclamation scheme. Publish only after constructing the record and
    // destroy this scope before destroying the record, including exceptional exits.
    template<typename Record>
    class runtime_atomic_borrowed_record_scope
    {
        ::std::atomic_ref<Record const*> slot_;
        Record const* previous_;

    public:
        explicit inline runtime_atomic_borrowed_record_scope(Record const*& slot, Record const* previous,
                                                             Record const* published) noexcept
            : slot_{slot}, previous_{previous}
        {
            // [older live record] <- [published live record]
            //                         ^^ slot_ points here only for this scope's lifetime.
            slot_.store(published, ::std::memory_order_release);
        }

        runtime_atomic_borrowed_record_scope(runtime_atomic_borrowed_record_scope const&) = delete;
        runtime_atomic_borrowed_record_scope& operator=(runtime_atomic_borrowed_record_scope const&) = delete;

        inline ~runtime_atomic_borrowed_record_scope() noexcept
        {
            // [older live record] <- [published record about to leave its scope]
            //  ^^ restore the slot before the borrowed stack record can die.
            slot_.store(previous_, ::std::memory_order_release);
        }
    };
} // namespace uwvm2::runtime::lib::details

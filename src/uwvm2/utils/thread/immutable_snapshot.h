/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <memory>
# include <utility>
# include <exception>
# include <uwvm2/utils/macro/push_macros.h>
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#  include <mutex>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::utils::thread
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // Copy/publish metadata while registered readers borrow immutable versions.
    // Registration and writers take a mutex; acquire/release use only lock-free
    // pointer atomics and allocate nothing. Register outside signal/trap paths.
    // Each reader belongs to one thread and is non-reentrant while a borrow is
    // in use: acquire() again or release() invalidates its previous borrowed pointer.
    // A payload may not mutate objects reachable through it after publication.
    // update callbacks/destructors must not reenter this store. The owner must
    // outlive every reader (normally enforced by an enclosing execution domain).
    template<typename Payload>
    class immutable_snapshot
    {
        struct node
        {
            Payload value;
            node* retired_next{};
            node() = default;
            explicit node(Payload const& source) : value{source} {}
            explicit node(Payload&& source) : value{::std::move(source)} {}
        };
        static_assert(::std::atomic<node*>::is_always_lock_free,
                      "immutable snapshot readers require lock-free pointer atomics");
        struct slot
        {
            ::std::atomic<node*> hazard{};
            slot* previous{};
            slot* next{};
        };
        ::std::mutex writers{};
        ::std::atomic<node*> current{};
        node* retired{};
        slot* readers{};

        void collect_locked() noexcept
        {
            // [owner.retired -> live retired nodes ...] | null
            // ^^ link borrows an owner/list field under writers; readers never traverse it.
            auto** link{::std::addressof(retired)};
            while(*link != nullptr)
            {
                auto* candidate{*link};
                bool protected_now{};
                // [registered slots ...] | null; registration/unlink requires writers.
                auto* reader{readers};
                while(reader != nullptr)
                {
                    if(reader->hazard.load(::std::memory_order_seq_cst) == candidate) { protected_now = true; break; }
                    reader = reader->next;
                    // ^^ reader is a registered successor or null, checked before access.
                }
                if(protected_now)
                {
                    link = ::std::addressof(candidate->retired_next);
                    // ^^ link now borrows this retained node's list field.
                }
                else
                {
                    *link = candidate->retired_next;
                    // Remaining live list bypasses candidate before its payload is destroyed.
                    delete candidate;
                    // No subsequent candidate access; link still belongs to owner/retained predecessor.
                }
            }
        }
        void publish_locked(node* replacement) noexcept
        {
            // All payload writes precede SC publication. A reader that wins the
            // second current check has already published its hazard before any
            // retiring writer may miss it. A losing reader never dereferences old.
            auto* previous{current.exchange(replacement, ::std::memory_order_seq_cst)};
            if(previous != nullptr)
            {
                previous->retired_next = retired;
                // ^^ previous's private retirement link borrows the old live list head.
                retired = previous;
                // ^^ retired owns the previously active version until hazards release it.
            }
            collect_locked();
        }

    public:
        class reader
        {
            immutable_snapshot& owner;
            slot registration{};
        public:
            explicit reader(immutable_snapshot& store) : owner{store}
            {
                ::std::lock_guard lock{owner.writers};
                registration.next = owner.readers;
                // [new live slot] -> [old registered head?]; protected by writers.
                if(owner.readers != nullptr) { owner.readers->previous = ::std::addressof(registration); }
                owner.readers = ::std::addressof(registration);
                // ^^ registered head borrows this non-movable reader's live slot.
            }
            reader(reader const&) = delete;
            reader& operator=(reader const&) = delete;
            reader(reader&&) = delete;
            reader& operator=(reader&&) = delete;
            ~reader()
            {
                release();
                ::std::lock_guard lock{owner.writers};
                if(registration.previous != nullptr) { registration.previous->next = registration.next; }
                else { owner.readers = registration.next; }
                // Predecessor/head bypasses the live slot before it may be destroyed.
                if(registration.next != nullptr) { registration.next->previous = registration.previous; }
                // Successor no longer references this slot; writers cannot retain dangling links.
            }
            [[nodiscard]] Payload const* acquire() noexcept
            {
                node* observed{};
                do
                {
                    observed = owner.current.load(::std::memory_order_seq_cst);
                    // observed is only an address candidate; it MUST NOT be dereferenced yet.
                    registration.hazard.store(observed, ::std::memory_order_seq_cst);
                }
                while(observed != owner.current.load(::std::memory_order_seq_cst));
                // [hazard-protected published node] or null. The second SC check
                // proves publication; reclamation sees this hazard until release.
                return observed == nullptr ? nullptr : ::std::addressof(observed->value);
            }
            void release() noexcept { registration.hazard.store(nullptr, ::std::memory_order_seq_cst); }
        };

        immutable_snapshot() = default;
        immutable_snapshot(immutable_snapshot const&) = delete;
        immutable_snapshot& operator=(immutable_snapshot const&) = delete;
        ~immutable_snapshot()
        {
            ::std::lock_guard lock{writers};
            if(readers != nullptr) { ::std::terminate(); } // Owner lifetime contract, never silently free a live borrow.
            publish_locked(nullptr);
        }
        template<typename Update>
        void update(Update&& edit)
        {
            ::std::lock_guard lock{writers};
            auto const previous{current.load(::std::memory_order_seq_cst)};
            // [current node?] remains live under writers; construct a PRIVATE copy.
            auto replacement{previous == nullptr ? ::std::make_unique<node>() : ::std::make_unique<node>(previous->value)};
            ::std::forward<Update>(edit)(replacement->value);
            // Throwing copy/edit leaves the current version and reader borrows intact.
            publish_locked(replacement.release());
        }
        void replace(Payload value)
        {
            auto replacement{::std::make_unique<node>(::std::move(value))};
            ::std::lock_guard lock{writers};
            // Construct privately before publication; allocation/move failure
            // leaves the current snapshot and every reader's borrow unchanged.
            publish_locked(replacement.release());
        }
        void clear() noexcept { ::std::lock_guard lock{writers}; publish_locked(nullptr); }
        void collect() noexcept { ::std::lock_guard lock{writers}; collect_locked(); }
    };
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif

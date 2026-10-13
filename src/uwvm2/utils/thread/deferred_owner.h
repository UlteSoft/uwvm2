/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <exception>
# include <limits>
# include <memory>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::utils::thread
{
    class deferred_native_owner_queue;

    // Embed this node in a genuinely owned native object. Its final native
    // deleter transfers that WHOLE live object, never a stack borrow or a guest
    // address. The queue is a lifetime mechanism, not execution, pause, source,
    // collector, or root-completeness authority. Only the caller can establish
    // the safe boundary at which arbitrary native destruction is permitted.
    class deferred_native_owner
    {
        friend class deferred_native_owner_queue;
        using reclaim_function = void (*)(deferred_native_owner*) noexcept;
        reclaim_function const reclaim_;
        deferred_native_owner* next_{};
        bool queued_{};
    protected:
        explicit deferred_native_owner(reclaim_function reclaim) noexcept
            : reclaim_{reclaim}
        { if(reclaim_ == nullptr) { ::std::terminate(); } }
        ~deferred_native_owner() noexcept
        {
            // A transferred object remains alive until the queue removes its
            // exact node. Destroying an embedded/stack node early is invalid.
            if(queued_ || next_ != nullptr) { ::std::terminate(); }
        }
    public:
        deferred_native_owner(deferred_native_owner const&) = delete;
        deferred_native_owner& operator=(deferred_native_owner const&) = delete;
        deferred_native_owner(deferred_native_owner&&) = delete;
        deferred_native_owner& operator=(deferred_native_owner&&) = delete;

        // Native type authentication before downcasting an embedded queue node.
        // Only the actual immutable callback is compared; guest bytes, a marker
        // or an object's address are never treated as a concrete-type proof.
        [[nodiscard]] bool has_native_reclaimer(void (*expected)(deferred_native_owner*) noexcept) const noexcept
        { return reclaim_ == expected; }
    };

    // Native thread-local FIFO, with no allocation, shared count, mutex, or
    // callback during retirement. The caller must finish explicitly AFTER its
    // outer admission/pause/cohort/registry locks are gone and BEFORE the native
    // resources borrowed by reclamation disappear. Ordinary shared lifetime
    // leases may remain alive to protect those resources during the callbacks.
    // It cannot infer that boundary from a count, boolean, token, or TLS depth.
    class deferred_native_owner_queue final
    {
        inline static thread_local deferred_native_owner_queue* current_{};
        deferred_native_owner_queue* previous_{};
        deferred_native_owner* head_{};
        deferred_native_owner* tail_{};
        ::std::size_t count_{};
        bool finished_{};
        bool reclaiming_{};

        void enqueue(deferred_native_owner& owner) noexcept
        {
            if(finished_ || current_ != this || owner.queued_ || owner.next_ != nullptr ||
               count_ == (::std::numeric_limits<::std::size_t>::max)()) { ::std::terminate(); }
            // [queue-owned stable tail/null][new complete native owner]
            // [safe] no callback or owner destruction occurs while linking.
            if(tail_ != nullptr) { tail_->next_ = ::std::addressof(owner); }
            else
            {
                if(head_ != nullptr || count_ != 0uz) { ::std::terminate(); }
                // [empty head slot] publish the complete transferred object.
                head_ = ::std::addressof(owner);
            }
            // [new stable native node] becomes the queue's actual final node.
            tail_ = ::std::addressof(owner);
            owner.queued_ = true;
            ++count_;
        }
    public:
        deferred_native_owner_queue() noexcept
            // [live enclosing native queue/null] borrowed only on this thread;
            // nested queues finish before that enclosing scope can end.
            : previous_{current_}
        {
            // [this fully constructed native queue] TLS is not guest-visible.
            current_ = this;
        }
        deferred_native_owner_queue(deferred_native_owner_queue const&) = delete;
        deferred_native_owner_queue& operator=(deferred_native_owner_queue const&) = delete;
        deferred_native_owner_queue(deferred_native_owner_queue&&) = delete;
        deferred_native_owner_queue& operator=(deferred_native_owner_queue&&) = delete;
        ~deferred_native_owner_queue() noexcept
        {
            // Never silently run arbitrary deleters from a member destructor
            // whose surrounding admission/resource destruction order is unknown.
            if(!finished_ || reclaiming_ || previous_ != nullptr || head_ != nullptr || tail_ != nullptr || count_ != 0uz)
            { ::std::terminate(); }
        }
        [[nodiscard]] ::std::size_t pending_count() const noexcept { return count_; }
        [[nodiscard]] static bool active_on_current_thread() noexcept { return current_ != nullptr; }

        static void retire(deferred_native_owner& owner) noexcept
        {
            if(owner.queued_ || owner.next_ != nullptr) { ::std::terminate(); }
            // [actual current-thread queue/null] initialize from native TLS,
            // never by converting a guest integer into a queue pointer.
            auto* queue{current_};
            if(queue != nullptr) { queue->enqueue(owner); }
            else
            {
                // No native queue is bound here. The privileged caller must
                // already allow destruction; invoke only this owner's immutable
                // native callback and never touch the object afterward.
                owner.reclaim_(::std::addressof(owner));
            }
        }

        // The caller supplies a read-only, noexcept native predicate that proves
        // EACH selected destructor cannot run an arbitrary native callback at
        // this boundary. It separately retains all actual data/resource owners.
        // This utility authenticates queue/thread membership ONLY, and does not
        // infer any VM pause, GC admission, source or destructor authority.
        // Unqualified nodes keep their order and remain deferred for finish.
        template<class Eligible>
        [[nodiscard]] ::std::size_t reclaim_qualified_native_leaves(Eligible&& eligible) noexcept
        {
            static_assert(noexcept(eligible(static_cast<deferred_native_owner const*>(nullptr))));
            if(current_ != this || finished_ || reclaiming_) { ::std::terminate(); }
            reclaiming_ = true;
            ::std::size_t reclaimed{};
            // [real queue head slot or a retained predecessor's next slot]
            // [safe] link always names a live queue-owned native pointer member.
            auto** link{::std::addressof(head_)};
            deferred_native_owner* previous{};
            while(*link != nullptr)
            {
                // [owned complete queued node] lifetime remains until callback.
                auto* owner{*link};
                if(!owner->queued_ || count_ == 0uz) { ::std::terminate(); }
                if(!eligible(static_cast<deferred_native_owner const*>(owner)))
                {
                    // [retained complete node] preserves its original FIFO place.
                    // [safe] move the predecessor borrow to this still-live node.
                    previous = owner;
                    // [retained node's initialized successor slot]
                    // [safe] advance within this queue-owned object only.
                    link = ::std::addressof(owner->next_);
                    continue;
                }
                // [predecessor slot][selected leaf][initialized successor/null]
                // [safe] remove the edge before ending any selected lifetime.
                *link = owner->next_;
                if(tail_ == owner)
                {
                    // [retained predecessor/null] becomes the real final node.
                    // [safe] previous is still linked, or the queue is empty.
                    tail_ = previous;
                }
                // [detached complete native leaf] no queue borrows its next slot.
                // [safe] erase that borrow before its authenticated destructor.
                owner->next_ = nullptr;
                owner->queued_ = false;
                --count_;
                ++reclaimed;
                owner->reclaim_(owner);
                // [ended selected lifetime] owner is invalid and is not read.
                // Native child releases may append; link names only head or a
                // retained predecessor, so the next iteration remains valid.
                if(current_ != this || !reclaiming_) { ::std::terminate(); }
            }
            if((head_ == nullptr) != (tail_ == nullptr) || (head_ == nullptr) != (count_ == 0uz))
            { ::std::terminate(); }
            reclaiming_ = false;
            return reclaimed;
        }

        void finish_after_native_resume() noexcept
        {
            // TLS authenticates the native thread and stack nesting, ONLY.
            // The actual caller separately retires every outer admission lock.
            if(current_ != this || finished_ || reclaiming_) { ::std::terminate(); }
            reclaiming_ = true;
            while(head_ != nullptr)
            {
                // [owned complete native node] remains alive until its callback.
                auto* owner{head_};
                if(!owner->queued_ || count_ == 0uz) { ::std::terminate(); }
                // [actual initialized successor/null] remove the borrow BEFORE
                // ending this object's lifetime or entering arbitrary native code.
                head_ = owner->next_;
                if(head_ == nullptr)
                {
                    // [empty queue] no longer borrows the retiring final node.
                    tail_ = nullptr;
                }
                // [detached complete node] no live queue now links through it.
                owner->next_ = nullptr;
                owner->queued_ = false;
                --count_;
                owner->reclaim_(owner);
                // Reentrant last-owner releases append to THIS still-live queue
                // and are reclaimed iteratively. A nested callback queue must
                // finish before returning. Do not dereference owner after reclaim.
                if(current_ != this) { ::std::terminate(); }
            }
            if(tail_ != nullptr || count_ != 0uz) { ::std::terminate(); }
            reclaiming_ = false;
            // [live enclosing queue/null] restore only after complete draining.
            current_ = previous_;
            // [finished scope] erase the enclosing stack borrow before destruction.
            previous_ = nullptr;
            finished_ = true;
        }
    };
}

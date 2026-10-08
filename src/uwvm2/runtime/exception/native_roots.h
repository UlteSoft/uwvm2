/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)
 * Copyright (c) 2025-present UlteSoft. All rights reserved.
 * Licensed under the APL-2.0 License (see LICENSE file).
 *************************************************************/
#pragma once
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
#ifndef UWVM_MODULE
# include <cstddef>
# include <exception>
# include <limits>
# include <memory>
# include <mutex>
# include <type_traits>
# include <utility>
# include <uwvm2/runtime/exception/immutable_value.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
#include "native_roots_internal_record.h"
#else
UWVM_MODULE_EXPORT namespace uwvm2::runtime::exception
{
    enum class native_exception_root_status : unsigned char
    { empty, registered, unregistered, empty_value, empty_owner, closed, capacity_exhausted, invalid_source, retired };
    enum class native_exception_visit_status : unsigned char { ok, rejected };
    struct native_exception_visit_result
    {
        native_exception_visit_status status{native_exception_visit_status::ok};
        ::std::size_t visited{};
    };
    class immutable_exception_root_lease;

    // Explicit native owner census ONLY, not VM pause, source/generation/cohort
    // authority, token validity, or permission to enable collection. Stack-owned
    // domains still support scoped native leases with the caller's lifetime
    // contract. Only make() publishes a canonical shared lifetime pin for an
    // activation; an arbitrary nonempty shared alias is not that pin.
    class native_exception_root_domain
    {
        friend class immutable_exception_root_lease;
        mutable ::std::mutex mutex_{};
        immutable_exception_root_lease* head_{};
        ::std::size_t count_{};
        bool closed_{};
        ::std::weak_ptr<native_exception_root_domain> canonical_owner_{};

        void require_empty_locked(immutable_exception_root_lease const&) const noexcept;
        void link_owned_locked(immutable_exception_root_lease&) noexcept;
        [[nodiscard]] native_exception_root_status attach(immutable_exception_root_lease&, value_ref const&) noexcept;
        [[nodiscard]] native_exception_root_status attach_move(immutable_exception_root_lease&, value_ref&&) noexcept;
        [[nodiscard]] native_exception_root_status clone_existing(
            immutable_exception_root_lease&, immutable_exception_root_lease const&) noexcept;
        void transfer_existing(immutable_exception_root_lease&, immutable_exception_root_lease&) noexcept;
        void detach(immutable_exception_root_lease&) noexcept;

#if defined(UWVM_EXPERIMENTAL_OWNED_EXCEPTION_DOMAIN) && UWVM_EXPERIMENTAL_OWNED_EXCEPTION_DOMAIN == 1
    protected:
        // Only actual DERIVED lifetime/control block before publication.
        // No generation/cohort/root-completeness or collection authority.
        void bind_actual_derived_lifetime_owner_before_publication(
            ::std::shared_ptr<native_exception_root_domain> const& owner) noexcept
        {
            if(owner.get() != this || owner.use_count() == 0 || !canonical_owner_.expired() ||
               head_ != nullptr || count_ != 0uz || closed_) { ::std::terminate(); }
            canonical_owner_ = owner;
        }
#endif
    public:
        class registered_view
        {
            friend class native_exception_root_domain;
            immutable_exception_root_lease const* head_{};
            ::std::size_t count_{};
            explicit registered_view(immutable_exception_root_lease const* head, ::std::size_t count) noexcept
                : head_{head}, count_{count} {}
        public:
            registered_view(registered_view const&) = delete;
            registered_view& operator=(registered_view const&) = delete;
            registered_view(registered_view&&) = delete;
            registered_view& operator=(registered_view&&) = delete;
            [[nodiscard]] ::std::size_t size() const noexcept { return count_; }
            template<class Visitor>
            [[nodiscard]] native_exception_visit_result for_each_value(Visitor&&) const noexcept;
        };

        native_exception_root_domain() noexcept = default;
        native_exception_root_domain(native_exception_root_domain const&) = delete;
        native_exception_root_domain& operator=(native_exception_root_domain const&) = delete;
        native_exception_root_domain(native_exception_root_domain&&) = delete;
        native_exception_root_domain& operator=(native_exception_root_domain&&) = delete;
        ~native_exception_root_domain() noexcept
        {
            ::std::lock_guard lock{mutex_};
            if(head_ != nullptr || count_ != 0uz) { ::std::terminate(); }
            closed_ = true;
        }

        [[nodiscard]] static inline ::std::shared_ptr<native_exception_root_domain> make()
        {
            auto owner{::std::make_shared<native_exception_root_domain>()};
            // [fully constructed domain][its own actual control block]
            // [safe ] initialize immutable weak identity before publication.
            owner->canonical_owner_ = owner;
            return owner;
        }
        [[nodiscard]] static inline bool has_canonical_owner(
            ::std::shared_ptr<native_exception_root_domain> const& owner) noexcept
        {
            // This proves this native domain's lifetime owner ONLY. It cannot
            // authenticate a generation or a complete paused root census.
            return owner && owner.use_count() != 0 &&
                   !owner->canonical_owner_.owner_before(owner) &&
                   !owner.owner_before(owner->canonical_owner_);
        }
        void close() noexcept { ::std::lock_guard lock{mutex_}; closed_ = true; }
        [[nodiscard]] bool closed() const noexcept { ::std::lock_guard lock{mutex_}; return closed_; }
        [[nodiscard]] ::std::size_t registered_count() const noexcept
        { ::std::lock_guard lock{mutex_}; return count_; }

        // One locked census can be enumerated twice. The OUTER callback may
        // prepare checked bounded native snapshots/work under a REAL closed VM
        // pause/admission. It cannot execute host/guest code, reenter/mutate this
        // domain, create/reset roots, destroy owners, or resume execution. The
        // per-value VISITOR also cannot allocate, throw, retain a borrow or call
        // native code. Retired values must be released outside this lock and all
        // registry/cohort/pause locks. This mutex itself is not pause authority.
        template<class Visit>
        [[nodiscard]] bool with_registered_roots(Visit&& visit) const noexcept
        {
            static_assert(::std::is_nothrow_invocable_r_v<bool, Visit&, registered_view const&>);
            ::std::lock_guard lock{mutex_};
            // [actual domain-owned native chain] or null, pinned by mutex.
            // [safe ] view borrows only until this callback returns.
            registered_view const view{head_, count_};
            return visit(view);
        }
    };

    // A supplied value is a canonical immutable value::make/make_owned owner;
    // an ordinary shared_ptr cannot authenticate alien nonempty-owner aliases.
    // Lease nodes are noncopyable/nonmovable. Explicit snapshot transfer below
    // changes their published address ONLY while holding the owning census
    // lock, without a root gap, allocation, or owner release. It is not a C++
    // move of links. Concurrent/recursive operations on one lease are forbidden.
    class immutable_exception_root_lease
    {
        friend class native_exception_root_domain;
        friend class native_exception_root_domain::registered_view;
        value_ref value_{};
        native_exception_root_domain* owner_{};
        immutable_exception_root_lease* previous_{};
        immutable_exception_root_lease* next_{};
        native_exception_root_status status_{native_exception_root_status::empty};
        void require_private_empty() const noexcept
        {
            if(owner_ != nullptr || previous_ != nullptr || next_ != nullptr || value_) { ::std::terminate(); }
        }
    public:
        immutable_exception_root_lease() noexcept = default;
        explicit immutable_exception_root_lease(native_exception_root_domain& domain, value_ref const& validated) noexcept
        { status_ = domain.attach(*this, validated); }
        explicit immutable_exception_root_lease(native_exception_root_domain& domain, value_ref&& validated) noexcept
        { status_ = domain.attach_move(*this, ::std::move(validated)); }
        immutable_exception_root_lease(immutable_exception_root_lease const&) = delete;
        immutable_exception_root_lease& operator=(immutable_exception_root_lease const&) = delete;
        immutable_exception_root_lease(immutable_exception_root_lease&&) = delete;
        immutable_exception_root_lease& operator=(immutable_exception_root_lease&&) = delete;
        ~immutable_exception_root_lease() noexcept { reset(); }

        [[nodiscard]] explicit operator bool() const noexcept { return owner_ != nullptr; }
        [[nodiscard]] native_exception_root_status status() const noexcept { return status_; }
        [[nodiscard]] value_ref const& instance() const noexcept { return value_; }
        // Identity borrowing only, not source/cohort/admission authority. Caller
        // keeps the real domain lifetime pin and this lease stable throughout.
        [[nodiscard]] native_exception_root_domain const* domain_identity() const noexcept { return owner_; }

        // Legacy activations remain strongly owned but NOT census registrations.
        // Untracked external shared copies/exception_ptr paths still require
        // native_roots_unknown and defined-tag gates. Preserve the old nonnull
        // activation contract; this path does not invent canonical GC authority.
        void adopt_unregistered(value_ref const& instance) noexcept
        {
            require_private_empty();
            if(!instance) { ::std::terminate(); }
            value_ = instance;
            status_ = native_exception_root_status::unregistered;
        }
        void adopt_unregistered(value_ref&& instance) noexcept
        {
            require_private_empty();
            if(!instance) { ::std::terminate(); }
            value_ = ::std::move(instance);
            status_ = native_exception_root_status::unregistered;
        }
        [[nodiscard]] native_exception_root_status clone_registered_from(
            immutable_exception_root_lease const& source) noexcept
        {
            require_private_empty();
            // [source-owned live domain] source and its lifetime pin stay live.
            // [safe ] clone authenticates this actual registration under lock.
            auto* owner{source.owner_};
            if(owner == nullptr) { return native_exception_root_status::invalid_source; }
            status_ = owner->clone_existing(*this, source);
            return status_;
        }
        void take_prepared_snapshot(immutable_exception_root_lease& source) noexcept
        {
            require_private_empty();
            if(this == ::std::addressof(source) || !source.value_) { ::std::terminate(); }
            // [real source registration or unregistered owned snapshot]
            // [safe ] source has one native owner, with a genuine domain pin.
            auto* owner{source.owner_};
            if(owner != nullptr) { owner->transfer_existing(*this, source); }
            else
            {
                if(source.previous_ != nullptr || source.next_ != nullptr ||
                   source.status_ != native_exception_root_status::unregistered) { ::std::terminate(); }
                value_.swap(source.value_);
                status_ = native_exception_root_status::unregistered;
                source.status_ = native_exception_root_status::retired;
            }
        }
        void reset() noexcept
        {
            // [domain pin alive] or private unregistered owner; this is not a
            // collector operation. No other native owner can mutate this lease.
            auto* owner{owner_};
            if(owner != nullptr) { owner->detach(*this); }
            else
            {
                if(previous_ != nullptr || next_ != nullptr) { ::std::terminate(); }
                value_ref retired{};
                value_.swap(retired);
                status_ = native_exception_root_status::retired;
                retired.reset(); // Arbitrary owner destruction outside every census lock.
            }
        }
    };

    inline void native_exception_root_domain::require_empty_locked(
        immutable_exception_root_lease const& node) const noexcept
    { node.require_private_empty(); }

    inline void native_exception_root_domain::link_owned_locked(immutable_exception_root_lease& node) noexcept
    {
        if(!node.value_) { ::std::terminate(); }
        // [live domain] <- [private owned node] -> [registered old head/null]
        // [safe ] all actual native objects stay live while mutex_ is held.
        node.owner_ = this;
        node.next_ = head_;
        if(head_ != nullptr)
        {
            // [registered old head] its new predecessor is fully initialized.
            head_->previous_ = ::std::addressof(node);
        }
        // [domain head slot] publish the real stable node after its owned value.
        head_ = ::std::addressof(node);
        ++count_;
        node.status_ = native_exception_root_status::registered;
    }

    [[nodiscard]] inline native_exception_root_status native_exception_root_domain::attach(
        immutable_exception_root_lease& node, value_ref const& validated) noexcept
    {
        if(!validated) { return native_exception_root_status::empty_value; }
        if(validated.use_count() == 0) { return native_exception_root_status::empty_owner; }
        ::std::lock_guard lock{mutex_};
        require_empty_locked(node);
        if(closed_) { return native_exception_root_status::closed; }
        if(count_ == (::std::numeric_limits<::std::size_t>::max)()) { return native_exception_root_status::capacity_exhausted; }
        node.value_ = validated;
        link_owned_locked(node);
        return native_exception_root_status::registered;
    }
    [[nodiscard]] inline native_exception_root_status native_exception_root_domain::attach_move(
        immutable_exception_root_lease& node, value_ref&& validated) noexcept
    {
        if(!validated) { return native_exception_root_status::empty_value; }
        if(validated.use_count() == 0) { return native_exception_root_status::empty_owner; }
        ::std::lock_guard lock{mutex_};
        require_empty_locked(node);
        if(closed_) { return native_exception_root_status::closed; }
        if(count_ == (::std::numeric_limits<::std::size_t>::max)()) { return native_exception_root_status::capacity_exhausted; }
        node.value_ = ::std::move(validated); // Consume only after every rejection check.
        link_owned_locked(node);
        return native_exception_root_status::registered;
    }
    [[nodiscard]] inline native_exception_root_status native_exception_root_domain::clone_existing(
        immutable_exception_root_lease& node, immutable_exception_root_lease const& source) noexcept
    {
        ::std::lock_guard lock{mutex_};
        require_empty_locked(node);
        if(source.owner_ != this || source.status_ != native_exception_root_status::registered || !source.value_)
        { return native_exception_root_status::invalid_source; }
        if(count_ == (::std::numeric_limits<::std::size_t>::max)()) { return native_exception_root_status::capacity_exhausted; }
        // closed_ intentionally does not reject an actual registered owner's
        // copy. The source is already in this locked census; no unknown value,
        // count, guessed pointer or generic bool can create the clone authority.
        node.value_ = source.value_;
        link_owned_locked(node);
        return native_exception_root_status::registered;
    }
    inline void native_exception_root_domain::transfer_existing(
        immutable_exception_root_lease& node, immutable_exception_root_lease& source) noexcept
    {
        ::std::lock_guard lock{mutex_};
        require_empty_locked(node);
        if(source.owner_ != this || source.status_ != native_exception_root_status::registered ||
           !source.value_ || count_ == 0uz) { ::std::terminate(); }
        // [registered source][private empty destination] both native lifetimes
        // are active. Replace the exact list address under ONE census lock.
        node.value_.swap(source.value_);
        // [old source predecessor/successor] these remain mutex-protected live links.
        node.owner_ = this;
        node.previous_ = source.previous_;
        node.next_ = source.next_;
        if(source.previous_ != nullptr)
        {
            if(source.previous_->next_ != ::std::addressof(source)) { ::std::terminate(); }
            // [proved predecessor] redirects only this real existing registration.
            source.previous_->next_ = ::std::addressof(node);
        }
        else
        {
            if(head_ != ::std::addressof(source)) { ::std::terminate(); }
            // [real head slot] now names the new fully owned stable node.
            head_ = ::std::addressof(node);
        }
        if(source.next_ != nullptr)
        {
            if(source.next_->previous_ != ::std::addressof(source)) { ::std::terminate(); }
            // [proved successor] its predecessor is the new real registration.
            source.next_->previous_ = ::std::addressof(node);
        }
        node.status_ = native_exception_root_status::registered;
        // [detached empty source] no census can now reach this private node.
        source.owner_ = nullptr;
        source.previous_ = nullptr;
        source.next_ = nullptr;
        source.status_ = native_exception_root_status::retired;
        // Cardinality and all immutable ownership are unchanged by relocation.
    }
    inline void native_exception_root_domain::detach(immutable_exception_root_lease& node) noexcept
    {
        value_ref retired{};
        {
            ::std::lock_guard lock{mutex_};
            if(node.owner_ != this || !node.value_ || count_ == 0uz) { ::std::terminate(); }
            if(node.previous_ != nullptr)
            {
                if(node.previous_->next_ != ::std::addressof(node)) { ::std::terminate(); }
                // [proved predecessor][this real lease][proved successor/null]
                // [safe ] unlink before ending any native node/value lifetime.
                node.previous_->next_ = node.next_;
            }
            else
            {
                if(head_ != ::std::addressof(node)) { ::std::terminate(); }
                // [real head slot] removes this owned native node first.
                head_ = node.next_;
            }
            if(node.next_ != nullptr)
            {
                if(node.next_->previous_ != ::std::addressof(node)) { ::std::terminate(); }
                // [proved successor] no longer links back to this lease.
                node.next_->previous_ = node.previous_;
            }
            --count_;
            // [fully detached node] clear all borrowing links before release.
            node.owner_ = nullptr;
            node.previous_ = nullptr;
            node.next_ = nullptr;
            node.value_.swap(retired);
            node.status_ = native_exception_root_status::retired;
        }
        retired.reset(); // Tag/payload native deleters may reenter ONLY outside locks.
    }
    template<class Visitor>
    [[nodiscard]] inline native_exception_visit_result
        native_exception_root_domain::registered_view::for_each_value(Visitor&& visitor) const noexcept
    {
        static_assert(::std::is_nothrow_invocable_r_v<bool, Visitor&, value const&>);
        native_exception_visit_result result{};
        // [mutex-protected actual lease chain] or null; borrowed head stays live.
        auto const* cursor{head_};
        while(cursor != nullptr)
        {
            if(result.visited == count_ || !cursor->value_ || cursor->owner_ == nullptr ||
               cursor->status_ != native_exception_root_status::registered) { ::std::terminate(); }
            ++result.visited;
            if(!visitor(*cursor->value_)) { result.status = native_exception_visit_status::rejected; return result; }
            // [initialized actual successor/null] protected until visitor returns.
            cursor = cursor->next_;
        }
        if(result.visited != count_) { ::std::terminate(); }
        return result;
    }
}
#endif
#endif

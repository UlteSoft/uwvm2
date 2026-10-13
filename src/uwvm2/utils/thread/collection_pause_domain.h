/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <chrono>
# include <cstddef>
# include <cstdint>
# include <exception>
# include <limits>
# include <memory>
# include <utility>
# include <vector>
# include <uwvm2/utils/macro/push_macros.h>
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#  include <condition_variable>
#  include <mutex>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
namespace uwvm2::uwvm::runtime::storage { class sealed_compact_entry; }
#endif
UWVM_MODULE_EXPORT namespace uwvm2::utils::thread
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    enum class collection_pause_result : unsigned { paused, timeout, stale_ticket, closed };

    // Host-only stop coordination, independent of Wasm and the debugger. The
    // collector is normally an enrolled allocating thread: its own published
    // context counts as stopped without asking that thread to wait for itself.
    // An explicitly parked blocking operation also counts as stopped, but must
    // leave its blocking scope BEFORE changing roots or reentering the guest.
    // This class does not enumerate roots, suspend an OS thread, or collect.
    // Unenrolled readers and native handles remain the caller's responsibility.
    class collection_pause_domain
    {
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        friend class ::uwvm2::uwvm::runtime::storage::sealed_compact_entry;
#endif
        struct slot
        {
            ::std::uint_least64_t id{};
            ::std::uint_least64_t native_owner{};
            void const* root_context{};
            bool parked{};
            bool blocking{};
        };
        ::std::mutex mutex_{};
        ::std::condition_variable changed_{};
        ::std::atomic_bool requested_{};
        ::std::vector<slot> slots_;
        ::std::shared_ptr<void const> identity_{::std::make_shared<unsigned char>()};
        ::std::size_t active_{}, parked_{}, admission_waiters_{}, tickets_{};
        ::std::size_t initiator_index_{};
        ::std::uint_least64_t serial_{}, next_participant_{1u}, initiator_id_{};
        bool closed_{};
        // OS thread ids and TLS addresses can be recycled after a thread exits.
        // Reserve a process-unique cookie on enrollment/the first cold request;
        // neither a peer lease nor a later thread can impersonate the initiator.
        inline static ::std::atomic<::std::uint_least64_t> next_native_owner_{1u};
        inline static thread_local ::std::uint_least64_t native_owner_{};

        [[nodiscard]] static ::std::uint_least64_t current_native_owner() noexcept
        {
            if(native_owner_ != 0u) { return native_owner_; }
            auto next{next_native_owner_.load(::std::memory_order_relaxed)};
            do
            {
                if(next == 0u || next == (::std::numeric_limits<::std::uint_least64_t>::max)())
                { ::std::terminate(); }
            }
            while(!next_native_owner_.compare_exchange_weak(next, next + 1u,
                ::std::memory_order_relaxed, ::std::memory_order_relaxed));
            native_owner_ = next;
            return next;
        }

        void cancel_locked() noexcept
        {
            if(initiator_id_ != 0u)
            {
                // [fixed live slot allocation] this id was leased when the
                // collector published its context; leave() rejects early reset.
                auto& entry{slots_[initiator_index_]};
                if(entry.id != initiator_id_ || !entry.parked || entry.blocking)
                { ::std::terminate(); }
                entry.parked = false;
                // [collector-owned context] clear this borrow before the
                // collecting thread may mutate or destroy its frame again.
                entry.root_context = nullptr;
                --parked_;
                initiator_id_ = 0u;
            }
            requested_.store(false, ::std::memory_order_release);
            changed_.notify_all();
        }
        void release_ticket(::std::uint_least64_t serial, void const* identity) noexcept
        {
            ::std::lock_guard lock{mutex_};
            if(identity != identity_.get() || tickets_ == 0uz) { ::std::terminate(); }
            if(serial == serial_ && requested_.load(::std::memory_order_relaxed)) { cancel_locked(); }
            --tickets_;
            changed_.notify_all();
        }
        void leave(::std::size_t index) noexcept
        {
            ::std::lock_guard lock{mutex_};
            // [fixed leased slot] only its native owner can release it. A
            // blocking scope or live collecting ticket must end first.
            auto& entry{slots_[index]};
            if(entry.id == 0u || entry.native_owner != native_owner_ ||
               entry.blocking || entry.id == initiator_id_)
            { ::std::terminate(); }
            if(entry.parked) { --parked_; }
            // [completed reader] clearing the context retires its root borrow
            // before admission can assign this slot a fresh nonrecycling id.
            entry = {};
            --active_;
            changed_.notify_all();
        }
        void poll(::std::size_t index, void const* context) noexcept
        {
            if(!requested_.load(::std::memory_order_acquire)) { return; }
            ::std::unique_lock lock{mutex_};
            if(closed_ || !requested_.load(::std::memory_order_relaxed)) { return; }
            auto& entry{slots_[index]}; // index belongs to the caller's live fixed slot.
            if(entry.parked || entry.blocking || entry.id == 0u ||
               entry.native_owner != native_owner_) { ::std::terminate(); }
            // [caller-owned complete root frame] the mutex publishes every
            // prior root write; the reader changes nothing until the park ends.
            entry.root_context = context;
            entry.parked = true;
            ++parked_;
            changed_.notify_all();
            changed_.wait(lock, [&] { return closed_ || !requested_.load(::std::memory_order_relaxed); });
            entry.parked = false;
            // [resuming reader] no stopped-world callback can retain this
            // borrowing pointer after releasing the domain mutex.
            entry.root_context = nullptr;
            --parked_;
            changed_.notify_all();
        }
        void finish_block(::std::size_t index, ::std::uint_least64_t id) noexcept
        {
            ::std::unique_lock lock{mutex_};
            auto& entry{slots_[index]}; // the participant outlives this blocking scope.
            if(entry.id != id || !entry.parked || !entry.blocking ||
               entry.native_owner != native_owner_) { ::std::terminate(); }
            // A wakeup is not permission to mutate roots while a collection is
            // stopped. A new request before this lock is acquired keeps us parked.
            changed_.wait(lock, [&] { return closed_ || !requested_.load(::std::memory_order_relaxed); });
            entry.blocking = false;
            entry.parked = false;
            // [blocking operation finished] retire the root-context borrow
            // before caller code can alter its frame or reenter the guest.
            entry.root_context = nullptr;
            --parked_;
            changed_.notify_all();
        }

    public:
        class participant;
        class pause_ticket
        {
            friend class collection_pause_domain;
            collection_pause_domain* owner_{};
            ::std::shared_ptr<void const> identity_{};
            ::std::uint_least64_t serial_{};
            ::std::uint_least64_t native_owner_cookie_{};
            pause_ticket(collection_pause_domain& owner, ::std::uint_least64_t serial) noexcept
                : owner_{::std::addressof(owner)}, identity_{owner.identity_}, serial_{serial},
                  native_owner_cookie_{native_owner_} {}
            [[nodiscard]] static collection_pause_domain* take_owner(pause_ticket& other) noexcept
            {
                if(other.owner_ != nullptr && other.native_owner_cookie_ != native_owner_)
                { ::std::terminate(); }
                // [native-owned ticket lease] move on its owning native thread
                // only; a management peer synchronizes through domain.close().
                return ::std::exchange(other.owner_, nullptr);
            }
        public:
            // Ticket access/move/reset belongs to one native thread. In
            // particular a const& must not race another thread's reset/move.
            pause_ticket() noexcept = default;
            pause_ticket(pause_ticket const&) = delete;
            pause_ticket& operator=(pause_ticket const&) = delete;
            pause_ticket(pause_ticket&& other) noexcept
                : owner_{take_owner(other)}, identity_{::std::move(other.identity_)},
                  serial_{other.serial_}, native_owner_cookie_{other.native_owner_cookie_} {}
            pause_ticket& operator=(pause_ticket&& other) noexcept
            {
                if(this != ::std::addressof(other))
                {
                    reset();
                    // [live ticket lease] transfer the domain borrow once;
                    // the source must no longer cancel this collection.
                    owner_ = take_owner(other);
                    identity_ = ::std::move(other.identity_);
                    serial_ = other.serial_;
                    native_owner_cookie_ = other.native_owner_cookie_;
                }
                return *this;
            }
            ~pause_ticket() { reset(); }
            [[nodiscard]] explicit operator bool() const noexcept { return owner_ != nullptr; }
            void reset() noexcept
            {
                if(owner_ != nullptr)
                {
                    if(native_owner_cookie_ != native_owner_) { ::std::terminate(); }
                    // [live domain retained by tickets_] clear the borrowed
                    // pointer first; drain may complete after release_ticket.
                    auto* owner{::std::exchange(owner_, nullptr)};
                    owner->release_ticket(serial_, identity_.get());
                    identity_.reset(); // never access owner after releasing its lease.
                }
            }
        };
        class blocking_scope
        {
            friend class collection_pause_domain;
            collection_pause_domain* owner_{};
            ::std::size_t index_{};
            ::std::uint_least64_t id_{};
            blocking_scope(collection_pause_domain& owner, ::std::size_t index, ::std::uint_least64_t id) noexcept
                : owner_{::std::addressof(owner)}, index_{index}, id_{id} {}
        public:
            blocking_scope() noexcept = default;
            blocking_scope(blocking_scope const&) = delete;
            blocking_scope& operator=(blocking_scope const&) = delete;
            blocking_scope(blocking_scope&& other) noexcept
                : owner_{::std::exchange(other.owner_, nullptr)}, index_{other.index_}, id_{other.id_} {}
            blocking_scope& operator=(blocking_scope&& other) noexcept
            {
                if(this != ::std::addressof(other))
                {
                    reset();
                    // [participant-owned live domain] transfer this unique
                    // blocking borrow; the source can no longer unpark it.
                    owner_ = ::std::exchange(other.owner_, nullptr);
                    index_ = other.index_; id_ = other.id_;
                }
                return *this;
            }
            ~blocking_scope() { reset(); }
            [[nodiscard]] explicit operator bool() const noexcept { return owner_ != nullptr; }
            void reset() noexcept
            {
                if(owner_ != nullptr)
                {
                    // [participant outlives this scope] clear the scope's
                    // borrow before permitting the native owner to run again.
                    auto* owner{::std::exchange(owner_, nullptr)};
                    owner->finish_block(index_, id_);
                }
            }
        };
        class participant
        {
            friend class collection_pause_domain;
            collection_pause_domain* owner_{};
            ::std::size_t index_{};
            participant(collection_pause_domain& owner, ::std::size_t index) noexcept
                : owner_{::std::addressof(owner)}, index_{index} {}
        public:
            participant() noexcept = default;
            participant(participant const&) = delete;
            participant& operator=(participant const&) = delete;
            participant(participant&& other) noexcept
                : owner_{::std::exchange(other.owner_, nullptr)}, index_{other.index_} {}
            participant& operator=(participant&& other) noexcept
            {
                if(this != ::std::addressof(other))
                {
                    reset();
                    // [leased fixed slot] transfer its borrowing owner pointer
                    // exactly once; the source must no longer call leave().
                    owner_ = ::std::exchange(other.owner_, nullptr);
                    index_ = other.index_;
                }
                return *this;
            }
            ~participant() { reset(); }
            [[nodiscard]] explicit operator bool() const noexcept { return owner_ != nullptr; }
            [[nodiscard]] ::std::uint_least64_t identifier() const noexcept
            { return owner_ == nullptr ? 0u : owner_->slots_[index_].id; }
            // One native thread owns this lease. Nested host callbacks reuse
            // it, and no call is allowed while its blocking/collecting scope lives.
            void poll(void const* context) const noexcept
            { if(owner_ != nullptr) { owner_->poll(index_, context); } }
            [[nodiscard]] blocking_scope park_for_blocking(void const* context) const noexcept
            { return owner_ == nullptr ? blocking_scope{} : owner_->park(index_, context); }
            void reset() noexcept
            {
                if(owner_ != nullptr)
                {
                    // [domain held by active_] invalidate the participant
                    // borrow before leave() can wake a draining destructor.
                    auto* owner{::std::exchange(owner_, nullptr)};
                    owner->leave(index_);
                }
            }
        };
        struct stopped_participant
        {
            ::std::uint_least64_t id{};
            void const* root_context{};
            bool blocking{}, collecting{};
        };
        class stopped_view
        {
            friend class collection_pause_domain;
            slot const* slots_{};
            ::std::size_t extent_{}, active_{};
            ::std::uint_least64_t initiator_id_{};
            stopped_view(slot const* slots, ::std::size_t extent, ::std::size_t active,
                         ::std::uint_least64_t initiator) noexcept
                : slots_{slots}, extent_{extent}, active_{active}, initiator_id_{initiator} {}
        public:
            [[nodiscard]] ::std::size_t participant_count() const noexcept { return active_; }
            template<typename Visitor> void for_each(Visitor&& visitor) const noexcept
            {
                static_assert(noexcept(visitor(stopped_participant{})));
                for(::std::size_t index{}; index != extent_; ++index)
                {
                    // [fixed slot allocation] only leased entries are exposed.
                    // The stopped-world commit holds mutex_ until iteration ends.
                    auto const& entry{slots_[index]};
                    if(entry.id != 0u)
                    { visitor(stopped_participant{entry.id, entry.root_context, entry.blocking, entry.id == initiator_id_}); }
                }
            }
        };

    private:
        [[nodiscard]] bool current(pause_ticket const& ticket) const noexcept
        {
            return ticket.native_owner_cookie_ == native_owner_ && ticket.owner_ == this &&
                   ticket.identity_.get() == identity_.get() &&
                   ticket.serial_ == serial_ && requested_.load(::std::memory_order_relaxed);
        }
        [[nodiscard]] blocking_scope park(::std::size_t index, void const* context) noexcept
        {
            ::std::lock_guard lock{mutex_};
            auto& entry{slots_[index]}; // participant lease fixes index and slot storage.
            if(closed_ || entry.parked || entry.blocking || entry.id == 0u ||
               entry.native_owner != native_owner_) { return {}; }
            // [complete immutable blocking context] publish it before the
            // native owner enters a wait; a collector need not wake that wait.
            entry.root_context = context;
            entry.parked = entry.blocking = true;
            ++parked_;
            changed_.notify_all();
            return blocking_scope{*this, index, entry.id};
        }
        [[nodiscard]] pause_ticket begin(participant const* initiator, void const* context) noexcept
        {
            auto const native_owner{current_native_owner()};
            ::std::lock_guard lock{mutex_};
            if(closed_ || requested_.load(::std::memory_order_relaxed) ||
               serial_ == (::std::numeric_limits<::std::uint_least64_t>::max)()) { return {}; }
            if(initiator != nullptr)
            {
                if(initiator->owner_ != this) { return {}; }
                auto& entry{slots_[initiator->index_]};
                if(entry.id == 0u || entry.parked || entry.blocking ||
                   entry.native_owner != native_owner) { return {}; }
                initiator_index_ = initiator->index_;
                initiator_id_ = entry.id;
                // [collector's complete root frame] the collecting native
                // thread will not mutate guest state while its ticket is live.
                entry.root_context = context;
                entry.parked = true;
                ++parked_;
            }
            ++serial_; ++tickets_;
            requested_.store(true, ::std::memory_order_release);
            changed_.notify_all();
            return pause_ticket{*this, serial_};
        }

    public:
        explicit collection_pause_domain(::std::size_t max_concurrent) : slots_(max_concurrent) {}
        collection_pause_domain(collection_pause_domain const&) = delete;
        collection_pause_domain& operator=(collection_pause_domain const&) = delete;
        // Shutdown first stops callers and cancels native waits. It must not
        // destroy a domain while holding one of its own participants/tickets.
        ~collection_pause_domain() { close(); drain(); }
        [[nodiscard]] participant enter()
        {
            auto const native_owner{current_native_owner()};
            ::std::unique_lock lock{mutex_};
            ++admission_waiters_;
            changed_.wait(lock, [&] { return closed_ || !requested_.load(::std::memory_order_relaxed); });
            --admission_waiters_; changed_.notify_all();
            if(closed_ || active_ == slots_.size() || next_participant_ == 0u) { return {}; }
            for(::std::size_t index{}; index != slots_.size(); ++index)
            {
                auto& entry{slots_[index]}; // index is strictly below fixed capacity.
                if(entry.id == 0u)
                {
                    entry.id = next_participant_++;
                    entry.native_owner = native_owner;
                    ++active_;
                    return participant{*this, index};
                }
            }
            return {};
        }
        // A failed concurrent request owns no stop. The allocating caller must
        // publish/poll for the other collector before retrying or failing safely.
        // This overload accepts ONLY the calling native thread's participant;
        // possession of a peer's lease never proves that peer has stopped.
        [[nodiscard]] pause_ticket request_pause(participant const& initiator, void const* context) noexcept
        { return begin(::std::addressof(initiator), context); }
        // For an unenrolled management thread only; never call this overload
        // while owning an active participant or it would wait for itself.
        [[nodiscard]] pause_ticket request_pause() noexcept { return begin(nullptr, nullptr); }
        [[nodiscard]] collection_pause_result wait_until_paused(pause_ticket const& ticket,
            ::std::chrono::steady_clock::time_point deadline)
        {
            ::std::unique_lock lock{mutex_};
            auto const ready{[&] { return closed_ || !current(ticket) || parked_ == active_; }};
            // A timeout keeps the request AND the initiator's published roots.
            // Reset the ticket before altering roots, entering guest/host code,
            // or retrying allocation. Only while_stopped() permits a sweep.
            if(!changed_.wait_until(lock, deadline, ready)) { return collection_pause_result::timeout; }
            if(closed_) { return collection_pause_result::closed; }
            if(!current(ticket)) { return collection_pause_result::stale_ticket; }
            return collection_pause_result::paused;
        }
        // The callback may inspect borrowed contexts and commit a collection.
        // It must not reenter this domain, execute guest code, throw, or retain
        // the view/contexts after return. Holding mutex_ excludes admission,
        // park completion, close and ticket cancellation for the entire commit.
        template<typename Commit>
        [[nodiscard]] collection_pause_result while_stopped(pause_ticket const& ticket, Commit&& commit) noexcept
        {
            static_assert(noexcept(commit(::std::declval<stopped_view>())));
            ::std::lock_guard lock{mutex_};
            if(closed_) { return collection_pause_result::closed; }
            if(!current(ticket)) { return collection_pause_result::stale_ticket; }
            if(parked_ != active_) { return collection_pause_result::timeout; }
            commit(stopped_view{slots_.data(), slots_.size(), active_, initiator_id_});
            return collection_pause_result::paused;
        }
        [[nodiscard]] bool pause_requested() const noexcept { return requested_.load(::std::memory_order_acquire); }
        void close() noexcept
        {
            ::std::lock_guard lock{mutex_};
            closed_ = true;
            cancel_locked();
        }
        void drain() noexcept
        {
            ::std::unique_lock lock{mutex_};
            changed_.wait(lock, [&] { return active_ == 0uz && tickets_ == 0uz && admission_waiters_ == 0uz; });
        }
    };
#endif
}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif

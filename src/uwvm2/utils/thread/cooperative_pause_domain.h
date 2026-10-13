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
# include <span>
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
UWVM_MODULE_EXPORT namespace uwvm2::utils::thread
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // Backend-independent, host-only coordination. Authorization belongs to the
    // controller: receiving a guest signal, descriptor, or message never grants
    // access to this object. Only explicitly enabled executions enroll here.
    // A participant polls outside runtime locks at compiler-selected safe points.
    // Ordinary executions do not enroll or poll. This is not an asynchronous
    // thread suspender: an uncooperative host call can make a pause time out.
    struct cooperative_pause_location
    {
        ::std::uint_least64_t code_unit{}, function{}, offset{}, code_generation{};
        friend constexpr bool operator==(cooperative_pause_location, cooperative_pause_location) noexcept = default;
    };
    enum class cooperative_pause_result : unsigned { paused, timeout, stale_ticket, closed };

    class cooperative_pause_domain
    {
    public:
        class participant;
        class pause_waker;
    private:
        struct slot
        {
            ::std::uint_least64_t id{};
            cooperative_pause_location location{};
            bool parked{};
            // Debugger-only native stepping may release exactly this guest from
            // the cooperative wait, then account for its kernel-trapped thread
            // as parked again. Neither state is set by ordinary Wasm polling.
            bool release_once{};
            bool externally_parked{};
        };
        ::std::mutex mutex_{};
        ::std::condition_variable changed_{};
        pause_waker* waker_first_{}; // Stack registrations, ONLY under mutex_.
        ::std::size_t waker_count_{}; // Cold opt-in waits; ordinary poll unchanged.
        // This is the only shared read on the enabled, running polling path.
        // Acquire pairs with the controller's request; recheck under the mutex
        // handles resume/re-pause races without trusting a stale fast-path read.
        ::std::atomic_bool requested_{};
        ::std::vector<slot> slots_;
        ::std::shared_ptr<void const> identity_{::std::make_shared<unsigned char>()};
        ::std::size_t active_{}, parked_{}, admission_waiters_{};
        ::std::uint_least64_t serial_{}, next_participant_{1};
        ::std::atomic_bool closed_{};
        // Cold management transfer only. The enabled running poll remains its
        // original one requested_ acquire; no transition field is read by Wasm.
        bool transition_held_{};
        ::std::uint_least64_t transition_serial_{};

        void leave(::std::size_t index) noexcept
        {
            ::std::lock_guard lock{mutex_};
            // [slots_: fixed capacity] index belongs exclusively to this lease.
            // [safe                  ] domain destruction drains leases first.
            auto& entry{slots_[index]};
            if(entry.parked) { --parked_; }
            entry = {};
            --active_;
            changed_.notify_all();
        }
        template<typename BeforePark>
        void poll(::std::size_t index, cooperative_pause_location location, BeforePark&& before_park) noexcept
        {
            if(!requested_.load(::std::memory_order_acquire)) { return; }
            // This cold callback runs on the live participant BEFORE acquiring
            // mutex_: it may copy its native stack without reversing the host
            // controller/domain lock order. Every actual park crosses this hook.
            // Resume during capture merely cancels this attempt; re-pause still
            // parks at the same live location. It must not wait, re-enter or throw.
            ::std::forward<BeforePark>(before_park)();
            ::std::unique_lock lock{mutex_};
            if(!requested_.load(::std::memory_order_relaxed) || closed_) { return; }
            // [slots_: fixed capacity] index names the caller's live leased slot.
            // [safe                  ] no slot storage reallocation after construction.
            auto& entry{slots_[index]};
            entry.location = location;
            entry.parked = true;
            entry.externally_parked = false;
            ++parked_;
            changed_.notify_all();
            // A closed domain must not wake a saved native cohort before the
            // retained transition owner finishes arming retirement. This check
            // is only in the cold parked wait; the running acquire fast path is
            // unchanged. Reset releases the hold and publishes its prior setup.
            changed_.wait(lock, [&]
            {
                return !transition_held_ && (!requested_.load(::std::memory_order_relaxed) || closed_ || entry.release_once);
            });
            // A host-selected native step already decremented parked_ before
            // waking this one participant. Resume can race that wakeup, so
            // inspect the actual slot instead of decrementing a second time.
            if(entry.parked) { entry.parked = false; --parked_; }
            entry.release_once = false;
            // Releasing mutex_ publishes resumed state. A new request made before
            // this waiter reacquires it simply keeps this participant parked.
        }

        void release_transition(::std::uint_least64_t serial) noexcept
        {
            ::std::lock_guard lock{mutex_};
            // The move-only lease constructor is private. Domain destruction
            // drains this actual retained lease before destroying the mutex.
            if(!transition_held_ || serial != transition_serial_) { ::std::terminate(); }
            transition_held_ = false; transition_serial_ = 0u;
            // In an open domain cancellation only releases management
            // ownership: the original current pause remains requested. In a
            // closed domain this release lets already parked guests retire;
            // the owner must arm retirement before dropping its retained hold.
            changed_.notify_all();
        }

    public:
        // HOST wake subscription only. It conveys no parked-cohort or snapshot
        // permission. A live participant creates this bounded stack-owned node;
        // its callback may only wake an external wait and must NOT re-enter this
        // pause domain. The actual waiter releases its shard BEFORE polling here.
        class pause_waker final
        {
            friend class cooperative_pause_domain;
            friend class participant;
            cooperative_pause_domain* owner_{};
            pause_waker* previous_{};
            pause_waker* next_{};
            void* context_{};
            void (*wake_)(void*) noexcept{};
            pause_waker(cooperative_pause_domain* owner, void* context,
                void (*wake)(void*) noexcept) noexcept
            {
                if(owner == nullptr || wake == nullptr) { return; }
                ::std::lock_guard lock{owner->mutex_};
                if(owner->closed_ || owner->waker_count_ == owner->slots_.size()) { return; }
                // [real leased domain][this stack node][registered live head]
                // [safe] bound registrations by fixed participant capacity;
                // link only while the actual domain mutex protects lifetimes.
                owner_ = owner; context_ = context; wake_ = wake;
                next_ = owner->waker_first_;
                if(next_ != nullptr) { next_->previous_ = this; }
                owner->waker_first_ = this; ++owner->waker_count_;
                // No callback can outlive this registered stack node. Handle an
                // already pending real request BEFORE returning the subscription.
                if(owner->requested_.load(::std::memory_order_relaxed)) { wake_(context_); }
            }
        public:
            pause_waker() noexcept = default;
            pause_waker(pause_waker const&) = delete;
            pause_waker& operator=(pause_waker const&) = delete;
            pause_waker(pause_waker&&) = delete;
            pause_waker& operator=(pause_waker&&) = delete;
            [[nodiscard]] explicit operator bool() const noexcept { return owner_ != nullptr; }
            ~pause_waker()
            {
                if(owner_ == nullptr) { return; }
                auto* owner{::std::exchange(owner_, nullptr)};
                // [actual live registration pins domain drain] no client pointer
                // [safe] clear our borrow BEFORE unlink; domain cannot drain
                // until this mutex publishes the final registration count.
                ::std::lock_guard lock{owner->mutex_};
                if(previous_ != nullptr) { previous_->next_ = next_; }
                else { owner->waker_first_ = next_; }
                if(next_ != nullptr) { next_->previous_ = previous_; }
                // Every predecessor/successor remains registered under this
                // mutex. No registry link references this node after the splice.
                --owner->waker_count_; owner->changed_.notify_all();
            }
        };
    private:
        void wake_registered_waiters() noexcept
        {
            // [actual locked intrusive wake registrations] | null
            // [safe] caller owns mutex_; all nodes/contexts remain live until
            // unregister obtains this SAME mutex. No allocation/guest callback.
            auto* current{waker_first_};
            while(current != nullptr)
            {
                current->wake_(current->context_);
                current = current->next_;
                // [visited nodes][remaining registered nodes] | null
                // [safe] loop checks the successor BEFORE its next dereference.
            }
        }
    public:
        // Host-only synchronous borrow of ONE genuine externally parked slot.
        // Construction is private to the domain while its actual mutex lives;
        // neither a copied roster nor caller-provided bool can manufacture it.
        // The runtime additionally holds its publication guard until commit's
        // real backend wake finishes. This object and ALL borrowed callbacks
        // MUST remain inside that synchronous guarded invocation.
        class external_resume_borrow final
        {
            friend class cooperative_pause_domain;
            cooperative_pause_domain& owner_;
            slot& entry_;
            bool attempted_{}, committed_{};
            external_resume_borrow(cooperative_pause_domain& owner, slot& entry) noexcept
                : owner_{owner}, entry_{entry} {}
        public:
            external_resume_borrow(external_resume_borrow const&) = delete;
            external_resume_borrow& operator=(external_resume_borrow const&) = delete;
            external_resume_borrow(external_resume_borrow&&) = delete;
            external_resume_borrow& operator=(external_resume_borrow&&) = delete;
            // The internal backend must either wake exactly this real native
            // gate and return true, or return false WITHOUT waking/changing it.
            // A false wake restores the actual slot before this domain unlocks.
            // No wait for worker ACK, domain reentry, external callback or
            // allocation belongs here. Prepare bounded OS resources BEFORE it.
            template<typename Wake>
            [[nodiscard]] bool commit(Wake&& wake) noexcept
            {
                static_assert(noexcept(::std::forward<Wake>(wake)()),
                    "actual native wake must be nonthrowing and cannot outlive this guarded borrow");
                if(attempted_) { return false; }
                attempted_ = true;
                // [actual domain-owned fixed slot] ONE mutex + original ALLN
                // [safe] this slot was verified parked/external with parked_>0
                // BEFORE changing either flag/accounting; no borrowed pointer.
                entry_.parked = entry_.externally_parked = false;
                --owner_.parked_;
                if(!::std::forward<Wake>(wake)())
                {
                    entry_.parked = entry_.externally_parked = true;
                    ++owner_.parked_;
                    return false;
                }
                committed_ = true;
                owner_.changed_.notify_all();
                return true;
            }
        };
        class paused_transition
        {
            friend class cooperative_pause_domain;
            cooperative_pause_domain* owner_{};
            ::std::uint_least64_t serial_{};
            paused_transition(cooperative_pause_domain& owner, ::std::uint_least64_t serial) noexcept
                : owner_{::std::addressof(owner)}, serial_{serial} {}
        public:
            paused_transition() noexcept = default;
            paused_transition(paused_transition const&) = delete;
            paused_transition& operator=(paused_transition const&) = delete;
            paused_transition(paused_transition&& other) noexcept
                : owner_{::std::exchange(other.owner_, nullptr)}, serial_{::std::exchange(other.serial_, 0u)} {}
            paused_transition& operator=(paused_transition&& other) noexcept
            {
                if(this != ::std::addressof(other))
                {
                    reset();
                    owner_ = ::std::exchange(other.owner_, nullptr);
                    serial_ = ::std::exchange(other.serial_, 0u);
                }
                return *this;
            }
            ~paused_transition() { reset(); }
            [[nodiscard]] explicit operator bool() const noexcept { return owner_ != nullptr; }
            void reset() noexcept
            {
                if(owner_ != nullptr)
                {
                    // [real domain retained by its own drain contract]
                    // [safe] clear borrowing pointer/serial BEFORE dropping the
                    // actual transition lease; waiting destruction may proceed.
                    auto* owner{::std::exchange(owner_, nullptr)};
                    auto const serial{::std::exchange(serial_, 0u)};
                    owner->release_transition(serial);
                    // No domain access after release_transition.
                }
            }
        };
        class pause_ticket
        {
            friend class cooperative_pause_domain;
            ::std::shared_ptr<void const> identity_{};
            ::std::uint_least64_t serial_{};
            pause_ticket(::std::shared_ptr<void const> identity, ::std::uint_least64_t serial) noexcept
                : identity_{::std::move(identity)}, serial_{serial} {}
        public:
            pause_ticket() noexcept = default;
            [[nodiscard]] explicit operator bool() const noexcept { return identity_ != nullptr; }
        };
        class participant
        {
            friend class cooperative_pause_domain;
            cooperative_pause_domain* owner_{};
            ::std::size_t index_{};
            participant(cooperative_pause_domain& owner, ::std::size_t index) noexcept
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
                    owner_ = ::std::exchange(other.owner_, nullptr);
                    index_ = other.index_;
                }
                return *this;
            }
            ~participant() { reset(); }
            [[nodiscard]] explicit operator bool() const noexcept { return owner_ != nullptr; }
            [[nodiscard]] ::std::uint_least64_t identifier() const noexcept
            {
                // [leased slot: fixed allocation] exclusively retained by this participant.
                // Its id is assigned before admission and never changes until reset releases it.
                return owner_ == nullptr ? 0u : owner_->slots_[index_].id;
            }
            // One native thread owns/uses this participant at a time. A nested
            // callback must reuse the outer participant, not create a second one.
            void poll(cooperative_pause_location location) const noexcept
            { if(owner_ != nullptr) { owner_->poll(index_, location, []() noexcept {}); } }
            template<typename BeforePark>
            void poll(cooperative_pause_location location, BeforePark&& before_park) const noexcept
            { if(owner_ != nullptr) { owner_->poll(index_, location, ::std::forward<BeforePark>(before_park)); } }
            // Register BEFORE acquiring an external wait-shard lock, destroy
            // AFTER releasing it. Actual pause requests take domain -> shard;
            // polling never takes shard -> domain. Result is wake DATA only.
            [[nodiscard]] pause_waker register_pause_waker(void* context, void (*wake)(void*) noexcept) const noexcept
            { return pause_waker{owner_, context, wake}; }
            void reset() noexcept
            {
                if(owner_ != nullptr)
                {
                    // [live domain] drain keeps it alive until leave releases this lease.
                    // ^^ owner: exchange clears the borrowing pointer before publication.
                    auto* owner{::std::exchange(owner_, nullptr)};
                    owner->leave(index_);
                    // No access to owner after leave: its waiting destructor may proceed.
                }
            }
        };
        struct stopped_participant
        {
            ::std::uint_least64_t id{};
            cooperative_pause_location location{};
        };
        struct snapshot
        {
            cooperative_pause_result result{cooperative_pause_result::stale_ticket};
            ::std::vector<stopped_participant> participants{};
        };

    private:
        [[nodiscard]] bool current(pause_ticket const& ticket) const noexcept
        {
            return ticket.identity_.get() == identity_.get() && ticket.serial_ == serial_ &&
                   requested_.load(::std::memory_order_relaxed);
        }

    public:
        // Fixed logical native payload only; excludes allocator/control-block
        // overhead. A quota caller can reject overflow BEFORE constructing slots.
        [[nodiscard]] static constexpr ::std::size_t fixed_native_payload_bytes(::std::size_t max_concurrent) noexcept
        {
            constexpr auto fixed{sizeof(cooperative_pause_domain)+sizeof(unsigned char)};
            return max_concurrent>(SIZE_MAX-fixed)/sizeof(slot) ? SIZE_MAX : fixed+max_concurrent*sizeof(slot);
        }
        explicit cooperative_pause_domain(::std::size_t max_concurrent) : slots_(max_concurrent) {}
        cooperative_pause_domain(cooperative_pause_domain const&) = delete;
        cooperative_pause_domain& operator=(cooperative_pause_domain const&) = delete;
        // The owner stops new callers before destruction and must not hold its
        // own participant or transition while draining. close() releases
        // waiters only after any retained transition is dropped; it does not
        // terminate guest execution or permit resources to be freed before drain.
        ~cooperative_pause_domain() { close(); drain(); }

        [[nodiscard]] participant enter()
        {
            ::std::unique_lock lock{mutex_};
            // Admission stays closed throughout a pause, including a zero-thread
            // pause. Never report all parked and then admit an unparked execution.
            ++admission_waiters_;
            changed_.wait(lock, [&] { return !requested_.load(::std::memory_order_relaxed) || closed_; });
            --admission_waiters_;
            changed_.notify_all();
            if(closed_ || active_ == slots_.size() || next_participant_ == 0) { return {}; }
            for(::std::size_t index{}; index != slots_.size(); ++index)
            {
                auto& entry{slots_[index]}; // index is strictly inside the fixed slot vector.
                if(entry.id != 0) { continue; }
                entry.id = next_participant_++;
                ++active_;
                return participant{*this, index};
            }
            return {};
        }
        // Native participant ownership comparison, never a parked-cohort or
        // execution capability. The caller retains the actual participant.
        [[nodiscard]] bool owns_current_participant(participant const& actual)
        {
            ::std::lock_guard lock{mutex_};
            return actual.owner_==this && actual.index_<slots_.size() &&
                slots_[actual.index_].id!=0u && !closed_;
        }
        [[nodiscard]] pause_ticket request_pause()
        {
            ::std::lock_guard lock{mutex_};
            // No wrapping ticket/participant identifiers: an old management
            // request must never acquire authority over a later pause or thread.
            if(closed_ || requested_.load(::std::memory_order_relaxed) ||
               serial_ == ::std::numeric_limits<::std::uint_least64_t>::max()) { return {}; }
            ++serial_;
            requested_.store(true, ::std::memory_order_release);
            wake_registered_waiters(); // Cold real request, ordinary poll adds no read.
            return pause_ticket{identity_, serial_};
        }
        [[nodiscard]] cooperative_pause_result wait_until_paused(pause_ticket const& ticket,
            ::std::chrono::steady_clock::time_point deadline)
        {
            ::std::unique_lock lock{mutex_};
            auto ready{[&] { return closed_ || !current(ticket) || parked_ == active_; }};
            if(!changed_.wait_until(lock, deadline, ready)) { return cooperative_pause_result::timeout; }
            if(closed_) { return cooperative_pause_result::closed; }
            if(!current(ticket)) { return cooperative_pause_result::stale_ticket; }
            return cooperative_pause_result::paused;
        }
        [[nodiscard]] snapshot capture(pause_ticket const& ticket)
        {
            ::std::lock_guard lock{mutex_};
            if(closed_) { return {cooperative_pause_result::closed, {}}; }
            if(!current(ticket)) { return {}; }
            if(parked_ != active_) { return {cooperative_pause_result::timeout, {}}; }
            snapshot result{cooperative_pause_result::paused, {}};
            result.participants.reserve(active_);
            for(auto const& entry : slots_)
            { if(entry.id != 0) { result.participants.push_back({entry.id, entry.location}); } }
            return result;
        }
        // Host-only native-step transfer. The caller must first authenticate
        // the selected JIT return PC and native thread. Only one participant
        // leaves the cooperative wait; every other guest remains stopped.
        [[nodiscard]] bool release_one_for_native_step(pause_ticket const& ticket, ::std::uint_least64_t id)
        {
            ::std::lock_guard lock{mutex_};
            if(closed_ || transition_held_ || !current(ticket) || parked_ != active_ || id == 0u) { return false; }
            for(auto& entry : slots_)
            {
                if(entry.id != id) { continue; }
                if(!entry.parked || entry.externally_parked || entry.release_once) { return false; }
                entry.parked = false;
                entry.release_once = true;
                --parked_;
                changed_.notify_all();
                return true;
            }
            return false;
        }
        // Called by the host manager only AFTER observing the target's
        // authenticated SIGTRAP park. The signal handler never enters here.
        [[nodiscard]] bool external_park(pause_ticket const& ticket, ::std::uint_least64_t id,
                                         cooperative_pause_location location)
        {
            ::std::lock_guard lock{mutex_};
            if(closed_ || transition_held_ || !current(ticket) || id == 0u) { return false; }
            for(auto& entry : slots_)
            {
                if(entry.id != id) { continue; }
                if(entry.parked || entry.externally_parked || entry.release_once) { return false; }
                entry.location = location;
                entry.parked = entry.externally_parked = true;
                ++parked_;
                changed_.notify_all();
                return true;
            }
            return false;
        }
        // Called before waking the signal gate for the next native instruction.
        // The target is no longer counted as parked while that instruction runs.
        [[nodiscard]] bool external_unpark(pause_ticket const& ticket, ::std::uint_least64_t id)
        {
            ::std::lock_guard lock{mutex_};
            if(closed_ || transition_held_ || !current(ticket) || id == 0u) { return false; }
            for(auto& entry : slots_)
            {
                if(entry.id != id) { continue; }
                if(!entry.parked || !entry.externally_parked) { return false; }
                entry.parked = entry.externally_parked = false;
                --parked_;
                changed_.notify_all();
                return true;
            }
            return false;
        }
        // A bounded publication operation can hold the stopped set stable against
        // concurrent resume/close. Never compile, wait for executions, or call back
        // into this domain from commit. Throwing unlocks without resuming threads.
        template<typename Commit>
        [[nodiscard]] bool while_stopped(pause_ticket const& ticket, Commit&& commit)
        {
            ::std::lock_guard lock{mutex_};
            if(closed_ || transition_held_ || !current(ticket) || parked_ != active_) { return false; }
            ::std::forward<Commit>(commit)();
            return true;
        }
        // One authenticated stopped participant, supplied directly from this
        // domain's slot. The callback cannot accept a request-provided PC, and
        // an externally parked native trap never reuses its previous Wasm PC.
        // Same bounded/no-reentry contract as while_stopped(); ONE domain lock.
        template<typename Inspect>
        [[nodiscard]] bool with_stopped_participant(pause_ticket const& ticket,
            ::std::uint_least64_t id, Inspect&& inspect)
        {
            ::std::lock_guard lock{mutex_};
            if(closed_ || !current(ticket) || parked_ != active_ || id == 0u) { return false; }
            for(auto const& entry : slots_)
            {
                if(entry.id != id) { continue; }
                if(!entry.parked || entry.externally_parked || entry.release_once) { return false; }
                // [domain-owned fixed participant slots] end
                // [safe                                ] entry remains parked
                //  ^^ copy only this authenticated location while the lock lives.
                ::std::forward<Inspect>(inspect)(entry.location);
                return true;
            }
            return false;
        }
        // Cold checkpoint roster guard: display/native-step parks do not prove
        // a materialized Wasm continuation. This helper supplies only actual
        // cooperative slots and a current-ticket comparison during ONE mutex
        // lifetime; neither scratch data nor a true result authorizes checkpoint
        // files, roots, restore or publication. The private runtime manager must
        // additionally authenticate each real before-park capture/owner and its
        // complete host/code/resource census. Empty cohorts are not admitted.
        // Inspect is bounded internal code: no compile, wait, external observer,
        // guest entry or domain reentry. It MUST NOT retain either borrow or the
        // ticket predicate beyond this synchronous callback.
    private:
        template<typename Inspect>
        [[nodiscard]] bool with_cooperatively_stopped_cohort_impl(pause_ticket const& ticket,
            ::std::span<stopped_participant> scratch, paused_transition* transfer, Inspect&& inspect)
        {
            ::std::lock_guard lock{mutex_};
            // Both cohort interfaces may run internal mutators. A retained
            // transition rejects every competing callback before any scratch
            // copy, including the interface without a transfer output.
            if(closed_ || transition_held_ || !current(ticket) || parked_ != active_ || active_ == 0u ||
               scratch.size() < active_ || (transfer != nullptr && *transfer)) { return false; }
            ::std::size_t observed{};
            // Validate the WHOLE roster before copying even one scratch cell.
            // In particular external_park still satisfies parked_==active_, but
            // it cannot supply a logical frame or reuse its previous Wasm PC.
            for(auto const& entry : slots_)
            {
                if(entry.id == 0u) { continue; }
                if(!entry.parked || entry.externally_parked || entry.release_once || observed == active_)
                { return false; }
                ++observed; // observed<=active_; an extra slot declines before overflow.
            }
            if(observed != active_) { return false; }
            ::std::size_t index{};
            for(auto const& entry : slots_)
            {
                if(entry.id == 0u) { continue; }
                // [caller-owned scratch ... active_<=scratch.size()] end
                // [safe                                           ] index<N
                // by the complete roster count above; no native address copied.
                scratch[index] = {entry.id, entry.location};
                ++index; // Next cell; index<=observed==active_ remains one-past safe.
            }
            // [actual scratch begin ... observed<=scratch.size() ... scratch end]
            // [safe prefix                             ] unused scratch suffix
            //  ^^ readonly view begins at the same caller-owned allocation. Its
            // end<=scratch.end(); no pointer advances or native addresses escape.
            // The view and ticket predicate live ONLY through Inspect while this
            // mutex owns the actual episode; never store/return either borrow.
            ::std::span<stopped_participant const> const participants{scratch.data(), observed};
            auto const matches_current_ticket{[this](pause_ticket const& captured) noexcept
            {
                // Comparison-only domain borrow. The caller cannot construct a
                // pause_ticket from a serial/id, and this predicate cannot outlive
                // the ONE guarded callback or be called recursively via domain API.
                return current(captured);
            }};
            if(!::std::forward<Inspect>(inspect)(participants, matches_current_ticket)) { return false; }
            if(transfer != nullptr)
            {
                // Still ONE current cohort lock: no resume/native-step can
                // intervene after successful actual resource preflight and
                // before sealing this move-only management transition.
                if(*transfer) { return false; } // Callback must not replace its output lease.
                transition_held_ = true; transition_serial_ = serial_;
                // [empty manager-owned output] this private constructor's exact
                // fields are initialized directly; no reset/reentry under mutex.
                transfer->owner_ = this; transfer->serial_ = serial_;
            }
            return true; // Roster or transition ownership, never VM restore authority.
        }
    public:
        template<typename Inspect>
        [[nodiscard]] bool with_cooperatively_stopped_cohort(pause_ticket const& ticket,
            ::std::span<stopped_participant> scratch, Inspect&& inspect)
        {
            return with_cooperatively_stopped_cohort_impl(ticket, scratch, nullptr,
                [&](auto participants, auto const& matches_current_ticket)
                {
                    ::std::forward<Inspect>(inspect)(participants, matches_current_ticket);
                    return true;
                });
        }
        // Host-only transaction transfer. Inspect performs bounded synchronous
        // preflight under the real complete cooperative roster and returns true
        // only after it is ready to take over this pause. All its borrows expire
        // on return; the move-only output retains only the actual pause episode.
        // It grants no roots, publication, code pointers, checkpoint/file or
        // execution permission. The domain owner MUST release its own transition
        // before calling drain/destruction, exactly as for a participant lease.
        // A cancelled transfer leaves the original pause requested.
        template<typename Inspect>
        [[nodiscard]] bool transfer_cooperatively_stopped_cohort(pause_ticket const& ticket,
            ::std::span<stopped_participant> scratch, paused_transition& transfer, Inspect&& inspect)
        {
            return with_cooperatively_stopped_cohort_impl(ticket, scratch, ::std::addressof(transfer),
                ::std::forward<Inspect>(inspect));
        }
        // Resume only the actual move-only transition produced by this domain.
        // Run AFTER all cohort/publication/root locks and retirement poll setup
        // have left. Wrong-domain/empty/stale leases do not mutate either pause.
        [[nodiscard]] bool resume_transition(paused_transition& transfer)
        {
            ::std::lock_guard lock{mutex_};
            if(closed_ || transfer.owner_ != this || !transition_held_ ||
               transfer.serial_ != serial_ || transition_serial_ != serial_ ||
               !requested_.load(::std::memory_order_relaxed) || parked_ != active_)
            { return false; }
            // The original complete roster excludes externally parked native
            // frames; management mutators were blocked throughout the transfer.
            for(auto const& entry : slots_)
            {
                if(entry.id != 0u && (!entry.parked || entry.externally_parked || entry.release_once)) { return false; }
            }
            // [live domain-owned management lease] no guest/native pointer
            // [safe] clear its borrowing owner BEFORE publishing resumed state.
            transfer.owner_ = nullptr; transfer.serial_ = 0u;
            transition_held_ = false; transition_serial_ = 0u;
            requested_.store(false, ::std::memory_order_release);
            changed_.notify_all();
            return true;
        }
        // ONE domain transaction for an actual externally parked native trap.
        // Runtime Inspect authenticates its opaque capture/cursor/current caller
        // and callee under publication, then finishes native snapshot inspection
        // BEFORE taking a second native wake guard. It retains publication until
        // the borrowed commit drops real park accounting and opens that gate.
        // Independent ALLN host mutators cannot publish between proof and wake.
        // Inspect is synchronous internal host code: no compilation, observer,
        // waiting for an execution/ACK, domain reentry, or retained borrow. It
        // may throw ONLY before a successful commit. A real native session owner
        // still retires the event/provider/capabilities after worker ACK outside
        // both locks; this accounting borrow provides no execution authority.
        template<typename Inspect>
        [[nodiscard]] bool with_externally_parked_participant_for_resume(pause_ticket const& ticket,
            ::std::uint_least64_t id, Inspect&& inspect)
        {
            ::std::lock_guard lock{mutex_};
            if(closed_ || transition_held_ || !current(ticket) || parked_ != active_ ||
               parked_ == 0u || id == 0u) { return false; }
            for(auto& entry : slots_)
            {
                if(entry.id != id) { continue; }
                if(!entry.parked || !entry.externally_parked || entry.release_once) { return false; }
                // [actual fixed domain slot] still under this ONE domain mutex
                // [safe] validate the genuine external park BEFORE constructing
                // the noncopyable synchronous borrow; no caller pointer accepted.
                external_resume_borrow resume{*this, entry};
                ::std::forward<Inspect>(inspect)(entry.location, resume);
                return resume.committed_; // Actual backend wake; never Inspect's return bool.
            }
            return false;
        }
        // Cold native-code inspection may admit an external park only AFTER
        // the runtime validates its private capture and REAL active trap session.
        // The external flag is an actual slot fact, not caller-provided authority.
        // Source/locals keep using with_stopped_participant(), which rejects it.
        // ONE domain lock; callback is bounded and must not re-enter this domain.
        template<typename Inspect>
        [[nodiscard]] bool with_parked_participant(pause_ticket const& ticket,
            ::std::uint_least64_t id, Inspect&& inspect)
        {
            ::std::lock_guard lock{mutex_};
            if(closed_ || !current(ticket) || parked_ != active_ || id == 0u) { return false; }
            for(auto const& entry : slots_)
            {
                if(entry.id != id) { continue; }
                if(!entry.parked || entry.release_once) { return false; }
                // [domain-owned fixed participant slots] end
                // [safe                                ] actual slot stays parked;
                //  ^^ copy location/park kind, never a requested native address.
                ::std::forward<Inspect>(inspect)(entry.location, entry.externally_parked);
                return true;
            }
            return false;
        }
        [[nodiscard]] bool resume(pause_ticket const& ticket)
        {
            ::std::lock_guard lock{mutex_};
            if(closed_ || transition_held_ || !current(ticket)) { return false; }
            // A native-step SIGTRAP can outlive this ticket until its host
            // manager opens the signal gate. Never carry that external park
            // into a later pause and report a running guest as stopped.
            for(auto& entry : slots_)
            {
                if(!entry.externally_parked) { continue; }
                entry.externally_parked = entry.parked = false;
                --parked_;
            }
            requested_.store(false, ::std::memory_order_release);
            changed_.notify_all();
            return true;
        }
        // Host observer reads only: a request is not proof that all executions
        // have parked. The manager still needs wait_until_paused/capture.
        [[nodiscard]] bool pause_requested() const noexcept
        { return requested_.load(::std::memory_order_acquire); }
        // Durable external-wait wake hint. This is not parked-cohort authority.
        // A wait shard must never take the pause-domain mutex: close wakes in
        // domain -> shard order. Publish close before invoking registered wakes.
        [[nodiscard]] bool wait_interrupt_requested() const noexcept
        { return requested_.load(::std::memory_order_acquire) || closed_.load(::std::memory_order_acquire); }
        [[nodiscard]] bool is_closed()
        {
            ::std::lock_guard lock{mutex_};
            return closed_;
        }
        // Cold lifetime observation only. A client scalar is not a
        // checkpoint credential; the private dispatcher must first
        // authenticate the captured owner/control block. IDs are never
        // reused, so actual lease retirement cannot be confused with a
        // newly enrolled participant in the same storage slot.
        [[nodiscard]] bool previous_participant_retired(::std::uint_least64_t known_id)
        {
            ::std::lock_guard lock{mutex_};
            if(closed_ || known_id == 0u || known_id >= next_participant_) { return false; }
            for(auto const& entry : slots_) { if(entry.id == known_id) { return false; } }
            return true;
        }
        void close()
        {
            ::std::lock_guard lock{mutex_};
            closed_.store(true, ::std::memory_order_release);
            requested_.store(false, ::std::memory_order_release);
            wake_registered_waiters(); // Wake subscriptions; actual owner cancels execution separately.
            changed_.notify_all();
        }
        void drain()
        {
            ::std::unique_lock lock{mutex_};
            changed_.wait(lock, [&] { return active_ == 0uz && admission_waiters_ == 0uz && waker_count_ == 0uz && !transition_held_; });
        }
    };
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif

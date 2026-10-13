/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <chrono>
# include <cstddef>
# include <memory>
# include <type_traits>
# include <utility>
# include <uwvm2/utils/macro/push_macros.h>
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#  include <mutex>
# endif
# include "execution_lifetime.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::utils::thread
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // A reusable owner of execution generations. One lease covers an entire host
    // entry, including nested callbacks; inner calls reuse that admission. Closing
    // rejects new entries, cooperatively cancels admitted work and drains it before
    // the reset callback may destroy borrowed resources. The next generation gets
    // a fresh cancellation source: delayed cancellation of an old generation cannot
    // cancel a replacement generation.
    //
    // Administrative operations must not be called while the calling thread owns
    // a lease. reset_resources must not recursively reset/drain this domain. Stop
    // callbacks may query admission or request_stop (no domain lock is held while
    // invoking them), but must not synchronously reset/drain their own generation.
    // As with any synchronization object, stop calling before destroying the owner.
    class execution_domain
    {
        ::std::mutex admission_mutex{};
        ::std::mutex maintenance_mutex{};
        ::std::size_t const capacity;
        ::std::shared_ptr<execution_lifetime> current;
        bool accepting{true};

        [[nodiscard]] ::std::shared_ptr<execution_lifetime> close_admission()
        {
            ::std::lock_guard lock{admission_mutex};
            accepting = false;
            // Copy ownership before dropping the lock: a concurrent reset may
            // retire this generation while a stop callback is still returning.
            return current;
        }

    public:
        using lease = execution_lifetime::lease;
        class maintenance_transition
        {
            friend class execution_domain;
            execution_domain* owner_{};
            ::std::unique_lock<::std::mutex> lock_{};
            maintenance_transition(execution_domain& actual)
                : owner_{::std::addressof(actual)}, lock_{actual.maintenance_mutex, ::std::try_to_lock}
            { if(!lock_.owns_lock()) { owner_ = nullptr; } }
        public:
            maintenance_transition() noexcept = default;
            maintenance_transition(maintenance_transition const&) = delete;
            maintenance_transition& operator=(maintenance_transition const&) = delete;
            maintenance_transition(maintenance_transition&& other) noexcept
                : owner_{::std::exchange(other.owner_, nullptr)}, lock_{::std::move(other.lock_)} {}
            maintenance_transition& operator=(maintenance_transition&& other) noexcept
            {
                if(this != ::std::addressof(other))
                {
                    lock_ = ::std::move(other.lock_);
                    owner_ = ::std::exchange(other.owner_, nullptr);
                }
                return *this;
            }
            [[nodiscard]] explicit operator bool() const noexcept { return owner_ != nullptr && lock_.owns_lock(); }
        };
        // Generic host-only lifecycle ownership, never Wasm restore permission.
        // Take BEFORE execution admission/cohort/publication. No wait and no
        // ordinary entry fast-path branch is added. Owner must outlive token.
        [[nodiscard]] maintenance_transition try_maintenance_transition()
        { return maintenance_transition{*this}; }
        explicit execution_domain(::std::size_t max_concurrent)
            : capacity{max_concurrent}, current{::std::make_shared<execution_lifetime>(max_concurrent)} {}
        execution_domain(execution_domain const&) = delete;
        execution_domain& operator=(execution_domain const&) = delete;
        ~execution_domain() { stop_and_drain(); }

        [[nodiscard]] lease try_enter()
        {
            ::std::lock_guard lock{admission_mutex};
            // Admission and retirement share this lock. The domain retains the
            // current generation until all returned leases have left; there is no
            // shared_ptr increment or allocation per execution entry.
            if(!accepting) { return {}; }
            return current->try_enter();
        }

        // Cold configuration only; no wait, cancellation, guest entry or
        // runtime resource retirement. Serializes with REAL reset/stop first:
        // maintenance -> admission -> short lifetime(active==0) -> publication.
        // Neither ordinary try_enter nor generated guest IR gains a new branch.
        // Callback must be the bounded INTERNAL setter: no compile, observer,
        // try_enter, reset, drain, pause/domain reentry or external destruction.
        template<typename Configure>
        [[nodiscard]] bool with_quiescent_configuration(Configure&& configure)
        {
            ::std::unique_lock maintenance{maintenance_mutex, ::std::try_to_lock};
            if(!maintenance.owns_lock()) { return false; }
            ::std::lock_guard admission{admission_mutex};
            if(!accepting || !current->quiescent_configuration_ready()) { return false; }
            // [actual privately retained current lifetime] end
            // [safe] admission excludes new try_enter while the callback lives;
            // current cannot be externally entered or changed outside this owner.
            ::std::forward<Configure>(configure)();
            return true;
        }

        // The caller retains THIS real maintenance mutex throughout timeout.
        // No reset/reopen can invalidate the lifetime while its work is pending.
        // Caller must not own an execution lease or invoke a provider callback.
        [[nodiscard]] bool drain_owned_transition_until(maintenance_transition const& transition,
            ::std::chrono::steady_clock::time_point deadline)
        {
            if(transition.owner_ != this || !transition.lock_.owns_lock()) { return false; }
            ::std::shared_ptr<execution_lifetime> generation{};
            {
                ::std::lock_guard admission{admission_mutex};
                if(accepting || !current->stop_requested()) { return false; }
                generation = current;
            }
            // [real shared lifetime owned while actual maintenance stays held]
            // [safe] no copied count/ACK or unrelated transition authorizes drain.
            return generation->drain_until(deadline);
        }

        // Trusted bounded-host policy only, using THIS real maintenance owner.
        // It closes admission and changes actual lease.stop_requested(), but
        // does NOT call std::stop_source callbacks or pretend blocked services
        // woke. Those genuine calls remain admitted/pending until they return.
        [[nodiscard]] bool request_stop_without_callbacks_owned_transition(maintenance_transition const& transition)
        {
            if(transition.owner_ != this || !transition.lock_.owns_lock()) { return false; }
            auto generation{close_admission()};
            generation->mark_stopping_without_callbacks();
            return true;
        }

        void request_stop()
        {
            auto generation{close_admission()};
            generation->request_stop();
        }

        // Keep administrative ownership through dependent resource shutdown.
        // A concurrent reset cannot publish a fresh generation between draining
        // executions and stopping their compiler/cache producers. No admission
        // lock is held while invoking the callback. It must not reset/drain this
        // same domain; failure leaves admission closed for an explicit reset.
        template<typename StopResources>
        void stop_and_drain(StopResources&& stop_resources)
        {
            ::std::unique_lock maintenance{maintenance_mutex};
            auto generation{close_admission()};
            generation->request_stop();
            generation->drain();
            ::std::forward<StopResources>(stop_resources)();
        }

        void stop_and_drain() { stop_and_drain([] {}); }

        // Only a genuine token retaining THIS domain's actual maintenance
        // mutex can drain/reset here. Caller must first drop its own execution
        // lease and every cohort/publication/root borrow. Never invoke reset or
        // stop_and_drain from the resource callback; they share this mutex.
        template<typename ResetResources>
        [[nodiscard]] bool reset_owned_transition(maintenance_transition& transition, ResetResources&& reset_resources)
        {
            if(transition.owner_ != this || !transition.lock_.owns_lock()) { return false; }
            auto generation{close_admission()};
            generation->request_stop(); generation->drain();
            ::std::forward<ResetResources>(reset_resources)();
            auto next{::std::make_shared<execution_lifetime>(capacity)};
            {
                ::std::lock_guard admission{admission_mutex};
                current = ::std::move(next); accepting = true;
            }
            return true;
        }

        // Cold generic lifecycle DATA, preallocated while THIS actual
        // maintenance transition owns its mutex. It exposes no lifetime/lease
        // constructor and cannot be entered before actual drained publication.
        class prepared_generation
        {
            friend class execution_domain;
            execution_domain* owner_{};
            ::std::shared_ptr<execution_lifetime> observed_{}, replacement_{};
            enum class phase : unsigned char { empty, prepared, published_closed, opened };
            phase phase_{phase::empty};
        public:
            prepared_generation() noexcept = default;
            prepared_generation(prepared_generation const&)=delete;
            prepared_generation& operator=(prepared_generation const&)=delete;
            prepared_generation(prepared_generation&& other) noexcept
                :owner_{::std::exchange(other.owner_,nullptr)},observed_{::std::move(other.observed_)},replacement_{::std::move(other.replacement_)},phase_{::std::exchange(other.phase_,phase::empty)} {}
            prepared_generation& operator=(prepared_generation&& other) noexcept
            {
                if(this!=::std::addressof(other))
                { owner_=::std::exchange(other.owner_,nullptr);observed_=::std::move(other.observed_);replacement_=::std::move(other.replacement_);phase_=::std::exchange(other.phase_,phase::empty); }
                return *this;
            }
            [[nodiscard]] explicit operator bool() const noexcept
            { return owner_!=nullptr && observed_ && phase_!=phase::empty && (phase_!=phase::prepared || replacement_); }
        };
        // A noncopyable LEXICAL receipt of this actual admitted lifetime's
        // successful physical scope drain. Only the domain constructs it after
        // drain_until and closed-admission recheck. It supplies no Wasm/source/
        // GC/host/OS-thread permission; those are independently proven by the
        // real runtime transaction. Never retain its address past the callback.
        class drained_generation
        {
            friend class execution_domain;
            execution_domain const* owner_{};
            maintenance_transition const* transition_{};
            ::std::shared_ptr<execution_lifetime> actual_;
            drained_generation(execution_domain const& owner,maintenance_transition const& transition,
                ::std::shared_ptr<execution_lifetime> const& actual) noexcept
                :owner_{::std::addressof(owner)},transition_{::std::addressof(transition)},actual_{actual} {}
        public:
            drained_generation() noexcept=default; // Empty conveys no actual drain.
            drained_generation(drained_generation const&)=delete;
            drained_generation& operator=(drained_generation const&)=delete;
            drained_generation(drained_generation&& other) noexcept
                :owner_{::std::exchange(other.owner_,nullptr)},transition_{::std::exchange(other.transition_,nullptr)},actual_{::std::move(other.actual_)} {}
            drained_generation& operator=(drained_generation&& other) noexcept
            {
                if(this!=::std::addressof(other))
                { owner_=::std::exchange(other.owner_,nullptr);transition_=::std::exchange(other.transition_,nullptr);actual_=::std::move(other.actual_); }
                return *this;
            }
            [[nodiscard]] explicit operator bool() const noexcept { return owner_!=nullptr && transition_!=nullptr && actual_; }
        };
        enum class prepared_reset_result : unsigned char { invalid_owner, pending_execution, publication_declined, published_closed };
        [[nodiscard]] prepared_generation prepare_replacement_generation(maintenance_transition const& transition)
        {
            prepared_generation prepared{};
            if(transition.owner_!=this || !transition.lock_.owns_lock()) { return prepared; }
            {
                ::std::lock_guard admission{admission_mutex};prepared.observed_=current;
            }
            // ALL potentially throwing next-lifetime allocation precedes the
            // resource publication callback. Old actual resources remain live
            // and untouched on failure. No new participant can enter this owner.
            prepared.replacement_=::std::make_shared<execution_lifetime>(capacity);
            prepared.owner_=this;prepared.phase_=prepared_generation::phase::prepared;return prepared;
        }
        enum class prepared_drain_status : unsigned char { invalid_owner, pending_execution, drained };
        struct prepared_drain_result
        {
            prepared_drain_status status{prepared_drain_status::invalid_owner};
            drained_generation actual{}; // Status DATA alone never authorizes mutation.
        };
        [[nodiscard]] prepared_drain_result drain_prepared_generation_until(maintenance_transition const& transition,
            prepared_generation const& prepared,::std::chrono::steady_clock::time_point deadline)
        {
            if(transition.owner_!=this || !transition.lock_.owns_lock() || prepared.owner_!=this ||
               prepared.phase_!=prepared_generation::phase::prepared || !prepared.observed_ || !prepared.replacement_ ||
               prepared.observed_.get()==prepared.replacement_.get()) { return {}; }
            auto generation{prepared.observed_};
            {
                ::std::lock_guard admission{admission_mutex};
                if(current.get()!=generation.get() || current.owner_before(generation) || generation.owner_before(current)) { return {}; }
                accepting=false;generation->mark_stopping_without_callbacks();
            }
            if(!generation->drain_until(deadline)) { return {prepared_drain_status::pending_execution,{}}; }
            {
                ::std::lock_guard admission{admission_mutex};
                if(accepting || current.get()!=generation.get() || current.owner_before(generation) || generation.owner_before(current) ||
                   !generation->stop_requested()) { return {}; }
            }
            // Actual maintenance pins the SAME closed current lifetime after
            // its true leases reached zero. New leases cannot revive it. This
            // move-only owner permits cold fallible preparation WITHOUT holding
            // admission_mutex, while old resources stay retained. It is not an
            // OS join, GC/N, source/host or Wasm publication permission.
            return {prepared_drain_status::drained,drained_generation{*this,transition,generation}};
        }
        [[nodiscard]] bool current_prepared_drain(maintenance_transition const& transition,
            prepared_generation const& prepared,drained_generation const& actual)
        {
            if(transition.owner_!=this || !transition.lock_.owns_lock() || prepared.owner_!=this ||
               prepared.phase_!=prepared_generation::phase::prepared || !prepared.observed_ || !prepared.replacement_ ||
               actual.owner_!=this || actual.transition_!=::std::addressof(transition) || !actual.actual_ ||
               actual.actual_.get()!=prepared.observed_.get() || actual.actual_.owner_before(prepared.observed_) ||
               prepared.observed_.owner_before(actual.actual_)) { return false; }
            ::std::lock_guard admission{admission_mutex};
            return !accepting && current.get()==actual.actual_.get() && !current.owner_before(actual.actual_) &&
                !actual.actual_.owner_before(current) && current->stop_requested();
        }
        template<typename PublishResources>
        [[nodiscard]] prepared_reset_result publish_prepared_drained_reset(maintenance_transition const& transition,
            prepared_generation& prepared,drained_generation const& actual,PublishResources&& publish_resources)
        {
            static_assert(::std::is_nothrow_invocable_r_v<bool,PublishResources&,drained_generation const&>,
                "The actual drained publication callback must be noexcept and keep resources unchanged when returning false.");
            if(transition.owner_!=this || !transition.lock_.owns_lock() || prepared.owner_!=this ||
               prepared.phase_!=prepared_generation::phase::prepared || !prepared.observed_ || !prepared.replacement_ ||
               actual.owner_!=this || actual.transition_!=::std::addressof(transition) || !actual.actual_ ||
               actual.actual_.get()!=prepared.observed_.get() || actual.actual_.owner_before(prepared.observed_) ||
               prepared.observed_.owner_before(actual.actual_)) { return prepared_reset_result::invalid_owner; }
            // The allocation-capable staging interval used this real drain owner
            // under maintenance+closed admission. Take admission BEFORE actual
            // mutation and requalify the exact strong current-generation tuple.
            // Callback is finite/noexcept/noalloc with no same-domain reentry,
            // provider destruction/OS join/wait: maintenance -> admission -> N/pub.
            ::std::lock_guard admission{admission_mutex};
            if(accepting || current.get()!=actual.actual_.get() || current.owner_before(actual.actual_) ||
               actual.actual_.owner_before(current) || !current->stop_requested() ||
               !prepared.replacement_->quiescent_configuration_ready()) { return prepared_reset_result::invalid_owner; }
            if(!::std::forward<PublishResources>(publish_resources)(actual)) { return prepared_reset_result::publication_declined; }
            // No allocation/additional mutex/callback after mutation. Normal
            // admission remains closed until ALL actual restored workers root.
            current=::std::move(prepared.replacement_);prepared.observed_=current;
            prepared.phase_=prepared_generation::phase::published_closed;
            return prepared_reset_result::published_closed;
        }
        template<typename PublishResources>
        [[nodiscard]] prepared_reset_result publish_prepared_drained_reset(maintenance_transition const& transition,
            prepared_generation& prepared,::std::chrono::steady_clock::time_point deadline,PublishResources&& publish_resources)
        {
            auto drained{drain_prepared_generation_until(transition,prepared,deadline)};
            if(drained.status==prepared_drain_status::invalid_owner) { return prepared_reset_result::invalid_owner; }
            if(drained.status!=prepared_drain_status::drained || !drained.actual) { return prepared_reset_result::pending_execution; }
            return publish_prepared_drained_reset(transition,prepared,drained.actual,::std::forward<PublishResources>(publish_resources));
        }

        // REAL leases for privately scheduled new workers while ordinary
        // try_enter() stays closed. A caller cannot forge the privately captured
        // native lifetime/control block by providing an epoch/ID/boolean.
        [[nodiscard]] lease try_enter_owned_published_startup_generation(prepared_generation const& prepared)
        {
            ::std::lock_guard admission{admission_mutex};
            if(accepting || prepared.owner_!=this || prepared.phase_!=prepared_generation::phase::published_closed ||
               !prepared.observed_ || prepared.replacement_ || current.get()!=prepared.observed_.get() ||
               current.owner_before(prepared.observed_) || prepared.observed_.owner_before(current) || current->stop_requested())
            { return {}; }
            return current->try_enter();
        }
        template<typename OpenActualStartup>
        [[nodiscard]] bool open_published_startup_generation(maintenance_transition const& transition,
            prepared_generation& prepared,OpenActualStartup&& open_actual_startup)
        {
            static_assert(::std::is_nothrow_invocable_r_v<bool,OpenActualStartup&>,
                "Opening startup requires a finite internal noexcept callback with actual root/worker proof.");
            if(transition.owner_!=this || !transition.lock_.owns_lock() || prepared.owner_!=this ||
               prepared.phase_!=prepared_generation::phase::published_closed || !prepared.observed_ || prepared.replacement_)
            { return false; }
            ::std::lock_guard admission{admission_mutex};
            if(accepting || current.get()!=prepared.observed_.get() || current.owner_before(prepared.observed_) ||
               prepared.observed_.owner_before(current) || current->stop_requested()) { return false; }
            // Generic lifecycle ownership alone is not Wasm restore permission.
            // The genuine runtime issuer verifies ALL seeded native participants/
            // current source/code/epoch under its own private gate here. No wait,
            // same-domain reentry, provider callback or physical join inside it.
            if(!::std::forward<OpenActualStartup>(open_actual_startup)()) { return false; }
            prepared.phase_=prepared_generation::phase::opened;accepting=true;return true;
        }

        template<typename ResetResources>
        void reset(ResetResources&& reset_resources)
        {
            ::std::unique_lock maintenance{maintenance_mutex};
            auto generation{close_admission()};
            // Do not hold the admission lock across cancellation, draining or
            // user cleanup: callbacks may try_enter and must promptly fail closed.
            generation->request_stop();
            generation->drain();
            ::std::forward<ResetResources>(reset_resources)();
            // If cleanup or allocation throws, admission remains closed. Never
            // publish a partially reset set of resources to a new caller.
            auto next{::std::make_shared<execution_lifetime>(capacity)};
            {
                ::std::lock_guard lock{admission_mutex};
                current = ::std::move(next);
                accepting = true;
            }
        }
    };
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif

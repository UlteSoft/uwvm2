/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <stop_token>
#include <utility>
#include "admission.h"
#include "wait.h"
#if defined(__linux__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# include <cerrno>
# include <pthread.h>
# include <sys/eventfd.h>
# include <sys/mman.h>
# include <unistd.h>
#endif

namespace uwvm2::uwvm::runtime::initializer { class ThreadedChildWorld; class ThreadedMainActiveSegments; }
namespace uwvm2::runtime::wasm_threads
{
    enum class termination_kind : unsigned { running, cancelled, trap, proc_exit, native_failure, memory_failure, main_return };
    struct group_outcome
    {
        termination_kind kind{termination_kind::running};
        ::std::uint32_t code{};
        ::std::size_t module{}, function{};
        ::std::uint64_t wasm_offset{};
    };
    enum class host_dispatch_status : unsigned { returned, group_stopped, rejected };
    enum class import_admission : unsigned { allowed, denied, unsupported };

#if defined(__linux__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // These objects never appear in a Guest import's raw argument ABI. Runtime
    // owns the group, closes spawn on stop/update/debug, and retains it on every
    // pending physical join. No Restore credential is accepted anywhere here.
    class ThreadedGroup final
    {
        friend class ::uwvm2::runtime::lib::runtime_threaded_profile_bridge;
        friend class ::uwvm2::uwvm::runtime::initializer::ThreadedMainActiveSegments;
        using world_owner = ::std::shared_ptr<::uwvm2::uwvm::runtime::initializer::ThreadedChildWorld const>;
        enum class slot_phase : unsigned { empty, preparing, starting, running, joining, cleaning, disposing };
        struct slot;
        struct compiler_slot;

    public:
        class CompilerLease;
        class ChildLease final
        {
            friend class ThreadedGroup;
            friend class ::uwvm2::runtime::lib::runtime_threaded_profile_bridge;
            ThreadedGroup* group_{};
            slot* slot_{};
            ChildLease(ThreadedGroup& group, slot& actual) noexcept : group_{&group}, slot_{&actual} {}
        public:
            ChildLease(ChildLease const&) = delete;
            ChildLease& operator=(ChildLease const&) = delete;
            [[nodiscard]] ::std::stop_token cancellation() const noexcept { return group_->cancellation_.get_token(); }
            [[nodiscard]] wait_domain& waits() const noexcept { return group_->waits_; }
            // A borrowed level-triggered fd. Host pollers MUST NOT consume it or
            // retain it past this actual native call. It closes only after join.
            [[nodiscard]] int cancellation_fd() const noexcept { return group_->stop_fd_; }
            [[nodiscard]] auto const& admission() const noexcept { return *group_->admission_; }
            [[nodiscard]] ::uwvm2::uwvm::runtime::initializer::ThreadedChildWorld const& world() const noexcept;
            [[nodiscard]] group_outcome read_outcome() const noexcept { return group_->read_outcome(); }
            void request_trap(::std::uint32_t kind, ::std::size_t module, ::std::size_t function,
                ::std::uint64_t offset) noexcept
            { group_->request_stop({termination_kind::trap, kind, module, function, offset}); }
            void request_proc_exit(::std::uint32_t code) noexcept
            { group_->request_stop({termination_kind::proc_exit, code, 0u, 0u, 0u}); }
            void report_memory_failure(::std::uint32_t phase, bool tainted,
                ::std::size_t module, ::std::size_t function, ::std::uint64_t offset) noexcept
            {
                if(tainted) { group_->memory_tainted_.store(true, ::std::memory_order_release); }
                group_->request_stop({termination_kind::memory_failure, phase, module, function, offset});
            }
            [[nodiscard]] bool full_source_reset_required() const noexcept
            { return group_->ever_launched_.load(::std::memory_order_acquire) ||
                group_->shared_effects_started_.load(::std::memory_order_acquire); }
            // Only synchronous Host dispatch on the native thread executing this
            // exact slot may use its principal/memory. An escaped borrow fails.
            [[nodiscard]] bool owns_current_native_thread() const noexcept;
            [[nodiscard]] ::std::size_t native_stack_usable_bytes() const noexcept;
        };

        enum class child_completion : unsigned { returned, group_stopped, rejected };
        class ChildWork
        {
        protected:
            ChildWork() noexcept = default;
        public:
            virtual ~ChildWork() = default;
            [[nodiscard]] virtual world_owner retained_world() const noexcept = 0;
            [[nodiscard]] virtual ::std::shared_ptr<ThreadedSpawnAdmission const> retained_admission() const noexcept = 0;
            [[nodiscard]] virtual ::std::size_t owned_storage_bytes() const noexcept = 0;
            [[nodiscard]] virtual ::std::size_t owned_code_bytes() const noexcept = 0;
            // The concrete Runtime adapter MUST hold the real child world and
            // one actually compiled/bound engine. There is no callback fallback,
            // parent Guest entry, ready flag or successful default implementation.
            [[nodiscard]] virtual child_completion run(ChildLease&, ::std::uint32_t tid,
                ::std::uint32_t opaque_start_argument) noexcept = 0;
        };

        class SpawnReservation final
        {
            friend class ThreadedGroup;
            friend class CompilerLease;
            friend class ::uwvm2::uwvm::runtime::initializer::ThreadedMainActiveSegments;
            ThreadedGroup* group_{};
            ::std::shared_ptr<ThreadedGroup> retained_group_;
            ::std::size_t position_{};
            ::std::uint32_t tid_{};
            SpawnReservation(::std::shared_ptr<ThreadedGroup> group, ::std::size_t position, ::std::uint32_t tid) noexcept
                : group_{group.get()}, retained_group_{::std::move(group)}, position_{position}, tid_{tid} {}
            void clear() noexcept { group_ = nullptr; }
        public:
            SpawnReservation() noexcept = default;
            SpawnReservation(SpawnReservation const&) = delete;
            SpawnReservation& operator=(SpawnReservation const&) = delete;
            SpawnReservation(SpawnReservation&& other) noexcept
                : group_{::std::exchange(other.group_, nullptr)}, retained_group_{::std::move(other.retained_group_)},
                  position_{other.position_}, tid_{other.tid_} {}
            SpawnReservation& operator=(SpawnReservation&& other) noexcept
            {
                if(this != &other)
                { cancel(); group_ = ::std::exchange(other.group_, nullptr); retained_group_ = ::std::move(other.retained_group_);
                  position_ = other.position_; tid_ = other.tid_; }
                return *this;
            }
            ~SpawnReservation() { cancel(); }
            void cancel() noexcept;
            [[nodiscard]] explicit operator bool() const noexcept { return group_ != nullptr; }
            [[nodiscard]] ::std::uint32_t tid() const noexcept { return tid_; }
        };

        struct launch_result { ::std::int32_t tid{-1}; int native_error{}; };
        struct retirement_result
        {
            bool physically_joined{};
            ::std::size_t preparing{}, native_pending{};
            int native_error{};
            ::std::size_t compiler_tickets_pending{}, native_cleanup_pending{};
        };

#include "thread_group_compiler_types.inc"

    private:
        struct slot
        {
            ThreadedGroup* group{};
            slot_phase phase{slot_phase::empty};
            ::std::uint32_t tid{}, argument{};
            bool initial_main{};
            ::pthread_t thread{};
            native_stack stack;
            bool native_created{};
            ::std::unique_ptr<ChildWork> work;
            // Deliberate retention cycle until the manager physically joins.
            // Dropping an external handle must not destroy live native code.
            ::std::shared_ptr<ThreadedGroup> native_retention;
            bool launch_published{};
            ::std::atomic<bool> body_returned{};
        };
        ::std::shared_ptr<ThreadedSpawnAdmission const> admission_;
        ::std::weak_ptr<ThreadedGroup> canonical_owner_;
        ::std::unique_ptr<slot[]> slots_;
        ::std::mutex mutex_, retirement_mutex_;
        ::std::condition_variable changes_;
        ::std::stop_source cancellation_;
        wait_domain waits_;
        ::std::atomic_flag outcome_claimed_{};
        ::std::atomic<termination_kind> outcome_kind_{termination_kind::running};
        ::std::uint32_t outcome_code_{};
        ::std::size_t outcome_module_{}, outcome_function_{};
        ::std::uint64_t outcome_offset_{};
        ::std::atomic<bool> stop_requested_{};
        ::std::atomic<bool> ever_launched_{}, memory_tainted_{}, shared_effects_started_{};
        ::std::size_t occupied_{}, storage_reserved_{}, code_reserved_{};
        ::std::size_t occupied_children_{};
        ::std::uint32_t next_tid_{1u}, attempts_{};
        bool accepting_{true};
        bool initial_main_reserved_{};
        int stop_fd_{-1};
        int initialization_error_{};

#include "thread_group_compiler_state.inc"

        [[nodiscard]] static bool limits_admitted(threaded_profile_limits const& limits) noexcept
        {
            constexpr auto maximum{static_cast<::std::size_t>(-1)};
            if(limits.max_active_children == 0u || limits.max_active_children >= (1u << 29u) ||
               limits.max_spawn_attempts == 0u || limits.max_spawn_attempts >= (1u << 29u) ||
               limits.native_stack_bytes < static_cast<::std::size_t>(PTHREAD_STACK_MIN) ||
               limits.compiler_stack_bytes < static_cast<::std::size_t>(PTHREAD_STACK_MIN) ||
               limits.compiler_stack_bytes == maximum ||
               limits.native_stack_bytes == maximum || limits.max_live_native_stack_bytes == maximum ||
               limits.max_live_native_stack_bytes < limits.native_stack_bytes ||
               limits.compiler_stack_bytes > limits.max_live_native_stack_bytes - limits.native_stack_bytes ||
               limits.max_child_storage_bytes == 0u || limits.max_child_storage_bytes == maximum ||
               limits.max_live_storage_bytes == maximum || limits.max_live_storage_bytes < limits.max_child_storage_bytes ||
               limits.max_child_code_bytes == 0u || limits.max_child_code_bytes == maximum ||
               limits.max_live_code_bytes == maximum || limits.max_live_code_bytes < limits.max_child_code_bytes ||
               limits.max_value_stack_bytes == 0u || limits.max_value_stack_bytes == maximum ||
               limits.max_control_frames == 0u || limits.max_control_frames == maximum ||
               limits.max_call_frames == 0u || limits.max_call_frames == maximum ||
               limits.max_table_elements == maximum || limits.max_local_memory_bytes == maximum ||
               limits.max_shared_memory_bytes == 0u || limits.max_shared_memory_bytes == maximum ||
               limits.max_total_segment_bytes == maximum ||
               limits.max_active_children + 1u > maximum / sizeof(slot) ||
               limits.max_active_children + 1u > maximum / sizeof(compiler_slot)) { return false; }
            return true;
        }

        explicit ThreadedGroup(::std::shared_ptr<ThreadedSpawnAdmission const> admission)
            : admission_{::std::move(admission)}
        {
            // The bridge has already admitted positive finite limits and the
            // actual native-thread provider. Creation failure publishes nothing.
            if(!admission_ || !admission_->owns_actual_generation() || !limits_admitted(admission_->limits()))
            { accepting_ = false; initialization_error_ = EINVAL; return; }
            auto const& limits{admission_->limits()};
            auto const page{::sysconf(_SC_PAGESIZE)};
            if(page <= 0) { accepting_ = false; initialization_error_ = EINVAL; return; }
            page_bytes_ = static_cast<::std::size_t>(page);
            if(page_bytes_ > SIZE_MAX / 2u || (page_bytes_ & (page_bytes_ - 1u)) != 0u ||
               limits.native_stack_bytes % page_bytes_ != 0u || limits.compiler_stack_bytes % page_bytes_ != 0u ||
               limits.native_stack_bytes <= page_bytes_ * 2u || limits.compiler_stack_bytes <= page_bytes_ * 2u ||
               limits.native_stack_bytes - page_bytes_ * 2u < static_cast<::std::size_t>(PTHREAD_STACK_MIN) ||
               limits.compiler_stack_bytes - page_bytes_ * 2u < static_cast<::std::size_t>(PTHREAD_STACK_MIN))
            { accepting_ = false; initialization_error_ = EINVAL; return; }
            auto const count{limits.max_active_children + 1u};
            auto const guest_bytes{count * sizeof(slot)}, queue_bytes{count * sizeof(compiler_slot)};
            if(guest_bytes > SIZE_MAX - sizeof(ThreadedGroup) ||
               queue_bytes > SIZE_MAX - sizeof(ThreadedGroup) - guest_bytes)
            { accepting_ = false; initialization_error_ = EOVERFLOW; return; }
            fixed_storage_bytes_ = sizeof(ThreadedGroup) + guest_bytes + queue_bytes;
            // Actual requested object/array payload, not allocator metadata/RSS.
            // The fixed Group/queue charge stays inside the SAME live cap.
            if(fixed_storage_bytes_ >= limits.max_live_storage_bytes)
            { accepting_ = false; initialization_error_ = ENOMEM; return; }
            storage_reserved_ = fixed_storage_bytes_;
            slots_.reset(new (::std::nothrow) slot[count]);
            compiler_jobs_.reset(new (::std::nothrow) compiler_slot[count]);
            if(!slots_ || !compiler_jobs_) { accepting_ = false; initialization_error_ = ENOMEM; return; }
            stop_fd_ = ::eventfd(0u, EFD_NONBLOCK | EFD_CLOEXEC);
            if(stop_fd_ == -1) { accepting_ = false; initialization_error_ = errno; return; }
            for(::std::size_t i{}; i != admission_->limits().max_active_children + 1u; ++i) { slots_[i].group = this; }
        }

        void release_reservation(slot& selected) noexcept
        {
            auto const& limits{admission_->limits()};
            --occupied_; storage_reserved_ -= limits.max_child_storage_bytes;
            if(!selected.initial_main) { --occupied_children_; }
            code_reserved_ -= limits.max_child_code_bytes;
            native_stack_reserved_ -= limits.native_stack_bytes;
            selected.phase = slot_phase::empty;
            selected.launch_published = false;
            selected.native_created = false;
        }
        void cancel_reservation(::std::size_t position, ::std::uint32_t tid) noexcept
        {
            ::std::lock_guard lock{mutex_};
            auto& selected{slots_[position]};
            if(selected.phase == slot_phase::preparing && selected.tid == tid)
            {
                // An actual stack mapping/resource must retire before releasing
                // its budget. Its owned no-thread cleanup is part of join_until.
                if(selected.stack.mapping || selected.work) { selected.phase = slot_phase::cleaning; }
                else { release_reservation(selected); }
                changes_.notify_all();
            }
        }

        static void* native_entry(void* pointer) noexcept
        {
            auto& selected{*static_cast<slot*>(pointer)};
            auto& group{*selected.group};
            {
                ::std::unique_lock lock{group.mutex_};
                group.changes_.wait(lock, [&] { return selected.launch_published; });
            }
            ChildLease lease{group, selected};
            if(!group.stop_requested_.load(::std::memory_order_acquire))
            {
                auto const result{selected.work->run(lease, selected.tid, selected.argument)};
                if(result == child_completion::rejected)
                { group.request_stop({termination_kind::native_failure, 1u, 0u, 0u, 0u}); }
            }
            // This reports ONLY body return. Native TLS destructors still run;
            // resources remain retained until pthread_tryjoin_np really succeeds.
            selected.body_returned.store(true, ::std::memory_order_release);
            group.changes_.notify_all();
            return nullptr;
        }

    public:
        ThreadedGroup(ThreadedGroup const&) = delete;
        ThreadedGroup& operator=(ThreadedGroup const&) = delete;
        // Controlled Runtime retirement first uses join_until and retains this
        // object on pending. Last-owner destruction is a synchronous emergency
        // join, never detach. It is forbidden on an owned child native thread.
        ~ThreadedGroup()
        {
            if(initialization_error_ != 0) { return; }
            request_stop({termination_kind::cancelled, 0u, 0u, 0u, 0u});
            for(;;)
            {
                auto const result{join_until(::std::chrono::steady_clock::now() + ::std::chrono::milliseconds{100})};
                if(result.physically_joined) { break; }
                if(result.native_error != 0) { ::std::terminate(); }
                // A live preparation reservation owns a borrow of this group;
                // normal Runtime ownership prevents destruction until it leaves.
            }
            if(stop_fd_ != -1) { ::close(stop_fd_); }
        }

        [[nodiscard]] SpawnReservation reserve() noexcept { return reserve_selected(false); }

    private:
        // Only Runtime starts the initial main, once, in a REAL owned native
        // slot with the same stack/stop/retirement policy. TID0 is internal and
        // is never a successful wasi.thread-spawn result. Parent code must be
        // independently compiled with the same threaded execution controls.
        [[nodiscard]] SpawnReservation reserve_initial_main() noexcept { return reserve_selected(true); }
        [[nodiscard]] SpawnReservation reserve_selected(bool initial_main) noexcept
        {
            if(initialization_error_ != 0 || !admission_) { return {}; }
            auto retained{canonical_owner_.lock()};
            if(!retained) { return {}; }
            ::std::lock_guard lock{mutex_};
            auto const& limits{admission_->limits()};
            if(initialization_error_ != 0 || !runtime_control_charged_ || !accepting_ || stop_requested_.load(::std::memory_order_acquire) ||
               (initial_main ? initial_main_reserved_ :
                   attempts_ >= limits.max_spawn_attempts || next_tid_ >= (1u << 29u) ||
                   occupied_children_ >= limits.max_active_children) ||
               limits.native_stack_bytes > limits.max_live_native_stack_bytes - native_stack_reserved_ ||
               compiler_payload_reserved_ > limits.max_live_storage_bytes - storage_reserved_ ||
               limits.max_child_storage_bytes > limits.max_live_storage_bytes - storage_reserved_ - compiler_payload_reserved_ ||
               limits.max_child_code_bytes > limits.max_live_code_bytes - code_reserved_) { return {}; }
            auto const begin{initial_main ? 0u : 1u};
            auto const end{initial_main ? 1u : limits.max_active_children + 1u};
            for(::std::size_t i{begin}; i != end; ++i)
            {
                auto& selected{slots_[i]};
                if(selected.phase != slot_phase::empty) { continue; }
                selected.phase = slot_phase::preparing; selected.initial_main = initial_main;
                selected.tid = initial_main ? 0u : next_tid_++;
                selected.body_returned.store(false, ::std::memory_order_relaxed);
                if(initial_main) { initial_main_reserved_ = true; }
                else { ++attempts_; ++occupied_children_; }
                ++occupied_;
                storage_reserved_ += limits.max_child_storage_bytes;
                native_stack_reserved_ += limits.native_stack_bytes;
                code_reserved_ += limits.max_child_code_bytes;
                return SpawnReservation{::std::move(retained), i, selected.tid};
            }
            return {};
        }

    public:
        [[nodiscard]] launch_result launch(SpawnReservation&& reservation,
            ::std::unique_ptr<ChildWork> work, ::std::uint32_t opaque_argument) noexcept
        {
            if(initialization_error_ != 0 || !admission_) { return {-1, initialization_error_}; }
            if(!actual_compiler_reservation(reservation) || !work || !work->retained_world() ||
               work->retained_admission().get() != admission_.get() ||
               work->owned_storage_bytes() > admission_->limits().max_child_storage_bytes ||
               work->owned_code_bytes() > admission_->limits().max_child_code_bytes) { return {}; }
            // The compiler worker mapped THIS exact Guest reservation before
            // codegen; frame proof and launch therefore see the same inner stack.
            auto& target{slots_[reservation.position_]};
            if(!target.stack.mapping || target.stack.preparation_error != 0 || target.native_created) { return {}; }
            ::pthread_attr_t attributes{};
            int error{::pthread_attr_init(&attributes)};
            if(error != 0) { return {-1, error}; }
            error = ::pthread_attr_setguardsize(&attributes, 0u);
            if(error == 0) { error = ::pthread_attr_setstack(&attributes, target.stack.usable_begin(), target.stack.usable_bytes()); }
            if(error != 0) { ::pthread_attr_destroy(&attributes); return {-1, error}; }
            launch_result result{};
            {
                ::std::lock_guard lock{mutex_};
                auto& selected{slots_[reservation.position_]};
                if(!accepting_ || stop_requested_.load(::std::memory_order_acquire) ||
                   selected.phase != slot_phase::preparing || selected.tid != reservation.tid_)
                { ::pthread_attr_destroy(&attributes); return {}; }
                selected.work = ::std::move(work); selected.argument = opaque_argument;
                selected.native_retention = reservation.retained_group_;
                selected.phase = slot_phase::starting;
                error = ::pthread_create(&selected.thread, &attributes, native_entry, &selected);
                if(error == 0)
                {
                    selected.native_created = true;
                    ever_launched_.store(true, ::std::memory_order_release);
                    selected.phase = slot_phase::running; selected.launch_published = true;
                    result.tid = static_cast<::std::int32_t>(selected.tid);
                    compiler_current_->actual_launch = result;
                    reservation.clear(); changes_.notify_all();
                }
                else
                {
                    // Keep exact work/mapping/reservation credits until join's
                    // no-thread cleanup destroys them OUTSIDE the Group lock.
                    selected.phase = slot_phase::cleaning;
                    reservation.clear(); result.native_error = error;
                    changes_.notify_all();
                }
            }
            ::pthread_attr_destroy(&attributes);
            return result;
        }

        void request_stop(group_outcome reason) noexcept
        {
            if(reason.kind == termination_kind::running) { reason.kind = termination_kind::cancelled; }
            if(!outcome_claimed_.test_and_set(::std::memory_order_acq_rel))
            {
                outcome_code_ = reason.code; outcome_module_ = reason.module;
                outcome_function_ = reason.function; outcome_offset_ = reason.wasm_offset;
                outcome_kind_.store(reason.kind, ::std::memory_order_release);
            }
            else if(outcome_kind_.load(::std::memory_order_acquire) == termination_kind::running)
            {
                // The first claimant still owns the unpublished payload. A
                // losing caller must not stop/wake workers before that complete
                // terminal result is visible. It never spins or waits for the
                // claimant; the claimant will perform the real stop below.
                return;
            }
            stop_requested_.store(true, ::std::memory_order_release);
            if(initialization_error_ != 0) { return; } // Nothing was published or launched.
            {
                ::std::lock_guard lock{mutex_}; accepting_ = false; compiler_closing_ = true;
            }
            // Compiler observers see the real stopping atomic immediately;
            // don't delay their wake behind synchronous stop callbacks.
            changes_.notify_all();
            // Every concurrent poller sees level-triggered readiness. EAGAIN
            // means it was already readable; nobody in the group drains it.
            ::std::uint64_t const one{1u};
            for(;;)
            {
                auto const written{::write(stop_fd_, &one, sizeof(one))};
                if(written == static_cast<::ssize_t>(sizeof(one)) || (written == -1 && errno == EAGAIN)) { break; }
                if(written == -1 && errno == EINTR) { continue; }
                // A broken owned cancellation fd cannot qualify safe stop.
                ::std::terminate();
            }
            (void)cancellation_.request_stop(); changes_.notify_all();
        }
        [[nodiscard]] group_outcome read_outcome() const noexcept
        {
            auto const kind{outcome_kind_.load(::std::memory_order_acquire)};
            if(kind == termination_kind::running) { return {}; }
            return {kind, outcome_code_, outcome_module_, outcome_function_, outcome_offset_};
        }

        [[nodiscard]] retirement_result join_until(::std::chrono::steady_clock::time_point deadline) noexcept
        {
            if(initialization_error_ != 0 || !admission_) { return {false, 0u, 0u, initialization_error_}; }
            auto retained_this{canonical_owner_.lock()};
            ::std::unique_lock retirement{retirement_mutex_, ::std::try_to_lock};
            if(!retirement.owns_lock()) { return {false, 0u, 0u, EBUSY}; }
            for(;;)
            {
                retirement_result result{true, 0u, 0u, 0};
                for(::std::size_t i{}; i != admission_->limits().max_active_children + 1u; ++i)
                {
                    ::pthread_t actual{};
                    bool has_native{}, cleanup{};
                    ::std::uint32_t expected_tid{};
                    {
                        ::std::lock_guard lock{mutex_};
                        auto& selected{slots_[i]};
                        expected_tid = selected.tid;
                        if(selected.phase == slot_phase::empty) { continue; }
                        result.physically_joined = false;
                        if(selected.phase == slot_phase::preparing || selected.phase == slot_phase::starting)
                        { ++result.preparing; continue; }
                        if(selected.phase == slot_phase::disposing) { ++result.native_cleanup_pending; continue; }
                        has_native = selected.native_created;
                        cleanup = !has_native;
                        if(has_native)
                        {
                            actual = selected.thread;
                            if(::pthread_equal(actual, ::pthread_self())) { result.native_error = EDEADLK; return result; }
                            selected.phase = slot_phase::joining;
                        }
                    }
                    if(has_native)
                    {
                        auto const joined{::pthread_tryjoin_np(actual, nullptr)};
                        ::std::lock_guard lock{mutex_};
                        auto& selected{slots_[i]};
                        if(joined == 0)
                        { selected.native_created = false; selected.phase = slot_phase::cleaning; cleanup = true; }
                        else
                        {
                            selected.phase = slot_phase::running; ++result.native_pending;
                            if(joined != EBUSY) { result.native_error = joined; }
                        }
                    }
                    if(cleanup)
                    {
                        auto const error{retire_slot_storage(i, expected_tid)};
                        if(error != 0) { result.native_error = error; ++result.native_cleanup_pending; }
                    }
                }
                ::std::size_t compiler_pending{};
                auto const compiler_error{retire_compiler_once(compiler_pending, result.preparing, result.native_cleanup_pending)};
                result.native_pending += compiler_pending;
                if(compiler_error != 0) { result.native_error = compiler_error; }
                {
                    ::std::unique_lock lock{mutex_};
                    if(occupied_ == 0u && !compiler_created_ && !compiler_stack_.mapping &&
                       !compiler_native_retention_ && compiler_current_ == nullptr && compiler_head_ == SIZE_MAX &&
                       compiler_payload_reserved_ == 0u && active_compiler_tickets_ == 0u &&
                       outcome_kind_.load(::std::memory_order_acquire) != termination_kind::running)
                    { return {true, 0u, 0u, 0}; }
                    // Empty native/resource counts alone cannot freeze an
                    // unpublished first outcome into Runtime's joined state.
                    // Retain the Group and use the same deadline-bounded wait;
                    // no synthetic native-pending count or unbounded spin.
                    result.compiler_tickets_pending = active_compiler_tickets_;
                    result.physically_joined = false;
                    if(result.native_error != 0 || ::std::chrono::steady_clock::now() >= deadline) { return result; }
                    auto const short_wait{::std::chrono::steady_clock::now() + ::std::chrono::milliseconds{1}};
                    changes_.wait_until(lock, short_wait < deadline ? short_wait : deadline);
                }
            }
        }

    };

#include "thread_group_compiler_impl.h"

    inline void ThreadedGroup::SpawnReservation::cancel() noexcept
    { if(group_ != nullptr) { group_->cancel_reservation(position_, tid_); clear(); } retained_group_.reset(); }
    inline ::uwvm2::uwvm::runtime::initializer::ThreadedChildWorld const& ThreadedGroup::ChildLease::world() const noexcept
    { return *slot_->work->retained_world(); }
    inline bool ThreadedGroup::ChildLease::owns_current_native_thread() const noexcept
    { return ::pthread_equal(slot_->thread, ::pthread_self()) != 0; }
    inline ::std::size_t ThreadedGroup::ChildLease::native_stack_usable_bytes() const noexcept
    { return owns_current_native_thread() ? slot_->stack.usable_bytes() : 0u; }

    // Implemented by the REAL Runtime bridge, not a user callback or a default
    // provider. It binds canonical principal/generation and child memory, audits
    // the import's exact ABI/permissions/blocking policy and supplies the real
    // stop fd to the Host call. Missing implementation remains a link failure.
    [[nodiscard]] host_dispatch_status dispatch_import(ThreadedGroup::ChildLease&,
        ::uwvm2::uwvm::runtime::initializer::ThreadedChildWorld const&,
        ::std::size_t caller_module, ::std::size_t caller_function, ::std::size_t imported_function,
        ::std::span<::std::byte> results, ::std::span<::std::byte const> params) noexcept;
    [[nodiscard]] import_admission admit_import(
        ::uwvm2::uwvm::runtime::initializer::ThreadedChildWorld const&,
        ::std::size_t caller_module, ::std::size_t imported_function) noexcept;
#endif
}

// Included INSIDE namespace uwvm2::runtime::wasm_threads, AFTER ThreadedGroup.
// Cold profile only. No ordinary compiler/Guest hot path calls this file.

    inline auto ThreadedGroup::allocate_native_stack(::std::size_t bytes) const noexcept -> native_stack
    {
        native_stack result{};
        if(page_bytes_ == 0u || bytes % page_bytes_ != 0u || bytes <= page_bytes_ * 2u ||
           bytes - page_bytes_ * 2u < static_cast<::std::size_t>(PTHREAD_STACK_MIN))
        { result.preparation_error = EINVAL; return result; }
        auto const mapping{::mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0)};
        if(mapping == MAP_FAILED) { result.preparation_error = errno; return result; }
        result.mapping = mapping; result.bytes = bytes; result.guard_bytes = page_bytes_;
        if(::mprotect(result.usable_begin(), result.usable_bytes(), PROT_READ | PROT_WRITE) != 0)
        { result.preparation_error = errno; }
        // Even mprotect failure returns its REAL mapping to Group ownership.
        // A failed cleanup must remain visible/pinned, never lose an address.
        return result;
    }
    inline int ThreadedGroup::release_native_stack(native_stack& stack) noexcept
    {
        if(stack.mapping == nullptr) { return 0; }
        if(::munmap(stack.mapping, stack.bytes) != 0) { return errno; }
        stack = {}; return 0;
    }

    inline bool ThreadedGroup::charge_runtime_control_storage(::std::size_t bytes) noexcept
    {
        auto retained{canonical_owner_.lock()};
        if(!retained || retained.get() != this || initialization_error_ != 0 || !admission_ || bytes == 0u) { return false; }
        ::std::lock_guard lock{mutex_};
        if(runtime_control_charged_ || compiler_started_ || occupied_ != 0u || native_stack_reserved_ != 0u ||
           compiler_head_ != SIZE_MAX || compiler_payload_reserved_ != 0u ||
           storage_reserved_ != fixed_storage_bytes_ || !accepting_ || stop_requested_.load(::std::memory_order_acquire) ||
           bytes > admission_->limits().max_live_storage_bytes - storage_reserved_) { return false; }
        fixed_storage_bytes_ += bytes; storage_reserved_ += bytes; runtime_control_charged_ = true;
        return true;
    }

    inline int ThreadedGroup::start_compiler_worker() noexcept
    {
        auto retained{canonical_owner_.lock()};
        if(!retained || initialization_error_ != 0 || !admission_) { return EINVAL; }
        {
            ::std::lock_guard lock{mutex_};
            auto const charge{admission_->limits().compiler_stack_bytes};
            if(compiler_started_) { return EALREADY; }
            if(!runtime_control_charged_) { return EINVAL; }
            if(!accepting_ || stop_requested_.load(::std::memory_order_acquire)) { return ECANCELED; }
            if(charge > admission_->limits().max_live_native_stack_bytes - native_stack_reserved_) { return ENOMEM; }
            compiler_started_ = true; compiler_starting_ = true;
            compiler_stack_charged_ = true; native_stack_reserved_ += charge;
            // This deliberate retention outlives body return and every job.
            // Only real pthread join plus owned-stack retirement releases it.
            compiler_native_retention_ = retained;
        }
        auto stack{allocate_native_stack(admission_->limits().compiler_stack_bytes)};
        {
            ::std::lock_guard lock{mutex_}; compiler_stack_ = stack;
            if(stack.preparation_error != 0)
            { compiler_starting_ = false; compiler_closing_ = true; changes_.notify_all(); return stack.preparation_error; }
        }
        ::pthread_attr_t attributes{};
        auto error{::pthread_attr_init(&attributes)};
        if(error == 0)
        {
            error = ::pthread_attr_setguardsize(&attributes, 0u);
            if(error == 0) { error = ::pthread_attr_setstack(&attributes, stack.usable_begin(), stack.usable_bytes()); }
            if(error == 0)
            {
                ::std::lock_guard lock{mutex_};
                if(!accepting_ || stop_requested_.load(::std::memory_order_acquire)) { error = ECANCELED; }
                else
                {
                    error = ::pthread_create(&compiler_thread_, &attributes, compiler_native_entry, this);
                    if(error == 0) { compiler_created_ = true; }
                }
                compiler_starting_ = false;
                if(error != 0) { compiler_closing_ = true; }
                changes_.notify_all();
            }
            static_cast<void>(::pthread_attr_destroy(&attributes));
        }
        if(error != 0)
        {
            ::std::lock_guard lock{mutex_}; compiler_starting_ = false; compiler_closing_ = true; changes_.notify_all();
        }
        // Failure does not silently free/forget the mapping or its reservation.
        // The published Root profile must use join_until even with no pthread.
        return error;
    }

    inline auto ThreadedGroup::submit_compilation(SpawnReservation&& reservation,
        ::std::unique_ptr<CompilerWork> work) noexcept -> compiler_submission
    {
        auto retained{canonical_owner_.lock()};
        if(!retained || initialization_error_ != 0 || !admission_ || !work || reservation.group_ != this)
        { return {{}, EINVAL}; }
        auto const actual_admission{work->retained_admission()};
        auto const bytes{work->owned_submission_bytes()};
        if(actual_admission.get() != admission_.get() ||
           actual_admission.owner_before(admission_) || admission_.owner_before(actual_admission) ||
           bytes < sizeof(CompilerWork) || bytes > admission_->limits().max_child_storage_bytes)
        { return {{}, EINVAL}; }
        ::std::lock_guard lock{mutex_};
        if(!accepting_ || compiler_closing_ || !compiler_created_ || compiler_starting_ ||
           stop_requested_.load(::std::memory_order_acquire)) { return {{}, ECANCELED}; }
        auto const count{admission_->limits().max_active_children + 1u};
        if(reservation.position_ >= count) { return {{}, EINVAL}; }
        auto const& guest{slots_[reservation.position_]};
        if(guest.phase != slot_phase::preparing || guest.tid != reservation.tid_ || guest.stack.mapping ||
           next_compiler_sequence_ == UINT64_MAX) { return {{}, EINVAL}; }
        auto const maximum{admission_->limits().max_live_storage_bytes};
        if(storage_reserved_ > maximum || compiler_payload_reserved_ > maximum - storage_reserved_ ||
           bytes > maximum - storage_reserved_ - compiler_payload_reserved_) { return {{}, ENOMEM}; }
        for(::std::size_t i{}; i != count; ++i)
        {
            auto& selected{compiler_jobs_[i]};
            if(selected.phase != compiler_job_phase::empty) { continue; }
            auto const sequence{next_compiler_sequence_++};
            selected.sequence = sequence; selected.next = SIZE_MAX; selected.charged_bytes = bytes;
            selected.abandoned = false; selected.result = {}; selected.actual_launch = {};
            selected.reservation = ::std::move(reservation); selected.work = ::std::move(work);
            selected.phase = compiler_job_phase::queued;
            selected.ticket_live = true; ++active_compiler_tickets_;
            compiler_payload_reserved_ += bytes;
            if(compiler_tail_ == SIZE_MAX) { compiler_head_ = i; }
            else { compiler_jobs_[compiler_tail_].next = i; }
            compiler_tail_ = i; changes_.notify_all();
            return {CompilerTicket{::std::move(retained), i, sequence}, 0};
        }
        return {{}, EAGAIN}; // fixed queue full; no hidden heap or retry effect.
    }

    inline bool ThreadedGroup::actual_compiler_reservation(SpawnReservation const& reservation) noexcept
    {
        ::std::lock_guard lock{mutex_};
        auto const* job{compiler_current_};
        if(!compiler_created_ || !job || !::pthread_equal(compiler_thread_, ::pthread_self()) ||
           job->phase != compiler_job_phase::running || &job->reservation != &reservation ||
           reservation.group_ != this || reservation.position_ >= admission_->limits().max_active_children + 1u) { return false; }
        auto const& selected{slots_[reservation.position_]};
        return selected.phase == slot_phase::preparing && selected.tid == reservation.tid_;
    }
    inline int ThreadedGroup::prepare_compiler_guest_stack(SpawnReservation const& reservation) noexcept
    {
        if(!actual_compiler_reservation(reservation)) { return EINVAL; }
        {
            ::std::lock_guard lock{mutex_};
            if(slots_[reservation.position_].stack.mapping) { return EALREADY; }
        }
        auto stack{allocate_native_stack(admission_->limits().native_stack_bytes)};
        {
            ::std::lock_guard lock{mutex_};
            auto& selected{slots_[reservation.position_]};
            // Only this worker owns the stored reservation. A stop/ticket
            // abandonment never releases or moves its live job/reservation.
            if(selected.phase != slot_phase::preparing || selected.tid != reservation.tid_ || selected.stack.mapping)
            { ::std::terminate(); }
            selected.stack = stack;
        }
        return stack.preparation_error;
    }

    inline void* ThreadedGroup::compiler_native_entry(void* pointer) noexcept
    {
        auto& group{*static_cast<ThreadedGroup*>(pointer)};
        for(;;)
        {
            compiler_slot* selected{};
            ::std::unique_ptr<CompilerWork> work;
            ::std::uint64_t sequence{};
            bool initial_main{};
            {
                ::std::unique_lock lock{group.mutex_};
                group.changes_.wait(lock, [&] { return group.compiler_head_ != SIZE_MAX || group.compiler_closing_; });
                if(group.compiler_head_ == SIZE_MAX)
                {
                    group.compiler_body_returned_ = true; group.changes_.notify_all();
                    return nullptr; // TLS destruction is STILL not a join acknowledgement.
                }
                auto const index{group.compiler_head_}; selected = &group.compiler_jobs_[index];
                if(selected->phase != compiler_job_phase::queued || !selected->work || !selected->reservation)
                { ::std::terminate(); }
                auto const& reserved{selected->reservation};
                if(reserved.position_ >= group.admission_->limits().max_active_children + 1u) { ::std::terminate(); }
                auto const& guest{group.slots_[reserved.position_]};
                if(guest.phase != slot_phase::preparing || guest.tid != reserved.tid_) { ::std::terminate(); }
                initial_main = reserved.position_ == 0u && reserved.tid_ == 0u && guest.initial_main;
                group.compiler_head_ = selected->next;
                if(group.compiler_head_ == SIZE_MAX) { group.compiler_tail_ = SIZE_MAX; }
                selected->next = SIZE_MAX; selected->phase = compiler_job_phase::running;
                group.compiler_current_ = selected; sequence = selected->sequence;
                work = ::std::move(selected->work);
            }
            compiler_result result{compiler_completion::cancelled, {}, 0};
            CompilerLease actual{group, *selected, sequence};
            if(!group.stop_requested_.load(::std::memory_order_acquire))
            {
                auto const stack_error{group.prepare_compiler_guest_stack(selected->reservation)};
                if(stack_error != 0) { result = {compiler_completion::native_failure, {}, stack_error}; }
                else if(!group.stop_requested_.load(::std::memory_order_acquire))
                {
                    // NO Group, Runtime registry, publication or retirement
                    // lock is held across actual World construction/codegen.
                    result = work->run(actual);
                }
            }
            // Do not publish completed until the actual job destructor returns.
            // LLVM is not force-cancelled; codegen and destruction may keep this
            // thread/stack/Source/provider generation pending indefinitely.
            bool inconsistent{};
            {
                ::std::lock_guard lock{group.mutex_};
                auto const launched{selected->actual_launch};
                inconsistent = launched.tid >= 0 ?
                    (result.completion != compiler_completion::launched || result.launched.tid != launched.tid || result.launched.native_error != launched.native_error) :
                    (result.completion == compiler_completion::launched || result.launched.tid >= 0);
            }
            if(inconsistent)
            {
                group.request_stop({termination_kind::native_failure, 2u, 0u, 0u, 0u});
                result = {compiler_completion::native_failure, {}, EIO};
            }
            if(initial_main && result.completion != compiler_completion::launched)
            {
                // Actual slot0 role, captured before reservation consumption.
                // Stack mapping can fail BEFORE Root's virtual main job runs;
                // such a failure must still terminate ordinary main wait.
                group.request_stop({termination_kind::native_failure, 3u, 0u, 0u, 0u});
            }
            work.reset();
            auto const guest_position{selected->reservation.position_};
            auto const guest_tid{selected->reservation.tid_};
            selected->reservation.cancel();
            {
                // Opportunistic no-thread failure cleanup. Never wait on a
                // retirement owner; its normal join pass can perform this too.
                ::std::unique_lock retirement{group.retirement_mutex_, ::std::try_to_lock};
                if(retirement.owns_lock()) { static_cast<void>(group.retire_slot_storage(guest_position, guest_tid)); }
            }
            {
                ::std::lock_guard lock{group.mutex_};
                if(group.compiler_current_ != selected || selected->sequence != sequence ||
                   selected->phase != compiler_job_phase::running) { ::std::terminate(); }
                group.compiler_payload_reserved_ -= selected->charged_bytes;
                selected->charged_bytes = 0u; selected->result = result;
                selected->phase = selected->abandoned ? compiler_job_phase::empty : compiler_job_phase::completed;
                group.compiler_current_ = nullptr; group.changes_.notify_all();
            }
        }
    }

    inline auto ThreadedGroup::observe_compilation(::std::size_t position, ::std::uint64_t sequence,
        ::std::chrono::steady_clock::time_point deadline) noexcept -> compiler_observation
    {
        ::std::unique_lock lock{mutex_};
        if(position >= admission_->limits().max_active_children + 1u) { return {}; }
        if(compiler_created_ && ::pthread_equal(compiler_thread_, ::pthread_self()))
        { return {compiler_wait_status::invalid, {compiler_completion::native_failure, {}, EDEADLK}}; }
        for(;;)
        {
            auto const& selected{compiler_jobs_[position]};
            if(selected.sequence != sequence || selected.phase == compiler_job_phase::empty || selected.abandoned) { return {}; }
            if(stop_requested_.load(::std::memory_order_acquire))
            { return {compiler_wait_status::group_stopped, {compiler_completion::cancelled, {}, 0}}; }
            if(selected.phase == compiler_job_phase::completed) { return {compiler_wait_status::completed, selected.result}; }
            if(::std::chrono::steady_clock::now() >= deadline) { return {compiler_wait_status::pending, {}}; }
            if(deadline == ::std::chrono::steady_clock::time_point::max()) { changes_.wait(lock); }
            else { changes_.wait_until(lock, deadline); }
        }
    }
    inline void ThreadedGroup::abandon_compilation(::std::size_t position, ::std::uint64_t sequence) noexcept
    {
        bool stop{};
        {
            ::std::lock_guard lock{mutex_};
            if(position >= admission_->limits().max_active_children + 1u) { return; }
            auto& selected{compiler_jobs_[position]};
            if(selected.sequence != sequence || !selected.ticket_live) { return; }
            selected.ticket_live = false; --active_compiler_tickets_;
            if(selected.phase == compiler_job_phase::completed) { selected.phase = compiler_job_phase::empty; }
            else { selected.abandoned = true; stop = true; }
            changes_.notify_all();
        }
        // No silent late launch after its sole observer went away. Stop is
        // real whole-Group cancellation; the live job is NOT erased/replayed.
        if(stop) { request_stop({termination_kind::cancelled, 0u, 0u, 0u, 0u}); }
    }

    inline int ThreadedGroup::retire_slot_storage(::std::size_t index, ::std::uint32_t expected_tid) noexcept
    {
        ::std::unique_ptr<ChildWork> retired;
        ::std::shared_ptr<ThreadedGroup> retained;
        {
            ::std::lock_guard lock{mutex_};
            auto& selected{slots_[index]};
            if(selected.tid != expected_tid || selected.phase != slot_phase::cleaning || selected.native_created) { return 0; }
            selected.phase = slot_phase::disposing;
            retired = ::std::move(selected.work); retained = ::std::move(selected.native_retention);
        }
        // Every actual code/World destructor and stack unmap occurs outside the
        // Group lock, after join==0 OR after a proved failed/no pthread creation.
        retired.reset();
        auto const error{release_native_stack(slots_[index].stack)};
        {
            ::std::lock_guard lock{mutex_};
            auto& selected{slots_[index]};
            if(error != 0)
            { selected.native_retention = retained; selected.phase = slot_phase::cleaning; }
            else { release_reservation(selected); }
            changes_.notify_all();
        }
        return error;
    }
    inline int ThreadedGroup::retire_compiler_once(::std::size_t& pending,
        ::std::size_t& preparing, ::std::size_t& cleanup_pending) noexcept
    {
        ::pthread_t actual{}; bool created{};
        {
            ::std::lock_guard lock{mutex_};
            if(!compiler_started_) { return 0; }
            if(compiler_starting_) { ++preparing; return 0; }
            created = compiler_created_;
            if(created)
            {
                actual = compiler_thread_;
                if(::pthread_equal(actual, ::pthread_self())) { ++pending; return EDEADLK; }
            }
        }
        if(created)
        {
            auto const error{::pthread_tryjoin_np(actual, nullptr)};
            if(error != 0) { ++pending; return error == EBUSY ? 0 : error; }
            ::std::lock_guard lock{mutex_}; compiler_created_ = false; compiler_joined_ = true;
        }
        {
            ::std::lock_guard lock{mutex_};
            if(compiler_current_ != nullptr || compiler_head_ != SIZE_MAX || compiler_payload_reserved_ != 0u)
            { ++cleanup_pending; return EIO; } // unexpected exit is never a clean job acknowledgement.
        }
        auto const error{release_native_stack(compiler_stack_)};
        if(error != 0) { ++cleanup_pending; return error; }
        ::std::shared_ptr<ThreadedGroup> retained;
        {
            ::std::lock_guard lock{mutex_};
            if(compiler_stack_charged_)
            { native_stack_reserved_ -= admission_->limits().compiler_stack_bytes; compiler_stack_charged_ = false; }
            retained = ::std::move(compiler_native_retention_); compiler_joined_ = true; changes_.notify_all();
        }
        return 0;
    }

    inline bool ThreadedGroup::CompilerLease::owns_current_native_thread() const noexcept
    {
        if(group_ == nullptr || job_ == nullptr) { return false; }
        ::std::lock_guard lock{group_->mutex_};
        return group_->compiler_created_ && group_->compiler_current_ == job_ && job_->sequence == sequence_ &&
            job_->phase == compiler_job_phase::running && ::pthread_equal(group_->compiler_thread_, ::pthread_self());
    }
    inline auto ThreadedGroup::CompilerLease::actual_reservation() const noexcept -> SpawnReservation&
    { if(!owns_current_native_thread()) { ::std::terminate(); } return job_->reservation; }
    inline ::std::size_t ThreadedGroup::CompilerLease::native_stack_usable_bytes() const noexcept
    { return owns_current_native_thread() ? group_->compiler_stack_.usable_bytes() : 0u; }
    inline ::std::size_t ThreadedGroup::CompilerLease::guest_native_stack_usable_bytes() const noexcept
    {
        if(!owns_current_native_thread()) { return 0u; }
        ::std::lock_guard lock{group_->mutex_};
        auto const& reservation{job_->reservation};
        if(reservation.group_ != group_ || reservation.position_ >= group_->admission_->limits().max_active_children + 1u) { return 0u; }
        auto const& selected{group_->slots_[reservation.position_]};
        return selected.phase == slot_phase::preparing && selected.tid == reservation.tid_ &&
            selected.stack.preparation_error == 0 ? selected.stack.usable_bytes() : 0u;
    }
    inline auto ThreadedGroup::CompilerLease::launch(::std::unique_ptr<ChildWork> work, ::std::uint32_t argument) noexcept -> launch_result
    {
        if(!owns_current_native_thread()) { return {-1, EINVAL}; }
        return group_->launch(::std::move(job_->reservation), ::std::move(work), argument);
    }
    inline auto ThreadedGroup::CompilerTicket::wait_until(::std::chrono::steady_clock::time_point deadline) const noexcept -> compiler_observation
    { return group_ ? group_->observe_compilation(position_, sequence_, deadline) : compiler_observation{}; }
    inline void ThreadedGroup::CompilerTicket::reset() noexcept
    {
        if(group_ != nullptr) { group_->abandon_compilation(position_, sequence_); group_ = nullptr; }
        retained_group_.reset();
    }

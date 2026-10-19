// Included within the actual bridge class. Ordinary runtime code is never an
// executable fallback. Work owns its concrete engine, World and all raw buffers.
    private:
        enum class backend : unsigned { integer, llvm };
        struct main_result
        {
            ::std::vector<::std::byte> bytes;
            ::std::atomic<bool> returned{};
        };
        struct profile_state
        {
            ::std::mutex retirement;
            ::uwvm2::utils::thread::execution_domain::private_generation_gate gate;
            ::std::shared_ptr<admission const> admitted;
            ::std::shared_ptr<group> native;
            ::std::shared_ptr<main_result> result;
            group::CompilerTicket main_preparation;
            ::uwvm2::uwvm::runtime::full::full_source_destruction_completion::owner old_source_completion;
            backend selected{};
            ::std::uint_least64_t generation{};
            bool joined{}, ordinary_reopened{}, requires_reset{};
            ::uwvm2::runtime::wasm_threads::group_outcome outcome{};
        };
        // Cold typed cancellation publication for providers without atomic<shared_ptr>.
        // These existing acquire/release/acq_rel accesses synchronize through this
        // independent mutex. It acquires no registry/Group lock and invokes no
        // callback or last-owner release. Ordinary Host context, not signal-safe.
        class stop_owner final
        {
            ::std::mutex mutex_;
            ::std::shared_ptr<group> value_;
        public:
            void store(::std::shared_ptr<group> next, ::std::memory_order) noexcept
            {
                {
                    ::std::lock_guard lock{mutex_};
                    value_.swap(next);
                }
                // The previous owner in next is released after the lock scope.
            }
            [[nodiscard]] ::std::shared_ptr<group> load(::std::memory_order) noexcept
            {
                ::std::shared_ptr<group> result;
                {
                    ::std::lock_guard lock{mutex_};
                    result = value_;
                }
                return result; // Retained owner; caller invokes Group only unlocked.
            }
            [[nodiscard]] ::std::shared_ptr<group> exchange(::std::shared_ptr<group> next, ::std::memory_order) noexcept
            {
                {
                    ::std::lock_guard lock{mutex_};
                    value_.swap(next);
                }
                return next; // Old owner moves to the caller after unlocking.
            }
        };
        struct control
        {
            ::std::mutex registry;
            ::std::shared_ptr<profile_state> profile;
            stop_owner stop_target;
            ::std::atomic_bool builtin_requested{};
            ::uwvm2::runtime::wasm_threads::threaded_profile_limits configured{};
            ::std::uint_least64_t blocked_generation{};
        };
        [[nodiscard]] static control& control_owner() noexcept
        {
            // Fixed process-control storage. It has no automatic static
            // destructor ordering dependency on g_runtime. Runtime shutdown
            // explicitly joins and clears every owned resource before its own
            // provider/source/domain destructors execute.
            alignas(control) static ::std::byte storage[sizeof(control)];
            static control* const actual{::new (static_cast<void*>(storage)) control{}};
            return *actual;
        }

        class concrete_work final : public group::ChildWork
        {
            world::owner world_;
            ::std::shared_ptr<admission const> admission_;
            ::std::unique_ptr<::uwvm2::runtime::compiler::uwvm_int::threaded::engine> integer_;
#if defined(UWVM_RUNTIME_LLVM_JIT)
            ::std::unique_ptr<ThreadedLlvmChild> llvm_;
#endif
            ::std::shared_ptr<main_result> result_;
            ::std::vector<::std::byte> parameters_;
            ::std::size_t entry_{};
            bool main_{};
        public:
            concrete_work(world::owner instance, ::std::shared_ptr<main_result> output,
                ::std::size_t entry, ::std::span<::std::byte const> parameters)
                : world_{::std::move(instance)}, admission_{world_->admission().retain()},
                  result_{::std::move(output)}, entry_{entry}, main_{!!result_}
            { parameters_.resize(parameters.size()); if(!parameters.empty()) { ::std::memcpy(parameters_.data(), parameters.data(), parameters.size()); } }
            [[nodiscard]] auto retained_world() const noexcept -> world::owner override { return world_; }
            [[nodiscard]] auto retained_admission() const noexcept -> ::std::shared_ptr<admission const> override { return admission_; }
            [[nodiscard]] ::std::size_t owned_storage_bytes() const noexcept override
            {
                auto const limit{admission_->limits().max_child_storage_bytes};
                ::std::size_t sum{world_->owned_storage_bytes()};
                auto add{[&](::std::size_t n) noexcept { if(sum > limit || n > limit - sum) { sum = SIZE_MAX; } else { sum += n; } }};
                add(sizeof(concrete_work)); add(parameters_.capacity());
                if(integer_) { add(integer_->owned_storage_bytes()); }
#if defined(UWVM_RUNTIME_LLVM_JIT)
                if(llvm_) { add(llvm_->owned_storage_bytes()); }
#endif
                return sum;
            }
            [[nodiscard]] ::std::size_t owned_code_bytes() const noexcept override
            {
                if(integer_) { return integer_->owned_code_bytes(); }
#if defined(UWVM_RUNTIME_LLVM_JIT)
                if(llvm_) { return llvm_->owned_code_bytes(); }
#endif
                return SIZE_MAX;
            }
            [[nodiscard]] bool prepare(backend selected, group::CompilerLease const& compiler) noexcept
            {
                if(!compiler.owns_current_native_thread() || &compiler.admission() != admission_.get() ||
                   compiler.cancellation().stop_requested()) { return false; }
                auto const& limits{admission_->limits()};
                auto const before{owned_storage_bytes()};
                if(before >= limits.max_child_storage_bytes) { return false; }
                if(selected == backend::integer)
                {
                    namespace integer = ::uwvm2::runtime::compiler::uwvm_int::threaded;
                    integer::limits budget{};
                    budget.storage_bytes = limits.max_child_storage_bytes - before;
                    budget.source_bytes = limits.max_child_code_bytes;
                    budget.instruction_bytes = limits.max_child_code_bytes;
                    budget.values = limits.max_value_stack_bytes / sizeof(::std::uint64_t);
                    budget.controls = limits.max_control_frames;
                    budget.calls = limits.max_call_frames;
                    budget.functions = world_->module(0u)->imported_function_vec_storage.size() +
                        world_->module(0u)->local_defined_function_vec_storage.size();
                    budget.host_bytes = limits.max_value_stack_bytes;
                    budget.exception_objects = limits.max_call_frames;
                    budget.exception_values = budget.values;
                    ::uwvm2::validation::error::code_validation_error_impl error{};
                    auto built{integer::engine::compile(world_, budget, error)};
                    if(!built.code || !built.outcome.normal()) { return false; }
                    integer_ = ::std::move(built.code);
                }
#if defined(UWVM_RUNTIME_LLVM_JIT)
                else
                {
                    auto built{ThreadedLlvmChild::prepare(world_, compiler, limits.max_child_storage_bytes - before)};
                    if(!built.child || built.status != ThreadedLlvmChild::preparation_status::ok) { return false; }
                    llvm_ = ::std::move(built.child);
                }
#else
                else { return false; }
#endif
                return owned_storage_bytes() <= limits.max_child_storage_bytes && owned_code_bytes() <= limits.max_child_code_bytes;
            }
            [[nodiscard]] bool refresh_after_actual_parent_active() noexcept
            {
                if(integer_) { return true; } // actual table cells read directly, no cached raw table view
#if defined(UWVM_RUNTIME_LLVM_JIT)
                if(llvm_) { return llvm_->refresh_table_views_before_launch(); }
#endif
                return false;
            }
            [[nodiscard]] group::child_completion execute(lease& actual, ::std::size_t entry,
                ::std::span<::std::byte> results, ::std::span<::std::byte const> parameters) noexcept
            {
                if(integer_)
                {
                    auto const result{integer_->run_entry(actual, entry, results, parameters)};
                    if(result.normal()) { return group::child_completion::returned; }
                    return actual.cancellation().stop_requested() ? group::child_completion::group_stopped : group::child_completion::rejected;
                }
#if defined(UWVM_RUNTIME_LLVM_JIT)
                if(llvm_)
                {
                    auto const result{llvm_->run_entry(actual, entry, results, parameters)};
                    if(result == ThreadedLlvmChild::run_status::completed) { return group::child_completion::returned; }
                    return result == ThreadedLlvmChild::run_status::group_stopped ? group::child_completion::group_stopped : group::child_completion::rejected;
                }
#endif
                return group::child_completion::rejected;
            }
            [[nodiscard]] group::child_completion run(lease& actual, ::std::uint32_t tid, ::std::uint32_t argument) noexcept override
            {
                if(!actual_lease(actual, *world_) || (main_ ? tid != 0u : tid == 0u)) { return group::child_completion::rejected; }
                auto const initialization{world_->initialization_function_index()};
                if(initialization)
                {
                    auto const initialized{execute(actual, *initialization, {}, {})};
                    if(initialized != group::child_completion::returned) { return initialized; }
                }
                if(main_)
                {
                    auto const result{initialization && *initialization == entry_ ? group::child_completion::returned :
                        execute(actual, entry_, result_->bytes, parameters_)};
                    if(result == group::child_completion::returned)
                    { result_->returned.store(true, ::std::memory_order_release); finish_main(actual); }
                    return result;
                }
                ::std::array<::std::uint32_t, 2u> parameters{tid, argument};
                return execute(actual, world_->thread_start_function_index(), {}, ::std::as_bytes(::std::span{parameters}));
            }
        };
        static void finish_main(lease& actual) noexcept
        { actual.group_->request_stop({::uwvm2::runtime::wasm_threads::termination_kind::main_return, 0u, 0u, 0u, 0u}); }

        [[nodiscard]] static ::std::unique_ptr<concrete_work> prepare_work(profile_state const& state,
            group::CompilerLease const& compiler, bool main, ::std::size_t entry, ::std::span<::std::byte const> parameters) noexcept
        {
            if(!compiler.owns_current_native_thread() || compiler.retained_admission() != state.admitted ||
               compiler.cancellation().stop_requested()) { return {}; }
            try
            {
                if(!actual_source_providers(*state.admitted)) { return {}; }
                auto created{main ? world::borrow_actual_parent(*state.admitted) : world::create(*state.admitted)};
                if(!created.world) { return {}; }
                auto work{::std::make_unique<concrete_work>(::std::move(created.world),
                    main ? state.result : nullptr, entry, parameters)};
                if(!work->prepare(state.selected, compiler) || compiler.cancellation().stop_requested()) { return {}; }
                return work;
            }
            catch(::std::bad_alloc const&) { return {}; }
            catch(...) { return {}; }
        }

        class concrete_compile_work final : public group::CompilerWork
        {
            ::std::shared_ptr<profile_state> state_;
            ::std::vector<::std::byte> parameters_;
            ::std::size_t entry_{};
            ::std::uint32_t argument_{};
            bool main_{};
        public:
            concrete_compile_work(::std::shared_ptr<profile_state> state, bool main, ::std::size_t entry,
                ::std::uint32_t argument, ::std::span<::std::byte const> parameters)
                : state_{::std::move(state)}, entry_{entry}, argument_{argument}, main_{main}
            {
                // Submission never borrows a Guest/Host raw span beyond return.
                parameters_.resize(parameters.size());
                if(!parameters.empty()) { ::std::memcpy(parameters_.data(), parameters.data(), parameters.size()); }
            }
            [[nodiscard]] auto retained_admission() const noexcept -> ::std::shared_ptr<admission const> override
            { return state_->admitted; }
            [[nodiscard]] ::std::size_t owned_submission_bytes() const noexcept override
            { return parameters_.capacity() <= SIZE_MAX - sizeof(concrete_compile_work) ?
                sizeof(concrete_compile_work) + parameters_.capacity() : SIZE_MAX; }
            [[nodiscard]] group::compiler_result run(group::CompilerLease& actual) noexcept override
            {
                using completion = group::compiler_completion;
                auto fail{[&](completion why, int error) noexcept -> group::compiler_result
                {
                    if(main_)
                    { state_->native->request_stop({::uwvm2::runtime::wasm_threads::termination_kind::native_failure,
                        static_cast<::std::uint32_t>(error), 0u, entry_, 0u}); }
                    return {why, {}, error};
                }};
                if(!actual.owns_current_native_thread() || actual.retained_admission() != state_->admitted)
                { return fail(completion::rejected, EINVAL); }
                if(actual.cancellation().stop_requested()) { return {completion::cancelled, {}, 0}; }
                auto work{prepare_work(*state_, actual, main_, entry_, parameters_)};
                if(!work) { return fail(actual.cancellation().stop_requested() ? completion::cancelled : completion::rejected, EINVAL); }
                if(main_)
                {
                    auto const initialized{::uwvm2::uwvm::runtime::initializer::ThreadedMainActiveSegments::apply(
                        *state_->admitted, work->retained_world(), *state_->native, actual.actual_reservation())};
                    if(initialized != ::uwvm2::uwvm::runtime::initializer::threaded_main_active_status::applied ||
                       !work->refresh_after_actual_parent_active()) { return fail(completion::rejected, EINVAL); }
                }
                if(actual.cancellation().stop_requested()) { return {completion::cancelled, {}, 0}; }
                auto const launched{actual.launch(::std::move(work), argument_)};
                if(launched.tid < 0) { return fail(completion::native_failure, launched.native_error != 0 ? launched.native_error : EINVAL); }
                return {completion::launched, launched, 0};
            }
        };

    public:
        [[nodiscard]] static threaded_management_exclusion try_management(threaded_management_kind purpose) noexcept
        {
            if(current_host_ != nullptr || get_runtime_execution_entry_depth() != 0u ||
               get_runtime_state_publication_depth() != 0u || get_runtime_compilation_metadata_callback_depth() != 0u)
            { return {}; }
            ::std::unique_lock registry{control_owner().registry, ::std::try_to_lock};
            if(!registry.owns_lock()) { return {}; }
            auto const& state{control_owner().profile};
            if(state && (!state->joined || state->native || state->admitted ||
                (state->requires_reset && purpose == threaded_management_kind::incremental_mutation))) { return {}; }
            auto maintenance{g_runtime.execution_domain.try_maintenance_transition()};
            if(!maintenance) { return {}; }
            source::owner observed;
            {
                runtime_state_publication_guard publication{::std::try_to_lock};
                if(!publication) { return {}; }
                observed = ::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin();
                if(observed && !source::has_canonical_owner(observed)) { return {}; }
            }
            auto completion{observed ? observed->destruction_completion() :
                ::uwvm2::uwvm::runtime::full::full_source_destruction_completion::owner{}};
            return threaded_management_exclusion{::std::move(registry), observed, ::std::move(completion),
                current_runtime_generation(), purpose, state && state->requires_reset};
        }
        [[nodiscard]] static threaded_management_exclusion try_management_under_publication(threaded_management_kind purpose) noexcept
        {
            // Actual same-thread Runtime guard owns the real atomic publication
            // lock and its private TLS depth. No external bool/label is accepted.
            if(current_host_ != nullptr || get_runtime_state_publication_depth() == 0u ||
               get_runtime_execution_entry_depth() != 0u || get_runtime_compilation_metadata_callback_depth() != 0u)
            { return {}; }
            ::std::unique_lock registry{control_owner().registry, ::std::try_to_lock};
            if(!registry.owns_lock()) { return {}; }
            auto const& state{control_owner().profile};
            if(state && (!state->joined || state->native || state->admitted ||
                (state->requires_reset && purpose == threaded_management_kind::incremental_mutation))) { return {}; }
            auto const observed{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
            if(observed && !source::has_canonical_owner(observed)) { return {}; }
            auto completion{observed ? observed->destruction_completion() :
                ::uwvm2::uwvm::runtime::full::full_source_destruction_completion::owner{}};
            return threaded_management_exclusion{::std::move(registry), observed, ::std::move(completion),
                current_runtime_generation(), purpose, state && state->requires_reset};
        }
        [[nodiscard]] static bool recheck_management(threaded_management_exclusion const& actual,
            threaded_management_kind purpose) noexcept
        {
            // Caller already holds its original exclusion before pause/Host
            // admission/publication. No lock, wait, allocation or join here.
            if(!actual || actual.actual_mutex_ != &control_owner().registry || actual.purpose_ != purpose ||
               actual.observed_generation_ != current_runtime_generation()) { return false; }
            auto const& state{control_owner().profile};
            if(state && (!state->joined || state->native || state->admitted ||
                (state->requires_reset && purpose == threaded_management_kind::incremental_mutation))) { return false; }
            auto const selected{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
            if(!selected) { return actual.observed_source_.expired() && !actual.source_completion_; }
            return !actual.observed_source_.owner_before(selected) && !selected.owner_before(actual.observed_source_) &&
                source::has_canonical_owner(selected);
        }
        [[nodiscard]] static bool source_destruction_completed(threaded_management_exclusion const& actual) noexcept
        {
            if(!actual || actual.actual_mutex_ != &control_owner().registry ||
               actual.purpose_ != threaded_management_kind::full_source_reset) { return false; }
            auto const& state{control_owner().profile};
            if(state && (!state->joined || state->native || state->admitted)) { return false; }
            bool const original{!state || !state->requires_reset ||
                (state->old_source_completion && state->old_source_completion->completed())};
            return original && (!actual.requires_reset_ || !actual.source_completion_ || actual.source_completion_->completed());
        }
        [[nodiscard]] static threaded_policy_status policy(::uwvm2::runtime::wasm_threads::threaded_profile_limits& out) noexcept
        {
            ::std::unique_lock registry{control_owner().registry, ::std::try_to_lock};
            if(!registry.owns_lock()) { return threaded_policy_status::busy; }
            out = control_owner().configured;
            return out.max_active_children != 0u ? threaded_policy_status::configured : threaded_policy_status::disabled;
        }
        [[nodiscard]] static bool builtin_requested() noexcept
        { return control_owner().builtin_requested.load(::std::memory_order_acquire); }
        [[nodiscard]] static bool configure(::uwvm2::runtime::wasm_threads::threaded_profile_limits const& limits) noexcept
        {
            if(!group::limits_admitted(limits)) { return false; }
            ::std::unique_lock registry{control_owner().registry, ::std::try_to_lock};
            if(!registry.owns_lock()) { return false; }
            if(control_owner().profile || control_owner().blocked_generation == current_runtime_generation()) { return false; }
            bool const accepted{g_runtime.execution_domain.with_quiescent_configuration([&]() noexcept { control_owner().configured = limits; })};
            if(accepted) { control_owner().builtin_requested.store(true, ::std::memory_order_release); }
            return accepted;
        }
        [[nodiscard]] static threaded_main_status start_main(::uwvm2::utils::container::u8string_view name,
            ::std::size_t entry, ::std::span<::std::byte const> parameters, ::std::size_t result_bytes) noexcept
        {
            ::std::shared_ptr<profile_state> state;
            group::SpawnReservation reservation;
            {
                ::std::unique_lock registry{control_owner().registry, ::std::try_to_lock};
                if(!registry.owns_lock()) { return threaded_main_status::rejected; }
                if(control_owner().configured.max_active_children == 0u) { return threaded_main_status::disabled; }
                if(control_owner().profile || control_owner().blocked_generation == current_runtime_generation() || current_host_ != nullptr ||
                   parameters.size() > control_owner().configured.max_value_stack_bytes || result_bytes > control_owner().configured.max_value_stack_bytes)
                { return threaded_main_status::rejected; }
                auto maintenance{g_runtime.execution_domain.try_maintenance_transition()};
                if(!maintenance) { return threaded_main_status::rejected; }
                try
                {
                    state = ::std::make_shared<profile_state>();
                    state->gate = g_runtime.execution_domain.suspend_public_admission(maintenance);
                    if(!state->gate) { throw ::std::bad_alloc{}; }
                    // Public admission is really closed and the current native
                    // generation has no active ordinary reader/compiler. Only
                    // now may startup read source/record/body state without racing
                    // ordinary compilation. Other mutation holds this same cold
                    // exclusion or fails its nonwaiting commit recheck.
                    auto const owner{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
                    if(!source::has_canonical_owner(owner)) { throw ::std::bad_alloc{}; }
                    auto const* parent{owner->initialized_main_module()};
                    auto const id{owner->bound_initialized_main_module_id()};
                    auto const generation{current_runtime_generation()};
                    auto const validation{owner->actual_full_validation_epoch()};
                    namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
                    if(parent == nullptr || id >= g_runtime.modules.size() || validation != generation ||
                       mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
                       g_runtime.modules.index_unchecked(id).runtime_module != parent ||
                       g_runtime.modules.index_unchecked(id).module_name != name ||
                       owner->actual_validated_file(id, validation, parent) != &owner->file() ||
                       (!parameters.empty() && parameters.data() == nullptr)) { throw ::std::bad_alloc{}; }
#if defined(UWVM_RUNTIME_LLVM_JIT)
                    // The first profile has no borrowed ordinary debugger or
                    // checkpoint execution state. Native transactions must use
                    // their real exclusion and a complete Source reset first.
                    if(g_runtime.debug_pause_control || g_runtime.checkpoint_profile ||
                       g_runtime.checkpoint_host_enabled.load(::std::memory_order_acquire)) { throw ::std::bad_alloc{}; }
#endif
#if defined(UWVM_IMPORT_WASI_WASIP1) && !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1)
                    if(::uwvm2::uwvm::wasm::storage::preload_expose_wasip1_host_api) { throw ::std::bad_alloc{}; }
#endif
                    auto const imported{parent->imported_function_vec_storage.size()};
                    if(entry < imported || entry - imported >= parent->local_defined_function_vec_storage.size()) { throw ::std::bad_alloc{}; }
                    auto const* entry_type{parent->local_defined_function_vec_storage.index_unchecked(entry - imported).function_type_ptr};
                    ::std::size_t parameter_bytes{}, output_bytes{};
                    if(!entry_type || !numeric_abi(*entry_type, *entry_type, parameter_bytes, output_bytes) ||
                       parameters.size() != parameter_bytes || result_bytes != output_bytes) { throw ::std::bad_alloc{}; }
                    state->result = ::std::make_shared<main_result>(); state->result->bytes.resize(result_bytes);
                    state->selected = backend::integer;
#if defined(UWVM_RUNTIME_LLVM_JIT)
                    if(runtime_compiler_requests_llvm_jit_translation()) { state->selected = backend::llvm; }
#endif
                    state->generation = generation;
                    state->old_source_completion = owner->destruction_completion();
                    if(!state->old_source_completion || state->old_source_completion->completed()) { throw ::std::bad_alloc{}; }
                    auto actual_generation{state->gate.try_enter()};
                    if(!actual_generation) { throw ::std::bad_alloc{}; }
                    auto admitted{::std::shared_ptr<admission>{new admission{owner, *parent, id,
                        generation, validation, control_owner().configured, ::std::move(actual_generation)}}};
                    admitted->canonical_owner_ = admitted; state->admitted = ::std::move(admitted);
                    state->native = ::std::shared_ptr<group>{new group{state->admitted}};
                    state->native->canonical_owner_ = state->native;
                    if(state->native->initialization_error_ != 0) { throw ::std::bad_alloc{}; }
                    auto control_bytes{sizeof(profile_state) + sizeof(main_result) + sizeof(admission)};
                    if(state->result->bytes.capacity() > SIZE_MAX - control_bytes) { throw ::std::bad_alloc{}; }
                    control_bytes += state->result->bytes.capacity();
                    if(!state->native->charge_runtime_control_storage(control_bytes)) { throw ::std::bad_alloc{}; }
                    reservation = state->native->reserve_initial_main();
                    if(!reservation) { throw ::std::bad_alloc{}; }
                    control_owner().profile = state; // reachable BEFORE any cold compiler/provider callback
                    control_owner().stop_target.store(state->native, ::std::memory_order_release);
                }
                catch(...)
                {
                    reservation.cancel();
                    if(state)
                    { state->native.reset(); state->admitted.reset();
                      (void)g_runtime.execution_domain.reopen_public_admission(maintenance, state->gate); }
                    return threaded_main_status::rejected;
                }
            }
            // The true preparing slot/provider lease remains visible while the
            // host allocates a finite submission. The only code builder runs on
            // the real Group-owned compiler pthread, never a Guest parent stack.
            auto reject{[&]() noexcept
            {
                state->native->request_stop({::uwvm2::runtime::wasm_threads::termination_kind::native_failure, 1u, 0u, entry, 0u});
                reservation.cancel();
                (void)retire_impl(::std::chrono::steady_clock::now(), threaded_management_kind::stop_only, true);
                return threaded_main_status::rejected;
            }};
            if(state->native->start_compiler_worker() != 0) { return reject(); }
            ::std::unique_ptr<concrete_compile_work> job;
            try { job = ::std::make_unique<concrete_compile_work>(state, true, entry, 0u, parameters); }
            catch(...) { return reject(); }
            {
                // Publish the actual ticket before a concurrent manager can
                // report native retirement. No wait/compile/callback under Root.
                ::std::unique_lock registry{control_owner().registry, ::std::try_to_lock};
                if(!registry.owns_lock()) { job.reset(); return reject(); }
                if(control_owner().profile != state || state->joined) { return threaded_main_status::rejected; }
                auto submitted{state->native->submit_compilation(::std::move(reservation), ::std::move(job))};
                if(!submitted.ticket)
                {
                    registry.unlock();
                    return reject();
                }
                state->main_preparation = ::std::move(submitted.ticket);
            }
            // Running includes admitted asynchronous preparation. Its native
            // failure/stop/result is observed only by wait/result, never restart.
            return threaded_main_status::running;
        }

        [[nodiscard]] static threaded_retirement retire_impl(::std::chrono::steady_clock::time_point deadline,
            threaded_management_kind purpose, bool deliver_stop) noexcept
        {
            if(current_host_ != nullptr) { return {}; } // never join the native caller's own Group
            ::std::shared_ptr<profile_state> state;
            { ::std::unique_lock registry{control_owner().registry, ::std::try_to_lock};
              if(!registry.owns_lock()) { return {false, false, 0u, 0u, EBUSY}; }
              state = control_owner().profile; }
            if(!state) { return {true, false, 0u, 0u, 0}; }
            ::std::unique_lock retirement{state->retirement, ::std::try_to_lock};
            if(!retirement.owns_lock())
            {
                ::std::unique_lock registry{control_owner().registry, ::std::try_to_lock};
                if(!registry.owns_lock()) { return {false, false, 0u, 0u, EBUSY}; }
                bool const reset{state->native ?
                    (state->native->ever_launched_.load(::std::memory_order_acquire) ||
                     state->native->shared_effects_started_.load(::std::memory_order_acquire)) : state->requires_reset};
                return {false, reset, 0u, 0u, EBUSY};
            }
            if(state->joined)
            {
                if(state->requires_reset || state->ordinary_reopened) { return {true, state->requires_reset, 0u, 0u, 0}; }
                auto maintenance{g_runtime.execution_domain.try_maintenance_transition()};
                if(!maintenance) { return {true, false, 0u, 0u, EBUSY}; }
                ::std::lock_guard registry{control_owner().registry};
                if(control_owner().profile.get() != state.get() ||
                   !g_runtime.execution_domain.reopen_public_admission(maintenance, state->gate))
                { return {true, false, 0u, 0u, EBUSY}; }
                state->ordinary_reopened = true;
                return {true, false, 0u, 0u, 0};
            }
            if(deliver_stop) { state->native->request_stop({::uwvm2::runtime::wasm_threads::termination_kind::cancelled, 0u, 0u, 0u, 0u}); }
            {
                ::std::unique_lock registry{control_owner().registry, ::std::try_to_lock};
                if(!registry.owns_lock()) { return {false, false, 0u, 0u, EBUSY}; }
                // Abandoning a live compiler ticket requests actual Group stop.
                // A normal wait must therefore retain it until real main/Host
                // termination has already closed this Group. Compiler work stays
                // owned until its own body/TLS and native pthread physically join.
                if(state->native->stop_requested_.load(::std::memory_order_acquire)) { state->main_preparation.reset(); }
            }
            auto const joined{state->native->join_until(deadline)};
            bool const reset{state->native->ever_launched_.load(::std::memory_order_acquire) ||
                state->native->shared_effects_started_.load(::std::memory_order_acquire)};
            if(!joined.physically_joined)
            { return {false, reset, joined.preparing, joined.native_pending, joined.native_error,
                joined.compiler_tickets_pending, joined.native_cleanup_pending}; }
            auto maintenance{g_runtime.execution_domain.try_maintenance_transition()};
            if(!maintenance) { return {false, reset, 0u, 0u, EBUSY}; }
            ::std::lock_guard registry{control_owner().registry};
            if(control_owner().profile.get() != state.get()) { return {}; }
            state->outcome = state->native->read_outcome(); state->requires_reset = reset;
            // True join destroyed every slot's engine/World. Release the real
            // provider-generation pins BEFORE any ordinary domain drain/reset.
            auto stop_target{control_owner().stop_target.exchange({}, ::std::memory_order_acq_rel)};
            stop_target.reset(); state->native.reset(); state->admitted.reset(); state->joined = true;
            if(reset) { control_owner().blocked_generation = state->generation; }
            else if(!g_runtime.execution_domain.reopen_public_admission(maintenance, state->gate))
            { return {true, false, 0u, 0u, EBUSY}; }
            else { state->ordinary_reopened = true; }
            (void)purpose; // caller MUST inspect permits(purpose); no mutation ACK is synthesized
            return {true, reset, 0u, 0u, 0};
        }

        [[nodiscard]] static threaded_retirement retire(::std::chrono::steady_clock::time_point deadline,
            threaded_management_kind purpose) noexcept
        { return retire_impl(deadline, purpose, true); }
        [[nodiscard]] static threaded_retirement wait(::std::chrono::steady_clock::time_point deadline) noexcept
        { return retire_impl(deadline, threaded_management_kind::stop_only, false); }
        static void request_stop() noexcept
        {
            // Independent typed cancellation route; no recursive cold-registry
            // lock when a genuine management exclusion is already held. This
            // owner grants neither execution nor source/provider permission.
            auto actual{control_owner().stop_target.load(::std::memory_order_acquire)};
            if(actual) { actual->request_stop({::uwvm2::runtime::wasm_threads::termination_kind::cancelled, 0u, 0u, 0u, 0u}); }
        }
        [[nodiscard]] static threaded_main_status result(::std::span<::std::byte> bytes,
            ::std::uint32_t* code, unsigned* kind) noexcept
        {
            ::std::unique_lock registry{control_owner().registry, ::std::try_to_lock};
            if(!registry.owns_lock()) { return threaded_main_status::retirement_pending; }
            auto const& state{control_owner().profile};
            if(!state) { return threaded_main_status::disabled; }
            if(!state->joined) { return threaded_main_status::retirement_pending; }
            if(bytes.size() != state->result->bytes.size() || code == nullptr || kind == nullptr) { return threaded_main_status::rejected; }
            if(state->result->returned.load(::std::memory_order_acquire) && !bytes.empty())
            { ::std::memcpy(bytes.data(), state->result->bytes.data(), bytes.size()); }
            *code = state->outcome.code; *kind = static_cast<unsigned>(state->outcome.kind);
            return threaded_main_status::finished;
        }
        static void after_actual_source_reset() noexcept
        {
            ::std::shared_ptr<profile_state> retired;
            { ::std::lock_guard registry{control_owner().registry};
              auto const& state{control_owner().profile};
              if(!state || !state->joined || state->native || state->admitted || state->generation == current_runtime_generation() ||
                 (state->requires_reset && (!state->old_source_completion || !state->old_source_completion->completed()))) { return; }
              retired = ::std::move(control_owner().profile); control_owner().blocked_generation = 0u; }
            // Gate/control/result destruction outside all Runtime publication
            // locks. The old Source/NativeMemory has already been destroyed by
            // the genuine full reset; an epoch setter cannot invoke this API.
        }
        static void after_actual_source_reset(threaded_management_exclusion const& actual) noexcept
        {
            if(!source_destruction_completed(actual)) { return; }
            auto const& state{control_owner().profile};
            if(!state || !state->joined || state->generation == current_runtime_generation()) { return; }
            auto retired{::std::move(control_owner().profile)};
            control_owner().blocked_generation = 0u;
        }
        [[nodiscard]] static bool source_destruction_completed() noexcept
        {
            ::std::lock_guard registry{control_owner().registry};
            auto const& state{control_owner().profile};
            if(!state) { return true; }
            return state->joined && (!state->requires_reset ||
                (state->old_source_completion && state->old_source_completion->completed()));
        }
        static void process_shutdown() noexcept
        {
            // Runtime-global destructor invokes this BEFORE destroying its real
            // execution domain/providers/source. Compiler preparation may still
            // be pending; never claim a timeout/body-return as physical join.
            for(;;)
            {
                auto const retired{retire_impl(::std::chrono::steady_clock::now() + ::std::chrono::milliseconds{100},
                    threaded_management_kind::stop_only, true)};
                if(retired.physically_joined && retired.native_error == 0) { break; }
                if(retired.native_error != 0 && retired.native_error != EBUSY) { ::std::terminate(); }
            }
            ::std::shared_ptr<profile_state> retired;
            { ::std::lock_guard registry{control_owner().registry}; retired = ::std::move(control_owner().profile); }
        }

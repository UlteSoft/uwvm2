// Runtime-private near-call continuation. No public constructor, requested PC,
// target or register snapshot. Only the actual canonical physical trap joins
// its immutable caller body and the exact pre-execution kernel context. NI
// may run an opaque host/VM call normally, with TF off, but supplies no callee
// code/register/stack/memory access. Cross-Wasm SI has a separate typed issuer.
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    class llvm_jit_debug_native_call_continuation final
    {
        llvm_jit_debug_native_call_continuation() = default;
        llvm_jit_debug_activation_capture_owner capture_{};
        llvm_jit_debug_native_activation_cursor_owner cursor_{};
        void const* session_{};
        llvm_jit_debug_native_call_continuation_view view_{};
        // Comparison-only scalars. They are NEVER converted to borrowed code,
        // native memory or stack pointers. Every query reselects actual owners
        // under the real publication guard and actual worker execution lease.
        void const* caller_owner_{}, *callee_owner_{};
        ::std::uintptr_t callee_target_{}, callee_begin_{}, callee_end_{};
        ::std::size_t register_index_{SIZE_MAX}; // private exact MC operand, never caller data
        ::std::size_t call_size_{};
        ::std::uintptr_t slot_address_{}; // comparison only; NEVER cast/dereferenced
        ::std::uint64_t trap_revision_{}; // actual kernel sequence; never supplied by caller
        ::std::array<::std::uint64_t, 38u> origin_gpr_{}; // private actual trap only, including the original SP
        ::std::uintptr_t origin_sp_{};
        llvm_jit_debug_native_breakpoint_plan software_plan_{};
#if defined(__linux__) && defined(__arm__)
        ::fast_io::string instruction_identity_{};
#endif
        bool opaque_call_{}; // runtime-minted NI fallthrough only; never a callee ownership grant
        ::std::uint_least64_t callee_module_{}, callee_function_{}, callee_generation_{};
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_mint_native_call_continuation_host_api(
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_native_activation_cursor_owner const&,
            void const*) noexcept -> llvm_jit_debug_native_call_continuation_owner;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_native_call_continuation_host_api(
            llvm_jit_debug_native_call_continuation_owner const&, void const*,
            llvm_jit_debug_native_call_continuation_view&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_resume_native_call_event_host_api(
            llvm_jit_debug_native_call_continuation_owner const&, void const*, void*,
            llvm_jit_debug_native_call_event_resume_callback) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_resume_native_call_continuation_host_api(
            llvm_jit_debug_native_call_continuation_owner const&, void const*, void*,
            llvm_jit_debug_native_call_resume_callback) noexcept -> bool;
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE) && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        ::uwvm2::uwvm::debugger::native_owned_instruction_semantics::call_memory_operand memory_operand_{};
        struct observation
        {
            llvm_jit_debug_native_call_continuation_view view{};
            ::std::uint64_t trap_revision{};
            ::std::uintptr_t stack{};
            ::std::array<::std::uint64_t, 38u> gpr{};
            void const* caller_owner{}, *callee_owner{};
            ::std::uintptr_t callee_begin{}, callee_end{}, callee_target{}, slot_address{};
            ::std::uint_least64_t callee_module{}, callee_function{}, callee_generation{};
        };
        // Cold bounded DATA join, ONLY inside observe's real publication guard.
        // A raw adapter/helper/descriptor/unknown alias cannot satisfy the true
        // role1 text boundary plus actual currently selected typed entry slot.
        [[nodiscard]] static bool find_actual_typed_callee(::std::uintptr_t target,
            ::std::uint_least64_t epoch, observation& output) noexcept
        {
            if(target == 0u || get_runtime_state_publication_depth() != 1u ||
               epoch == 0u || epoch != current_runtime_generation() || g_runtime.modules.size() > 262144u) { return false; }
            ::std::size_t inspected{};
            bool found{};
            for(auto& record : g_runtime.modules)
            {
                auto const* module{record.runtime_module};
                if(module == nullptr || !record.llvm_jit_ready) { continue; }
                auto const count{module->local_defined_function_vec_storage.size()};
                auto const imports{module->imported_function_vec_storage.size()};
                if(count > 262144u - inspected || imports > SIZE_MAX - count) { return false; }
                inspected += count; // Checked before addition; bounded total cold work.
                if(record.llvm_jit_debug_full_typed_entry_targets.empty()) { continue; }
                if(record.llvm_jit_debug_full_typed_entry_targets.size() != count ||
                   record.llvm_jit_local_entry_addresses.size() != count ||
                   record.llvm_jit_debug_full_entry_generations.size() != count) { return false; }
                for(::std::size_t local{}; local != count; ++local)
                {
                    // [real current typed slots0..count][local/gen arrays0..count]
                    // [safe] equal complete extents checked BEFORE local lookup.
                    // The acquire reads the same real fixed patchable slot that
                    // generated debug-full Wasm calls use; no numeric memory read.
                    auto& slot{record.llvm_jit_debug_full_typed_entry_targets.index_unchecked(local)};
                    auto const actual_target{::std::atomic_ref<::std::uintptr_t>{slot}.load(::std::memory_order_acquire)};
                    if(actual_target != target) { continue; }
                    if(found || record.llvm_jit_local_entry_addresses.index_unchecked(local) != target) { return false; }
                    auto const id{find_runtime_module_id_from_storage_ptr(module)};
                    auto const function{imports + local}; // local<count and imports+count checked.
                    auto const generation{record.llvm_jit_debug_full_entry_generations[local]};
                    ::std::uintptr_t begin{}, end{};
                    if(id == SIZE_MAX || generation == 0u ||
                       !debug_resolve_actual_native_function_body(id, function, generation, epoch, target, begin, end) ||
                       begin != target || end <= begin) { return false; }
                    auto const owner{debug_activation_current_code_owner(record, id, function, local, generation)};
                    if(owner == nullptr) { return false; }
                    output.callee_owner = owner; output.callee_begin = begin; output.callee_end = end;
                    output.callee_module = id; output.callee_function = function; output.callee_generation = generation;
                    found = true;
                }
            }
            return found;
        }
        [[nodiscard]] static bool derive_call_slot(
            ::uwvm2::uwvm::debugger::native_owned_instruction_semantics::call_memory_operand const& operand,
            ::std::array<::std::uint64_t,38u> const& registers,::std::uintptr_t pc,::std::size_t size,
            ::std::uintptr_t& output) noexcept
        {
            output=0u;if(!operand.valid || size==0u || (operand.scale!=1u && operand.scale!=2u && operand.scale!=4u && operand.scale!=8u))
            { return false; }
            ::std::uintptr_t address{};
            if(operand.rip_relative) { if(size>UINTPTR_MAX-pc) { return false; } address=pc+size; }
            else if(operand.base!=SIZE_MAX)
            { if(operand.base>=registers.size() || registers[operand.base]>UINTPTR_MAX) { return false; } address=registers[operand.base]; }
            if(operand.index!=SIZE_MAX)
            {
                if(operand.index>=registers.size() || registers[operand.index]>UINTPTR_MAX/operand.scale) { return false; }
                auto const index{static_cast<::std::uintptr_t>(registers[operand.index])*operand.scale};
                if(index>UINTPTR_MAX-address) { return false; } address+=index;
            }
            if(operand.displacement<0)
            {
                auto const amount{static_cast<::std::uint64_t>(-(operand.displacement+1))+1u};
                if(amount>address) { return false; } address-=static_cast<::std::uintptr_t>(amount);
            }
            else
            {
                auto const amount{static_cast<::std::uint64_t>(operand.displacement)};
                if(amount>UINTPTR_MAX-address) { return false; } address+=static_cast<::std::uintptr_t>(amount);
            }
            output=address;return address!=0u;
        }
        [[nodiscard]] static bool actual_typed_slot_target(::std::uintptr_t address,::std::uint_least64_t epoch,
            ::std::uintptr_t& target) noexcept
        {
            target=0u;
            if(address==0u || get_runtime_state_publication_depth()!=1u || epoch==0u || epoch!=current_runtime_generation() ||
               g_runtime.modules.size()>262144u) { return false; }
            ::std::size_t total{};bool found{};
            for(auto& record:g_runtime.modules)
            {
                auto const* module{record.runtime_module};
                if(module==nullptr || !record.llvm_jit_ready) { continue; }
                auto const count{module->local_defined_function_vec_storage.size()};
                if(count>262144u-total) { return false; } total+=count;
                if(record.llvm_jit_debug_full_typed_entry_targets.empty()) { continue; }
                if(record.llvm_jit_debug_full_typed_entry_targets.size()!=count) { return false; }
                for(::std::size_t n{};n!=count;++n)
                {
                    // Compare scalar address to a genuine currently installed
                    // typed Wasm slot BEFORE reading that owned slot. The decoded
                    // effective address is NEVER converted to a native pointer.
                    auto& slot{record.llvm_jit_debug_full_typed_entry_targets.index_unchecked(n)};
                    if(reinterpret_cast<::std::uintptr_t>(::std::addressof(slot))!=address) { continue; }
                    if(found) { return false; }
                    target=::std::atomic_ref<::std::uintptr_t>{slot}.load(::std::memory_order_acquire);found=true;
                }
            }
            return found && target!=0u;
        }
        // Actual protected transaction leaf. Caller ALREADY holds GC/lease,
        // ONE real domain callback and publication. No recursive domain entry.
        // Native snapshot borrow ends before event prepare/commit, while the
        // caller still holds domain+publication through actual wake.
        [[nodiscard]] bool observe_native_locked(::uwvm2::utils::thread::cooperative_pause_location actual,
            bool external, observation& output, llvm_jit_debug_native_function_image* image) const noexcept
        {
            if(get_runtime_state_publication_depth() != 1u) { return false; }
            bool valid{};
            if(!external || !capture_->matches_native_publication_locked(actual, external) || capture_->snapshot_.frames.empty() ||
               capture_->code_owners_.empty() || !capture_->native_ledger_owner_ ||
               cursor_->ledger_ != capture_->native_ledger_owner_.get()) { return false; }
            auto const& site{capture_->native_site_};
            auto const generation{capture_->snapshot_.frames.back().function_generation};
            static_cast<void>(::uwvm2::uwvm::debugger::native_step::with_owned_registers_and_revision(session_,
                [&](auto thread, auto pc, auto begin, auto end, auto const& registers, auto revision, bool delay_slot = false) noexcept
            {
                namespace nr = ::uwvm2::uwvm::debugger::native_registers;
                // Fields of the cursor/kernel snapshot are read ONLY inside
                // the authenticated trapped gate, after canonicalization.
                if(revision == 0u || (trap_revision_ != 0u && revision != trap_revision_) ||
                   delay_slot || registers.size() == 0u || registers.values.size() != 38u ||
                   registers.pc() != pc || registers.sp() == 0u ||
                   !site.valid || site.participant != capture_->snapshot_.participant || site.native_thread != thread ||
                   site.owner_begin != begin || site.owner_end != end ||
                   thread != cursor_->thread_ || begin != cursor_->begin_ || end != cursor_->end_ ||
                   site.return_pc != cursor_->first_ || !cursor_->witnessed_ || cursor_->retirement_ != 4u ||
                   cursor_->witnessed_pc_ != pc || cursor_->witnessed_sp_ != registers.sp() ||
                   cursor_->count_ == 0u ||
                   !cursor_->ledger_->matches_native_chain(cursor_->expected_, cursor_->count_)) { return; }
                ::std::uintptr_t actual_begin{}, actual_end{};
                if(!debug_resolve_actual_native_function_body(actual.code_unit, actual.function, generation,
                    actual.code_generation, pc, actual_begin, actual_end) || actual_begin != begin || actual_end != end ||
                   begin == 0u || end <= begin || pc < begin || pc >= end ||
                   end - begin > PTRDIFF_MAX) { return; }
                observation candidate{}; candidate.trap_revision = revision; candidate.stack = registers.sp();
                candidate.view = {thread, pc, 0u, begin, end, actual.code_unit, actual.function, generation, actual.code_generation};
                candidate.caller_owner = capture_->code_owners_.back();
                if(candidate.caller_owner == nullptr) { return; }
                for(::std::size_t i{}; i != candidate.gpr.size(); ++i)
                {
                    // [actual private snapshot values0..38][owned proof values0..38]
                    // [safe] both fixed arrays have 38 entries BEFORE indexing; no caller
                    // populated register snapshot reaches this actual join.
                    candidate.gpr[i] = registers.values[i];
                }
                if(opaque_call_)
                {
                    // A near-call NI does not enter, inspect or single-step its
                    // callee. Do not derive/dereference any target slot, stack,
                    // VM pointer or host body. The exact caller trap/GPRs and
                    // sealed caller fallthrough remain the sole resume proof.
                }
                else if(memory_operand_.valid)
                {
                    ::std::uintptr_t address{},target{};
                    if(!derive_call_slot(memory_operand_,candidate.gpr,pc,call_size_,address) ||
                       (slot_address_!=0u && slot_address_!=address) ||
                       !actual_typed_slot_target(address,actual.code_generation,target) ||
                       (callee_target_!=0u && target!=callee_target_) ||
                       !find_actual_typed_callee(target,actual.code_generation,candidate)) { return; }
                    candidate.callee_target=target;candidate.slot_address=address;
                }
                else if(callee_target_ != 0u)
                {
                    if(register_index_ != SIZE_MAX && (register_index_ >= candidate.gpr.size() ||
                        candidate.gpr[register_index_] != callee_target_)) { return; }
                    if(!find_actual_typed_callee(callee_target_, actual.code_generation, candidate)) { return; }
                    candidate.callee_target=callee_target_;
                }
                if(image != nullptr)
                {
                    if(!debug_copy_actual_native_target(actual.code_unit, actual.function, generation,
                        candidate.caller_owner, image->target)) { return; }
                    if(image->bytes.size() != end - begin || image->guest_code.size() != end - begin) { return; }
                    image->size = static_cast<::std::size_t>(end - begin);
                    image->stop_pc = pc; image->owner_begin = begin; image->owner_end = end;
                    image->native_instruction_stop = true; image->participant = capture_->snapshot_.participant;
                    image->module = actual.code_unit; image->function = actual.function;
                    image->function_generation = generation; image->runtime_epoch = actual.code_generation;
                    // [sealed genuine Wasm text begin ... end][owned exact-sized bytes]
                    // [safe] nonzero full extent equals storage BEFORE pointer formation;
                    //  ^^ borrow only under actual GC/lease/domain/pub/native gate.
                    // No VM/adapter/trailer extent or diagnostic range is copied.
                    ::std::memcpy(image->bytes.data(), reinterpret_cast<void const*>(begin), image->size);
#if defined(__linux__) && defined(__arm__)
                    if(image->instruction_code.size()!=image->size || instruction_identity_.empty()) { return; }
                    ::std::size_t expression_size{};
                    auto const* rows{debug_native_provenance_for_function_locked(actual.code_unit,actual.function,
                        generation,actual.code_generation,expression_size)};
                    if(!rows || !rows->instruction_code(begin,image->size,begin,end,
                        {instruction_identity_.data(),instruction_identity_.size()},expression_size,
                        actual.code_generation,image->instruction_code.data())) { return; }
#endif
                }
                output = candidate; valid = true;
            }));
            return valid;
        }
        // Class-only access to the canonical capture. Prepare owned DATA before
        // taking any native snapshot gate; no allocation can throw through the
        // actual noexcept trapped callback. observe rechecks the full live extent.
        [[nodiscard]] bool prepare_image_storage(llvm_jit_debug_native_function_image& image)
        {
            auto const& origin{capture_->native_site_};
            if(!origin.valid || origin.owner_begin == 0u || origin.owner_end <= origin.owner_begin ||
               origin.owner_end - origin.owner_begin > PTRDIFF_MAX) { return false; }
            auto const extent{static_cast<::std::size_t>(origin.owner_end - origin.owner_begin)};
            image.bytes.resize(extent); image.guest_code.resize(extent);
#if defined(__linux__) && defined(__arm__)
            if(capture_->snapshot_.frames.empty()) { return false; }
            auto const& frame{capture_->snapshot_.frames.back()};
            instruction_identity_=::fast_io::concat_fast_io("uwvm-m",::fast_io::mnp::dec(frame.module),
                "-f",::fast_io::mnp::dec(frame.function),"-g",::fast_io::mnp::dec(frame.function_generation),".wasm-native-v1");
            image.instruction_code.resize(extent);
#endif
            return true;
        }
        // Each query acquires GC admission + execution lease + ONE genuine
        // stopped participant + publication + actual native snapshot gate.
        [[nodiscard]] bool observe(observation& output, llvm_jit_debug_native_function_image* image) const
        {
            if(!capture_ || !cursor_ || session_ == nullptr || !capture_->control_ || !capture_->ticket_ ||
               cursor_->session_ != session_ || cursor_->capture_.get() != capture_.get() ||
               cursor_->capture_.owner_before(capture_) || capture_.owner_before(cursor_->capture_)) { return false; }
            auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; if(!gc_reader) { return false; }
            auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
            bool valid{};
            bool const stopped{capture_->control_->with_parked_participant(capture_->ticket_, capture_->snapshot_.participant,
                [&](auto const actual, bool external)
            {
                runtime_state_publication_guard publication{};
                valid = observe_native_locked(actual, external, output, image);
            })};
            return stopped && valid;
        }
        [[nodiscard]] bool matches_observation(observation const& actual) const noexcept
        {
            auto expected{view_}; expected.continuation_pc = 0u;
            return trap_revision_ != 0u && actual.trap_revision == trap_revision_ && actual.view == expected &&
                actual.gpr == origin_gpr_ && actual.stack == origin_sp_ &&
                actual.caller_owner == caller_owner_ && actual.callee_owner == callee_owner_ &&
                actual.callee_begin == callee_begin_ && actual.callee_end == callee_end_ &&
                actual.callee_module == callee_module_ && actual.callee_function == callee_function_ &&
                actual.callee_generation == callee_generation_ && actual.callee_target==callee_target_ && actual.slot_address==slot_address_;
        }
        [[nodiscard]] bool resume_event(llvm_jit_debug_native_call_continuation_owner const& self,
            void* context, llvm_jit_debug_native_call_event_resume_callback callback) const
        {
#if UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
            if(!self || self.get()!=this || !callback || !software_plan_.valid() || !capture_ || !cursor_ ||
               !capture_->control_ || !capture_->ticket_ || trap_revision_==0u || trap_revision_==UINT64_MAX) { return false; }
            // All allocation precedes the actual resume transaction. A retained
            // copy loses authority immediately after this synchronous callback.
            auto window{::std::make_shared<llvm_jit_debug_native_call_event::resume_window>()};
            auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; if(!gc_reader) { return false; }
            auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
            return capture_->control_->with_externally_parked_participant_for_resume(
                capture_->ticket_,capture_->snapshot_.participant,[&](auto actual,auto& borrow) noexcept
            {
                runtime_state_publication_guard publication{};
                observation current{};
                if(!observe_native_locked(actual,true,current,nullptr) || !matches_observation(current)) { return; }
                llvm_jit_debug_native_call_event event{window,self,software_plan_,origin_sp_};
                window->active.store(true,::std::memory_order_release);
                callback(context,event,view_,borrow);
                window->active.store(false,::std::memory_order_release);
            });
#else
            (void)self;(void)context;(void)callback;return false;
#endif
        }
        [[nodiscard]] bool resume(void* context, llvm_jit_debug_native_call_resume_callback callback) const
        {
            if(!capture_ || !cursor_ || session_ == nullptr || !capture_->control_ || !capture_->ticket_ ||
               cursor_->session_ != session_ || cursor_->capture_.get() != capture_.get() ||
               cursor_->capture_.owner_before(capture_) || capture_.owner_before(cursor_->capture_)) { return false; }
            if(callback == nullptr || (!opaque_call_ && callee_target_ == 0u) ||
               trap_revision_ == 0u || trap_revision_ == UINT64_MAX) { return false; }
            auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; if(!gc_reader) { return false; }
            auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
            // The domain validates current ticket/ALLN/external actual slot and
            // constructs a one-shot noncopyable synchronous resume borrow.
            // Its actual commit drops this slot, wakes under the same mutex,
            // and rolls real counts/markers back if wake returns false.
            return capture_->control_->with_externally_parked_participant_for_resume(
                capture_->ticket_, capture_->snapshot_.participant,
                [&](auto const actual, auto& borrow) noexcept
            {
                runtime_state_publication_guard publication{};
                observation current{};
                if(!observe_native_locked(actual, true, current, nullptr) || !matches_observation(current)) { return; }
                // The native host-read guard has ended; this ONE domain mutex
                // and publication guard STILL remain held through actual wake.
                // No legal replacement/reset can change caller/gen/epoch in
                // the query -> event prepare/enable -> actual wake interval.
                // This synchronous trusted host callback may prepare the FastIO
                // event, commit exact backend wake or retire the failed event.
                // It must never wait for worker ACK or reenter this domain.
                callback(context, view_, borrow);
            });
        }
#endif
    public:
        llvm_jit_debug_native_call_continuation(llvm_jit_debug_native_call_continuation const&) = delete;
        llvm_jit_debug_native_call_continuation& operator=(llvm_jit_debug_native_call_continuation const&) = delete;
    };
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE) && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
    namespace
    {
        ::std::mutex g_debug_native_call_continuation_mutex{};
        ::std::weak_ptr<llvm_jit_debug_native_call_continuation const> g_debug_native_call_continuations[256u]{};
        [[nodiscard]] llvm_jit_debug_native_call_continuation_owner canonical_native_call_continuation(
            llvm_jit_debug_native_call_continuation_owner const& supplied) noexcept
        {
            if(!supplied) { return {}; }
            ::std::lock_guard lock{g_debug_native_call_continuation_mutex};
            for(auto const& slot : g_debug_native_call_continuations)
            {
                auto const owner{slot.lock()};
                // Compare address AND control block BEFORE reading any supplied
                // alias. The actual weak registry alone names minted objects.
                if(owner && owner.get() == supplied.get() && !owner.owner_before(supplied) && !supplied.owner_before(owner)) { return owner; }
            }
            return {};
        }
    }
#endif
    extern "C++" llvm_jit_debug_native_call_continuation_owner llvm_jit_debug_mint_native_call_continuation_host_api(
        llvm_jit_debug_activation_capture_owner const& supplied, llvm_jit_debug_native_activation_cursor_owner const& supplied_cursor,
        void const* session) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE) && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(session == nullptr || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return {}; }
        try
        {
            auto const capture{debug_activation_canonical_capture(supplied)};
            auto const cursor{canonical_native_activation_cursor(supplied_cursor)};
            if(!capture || !cursor) { return {}; }
            // Allocate all bounded cold owners/copies before entering the real
            // native gate; never allocate in a handler or after event enable.
            auto candidate{::std::shared_ptr<llvm_jit_debug_native_call_continuation>{new llvm_jit_debug_native_call_continuation}};
            candidate->capture_ = capture; candidate->cursor_ = cursor; candidate->session_ = session;
#if UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
            candidate->opaque_call_ = true; // NI supplies no callee observer.
#endif
            auto image{::std::make_unique<llvm_jit_debug_native_function_image>()};
            if(!candidate->prepare_image_storage(*image)) { return {}; }
            llvm_jit_debug_native_call_continuation::observation first{};
            if(!candidate->observe(first, image.get()) || image->owner_begin == 0u || image->owner_end <= image->owner_begin ||
               image->size != image->owner_end - image->owner_begin || image->size == 0u ||
               !image->complete_storage() ||
               image->stop_pc < image->owner_begin || image->stop_pc >= image->owner_end ||
               first.trap_revision == UINT64_MAX) { return {}; }
            candidate->trap_revision_ = first.trap_revision;
            // Real engine TargetMachine description, not host/default triple.
            ::uwvm2::uwvm::debugger::native_owned_instruction_semantics::decoder decoder{image->target};
            if(!decoder) { return {}; }
            auto const offset{static_cast<::std::size_t>(image->stop_pc - image->owner_begin)};
            decltype(decoder.decode(0u, {})) call{};
#if defined(__linux__) && defined(__arm__)
            ::uwvm2::uwvm::debugger::native_disassembly::decoder display{image->target};
            auto const boundary{::uwvm2::uwvm::debugger::native_disassembly::decode_window_with(display,
                {image->bytes.data(),image->size},image->owner_begin,image->stop_pc,0,0,1u,image->instruction_code)};
            if(!boundary.available || boundary.count!=1u || boundary.instructions[0u].pc!=image->stop_pc) { return {}; }
            call=decoder.decode(image->stop_pc,{image->bytes.data()+offset,image->size-offset});
            if(call.semantics().size!=boundary.instructions[0u].size) { return {}; }
#else
            ::std::size_t at{};
            for(;;)
            {
                if(at >= image->size || at > offset) { return {}; }
                // [owned whole true body bytes0..size][at<=offset<size] end
                // [safe] remaining extent BEFORE forming the indexed byte view;
                //  ^^ integer display-PC addition is bounded by true owner_end.
                auto const decoded{decoder.decode(image->owner_begin + at,
                    {image->bytes.data() + at, image->size - at})};
                auto const size{decoded.semantics().size};
                if(!decoded || size == 0u || size > image->size - at) { return {}; }
                if(at == offset) { call = decoded; break; }
                if(size > offset - at) { return {}; }
                at += size; // Complete actual MC boundary, cannot cross the real stop.
            }
#endif
            auto const size{call.semantics().size};
            if(!call.safe_for_call_continuation() || size >= image->size - offset) { return {}; }
            auto const delay{call.call_delay_bytes()};
            if(delay)
            {
                if(size!=4u || delay!=4u || delay>=image->size-offset-size) { return {}; }
                auto const slot{decoder.decode(image->stop_pc+size,
                    {image->bytes.data()+offset+size,image->size-offset-size})};
                if(!slot || slot.semantics().size!=4u || !slot.safe_for_single_instruction()) { return {}; }
            }
            auto const next{offset + size + delay}; // size<remaining establishes next<size BEFORE addition.
            auto const continuation{image->owner_begin + next}; // true owner extent established.
            // [owned whole true body bytes0..size][next<size] image_end
            // [safe] size<remaining above proves next<size BEFORE byte pointer
            //         formation; the remaining span never leaves this owned copy.
            //         ^^ image->bytes.data() + next is an actual instruction fallthrough.
            auto const resumed{decoder.decode(continuation, {image->bytes.data() + next, image->size - next})};
            if(!resumed || resumed.semantics().size == 0u || resumed.semantics().size > image->size - next) { return {}; }
#if defined(__linux__) && defined(__arm__)
            // Complete mapped TEXT extent precedes every selected instruction.
            // A decoded literal-pool word can never authorize a return trap.
            for(::std::size_t b{};b!=resumed.semantics().size;++b)
            { if(image->instruction_code[next+b]!=1u) { return {}; } }
#endif
            auto const register_index{call.call_register_index()};
#if !UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
            if(call.call_memory().valid)
            {
                if(register_index!=SIZE_MAX || call.destination()) { return {}; }
                candidate->memory_operand_=call.call_memory();candidate->call_size_=size;
            }
            else if(register_index < first.gpr.size())
            {
                auto const value{first.gpr[register_index]};
                if(value == 0u || value > UINTPTR_MAX || call.destination()) { return {}; }
                candidate->callee_target_ = static_cast<::std::uintptr_t>(value);
                candidate->register_index_ = register_index;
            }
            else
            {
                if(register_index != SIZE_MAX || !call.destination()) { return {}; }
                candidate->callee_target_ = call.destination().display_pc;
            }
#else
            (void)register_index;
            // A recursive activation can hit the same return address. Its
            // private escape executes only an ordinary instruction or a direct
            // branch whose EVERY reachable successor is proved in this exact
            // immutable Wasm body. Calls/returns/indirect or escaping branches
            // never borrow this permission. No descendant snapshot is public.
#if !defined(__linux__) || !defined(__arm__)
            ::uwvm2::uwvm::debugger::native_disassembly::decoder display{image->target};
#endif
            auto const escaped{::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(display,decoder,
                {image->bytes.data(),image->size},image->owner_begin,image->owner_end,continuation,false,0u,image->instruction_code)};
            if(!escaped) { return {}; }
            auto const width{::uwvm2::uwvm::debugger::native_step::details::breakpoint_width()};
            auto const alignment{image->target.minimum_instruction_alignment};
            ::std::array<::std::uintptr_t,3u> sites{continuation,escaped.first_successor,0u};
            unsigned count{2u};
            if(escaped.second_successor && escaped.second_successor!=escaped.first_successor)
            { sites[count++]=escaped.second_successor; }
            ::std::array<::std::array<unsigned char,4u>,3u> originals{};
            for(unsigned i{};i!=count;++i)
            {
                auto const pc{sites[i]};
                if(pc<image->owner_begin || pc>=image->owner_end || alignment==0u || pc%alignment!=0u ||
                   width>image->owner_end-pc || (i && pc==continuation)) { return {}; }
                for(unsigned j{};j!=i;++j)
                { if(pc>sites[j] ? pc-sites[j]<width : sites[j]-pc<width) { return {}; } }
                auto const at{static_cast<::std::size_t>(pc-image->owner_begin)};
                for(unsigned b{};b!=width;++b) { originals[i][b]=image->bytes[at+b]; }
            }
            candidate->software_plan_=cursor->breakpoint_plan(cursor,image->stop_pc,first.trap_revision,false,width,count,sites,originals,
                {escaped.first_successor,escaped.second_successor},escaped.successor_npc,escaped.successor_delay_slot);
#endif
            llvm_jit_debug_native_call_continuation::observation second{};
            if(!candidate->observe(second, nullptr))
            {
                // Typed Wasm SI and NI fallthrough have distinct authorities.
                // A genuine near CALL in this exact sealed Wasm caller may run
                // a VM/host helper normally while TF stays off. Its destination
                // does not become code/read/unwind authority. Remove every
                // target/slot claim before retrying the COMPLETE caller proof;
                // a stale capture, changed trap or publication still refuses.
                candidate->opaque_call_ = true;
                candidate->callee_target_ = 0u; candidate->register_index_ = SIZE_MAX;
                candidate->memory_operand_ = {}; candidate->call_size_ = 0u;
                second = {};
                if(!candidate->observe(second, nullptr)) { return {}; }
            }
            if(second.trap_revision != first.trap_revision || second.gpr != first.gpr ||
               second.view != first.view || second.caller_owner != first.caller_owner) { return {}; }
            if(!candidate->opaque_call_ && second.callee_target == 0u) { return {}; }
            candidate->origin_gpr_ = second.gpr; candidate->origin_sp_ = second.stack;
            candidate->callee_target_=second.callee_target;candidate->slot_address_=second.slot_address;
            candidate->view_ = second.view; candidate->view_.continuation_pc = continuation;
            candidate->caller_owner_ = second.caller_owner; candidate->callee_owner_ = second.callee_owner;
            candidate->callee_begin_ = second.callee_begin; candidate->callee_end_ = second.callee_end;
            candidate->callee_module_ = second.callee_module; candidate->callee_function_ = second.callee_function;
            candidate->callee_generation_ = second.callee_generation;
            ::std::lock_guard registry{g_debug_native_call_continuation_mutex};
            for(auto& slot : g_debug_native_call_continuations)
            { if(slot.expired()) { slot = candidate; return candidate; } }
            return {}; // Bounded256 capacity exhaustion retains the trapped stop.
        }
        catch(...) { return {}; }
#else
        (void)supplied; (void)supplied_cursor; (void)session; return {};
#endif
    }
    extern "C++" bool llvm_jit_debug_native_call_continuation_host_api(
        llvm_jit_debug_native_call_continuation_owner const& supplied, void const* session,
        llvm_jit_debug_native_call_continuation_view& out) noexcept
    {
        out = {};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE) && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(session == nullptr || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const owner{canonical_native_call_continuation(supplied)};
            if(!owner || owner->session_ != session || (!owner->opaque_call_ && owner->callee_target_ == 0u) ||
               !debug_activation_canonical_capture(owner->capture_) || !canonical_native_activation_cursor(owner->cursor_)) { return false; }
            llvm_jit_debug_native_call_continuation::observation actual{};
            if(!owner->observe(actual, nullptr) || !owner->matches_observation(actual)) { return false; }
            out = owner->view_; return true;
        }
        catch(...) { out = {}; return false; }
#else
        (void)supplied; (void)session; return false;
#endif
    }
    // This is the sole call-capability execution transition. A copied view or
    // earlier query cannot detach the protected native resume transaction.
    extern "C++" bool llvm_jit_debug_resume_native_call_continuation_host_api(
        llvm_jit_debug_native_call_continuation_owner const& supplied, void const* session,
        void* context, llvm_jit_debug_native_call_resume_callback callback) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE) && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(session == nullptr || callback == nullptr || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const owner{canonical_native_call_continuation(supplied)};
            if(!owner || owner->session_ != session || !debug_activation_canonical_capture(owner->capture_) ||
               !canonical_native_activation_cursor(owner->cursor_)) { return false; }
            return owner->resume(context, callback);
        }
        catch(...) { return false; }
#else
        (void)supplied; (void)session; (void)context; (void)callback; return false;
#endif
    }
    extern "C++" bool llvm_jit_debug_resume_native_call_event_host_api(
        llvm_jit_debug_native_call_continuation_owner const& supplied, void const* session,
        void* context, llvm_jit_debug_native_call_event_resume_callback callback) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE) && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(session == nullptr || callback == nullptr || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const owner{canonical_native_call_continuation(supplied)};
            if(!owner || owner->session_ != session || !debug_activation_canonical_capture(owner->capture_) ||
               !canonical_native_activation_cursor(owner->cursor_)) { return false; }
            return owner->resume_event(owner,context,callback);
        }
        catch(...) { return false; }
#else
        (void)supplied; (void)session; (void)context; (void)callback; return false;
#endif
    }
#endif

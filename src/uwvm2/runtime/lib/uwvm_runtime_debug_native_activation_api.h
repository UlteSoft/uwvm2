// Runtime-private physical Wasm activation cursor. A before-park capture is
// the originating event, not a claim that its logical frame remains alive.
// The actual kernel trap witnesses the real bounded dynamic ledger anew before
// any native stop publication. Only genuine leave hooks recognize retirement
// into the same physical Wasm epilogue; RET/out-of-owner successors stay denied.
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
    namespace
    {
        struct debug_native_actual_leave_witness
        {
            details::debug_activation::ledger const* ledger{};
            ::std::uint64_t incarnation{};
            unsigned kind{4u};
        };
        // Runtime implementation-private marker, never exposed by runtime.h.
        // Only the genuine generated leave bridge sets it around its synchronous
        // post-ledger.leave reporter; it is not read from a signal callback.
        inline constinit thread_local debug_native_actual_leave_witness g_debug_native_actual_leave_witness{};
    }
#endif
    class llvm_jit_debug_native_activation_cursor final
    {
        llvm_jit_debug_native_activation_cursor() = default;
        friend class llvm_jit_debug_native_call_continuation;
        friend class llvm_jit_debug_native_return_continuation;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_mint_native_call_continuation_host_api(
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_native_activation_cursor_owner const&,
            void const*) noexcept -> llvm_jit_debug_native_call_continuation_owner;
        friend class llvm_jit_debug_activation_capture;
        llvm_jit_debug_activation_capture_owner capture_{};
        // Capture retains the ACTUAL dynamic debug-ledger control block. A
        // retained provider never borrows freed storage, but physical liveness
        // still requires real kernel/leave witnesses. Actual scope exit poisons
        // the ledger after native ACK, so stale providers cannot revive it.
        details::debug_activation::ledger const* ledger_{};
        ::std::unique_ptr<details::debug_activation::frame[]> expected_storage_{};
        details::debug_activation::frame const* expected_{};
        ::std::size_t count_{};
        void const* session_{};
        ::std::uint_least64_t thread_{};
        ::std::uintptr_t begin_{}, end_{}, first_{};
        // Only the actual selected worker writes these members. Host inspection
        // holds the real trapped gate after phase::trapped's acquire publication.
        bool witnessed_{};
        unsigned retirement_{4u};
        ::std::uintptr_t witnessed_pc_{}, witnessed_sp_{};
#if defined(__linux__) && ((defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)) || (defined(__i386__) && __SIZEOF_POINTER__ == 4))
        // Manager-only code facts for ONE genuine kernel trap revision. These
        // contain no live register, CFA, saved word or read/resume authority.
        // Access requires the actual domain/publication/native read guards;
        // each query still reauthenticates every owner and every CFI slot.
        struct native_return_code_key
        {
            ::std::uintptr_t begin{}, end{};
            ::std::uint_least64_t module{}, function{}, generation{}, epoch{};
            void const* owner{};
            bool operator==(native_return_code_key const&) const noexcept = default;
        };
        struct native_return_code_facts
        {
            struct boundary { native_return_code_key key{}; ::std::uintptr_t return_pc{}; };
            struct epilogue { native_return_code_key key{}; ::std::size_t pop{}; };
            ::std::uint64_t revision{};
            ::std::array<boundary,32u> boundaries{};
            ::std::array<epilogue,32u> returns{};
            ::std::size_t boundary_count{}, return_count{};
        };
        mutable native_return_code_facts return_code_facts_{};
#endif
        // Cold-owned original cursor for a sealed parent-return event. No
        // shared_ptr operation occurs in the actual signal/leave callbacks.
        llvm_jit_debug_native_activation_cursor_owner return_origin_{};
        ::std::uintptr_t return_stack_{};
        // Qualified function friends nominate the real global-module APIs
        // first declared with C++ linkage in runtime.h. This class keeps its
        // runtime module attachment; no new friend overload is introduced.
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_mint_native_activation_host_api(
            llvm_jit_debug_activation_capture_owner const&, void const*) noexcept -> llvm_jit_debug_native_activation_cursor_owner;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_native_activation_provider_host_api(
            llvm_jit_debug_native_activation_cursor_owner const&, llvm_jit_debug_native_activation_provider&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_native_caller_host_api(
            llvm_jit_debug_native_activation_cursor_owner const&, void const*, llvm_jit_debug_native_caller_view&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_native_backtrace_host_api(
            llvm_jit_debug_native_activation_cursor_owner const&, void const*, llvm_jit_debug_native_backtrace_view&, ::std::size_t) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_native_activation_host_api(
            llvm_jit_debug_native_activation_cursor_owner const&, void const*) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_native_breakpoint_plan_host_api(
            llvm_jit_debug_native_activation_cursor_owner const&, void const*, bool,
            llvm_jit_debug_native_breakpoint_plan&) noexcept -> bool;
        [[nodiscard]] llvm_jit_debug_native_breakpoint_plan breakpoint_plan(
            llvm_jit_debug_native_activation_cursor_owner const& owner, ::std::uintptr_t pc,
            ::std::uint64_t revision, bool initial, unsigned width, unsigned count,
            ::std::array<::std::uintptr_t,3u> sites,
            ::std::array<::std::array<unsigned char,4u>,3u> original,
            ::std::array<::std::uintptr_t,2u> delayed_pc = {},
            ::std::array<::std::uintptr_t,2u> delayed_npc = {},
            ::std::array<bool,2u> delayed_slot = {}) const noexcept
        { return {owner,session_,thread_,begin_,end_,pc,revision,initial,width,count,sites,original,delayed_pc,delayed_npc,delayed_slot}; }
        [[nodiscard]] static bool witness_callback(void* context, void const* session,
            ::std::uint_least64_t thread, ::std::uintptr_t pc, ::std::uintptr_t sp) noexcept
        {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
            // [private provider borrow of actual cursor] owner_end
            // [safe                                    ] controller retains
            //  ^^ the strong minted owner until this exact worker's real ACK.
            auto& self{*static_cast<llvm_jit_debug_native_activation_cursor*>(context)};
            if(session != self.session_ || thread != self.thread_ || self.ledger_ == nullptr || sp == 0u ||
               pc < self.begin_ || pc >= self.end_ ||
               !::uwvm2::uwvm::debugger::native_step::in_owned_kernel_activation_witness(session, thread, pc, sp)) { return false; }
            if(!self.witnessed_ && pc != self.first_) { return false; }
            if(self.return_origin_ && !self.witnessed_)
            {
                auto const& origin{*self.return_origin_};
                // Only a genuine same-worker NORMAL leave plus actual parent
                // chain and the privately proved CFA authorize this new owner.
                // Tail replacement, host transition and EH never pass this gate.
                if(sp != self.return_stack_ || origin.retirement_ != 0u ||
                   !origin.ledger_->matches_native_retired_prefix(origin.expected_, origin.count_,
                       details::debug_activation::exit_kind::returned)) { return false; }
            }
            // [actual scope-owned dynamic ledger][immutable bounded cursor frame array]
            // [safe                            ] actual Wasm-owned kernel PC
            //  ^^ cannot interrupt a host ledger mutation. No TLS ledger lookup,
            // allocation, lock, standard-library container call or native SP read.
            bool const current{self.retirement_ == 4u
                ? self.ledger_->matches_native_chain(self.expected_, self.count_)
                : self.ledger_->matches_native_retired_prefix(self.expected_, self.count_,
                    static_cast<details::debug_activation::exit_kind>(self.retirement_))};
            if(!current) { return false; }
            self.witnessed_ = true; self.witnessed_pc_ = pc; self.witnessed_sp_ = sp;
            return true;
#else
            (void)context; (void)session; (void)thread; (void)pc; (void)sp; return false;
#endif
        }
        static void leave_callback(void* context, void const* session,
            ::std::uint64_t incarnation, unsigned kind) noexcept
        {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
            auto& self{*static_cast<llvm_jit_debug_native_activation_cursor*>(context)};
            if(self.return_origin_)
            {
                // The provider owns both cursors until real worker ACK. Forward
                // only to the sealed original callback; its implementation-only
                // actual-leave witness rejects all host-supplied reports.
                leave_callback(const_cast<llvm_jit_debug_native_activation_cursor*>(self.return_origin_.get()),
                    session, incarnation, kind);
            }
            if(session != self.session_ || self.count_ == 0u || self.ledger_ == nullptr ||
               incarnation != self.expected_[self.count_ - 1u].incarnation ||
               g_debug_native_actual_leave_witness.ledger != self.ledger_ ||
               g_debug_native_actual_leave_witness.incarnation != incarnation ||
               g_debug_native_actual_leave_witness.kind != kind) { return; }
            // Only the sealed SAME worker hook may touch mutable witness state.
            // A retained descriptor on another host thread returns above before
            // any unsynchronized read of this worker's witnessed_/retirement_.
            if(!self.witnessed_) { return; }
            // A real generated leave hook reports ONLY after ledger.leave.
            // Physical frame retirement cannot be guessed from old snapshots.
            if(self.retirement_ != 4u || kind > 2u ||
               !self.ledger_->matches_native_retired_prefix(self.expected_, self.count_,
                   static_cast<details::debug_activation::exit_kind>(kind)))
            { self.retirement_ = 5u; return; } // permanently invalid; never a revived origin
            self.retirement_ = kind;
#else
            (void)context; (void)session; (void)incarnation; (void)kind;
#endif
        }
        static bool descendant_callback(void* context,void const* session,::std::uint_least64_t thread,
            ::std::uintptr_t pc,::std::uintptr_t sp) noexcept
        {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
            auto const& self{*static_cast<llvm_jit_debug_native_activation_cursor const*>(context)};
            if(session!=self.session_ || thread!=self.thread_ || !self.ledger_ || self.retirement_!=4u ||
               sp==0u || pc<self.begin_ || pc>=self.end_ ||
               !::uwvm2::uwvm::debugger::native_step::in_owned_kernel_activation_witness(session,thread,pc,sp)) { return false; }
            if(self.return_origin_ && !self.witnessed_)
            {
                // A not-yet-returned parent is NEVER published by this check.
                // A same-PC recursive escape must still retain the actual
                // trapped child's chain inside this parent's original island.
                auto const& origin{*self.return_origin_};
                return sp<self.return_stack_ && origin.witnessed_ && origin.retirement_==4u &&
                    (origin.ledger_->matches_native_chain(origin.expected_,origin.count_) ||
                     origin.ledger_->matches_native_descendant(origin.expected_,origin.count_)) &&
                    self.ledger_->matches_native_descendant(self.expected_,self.count_);
            }
            return self.witnessed_ && self.ledger_->matches_native_descendant(self.expected_,self.count_);
#else
            (void)context;(void)session;(void)thread;(void)pc;(void)sp;return false;
#endif
        }
        [[nodiscard]] llvm_jit_debug_native_activation_provider provider(
            llvm_jit_debug_native_activation_cursor_owner const& owner) noexcept
        { return {owner, this, witness_callback, leave_callback, session_, thread_, begin_, end_, first_,descendant_callback}; }
    public:
        llvm_jit_debug_native_activation_cursor(llvm_jit_debug_native_activation_cursor const&) = delete;
        llvm_jit_debug_native_activation_cursor& operator=(llvm_jit_debug_native_activation_cursor const&) = delete;
    };
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
    namespace
    {
        ::std::mutex g_debug_native_activation_cursor_mutex{};
        ::std::weak_ptr<llvm_jit_debug_native_activation_cursor const> g_debug_native_activation_cursors[256u]{};
        [[nodiscard]] llvm_jit_debug_native_activation_cursor_owner canonical_native_activation_cursor(
            llvm_jit_debug_native_activation_cursor_owner const& supplied) noexcept
        {
            if(!supplied) { return {}; }
            ::std::lock_guard lock{g_debug_native_activation_cursor_mutex};
            for(auto const& slot : g_debug_native_activation_cursors)
            {
                auto const owner{slot.lock()};
                // Compare pointer AND control-block before dereferencing supplied
                // aliases; a display PC or shared_ptr alias is never authority.
                if(owner && owner.get() == supplied.get() && !owner.owner_before(supplied) && !supplied.owner_before(owner))
                { return owner; }
            }
            return {};
        }
    }
#endif
    bool llvm_jit_debug_activation_capture::matches_native_cursor_locked(void const* session,
        ::std::uint_least64_t thread, ::std::uintptr_t pc, ::std::uintptr_t sp,
        ::std::uintptr_t begin, ::std::uintptr_t end) const noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
        if(session == nullptr || sp == 0u || get_runtime_state_publication_depth() != 1u ||
           !native_ledger_owner_) { return false; }
        // The caller holds the actual native trap gate throughout this check
        // AND the subsequent copy. Only registry-owned cursors are inspected;
        // a displayed address, old function range or old source capture cannot
        // substitute for this exact activation's kernel PC/SP witness.
        ::std::lock_guard registry{g_debug_native_activation_cursor_mutex};
        for(auto const& slot : g_debug_native_activation_cursors)
        {
            auto const owner{slot.lock()};
            if(!owner || owner->capture_.get() != this || owner->session_ != session ||
               owner->ledger_ != native_ledger_owner_.get() || owner->thread_ != thread ||
               owner->begin_ != begin || owner->end_ != end || !owner->witnessed_ ||
               owner->retirement_ > 4u || owner->witnessed_pc_ != pc || owner->witnessed_sp_ != sp) { continue; }
            return owner->retirement_ == 4u
                ? owner->ledger_->matches_native_chain(owner->expected_,owner->count_)
                : owner->ledger_->matches_native_retired_prefix(owner->expected_,owner->count_,
                    static_cast<details::debug_activation::exit_kind>(owner->retirement_));
        }
        return false;
#else
        (void)session; (void)thread; (void)pc; (void)sp; (void)begin; (void)end; return false;
#endif
    }
    namespace
    {
        inline void debug_native_report_actual_activation_leave(details::debug_activation::ledger const* ledger,
            ::std::uint64_t incarnation, unsigned kind) noexcept
        {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
            // Called exclusively after the actual generated bridge validated its
            // entry depth and applied leave to its real dynamic TLS-owned ledger.
            // A retained provider copy cannot mint this implementation-only seal.
            if(g_debug_native_actual_leave_witness.ledger != nullptr) { ::fast_io::fast_terminate(); }
            g_debug_native_actual_leave_witness = {ledger, incarnation, kind};
            ::uwvm2::uwvm::debugger::native_step::report_activation_leave_on_worker(incarnation, kind);
            g_debug_native_actual_leave_witness = {};
#else
            (void)ledger; (void)incarnation; (void)kind;
#endif
        }
    }
    extern "C++" llvm_jit_debug_native_activation_cursor_owner llvm_jit_debug_mint_native_activation_host_api(
        llvm_jit_debug_activation_capture_owner const& supplied, void const* session) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
        if(session == nullptr || !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return {}; }
        try
        {
            auto const capture{debug_activation_canonical_capture(supplied)};
            if(!capture || !capture->control_ || !capture->ticket_) { return {}; }
            auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; if(!gc_reader) { return {}; }
            auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return {}; }
            ::std::shared_ptr<llvm_jit_debug_native_activation_cursor> candidate{};
            bool const stopped{capture->control_->with_parked_participant(capture->ticket_, capture->snapshot_.participant,
                [&](auto const actual, bool external)
            {
                runtime_state_publication_guard publication{};
                if(external || !capture->matches_publication_locked(actual) || !capture->native_ledger_owner_) { return; }
                auto const& site{capture->native_site_};
                auto const count{capture->snapshot_.frames.size()};
                if(!site.valid || site.participant != capture->snapshot_.participant || site.native_thread == 0u ||
                   site.owner_begin == 0u || site.owner_end <= site.owner_begin ||
                   site.return_pc < site.owner_begin || site.return_pc >= site.owner_end ||
                   count == 0u) { return; }
                ::std::uintptr_t actual_begin{}, actual_end{};
                if(!debug_resolve_actual_native_function_body(actual.code_unit, actual.function,
                    capture->snapshot_.frames.back().function_generation, actual.code_generation,
                    site.return_pc, actual_begin, actual_end) || actual_begin != site.owner_begin || actual_end != site.owner_end) { return; }
                candidate.reset(new llvm_jit_debug_native_activation_cursor);
                candidate->capture_ = capture; candidate->ledger_ = capture->native_ledger_owner_.get();
                candidate->session_ = session; candidate->thread_ = site.native_thread;
                candidate->begin_ = site.owner_begin; candidate->end_ = site.owner_end; candidate->first_ = site.return_pc;
                candidate->expected_storage_ = ::std::make_unique<details::debug_activation::frame[]>(count);
                candidate->expected_ = candidate->expected_storage_.get();
                candidate->count_ = count;
                for(::std::size_t i{}; i != count; ++i)
                {
                    // [canonical immutable capture frame[0,count)][cursor-owned frame array]
                    // [safe                                             ] checked i;
                    //  ^^ copy comparison-only identities, never stack/code pointers.
                    auto const& frame{capture->snapshot_.frames[i]};
                    candidate->expected_storage_[i] = {frame.incarnation, frame.parent, frame.continuation,
                        frame.module, frame.function, frame.function_generation, frame.runtime_epoch, 0u};
                }
                // ONE actual parked participant pins the dynamic ledger pointer.
                // Current complete chain must still equal its originating capture.
                if(!candidate->ledger_->matches_native_chain(candidate->expected_, count)) { candidate.reset(); }
            })};
            if(!stopped || !candidate) { return {}; }
            ::std::lock_guard registry{g_debug_native_activation_cursor_mutex};
            for(auto& slot : g_debug_native_activation_cursors)
            { if(slot.expired()) { slot = candidate; return candidate; } }
            return {};
        }
        catch(...) { return {}; }
#else
        (void)supplied; (void)session; return {};
#endif
    }
    extern "C++" bool llvm_jit_debug_native_activation_provider_host_api(
        llvm_jit_debug_native_activation_cursor_owner const& supplied, llvm_jit_debug_native_activation_provider& out) noexcept
    {
        out = {};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
        auto const cursor{canonical_native_activation_cursor(supplied)};
        if(!cursor) { return false; }
        // Registry proves the exact immutable private owner. Descriptor copies
        // retain this SAME strong owner; no stale raw callback context escapes.
        // No session, ledger or code is dereferenced by this cold operation.
        out = const_cast<llvm_jit_debug_native_activation_cursor*>(cursor.get())->provider(cursor);
        return out.valid();
#else
        (void)supplied; return false;
#endif
    }
    extern "C++" bool llvm_jit_debug_native_activation_host_api(
        llvm_jit_debug_native_activation_cursor_owner const& supplied, void const* session) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE)
        if(session == nullptr || !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const cursor{canonical_native_activation_cursor(supplied)};
            if(!cursor || session != cursor->session_) { return false; }
            auto const& capture{cursor->capture_};
            if(!capture || !capture->control_ || !capture->ticket_) { return false; }
            auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; if(!gc_reader) { return false; }
            auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
            bool valid{};
            bool const stopped{capture->control_->with_parked_participant(capture->ticket_, capture->snapshot_.participant,
                [&](auto actual, bool external)
            {
                runtime_state_publication_guard publication{};
                if(!external || !capture->matches_native_publication_locked(actual, external)) { return; }
                // Lock order stays lease -> ONE domain -> publication -> native.
                static_cast<void>(::uwvm2::uwvm::debugger::native_step::with_owned_registers(session,
                    [&](auto thread, auto pc, auto begin, auto end, auto const& registers) noexcept
                {
                    // The actual kernel context's bounded ledger witness was
                    // release-published BEFORE the real phase::trapped gate.
                    if(thread != cursor->thread_ || begin != cursor->begin_ || end != cursor->end_ ||
                       !cursor->witnessed_ || cursor->retirement_ > 4u || pc != cursor->witnessed_pc_ ||
                       registers.pc() != pc || registers.sp() == 0u || registers.sp() != cursor->witnessed_sp_)
                    { return; }
                    valid = true;
                }));
            })};
            return stopped && valid;
        }
        catch(...) { return false; }
#else
        (void)supplied; (void)session; return false;
#endif
    }
    extern "C++" bool llvm_jit_debug_native_breakpoint_plan_host_api(
        llvm_jit_debug_native_activation_cursor_owner const& supplied, void const* session,
        bool initial, llvm_jit_debug_native_breakpoint_plan& out) noexcept
    {
        out = {};
#if UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE && defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
        try
        {
            auto const cursor{canonical_native_activation_cursor(supplied)};
            if(!cursor || !session || session != cursor->session_) { return false; }
            if(!initial && !llvm_jit_debug_native_activation_host_api(cursor,session)) { return false; }
            llvm_jit_debug_native_function_image image{};
            if(!llvm_jit_debug_copy_native_function_host_api(cursor->capture_,initial ? nullptr : session,image) ||
               image.native_instruction_stop == initial || image.owner_begin != cursor->begin_ || image.owner_end != cursor->end_ ||
               (initial && image.stop_pc != cursor->first_)) { return false; }
            ::std::uint64_t revision{};
            ::std::uintptr_t pending_npc{};
            bool pending_delay_slot{};
            if(!initial)
            {
                bool same{};
                if(!::uwvm2::uwvm::debugger::native_step::with_owned_registers_and_revision(session,
                    [&](auto thread,auto pc,auto begin,auto end,auto const& raw,auto actual_revision,bool actual_delay_slot) noexcept
                    { same = thread == cursor->thread_ && pc == image.stop_pc && begin == image.owner_begin && end == image.owner_end;
                      if(same)
                      {
                          revision = actual_revision;
#if defined(__sparc__) && defined(__arch64__)
                          if(raw.machine == ::uwvm2::uwvm::debugger::native_registers::architecture::sparc64)
                          { pending_npc = raw.values[33u];
                            pending_delay_slot = actual_delay_slot; }
#endif
                      } }) || !same) { return false; }
            }
            namespace debugger = ::uwvm2::uwvm::debugger;
            debugger::native_disassembly::decoder display{image.target};
            debugger::native_owned_instruction_semantics::decoder semantics{image.target};
            auto const permitted{debugger::native_wasm_step_boundary::prepare(display,semantics,
                {image.bytes.data(),image.size},image.owner_begin,image.owner_end,image.stop_pc,false,pending_npc,image.instruction_code,pending_delay_slot)};
            if(!permitted || image.target.minimum_instruction_alignment == 0u) { return false; }
            unsigned const width{debugger::native_step::details::breakpoint_width()};
            ::std::array<::std::uintptr_t,3u> sites{};
            ::std::array<::std::array<unsigned char,4u>,3u> original{};
            unsigned count{};
            if(initial) { sites[count++] = image.stop_pc; }
            sites[count++] = permitted.first_successor;
            if(permitted.second_successor && permitted.second_successor != permitted.first_successor) { sites[count++] = permitted.second_successor; }
            for(unsigned i{}; i != count; ++i)
            {
                auto const pc{sites[i]};
                if(pc < image.owner_begin || pc >= image.owner_end || width > image.owner_end - pc ||
                   (!(initial && i == 0u) && pc == image.stop_pc) || pc % image.target.minimum_instruction_alignment != 0u) { return false; }
                auto const offset{pc - image.owner_begin};
                for(unsigned j{}; j != width; ++j) { original[i][j] = image.bytes[offset+j]; }
                for(unsigned j{}; j != i; ++j)
                { if(pc > sites[j] ? pc-sites[j] < width : sites[j]-pc < width) { return false; } }
            }
            out = cursor->breakpoint_plan(cursor,image.stop_pc,revision,initial,width,count,sites,original,
                {permitted.first_successor,permitted.second_successor},permitted.successor_npc,permitted.successor_delay_slot);
            return out.valid();
        }
        catch(...) { return false; }
#else
        (void)supplied; (void)session; (void)initial; return false;
#endif
    }
#endif

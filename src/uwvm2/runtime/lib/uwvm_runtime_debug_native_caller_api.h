// Included inside the implementation namespace, after the genuine activation
// and call-continuation issuers. No public native address/read entry exists.
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    class llvm_jit_debug_native_return_continuation final
    {
        llvm_jit_debug_native_return_continuation() = default;
        llvm_jit_debug_native_activation_cursor_owner cursor_{};
        void const* session_{};
        // Runtime-only comparison evidence. No address/register/stack fields
        // are declared in the public return-plan query or its serialization.
        struct evidence
        {
            ::std::uint_least64_t thread{}, module{}, function{}, generation{}, epoch{};
            ::std::uint64_t revision{}, parent_incarnation{}, child_incarnation{};
            ::std::uintptr_t origin_pc{}, origin_sp{}, origin_begin{}, origin_end{};
            ::std::uintptr_t return_pc{}, cfa{}, target_sp{}, parent_begin{}, parent_end{};
            void const* child_owner{}; void const* parent_owner{};
            ::std::uintptr_t software_skip{};
            ::std::array<unsigned char,4u> software_original{}, software_skip_original{};
            friend bool operator==(evidence const&, evidence const&) noexcept = default;
        } evidence_{};
        [[nodiscard]] static bool observe(llvm_jit_debug_native_activation_cursor_owner const&, void const*,
            llvm_jit_debug_native_backtrace_view&, ::std::size_t, evidence*) noexcept;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_native_backtrace_host_api(
            llvm_jit_debug_native_activation_cursor_owner const&, void const*, llvm_jit_debug_native_backtrace_view&,
            ::std::size_t) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_mint_native_return_continuation_host_api(
            llvm_jit_debug_native_activation_cursor_owner const&, void const*) noexcept -> llvm_jit_debug_native_return_continuation_owner;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_native_return_continuation_host_api(
            llvm_jit_debug_native_return_continuation_owner const&, void const*, llvm_jit_debug_native_caller_view&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_resume_native_return_event_host_api(
            llvm_jit_debug_native_return_continuation_owner const&, void const*, void*,
            llvm_jit_debug_native_return_event_callback) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_query_native_return_event_host_api(
            llvm_jit_debug_native_return_event const&, void const*, llvm_jit_debug_native_caller_view&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_capture_native_return_stop_host_api(
            llvm_jit_debug_native_return_event const&, void const*, llvm_jit_debug_native_return_stop&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_resume_native_return_continuation_host_api(
            llvm_jit_debug_native_return_continuation_owner const&, void const*, void*,
            llvm_jit_debug_native_return_resume_callback) noexcept -> bool;
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && ((__SIZEOF_POINTER__ == 8 && (defined(__x86_64__) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || defined(__loongarch64) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)) && \
    LLVM_VERSION_MAJOR >= 23 && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        static void observe_native_locked(llvm_jit_debug_native_activation_cursor_owner const&, void const*,
            ::uwvm2::utils::thread::cooperative_pause_location, bool, llvm_jit_debug_native_backtrace_view&,
            ::std::size_t, evidence*);
        [[nodiscard]] bool resume(void*, llvm_jit_debug_native_return_resume_callback) const;
        [[nodiscard]] bool resume_event(llvm_jit_debug_native_return_continuation_owner const&,
            void*, llvm_jit_debug_native_return_event_callback) const;
        [[nodiscard]] static bool query_event(llvm_jit_debug_native_return_event const&,
            void const*, llvm_jit_debug_native_caller_view&);
        template<typename Inspect>
        [[nodiscard]] static bool inspect_event(llvm_jit_debug_native_return_event const&, void const*, Inspect&&);
        [[nodiscard]] static bool capture_event(llvm_jit_debug_native_return_event const&,
            void const*, llvm_jit_debug_native_return_stop&);
#endif
    public:
        llvm_jit_debug_native_return_continuation(llvm_jit_debug_native_return_continuation const&) = delete;
        llvm_jit_debug_native_return_continuation& operator=(llvm_jit_debug_native_return_continuation const&) = delete;
    };
    bool llvm_jit_debug_native_return_continuation::observe(
        llvm_jit_debug_native_activation_cursor_owner const& supplied, void const* session,
        llvm_jit_debug_native_backtrace_view& out, ::std::size_t maximum_frames, evidence* proof) noexcept
    {
        out = {}; if(proof != nullptr) { *proof = {}; }
        if(maximum_frames == 0u || maximum_frames > out.frames.size()) { return false; }
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && ((__SIZEOF_POINTER__ == 8 && (defined(__x86_64__) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || defined(__loongarch64) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)) && \
    LLVM_VERSION_MAJOR >= 23 && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        namespace cfi = ::uwvm2::runtime::compiler::llvm_jit::details;
        namespace step = ::uwvm2::uwvm::debugger::native_step;
        if(session == nullptr || !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const cursor{canonical_native_activation_cursor(supplied)};
            if(!cursor || cursor->session_ != session) { return false; }
            auto const& capture{cursor->capture_};
            if(!capture || !capture->control_ || !capture->ticket_ || !capture->native_stack_owner_ ||
               cursor->count_ < 2u || capture->snapshot_.frames.size() != cursor->count_ ||
               capture->code_owners_.size() != cursor->count_) { return false; }
            auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; if(!gc_reader) { return false; }
            auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
            auto const stopped{capture->control_->with_parked_participant(capture->ticket_, capture->snapshot_.participant,
                [&](auto const actual, bool external)
            {
                runtime_state_publication_guard publication{};
                observe_native_locked(cursor, session, actual, external, out, maximum_frames, proof);
            })};
            if(!stopped || out.count == 0u) { out = {}; return false; } return true;
        }
        catch(...) { out = {}; if(proof != nullptr) { *proof = {}; } return false; }
#else
        (void)supplied; (void)session; return false;
#endif
    }
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && ((__SIZEOF_POINTER__ == 8 && (defined(__x86_64__) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || defined(__loongarch64) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)) && \
    LLVM_VERSION_MAJOR >= 23 && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
    void llvm_jit_debug_native_return_continuation::observe_native_locked(
        llvm_jit_debug_native_activation_cursor_owner const& cursor, void const* session,
        ::uwvm2::utils::thread::cooperative_pause_location actual, bool external,
        llvm_jit_debug_native_backtrace_view& out, ::std::size_t maximum_frames, evidence* proof)
    {
        namespace cfi = ::uwvm2::runtime::compiler::llvm_jit::details;
        namespace step = ::uwvm2::uwvm::debugger::native_step;
        auto const& capture{cursor->capture_};
        // Caller owns GC admission + execution lease + ONE real domain mutex.
        // Never query/reenter that domain from this locked implementation.
        if(get_runtime_state_publication_depth() != 1u || !external ||
           !capture->matches_native_publication_locked(actual, external)) { return; }
        // Select ONLY the manager owned by this exact published engine.
        // A PC/range from another engine or an uncommitted replacement
        // cannot nominate CFI. Every owner borrow ends in this guard.
        auto const manager_for{[&](auto const& frame, void const* expected) noexcept
            -> cfi::runtime_llvm_jit_section_memory_manager const*
        {
            if(frame.module >= g_runtime.modules.size()) { return nullptr; }
            auto const& record{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(frame.module))};
            auto const* module{record.runtime_module};
            if(module == nullptr || !record.llvm_jit_ready || !record.llvm_jit_full_publication) { return nullptr; }
            auto const imports{module->imported_function_vec_storage.size()};
            if(frame.function < imports || frame.function - imports >= module->local_defined_function_vec_storage.size()) { return nullptr; }
            auto const local{static_cast<::std::size_t>(frame.function - imports)};
            if(debug_activation_current_code_owner(record, frame.module, frame.function, local,
                frame.function_generation) != expected) { return nullptr; }
            if(frame.function_generation == 1u)
            {
                auto const& owner{record.llvm_jit_full_publication};
                return owner.get() == expected && owner->engine && owner->context ? owner->debug_cfi_manager : nullptr;
            }
            cfi::runtime_llvm_jit_section_memory_manager const* found{};
            for(auto const& retained: record.llvm_jit_debug_full_retained_generations)
            {
                if(!retained || retained.get() != expected || !retained->committed || !retained->engine || !retained->context ||
                   retained->module_id != frame.module || retained->function_index != frame.function || retained->local_index != local ||
                   retained->expected_generation != frame.function_generation - 1u || retained->expected_runtime_epoch != frame.runtime_epoch) { continue; }
                if(found != nullptr) { return nullptr; } found = retained->debug_cfi_manager;
            }
            return found;
        }};
        static_cast<void>(step::with_owned_registers_and_revision(session,
            [&](auto thread, auto pc, auto begin, auto end, auto const& raw, auto revision) noexcept
        {
            if(thread != cursor->thread_ || begin != cursor->begin_ || end != cursor->end_ || revision == 0u ||
               !cursor->witnessed_ || cursor->retirement_ != 4u || cursor->witnessed_pc_ != pc ||
               raw.pc() != pc || raw.sp() == 0u || raw.sp() != cursor->witnessed_sp_ ||
               !((raw.machine == ::uwvm2::uwvm::debugger::native_registers::architecture::x86_64 && raw.size() == 18u) ||
                 (raw.machine == ::uwvm2::uwvm::debugger::native_registers::architecture::riscv64 && raw.size() == 34u) ||
                 (raw.machine == ::uwvm2::uwvm::debugger::native_registers::architecture::aarch64 && raw.size() == 34u) ||
                 (raw.machine == ::uwvm2::uwvm::debugger::native_registers::architecture::i686 && raw.size() == 10u) ||
                 (raw.machine == ::uwvm2::uwvm::debugger::native_registers::architecture::loongarch64 && raw.size() == 34u) ||
                 (raw.machine == ::uwvm2::uwvm::debugger::native_registers::architecture::mips64 && raw.size() == 36u)) ||
               !cursor->ledger_->matches_native_chain(cursor->expected_, cursor->count_)) { return; }
            auto const& leaf{capture->snapshot_.frames.back()};
            ::std::uintptr_t frame_begin{}, frame_end{};
            if(!debug_resolve_actual_native_function_body(leaf.module, leaf.function, leaf.function_generation,
                leaf.runtime_epoch, pc, frame_begin, frame_end) || frame_begin != begin || frame_end != end) { return; }
            // Initial registers come ONLY from this live kernel trap.
            // Later frames carry ONLY CFI-recovered preserved registers
            // and CFA/SP; unavailable volatile values never become zero
            // guesses for another frame's CFA or register-memory rule.
#if defined(__riscv) && __riscv_xlen == 64
            constexpr unsigned register_count{32u}, sp_register{2u}, ra_register{1u};
            constexpr unsigned preserved[]{1u,8u,9u,18u,19u,20u,21u,22u,23u,24u,25u,26u,27u};
            ::std::array<::std::uint64_t,32u> registers{};
            for(unsigned n{}; n != 32u; ++n) { registers[n] = raw.values[n]; }
            ::std::uint32_t known{UINT32_MAX};
#elif (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8)
            constexpr unsigned register_count{32u}, sp_register{31u}, ra_register{30u};
            constexpr unsigned preserved[]{19u,20u,21u,22u,23u,24u,25u,26u,27u,28u,29u,30u};
            ::std::array<::std::uint64_t,32u> registers{};
            for(unsigned n{}; n != 32u; ++n) { registers[n] = raw.values[n]; }
            ::std::uint32_t known{UINT32_MAX};
#elif defined(__loongarch64) && __SIZEOF_POINTER__ == 8
            constexpr unsigned register_count{32u}, sp_register{3u}, ra_register{1u};
            constexpr unsigned preserved[]{1u,22u,23u,24u,25u,26u,27u,28u,29u,30u,31u};
            ::std::array<::std::uint64_t,32u> registers{};
            for(unsigned n{}; n != 32u; ++n) { registers[n] = raw.values[n]; }
            ::std::uint32_t known{UINT32_MAX};
#elif (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips))
            constexpr unsigned register_count{32u}, sp_register{29u}, ra_register{31u};
            constexpr unsigned preserved[]{16u,17u,18u,19u,20u,21u,22u,23u,28u,30u,31u};
            ::std::array<::std::uint64_t,32u> registers{};
            for(unsigned n{}; n != 32u; ++n) { registers[n] = raw.values[n]; }
            ::std::uint32_t known{UINT32_MAX};
#elif defined(__i386__) && __SIZEOF_POINTER__ == 4
            constexpr unsigned register_count{8u}, sp_register{4u}, ra_register{8u};
            constexpr unsigned preserved[]{3u,5u,6u,7u,8u};
            // Real kernel EAX,EBX,ECX,EDX,ESI,EDI,EBP,ESP,EIP -> i386 DWARF.
            constexpr unsigned dwarf_to_snapshot[9u]{0u,2u,3u,1u,7u,6u,4u,5u,8u};
            ::std::array<::std::uint64_t,9u> registers{};
            for(unsigned n{}; n != 9u; ++n) { registers[n] = raw.values[dwarf_to_snapshot[n]]; }
            ::std::uint32_t known{0xffu};
#else
            constexpr unsigned register_count{16u}, sp_register{7u}, ra_register{16u};
            constexpr unsigned preserved[]{3u,6u,12u,13u,14u,15u,16u};
            constexpr unsigned dwarf_to_snapshot[17u]{0u,3u,2u,1u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,14u,15u,16u};
            ::std::array<::std::uint64_t,17u> registers{};
            for(unsigned n{}; n != 17u; ++n) { registers[n] = raw.values[dwarf_to_snapshot[n]]; }
            ::std::uint32_t known{0xffffu};
#endif
            ::std::uintptr_t frame_pc{pc}, sp{static_cast<::std::uintptr_t>(raw.sp())};
            auto const add{[](::std::uint64_t base, ::std::int32_t delta, ::std::uintptr_t& value) noexcept
            {
                if(base > UINTPTR_MAX) { return false; }
                if(delta < 0)
                {
                    auto const amount{static_cast<::std::uint64_t>(-static_cast<::std::int64_t>(delta))};
                    if(amount > base) { return false; } value = static_cast<::std::uintptr_t>(base - amount);
                }
                else
                {
                    if(static_cast<::std::uint32_t>(delta) > UINTPTR_MAX - base) { return false; }
                    value = static_cast<::std::uintptr_t>(base + static_cast<::std::uint32_t>(delta));
                }
                return true;
            }};
#if (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)
            // Repeated inspection of the SAME genuine stopped revision can
            // reuse immutable decoded code facts. The cursor is canonical and
            // this domain/publication/native guard serializes these fields.
            // Advancing the kernel revision discards ALL facts before lookup.
            // Every live CFI row, saved word, CFA and current owner is checked
            // again; no live value or execution/read authority is cached.
            using code_key = llvm_jit_debug_native_activation_cursor::native_return_code_key;
            auto& facts{cursor->return_code_facts_};
            if(facts.revision != revision) { facts = {}; facts.revision = revision; }
            auto& boundaries{facts.boundaries}; auto& returns{facts.returns};
            auto& boundary_count{facts.boundary_count}; auto& return_count{facts.return_count};
#endif
            for(::std::size_t index{cursor->count_ - 1u}; index != 0u && out.count != maximum_frames; --index)
            {
                auto const& current{capture->snapshot_.frames[index]};
                auto const& parent{capture->snapshot_.frames[index - 1u]};
                if(current.parent != parent.incarnation || current.incarnation == parent.incarnation ||
                   parent.runtime_epoch != current.runtime_epoch) { return; }
                auto const* manager{manager_for(current, capture->code_owners_[index])};
                if(manager == nullptr || manager_for(parent, capture->code_owners_[index - 1u]) == nullptr) { return; }
                cfi::native_debug_cfi_row row{};
                if(!manager->copy_debug_native_cfi_row(frame_begin, frame_end, frame_pc, row) || !row.usable || row.cfa_register >= register_count ||
                   !(known & (1u << row.cfa_register)) || !(known & (1u << sp_register)) || registers[sp_register] != sp ||
                   row.registers[ra_register].kind == cfi::native_debug_cfi_rule_kind::unavailable) { return; }
#if defined(__x86_64__)
                if(row.registers[16u].kind != cfi::native_debug_cfi_rule_kind::cfa_memory ||
                   row.registers[16u].offset != -8) { return; }
#elif defined(__i386__) && __SIZEOF_POINTER__ == 4
                if(row.registers[8u].kind != cfi::native_debug_cfi_rule_kind::cfa_memory ||
                   row.registers[8u].offset != -4) { return; }
#endif
                ::std::uintptr_t cfa{};
                if(!add(registers[row.cfa_register], row.cfa_offset, cfa) ||
                   !capture->native_stack_owner_->contains_frame(thread, sp, cfa)) { return; }
                #if defined(__i386__) && __SIZEOF_POINTER__ == 4
                using owned_word = cfi::native_debug_cfi_i386_owned_word;
#else
                using owned_word = cfi::native_debug_cfi_owned_word;
#endif
                constexpr auto slot_width{sizeof(::std::uintptr_t)};
                ::std::array<owned_word, ::std::size(preserved)> slots{};
                ::std::size_t slot_count{};
                for(unsigned n : preserved)
                {
                    auto& rule{row.registers[n]}; using K = cfi::native_debug_cfi_rule_kind;
                    if(rule.kind != K::cfa_memory && rule.kind != K::register_memory) { continue; }
                    ::std::uintptr_t address{};
                    bool const bounded{rule.kind == K::cfa_memory ? add(cfa, rule.offset, address) :
                        rule.reg < register_count && (known & (1u << rule.reg)) && add(registers[rule.reg], rule.offset, address)};
                    ::std::array<::std::byte, slot_width> word{};
                    if(!bounded || address < sp || address >= cfa || cfa - address < slot_width ||
#if defined(__i386__) && __SIZEOF_POINTER__ == 4
                       !capture->native_stack_owner_->copy_word32(thread, sp, cfa, address, word.data()))
#else
                       !capture->native_stack_owner_->copy_word(thread, sp, cfa, address, word.data()))
#endif
                    {
                        rule = {}; // Unknown stays unknown in the next frame.
                        if(n == ra_register) { return; }
                        continue;
                    }
                    bool present{};
                    for(::std::size_t slot{}; slot != slot_count; ++slot)
                    { if(slots[slot].address == address) { present = true; break; } }
                    if(!present)
                    {
                        auto& saved{slots[slot_count++]}; saved.address = address;
                        ::std::memcpy(::std::addressof(saved.value), word.data(), slot_width);
                    }
                }
                #if defined(__riscv) && __riscv_xlen == 64
                cfi::native_debug_cfi_riscv64_caller recovered{};
                if(!cfi::evaluate_native_debug_cfi_riscv64_sparse
#elif (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8)
                cfi::native_debug_cfi_aarch64_caller recovered{};
                if(!cfi::evaluate_native_debug_cfi_aarch64_sparse
#elif defined(__loongarch64) && __SIZEOF_POINTER__ == 8
                cfi::native_debug_cfi_loongarch64_caller recovered{};
                if(!cfi::evaluate_native_debug_cfi_loongarch64_sparse
#elif (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips))
                cfi::native_debug_cfi_mips64_caller recovered{};
                if(!cfi::evaluate_native_debug_cfi_mips64_sparse
#elif defined(__i386__) && __SIZEOF_POINTER__ == 4
                cfi::native_debug_cfi_i386_caller recovered{};
                if(!cfi::evaluate_native_debug_cfi_i386_sparse
#else
                cfi::native_debug_cfi_caller recovered{};
                if(!cfi::evaluate_native_debug_cfi_x64_sparse
#endif
                    (row, registers, known, sp, cfa,
                    {slots.data(), slot_count}, recovered) || recovered.cfa != cfa) { return; }
                ::std::uintptr_t parent_begin{}, parent_end{};
                if(!debug_resolve_actual_native_function_body(parent.module, parent.function, parent.function_generation,
                    parent.runtime_epoch, recovered.return_pc, parent_begin, parent_end) || parent_end <= parent_begin ||
                   parent_end - parent_begin > static_cast<::std::uintptr_t>(PTRDIFF_MAX) || recovered.return_pc <= parent_begin) { return; }
                llvm_jit_debug_native_target target{};
                if(!debug_copy_actual_native_target(parent.module, parent.function, parent.function_generation,
                    capture->code_owners_[index - 1u], target)) { return; }
                // [exact published Wasm parent body ... end]
                // [safe] SAME native/domain/publication guards. A return
                // into VM/host code refuses before decoding that code.
                // This private code borrow survives only these SAME native,
                // domain and publication guards; no pointer or byte escapes.
                auto const* parent_code{reinterpret_cast<unsigned char const*>(parent_begin)};
                bool call_return{};
#if (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)
                code_key const parent_key{parent_begin,parent_end,parent.module,parent.function,
                    parent.function_generation,parent.runtime_epoch,capture->code_owners_[index-1u]};
                for(::std::size_t n{}; n != boundary_count; ++n)
                {
                    if(boundaries[n].key == parent_key && boundaries[n].return_pc == recovered.return_pc)
                    { call_return = true; break; }
                }
                if(!call_return)
#endif
                {
                    ::uwvm2::uwvm::debugger::native_owned_instruction_semantics::decoder decoder{target};
#if (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips))
                    // N64 R2 returns after BOTH the call and its delay slot.
                    // Only bytes in this exact published Wasm parent are read;
                    // a VM/trampoline target never becomes caller authority.
                    if((parent_begin & 3u) || (recovered.return_pc & 3u) ||
                       recovered.return_pc - parent_begin < 8u) { return; }
                    auto const call_pc{recovered.return_pc - 8u};
                    auto const call{decoder.decode(call_pc,{parent_code + (call_pc - parent_begin),4u})};
                    auto const delay{decoder.decode(call_pc + 4u,{parent_code + (call_pc - parent_begin) + 4u,4u})};
                    if(!call || call.semantics().size != 4u || !call.safe_for_call_continuation() ||
                       call.call_delay_bytes() != 4u || !delay || delay.semantics().size != 4u ||
                       !delay.safe_for_single_instruction()) { return; }
                    call_return = true;
#elif (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8)
                    // A64 and LoongArch instructions are exactly four aligned bytes. The
                    // exact owned entry and CFI-recovered return PC prove this
                    // boundary without rescanning a large parent prologue.
                    // Actual MC must still classify the preceding instruction
                    // as a conventional returning call; no address is granted.
                    if((parent_begin & 3u) || (recovered.return_pc & 3u) ||
                       recovered.return_pc - parent_begin < 4u) { return; }
                    auto const call_pc{recovered.return_pc - 4u};
                    auto const decoded{decoder.decode(call_pc,{parent_code + (call_pc - parent_begin),4u})};
                    if(!decoded || decoded.semantics().size != 4u || !decoded.safe_for_call_continuation()) { return; }
                    call_return = true;
#else
                    ::std::uintptr_t position{parent_begin};
                    while(position < recovered.return_pc)
                    {
                        auto const decoded{decoder.decode(position, {parent_code + (position - parent_begin),
                            static_cast<::std::size_t>(parent_end - position)})};
                        if(!decoded || decoded.semantics().size == 0u || decoded.semantics().size > parent_end - position) { return; }
                        position += decoded.semantics().size;
                        if(position == recovered.return_pc) { call_return = decoded.safe_for_call_continuation(); }
                    }
                    if(position != recovered.return_pc || !call_return) { return; }
#endif
#if (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)
                    if(boundary_count == boundaries.size()) { return; }
                    boundaries[boundary_count++] = {parent_key,recovered.return_pc};
#endif
                }
                ::std::uintptr_t parent_sp{recovered.cfa};
#if (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)
                // Validate the actual software-step return CFA for EVERY recovered
                // frame, including a backtrace-only query with no event seal.
                constexpr bool need_return_pop{true};
#else
                bool const need_return_pop{proof != nullptr && out.count == 0u};
#endif
                if(need_return_pop)
                {
                    // Some Wasm calling conventions pop an argument-area
                    // reservation in the epilogue. CFA identifies the return slot
                    // but is not necessarily the caller SP after that RET. Derive
                    // the adjustment from this SAME owned emitted child body;
                    // never guess an ABI constant or relax exact SP matching.
                    ::std::size_t pop{SIZE_MAX};
#if (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)
                    code_key const child_key{frame_begin,frame_end,current.module,current.function,
                        current.function_generation,current.runtime_epoch,capture->code_owners_[index]};
                    for(::std::size_t n{}; n != return_count; ++n)
                    { if(returns[n].key == child_key) { pop = returns[n].pop; break; } }
                    if(pop == SIZE_MAX)
#endif
                    {
                    auto const length{frame_end - frame_begin};
                    llvm_jit_debug_native_target child_target{};
                    if(length == 0u || length > static_cast<::std::uintptr_t>(PTRDIFF_MAX) || !debug_copy_actual_native_target(current.module,current.function,
                        current.function_generation,capture->code_owners_[index],child_target)) { return; }
                    auto const* child_code{reinterpret_cast<unsigned char const*>(frame_begin)};
                    ::uwvm2::uwvm::debugger::native_owned_instruction_semantics::decoder child_decoder{child_target};
                    ::std::size_t offset{};
                    while(offset != length)
                    {
                        auto const instruction{child_decoder.decode(frame_begin + offset,{child_code + offset,length - offset})};
                        if(!instruction || instruction.semantics().size == 0u || instruction.semantics().size > length - offset) { return; }
                        if(instruction.semantics().kind == ::uwvm2::uwvm::debugger::native_instruction_semantics::flow::return_instruction)
                        {
#if (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips))
                            // Conventional C ABI: prove restored SP at the actual
                            // JR RA and a non-control delay with no ABI operands.
                            // A delayed stack/link restore is not guessed here.
                            if(instruction.semantics().size != 4u || length - offset < 8u) { return; }
                            auto const delay{child_decoder.decode(frame_begin + offset + 4u,
                                {child_code + offset + 4u,4u})};
                            cfi::native_debug_cfi_row epilogue{};
                            if(!delay || delay.semantics().size != 4u || !delay.safe_for_single_instruction() ||
                               !delay.safe_for_public_display() ||
                               !manager->copy_debug_native_cfi_row(frame_begin,frame_end,frame_begin+offset,epilogue) ||
                               !epilogue.usable || epilogue.cfa_register != sp_register || epilogue.cfa_offset != 0 ||
                               epilogue.registers[ra_register].kind != cfi::native_debug_cfi_rule_kind::same) { return; }
                            constexpr ::std::size_t amount{};
#elif (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8)
                            // The exact emitted return row accounts for the
                            // actual ABI (including TailCC argument-area pops).
                            // LoongArch uses C and must prove its zero adjustment. This
                            // private adjustment never guesses an ABI constant.
                            cfi::native_debug_cfa_row epilogue{};
                            if(!manager->copy_debug_native_cfa_row(frame_begin,frame_end,frame_begin+offset,epilogue) ||
                               !epilogue.usable || epilogue.cfa_register != sp_register || epilogue.cfa_offset > 0) { return; }
                            auto const amount{static_cast<::std::size_t>(-static_cast<::std::int64_t>(epilogue.cfa_offset))};
#else
                            auto const amount{instruction.near_return_pop_bytes()};
#endif
                            if(amount == SIZE_MAX || (pop != SIZE_MAX && pop != amount)) { return; }
                            pop = amount;
                        }
                        offset += instruction.semantics().size;
                    }
#if (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)
                    if(pop != SIZE_MAX)
                    {
                        if(return_count == returns.size()) { return; }
                        returns[return_count++] = {child_key,pop};
                    }
#endif
                    }
                    if(pop == SIZE_MAX || pop > UINTPTR_MAX - recovered.cfa ||
                       !capture->native_stack_owner_->contains_frame(thread,sp,recovered.cfa + pop)) { return; }
                    parent_sp = recovered.cfa + pop;
#if (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips))
                    // A stale epilogue row can claim the current call site is
                    // already a zero-sized restored frame. It cannot prove a
                    // second physical activation at the identical PC and SP.
                    if(parent_sp == sp && recovered.return_pc == frame_pc &&
                       parent_begin == frame_begin && parent_end == frame_end) { return; }
#endif
                    if(proof != nullptr && out.count == 0u)
                    {
                    // The target must be a real decoded return into the
                    // actual Wasm parent. This is sealed BEFORE any
                    // native/domain/publication guard is released.
                    *proof = {thread, parent.module, parent.function, parent.function_generation, parent.runtime_epoch,
                        revision, parent.incarnation, current.incarnation, pc, static_cast<::std::uintptr_t>(raw.sp()),
                        begin, end, recovered.return_pc, recovered.cfa, recovered.cfa + pop, parent_begin, parent_end,
                        capture->code_owners_[index], capture->code_owners_[index - 1u]};
#if (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips))
                    // Same-PC recursive returns can occur at another SP. The
                    // sealed ordinary instruction and its successor permit one
                    // REAL instruction to pass, then re-arm the exact return PC.
                    ::uwvm2::uwvm::debugger::native_owned_instruction_semantics::decoder decoder{target};
                    auto const next{decoder.decode(recovered.return_pc,{parent_code+(recovered.return_pc-parent_begin),parent_end-recovered.return_pc})};
                    constexpr auto width{step::details::breakpoint_width()};
                    if(!next || !next.safe_for_single_instruction() || next.semantics().size < width ||
                       next.semantics().size > parent_end-recovered.return_pc ||
                       width > parent_end-recovered.return_pc-next.semantics().size) { *proof={}; return; }
                    proof->software_skip=recovered.return_pc+next.semantics().size;
                    for(unsigned b{}; b!=width; ++b)
                    {
                        proof->software_original[b]=parent_code[recovered.return_pc-parent_begin+b];
                        proof->software_skip_original[b]=parent_code[proof->software_skip-parent_begin+b];
                    }
#endif
                    }
                }
                out.frames[out.count++] = {parent.module, parent.function, parent.function_generation, parent.runtime_epoch,
                    parent.incarnation, current.incarnation, true};
                // Stop at the actual logical Wasm root WITHOUT reading
                // its return slot, saved host registers or host caller.
                if(index == 1u) { out.complete = parent.parent == 0u; return; }
                #if defined(__i386__) && __SIZEOF_POINTER__ == 4
                for(unsigned n{}; n != 9u; ++n) { registers[n] = recovered.registers[n]; }
#else
                registers = recovered.registers;
#endif
                known = recovered.known;
#if (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)
                sp = parent_sp; registers[sp_register] = sp;
#else
                sp = recovered.cfa;
#endif
                frame_pc = recovered.return_pc; frame_begin = parent_begin; frame_end = parent_end;
            }
            // Limit exhaustion is an explicitly partial proved prefix.
        }));
    }
    bool llvm_jit_debug_native_return_continuation::resume(void* context,
        llvm_jit_debug_native_return_resume_callback callback) const
    {
        if(!cursor_ || session_ == nullptr || callback == nullptr || cursor_->session_ != session_ ||
           evidence_.revision == 0u || evidence_.revision == UINT64_MAX) { return false; }
        auto const& capture{cursor_->capture_};
        if(!debug_activation_canonical_capture(capture) || !capture->control_ || !capture->ticket_ || !capture->native_stack_owner_ ||
           cursor_->count_ < 2u || capture->snapshot_.frames.size() != cursor_->count_ ||
           capture->code_owners_.size() != cursor_->count_) { return false; }
        auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; if(!gc_reader) { return false; }
        auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
        return capture->control_->with_externally_parked_participant_for_resume(
            capture->ticket_, capture->snapshot_.participant, [&](auto const actual, auto& borrow) noexcept
        {
            runtime_state_publication_guard publication{};
            try
            {
                llvm_jit_debug_native_backtrace_view trace{}; evidence current{};
                observe_native_locked(cursor_, session_, actual, true, trace, 1u, ::std::addressof(current));
                if(trace.count != 1u || !trace.frames[0u].valid || !(current == evidence_)) { return; }
                // Native snapshot read guard has ended, but this SAME domain
                // mutex and publication guard survive through the callback's
                // actual one-shot wake. Replacement/reset cannot split the
                // fresh CFI/owner/epoch proof from the execution transition.
                // Only Wasm identity leaves the unwinder, never RA/CFA or slots.
                callback(context, trace.frames[0u], borrow);
            }
            catch(...) {} // No commit occurred; the real external stop survives.
        });
    }
    bool llvm_jit_debug_native_return_continuation::resume_event(
        llvm_jit_debug_native_return_continuation_owner const& owner,
        void* context, llvm_jit_debug_native_return_event_callback callback) const
    {
        if(!cursor_ || !session_ || !callback || cursor_->session_ != session_ ||
           evidence_.revision == 0u || evidence_.revision == UINT64_MAX) { return false; }
        auto const& capture{cursor_->capture_};
        if(!debug_activation_canonical_capture(capture) || !capture->control_ || !capture->ticket_ ||
           !capture->native_stack_owner_ || cursor_->count_ < 2u ||
           capture->snapshot_.frames.size() != cursor_->count_ || capture->code_owners_.size() != cursor_->count_) { return false; }
        // All allocation and reference retention precede admission/domain guards.
        // This cursor is exclusively a sealed event provider, NEVER registered
        // as a logical capture and NEVER publishes the old copied parent locals.
        auto parent{::std::shared_ptr<llvm_jit_debug_native_activation_cursor>{new llvm_jit_debug_native_activation_cursor}};
        parent->capture_ = capture; parent->ledger_ = cursor_->ledger_; parent->session_ = session_;
        parent->thread_ = cursor_->thread_; parent->begin_ = evidence_.parent_begin;
        parent->end_ = evidence_.parent_end; parent->first_ = evidence_.return_pc;
        parent->count_ = cursor_->count_ - 1u;
        parent->expected_storage_ = ::std::make_unique<details::debug_activation::frame[]>(parent->count_);
        for(::std::size_t n{}; n != parent->count_; ++n) { parent->expected_storage_[n] = cursor_->expected_[n]; }
        parent->expected_ = parent->expected_storage_.get(); parent->return_origin_ = cursor_;
        parent->return_stack_ = evidence_.target_sp;
        auto window{::std::make_shared<llvm_jit_debug_native_return_event::resume_window>()};
        llvm_jit_debug_native_return_event event{window,owner,parent,parent->provider(parent),session_,evidence_.thread,
            evidence_.origin_pc,evidence_.origin_sp,evidence_.origin_begin,evidence_.origin_end,evidence_.revision,
            evidence_.return_pc,evidence_.target_sp,evidence_.parent_begin,evidence_.parent_end,
            evidence_.software_skip,evidence_.software_original,evidence_.software_skip_original};
        auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; if(!gc_reader) { return false; }
        auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
        return capture->control_->with_externally_parked_participant_for_resume(
            capture->ticket_, capture->snapshot_.participant, [&](auto const actual, auto& borrow) noexcept
        {
            runtime_state_publication_guard publication{};
            try
            {
                llvm_jit_debug_native_backtrace_view trace{}; evidence current{};
                observe_native_locked(cursor_,session_,actual,true,trace,1u,::std::addressof(current));
                if(trace.count != 1u || !trace.frames[0u].valid || !(current == evidence_)) { return; }
                // Exact same domain/publication remain held through event enable
                // and backend wake. A prior scalar target query grants nothing.
                // The sealed backend borrow is live only in THIS issuing host
                // callback. Retained plans cannot bypass later revalidation.
                window->active.store(true,::std::memory_order_release);
                callback(context,event,borrow);
                window->active.store(false,::std::memory_order_release);
            }
            catch(...) {} // Without commit, original external stop survives.
        });
    }
    template<typename Inspect>
    bool llvm_jit_debug_native_return_continuation::inspect_event(
        llvm_jit_debug_native_return_event const& event, void const* session, Inspect&& inspect)
    {
        auto const& origin{event.proof_->cursor_}; auto const& parent{event.parent_};
        auto const& proof{event.proof_->evidence_};
        if(!parent || !origin || event.proof_->session_ != session || parent->session_ != session ||
           parent->return_origin_.get() != origin.get() || parent->count_ + 1u != origin->count_) { return false; }
        auto const& capture{origin->capture_};
        if(!debug_activation_canonical_capture(capture) || !capture->control_ || !capture->ticket_) { return false; }
        auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; if(!gc_reader) { return false; }
        auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
        bool valid{};
        bool const stopped{capture->control_->with_parked_participant(capture->ticket_,capture->snapshot_.participant,
            [&](auto actual, bool external)
        {
            runtime_state_publication_guard publication{};
            // Original domain ticket/location establishes ownership of this
            // kernel event. It is NOT a rebased public parent/source snapshot.
            if(!external || !capture->matches_native_publication_locked(actual, external)) { return; }
            static_cast<void>(::uwvm2::uwvm::debugger::native_step::with_owned_registers_and_revision(session,
                [&](auto thread, auto pc, auto begin, auto end, auto const& raw, auto revision) noexcept
            {
                if(thread != proof.thread || pc != proof.return_pc || begin != proof.parent_begin || end != proof.parent_end ||
                   revision != proof.revision + 1u || raw.pc() != pc || raw.sp() != proof.target_sp ||
                   !((raw.machine == ::uwvm2::uwvm::debugger::native_registers::architecture::x86_64 && raw.size() == 18u) ||
                 (raw.machine == ::uwvm2::uwvm::debugger::native_registers::architecture::riscv64 && raw.size() == 34u) ||
                 (raw.machine == ::uwvm2::uwvm::debugger::native_registers::architecture::aarch64 && raw.size() == 34u) ||
                 (raw.machine == ::uwvm2::uwvm::debugger::native_registers::architecture::i686 && raw.size() == 10u) ||
                 (raw.machine == ::uwvm2::uwvm::debugger::native_registers::architecture::loongarch64 && raw.size() == 34u) ||
                 (raw.machine == ::uwvm2::uwvm::debugger::native_registers::architecture::mips64 && raw.size() == 36u)) ||
                   !parent->witnessed_ || parent->retirement_ != 4u || parent->witnessed_pc_ != pc || parent->witnessed_sp_ != raw.sp() ||
                   origin->retirement_ != 0u || !parent->ledger_->matches_native_chain(parent->expected_,parent->count_) ||
                   !origin->ledger_->matches_native_retired_prefix(origin->expected_,origin->count_,details::debug_activation::exit_kind::returned)) { return; }
                ::std::uintptr_t actual_begin{}, actual_end{};
                if(!debug_resolve_actual_native_function_body(proof.module,proof.function,proof.generation,proof.epoch,
                    pc,actual_begin,actual_end) || actual_begin != begin || actual_end != end) { return; }
                valid = inspect(parent, proof);
            }));
        })};
        return stopped && valid;
    }
    bool llvm_jit_debug_native_return_continuation::query_event(
        llvm_jit_debug_native_return_event const& event, void const* session,
        llvm_jit_debug_native_caller_view& out)
    {
        return inspect_event(event, session, [&](auto const&, auto const& proof) noexcept
        {
            out = {proof.module,proof.function,proof.generation,proof.epoch,proof.parent_incarnation,proof.child_incarnation,true};
            return true;
        });
    }
#endif
    extern "C++" bool llvm_jit_debug_native_backtrace_host_api(
        llvm_jit_debug_native_activation_cursor_owner const& supplied, void const* session,
        llvm_jit_debug_native_backtrace_view& out, ::std::size_t maximum_frames) noexcept
    { return llvm_jit_debug_native_return_continuation::observe(supplied, session, out, maximum_frames, nullptr); }
    namespace
    {
        ::std::mutex g_debug_native_return_continuation_mutex{};
        ::std::weak_ptr<llvm_jit_debug_native_return_continuation const> g_debug_native_return_continuations[256u]{};
        [[nodiscard]] llvm_jit_debug_native_return_continuation_owner canonical_native_return_continuation(
            llvm_jit_debug_native_return_continuation_owner const& supplied) noexcept
        {
            if(!supplied) { return {}; }
            ::std::lock_guard lock{g_debug_native_return_continuation_mutex};
            for(auto const& slot : g_debug_native_return_continuations)
            {
                auto const owner{slot.lock()};
                // Compare both identities BEFORE dereferencing an untrusted
                // alias, even when its pointer equals a real owner's pointer.
                if(owner && owner.get() == supplied.get() && !owner.owner_before(supplied) && !supplied.owner_before(owner))
                { return owner; }
            }
            return {};
        }
    }
    extern "C++" llvm_jit_debug_native_return_continuation_owner llvm_jit_debug_mint_native_return_continuation_host_api(
        llvm_jit_debug_native_activation_cursor_owner const& supplied, void const* session) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && ((__SIZEOF_POINTER__ == 8 && (defined(__x86_64__) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || defined(__loongarch64) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)) && \
    LLVM_VERSION_MAJOR >= 23 && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        try
        {
            auto const cursor{canonical_native_activation_cursor(supplied)};
            if(!cursor || session == nullptr) { return {}; }
            auto candidate{::std::shared_ptr<llvm_jit_debug_native_return_continuation>{new llvm_jit_debug_native_return_continuation}};
            llvm_jit_debug_native_backtrace_view trace{};
            if(!llvm_jit_debug_native_return_continuation::observe(cursor, session, trace, 1u, ::std::addressof(candidate->evidence_)) ||
               trace.count != 1u || !trace.frames[0u].valid) { return {}; }
            candidate->cursor_ = cursor; candidate->session_ = session;
            // A bounded weak registry never evicts live proofs. The strong
            // handle retains the originating cursor/ticket/stack registration,
            // but does not independently keep a departed OS stack alive.
            ::std::lock_guard lock{g_debug_native_return_continuation_mutex};
            for(auto& slot : g_debug_native_return_continuations)
            { if(slot.expired()) { slot = candidate; return candidate; } }
            return {};
        }
        catch(...) { return {}; }
#else
        (void)supplied; (void)session; return {};
#endif
    }
    extern "C++" bool llvm_jit_debug_native_return_continuation_host_api(
        llvm_jit_debug_native_return_continuation_owner const& supplied, void const* session,
        llvm_jit_debug_native_caller_view& out) noexcept
    {
        out = {};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && ((__SIZEOF_POINTER__ == 8 && (defined(__x86_64__) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || defined(__loongarch64) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)) && \
    LLVM_VERSION_MAJOR >= 23 && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        try
        {
            auto const owner{canonical_native_return_continuation(supplied)};
            if(!owner || session == nullptr || owner->session_ != session) { return false; }
            llvm_jit_debug_native_return_continuation::evidence current{};
            llvm_jit_debug_native_backtrace_view trace{};
            // Re-read genuine CFI/owned stack and parent engine inside ONE
            // original domain/publication/native gate. Actual trap revision
            // rejects later reuse of the same physical PC and SP.
            if(!llvm_jit_debug_native_return_continuation::observe(owner->cursor_, session, trace, 1u, ::std::addressof(current)) ||
               trace.count != 1u || !trace.frames[0u].valid || !(current == owner->evidence_)) { return false; }
            out = trace.frames[0u]; return true;
        }
        catch(...) { out = {}; return false; }
#else
        (void)supplied; (void)session; return false;
#endif
    }
    extern "C++" bool llvm_jit_debug_resume_native_return_continuation_host_api(
        llvm_jit_debug_native_return_continuation_owner const& supplied, void const* session,
        void* context, llvm_jit_debug_native_return_resume_callback callback) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && ((__SIZEOF_POINTER__ == 8 && (defined(__x86_64__) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || defined(__loongarch64) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)) && \
    LLVM_VERSION_MAJOR >= 23 && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(session == nullptr || callback == nullptr || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const owner{canonical_native_return_continuation(supplied)};
            if(!owner || owner->session_ != session || !canonical_native_activation_cursor(owner->cursor_)) { return false; }
            return owner->resume(context, callback);
        }
        catch(...) { return false; }
#else
        (void)supplied; (void)session; (void)context; (void)callback; return false;
#endif
    }
    extern "C++" bool llvm_jit_debug_native_caller_host_api(
        llvm_jit_debug_native_activation_cursor_owner const& supplied, void const* session,
        llvm_jit_debug_native_caller_view& out) noexcept
    {
        out = {};
        llvm_jit_debug_native_backtrace_view trace{};
        if(!llvm_jit_debug_native_backtrace_host_api(supplied, session, trace, 1u)) { return false; }
        out = trace.frames[0u]; return true;
    }
    extern "C++" bool llvm_jit_debug_resume_native_return_event_host_api(
        llvm_jit_debug_native_return_continuation_owner const& supplied, void const* session,
        void* context, llvm_jit_debug_native_return_event_callback callback) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && ((__SIZEOF_POINTER__ == 8 && (defined(__x86_64__) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || defined(__loongarch64) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)) && \
    LLVM_VERSION_MAJOR >= 23 && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(!session || !callback || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const owner{canonical_native_return_continuation(supplied)};
            return owner && owner->session_ == session && canonical_native_activation_cursor(owner->cursor_) &&
                owner->resume_event(owner,context,callback);
        }
        catch(...) { return false; }
#else
        (void)supplied; (void)session; (void)context; (void)callback; return false;
#endif
    }
    extern "C++" bool llvm_jit_debug_query_native_return_event_host_api(
        llvm_jit_debug_native_return_event const& event, void const* session,
        llvm_jit_debug_native_caller_view& out) noexcept
    {
        out = {};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && ((__SIZEOF_POINTER__ == 8 && (defined(__x86_64__) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || defined(__loongarch64) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)) && \
    LLVM_VERSION_MAJOR >= 23 && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        if(!event.valid() || !session || !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            // Private sealed owner is compared against the original canonical
            // registry BEFORE its actual capture/ledger/provider is inspected.
            auto const owner{canonical_native_return_continuation(event.proof_)};
            return owner && owner->session_ == session && owner->query_event(event,session,out);
        }
        catch(...) { out = {}; return false; }
#else
        (void)event; (void)session; return false;
#endif
    }
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && ((__SIZEOF_POINTER__ == 8 && (defined(__x86_64__) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || defined(__loongarch64) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)) && \
    LLVM_VERSION_MAJOR >= 23 && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
    bool llvm_jit_debug_native_return_continuation::capture_event(
        llvm_jit_debug_native_return_event const& event, void const* session,
        llvm_jit_debug_native_return_stop& out)
    {
        if(!event.valid()) { return false; }
        auto const proof{canonical_native_return_continuation(event.proof_)};
        if(!proof || proof->session_ != session || !canonical_native_activation_cursor(proof->cursor_)) { return false; }
        auto const& origin{proof->cursor_->capture_};
        if(!debug_activation_canonical_capture(origin) || origin->snapshot_.frames.size() < 2u ||
           origin->code_owners_.size() != origin->snapshot_.frames.size()) { return false; }
        // Allocate and copy immutable identities before domain/publication/native
        // admission. Only the exact final kernel witness may publish this copy.
        auto candidate{::std::shared_ptr<llvm_jit_debug_activation_capture>{new llvm_jit_debug_activation_capture}};
        candidate->control_ = origin->control_; candidate->ticket_ = origin->ticket_;
        candidate->native_return_anchor_ = origin->native_return_anchor_ ? origin->native_return_anchor_ : origin;
        candidate->snapshot_ = origin->snapshot_; candidate->snapshot_.frames.pop_back();
        candidate->code_owners_ = origin->code_owners_; candidate->code_owners_.pop_back();
        candidate->native_ledger_owner_ = origin->native_ledger_owner_;
        candidate->native_stack_owner_ = origin->native_stack_owner_;
        auto const& identity{candidate->snapshot_.frames.back()};
        // Zero is an unavailable cooperative offset, never a source-PC claim.
        // Native provenance resolves the real parent kernel PC independently.
        candidate->snapshot_.location = {identity.module,identity.function,0u,identity.runtime_epoch};
        auto const& evidence{proof->evidence_};
        candidate->native_site_ = {candidate->snapshot_.participant,evidence.thread,
            evidence.return_pc,evidence.parent_begin,evidence.parent_end,true};
        candidate->native_code_site_ = {candidate->snapshot_.participant,
            evidence.return_pc,evidence.parent_begin,evidence.parent_end,true};
        return inspect_event(event,session,[&](auto const& parent, auto const& current) noexcept
        {
            if(parent->count_ != candidate->snapshot_.frames.size() ||
               current.module != identity.module || current.function != identity.function ||
               current.generation != identity.function_generation || current.epoch != identity.runtime_epoch ||
               current.parent_incarnation != identity.incarnation) { return false; }
            // Exact trap remains parked. Registry publication is all-or-nothing;
            // live owners are never evicted, and no signal callback takes locks
            // or touches capture_ ownership. Lock order is native -> capture ->
            // cursor; existing canonical lookups release before native admission.
            ::std::lock_guard capture_registry{g_debug_activation_capture_mutex};
            ::std::lock_guard cursor_registry{g_debug_native_activation_cursor_mutex};
            auto const& existing{parent->capture_};
            // A new event initially retains its originating capture, which
            // may itself already be native-only after an earlier return. Only
            // a DIFFERENT capture marks publication of THIS event's parent.
            // Testing the anchor alone confuses a chained origin with a repeat.
            if(existing.get() != origin.get())
            {
                if(!existing || !existing->native_return_anchor_ ||
                   existing->snapshot_.frames.size() != parent->count_ ||
                   existing->code_owners_.size() != parent->count_ ||
                   existing->native_return_anchor_.get() != candidate->native_return_anchor_.get()) { return false; }
                bool found_capture{}, found_cursor{};
                for(auto const& slot : g_debug_activation_captures)
                { auto const value{slot.lock()}; if(value && value.get()==existing.get() &&
                    !value.owner_before(existing) && !existing.owner_before(value)) { found_capture=true; } }
                for(auto const& slot : g_debug_native_activation_cursors)
                { auto const value{slot.lock()}; if(value && value.get()==parent.get() &&
                    !value.owner_before(parent) && !parent.owner_before(value)) { found_cursor=true; } }
                if(!found_capture || !found_cursor) { return false; }
                out = {existing,parent}; return true;
            }
            ::std::weak_ptr<llvm_jit_debug_activation_capture const>* capture_slot{};
            ::std::weak_ptr<llvm_jit_debug_native_activation_cursor const>* cursor_slot{};
            for(auto& slot : g_debug_activation_captures) { if(slot.expired()) { capture_slot=::std::addressof(slot); break; } }
            for(auto& slot : g_debug_native_activation_cursors) { if(slot.expired()) { cursor_slot=::std::addressof(slot); break; } }
            if(!capture_slot || !cursor_slot) { return false; }
            // The sealed event owns this exact mutable private provider. It is
            // fully kernel-witnessed and externally parked; capture_ changes
            // once, before cold registration, and never on the worker/signal path.
            auto* mutable_parent{const_cast<llvm_jit_debug_native_activation_cursor*>(parent.get())};
            mutable_parent->capture_ = candidate;
            *capture_slot = candidate; *cursor_slot = parent; out = {candidate,parent};
            return true;
        });
    }
#endif
    extern "C++" bool llvm_jit_debug_capture_native_return_stop_host_api(
        llvm_jit_debug_native_return_event const& event, void const* session,
        llvm_jit_debug_native_return_stop& out) noexcept
    {
        out = {};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && ((__SIZEOF_POINTER__ == 8 && (defined(__x86_64__) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || defined(__loongarch64) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)) && \
    LLVM_VERSION_MAJOR >= 23 && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(!session || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try { return llvm_jit_debug_native_return_continuation::capture_event(event,session,out); }
        catch(...) { out = {}; return false; }
#else
        (void)event; (void)session; return false;
#endif
    }

#endif

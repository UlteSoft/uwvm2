// Private selection runs only under execution/domain/publication ownership.
#if defined(UWVM_RUNTIME_LLVM_JIT)
namespace
{
    [[nodiscard]] inline details::native_loaded_provenance::image const* debug_native_provenance_for_function_locked(
        ::std::uint_least64_t module_id, ::std::uint_least64_t function_id, ::std::uint_least64_t generation,
        ::std::uint_least64_t epoch, ::std::size_t& expression_size) noexcept
    {
        expression_size = 0u;
        if(module_id >= g_runtime.modules.size() || generation == 0u || epoch == 0u) { return nullptr; }
        auto const& record{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(module_id))};
        auto const* module{record.runtime_module};
        if(module == nullptr || !record.llvm_jit_ready || !record.llvm_jit_full_publication ||
           !record.llvm_jit_full_publication->engine || !record.llvm_jit_full_publication->context) { return nullptr; }
        auto const imports{module->imported_function_vec_storage.size()};
        if(function_id < imports || function_id - imports >= module->local_defined_function_vec_storage.size()) { return nullptr; }
        auto const local{static_cast<::std::size_t>(function_id - imports)};
        if(generation == 1u)
        {
            if(local >= record.llvm_jit_compiled.local_funcs.size()) { return nullptr; }
            auto const& function{record.llvm_jit_compiled.local_funcs.index_unchecked(local)};
            auto const begin{reinterpret_cast<::std::uintptr_t>(function.code_begin)}, end{reinterpret_cast<::std::uintptr_t>(function.code_end)};
            if(begin == 0u || end <= begin) { return nullptr; }
            expression_size = end - begin;
            return ::std::addressof(record.llvm_jit_full_publication->native_provenance);
        }
        details::native_loaded_provenance::image const* result{};
        for(auto const& retained: record.llvm_jit_debug_full_retained_generations)
        {
            if(!retained || !retained->committed || !retained->engine || !retained->context || retained->module_id != module_id ||
               retained->function_index != function_id || retained->local_index != local || retained->expected_generation != generation - 1u ||
               retained->expected_runtime_epoch != epoch) { continue; }
            if(result != nullptr) { return nullptr; }
            result = ::std::addressof(retained->native_provenance); expression_size = retained->debug_expression_size;
        }
        return result;
    }
}
#endif

// Cold native-code copy. The supplied capture is first canonicalized by the
// private runtime registry. No public native_site, decimal PC or DAP label is
// read authority. ONE real domain callback pins the actual stopped participant;
// its external park additionally requires the real active native trap session.
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    extern "C++" bool llvm_jit_debug_copy_native_code_host_api(
        llvm_jit_debug_activation_capture_owner const& capture, void const* native_session_identity,
        llvm_jit_debug_native_code_bytes& out) noexcept
    {
        out = {};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(!capture || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const canonical{debug_activation_canonical_capture(capture)};
            // [private registry-owned immutable capture] or null
            // [safe                                   ] compare control blocks
            //  ^^ BEFORE dereferencing any caller-provided pointer/alias.
            if(!canonical || !canonical->control_ || !canonical->ticket_ || canonical->snapshot_.participant == 0u) { return false; }
            if(canonical->snapshot_.frames.empty()) { return false; }
            auto const& frame{canonical->snapshot_.frames.back()};
            auto const identity{::fast_io::concat_fast_io("uwvm-m", ::fast_io::mnp::dec(frame.module),
                "-f", ::fast_io::mnp::dec(frame.function), "-g", ::fast_io::mnp::dec(frame.function_generation), ".wasm-native-v1")};
            // Real cold native reader excludes the original managed sweep.
            auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()};
            if(!gc_reader) { return false; }
            auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
            bool valid{};
            auto const stopped{canonical->control_->with_parked_participant(canonical->ticket_, canonical->snapshot_.participant,
                [&](auto actual, bool const external)
            {
                runtime_state_publication_guard publication{};
                if(!canonical->matches_native_publication_locked(actual, external) || external != (native_session_identity != nullptr)) { return; }
                auto const& site{canonical->native_code_site_};
                if(!site.valid || site.participant != canonical->snapshot_.participant ||
                   site.owner_begin == 0u || site.owner_end <= site.owner_begin ||
                   site.return_pc < site.owner_begin || site.return_pc >= site.owner_end) { return; }
                if(external)
                {
                    auto const& actual_step{canonical->native_site_};
                    if(!actual_step.valid || actual_step.participant != site.participant || actual_step.native_thread == 0u ||
                       actual_step.return_pc != site.return_pc || actual_step.owner_begin != site.owner_begin ||
                       actual_step.owner_end != site.owner_end) { return; }
                }
                auto const copy{[&](::std::uint_least64_t native_thread, ::std::uintptr_t pc,
                    ::std::uintptr_t owner_begin, ::std::uintptr_t owner_end) noexcept
                {
                    if((external && native_thread != canonical->native_site_.native_thread) ||
                       owner_begin != site.owner_begin || owner_end != site.owner_end ||
                       pc < owner_begin || pc >= owner_end) { return; }
                    if(canonical->snapshot_.frames.empty()) { return; }
                    ::std::uintptr_t actual_begin{}, actual_end{};
                    if(!debug_resolve_actual_native_function_body(actual.code_unit, actual.function,
                        canonical->snapshot_.frames.back().function_generation, actual.code_generation,
                        pc, actual_begin, actual_end) || actual_begin != owner_begin || actual_end != owner_end) { return; }
                    auto const count{::std::min<::std::size_t>(llvm_jit_debug_max_native_code_bytes, owner_end - pc)};
                    if(count == 0u || canonical->snapshot_.frames.empty()) { return; }
                    llvm_jit_debug_native_code_bytes candidate{};
                    candidate.native_instruction_stop = external;
                    if(canonical->code_owners_.empty() ||
                       !debug_copy_actual_native_target(actual.code_unit, actual.function,
                           canonical->snapshot_.frames.back().function_generation,
                           canonical->code_owners_.back(), candidate.target)) { return; }
                    candidate.pc = pc; candidate.size = count; candidate.participant = site.participant;
                    candidate.module = actual.code_unit; candidate.function = actual.function;
                    candidate.runtime_epoch = actual.code_generation;
                    candidate.function_generation = canonical->snapshot_.frames.back().function_generation;
                    // [actual published function owner_begin ... owner_end)
                    // [safe bytes pc ... pc + count] count <= owner_end - pc;
                    //             ^^ only this exact code-owner interval is borrowed,
                    // with execution lease + domain + publication still held.
                    // In the native path host-transition also keeps the real trap
                    // gate closed. Copy to OWNED bytes; no borrowed pointer escapes.
                    ::std::memcpy(candidate.bytes, reinterpret_cast<void const*>(pc), count);
                    ::std::size_t expression_size{};
                    auto const* rows{debug_native_provenance_for_function_locked(candidate.module, candidate.function,
                        candidate.function_generation, candidate.runtime_epoch, expression_size)};
                    if(rows != nullptr && !rows->code_permissions(candidate.pc, candidate.size, owner_begin, owner_end,
                        {identity.data(), identity.size()}, expression_size, candidate.runtime_epoch, candidate.guest_code))
                    { ::std::memset(candidate.guest_code, 0, candidate.size); }
                    out = candidate; valid = true;
                }};
                if(!external) { copy(0u, site.return_pc, site.owner_begin, site.owner_end); }
                else
                {
                    // Compare to actual active session BEFORE reading its fields;
                    // hold native host-transition through the bounded code copy.
                    // Lock order: lease -> domain -> publication -> native host.
                    // Native release/request never hold host while taking domain.
#if UWVM2_RUNTIME_NATIVE_STEP_PLATFORM
                    static_cast<void>(::uwvm2::uwvm::debugger::native_step::with_owned_registers(native_session_identity,
                        [&](auto thread, auto pc, auto begin, auto end, auto const& registers) noexcept
                    {
                        if(registers.pc() != pc || !canonical->matches_native_cursor_locked(
                            native_session_identity,thread,pc,registers.sp(),begin,end)) { return; }
                        copy(thread,pc,begin,end);
                    }));
#else
                    return; // No real platform trap capability exists here.
#endif
                }
            })};
            if(!stopped || !valid) { out = {}; return false; }
            return true;
        }
        catch(...) { out = {}; return false; }
#else
        (void)capture; (void)native_session_identity; return false;
#endif
    }
#endif

// Whole current native function, copied once for cold bounded forward decoding
// from its real loaded symbol entry. Negative instruction offsets are later
// selected from those owned boundaries; they never scan live bytes backwards.
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    extern "C++" bool llvm_jit_debug_copy_native_function_host_api(
        llvm_jit_debug_activation_capture_owner const& capture, void const* native_session_identity,
        llvm_jit_debug_native_function_image& out) noexcept
    {
        out = {};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(!capture || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const canonical{debug_activation_canonical_capture(capture)};
            // [private registry-owned immutable capture] or null
            // [safe                                   ] compare control blocks
            //  ^^ BEFORE dereferencing any caller-provided pointer/alias.
            if(!canonical || !canonical->control_ || !canonical->ticket_ || canonical->snapshot_.participant == 0u) { return false; }
            if(canonical->snapshot_.frames.empty()) { return false; }
            auto const& frame{canonical->snapshot_.frames.back()};
            auto const identity{::fast_io::concat_fast_io("uwvm-m", ::fast_io::mnp::dec(frame.module),
                "-f", ::fast_io::mnp::dec(frame.function), "-g", ::fast_io::mnp::dec(frame.function_generation), ".wasm-native-v1")};
            // Allocate only cold owned DATA from the immutable privately minted
            // extent, before taking the domain/publication/native gate. The
            // complete live owner is reauthenticated below before any code read.
            auto const& origin{canonical->native_code_site_};
            if(!origin.valid || origin.owner_begin == 0u || origin.owner_end <= origin.owner_begin ||
               origin.owner_end - origin.owner_begin > PTRDIFF_MAX) { return false; }
            llvm_jit_debug_native_function_image candidate{};
            auto const extent{static_cast<::std::size_t>(origin.owner_end - origin.owner_begin)};
            candidate.bytes.resize(extent); candidate.guest_code.resize(extent);
#if defined(__linux__) && defined(__arm__)
            candidate.instruction_code.resize(extent);
#endif
            // Real cold native reader excludes the original managed sweep.
            auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()};
            if(!gc_reader) { return false; }
            auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
            bool valid{};
            auto const stopped{canonical->control_->with_parked_participant(canonical->ticket_, canonical->snapshot_.participant,
                [&](auto actual, bool const external)
            {
                runtime_state_publication_guard publication{};
                if(!canonical->matches_native_publication_locked(actual, external) || external != (native_session_identity != nullptr)) { return; }
                auto const& site{canonical->native_code_site_};
                if(!site.valid || site.participant != canonical->snapshot_.participant ||
                   site.owner_begin == 0u || site.owner_end <= site.owner_begin ||
                   site.return_pc < site.owner_begin || site.return_pc >= site.owner_end) { return; }
                if(external)
                {
                    auto const& actual_step{canonical->native_site_};
                    if(!actual_step.valid || actual_step.participant != site.participant || actual_step.native_thread == 0u ||
                       actual_step.return_pc != site.return_pc || actual_step.owner_begin != site.owner_begin ||
                       actual_step.owner_end != site.owner_end) { return; }
                }
                auto const copy{[&](::std::uint_least64_t native_thread, ::std::uintptr_t pc,
                    ::std::uintptr_t owner_begin, ::std::uintptr_t owner_end) noexcept
                {
                    if((external && native_thread != canonical->native_site_.native_thread) ||
                       owner_begin != site.owner_begin || owner_end != site.owner_end ||
                       pc < owner_begin || pc >= owner_end || owner_end - owner_begin != extent)
                    { return; }
                    if(canonical->snapshot_.frames.empty() || actual.code_unit >= g_runtime.modules.size()) { return; }
                    ::std::uintptr_t actual_begin{}, actual_end{};
                    if(!debug_resolve_actual_native_function_body(actual.code_unit, actual.function,
                        canonical->snapshot_.frames.back().function_generation, actual.code_generation,
                        pc, actual_begin, actual_end) || actual_begin != owner_begin || actual_end != owner_end) { return; }
                    candidate.native_instruction_stop = external;
                    if(canonical->code_owners_.empty() ||
                       !debug_copy_actual_native_target(actual.code_unit, actual.function,
                           canonical->snapshot_.frames.back().function_generation,
                           canonical->code_owners_.back(), candidate.target)) { return; }
                    candidate.stop_pc = pc; candidate.owner_begin = owner_begin; candidate.owner_end = owner_end;
                    candidate.size = static_cast<::std::size_t>(owner_end - owner_begin);
                    candidate.participant = site.participant; candidate.module = actual.code_unit; candidate.function = actual.function;
                    candidate.runtime_epoch = actual.code_generation;
                    candidate.function_generation = canonical->snapshot_.frames.back().function_generation;
                    // [actual published symbol owner_begin ... owner_end)
                    // [safe                                        ] whole
                    //  ^^ exact extent equals owned candidate.bytes.size(). The
                    // lease + ONE domain + publication guard keep code alive;
                    // native host-transition additionally keeps its trap closed.
                    // The real owner extent is bounded independently; MC also
                    // proves actual instruction boundaries before display/step.
                    ::std::memcpy(candidate.bytes.data(), reinterpret_cast<void const*>(owner_begin), candidate.size);
                    auto const& rec{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(actual.code_unit))};
                    auto const name{resolve_func_display_name(rec.module_name, static_cast<::std::size_t>(actual.function))};
                    if(!name.empty() && name.size() <= llvm_jit_debug_max_native_function_name_bytes)
                    {
                        // [actual parser/module-owned name ... name.end)
                        // [safe                                        ] its
                        //  ^^ module/parser storage remains execution-pinned.
                        // Copy the complete bounded name; overlong/absent names
                        // are omitted, never truncated or used as read authority.
                        ::std::memcpy(candidate.function_name, name.data(), name.size());
                        candidate.function_name_size = name.size();
                    }
                    ::std::size_t expression_size{};
                    auto const* rows{debug_native_provenance_for_function_locked(candidate.module, candidate.function,
                        candidate.function_generation, candidate.runtime_epoch, expression_size)};
                    if(rows != nullptr && !rows->code_permissions(candidate.owner_begin, candidate.size, owner_begin, owner_end,
                        {identity.data(), identity.size()}, expression_size, candidate.runtime_epoch, candidate.guest_code.data()))
                    { ::std::memset(candidate.guest_code.data(), 0, candidate.size); }
#if defined(__linux__) && defined(__arm__)
                    if(rows == nullptr || !rows->instruction_code(candidate.owner_begin,candidate.size,owner_begin,owner_end,
                        {identity.data(),identity.size()},expression_size,candidate.runtime_epoch,candidate.instruction_code.data()))
                    { return; } // Missing or malformed mapping never guesses a boundary after DATA.
#endif
                    out = ::std::move(candidate); valid = true;
                }};
                if(!external) { copy(0u, site.return_pc, site.owner_begin, site.owner_end); }
                else
                {
                    // Actual session identity is compared before dereference.
                    // Lock order: lease -> domain -> publication -> native host.
#if UWVM2_RUNTIME_NATIVE_STEP_PLATFORM
                    static_cast<void>(::uwvm2::uwvm::debugger::native_step::with_owned_registers(native_session_identity,
                        [&](auto thread, auto pc, auto begin, auto end, auto const& registers) noexcept
                    {
                        if(registers.pc() != pc || !canonical->matches_native_cursor_locked(
                            native_session_identity,thread,pc,registers.sp(),begin,end)) { return; }
                        copy(thread,pc,begin,end);
                    }));
#else
                    return; // No real platform trap capability exists here.
#endif
                }
            })};
            if(!stopped || !valid) { out = {}; return false; }
            return true;
        }
        catch(...) { out = {}; return false; }
#else
        (void)capture; (void)native_session_identity; return false;
#endif
    }
#endif

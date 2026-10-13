/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
// Included only inside the actual runtime implementation's namespace. All
// collector/LLVM declarations are in its global fragment, not nested here.
#if defined(UWVM_RUNTIME_LLVM_JIT)
    namespace
    {
        // Future actual checkpoint restore/generation publication calls this
        // while execution is drained and runtime publication is held. Old rows
        // must never be rebound to restored/new epochs. Ordinary execution does
        // not poll or call this hook; reset already destroys these owners.
        [[maybe_unused]] inline void invalidate_debug_native_provenance_after_actual_generation_change() noexcept
        {
            for(auto& record: g_runtime.modules)
            {
                if(record.llvm_jit_full_publication)
                { record.llvm_jit_full_publication->native_provenance.invalidate_runtime_generation(); }
                for(auto& retained: record.llvm_jit_debug_full_retained_generations)
                {
                    if(retained) { retained->native_provenance.invalidate_runtime_generation(); }
                }
            }
        }
    }
#endif
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    extern "C++" bool llvm_jit_debug_native_position_host_api(
        llvm_jit_debug_activation_capture_owner const& capture, void const* native_session_identity,
        llvm_jit_debug_native_position& out) noexcept
    {
        out = {};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && UWVM2_RUNTIME_NATIVE_STEP_PLATFORM
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        namespace provenance = ::uwvm2::runtime::lib::details::native_loaded_provenance;
        if(!capture || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const canonical{debug_activation_canonical_capture(capture)};
            // [private canonical capture registry] or null
            // [safe                               ] match control-block identity
            //  ^^ BEFORE dereferencing any supplied capture/alias pointer.
            if(!canonical || !canonical->control_ || !canonical->ticket_ ||
               canonical->snapshot_.participant == 0u || canonical->snapshot_.frames.empty()) { return false; }
            auto const& frame{canonical->snapshot_.frames.back()};
            if(frame.function_generation == 0u || frame.runtime_epoch == 0u) { return false; }
            // A trusted immutable snapshot supplies this opaque synthetic name.
            // No hand decimal parser or guest filename is used. Allocate before
            // the domain/publication/native locks and before noexcept callbacks.
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
                if(!canonical->matches_native_publication_locked(actual, external) || external != (native_session_identity != nullptr) ||
                   frame.module != actual.code_unit || frame.function != actual.function || frame.runtime_epoch != actual.code_generation)
                { return; }
                auto const& site{canonical->native_site_};
                if(!site.valid || site.participant != canonical->snapshot_.participant || site.native_thread == 0u ||
                   site.owner_begin == 0u || site.owner_end <= site.owner_begin ||
                   site.return_pc < site.owner_begin || site.return_pc >= site.owner_end) { return; }
                auto const query{[&](::std::uint_least64_t native_thread, ::std::uintptr_t pc,
                    ::std::uintptr_t owner_begin, ::std::uintptr_t owner_end) noexcept
                {
                    if(native_thread != site.native_thread || owner_begin != site.owner_begin || owner_end != site.owner_end ||
                       pc < owner_begin || pc >= owner_end || actual.code_unit >= g_runtime.modules.size()) { return; }
                    ::std::uintptr_t actual_begin{}, actual_end{};
                    if(!debug_resolve_actual_native_function_body(actual.code_unit, actual.function,
                        frame.function_generation, actual.code_generation, pc, actual_begin, actual_end) ||
                       actual_begin != owner_begin || actual_end != owner_end) { return; }
                    // [actual publication module table ... modules.size) end
                    // [safe                                                ] actual.code_unit checked;
                    //  ^^ code/metadata owners remain pinned by lease + domain + publication.
                    auto const& record{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(actual.code_unit))};
                    auto const* const module{record.runtime_module};
                    if(module == nullptr || !record.llvm_jit_ready || !record.llvm_jit_full_publication ||
                       !record.llvm_jit_full_publication->engine || !record.llvm_jit_full_publication->context) { return; }
                    auto const imports{module->imported_function_vec_storage.size()};
                    if(actual.function < imports || actual.function - imports >= module->local_defined_function_vec_storage.size()) { return; }
                    auto const local{static_cast<::std::size_t>(actual.function - imports)};
                    provenance::image const* rows{};
                    ::std::size_t expression_size{};
                    if(frame.function_generation == 1u)
                    {
                        if(local >= record.llvm_jit_compiled.local_funcs.size()) { return; }
                        auto const& function{record.llvm_jit_compiled.local_funcs.index_unchecked(local)};
                        auto const begin{reinterpret_cast<::std::uintptr_t>(function.code_begin)};
                        auto const end{reinterpret_cast<::std::uintptr_t>(function.code_end)};
                        // [owner-pinned original expression begin ... end) end
                        // [safe                                             ] integer extent only;
                        //  ^^ no bytecode pointer moves or guest bytes are read here.
                        if(begin == 0u || end <= begin) { return; }
                        expression_size = end - begin;
                        rows = ::std::addressof(record.llvm_jit_full_publication->native_provenance);
                    }
                    else
                    {
                        for(auto const& retained: record.llvm_jit_debug_full_retained_generations)
                        {
                            if(!retained || !retained->committed || !retained->engine || !retained->context ||
                               retained->module_id != actual.code_unit || retained->function_index != actual.function ||
                               retained->local_index != local || retained->expected_generation != frame.function_generation - 1u ||
                               retained->expected_runtime_epoch != actual.code_generation) { continue; }
                            if(rows != nullptr) { return; } // conflicting owner publication, never first-match authority.
                            rows = ::std::addressof(retained->native_provenance);
                            expression_size = retained->debug_expression_size;
                        }
                    }
                    if(rows == nullptr || expression_size == 0u) { return; }
                    auto const located{rows->lookup(pc, owner_begin, owner_end,
                        {identity.data(), identity.size()}, expression_size, actual.code_generation)};
                    llvm_jit_debug_native_position candidate{};
                    candidate.pc = pc; candidate.owner_begin = owner_begin; candidate.owner_end = owner_end;
                    candidate.row_begin = located.begin; candidate.row_end = located.end;
                    candidate.participant = site.participant; candidate.module = actual.code_unit; candidate.function = actual.function;
                    candidate.function_generation = frame.function_generation; candidate.runtime_epoch = actual.code_generation;
                    switch(located.state)
                    {
                        case provenance::status::exact:
                            candidate.status = llvm_jit_debug_native_position_status::exact; candidate.wasm_offset = located.wasm_offset; break;
                        case provenance::status::ambiguous: candidate.status = llvm_jit_debug_native_position_status::ambiguous; break;
                        case provenance::status::unknown: candidate.status = llvm_jit_debug_native_position_status::unknown; break;
                        default: break;
                    }
                    candidate.numeric_location_count = rows->numeric_locations(pc, owner_begin, owner_end,
                        {identity.data(), identity.size()}, expression_size, actual.code_generation,
                        candidate.numeric_locations, 64u);
                    out = candidate; valid = true;
                }};
                if(!external) { query(site.native_thread, site.return_pc, site.owner_begin, site.owner_end); }
                else
                {
                    // Same established lock order as native code/register copies:
                    // lease -> ONE domain -> publication -> native host transition.
                    // Actual session identity is checked before any dereference;
                    // that gate stays closed through the entire owned-data query.
                    static_cast<void>(::uwvm2::uwvm::debugger::native_step::with_owned_registers(native_session_identity,
                        [&](auto thread, auto pc, auto begin, auto end, auto const& registers) noexcept
                    {
                        if(registers.pc() != pc || !canonical->matches_native_cursor_locked(
                            native_session_identity,thread,pc,registers.sp(),begin,end)) { return; }
                        query(thread,pc,begin,end);
                    }));
                }
            })};
            if(!stopped || !valid) { out = {}; return false; }
            return true;
        }
        catch(...) { out = {}; return false; }
#else
        static_cast<void>(capture); static_cast<void>(native_session_identity); return false;
#endif
    }
#endif

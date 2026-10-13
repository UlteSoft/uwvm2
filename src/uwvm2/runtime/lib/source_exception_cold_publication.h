// Included only in the LLVM-full runtime with typed exception source retention.
// This is a cold publication boundary. It never grants collection authority or
// changes the existing rejection of an incomplete exception-root population.
namespace
{
    void prepare_actual_source_exception_publication() noexcept
    {
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        using publisher = ::uwvm2::runtime::gc::source_exception_publisher;
        using source_type = ::uwvm2::uwvm::runtime::full::full_source_instance;
        if(mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth()))
        { return; }

        // The real generation lease excludes drained source/code replacement.
        // Declare every potentially last native DATA owner outside the shorter
        // exclusion/publication scopes, so failed candidates retire afterward.
        auto generation{g_runtime.execution_domain.try_enter()};
        if(!generation) { return; }
        source_type::owner source{};
        publisher::owner candidate{};
        ::std::size_t module_id{SIZE_MAX};
        ::std::uint_least64_t epoch{};
        {
            // Compilers and their metadata callbacks may themselves enter the
            // normal root-graph writer API. Keep a real shared reader here;
            // compiling while holding exclusive admission could self-deadlock.
            auto compiler_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            {
                runtime_state_publication_guard publication_guard{};
                source = ::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin();
                // Debug/checkpoint execution owns its compilation admission and
                // source publication. Reject here, before the pre-entry helper
                // can compile; the later publication check already rejects it.
                if(g_runtime.debug_pause_control || !source_type::has_canonical_owner(source) || !source->initialized_from_actual_state() ||
                   source->initialized_main_module()->local_defined_tag_vec_storage.empty())
                { return; }
            }
            // The existing builder releases its own publication lock. This
            // cold compilation is protected by real generation/reader leases.
            compile_all_modules_if_needed();
        }
        {
            // At zero actual readers, this real exclusive lease prevents a
            // guest, initializer writer or collector from overlapping setup.
            // Neither TLS depth nor a source pointer substitutes for this lease.
            auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
            if(!exclusive) { return; }

            {
                runtime_state_publication_guard publication_guard{};
                source = ::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin();
                if(!source_type::has_canonical_owner(source) ||
                   !source->initialized_from_actual_state() || g_runtime.debug_pause_control)
                { return; }
                module_id = source->assigned_main_module_id();
                if(module_id >= g_runtime.modules.size()) { return; }
                // [actual immutable module vector][module_id < size] [one-past]
                // [safe] borrow only a complete generation-pinned native record.
                auto const& record{g_runtime.modules.index_unchecked(module_id)};
                auto const& code{record.llvm_jit_full_publication};
                if(!record.llvm_jit_ready || !code || !code->engine || !code->context ||
                   record.runtime_module != source->initialized_main_module() ||
                   !code->source || code->source.get() != source.get() ||
                   code->source.owner_before(source) || source.owner_before(code->source))
                { return; }
                epoch = current_runtime_generation();
                if(epoch == 0u) { return; }
                if(code->source_exceptions)
                {
                    // A genuine existing publication owns the same generation;
                    // no new domain is manufactured for its existing aliases.
                    return;
                }
            }

            // The factory independently checks the actual locked full-code
            // observer, initialized schema/store, canonical owners and imports.
            // Its freshly allocated empty domain has no native payload callback;
            // source/module/store remain pinned even if construction runs out
            // of memory. Every complete candidate below retires after exclusion.
            try { candidate = publisher::prepare_from_actual_full(source); }
            catch(::std::bad_alloc const&) { return; }
            if(!publisher::canonical(candidate)) { return; }
            {
                runtime_state_publication_guard publication_guard{};
                auto const current{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
                if(!current || current.get() != source.get() || current.owner_before(source) ||
                   source.owner_before(current) || current_runtime_generation() != epoch ||
                   g_runtime.debug_pause_control || module_id >= g_runtime.modules.size())
                { return; }
                // [same actual immutable module vector][bounded module_id]
                // [safe] reborrow after the observer has released its own lock.
                auto& record{g_runtime.modules.index_unchecked(module_id)};
                auto& code{record.llvm_jit_full_publication};
                if(!record.llvm_jit_ready || !code || !code->engine || !code->context ||
                   record.runtime_module != source->initialized_main_module() ||
                   !code->source || code->source.get() != source.get() ||
                   code->source.owner_before(source) || source.owner_before(code->source) ||
                   candidate->observed_epoch() != epoch ||
                   candidate->retained_source().get() != source.get() ||
                   candidate->retained_source().owner_before(source) ||
                   source.owner_before(candidate->retained_source()) || code->source_exceptions)
                { return; }
                // This private code publication, protected by both real leases,
                // retains the exact concrete publisher. An ID/epoch alone never
                // gives a raw exception value this source's creation origin.
                code->runtime_epoch = epoch;
                code->source_exceptions = ::std::move(candidate);
            }
        }
        // Publication/exclusion locks are gone before candidate/source release;
        // the genuine generation lease still retains native code and its graph.
    }
}

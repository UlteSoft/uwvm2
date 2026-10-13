// Included inside the runtime's unnamed namespace, after its actual entry
// class. Define the bridge in its ORIGINAL runtime namespace/module; its first
// declaration is in uwvm_runtime.h, never in the publisher's foreign module.
    }

    class source_exception_guest_bridge final
    {
        using publisher = ::uwvm2::runtime::gc::source_exception_publisher;
        using exception_value = ::uwvm2::runtime::exception::value_ref;

        [[nodiscard]] static runtime_execution_entry_scope* actual_entry() noexcept
        {
            // [native thread's actual outer scope/null]
            // [safe] initialize from the real scope's private binding, never an
            // integer/module/TLS depth supplied by a guest. Its live members
            // carry the real generation, admission, queue and source lifetime.
            auto* entry{runtime_execution_entry_scope::source_leaf_entry};
            if(entry == nullptr || !entry->entered || !entry->execution_lease ||
               !entry->gc_admission || !entry->exception_retirement || !entry->exception_source_pin)
            { return nullptr; }
            // A managed page can move the real leases out of the entry. In that
            // case the checks above decline; no count/borrow impersonates them.
            return entry;
        }
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
        friend void details::llvm_jit_throw_numeric_abi_bridge(::std::uintptr_t,::std::uintptr_t,::std::uintptr_t,::std::size_t) UWVM_THROWS;
        friend void details::llvm_jit_throw_tuple_abi_bridge(::std::uintptr_t,::std::uintptr_t,::std::uintptr_t,::std::size_t) UWVM_THROWS;
        friend void details::llvm_jit_throw_ref_abi_bridge(::std::uintptr_t,::std::uintptr_t) UWVM_THROWS;
        enum class native_build_kind : unsigned char { fresh, reference };
        class native_build_scope final
        {
            friend class source_exception_guest_bridge;
            runtime_execution_entry_scope* entry_{};
            void* previous_{};
            ::std::uintptr_t module_{};
            native_build_kind kind_{};
            bool registered_{};
        public:
            explicit native_build_scope(::std::uintptr_t module, native_build_kind kind=native_build_kind::fresh) noexcept : module_{module},kind_{kind}
            {
                auto* actual{actual_entry()};
                namespace mode=::uwvm2::uwvm::runtime::runtime_mode;
                if(actual==nullptr || get_runtime_execution_entry_depth()!=1uz ||
                   get_llvm_jit_generated_wasm_bridge_entry_depth()==0uz ||
                   get_runtime_state_publication_depth()!=0uz || get_runtime_compilation_metadata_callback_depth()!=0uz ||
                   mode::global_runtime_mode!=mode::runtime_mode_t::full_compile ||
                   mode::global_runtime_compiler!=mode::runtime_compiler_t::llvm_jit_only ||
                   module!=actual->exception_source_pin->retained_source()->assigned_main_module_id())
                { g_runtime.gc_collection.note_unqualified_exception();return; }
                entry_=actual;
                // [older live helper guard/null][this initialized native guard]
                // [safe] actual outer entry/generation survives both stack scopes.
                previous_=entry_->exception_build_current;
                entry_->exception_build_current=this;
            }
            native_build_scope(native_build_scope const&)=delete;
            native_build_scope& operator=(native_build_scope const&)=delete;
            ~native_build_scope() noexcept
            {
                if(entry_==nullptr) { return; }
                if(entry_->exception_build_current!=this) { ::fast_io::fast_terminate(); }
                // [same actual helper borrow][older initialized live guard/null]
                // [safe] restore before *this dies; no builder pointer escapes.
                entry_->exception_build_current=previous_;
                if(!registered_) { g_runtime.gc_collection.note_unqualified_exception(); }
            }
        };
        [[nodiscard]] static bool genuine_current_build(runtime_execution_entry_scope const& entry,
            ::std::uintptr_t module_id, native_build_kind kind) noexcept
        {
            // Nonnull borrow is issued ONLY by the PRIVATE stack guard in our
            // three exact throw ABI friends, never a public constructor/bool seed.
            auto const* current{static_cast<native_build_scope const*>(entry.exception_build_current)};
            return current!=nullptr && current->entry_==::std::addressof(entry) && current->module_==module_id && current->kind_==kind;
        }
        static void complete_actual_private_build(runtime_execution_entry_scope& entry,
            ::std::uintptr_t module_id, exception_value const& value, native_build_kind kind) noexcept
        {
            if(!genuine_current_build(entry,module_id,kind) || !publisher::is_actual_private_native_value(entry.exception_source_pin,value))
            { g_runtime.gc_collection.note_unqualified_exception();return; }
            // [genuine current guard's complete bool member] exists until throw
            // helper cleanup; only actual registered-source result can set it.
            auto* current{static_cast<native_build_scope*>(entry.exception_build_current)};
            current->registered_=true;
        }
#endif
    public:
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
        static void before_actual_caught_value(::std::uintptr_t module_id,
            ::uwvm2::runtime::exception::guest_exception const& caught) noexcept
        {
            auto* entry{actual_entry()};
            if(entry==nullptr || module_id!=entry->exception_source_pin->retained_source()->assigned_main_module_id() ||
               !caught.root_registered() || !publisher::is_actual_private_native_value(entry->exception_source_pin,caught.instance()))
            { g_runtime.gc_collection.note_unqualified_exception(); }
        }
        static void bind_actual_managed_gc_entry() noexcept
        {
            auto* entry{actual_entry()};
            if(entry == nullptr || entry->exception_gc_cohort || !g_runtime.gc_collection.roots_requested() ||
               g_runtime.gc_collection.disabled() || get_runtime_execution_entry_depth()!=1uz) { return; }
            namespace mode=::uwvm2::uwvm::runtime::runtime_mode;
            if(mode::global_runtime_mode!=mode::runtime_mode_t::full_compile ||
               mode::global_runtime_compiler!=mode::runtime_compiler_t::llvm_jit_only) { return; }
            auto const& source{entry->exception_source_pin->retained_source()};
            auto const module_id{source->assigned_main_module_id()};
            {
                runtime_state_publication_guard lock{};
                if(module_id>=g_runtime.modules.size()) { return; }
                // [generation-pinned actual full records][bounded module_id] end
                // [safe] read only this real source's finalized native record.
                auto const& record{g_runtime.modules.index_unchecked(module_id)};
                auto const& code{record.llvm_jit_full_publication};
                if(!record.llvm_jit_ready || !code || !code->engine || !code->context ||
                   record.runtime_module!=source->initialized_main_module() ||
                   (!record.runtime_module->local_defined_function_vec_storage.empty() &&
                    !record.llvm_jit_precise_gc_roots_emitted) ||
                   !code->source || code->source.get()!=source.get() ||
                   code->source.owner_before(source) || source.owner_before(code->source) ||
                   code->source_exceptions.get()!=entry->exception_source_pin.get() ||
                   code->source_exceptions.owner_before(entry->exception_source_pin) ||
                   entry->exception_source_pin.owner_before(code->source_exceptions) ||
                   code->runtime_epoch!=current_runtime_generation() ||
                   code->runtime_epoch!=entry->exception_source_pin->observed_epoch()) { return; }
            }
            // The actual generation/entry admission pins persist after the short
            // publication read; descriptor construction and potential last DATA
            // release happen outside its lock. No raw source/root flag grants it.
            auto cohort{publisher::mint_actual_managed_cohort(entry->exception_source_pin)};
            if(cohort && g_runtime.gc_collection.bind_local_exception_cohort(*cohort))
            { entry->exception_gc_cohort.emplace(::std::move(*cohort)); }
        }
        [[nodiscard]] static ::uwvm2::runtime::gc::managed_exception_cohort const* actual_managed_gc_entry() noexcept
        {
            auto* entry{actual_entry()};
            if(entry!=nullptr && entry->exception_build_current!=nullptr)
            {
                // Allocator/constructor reentry cannot collect an unpublished
                // payload. Revoke BEFORE any paused census/allocation attempt.
                g_runtime.gc_collection.note_unqualified_exception();return nullptr;
            }
            // [actual scope-owned immutable descriptor/null] its real generation,
            // shared admission, queue and producer pins outlive this synchronous poll.
            return entry != nullptr && entry->exception_gc_cohort ? ::std::addressof(*entry->exception_gc_cohort) : nullptr;
        }
        static void after_actual_managed_gc_poll() noexcept
        {
            auto* entry{actual_entry()};
            if(entry == nullptr || entry->exception_retirement->pending_count() == 0uz) { return; }
            // The actual collector has completely returned. Keep genuine source,
            // code/tag/store and generation pins through every authenticated
            // frozen-native leaf callback; generic owners remain deferred.
            (void)publisher::reclaim_actual_native_leaves(entry->exception_source_pin,*entry->exception_retirement);
        }
        [[nodiscard]] static exception_value try_publish_ref(::std::uintptr_t module_id,
            ::uwvm2::uwvm::runtime::storage::gc_reference reference)
        {
            auto* entry{actual_entry()};
            if(entry != nullptr && module_id == entry->exception_source_pin->retained_source()->assigned_main_module_id() &&
               genuine_current_build(*entry,module_id,native_build_kind::reference))
            {
                auto result{publisher::materialize_local_token(entry->exception_source_pin,reference)};
                if(result)
                { complete_actual_private_build(*entry,module_id,result,native_build_kind::reference);return result; }
            }
            // Unknown owner/origin retains old throw_ref execution semantics,
            // but permanently forbids managed collection BEFORE propagation.
            g_runtime.gc_collection.note_unqualified_exception(); return {};
        }
#endif
        // Called solely after the generated tuple's COMPLETE signature/carrier
        // checks. This vector is a fresh private runtime builder whose mutable
        // pointers, references, iterators and spans have all retired.
        [[nodiscard]] static exception_value try_publish_owned(
            ::std::uintptr_t module_id, ::std::uintptr_t tag_index,
            ::std::vector<::uwvm2::runtime::exception::payload_field>&& fields,
            ::uwvm2::runtime::exception::diagnostic_trace_ref const& trace)
        {
            auto* entry{actual_entry()};
            if(entry == nullptr || module_id != entry->exception_source_pin->retained_source()->assigned_main_module_id())
            {
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
                g_runtime.gc_collection.note_unqualified_exception();
#endif
                return {};
            } // The private builder is untouched on decline.
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
            if(!genuine_current_build(*entry,module_id,native_build_kind::fresh))
            { g_runtime.gc_collection.note_unqualified_exception();return {}; }
#endif
            auto const& producer{entry->exception_source_pin};
            auto produced{publisher::publish_native_owned(producer,tag_index,::std::move(fields),trace)};
            if(!produced)
            {
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
                g_runtime.gc_collection.note_unqualified_exception();
#endif
                return {};
            } // Preflight decline still leaves fields intact.
            auto result{publisher::materialize_for_recipient(producer,*produced)};
            // After private make_owned consumes fields, never silently retry an
            // empty vector. A genuine same-generation publication cannot close
            // under this real entry. Allocation failure keeps normal C++ policy.
            if(!result) { ::std::terminate(); }
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
            complete_actual_private_build(*entry,module_id,result,native_build_kind::fresh);
#endif
            return result;
        }

        static void after_actual_end_catch() noexcept
        {
            auto* entry{actual_entry()};
            if(entry == nullptr || entry->exception_retirement->pending_count() == 0uz) { return; }
            // An empty actual thread-local queue has no callback-bearing owner
            // to classify. Retained exception_ptr/throw_ref aliases commonly
            // leave it empty, so the full producer match and FIFO walk are cold.
            // The actual C++ catch/header and its public owner are already
            // retired. Payload SSA/carriers were copied before end_catch.
            // This strong typed producer pins source/tag/store/domain throughout
            // selective destruction; unknown arbitrary callbacks remain queued.
            (void)publisher::reclaim_actual_native_leaves(
                entry->exception_source_pin,*entry->exception_retirement);
        }
    };

    namespace
    {
#if defined(UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_GNU_CXX)
        inline void runtime_source_exception_end_catch() noexcept
        {
            // The mapped declaration is used only by our exact final guest
            // landingpads, whose destructors are noexcept. Do not globally mark
            // arbitrary foreign C++ catch destructors as nonthrowing.
            ::uwvm2::runtime::lib::details::native_exception_host::end_exact_guest_catch_noexcept();
            source_exception_guest_bridge::after_actual_end_catch();
        }
#endif

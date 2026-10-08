#pragma once
// Actual host-only logical thread execution slice. Native entries and real
// returned-child events remain private; resource rollback/new-epoch publication
// require the separate complete-instance transaction and are not supplied here.
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
extern "C++"
{
    class runtime_checkpoint_continuation_dispatcher final
    {
        template<typename Owner>
        [[nodiscard]] static bool same_owner(Owner const& a, Owner const& b) noexcept
        { return a && b && a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a); }
        struct prepared_frame
        {
            ::uwvm2::runtime::checkpoint::logical_frame logical{};
            ::uwvm2::runtime::checkpoint::caller_return_projection::owner child_projection{};
            ::std::uintptr_t address{};
            ::std::uint64_t landing{};
            ::std::vector<::std::byte> payload{}, output{};
            ::std::vector<::std::uint8_t> flags{};
            ::std::vector<::std::size_t> result_offsets{}, result_widths{};
            ::std::vector<::uwvm2::runtime::checkpoint::native_value> returned{};
            ::uwvm2::utils::container::u8string module_name{};
            ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store> declared_store{};
            ::std::vector<::uwvm2::runtime::gc::root_reference> result_roots{};
            ::uwvm2::runtime::gc::root_frame result_root_frame{};
            ::std::size_t reference_results{};
            bool result_roots_live{}, result_escapes_host{}, entry_bound{};
        };
        [[nodiscard]] static bool installed(
            ::uwvm2::runtime::checkpoint::sealed_function_plan::owner const& plan, ::std::uint64_t site) noexcept
        {
            if(!plan || !plan->get().profile ||
               plan->get().profile->purpose() != ::uwvm2::runtime::checkpoint::compilation_purpose::resumable ||
               plan->get().resume_abi_revision != 2u || site == 0u || site > plan->get().sites.size()) { return false; }
            for(auto const actual : plan->get().resume_sites) { if(actual == site) { return true; } }
            return false;
        }
        // A valid guest may return a presently unsupported non-scalar carrier.
        // Private cleanup-only failure must leave the host output untouched;
        // it is not a VM trap, Wasm exception tag, or resume permission.
        class unsupported_child_result final {};
        using native_entry = void(*)(::std::uint64_t, void const*, ::std::uintptr_t, void const*,
            ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t);
        struct actual_resume_context final
        {
            ::std::vector<prepared_frame>& frames;
            runtime_checkpoint_thread_capture::owner const& capture;
            ::std::uint64_t epoch;
            ::std::size_t active{};
            actual_resume_context(::std::vector<prepared_frame>& f, runtime_checkpoint_thread_capture::owner const& c,
                ::std::uint64_t e) noexcept : frames{f}, capture{c}, epoch{e} {}
            actual_resume_context(actual_resume_context const&) = delete;
            actual_resume_context& operator=(actual_resume_context const&) = delete;
        };
        // Inert native TLS for default execution. Only execute_saved's fully
        // authenticated private invocation installs this STACK-owned context;
        // no file label, foreign shared_ptr or guest native address can mint it.
        inline static thread_local actual_resume_context* actual_resume_{};
        class resume_context_scope final
        {
            actual_resume_context* owner_{};
        public:
            explicit resume_context_scope(actual_resume_context& actual) noexcept : owner_{::std::addressof(actual)}
            {
                if(actual_resume_ != nullptr) { ::fast_io::fast_terminate(); }
                actual_resume_ = owner_;
            }
            ~resume_context_scope()
            {
                if(actual_resume_ != owner_) { ::fast_io::fast_terminate(); }
                actual_resume_ = nullptr; // clear before prepared owners/entry lease retire
            }
            resume_context_scope(resume_context_scope const&) = delete;
            resume_context_scope& operator=(resume_context_scope const&) = delete;
        };
        [[nodiscard]] static bool declared_child_results_assignable(prepared_frame const& parent,
            prepared_frame const& child) noexcept
        {
            if(!parent.child_projection || !same_owner(parent.child_projection->plan(), parent.logical.plan) ||
               !parent.logical.plan || !child.logical.plan) { return false; }
            auto const expected{parent.child_projection->result_types()};
            if(expected.size() != child.returned.size()) { return false; }
            using store = ::uwvm2::uwvm::runtime::storage::gc_object_store;
            for(::std::size_t n{}; n != expected.size(); ++n)
            {
                // [owned expected and actual child result declarations0..N] end
                // [safe] complete equal counts BEFORE either bounded slot read.
                // The stores came from real canonical module preflight; no flat
                // index or physical carrier can stand for cross-module identity.
                if(!child.returned[n].declaration.initialized ||
                   !store::canonical_value_type_matches(child.declared_store.get(), child.returned[n].declaration.type,
                        parent.declared_store.get(), expected[n])) { return false; }
            }
            return true;
        }
        [[nodiscard]] static bool actual_child_results_assignable(prepared_frame const& parent,
            prepared_frame const& child) noexcept
        {
            if(!declared_child_results_assignable(parent, child)) { return false; }
            namespace checkpoint = ::uwvm2::runtime::checkpoint;
            auto const expected{parent.child_projection->result_types()};
            for(::std::size_t n{}; n != expected.size(); ++n)
            {
                // [complete privately owned normal child result tuple] end
                // [safe] exact full count was checked before either index.
                if(expected[n].kind != checkpoint::types::value_kind::reference) { continue; }
                auto upcast{child.returned[n]}; upcast.declaration = {expected[n], true};
                if(runtime_checkpoint_thread_capture::check_scalar_reference(upcast) == checkpoint_thread_capture_status::captured)
                { continue; }
                checkpoint::native_reference reference{};
                static_assert(sizeof(reference) <= checkpoint::native_slot_bytes);
                // [actual complete child-produced carrier bits0..16] end
                // [safe] fixed extent BEFORE copying into a constructed token;
                // neither the token nor a supplied host address is dereferenced.
                ::fast_io::freestanding::my_memcpy(::std::addressof(reference), upcast.bits.data(), sizeof(reference));
                if(!parent.declared_store || reference.storage.ptr == nullptr ||
                   !parent.declared_store->reference_type_matches(reference, expected[n])) { return false; }
            }
            // Original child declarations remain intact for its real ABI/store
            // proof. Generated parent SSA/after-site metadata performs the safe
            // upcast only after this actual return and before publishing roots.
            return true;
        }
        [[nodiscard]] static bool decode_actual_results(prepared_frame& next) noexcept
        {
            if(next.result_offsets.size() != next.returned.size() || next.result_widths.size() != next.returned.size())
            { return false; }
            next.reference_results = 0u; next.result_escapes_host = false;
            for(::std::size_t n{}; n != next.returned.size(); ++n)
            {
                // [actual complete privately owned result tuple] end
                // [safe] equal vectors BEFORE offset/width/typed slot selection.
                auto& value{next.returned[n]}; auto const offset{next.result_offsets[n]}; auto const width{next.result_widths[n]};
                if(offset > next.output.size() || width > next.output.size()-offset || width > value.bits.size()) { return false; }
                // [actual returned packed result owner ... offset ...] end
                // [safe] complete width and parent-owned lifetime before byte
                // advance; native carrier tokens are copied, never dereferenced.
                ::fast_io::freestanding::my_memcpy(value.bits.data(), next.output.data()+offset, width);
                if(value.declaration.type.kind == ::uwvm2::runtime::checkpoint::types::value_kind::reference)
                {
                    using kind = ::uwvm2::object::global::wasm_ref_kind;
                    ::uwvm2::runtime::gc::root_reference reference{};
                    static_assert(sizeof(reference) <= ::uwvm2::runtime::checkpoint::native_slot_bytes);
                    if(width != sizeof(reference)) { return false; }
                    // [one actual privately owned complete returned ABI slot] end
                    // [safe] fixed carrier width and exact generated return owner
                    // BEFORE copying into a constructed value; no token read.
                    ::fast_io::freestanding::my_memcpy(::std::addressof(reference), value.bits.data(), sizeof(reference));
                    bool runtime_kind{};
                    switch(reference.kind)
                    {
                        case kind::wasm_null: case kind::wasm_i31: case kind::wasm_func_imported:
                        case kind::wasm_func_defined: case kind::wasm_struct: case kind::wasm_array:
                        case kind::wasm_exn: case kind::wasm_extern: runtime_kind = true; break;
                        case kind::wasm_func: break; // parsed index is NEVER a native returned reference
                    }
                    if(!runtime_kind || next.reference_results >= next.result_roots.size()) { return false; }
                    // Abstract funcref's Core type test alone does not require
                    // a defined signature. Independently compare this actual
                    // token to the initialized runtime function registry BEFORE
                    // admitting it as a VM-owned return; no pointee read here.
                    if((reference.kind == kind::wasm_func_defined &&
                        find_defined_func_info(static_cast<runtime_local_func_storage_t const*>(reference.storage.ptr)) == nullptr) ||
                       (reference.kind == kind::wasm_func_imported &&
                        find_cached_import_target(static_cast<runtime_imported_func_storage_t const*>(reference.storage.ptr)) == nullptr))
                    { return false; }
                    auto const scalar{runtime_checkpoint_thread_capture::check_scalar_reference(value)};
                    if(scalar != checkpoint_thread_capture_status::captured)
                    {
                        // This is an actual normal return from the ALL-resolved
                        // privately retained engine, not capture/file/native-address
                        // reconstruction. The real outer managed-entry lease pins
                        // the GC read domain; canonical source preflight pinned THIS
                        // declaration store. Its checked runtime subtype predicate
                        // authenticates registered aggregate/function/exn/wrapper
                        // membership and retains genuine foreign store owners.
                        // Actual opaque extern payloads keep the existing VM
                        // semantics: the checked predicate never dereferences
                        // them. Carrying this actual normal-return value within
                        // the VM neither invents a host replay adapter nor clears
                        // the independent sticky native-escape census. Serialized
                        // or old captured opaque tokens are NOT accepted here.
                        if(!next.declared_store || reference.storage.ptr == nullptr ||
                           !next.declared_store->reference_type_matches(reference,value.declaration.type)) { return false; }
                        next.result_escapes_host = true;
                    }
                    // [preallocated exact actual native result-root carrier tuple] end
                    // [safe] capacity verified BEFORE replacing one constructed
                    // reference object. It becomes a real root BEFORE return to LLVM.
                    next.result_roots[next.reference_results++] = reference;
                }
            }
            return true;
        }
        friend void details::llvm_jit_checkpoint_bind_resume_entry_abi_bridge(
            ::std::uintptr_t,::std::uintptr_t,::std::uint64_t,::std::uint64_t,void const*,::std::uintptr_t,
            void const*,::std::uintptr_t,::std::uintptr_t,::std::uintptr_t) noexcept;
        static void bind_actual_entry(::std::uintptr_t module,::std::uintptr_t function,::std::uint64_t generation,
            ::std::uint64_t site,void const* payload,::std::uintptr_t payload_bytes,
            void const* flags,::std::uintptr_t flag_count,::std::uintptr_t output,::std::uintptr_t output_bytes) noexcept
        {
            auto const actual{actual_resume_};
            if(actual == nullptr || actual->epoch != current_runtime_generation() ||
               actual->active >= actual->frames.size() || !actual->capture ||
               get_runtime_execution_entry_depth() != 1u || get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u ||
               g_debug_observer_active || g_debug_pause_participant == nullptr ||
               runtime_execution_entry_scope::checkpoint_actual_gc_admission() == nullptr ||
               !same_owner(actual->capture->control_,g_runtime.debug_pause_control) ||
               !same_owner(actual->capture->profile_,g_runtime.checkpoint_profile)) { ::fast_io::fast_terminate(); }
            auto const index{actual->active};
            // [canonical ALL-prepared frame owners0 ... active ... N] end
            // [safe] active<N BEFORE selecting the privately constructed owner.
            // Supplemental entry-bound state NEVER substitutes for these owners.
            auto& entry{actual->frames[index]};
            auto const shadow{g_checkpoint_shadow_ledger};auto const activations{g_debug_activation_ledger};
            if(!entry.logical.plan || !installed(entry.logical.plan,entry.landing) || entry.entry_bound ||
               entry.logical.plan->get().module != module || entry.logical.plan->get().function != function ||
               entry.logical.plan->get().function_generation != generation || entry.logical.site != site || entry.landing != site ||
               shadow == nullptr || shadow->failure() != ::uwvm2::runtime::checkpoint::status::ok ||
               activations == nullptr ||
               !(index == 0u ? activations->ready_for_root_entry() : activations->complete()) ||
               activations->count() != index || shadow->size() != index ||
               payload != entry.payload.data() || payload_bytes != entry.payload.size() ||
               flags != entry.flags.data() || flag_count != entry.flags.size() ||
               output != reinterpret_cast<::std::uintptr_t>(entry.output.data()) || output_bytes != entry.output.size())
            { ::fast_io::fast_terminate(); }
            // BEFORE any generated packet read, bind every raw argument to the
            // actual dispatcher-owned range/result owner. No supplied pointer
            // is dereferenced; a serialized label or matching byte count alone
            // grants no input, output, source or continuation capability.
            entry.entry_bound = true;
        }
        friend ::std::uint64_t details::llvm_jit_checkpoint_resume_input_abi_bridge(
            ::std::uint64_t,::std::uintptr_t) noexcept;
        [[nodiscard]] static ::std::uint64_t read_actual_input(::std::uint64_t incarnation,::std::uintptr_t component) noexcept
        {
            namespace checkpoint=::uwvm2::runtime::checkpoint;
            static_assert(sizeof(::std::uintptr_t) <= sizeof(::std::uint64_t));
            auto const actual{actual_resume_};
            if(actual == nullptr || component >= 5u || actual->epoch != current_runtime_generation() ||
               actual->active >= actual->frames.size() || !actual->capture ||
               get_runtime_execution_entry_depth() != 1u || get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u ||
               g_debug_observer_active || g_debug_pause_participant == nullptr ||
               runtime_execution_entry_scope::checkpoint_actual_gc_admission() == nullptr ||
               ::uwvm2::runtime::gc::current_root_frames() == nullptr ||
               !same_owner(actual->capture->control_,g_runtime.debug_pause_control) ||
               !same_owner(actual->capture->profile_,g_runtime.checkpoint_profile)) { ::fast_io::fast_terminate(); }
            auto const index{actual->active};
            // [private canonical prepared frame owners0 ... active ... N] end
            // [safe] active<N BEFORE selecting input owner; N<=profile frame
            // cap from canonical preflight, so active+1 does not overflow.
            auto const& entry{actual->frames[index]};
            checkpoint::activation_identity identity{};checkpoint::sealed_function_plan::owner plan{};
            auto const shadow{g_checkpoint_shadow_ledger};auto const activations{g_debug_activation_ledger};
            if(!entry.entry_bound || !checkpoint_actual_frame(incarnation,identity,plan) ||
               !same_owner(plan,entry.logical.plan) || !same_owner(plan->get().profile,actual->capture->profile_) ||
               plan->get().resume_abi_revision != 2u || !installed(plan,entry.landing) ||
               shadow == nullptr || shadow->failure() != checkpoint::status::ok || activations == nullptr ||
               !activations->complete() || activations->count() != index+1u || shadow->size() != index+1u)
            { ::fast_io::fast_terminate(); }
            auto const current{shadow->frames_while_actually_stopped()};
            // The CURRENT native thread alone owns this unexposed ledger view;
            // no poll, host callback, allocation, park or concurrent publication
            // occurs in this getter. The managed GC admission remains held.
            // [actual native current logical frame prefix0 ... index ... index+1] end
            // [safe] exact count BEFORE last-slot borrow; compare the genuine NEW
            // incarnation, never demand equality with an old captured incarnation.
            if(current.size() != index+1u || current[index].identity != identity ||
               !same_owner(current[index].plan,plan)) { ::fast_io::fast_terminate(); }
            // [five fixed components of this actual privately owned packet] end
            // [safe] component<5 above BEFORE field selection; pointers become
            // native internal ABI integers only, never guest/wire-visible values.
            switch(component)
            {
                case 0u:return entry.landing;
                case 1u:return static_cast<::std::uint64_t>(reinterpret_cast<::std::uintptr_t>(entry.payload.data()));
                case 2u:return static_cast<::std::uint64_t>(entry.payload.size());
                case 3u:return static_cast<::std::uint64_t>(reinterpret_cast<::std::uintptr_t>(entry.flags.data()));
                case 4u:return static_cast<::std::uint64_t>(entry.flags.size());
            }
            ::fast_io::fast_terminate();
        }
        friend ::std::uintptr_t details::llvm_jit_checkpoint_resume_child_abi_bridge(
            ::std::uint64_t, ::std::uint64_t, ::std::uintptr_t) UWVM_THROWS;
        [[nodiscard]] static ::std::uintptr_t invoke_actual_child(
            ::std::uint64_t incarnation, ::std::uint64_t site, ::std::uintptr_t result_bytes) UWVM_THROWS
        {
            namespace checkpoint = ::uwvm2::runtime::checkpoint;
            auto const* context{actual_resume_};
            if(context == nullptr || context->epoch != current_runtime_generation() ||
               context->active >= context->frames.size() || context->active+1u >= context->frames.size() ||
               get_runtime_execution_entry_depth() != 1u || get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u ||
               g_debug_observer_active || g_debug_pause_participant == nullptr ||
               runtime_execution_entry_scope::checkpoint_actual_gc_admission() == nullptr ||
               ::uwvm2::runtime::gc::current_root_frames() == nullptr ||
               !same_owner(context->capture->control_, g_runtime.debug_pause_control) ||
               !same_owner(context->capture->profile_, g_runtime.checkpoint_profile)) { ::fast_io::fast_terminate(); }
            auto const index{context->active};
            // [private actual prepared parent | immediately following child] end
            // [safe] index+1<N BEFORE either selection. All typed packet/result
            // storage and actual native entries were prepared before execution.
            auto& parent{context->frames[index]}; auto& child{context->frames[index+1u]};
            auto const shadow{g_checkpoint_shadow_ledger}; auto const activations{g_debug_activation_ledger};
            checkpoint::activation_identity identity{}; checkpoint::sealed_function_plan::owner plan{};
            if(!checkpoint_actual_frame(incarnation, identity, plan) || !same_owner(plan, parent.logical.plan) ||
               parent.landing != site || parent.logical.site != site || !parent.child_projection ||
               result_bytes != child.output.size() || shadow == nullptr || activations == nullptr ||
               !activations->complete() || activations->count() != index+1u ||
               !shadow->has_complete_materialized_frames() || shadow->size() != index+1u)
            { ::fast_io::fast_terminate(); }
            auto const actual{shadow->frames_while_actually_stopped()};
            // This same native thread has not parked or published these spans to
            // another thread. Its real outer GC shared lease and parent frame
            // remain live; the shadow owner may be synchronously checked here.
            // [actual native-current materialized frame array0..index] end
            // [safe] complete owner/size proof BEFORE selecting the last slot.
            if(actual.size() != index+1u || actual[index].identity != identity || !actual[index].materialized ||
               !same_owner(actual[index].plan, plan) || actual[index].site != site ||
               runtime_checkpoint_thread_capture::check_owned_values(actual[index]) != checkpoint_thread_capture_status::captured ||
               site == 0u || site > plan->get().sites.size() ||
               plan->get().sites[static_cast<::std::size_t>(site-1u)].phase != checkpoint::frame_phase::awaiting_call_return)
            { ::fast_io::fast_terminate(); }
            auto const expected{parent.child_projection->result_types()};
            if(expected.size() != child.returned.size() || child.address == 0u || !installed(child.logical.plan, child.landing))
            { ::fast_io::fast_terminate(); }
            if(!declared_child_results_assignable(parent, child)) { ::fast_io::fast_terminate(); }
            struct active_child_scope final
            {
                actual_resume_context& owner; ::std::size_t previous;
                explicit active_child_scope(actual_resume_context& c) noexcept : owner{c}, previous{c.active} { ++owner.active; }
                ~active_child_scope() { owner.active = previous; }
            };
            // [actual TLS context owned by outer canonical execute_saved frame]
            // [safe] verified pointer is borrowed until this potential unwind
            // returns; the child scope restores it before parent cleanup runs.
            active_child_scope entered{*actual_resume_};
            auto const invoke{reinterpret_cast<native_entry>(child.address)};
            // NO inner catch/noexcept/native-provider suspension: child guest EH
            // and private retirement unwind through the genuine parent's LLVM
            // cleanup. All logical/native/root frames stay visible to a new stop.
            invoke(child.landing, child.payload.data(), child.payload.size(), child.flags.data(), child.flags.size(),
                reinterpret_cast<::std::uintptr_t>(child.output.data()), child.output.size());
            if(!decode_actual_results(child) || !actual_child_results_assignable(parent, child))
            { throw unsupported_child_result{}; }
            if(child.result_roots_live || child.reference_results > child.result_roots.size() ||
               child.result_roots.size() > ::uwvm2::runtime::gc::frame_root_details::max_capacity)
            { ::fast_io::fast_terminate(); }
            // [complete native-owned constructed result-root carrier tuple] end
            // [safe] real child returned, ALL types/members authenticated and
            // capacity*carrier width bounded BEFORE forming the byte span.
            // This real registered native root frame protects the child result
            // while LLVM reads its private output and installs parent SSA values.
            if(!::uwvm2::runtime::gc::enter_root_frame(child.result_root_frame,
                   reinterpret_cast<::std::byte const*>(child.result_roots.data()),child.result_roots.size()) ||
               !::uwvm2::runtime::gc::publish_root_frame(child.result_root_frame,child.reference_results))
            { ::fast_io::fast_terminate(); }
            child.result_roots_live = true;
            // Return only the live private owner's bounded normal-return view.
            // It is retained until the outer entry exits, never serialized or
            // exported to the guest, and no caller-selected host pointer is used.
            return reinterpret_cast<::std::uintptr_t>(child.output.data());
        }
        friend void details::llvm_jit_checkpoint_release_child_roots_abi_bridge(::std::uint64_t,::std::uint64_t) noexcept;
        static void release_actual_child_roots(::std::uint64_t incarnation,::std::uint64_t site) noexcept
        {
            auto const* actual{actual_resume_};
            if(actual == nullptr || actual->epoch != current_runtime_generation() || actual->active >= actual->frames.size() ||
               actual->active+1u >= actual->frames.size() || g_debug_observer_active ||
               runtime_execution_entry_scope::checkpoint_actual_gc_admission() == nullptr) { ::fast_io::fast_terminate(); }
            ::uwvm2::runtime::checkpoint::activation_identity identity{};
            ::uwvm2::runtime::checkpoint::sealed_function_plan::owner plan{};
            // [canonical actual parent | retained child-result root owner] end
            // [safe] both indices fully bounded before selection; the private
            // immutable TLS context was installed only after real engine preflight.
            auto const& parent{actual->frames[actual->active]}; auto& child{actual->frames[actual->active+1u]};
            if(!checkpoint_actual_frame(incarnation,identity,plan) || !same_owner(plan,parent.logical.plan) ||
               parent.landing != site || !child.result_roots_live ||
               ::uwvm2::runtime::gc::current_root_frames() != ::std::addressof(child.result_root_frame) ||
               !::uwvm2::runtime::gc::leave_root_frame(child.result_root_frame)) { ::fast_io::fast_terminate(); }
            child.result_roots_live = false;
            // NO allocation/host callback/poll/park. Generated code immediately
            // publishes the restored prefix+results into its OWN precise parent
            // root frame, before any subsequent guest allocation or debug stop.
        }
    public:
        [[nodiscard]] UWVM_NOINLINE static llvm_jit_checkpoint_continuation_status execute_saved(
            llvm_jit_checkpoint_thread_capture_owner const& supplied, void* result, ::std::size_t result_bytes,
            bool allow_call_dispatch) noexcept
        {
            namespace checkpoint = ::uwvm2::runtime::checkpoint;
            namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
            namespace emit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            using outcome = llvm_jit_checkpoint_continuation_status;
            if(mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
               mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only)
            { return outcome::requires_llvm_jit_full; }
            if(get_runtime_execution_entry_depth() != 0u || g_debug_observer_active ||
               get_runtime_state_publication_depth() != 0u || get_runtime_compilation_metadata_callback_depth() != 0u)
            { return outcome::invalid_management_entry; }
            // Authenticate BOTH actual registries before supplied pointee use.
            // A shared_ptr alias, file label or saved numeric tuple grants none.
            auto const wrapper{llvm_jit_checkpoint_thread_capture::canonical(supplied)};
            if(!wrapper) { return outcome::invalid_capture_owner; }
            auto const capture{runtime_checkpoint_thread_capture::canonical(wrapper->actual_)};
            if(!capture) { return outcome::invalid_capture_owner; }
            if(!capture->control_ || capture->control_->is_closed()) { return outcome::stale_publication; }
            if(!capture->control_->previous_participant_retired(capture->participant_))
            { return outcome::original_execution_still_live; }
            // Canonical observation captures never grant executable continuation.
            if(!capture->profile_ || capture->profile_->purpose() != checkpoint::compilation_purpose::resumable)
            { return outcome::unavailable_compiled_continuation; }
            auto const count{capture->frames_.size()};
            if(count == 0u || !capture->profile_ || count > capture->profile_->limits().frames ||
               (!allow_call_dispatch && count != 1u)) { return outcome::requires_complete_call_dispatch; }
            try
            {
                ::std::vector<prepared_frame> prepared{}; prepared.resize(count);
                // One genuine execution-generation lease pins EVERY engine
                // across all resumed entries and their returned-child edges.
                runtime_execution_entry_scope entry_scope{};
                ::uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment fp{
                    get_llvm_wasm_fp_environment_active_state()};
                if(!fp.ready()) { return outcome::invalid_native_environment; }
                // This early real execution-generation and GC shared admission
                // pins current source/code/stores during complete preflight.
                // It is not a native guest frame or a CLI classification token;
                // no participant/root/shadow/managed-collector entry exists yet.
                {
                    runtime_state_publication_guard lock{};
                    if(!g_runtime.compiled_all.load(::std::memory_order_acquire) ||
                       !same_owner(capture->profile_, g_runtime.checkpoint_profile) ||
                       !same_owner(capture->control_, g_runtime.debug_pause_control)) { return outcome::stale_publication; }
                    for(::std::size_t i{}; i != count; ++i)
                    {
                        // [canonical retained captured frames] [new prepared frames] end
                        // [safe] i<count and equal vectors BEFORE either owning slot.
                        auto const& saved{capture->frames_[i]}; auto const& frame{saved.logical}; auto& next{prepared[i]};
                        if(saved.activation.runtime_epoch != current_runtime_generation() ||
                           saved.activation.parent != (i == 0u ? 0u : capture->frames_[i-1u].activation.incarnation) ||
                           runtime_checkpoint_thread_capture::check_owned_values(frame) != checkpoint_thread_capture_status::captured ||
                           !frame.plan || frame.site == 0u || frame.site > frame.plan->get().sites.size())
                        { return outcome::invalid_typed_packet; }
                        auto const module_id{saved.activation.module};
                        if(module_id >= g_runtime.modules.size()) { return outcome::stale_publication; }
                        // [actual dense generation-owned module registry0..N] end
                        // [safe] module_id<N BEFORE selecting actual owners/addresses.
                        auto const& record{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(module_id))};
                        auto const code{record.llvm_jit_full_publication.get()}; auto const module{record.runtime_module};
                        if(code == nullptr || code != saved.publication_identity || !code->engine || !code->context ||
                           !same_owner(code->source,saved.source) || !same_owner(code->checkpoint_profile,capture->profile_) ||
                           module == nullptr || !record.llvm_jit_ready || !record.llvm_jit_precise_gc_roots_emitted)
                        { return outcome::stale_publication; }
                        auto const imports{module->imported_function_vec_storage.size()};
                        if(saved.activation.function < imports) { return outcome::stale_publication; }
                        auto const local{static_cast<::std::size_t>(saved.activation.function-imports)};
                        if(local >= record.llvm_jit_compiled.local_funcs.size() || local >= record.llvm_jit_debug_full_entry_generations.size() ||
                           local >= module->local_defined_function_vec_storage.size() || local >= code->checkpoint_resume_entries.size() ||
                           record.llvm_jit_debug_full_entry_generations[local] != saved.activation.function_generation)
                        { return outcome::stale_publication; }
                        // [same complete compiled/entry/declaration/generation vectors] end
                        // [safe] local bound proved separately for ALL vectors before use.
                        auto const current_plan{checkpoint_current_generation_plan(record, local, saved.activation.function_generation)};
                        auto const& resolved{code->checkpoint_resume_entries[local]};
                        auto const& declaration{module->local_defined_function_vec_storage.index_unchecked(local)};
                        if(!same_owner(current_plan,frame.plan) || !same_owner(resolved.plan,frame.plan) ||
                           resolved.address == 0u || declaration.function_type_ptr == nullptr ||
                           frame.plan->get().module != module_id || frame.plan->get().function != saved.activation.function ||
                           frame.plan->get().function_generation != saved.activation.function_generation)
                        { return outcome::stale_publication; }
                        auto const abi{emit::get_runtime_wasm_call_abi_layout(*declaration.function_type_ptr)};
                        if(!abi.valid || abi.result_bytes > static_cast<::std::size_t>(PTRDIFF_MAX)) { return outcome::invalid_result_buffer; }
                        // [original canonical site vector ... frame.site-1 ... N] end
                        // [safe] site nonzero and <=N checked BEFORE immutable borrow.
                        auto const& stopped{frame.plan->get().sites[static_cast<::std::size_t>(frame.site-1u)]};
                        auto landing_id{frame.site};
                        if(i+1u != count)
                        {
                            if(stopped.phase != checkpoint::frame_phase::awaiting_call_return || frame.site == UINT64_MAX ||
                               frame.site+1u > frame.plan->get().sites.size()) { return outcome::requires_complete_call_dispatch; }
                            auto const after_id{frame.site+1u};
                            auto const& after{frame.plan->get().sites[static_cast<::std::size_t>(after_id-1u)]};
                            if(after.operand_count < stopped.operand_count || after.local_count != stopped.local_count ||
                               after.slots.size() < stopped.slots.size()) { return outcome::invalid_typed_packet; }
                            auto const prefix{stopped.local_count+stopped.operand_count};
                            auto const results{after.operand_count-stopped.operand_count};
                            if(prefix > after.slots.size() || results > after.slots.size()-prefix)
                            { return outcome::invalid_typed_packet; }
                            ::std::vector<checkpoint::types::core_value_type> expected{}; expected.reserve(results);
                            for(::std::size_t n{}; n != results; ++n)
                            {
                                // [bounded post-call prefix | result tuple | saved params] end
                                // [safe] prefix+results<=slots size before each exact type.
                                expected.push_back(after.slots[prefix+n].type);
                            }
                            next.child_projection=checkpoint::caller_return_projection::seal_compiler_data(frame.plan,frame.site,after_id,expected);
                            if(!next.child_projection) { return outcome::requires_complete_call_dispatch; }
                        }
                        else if(stopped.phase != checkpoint::frame_phase::before_opcode)
                        { return outcome::requires_complete_call_dispatch; }
                        if(!installed(frame.plan,landing_id)) { return outcome::unavailable_compiled_continuation; }
                        auto const& landing{frame.plan->get().sites[static_cast<::std::size_t>(landing_id-1u)]};
                        // installed() bound this genuine ABI2 site in the actual
                        // canonical publication. Lexical clauses use the original
                        // compiled Invoke/PHIs, not an invented pending catch.
                        if(landing.local_count > landing.slots.size() ||
                           landing.slots.size() > static_cast<::std::size_t>(PTRDIFF_MAX)/checkpoint::native_slot_bytes)
                        { return outcome::invalid_typed_packet; }
                        auto const& result_types{stopped.controls.front().declared_results};
                        if(result_types.size() != abi.result_count) { return outcome::invalid_typed_packet; }
                        next.logical=frame; next.landing=landing_id; next.address=resolved.address;
                        next.payload.resize(landing.slots.size()*checkpoint::native_slot_bytes);
                        next.flags.resize(landing.local_count); next.output.resize(abi.result_bytes);
                        next.returned.resize(abi.result_count); next.result_offsets.reserve(abi.result_count); next.result_widths.reserve(abi.result_count);
                        next.declared_store = module->gc_store; next.result_roots.resize(abi.result_count);
                        for(auto& root : next.result_roots) { root.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_null; root.storage.ptr = nullptr; }
                        ::std::size_t offset{};
                        for(::std::size_t n{}; n != abi.result_count; ++n)
                        {
                            // [actual complete declaration result tuple] [private result slots] end
                            // [safe] all counts equal BEFORE either physical/exact type borrow.
                            auto const width{emit::get_runtime_wasm_value_type_abi_size(declaration.function_type_ptr->result.begin[n])};
                            auto const typed_width{emit::get_runtime_wasm_value_type_abi_size(emit::checkpoint_packet_physical_carrier(result_types[n]))};
                            if(width == 0u || width != typed_width || width > checkpoint::native_slot_bytes || offset > abi.result_bytes || width > abi.result_bytes-offset)
                            { return outcome::invalid_typed_packet; }
                            next.result_offsets.push_back(offset); next.result_widths.push_back(width); offset+=width;
                            next.returned[n].declaration={result_types[n],true};
                        }
                        if(offset != abi.result_bytes) { return outcome::invalid_typed_packet; }
                        next.module_name=::uwvm2::utils::container::u8concat_uwvm(record.module_name);
                    }
                    // Validate EVERY child result covariance relation against the
                    // authentic child/parent module stores before any native entry.
                    // Scalar declarations remain exact; reference equality is not
                    // sufficient when equal flat indices belong to different modules.
                    for(::std::size_t i{}; i+1u != count; ++i)
                    {
                        // [actual prepared parent | immediately following child] end
                        // [safe] i+1<count BEFORE selecting either authentic owner.
                        if(!declared_child_results_assignable(prepared[i],prepared[i+1u]))
                        { return outcome::requires_complete_call_dispatch; }
                    }
                    if(result_bytes != prepared.front().output.size() || (result_bytes != 0u && result == nullptr) ||
                       result_bytes > static_cast<::std::size_t>(PTRDIFF_MAX) ||
                       (result != nullptr && reinterpret_cast<::std::uintptr_t>(result) > UINTPTR_MAX-result_bytes))
                    { return outcome::invalid_result_buffer; }
                }
#if UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE && UWVM_HAS_BUILTIN(__builtin_dwarf_cfa)
                llvm_jit_native_call_boundary host_boundary{};
                ::std::optional<::uwvm2::runtime::lib::details::runtime_atomic_borrowed_record_scope<llvm_jit_native_call_boundary>> host_boundary_scope{};
                if(runtime_llvm_jit_unwind_call_stack_requested() && runtime_llvm_jit_unwind_can_replace_instruction_frames())
                {
                    auto& tls{get_call_stack()};
                    auto const previous{::std::atomic_ref<llvm_jit_native_call_boundary const*>{tls.llvm_jit_native_boundary}.load(::std::memory_order_acquire)};
                    host_boundary={previous,reinterpret_cast<::std::uintptr_t>(__builtin_dwarf_cfa()),tls.frames.size(),true};
                    // [previous live boundary] <- [this noinline owned host frame]
                    // [safe] initialize before borrow; RAII clears TLS before frame exit.
                    host_boundary_scope.emplace(tls.llvm_jit_native_boundary,previous,::std::addressof(host_boundary));
                }
#endif
                // Populate EVERY owned packet before the first native entry;
                // no child-return projection invents a physical parent frame.
                for(auto& next : prepared)
                {
                    auto const& actual{next.logical};
                    if(actual.site != next.landing || actual.values.size() > SIZE_MAX/checkpoint::native_slot_bytes ||
                       actual.values.size()*checkpoint::native_slot_bytes != next.payload.size())
                    { return outcome::invalid_typed_packet; }
                    for(::std::size_t n{}; n != actual.values.size(); ++n)
                    {
                        // [owned exact payload0 ... n*16 ... N*16] end
                        // [safe] complete count/product BEFORE EACH byte advance;
                        // source is one authenticated complete native bits[16].
                        ::fast_io::freestanding::my_memcpy(next.payload.data()+n*checkpoint::native_slot_bytes,
                            actual.values[n].bits.data(),checkpoint::native_slot_bytes);
                        if(n < next.flags.size()) { next.flags[n]=actual.values[n].declaration.initialized ? 1u : 0u; }
                    }
                }
                // Every actual canonical source/profile/current generation/
                // engine-resolved entry and exact scalar input packet passed its
                // real lease/publication/ABI proof above. Only NOW classify this
                // privately issued continuation as a VM-owned managed execution.
                // Existing CLI TLS is a collector classification, NEVER a world-
                // stop, N-exclusion, checkpoint or serialized restore authority.
                // Sticky real host escapes remain; final non-scalar host output
                // still independently marks its actual escape below.
                ::uwvm2::runtime::gc::scoped_cli_gc_execution managed_wasm_execution{};
                runtime_gc_prepare_full_cohort();
                runtime_debug_execution_scope debug_scope{};
                entry_scope.checkpoint_participant_admitted();
                llvm_jit_generated_wasm_bridge_entry_scope generated_scope{true};
                if(runtime_execution_entry_scope::checkpoint_actual_gc_admission() == nullptr ||
                   g_checkpoint_shadow_ledger == nullptr || g_debug_activation_ledger == nullptr ||
                   g_checkpoint_shadow_ledger->size() != 0u || g_debug_activation_ledger->count() != 0u)
                { return outcome::invalid_native_environment; }
                // All input/result declarations are canonical scalar/i31/null
                // and every actual function emitted precise roots. This PRIVATE
                // invocation derives returned references only from actual
                // native normal returns, validates them against the real store,
                // and roots child→parent handoff; only final public-host output
                // accounts for a new native escape. Keep sticky state; ordinary
                // public raw-host entries retain their native escape accounting.
                actual_resume_context context{prepared, capture, current_runtime_generation()};
                resume_context_scope actual_context{context};
                auto& root{prepared.front()}; auto const invoke{reinterpret_cast<native_entry>(root.address)};
                try
                {
                    // This is the genuine already-authenticated full native
                    // continuation boundary. Late cancellation returns before
                    // guest effects; otherwise only the private generated
                    // cleanup signal may cross the selected Wasm native frames.
                    // Saved data/IDs and DWARF display do not issue this scope.
                    if(runtime_debug_shutdown_bridge::skip_cancelled_entry())
                    { return outcome::execution_retired; }
                    runtime_debug_shutdown_bridge::full_entry_scope managed_cancel_boundary{};
                    // [ALL-symbol-resolved actual generation-pinned root entry]
                    // [safe] native address comes from canonical publication;
                    // descendants are invoked only by the private TLS bridge
                    // after their REAL parent frame/roots have been restored.
                    invoke(root.landing,root.payload.data(),root.payload.size(),root.flags.data(),root.flags.size(),
                        reinterpret_cast<::std::uintptr_t>(root.output.data()),root.output.size());
                }
                catch(runtime_debug_shutdown_bridge::signal const& cancelled)
                {
                    if(!runtime_debug_shutdown_bridge::accept_at_full_entry(cancelled)) { ::fast_io::fast_terminate(); }
                    // LLVM cleanup really retired these activations; outer
                    // execution/debug/GC/FP RAII leaves after this return. Only
                    // the later actual lifetime drain proves entry retirement.
                    return outcome::execution_retired; // no host output publication
                }
                catch(runtime_checkpoint_retirement_bridge::signal const& retired)
                {
                    if(!runtime_checkpoint_retirement_bridge::accept_at_full_entry(retired)) { ::fast_io::fast_terminate(); }
                    return outcome::execution_retired; // actual cleanup completed; no host output publication
                }
                catch(unsupported_child_result const&)
                {
                    if(g_debug_activation_ledger->count() != 0u || g_checkpoint_shadow_ledger->size() != 0u ||
                       ::uwvm2::runtime::gc::current_root_frames() != nullptr) { ::fast_io::fast_terminate(); }
                    return outcome::invalid_typed_packet; // actual parent cleanup, host output unchanged
                }
                catch(::uwvm2::runtime::exception::guest_exception const& caught)
                {
                    uncaught_guest_exception_fatal(
                        ::uwvm2::utils::container::u8string_view{root.module_name.data(),root.module_name.size()},
                        capture->frames_.front().activation.function,caught);
                }
                if(!decode_actual_results(root) || g_debug_activation_ledger->count() != 0u ||
                   g_checkpoint_shadow_ledger->size() != 0u || ::uwvm2::runtime::gc::current_root_frames() != nullptr)
                { return outcome::invalid_typed_packet; }
                if(result_bytes != 0u)
                {
                    // [actual caller's exact preflighted host result owner] end
                    // [safe] count/null/end/lifetime proved before final copy;
                    // any error leaves this output untouched (resources are not rolled back).
                    // An actual non-scalar native carrier is now exposed to the
                    // trusted host ABI. Account for THIS real escape only; child
                    // return/parent SSA/root handoff alone is entirely VM internal.
                    if(root.result_escapes_host) { g_runtime.gc_collection.note_native_escape(); }
                    ::fast_io::freestanding::my_memcpy(result,prepared.front().output.data(),result_bytes);
                }
                return outcome::continued;
            }
            catch(...) { return outcome::allocation_failed; }
        }
    };
    extern "C++" void details::llvm_jit_checkpoint_bind_resume_entry_abi_bridge(
        ::std::uintptr_t module,::std::uintptr_t function,::std::uint64_t generation,::std::uint64_t site,
        void const* payload,::std::uintptr_t payload_bytes,void const* flags,::std::uintptr_t flag_count,
        ::std::uintptr_t output,::std::uintptr_t output_bytes) noexcept
    { runtime_checkpoint_continuation_dispatcher::bind_actual_entry(module,function,generation,site,payload,payload_bytes,
        flags,flag_count,output,output_bytes); }
    extern "C++" ::std::uint64_t details::llvm_jit_checkpoint_resume_input_abi_bridge(
        ::std::uint64_t incarnation,::std::uintptr_t component) noexcept
    { return runtime_checkpoint_continuation_dispatcher::read_actual_input(incarnation,component); }
    extern "C++" ::std::uintptr_t details::llvm_jit_checkpoint_resume_child_abi_bridge(
        ::std::uint64_t parent_incarnation, ::std::uint64_t waiting_site, ::std::uintptr_t result_bytes) UWVM_THROWS
    { return runtime_checkpoint_continuation_dispatcher::invoke_actual_child(parent_incarnation, waiting_site, result_bytes); }
    extern "C++" void details::llvm_jit_checkpoint_release_child_roots_abi_bridge(
        ::std::uint64_t parent_incarnation,::std::uint64_t waiting_site) noexcept
    { runtime_checkpoint_continuation_dispatcher::release_actual_child_roots(parent_incarnation,waiting_site); }
    [[nodiscard]] llvm_jit_checkpoint_continuation_status llvm_jit_checkpoint_continue_saved_leaf_host_api(
        llvm_jit_checkpoint_thread_capture_owner const& capture,void* result,::std::size_t result_bytes) noexcept
    { return runtime_checkpoint_continuation_dispatcher::execute_saved(capture,result,result_bytes,false); }
    [[nodiscard]] llvm_jit_checkpoint_continuation_status llvm_jit_checkpoint_continue_saved_thread_host_api(
        llvm_jit_checkpoint_thread_capture_owner const& capture,void* result,::std::size_t result_bytes) noexcept
    { return runtime_checkpoint_continuation_dispatcher::execute_saved(capture,result,result_bytes,true); }
}
#else
extern "C++" [[nodiscard]] llvm_jit_checkpoint_continuation_status llvm_jit_checkpoint_continue_saved_leaf_host_api(
    llvm_jit_checkpoint_thread_capture_owner const&,void*,::std::size_t) noexcept
{ return llvm_jit_checkpoint_continuation_status::requires_llvm_jit_full; }
extern "C++" [[nodiscard]] llvm_jit_checkpoint_continuation_status llvm_jit_checkpoint_continue_saved_thread_host_api(
    llvm_jit_checkpoint_thread_capture_owner const&,void*,::std::size_t) noexcept
{ return llvm_jit_checkpoint_continuation_status::requires_llvm_jit_full; }
#endif
#endif

// Link-complete fail-closed internal bridges for builds without an actual
// threaded exception-capable full-JIT continuation dispatcher. Ordinary
// profile0/observer code never invokes them; no fallback can install TLS or
// treat file/raw input as executable authority.
#if !defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) || !defined(UWVM_RUNTIME_LLVM_JIT) || !defined(UWVM_CPP_EXCEPTIONS)
extern "C++" void details::llvm_jit_checkpoint_bind_resume_entry_abi_bridge(
    ::std::uintptr_t,::std::uintptr_t,::std::uint64_t,::std::uint64_t,void const*,::std::uintptr_t,
    void const*,::std::uintptr_t,::std::uintptr_t,::std::uintptr_t) noexcept
{ ::fast_io::fast_terminate(); }
extern "C++" ::std::uint64_t details::llvm_jit_checkpoint_resume_input_abi_bridge(
    ::std::uint64_t,::std::uintptr_t) noexcept
{ ::fast_io::fast_terminate(); }
extern "C++" ::std::uintptr_t details::llvm_jit_checkpoint_resume_child_abi_bridge(
    ::std::uint64_t,::std::uint64_t,::std::uintptr_t) UWVM_THROWS
{ ::fast_io::fast_terminate(); }
extern "C++" void details::llvm_jit_checkpoint_release_child_roots_abi_bridge(
    ::std::uint64_t,::std::uint64_t) noexcept
{ ::fast_io::fast_terminate(); }
#endif

// PRIVATE native producer candidate. Include inside the real runtime namespace
// only AFTER canonical full publications, exact debug activations/checkpoint TLS
// and current-generation helpers exist. This file is not presently included.
// A typed logical capture is one stopped-thread prerequisite, NEVER worldstop,
// a GC root registration, protected database publication or executable restore.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
// Same nonexported global-module attachment as the private host bridge. This
// exact qualified friend avoids a second anonymous class with the same name.
extern "C++" { class runtime_checkpoint_coherent_manager; class runtime_checkpoint_continuation_dispatcher; class runtime_checkpoint_gc_state_borrow; class runtime_checkpoint_retirement_bridge; }
namespace
{
    enum class checkpoint_thread_capture_status : unsigned char
    {
        captured, not_selected, not_actual_before_park, stale_ticket, invalid_activation,
        incomplete_logical_frames, stale_code_generation, invalid_publication,
        invalid_typed_packet, unavailable_reference_roots, unavailable_exception_continuation,
        registry_exhausted, allocation_failed, non_replayable_import, activation_resource_exhausted
    };
    // Owns copied logical DATA plus canonical source/plan lifetime pins. It does
    // not independently retain an ExecutionEngine or native stack. A consumer
    // must reacquire an actual runtime-generation lease, ONE authenticated all-
    // participant domain transaction, host-operation closure and publication.
    class runtime_checkpoint_thread_capture final
    {
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_with_source_frame_memory_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_source_binding_owner const&,
            ::std::uint_least64_t, void*, llvm_jit_debug_source_memory_callback) noexcept -> bool;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_coherent_manager;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_continuation_dispatcher;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_gc_state_borrow;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_retirement_bridge;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_guest_worker_bridge;
        using logical_frame = ::uwvm2::runtime::checkpoint::logical_frame;
        using activation_frame = details::debug_activation::frame;
        using source_owner = ::uwvm2::uwvm::runtime::full::full_source_instance::owner;
        using domain = ::uwvm2::utils::thread::cooperative_pause_domain;
        struct owned_frame
        {
            logical_frame logical{};
            activation_frame activation{};
            source_owner source{};
            // Comparison token only: the original live entry owns the engine.
            // NEVER dereference this outside a fresh actual publication guard.
            full_code_publication const* publication_identity{};
        };
        runtime_checkpoint_thread_capture() = default;
        ::std::shared_ptr<domain> control_{};
        domain::pause_ticket ticket_{};
        ::uwvm2::utils::thread::cooperative_pause_location location_{};
        ::uwvm2::runtime::checkpoint::compilation_profile::owner profile_{};
        ::std::uint_least64_t participant_{};
        bool suspended_wait_{};
        ::std::vector<owned_frame> frames_{};
        // Comparison/borrow only. NEVER read after this actual episode ends.
        ::uwvm2::runtime::gc::managed_entry_admission::shared_lease const* actual_gc_lease_{};
        ::uwvm2::runtime::gc::root_frame const* actual_gc_frames_{};
        inline static ::std::mutex registry_mutex_{};
        inline static ::std::weak_ptr<runtime_checkpoint_thread_capture const> registry_[256u]{};

        template<typename Owner>
        [[nodiscard]] static bool same_owner(Owner const& a, Owner const& b) noexcept
        { return a && b && a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a); }
        // An observation is a comparison-only carrier copy. Only the real
        // current-episode roots/N/publication manager may inspect it later.
        // Executable DATA and continuation still use the strict scalar purpose.
        enum class value_use : unsigned char { executable_scalar, live_observation };
        [[nodiscard]] static checkpoint_thread_capture_status check_scalar_reference(
            ::uwvm2::runtime::checkpoint::native_value const& value,
            value_use use = value_use::executable_scalar) noexcept
        {
            namespace checkpoint = ::uwvm2::runtime::checkpoint;
            if(value.declaration.type.kind != checkpoint::types::value_kind::reference || !value.declaration.initialized)
            { return checkpoint_thread_capture_status::captured; }
            checkpoint::native_reference reference{};
            static_assert(sizeof(reference) <= checkpoint::native_slot_bytes);
            // [actual privately owned complete native bits[16]] end
            // [safe                                          ] fixed extent
            // bounds memcpy into a constructed carrier; no token dereference.
            ::std::memcpy(::std::addressof(reference), value.bits.data(), sizeof(reference));
            using kind = ::uwvm2::object::global::wasm_ref_kind;
            if(reference.kind == kind::wasm_null)
            {
                return value.declaration.type.nullable ? checkpoint_thread_capture_status::captured :
                    checkpoint_thread_capture_status::invalid_typed_packet;
            }
            if(reference.kind == kind::wasm_i31)
            {
                auto const heap{value.declaration.type.heap};
                if(heap.is_defined()) { return checkpoint_thread_capture_status::invalid_typed_packet; }
                using abstract = checkpoint::types::abstract_heap_type;
                auto const code{static_cast<abstract>(heap.code)};
                return code == abstract::i31 || code == abstract::eq || code == abstract::any ?
                    checkpoint_thread_capture_status::captured : checkpoint_thread_capture_status::invalid_typed_packet;
            }
            // Future complete GC/static/native-exception roots and external
            // replay resources must be really registered before copying live
            // history can outlast a collection. A source/store shared_ptr pin
            // keeps ownership alive but does not prove object liveness. Do not
            // serialize, inspect or publish these presently unrooted carriers.
            switch(reference.kind)
            {
                case kind::wasm_func_imported: case kind::wasm_func_defined:
                case kind::wasm_struct: case kind::wasm_array: case kind::wasm_exn: case kind::wasm_extern:
                    // No supplied token is dereferenced here. The actual before-
                    // park producer owns generated roots and its entry GC lease;
                    // a later observation requires the SAME live episode and N
                    // exclusion before canonical registry/type/payload lookup.
                    if(use == value_use::live_observation && reference.storage.ptr != nullptr)
                    { return checkpoint_thread_capture_status::captured; }
                    return checkpoint_thread_capture_status::unavailable_reference_roots;
                case kind::wasm_func: return checkpoint_thread_capture_status::invalid_typed_packet;
                case kind::wasm_null: case kind::wasm_i31: break;
            }
            return checkpoint_thread_capture_status::invalid_typed_packet;
        }
        [[nodiscard]] static checkpoint_thread_capture_status check_owned_values(logical_frame const& frame, value_use use = value_use::executable_scalar) noexcept
        {
            namespace checkpoint = ::uwvm2::runtime::checkpoint;
            if(!frame.plan || !frame.materialized || frame.site == 0u || frame.site > frame.plan->get().sites.size())
            { return checkpoint_thread_capture_status::incomplete_logical_frames; }
            // [independently sealed exact logical sites ... site-1 ...] end
            // [safe                                                 ] site>0
            // and site<=N BEFORE subtracting/indexing the owned metadata.
            auto const& site{frame.plan->get().sites[static_cast<::std::size_t>(frame.site - 1u)]};
            if(checkpoint::validate_site(site, frame.plan->get()) != checkpoint::status::ok || frame.values.size() != site.slots.size())
            { return checkpoint_thread_capture_status::invalid_typed_packet; }
            // A half-consumed native catch needs an actual pending exception
            // owner and continuation; lexical clauses never stand for that state.
            if(site.phase == checkpoint::frame_phase::exception_continuation)
            { return checkpoint_thread_capture_status::unavailable_exception_continuation; }
            if(use == value_use::executable_scalar && !site.handlers.empty())
            {
                // Nested lexical EH is admitted only at an actual emitted ABI2
                // entry from this sealed resumable plan. This metadata proof
                // grants no execution: check_actual_frame and the canonical
                // capture/retirement/dispatcher still require the real native
                // source, generation, publication, roots and coherent episode.
                if(!frame.plan->get().profile ||
                   frame.plan->get().profile->purpose() != checkpoint::compilation_purpose::resumable ||
                   frame.plan->get().resume_abi_revision != 2u ||
                   (site.phase != checkpoint::frame_phase::before_opcode &&
                    site.phase != checkpoint::frame_phase::awaiting_call_return))
                { return checkpoint_thread_capture_status::unavailable_exception_continuation; }
                bool emitted{};
                for(auto const actual : frame.plan->get().resume_sites)
                { if(actual == frame.site) { emitted = true; break; } }
                if(!emitted) { return checkpoint_thread_capture_status::unavailable_exception_continuation; }
            }
            for(::std::size_t i{}; i != frame.values.size(); ++i)
            {
                auto const& actual{frame.values[i]}; auto const& declared{site.slots[i]};
                if(actual.declaration.type != declared.type || (declared.initialized && !actual.declaration.initialized) ||
                   (!actual.declaration.initialized && (i >= site.local_count ||
                    actual.declaration.type.kind != checkpoint::types::value_kind::reference || actual.declaration.type.nullable)))
                { return checkpoint_thread_capture_status::invalid_typed_packet; }
                if(!actual.declaration.initialized)
                {
                    // Unset storage is producer-cleared DATA, never a load of
                    // an uninitialized LLVM alloca and never a fabricated null.
                    for(auto byte : actual.bits)
                    { if(byte != ::std::byte{}) { return checkpoint_thread_capture_status::invalid_typed_packet; } }
                }
                auto const reference{check_scalar_reference(actual, use)};
                if(reference != checkpoint_thread_capture_status::captured) { return reference; }
            }
            return checkpoint_thread_capture_status::captured;
        }
        [[nodiscard]] static checkpoint_thread_capture_status check_actual_frame(
            activation_frame const& actual, logical_frame const& logical, full_code_publication const*& code,
            source_owner& source) noexcept
        {
            // Only the genuine synchronous before-park producer calls here.
            // Its outer real execution-generation lease remains live, and this
            // participant has not yet parked: reset cannot destroy code and an
            // all-participant stopped replacement cannot mutate publication.
            // compiled_all is published before this execution starts. Future
            // publishers must retain that actual lease/allpark protocol; an
            // arbitrary caller depth/epoch alone would not justify these reads.
            namespace checkpoint = ::uwvm2::runtime::checkpoint;
            if(actual.module >= g_runtime.modules.size() || actual.runtime_epoch != current_runtime_generation() ||
               actual.incarnation == 0u || actual.continuation == 0u ||
               logical.identity != checkpoint::activation_identity{actual.incarnation, actual.parent, actual.continuation, actual.runtime_epoch})
            { return checkpoint_thread_capture_status::invalid_activation; }
            // [actual generation-owned dense module array ... module ...] end
            // [safe                                                   ] module<N
            // BEFORE record read; no request-selected pointer is dereferenced.
            auto const& record{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(actual.module))};
            auto const module{record.runtime_module}; auto const publication{record.llvm_jit_full_publication.get()};
            if(module == nullptr || !record.llvm_jit_ready || !record.llvm_jit_precise_gc_roots_emitted || publication == nullptr ||
               !publication->engine || !publication->context || publication->plan ||
               !same_owner(publication->checkpoint_profile, g_runtime.checkpoint_profile))
            { return checkpoint_thread_capture_status::invalid_publication; }
            auto const& owning_source{publication->source};
            if(!::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(owning_source) ||
               !owning_source->initialized_from_actual_state() ||
               owning_source->actual_validated_file(static_cast<::std::size_t>(actual.module), actual.runtime_epoch, module) == nullptr)
            { return checkpoint_thread_capture_status::invalid_publication; }
            auto const member{owning_source->registry().find(record.module_name)};
            if(member == owning_source->registry().end() || ::std::addressof(member->second) != module)
            { return checkpoint_thread_capture_status::invalid_publication; }
            auto const imports{module->imported_function_vec_storage.size()};
            if(actual.function < imports || actual.function - imports >= record.llvm_jit_compiled.local_funcs.size() ||
               actual.function - imports >= record.llvm_jit_debug_full_entry_generations.size())
            { return checkpoint_thread_capture_status::invalid_publication; }
            auto const local{static_cast<::std::size_t>(actual.function - imports)};
            auto const metadata{checkpoint_current_generation_plan(record, local, actual.function_generation)};
            // Resolve the actual current engine/typed/raw targets and exact
            // retained compiler plan. Old generation metadata cannot authorize
            // either replacement-native slots or a new logical continuation.
            if(!metadata || record.llvm_jit_debug_full_entry_generations[local] != actual.function_generation ||
               !same_owner(metadata, logical.plan) || !same_owner(metadata->get().profile, publication->checkpoint_profile) ||
               metadata->get().module != actual.module || metadata->get().function != actual.function ||
               metadata->get().function_generation != actual.function_generation)
            { return checkpoint_thread_capture_status::stale_code_generation; }
            code = publication; source = owning_source; return checkpoint_thread_capture_status::captured;
        }

        // Only the real coherent manager can resolve the canonical owner. A
        // foreign shared_ptr alias must not even be dereferenced as a capture.
        [[nodiscard]] static ::std::shared_ptr<runtime_checkpoint_thread_capture const> canonical(
            ::std::shared_ptr<runtime_checkpoint_thread_capture const> const& supplied) noexcept
        {
            if(!supplied) { return {}; }
            ::std::lock_guard lock{registry_mutex_};
            for(auto const& slot : registry_)
            {
                auto const actual{slot.lock()};
                if(same_owner(actual, supplied)) { return actual; }
            }
            return {};
        }
    public:
        using owner = ::std::shared_ptr<runtime_checkpoint_thread_capture const>;
        struct result { checkpoint_thread_capture_status status{checkpoint_thread_capture_status::not_selected}; owner capture{}; };
        runtime_checkpoint_thread_capture(runtime_checkpoint_thread_capture const&) = delete;
        runtime_checkpoint_thread_capture& operator=(runtime_checkpoint_thread_capture const&) = delete;
        ~runtime_checkpoint_thread_capture() = default;
        // This native-only method accepts an actual privately constructed ticket,
        // not an epoch/participant bool, and proves its genuine TLS producer. No
        // guest import or command is added. A successfully copied capture still
        // conveys no executable resume, snapshot or protected-asset permission.
        [[nodiscard]] static result mint_current_before_park(domain::pause_ticket const& ticket) noexcept
        {
            namespace checkpoint = ::uwvm2::runtime::checkpoint;
            auto const shadow{g_checkpoint_shadow_ledger}; auto const activations{g_debug_activation_ledger};
            if(shadow == nullptr || !g_runtime.checkpoint_profile) { return {}; }
            if(!ticket || !g_runtime.debug_pause_control || !g_debug_observer_active || !g_debug_activation_park_site.before_park ||
               g_debug_pause_participant == nullptr || get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u)
            { return {checkpoint_thread_capture_status::not_actual_before_park, {}}; }
            if(shadow->failure() == checkpoint::status::non_replayable_import)
            { return {checkpoint_thread_capture_status::non_replayable_import, {}}; }
            if(shadow->failure() == checkpoint::status::quota_exceeded)
            { return {checkpoint_thread_capture_status::activation_resource_exhausted, {}}; }
            if(activations != nullptr && (activations->failure_reason() == details::debug_activation::failure::resource_exhausted ||
               activations->failure_reason() == details::debug_activation::failure::allocation_failed))
            { return {checkpoint_thread_capture_status::activation_resource_exhausted, {}}; }
            if(activations == nullptr || !activations->complete() || shadow->failure() != checkpoint::status::ok ||
               !shadow->has_complete_materialized_frames())
            { return {checkpoint_thread_capture_status::incomplete_logical_frames, {}}; }
            try
            {
                auto const control{g_runtime.debug_pause_control};
                // The actual current participant has not parked. The current
                // request must be incomplete. No nested publication/domain lock
                // is entered; canceled/replaced/native-step tickets fail closed.
                if(control->capture(ticket).result != ::uwvm2::utils::thread::cooperative_pause_result::timeout)
                { return {checkpoint_thread_capture_status::stale_ticket, {}}; }
                auto const location{g_debug_activation_park_site.location};
                auto const count{activations->count()}; auto const logical{shadow->frames_while_actually_stopped()};
                if(count == 0u || count != logical.size() ||
                   !g_runtime.compiled_all.load(::std::memory_order_acquire))
                { return {checkpoint_thread_capture_status::incomplete_logical_frames, {}}; }
                // [actual bounded scope-owned activation array ... count] end
                // [safe                                                 ] count
                // checked before current-frame indexing; no native stack read.
                auto const actual{activations->data()};
                if(actual[count - 1u].module != location.code_unit || actual[count - 1u].function != location.function ||
                   actual[count - 1u].runtime_epoch != location.code_generation)
                { return {checkpoint_thread_capture_status::invalid_activation, {}}; }
                auto const* gc_lease{runtime_execution_entry_scope::checkpoint_actual_gc_admission()};
                if(gc_lease == nullptr) { return {checkpoint_thread_capture_status::unavailable_reference_roots, {}}; }
                ::std::shared_ptr<runtime_checkpoint_thread_capture> candidate{new runtime_checkpoint_thread_capture};
                candidate->actual_gc_lease_ = gc_lease;
                candidate->actual_gc_frames_ = ::uwvm2::runtime::gc::current_root_frames();
                candidate->frames_.reserve(count);
                for(::std::size_t i{}; i != count; ++i)
                {
                    auto const& event{actual[i]}; auto const& frame{logical[i]};
                    if(event.parent != (i == 0u ? 0u : actual[i - 1u].incarnation))
                    { return {checkpoint_thread_capture_status::invalid_activation, {}}; }
                    // Resolve the actual canonical compiler plan/control block
                    // BEFORE interpreting any copied typed payload or metadata.
                    full_code_publication const* code{}; source_owner source{};
                    auto const identity{check_actual_frame(event, frame, code, source)};
                    if(identity != checkpoint_thread_capture_status::captured) { return {identity, {}}; }
                    auto const values{check_owned_values(frame, value_use::live_observation)};
                    if(values != checkpoint_thread_capture_status::captured) { return {values, {}}; }
                    auto const& site{frame.plan->get().sites[static_cast<::std::size_t>(frame.site - 1u)]};
                    if((i + 1u == count && (site.phase != checkpoint::frame_phase::before_opcode || site.opcode_offset != location.offset)) ||
                       (i + 1u != count && site.phase != checkpoint::frame_phase::awaiting_call_return))
                    { return {checkpoint_thread_capture_status::incomplete_logical_frames, {}}; }
                    candidate->frames_.push_back({frame, event, ::std::move(source), code});
                }
                candidate->control_ = control; candidate->ticket_ = ticket; candidate->location_ = location;
                candidate->participant_ = g_debug_pause_participant->identifier();
                candidate->suspended_wait_ = g_debug_activation_park_site.suspended_wait;
                candidate->profile_ = g_runtime.checkpoint_profile;
                if(candidate->participant_ == 0u) { return {checkpoint_thread_capture_status::invalid_activation, {}}; }
                // Publish only a complete private capture in a bounded canonical
                // registry; do not evict a live owner or accept a caller alias.
                ::std::lock_guard lock{registry_mutex_};
                for(auto& slot : registry_)
                {
                    if(slot.expired()) { slot = candidate; return {checkpoint_thread_capture_status::captured, candidate}; }
                }
                return {checkpoint_thread_capture_status::registry_exhausted, {}};
            }
            catch(...) { return {checkpoint_thread_capture_status::allocation_failed, {}}; }
        }
    };
}
#endif

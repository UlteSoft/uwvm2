// Private SOURCE candidate: typed creation origin and DATA retention, not GC admission.
#pragma once
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
#if !defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) || UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES != 1
# error "exception source retention requires homogeneous external handles"
#endif
#ifndef UWVM_MODULE
# include <atomic>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <optional>
# include <span>
# include <utility>
# include <vector>
# include <uwvm2/runtime/exception/activation.h>
# include <uwvm2/runtime/exception/external_handle.h>
# include <uwvm2/runtime/gc/entry_admission.h>
# include <uwvm2/runtime/gc/managed_exception_cohort.h>
# include <uwvm2/uwvm/runtime/storage/full.h>
# include <uwvm2/runtime/lib/uwvm_runtime.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::gc
{
    namespace source_exn = ::uwvm2::runtime::exception;
    namespace source_storage = ::uwvm2::uwvm::runtime::storage;
    namespace source_core = ::uwvm2::parser::wasm::standard::wasm3::type;

    class source_exception_publisher final : public source_exn::external_exception_lifetime
    {
        friend class ::uwvm2::runtime::lib::source_exception_guest_bridge;
        using native_leaf_access = source_exn::external_exception_native_leaf_access<source_exception_publisher>;
        using source_type = ::uwvm2::uwvm::runtime::full::full_source_instance;
        using store_type = source_storage::gc_object_store;
        using module_type = source_storage::wasm_module_storage_t;
        // Reverse release: domain, module lease, store, source. Internal-record
        // destruction has already happened before its external lifetime pin dies.
        source_type::owner source_;
        ::std::shared_ptr<store_type> store_;
        ::std::shared_ptr<source_storage::gc_lease_owner> module_lease_;
        ::std::shared_ptr<source_exn::native_exception_root_domain> domain_;
        module_type const* module_{};
        ::std::uint_least64_t serial_{}, observed_epoch_{};
        mutable ::std::atomic_bool retired_{};
        ::std::weak_ptr<source_exception_publisher const> canonical_owner_{};
        class construction_key
        {
            friend class source_exception_publisher;
            construction_key() noexcept = default;
        public:
            construction_key(construction_key const&) noexcept = default;
        };
        [[nodiscard]] bool owns_current_initialized_graph() const noexcept
        {
            // This selected-source read follows the existing externally
            // serialized native administration / genuine outer execution-lease
            // contract. Its control block is not inferred from TLS/depth/count.
            auto selected{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
            return selected && selected.get()==source_.get() &&
                !selected.owner_before(source_) && !source_.owner_before(selected) &&
                source_->initialized_main_module()==module_ &&
                module_->gc_collection_phase.ready_for(serial_) &&
                published_initializer_serial.load(::std::memory_order_acquire)==serial_;
        }
        [[nodiscard]] static bool same_root_owner(source_exn::instance_root const& root) noexcept
        {
            // Require BOTH null address and genuinely empty control block.
            // A nonempty owner aliasing null is not an EMPTY payload root.
            source_exn::instance_root empty{};
            return !root && !root.owner_before(empty) && !empty.owner_before(root);
        }
        [[nodiscard]] static source_exn::payload_kind carrier_kind(unsigned raw) noexcept
        {
            using kind=source_exn::payload_kind;
            switch(raw)
            {
                case 0x7fu: return kind::i32; case 0x7eu: return kind::i64;
                case 0x7du: return kind::f32; case 0x7cu: return kind::f64;
                case 0x7bu: return kind::v128;
                case 0x69u: case 0x6fu: case 0x70u: return kind::wasm_reference;
                default: return kind::reference; // unsupported native/host kind
            }
        }
        [[nodiscard]] bool preflight(::std::size_t tag_index,
            ::std::span<source_exn::payload_field const> fields) const noexcept
        {
            if(tag_index>=module_->local_defined_tag_vec_storage.size()) { return false; }
            // [immutable source-owned tag array][tag_index<size]
            // [safe] borrow one actual record only while this source pin lives.
            auto const& tag{module_->local_defined_tag_vec_storage.index_unchecked(tag_index)};
            if(!tag.exception_identity || tag.function_type_ptr==nullptr ||
               tag.function_type_ptr->result.begin!=tag.function_type_ptr->result.end) { return false; }
            auto const& parameters{tag.function_type_ptr->parameter};
            if(parameters.begin!=parameters.end && (parameters.begin==nullptr || parameters.end==nullptr)) { return false; }
            // [actual parser-owned same-allocation endpoints]
            // [safe] no caller supplies a schema pointer; empty skips subtraction.
            auto const count{parameters.begin==parameters.end ? 0uz :
                static_cast<::std::size_t>(parameters.end-parameters.begin)};
            if(fields.size()!=count) { return false; }
            auto const& types{module_->type_section_storage};
            source_storage::wasm_binfmt1_owned_signature_t const* rich{};
            if(types.owned_signature_begin!=nullptr || types.owned_signature_end!=nullptr)
            {
                if(types.owned_signature_begin==nullptr || types.owned_signature_end==nullptr) { return false; }
                // [source-owned immutable rich-signature allocation]
                // [safe] actual initialized endpoints are one array, not caller data.
                auto const size{static_cast<::std::size_t>(types.owned_signature_end-types.owned_signature_begin)};
                if(tag.type_index>=size) { return false; }
                // [size actual signatures][tag.type_index<size]
                // [safe] select only an existing pinned schema object.
                rich=::std::addressof(types.owned_signature_begin[tag.type_index]);
                if(rich->parameters.size()!=count || !rich->results.empty()) { return false; }
            }
            for(::std::size_t index{};index!=count;++index)
            {
                // [complete fields span][actual parameters count]
                // [safe] both indices are bounded before reading kind or bytes.
                auto const kind{carrier_kind(static_cast<unsigned>(parameters.begin[index]))};
                auto const& field{fields[index]};
                if(kind==source_exn::payload_kind::reference || field.kind()!=kind ||
                   field.bits().size()!=source_exn::payload_width(kind) || !same_root_owner(field.root())) { return false; }
                if(kind==source_exn::payload_kind::wasm_reference)
                {
                    // Legacy extern/funcref or missing rich signature is not a
                    // local heap-origin proof. Exn/extern/functions decline below.
                    if(rich==nullptr || field.bits().size()!=sizeof(source_storage::gc_reference)) { return false; }
                    auto const& expected{rich->parameters.index_unchecked(index)};
                    if(expected.kind!=source_core::value_kind::reference) { return false; }
                    source_storage::gc_reference reference{};
                    // [exact immutable carrier extent][aligned native carrier]
                    // [safe] copy bytes; never cast token payload into an object.
                    ::std::memcpy(::std::addressof(reference),field.bits().data(),sizeof(reference));
                    // First co-collection slice keeps nested exn, funcref,
                    // extern and foreign owners on the original decline path.
                    // Removing the tag gate never weakens actual payload proof.
                    if(!store_->exception_payload_is_local_member(reference) ||
                       !store_->reference_type_matches(reference,expected)) { return false; }
                }
            }
            return true;
        }
    public:
        using owner=::std::shared_ptr<source_exception_publisher const>;
        class produced_value final
        {
            friend class source_exception_publisher;
            owner origin_{};
            source_exn::immutable_record_ref record_{};
            // Release the real exclusion FIRST at receipt destruction; then
            // destroy the internal record and finally its typed source pin.
            // This protects the unregistered create->materialize gap without
            // pretending a source/store pin alone is a collector root.
            managed_entry_admission::shared_lease admission_{};
            bool private_native_leaf_{};
            produced_value(owner origin, source_exn::immutable_record_ref record,
                managed_entry_admission::shared_lease admission, bool private_native_leaf = false) noexcept
                : origin_{::std::move(origin)},record_{::std::move(record)},admission_{::std::move(admission)},
                  private_native_leaf_{private_native_leaf} {}
        public:
            produced_value() noexcept = default;
            produced_value(produced_value const&)=delete;
            produced_value& operator=(produced_value const&)=delete;
            produced_value(produced_value&&)=default;
            // Deleted assignment avoids implicit last-record release under a
            // caller's outer locks. Explicit reset has the documented boundary.
            produced_value& operator=(produced_value&&)=delete;
            [[nodiscard]] explicit operator bool() const noexcept { return origin_ && record_; }
            void reset_after_all_outer_locks() noexcept { admission_.reset();record_.reset();origin_.reset(); }
        };
        explicit source_exception_publisher(construction_key, source_type::owner source,
            ::std::shared_ptr<source_exn::native_exception_root_domain> domain,
            ::std::uint_least64_t serial, ::std::uint_least64_t epoch) noexcept
            : source_{::std::move(source)},store_{source_->initialized_main_module()->gc_store},
              module_lease_{source_->initialized_main_module()->gc_lease_roots},
              domain_{::std::move(domain)},module_{source_->initialized_main_module()},
              serial_{serial},observed_epoch_{epoch} {}
        source_exception_publisher(source_exception_publisher const&)=delete;
        source_exception_publisher& operator=(source_exception_publisher const&)=delete;
        ~source_exception_publisher() noexcept override = default;
        [[nodiscard]] static bool canonical(owner const& candidate) noexcept
        { return candidate && !candidate->canonical_owner_.owner_before(candidate) &&
            !candidate.owner_before(candidate->canonical_owner_); }

        // COLD serialized native setup only; no callback bool, generic Owner,
        // arbitrary registry/module/epoch, or external value enters the factory.
        // Query the real existing locked full-code observer ourselves. Its
        // recorded integers are observations; source/store/control blocks pin data.
        [[nodiscard]] static owner prepare_from_actual_full(source_type::owner source)
        {
            if(!source_type::has_canonical_owner(source) || !source->initialized_from_actual_state() ||
               source->registry().size()!=1uz) { return {}; }
            auto const view{::uwvm2::runtime::lib::llvm_jit_full_source_publication_host_api(source->assigned_main_module_id())};
            auto const* module{source->initialized_main_module()};
            if(!view.ready || !view.canonical_source || !view.initialized_source || !view.publication_owns_source ||
               view.exception_source_debug_management || view.runtime_epoch==0u ||
               view.engine_address==0u || view.llvm_context_address==0u ||
               view.source_address!=reinterpret_cast<::std::uintptr_t>(source.get()) ||
               view.module_address!=reinterpret_cast<::std::uintptr_t>(module) ||
               module==nullptr || !module->gc_store || !module->gc_store->valid() || !module->gc_lease_roots) { return {}; }
            if(!module->imported_function_vec_storage.empty() || !module->imported_table_vec_storage.empty() ||
               !module->imported_global_vec_storage.empty() || !module->imported_memory_vec_storage.empty() ||
               !module->imported_tag_vec_storage.empty()) { return {}; }
            auto const serial{published_initializer_serial.load(::std::memory_order_acquire)};
            if(!module->gc_collection_phase.ready_for(serial)) { return {}; }
            auto const& store{module->gc_store};
            auto const canonical_store{store->weak_from_this()};
            if(canonical_store.owner_before(store) || store.owner_before(canonical_store)) { return {}; }
            auto domain{source_exn::native_exception_root_domain::make()};
            auto mutable_owner{::std::make_shared<source_exception_publisher>(construction_key{},source,domain,serial,view.runtime_epoch)};
            mutable_owner->canonical_owner_=mutable_owner;
            return mutable_owner;
        }
        [[nodiscard]] static ::std::optional<produced_value> publish_fresh(owner const& producer,
            ::std::size_t actual_tag_index, ::std::span<source_exn::payload_field const> fields,
            source_exn::diagnostic_trace_ref trace={})
        {
            if(!canonical(producer) || producer->retired_.load(::std::memory_order_acquire) ||
               producer->domain_->closed()) { return {}; }
            // REAL global reader admission blocks exclusive sweep until the
            // complete root/typed schema preflight and immutable construction end.
            // It is NOT a generation/cohort authentication substitute.
            auto admission{runtime_gc_entry_admission.enter()};
            if(!producer->owns_current_initialized_graph() || !producer->preflight(actual_tag_index,fields)) { return {}; }
            // [preflight-proven source-owned local tag][complete immutable inputs]
            // [safe] construct the value HERE. An A/raw record cannot be relabeled B.
            auto const& tag{producer->module_->local_defined_tag_vec_storage.index_unchecked(actual_tag_index)};
            // Retain the original trace parameter through a failed make: locals
            // (including real admission) retire BEFORE this parameter's last
            // owner. Successful record construction owns the same trace object.
            auto raw{source_exn::value::make(tag.exception_identity,fields,trace)};
            auto record{source_exn::value::record_from_native(raw)};
            if(!record) { return {}; }
            // The private receipt keeps real exclusion AND source alive until
            // external registration completes. Collector cannot skip this gap.
            // No strong source pin is inserted into the internal immutable record.
            return ::std::optional<produced_value>{produced_value{producer,::std::move(record),::std::move(admission)}};
        }
        [[nodiscard]] static source_exn::value_ref materialize_for_recipient(
            owner const& recipient, produced_value& produced)
        {
            if(!canonical(recipient) || !produced || !canonical(produced.origin_) ||
               recipient.get()!=produced.origin_.get() ||
               recipient.owner_before(produced.origin_) || produced.origin_.owner_before(recipient) ||
               recipient->retired_.load(::std::memory_order_acquire)) { return {}; }
            // Exact immutable publisher/control block carries origin. No raw value,
            // tag shape, module ID, epoch, or caller-supplied bool can replace it.
            auto candidate{produced.record_.native_untracked_owner()};
            auto lifetime{::std::static_pointer_cast<source_exn::external_exception_lifetime const>(recipient)};
            auto result{produced.private_native_leaf_ ?
                native_leaf_access::materialize(recipient->domain_,candidate,::std::move(lifetime)) :
                source_exn::external_exception_handle::materialize_retained(
                    recipient->domain_,candidate,::std::move(lifetime))};
            if(result)
            {
                // Called outside ALL outer locks in this SOURCE witness. Real VM
                // use requires ROOT's genuine deferred-last-release integration.
                produced.reset_after_all_outer_locks();
            }
            return result; // failure never consumes the private receipt
        }
        [[nodiscard]] source_exn::value_ref reuse_existing_external(owner const& recipient,
            source_exn::value_ref const& candidate) const
        {
            if(!canonical(recipient) || recipient.get()!=this || source_exn::value::record_from_native(candidate)) { return {}; }
            auto retained{source_exn::external_exception_handle::retained_lifetime(candidate)};
            auto expected{::std::static_pointer_cast<source_exn::external_exception_lifetime const>(recipient)};
            if(!retained || retained.get()!=expected.get() ||
               retained.owner_before(expected) || expected.owner_before(retained)) { return {}; }
            // Existing same-origin root remains genuine after close. Foreign or
            // generic/unretained external candidates decline WITHOUT consumption.
            return source_exn::external_exception_handle::materialize(domain_,candidate);
        }
        void close_after_actual_code_drain() const noexcept
        { retired_.store(true,::std::memory_order_release);domain_->close(); }
        [[nodiscard]] ::std::shared_ptr<source_exn::native_exception_root_domain> const& census_lifetime() const noexcept { return domain_; }
        [[nodiscard]] source_type::owner const& retained_source() const noexcept { return source_; }
        [[nodiscard]] ::std::shared_ptr<store_type> const& retained_store() const noexcept { return store_; }
        [[nodiscard]] ::std::shared_ptr<source_storage::gc_lease_owner> const& retained_module_lease() const noexcept { return module_lease_; }
        [[nodiscard]] ::std::uint_least64_t observed_epoch() const noexcept { return observed_epoch_; }

    private:
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
        [[nodiscard]] static bool is_actual_private_native_value(owner const& producer,
            source_exn::value_ref const& candidate) noexcept
        {
            if(!canonical(producer) || producer->retired_.load(::std::memory_order_acquire) ||
               producer->domain_->closed() || !producer->owns_current_initialized_graph()) { return false; }
            // Registry visits complete before either temporary owner releases.
            // Certificate is type-level ONLY; exact canonical producer CB is
            // required independently for each genuine complete external block.
            auto record{source_exn::external_exception_handle::registered_record(candidate)};
            auto provenance{source_exn::external_exception_handle::source_native_provenance(candidate)};
            auto expected{::std::static_pointer_cast<source_exn::external_exception_lifetime const>(producer)};
            return record && provenance.lifetime && provenance.lifetime.get()==expected.get() &&
                !provenance.lifetime.owner_before(expected) && !expected.owner_before(provenance.lifetime) &&
                native_leaf_access::matches_captured_certificate(provenance.certificate);
        }
        [[nodiscard]] static ::std::optional<managed_exception_cohort> mint_actual_managed_cohort(owner const& producer) noexcept
        {
            if(!canonical(producer) || producer->retired_.load(::std::memory_order_acquire) ||
               producer->domain_->closed() || !producer->owns_current_initialized_graph() ||
               producer->source_->registry().size() != 1uz || producer->observed_epoch_ == 0u ||
               producer->module_->local_defined_tag_vec_storage.empty()) { return {}; }
            return construct_private_source_data<managed_exception_cohort>(
                ::std::static_pointer_cast<source_exn::external_exception_lifetime const>(producer),
                producer->store_,producer->domain_,producer->module_,producer->serial_,producer->observed_epoch_);
        }
        [[nodiscard]] static source_exn::value_ref materialize_local_token(owner const& producer,
            source_storage::gc_reference reference)
        {
            if(!canonical(producer) || producer->retired_.load(::std::memory_order_acquire) ||
               producer->domain_->closed() || !producer->owns_current_initialized_graph()) { return {}; }
            auto origin{::std::static_pointer_cast<source_exn::external_exception_lifetime const>(producer)};
            void const* certificate{};
            auto record{producer->store_->lookup_local_source_exception(reference,origin,::std::addressof(certificate))};
            if(!record || !native_leaf_access::matches_captured_certificate(certificate)) { return {}; }
            // SAME immutable record/tag plus exact issuer/origin control blocks
            // were authenticated before the type-level certificate. Generic or
            // foreign native owners never become this private source population.
            return native_leaf_access::materialize(producer->domain_,record.native_untracked_owner(),::std::move(origin));
        }
#endif
        // Only the actual runtime bridge can transfer its freshly built private
        // vector. All mutable aliases are retired there. The public span-copy
        // factory, generic native records and foreign values cannot issue this
        // receipt. Source/tag/store checks alone never prove mutable ownership.
        [[nodiscard]] static ::std::optional<produced_value> publish_native_owned(
            owner const& producer, ::std::size_t actual_tag_index,
            ::std::vector<source_exn::payload_field>&& private_fields,
            source_exn::diagnostic_trace_ref trace)
        {
            if(!canonical(producer) || producer->retired_.load(::std::memory_order_acquire) ||
               producer->domain_->closed()) { return {}; }
            source_exn::diagnostic_trace_ref empty_trace{};
            if(trace ? !source_exn::diagnostic_trace::has_canonical_owner(trace) :
                (trace.owner_before(empty_trace) || empty_trace.owner_before(trace))) { return {}; }
            auto admission{runtime_gc_entry_admission.enter()};
            if(!producer->owns_current_initialized_graph() ||
               !producer->preflight(actual_tag_index,private_fields)) { return {}; }
            // Every decline above leaves private_fields intact. After this
            // point the runtime must not fall back using moved payload storage.
            auto const& tag{producer->module_->local_defined_tag_vec_storage.index_unchecked(actual_tag_index)};
            // Copy trace through allocation failure so its last native release
            // cannot happen while the local admission member remains acquired.
            auto raw{source_exn::value::make_owned(tag.exception_identity,::std::move(private_fields),trace)};
            auto record{source_exn::value::record_from_native(raw)};
            if(!record) { ::std::terminate(); }
            return ::std::optional<produced_value>{produced_value{
                producer,::std::move(record),::std::move(admission),true}};
        }

        [[nodiscard]] static ::std::size_t reclaim_actual_native_leaves(
            owner const& producer, ::uwvm2::utils::thread::deferred_native_owner_queue& actual_queue) noexcept
        {
            if(!canonical(producer) || producer->retired_.load(::std::memory_order_acquire)) { return 0uz; }
            // This real strong pin, plus the retained source/store/module/domain,
            // survives ALL callbacks. No publisher/source destructor can run
            // under admission. Known records contain only private frozen native
            // data, empty payload owners and actual source-owned tag identities.
            auto lifetime{::std::static_pointer_cast<source_exn::external_exception_lifetime const>(producer)};
            return actual_queue.reclaim_qualified_native_leaves(
                [&lifetime](::uwvm2::utils::thread::deferred_native_owner const* node) noexcept
                {
                    return native_leaf_access::matches(node,lifetime)
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
                        || managed_exception_retirement::matches_native_leaf(
                            node,lifetime,native_leaf_access::captured_certificate())
#endif
                        ;
                });
        }
    };
}
#endif

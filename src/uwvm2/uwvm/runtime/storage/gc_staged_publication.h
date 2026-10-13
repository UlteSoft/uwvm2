// Included at gc_object.h tail INSIDE the original storage export namespace.
// Native-only unpublished restoration staging. No DATA/view/decoder/API can
// construct this owner, reserve a shell or publish a store. Its sole issuer is
// the complete-world transaction; the private initializer only builds metadata.
#pragma once
class checkpoint_gc_staging final
{
    friend class ::uwvm2::uwvm::runtime::initializer::restoration_context;
    friend class ::uwvm2::runtime::lib::runtime_checkpoint_world_transaction;
    using source_type = ::uwvm2::uwvm::runtime::full::full_source_instance;
    using store_owner = ::std::shared_ptr<gc_object_store>;
    struct shell
    {
        store_owner store{};
        gc_object_store::object* object{};
        bool filled{};
    };
    ::std::vector<store_owner> stores_{};
    ::std::vector<shell> shells_{};
    bool published_{};
    // Sole-world cold backing budget. This private callback records allocation
    // payloads only; it grants no source, graph, epoch or publication permission.
    // The nonmoving owning transaction outlives this stage and all callbacks.
    void* native_budget_owner_{};
    bool (*native_budget_charge_)(void*, ::std::size_t, ::std::size_t) noexcept{};
    [[nodiscard]] bool charge_private_backing(::std::size_t count, ::std::size_t width) noexcept
    {
        return native_budget_owner_ != nullptr && native_budget_charge_ != nullptr &&
            native_budget_charge_(native_budget_owner_, count, width);
    }
    struct exception
    {
        store_owner issuer{};
        ::std::unique_ptr<gc_object_store::exn_token_entry> entry{};
    };
    struct exception_root
    {
        store_owner recipient{};
        gc_object_store::exn_token_entry* entry{};
        ::std::unique_ptr<gc_object_store::exn_root_node> root{};
    };
    struct bridge
    {
        store_owner issuer{};
        ::std::unique_ptr<gc_object_store::extern_bridge> entry{};
    };
    ::std::vector<exception> exceptions_{};
    ::std::vector<exception_root> exception_roots_{};
    ::std::vector<bridge> bridges_{};
    [[nodiscard]] exception const* find_pending_exception(gc_reference ref) const noexcept
    {
        if(ref.kind!=::uwvm2::object::global::wasm_ref_kind::wasm_exn || ref.storage.ptr==nullptr) { return nullptr; }
        auto const token{reinterpret_cast<::std::uintptr_t>(ref.storage.ptr)};
        for(auto const& pending:exceptions_)
        { if(pending.entry && pending.entry->token==token) { return ::std::addressof(pending); } }
        return nullptr;
    }
    [[nodiscard]] bridge const* find_pending_bridge(gc_reference ref) const noexcept
    {
        if(ref.kind!=::uwvm2::object::global::wasm_ref_kind::wasm_extern || ref.storage.ptr==nullptr) { return nullptr; }
        for(auto const& pending:bridges_)
        { if(pending.entry && pending.entry->token==ref.storage.ptr) { return ::std::addressof(pending); } }
        return nullptr;
    }
    [[nodiscard]] gc_object_status retain_pending_exception(store_owner const& recipient,
        gc_object_store::exn_token_entry* entry) noexcept
    {
        if(!owns(recipient) || entry==nullptr || recipient->cohort_registered_) { return gc_object_status::invalid_store; }
        for(auto const& existing:exception_roots_)
        { if(existing.recipient.get()==recipient.get() && existing.entry==entry) { return gc_object_status::ok; } }
        if(exception_roots_.size()==SIZE_MAX) { return gc_object_status::size_overflow; }
        // Allocate only new PRIVATE recipient bookkeeping. No global registry
        // insertion/root count occurs until the actual joint commit boundary.
        if(!recipient->exn_root_buckets_)
        {
            if(!charge_private_backing(1u, sizeof(gc_object_store::exn_root_bucket_array)))
            { return gc_object_status::size_overflow; }
            recipient->exn_root_buckets_.reset(new(::std::nothrow) gc_object_store::exn_root_bucket_array{});
            if(!recipient->exn_root_buckets_) { return gc_object_status::out_of_memory; }
        }
        if(!charge_private_backing(1u, sizeof(gc_object_store::exn_root_node)) ||
           !charge_private_backing(1u, sizeof(exception_root)))
        { return gc_object_status::size_overflow; }
        ::std::unique_ptr<gc_object_store::exn_root_node> root{new(::std::nothrow) gc_object_store::exn_root_node{}};
        if(!root) { return gc_object_status::out_of_memory; }
#if defined(UWVM_CPP_EXCEPTIONS)
        try { exception_roots_.push_back(exception_root{recipient,entry,::std::move(root)}); }
        catch(...) { return gc_object_status::out_of_memory; }
#else
        exception_roots_.push_back(exception_root{recipient,entry,::std::move(root)});
#endif
        return gc_object_status::ok;
    }
    [[nodiscard]] gc_object_status stage_exception(store_owner const& store,
        ::uwvm2::runtime::exception::value_ref const& actual_new_value,gc_reference& result) noexcept
    {
        // Sole joint-world issuer built this immutable value from fresh real
        // tag owner, relocated typed payload and owned original diagnostics.
        // This helper cannot authenticate a supplied old/native/wire value.
        if(published_ || !owns(store) || store->cohort_registered_ || !actual_new_value)
        { return gc_object_status::invalid_store; }
        if(!charge_private_backing(1u, sizeof(gc_object_store::exn_token_entry)) ||
           !charge_private_backing(1u, sizeof(exception)))
        { return gc_object_status::size_overflow; }
        ::std::unique_ptr<gc_object_store::exn_token_entry> entry{new(::std::nothrow) gc_object_store::exn_token_entry{}};
        if(!entry) { return gc_object_status::out_of_memory; }
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
        auto record{::uwvm2::runtime::exception::external_exception_handle::record_from_privileged_native(actual_new_value)};
        if(!record) { return gc_object_status::invalid_value; }
        entry->value=::std::move(record);
#else
        entry->value=actual_new_value;
#endif
        // [actual stage-owned source issuer][new private immutable value]
        // [safe] weak owner derives from the genuine strong control block.
        entry->owner=store;
        {
            gc_object_store::exn_guard identity_guard{};
            if(gc_object_store::next_exn_id_>gc_object_store::token_payload_mask) { return gc_object_status::size_overflow; }
            // [actual unique integer token counter, bounded before increment]
            // [safe] reserve identity only; no exn bucket/root can discover entry.
            entry->token=gc_object_store::exn_token_prefix|gc_object_store::next_exn_id_++;
        }
        auto* const retained{entry.get()};
#if defined(UWVM_CPP_EXCEPTIONS)
        try { exceptions_.push_back(exception{store,::std::move(entry)}); }
        catch(...) { return gc_object_status::out_of_memory; }
#else
        exceptions_.push_back(exception{store,::std::move(entry)});
#endif
        auto const rooted{retain_pending_exception(store,retained)};
        if(rooted!=gc_object_status::ok) { exceptions_.pop_back(); return rooted; }
        result={};result.kind=::uwvm2::object::global::wasm_ref_kind::wasm_exn;
        result.storage.ptr=reinterpret_cast<void*>(retained->token);
        return gc_object_status::ok;
    }
    [[nodiscard]] gc_object_status stage_external_bridge(store_owner const& store,
        gc_reference actual_staged_inner,gc_reference& result) noexcept
    {
        if(published_ || !owns(store) || store->cohort_registered_) { return gc_object_status::invalid_store; }
        using kind=::uwvm2::object::global::wasm_ref_kind;
        auto const* aggregate{find_pending(actual_staged_inner)};
        if(aggregate==nullptr && actual_staged_inner.kind!=kind::wasm_i31) { return gc_object_status::invalid_reference; }
        for(auto const& existing:bridges_)
        {
            if(existing.entry && gc_object_store::same_bridge_inner(existing.entry->inner,actual_staged_inner))
            {
                result={};result.kind=kind::wasm_extern;result.storage.ptr=existing.entry->token;
                return gc_object_status::ok;
            }
        }
        if(!charge_private_backing(1u, sizeof(gc_object_store::extern_bridge)) ||
           !charge_private_backing(1u, sizeof(bridge)))
        { return gc_object_status::size_overflow; }
        ::std::unique_ptr<gc_object_store::extern_bridge> entry{new(::std::nothrow) gc_object_store::extern_bridge{}};
        if(!entry) { return gc_object_status::out_of_memory; }
        // [actual canonical stage-owned issuer][checked staged inner identity]
        // [safe] owner borrows only the strong stores_ pin, never a wire pointer.
        entry->owner=store.get();entry->inner=actual_staged_inner;
        if(aggregate!=nullptr && aggregate->store.get()!=store.get()) { entry->inner_owner=aggregate->store; }
        {
            gc_object_store::bridge_guard identity_guard{};
            if(gc_object_store::next_bridge_id_>gc_object_store::token_payload_mask) { return gc_object_status::size_overflow; }
            // [new owned bridge][bounded process-unique reserved integer token]
            // [safe] no global token/inner bucket is linked during preparation.
            entry->token=reinterpret_cast<void*>(gc_object_store::token_prefix|gc_object_store::next_bridge_id_++);
        }
        auto const token{entry->token};
#if defined(UWVM_CPP_EXCEPTIONS)
        try { bridges_.push_back(bridge{store,::std::move(entry)}); }
        catch(...) { return gc_object_status::out_of_memory; }
#else
        bridges_.push_back(bridge{store,::std::move(entry)});
#endif
        result={};result.kind=kind::wasm_extern;result.storage.ptr=token;
        return gc_object_status::ok;
    }
    // A global/table/segment held by another module needs the same genuine
    // originating-store lifetime as an aggregate's embedded reference.
    [[nodiscard]] gc_object_status retain_staged_reference(store_owner const& recipient,gc_reference reference) noexcept
    {
        if(published_ || !owns(recipient) || recipient->cohort_registered_) { return gc_object_status::invalid_store; }
        store_owner const* issuer{};
        if(auto const* pending=find_pending(reference)) { issuer=::std::addressof(pending->store); }
        else if(auto const* pending=find_pending_exception(reference))
        {
            auto const retained{retain_pending_exception(recipient,pending->entry.get())};
            if(retained!=gc_object_status::ok) { return retained; }
            issuer=::std::addressof(pending->issuer);
        }
        else if(auto const* pending=find_pending_bridge(reference)) { issuer=::std::addressof(pending->issuer); }
        else
        {
            using kind=::uwvm2::object::global::wasm_ref_kind;
            return reference.kind==kind::wasm_null || reference.kind==kind::wasm_i31 ||
                reference.kind==kind::wasm_func_defined || reference.kind==kind::wasm_func_imported ?
                gc_object_status::ok : gc_object_status::invalid_reference;
        }
        if(issuer->get()==recipient.get()) { return gc_object_status::ok; }
        auto lease{recipient->lease_owner_.lock()};
        if(!lease) { return gc_object_status::invalid_store; }
        return lease->hold(*issuer)?gc_object_status::ok:gc_object_status::out_of_memory;
    }
    // Cold private native payload-root lookup. The real world issuer already
    // relocated and type-checked the carrier; this helper independently proves
    // pending membership and source-recipient lifetime without published indices.
    [[nodiscard]] bool pending_reference_root(store_owner const& recipient,gc_reference reference,
        ::uwvm2::runtime::exception::instance_root& result) noexcept
    {
        result={};
        if(published_ || !owns(recipient) || recipient->cohort_registered_) { return false; }
        using kind=::uwvm2::object::global::wasm_ref_kind;
        if(reference.kind==kind::wasm_null) { return reference.storage.ptr==nullptr; }
        if(reference.kind==kind::wasm_i31) { return true; }
        if(reference.kind==kind::wasm_func_defined || reference.kind==kind::wasm_func_imported)
        {
            // Actual source-bound callback proves exact record membership. An
            // empty payload root is the ordinary function-carrier convention;
            // the genuine complete source/world owner supplies its lifetime.
            return reference.storage.ptr!=nullptr && recipient->function_type_match_!=nullptr &&
                recipient->function_type_match_(reference,recipient.get(),UINT_LEAST32_MAX);
        }
        if(auto const* aggregate=find_pending(reference))
        {
            if(retain_staged_reference(recipient,reference)!=gc_object_status::ok) { return false; }
            // Local roots are empty to avoid store->exn->same-store ownership
            // cycles; actual private exn/root registrations trace their payload.
            if(aggregate->store.get()!=recipient.get()) { result=::std::static_pointer_cast<void const>(aggregate->store); }
            return true;
        }
        if(auto const* pending=find_pending_exception(reference))
        {
            if(retain_staged_reference(recipient,reference)!=gc_object_status::ok) { return false; }
            if(pending->issuer.get()!=recipient.get())
            {
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
                auto value{pending->entry->value.native_untracked_owner()};
#else
                auto value{pending->entry->value};
#endif
                if(!value) { return false; }
                result=::std::static_pointer_cast<void const>(::std::move(value));
            }
            return true;
        }
        if(auto const* pending=find_pending_bridge(reference))
        {
            if(retain_staged_reference(recipient,reference)!=gc_object_status::ok) { return false; }
            if(pending->issuer.get()!=recipient.get()) { result=::std::static_pointer_cast<void const>(pending->issuer); }
            return true;
        }
        return false; // No old token/global index/opaque host pointer shortcut.
    }
    // Conservative actual allocator upper bound: ordinary shell storage, or
    // one complete numeric slab chunk if this object can open a new class. This
    // may overcharge reused chunks but cannot undercount private native backing.
    [[nodiscard]] bool shell_native_upper_bound(store_owner const& store,::std::uint_least32_t type,
        gc_type::composite_kind kind,::std::size_t length,::std::size_t& bytes) const noexcept
    {
        bytes=0u;
        if(published_ || !owns(store) || store->cohort_registered_) { return false; }
        auto const* layout{store->checked_type(type,kind)};
        if(layout==nullptr || (kind!=gc_type::composite_kind::struct_ && kind!=gc_type::composite_kind::array) ||
           (kind==gc_type::composite_kind::struct_ && layout->field_count!=length)) { return false; }
        constexpr auto maximum{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
        constexpr auto header{gc_object_store::object_value_offset},width{sizeof(gc_object_value)};
        if(header>maximum || length>(maximum-header)/width) { return false; }
        auto const body{header+length*width}; // bounded quotient BEFORE sum/product.
        if(store->numeric_slab_eligible(type,kind,length))
        {
            constexpr auto alignment{gc_object_store::allocation_alignment},prefix{gc_object_store::chunk_slots_offset};
            if(body>maximum-(alignment-1u)) { return false; }
            auto const stride{(body+alignment-1u)/alignment*alignment};
            if(prefix>maximum || stride>(maximum-prefix)/gc_object_store::numeric_slab_slots) { return false; }
            bytes=prefix+stride*gc_object_store::numeric_slab_slots;
        }
        else { bytes=body; } // Packed numeric arrays need no larger backing than16/element.
        return true;
    }
    checkpoint_gc_staging() noexcept = default;
    [[nodiscard]] bool owns(store_owner const& store) const noexcept
    {
        if(!store) { return false; }
        for(auto const& own : stores_)
        {
            if(own.get()==store.get() && !own.owner_before(store) && !store.owner_before(own))
            { return true; }
        }
        return false;
    }
    // Source is deliberately dependent until the actual source definition is
    // complete. No storage->initializer/source import cycle or pointer map.
    template<typename Source>
    [[nodiscard]] store_owner create_store(gc_type::recursive_type_section const& section,
        ::std::shared_ptr<gc_lease_owner> const& lease_owner,
        gc_function_type_match_callback callback, ::std::shared_ptr<Source const> const& source) noexcept
    {
        static_assert(::std::is_same_v<Source,source_type>);
        if(published_ || !lease_owner || !Source::has_canonical_owner(source)) { return {}; }
#if defined(UWVM_CPP_EXCEPTIONS)
        try
#endif
        {
            if(!charge_private_backing(1u, sizeof(gc_object_store)) ||
               !charge_private_backing(1u, sizeof(store_owner))) { return {}; }
            auto* const fresh{new(::std::nothrow) gc_object_store(section,lease_owner,callback,gc_object_store::checkpoint_staged_tag{})};
            if(fresh==nullptr) { return {}; }
            // shared_ptr constructor deletes fresh if its control allocation
            // throws; its actual canonical owner is never minted from an alias.
            auto owned{store_owner{fresh}};
            if(!owned->valid_ || owned->cohort_registered_) { return {}; }
            // [canonical strong source control block][new private unregistered store]
            // [safe] no source native address is cast or accepted from file DATA.
            // ^^ weak binding cannot keep a destroyed initializer/context alive.
            owned->checkpoint_source_binding_=source;
            stores_.push_back(owned);
            return owned;
        }
#if defined(UWVM_CPP_EXCEPTIONS)
        catch(...) { return {}; }
#endif
    }
    [[nodiscard]] gc_object_status reserve_shell(store_owner const& store,
        ::std::uint_least32_t type, gc_type::composite_kind kind, ::std::size_t length,
        ::std::size_t& ordinal, gc_reference& reference) noexcept
    {
        if(published_ || !owns(store) || store->cohort_registered_)
        { return gc_object_status::invalid_store; }
        auto const* layout{store->checked_type(type,kind)};
        if(layout==nullptr || (kind!=gc_type::composite_kind::struct_ && kind!=gc_type::composite_kind::array) ||
           (kind==gc_type::composite_kind::struct_ && length!=layout->field_count))
        { return gc_object_status::invalid_type; }
        if(!charge_private_backing(1u, sizeof(shell))) { return gc_object_status::size_overflow; }
        gc_object_store::object* fresh{};
        auto const allocated{store->allocate(type,kind,length,fresh)};
        if(allocated!=gc_object_status::ok) { return allocated; }
        auto const token{gc_object_store::issue_object_token()};
        if(token==nullptr) { gc_object_store::destroy_object(fresh); return gc_object_status::size_overflow; }
        // [actual owned unregistered fresh header][reserved nonrecycling identity]
        // [safe] token is only compared; no bucket/cohort publishes this address.
        // ^^ assign token before any private graph edge can refer to this shell.
        fresh->token=token;
#if defined(UWVM_CPP_EXCEPTIONS)
        try { shells_.push_back(shell{store,fresh,false}); }
        catch(...) { gc_object_store::destroy_object(fresh); return gc_object_status::out_of_memory; }
#else
        shells_.push_back(shell{store,fresh,false});
#endif
        ordinal=shells_.size()-1u; // successful append proved nonempty BEFORE subtraction.
        reference={}; reference.storage.ptr=token;
        reference.kind=kind==gc_type::composite_kind::struct_ ?
            ::uwvm2::object::global::wasm_ref_kind::wasm_struct : ::uwvm2::object::global::wasm_ref_kind::wasm_array;
        return gc_object_status::ok;
    }
    [[nodiscard]] shell const* find_pending(gc_reference reference) const noexcept
    {
        using kind=::uwvm2::object::global::wasm_ref_kind;
        if(reference.storage.ptr==nullptr || (reference.kind!=kind::wasm_struct && reference.kind!=kind::wasm_array)) { return nullptr; }
        for(auto const& pending : shells_)
        {
            // [strong pinned private shell list][fresh still owned by this stage]
            // [safe] reference token is COMPARED BEFORE any supplied-payload read.
            if(pending.object!=nullptr && pending.object->token==reference.storage.ptr &&
               (reference.kind==kind::wasm_struct ? pending.object->kind==gc_type::composite_kind::struct_ :
                                                   pending.object->kind==gc_type::composite_kind::array))
            { return ::std::addressof(pending); }
        }
        return nullptr;
    }
    [[nodiscard]] bool pending_value_matches(gc_object_value value,gc_type::storage_type storage,
        store_owner const& expected_owner) const noexcept
    {
        if(storage.packed!=gc_type::packed_kind::none || storage.value.kind!=gc_type::value_kind::reference) { return true; }
        using kind=::uwvm2::object::global::wasm_ref_kind;
        auto const ref{value.as<gc_reference>()};
        if(ref.kind==kind::wasm_null) { return ref.storage.ptr==nullptr && storage.value.nullable; }
        if(ref.kind==kind::wasm_i31) { return expected_owner->reference_matches(ref,storage.value,expected_owner.get()); }
        if(auto const* aggregate=find_pending(ref))
        {
            gc_type::core_value_type actual{gc_type::value_kind::reference};
            actual.nullable=false; actual.heap.code=aggregate->object->type_index;
            return gc_object_store::canonical_value_type_matches(aggregate->store.get(),actual,expected_owner.get(),storage.value);
        }
        if(ref.kind==kind::wasm_func_defined || ref.kind==kind::wasm_func_imported)
        {
            if(ref.storage.ptr==nullptr || expected_owner->function_type_match_==nullptr) { return false; }
            // Source-bound staged callback authenticates EVERY native function
            // record, even abstract func: sentinel means owned membership only.
            // Ordinary callback/abstract-function execution paths are unchanged.
            auto index{UINT_LEAST32_MAX};
            if(storage.value.heap.is_defined())
            {
                if(storage.value.heap.code<0 || static_cast<::std::uint_least64_t>(storage.value.heap.code)>UINT_LEAST32_MAX) { return false; }
                index=static_cast<::std::uint_least32_t>(storage.value.heap.code);
                if(expected_owner->checked_type(index,gc_type::composite_kind::function)==nullptr) { return false; }
            }
            else if(storage.value.heap.code!=static_cast<::std::int_least64_t>(gc_type::abstract_heap_type::func)) { return false; }
            return expected_owner->function_type_match_(ref,expected_owner.get(),index);
        }
        if(find_pending_exception(ref)!=nullptr)
        { return storage.value.heap.code==static_cast<::std::int_least64_t>(gc_type::abstract_heap_type::exn); }
        if(find_pending_bridge(ref)!=nullptr)
        { return storage.value.heap.code==static_cast<::std::int_least64_t>(gc_type::abstract_heap_type::extern_); }
        // Opaque host references need an actual versioned adapter/new resource
        // owner; a published old token or wire address is never a shortcut.
        return false;
    }
    [[nodiscard]] gc_object_status fill_shell(::std::size_t ordinal,::std::span<gc_object_value const> values) noexcept
    {
        if(published_ || ordinal>=shells_.size() ||
           values.size()>static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(gc_object_value) ||
           (values.size()!=0u && values.data()==nullptr))
        { return gc_object_status::invalid_value; }
        // [shells_.data(),shells_.data()+size) owns ordinal<size before selection.
        // [safe] no published object is selected or overwritten by this method.
        auto& selected{shells_[ordinal]}; auto* object{selected.object};
        if(object==nullptr || selected.filled || !owns(selected.store) || selected.store->cohort_registered_ || values.size()!=object->length)
        { return gc_object_status::invalid_value; }
        auto const* layout{selected.store->checked_type(object->type_index,object->kind)};
        if(layout==nullptr || (object->kind==gc_type::composite_kind::struct_ ? layout->field_count!=values.size() : layout->field_count!=1u))
        { return gc_object_status::invalid_type; }
        for(::std::size_t n{};n!=values.size();++n)
        {
            // [checked complete input values][immutable field layout0..count]
            // [safe] struct n<count; array chooses proven solefield0.
            auto const storage{layout->fields[object->kind==gc_type::composite_kind::struct_?n:0u].storage};
            if(!pending_value_matches(values[n],storage,selected.store)) { return gc_object_status::invalid_value; }
            if(storage.packed==gc_type::packed_kind::none && storage.value.kind==gc_type::value_kind::reference)
            {
                if(auto const* target=find_pending(values[n].as<gc_reference>());target!=nullptr && target->store.get()!=selected.store.get())
                { if(!object->value_leases.hold(target->store)) { return gc_object_status::out_of_memory; } }
                else if(auto const* target=find_pending_exception(values[n].as<gc_reference>());target!=nullptr)
                {
                    auto const rooted{retain_pending_exception(selected.store,target->entry.get())};
                    if(rooted!=gc_object_status::ok) { return rooted; }
                    if(target->issuer.get()!=selected.store.get() && !object->value_leases.hold(target->issuer)) { return gc_object_status::out_of_memory; }
                }
                else if(auto const* target=find_pending_bridge(values[n].as<gc_reference>());target!=nullptr && target->issuer.get()!=selected.store.get())
                { if(!object->value_leases.hold(target->issuer)) { return gc_object_status::out_of_memory; } }
            }
        }
        // No fallible step follows the full typing/foreign-lease preflight.
        for(::std::size_t n{};n!=values.size();++n)
        {
            auto const storage{layout->fields[object->kind==gc_type::composite_kind::struct_?n:0u].storage};
            auto const packed{gc_object_store::pack(values[n],storage.packed)};
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            if(object->kind==gc_type::composite_kind::array) { gc_object_store::store_array_value(*object,n,packed,storage); }
            else
#endif
            { object->values[n]=packed; }
        }
        selected.filled=true;
        return gc_object_status::ok;
    }
    [[nodiscard]] bool publication_preflight() const noexcept
    {
        if(published_ || exception_roots_.size()>static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(exception_root)) { return false; }
        for(auto const& store:stores_) { if(!store || !store->valid_ || store->cohort_registered_) { return false; } }
        for(auto const& pending:shells_)
        { if(!pending.filled || pending.object==nullptr || pending.object->token==nullptr || !owns(pending.store)) { return false; } }
        for(auto const& pending:exceptions_)
        { if(!pending.entry || !pending.entry->value || pending.entry->token==0u || !owns(pending.issuer)) { return false; } }
        for(auto const& pending:exception_roots_)
        { if(!pending.root || !pending.recipient->exn_root_buckets_ || pending.entry==nullptr || !owns(pending.recipient)) { return false; } }
        for(auto const& pending:bridges_)
        { if(!pending.entry || pending.entry->token==nullptr || !owns(pending.issuer)) { return false; } }
        return true;
    }
    // Sole real joint-world issuer calls AFTER actual old-world drain, source/
    // engine/resource preparation and its non-fallible publication boundary.
    // This private helper does not acquire/mint those permissions itself.
    void publish_preflighted() noexcept
    {
        if(!publication_preflight()) { ::fast_io::fast_terminate(); }
        for(auto const& store:stores_) { store->register_cohort_member(); }
        {
            gc_object_store::exn_guard registry_guard{};
            for(auto& pending:exceptions_)
            {
                auto& bucket{gc_object_store::exn_buckets_[gc_object_store::exn_bucket(pending.entry->token)]};
                // [new private fully typed immutable exception][held real registry]
                // [safe] link before releasing ownership; no wire pointer is read.
                pending.entry->next=bucket;bucket=pending.entry.release();
            }
            for(auto& pending:exception_roots_)
            {
                auto& recipient{*pending.recipient};gc_object_store::exn_roots_guard roots_guard{recipient};
                auto const bucket{gc_object_store::exn_bucket(pending.entry->token)};
                pending.root->entry=pending.entry;
                // [actual preallocated module root buckets][live real entry]
                // [safe] every native link remains owned under the two guards.
                pending.root->bucket_next=(*recipient.exn_root_buckets_)[bucket];
                (*recipient.exn_root_buckets_)[bucket]=pending.root.get();
                pending.root->next=recipient.exn_roots_;recipient.exn_roots_=pending.root.release();
                ++pending.entry->root_count; // count bounded by preflighted private root roster.
            }
        }
        {
            gc_object_store::bridge_guard registry_guard{};
            for(auto& pending:bridges_)
            {
                auto& token_bucket{gc_object_store::bridge_token_buckets_[gc_object_store::bridge_token_bucket(pending.entry->token)]};
                auto& inner_bucket{gc_object_store::bridge_inner_buckets_[gc_object_store::bridge_inner_bucket(pending.entry->inner)]};
                // [complete new private bridge/strong inner lease][held two-index lock]
                // [safe] both links publish before any allowed reader can run.
                pending.entry->token_next=token_bucket;pending.entry->inner_next=inner_bucket;
                auto* published{pending.entry.release()};token_bucket=published;inner_bucket=published;
            }
        }
        for(auto& pending:shells_)
        {
            gc_reference ignored{};
            if(pending.store->publish_reserved_object(pending.object,pending.object->token,ignored)!=gc_object_status::ok)
            { ::fast_io::fast_terminate(); }
            // [real store-owned three-index publication now owns complete object]
            // [safe] private stage gives up its destructor responsibility exactly once.
            // ^^ clear pointer only after the ordinary publication completed.
            pending.object=nullptr;
        }
        published_=true;
    }
public:
    checkpoint_gc_staging(checkpoint_gc_staging const&)=delete;
    checkpoint_gc_staging& operator=(checkpoint_gc_staging const&)=delete;
    checkpoint_gc_staging(checkpoint_gc_staging&&)=delete;
    checkpoint_gc_staging& operator=(checkpoint_gc_staging&&)=delete;
    ~checkpoint_gc_staging()
    {
        // All complete store strong pins remain live until EVERY private shell
        // and embedded foreign lease is destroyed. Cycles cannot drop a later
        // arena before its still-owned object/header allocation is released.
        for(auto& pending:shells_)
        {
            if(pending.object!=nullptr)
            {
                gc_object_store::destroy_object(pending.object);
                // [private fresh object lifetime ended, stores_ pins still complete]
                // [safe] null prevents any later duplicate rollback deletion.
                pending.object=nullptr;
            }
        }
        // Original source/value roots and bridge inner-owner pins are released
        // only after all private aggregate allocations/embedded leases above.
        // stores_ and each pending record's strong store owners are still live.
        for(auto& pending:bridges_) { pending.entry.reset(); }
        for(auto& pending:exception_roots_) { pending.root.reset(); }
        for(auto& pending:exceptions_) { pending.entry.reset(); }
    }
};

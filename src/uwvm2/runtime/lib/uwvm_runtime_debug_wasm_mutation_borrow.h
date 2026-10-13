// Include INSIDE constructor-private runtime_checkpoint_gc_state_borrow.
// This is a separate manager-only cold mutator, never a permission on copied
// Wasm DATA. The genuine cohort/hostclose/N/publication proof remains live.
#pragma once
        [[nodiscard]] static bool mutation_original(work& state,capture const& actual,
            selection const& requested,native_value& carrier)
        {
            namespace ws=::uwvm2::uwvm::debugger::wasm_state;
            copied_value copied{};
            if(ws::frame_selection(requested.selected))
            {
                if(requested.frame>=actual.frames_.size()) { state.copy_error=copy_status::out_of_range;return false; }
                // [real exact captured frame array0..N][frame<N] end
                // [safe] ordinal subtraction BEFORE indexed frame access.
                auto const ordinal{actual.frames_.size()-1u-static_cast<::std::size_t>(requested.frame)};
                auto const& saved{actual.frames_[ordinal]};auto const& frame{saved.logical};
                if(saved.activation.module>=state.modules.size() || capture::check_owned_values(frame,
                    capture::value_use::live_observation)!=checkpoint_thread_capture_status::captured)
                { state.copy_error=copy_status::unavailable_typed_site;return false; }
                auto const owner{static_cast<::std::size_t>(saved.activation.module)};
                // check_owned_values proves 0<site<=sealed actual site count.
                auto const& site{frame.plan->get().sites[static_cast<::std::size_t>(frame.site-1u)]};
                if(site.local_count>frame.values.size() || site.operand_count>frame.values.size()-site.local_count ||
                   site.saved_parameter_count!=frame.values.size()-site.local_count-site.operand_count) { return false; }
                auto const start{requested.selected==ws::selection::locals ? 0u : requested.selected==ws::selection::saved_parameters ?
                    static_cast<::std::size_t>(site.local_count+site.operand_count) : static_cast<::std::size_t>(site.local_count)};
                auto const count{requested.selected==ws::selection::locals ? static_cast<::std::size_t>(site.local_count) :
                    requested.selected==ws::selection::saved_parameters ? site.saved_parameter_count : site.operand_count};
                if(requested.first>=count) { state.copy_error=copy_status::out_of_range;return false; }
                // [packet partition start..start+count<=values.size][first<count]
                // [safe] exact partition proof BEFORE adding original index.
                auto const& value{frame.values[start+static_cast<::std::size_t>(requested.first)]};
                if(!copy_native(state,owner,value.declaration.type,value.bits.data(),value.declaration.initialized,copied,
                    ::std::addressof(carrier)) || !carrier.initialized) { return false; }
            }
            else
            {
                if(requested.module>=state.modules.size() || requested.first>(::std::numeric_limits<::std::size_t>::max)())
                { state.copy_error=copy_status::out_of_range;return false; }
                auto const owner{static_cast<::std::size_t>(requested.module)};
                if(requested.selected==ws::selection::globals)
                {
                    if(!copy_global(state,owner,static_cast<::std::size_t>(requested.first),copied,::std::addressof(carrier))) { return false; }
                }
                else
                {
                    if(requested.index>(::std::numeric_limits<::std::size_t>::max)() ||
                       !copy_table(state,owner,static_cast<::std::size_t>(requested.index),requested.first,1u,::std::addressof(carrier)) ||
                       state.output.rows.size()!=1u) { return false; }
                    copied=state.output.rows.front().data;state.output.rows.clear();
                }
            }
            auto const depth_count{requested.compressed_path() ? requested.long_path.size() : requested.path_size};
            for(::std::size_t depth{};depth!=depth_count;++depth)
            {
                if(!copied.available || copied.type.kind!=ws::value_kind::reference || copied.ref.object==0u ||
                   copied.ref.object>state.pending.size()) { return false; }
                // [query-local freshly authenticated scratch ID1..pendingN]
                // [safe] nonzero/current extent BEFORE id-1 or owner access.
                auto const index{static_cast<::std::size_t>(copied.ref.object-1u)};
                ws::row member{};auto const edge{requested.compressed_path() ? requested.long_path[depth] : requested.path[depth]};
                if(!copy_member(state,index,edge,member,::std::addressof(carrier))) { return false; }
                copied=::std::move(member.data);
                // Retain one native current node ONLY under this lexical N.
                // Original root/path is rematerialized afresh on every command.
                if(copied.ref.object!=0u)
                {
                    if(copied.ref.object>state.pending.size() || state.pending.size()!=state.output.objects.size()) { return false; }
                    auto const next{static_cast<::std::size_t>(copied.ref.object-1u)};
                    auto native{::std::move(state.pending[next])};auto object{::std::move(state.output.objects[next])};
                    state.pending.clear();state.output.objects.clear();object.identifier=1u;
                    state.pending.push_back(::std::move(native));state.output.objects.push_back(::std::move(object));copied.ref.object=1u;
                }
                else { state.pending.clear();state.output.objects.clear(); }
            }
            return carrier.initialized && copied.available;
        }
        [[nodiscard]] static bool mutation_retain(work& state,::std::size_t destination,core_type type,reference ref, ::uwvm2::uwvm::debugger::wasm_mutation::refusal& reason)
        {
            namespace wm=::uwvm2::uwvm::debugger::wasm_mutation;reason=wm::refusal::retention_failed;
            if(destination>=state.modules.size()) { return false; }
            auto const& target{state.modules[destination]};
            auto const roots{target.store->lease_owner_.lock()};
            if(!roots || !same_owner(roots,target.module->gc_lease_roots)) { return false; }
            // Pure abstract/type tests BEFORE any root or recipient bookkeeping.
            // This authenticates real function records, actual tag/issuer and
            // aggregate local tokens+canonical recursive types; no foreign
            // compact reader or shared-N reentry is used for aggregates.
            auto checked{declared_value(type)};
            if(!append_reference(state,destination,type,ref,checked)) { reason=wm::refusal::type_mismatch;return false; }
            if(ref.kind==reference_kind::wasm_struct || ref.kind==reference_kind::wasm_array)
            {
                ::std::size_t owner{},length{};::std::uint_least32_t index{};
                if(!aggregate_owner(state,ref,owner,index,length) || owner>=state.modules.size()) { return false; }
                // Real local registry membership and actual strong source owner
                // were established before hold. hold uses its own short lease
                // lock+nothrow allocation; it NEVER reenters N/cohort/admission.
                if(owner!=destination && !roots->hold(state.modules[owner].store)) { return false; }
                return true;
            }
            if(ref.kind==reference_kind::wasm_extern && store_type::bridge_token_shape(ref.storage.ptr))
            {
                ::std::shared_ptr<store_type const> issuer{};
                {
                    store_type::bridge_guard lock{};
                    auto const* bridge{store_type::find_bridge_token_locked(ref.storage.ptr)};
                    if(bridge==nullptr) { return false; }
                    bool found{};
                    for(auto const& pin:state.modules)
                    {
                        // Compare native owner ONLY to real strong pins before
                        // invoking weak_from_this on a registry-owned pointee.
                        if(bridge->owner!=pin.store.get()) { continue; }
                        issuer=pin.store->weak_from_this().lock();
                        found=same_owner(issuer,pin.store);break;
                    }
                    if(!found) { return false; }
                }
                // Release bridge registry lock BEFORE ordinary precisely-proven
                // extern retention (bridge registry -> destination lease lock).
                return target.store->retain_gc_reference(ref)==::uwvm2::uwvm::runtime::storage::gc_object_status::ok;
            }
            if(ref.kind==reference_kind::wasm_exn)
            {
                ::std::shared_ptr<store_type const> issuer{};
                {
                    store_type::exn_guard lock{};
                    auto const* entry{store_type::find_exn_locked(ref.storage.ptr)};
                    if(entry==nullptr) { return false; }issuer=entry->owner.lock();
                    bool found{};for(auto const& pin:state.modules) { if(same_owner(issuer,pin.store)) { found=true;break; } }
                    if(!found) { return false; }
                }
                // append_reference already checked actual exception tag identity
                // and payload origin. Retain outside the previous exn lock;
                // existing exn->recipient-root->lease lock path never enters N.
                return target.store->retain_exn_reference(ref)==::uwvm2::uwvm::runtime::storage::gc_object_status::ok;
            }
            // Null/i31/functions need no heap lifetime extension. Opaque host
            // extern values arrive ONLY from authentic current typed roots:
            // this reassigns the carrier without reading it or granting host
            // rights/state restoration. No literal host address exists.
            return ref.kind==reference_kind::wasm_null || ref.kind==reference_kind::wasm_i31 ||
                ref.kind==reference_kind::wasm_func_imported || ref.kind==reference_kind::wasm_func_defined || ref.kind==reference_kind::wasm_extern;
        }
        // Shared scalar/reference source materialization for globals, tables
        // and GC members. Called ONLY within the actual manager-issued borrow.
        // It returns native carriers lexically under N, never in debugger DATA.
        [[nodiscard]] static bool mutation_source(work& state,capture const& actual,
            ::uwvm2::uwvm::debugger::wasm_mutation::request const& requested,
            ::std::size_t owner,core_type declared,native_value& value,reference& ref,
            ::uwvm2::uwvm::debugger::wasm_mutation::result& result)
        {
            namespace wm=::uwvm2::uwvm::debugger::wasm_mutation;
            namespace ws=::uwvm2::uwvm::debugger::wasm_state;
                state.pending.clear();state.output.objects.clear();state.pending.reserve(ws::maximum_objects);state.output.objects.reserve(ws::maximum_objects);
                value.owner=owner;value.type=declared;value.initialized=true;
                if(requested.source==wm::source_kind::original_root)
                {
                    value.initialized=false; // authentic source must set this inside copy_native.
                    if(!mutation_original(state,actual,requested.original,value))
                    { result.status=state.copy_error;result.reason=wm::refusal::unavailable_source;return false; }
                    if(value.type.kind==core_kind::reference) { ::std::memcpy(::std::addressof(ref),value.carrier.bits.data(),sizeof(ref)); }
                    else if(value.type.kind!=declared.kind) { result.status=copy_status::available;result.reason=wm::refusal::type_mismatch;return false; }
                }
                else if(requested.source==wm::source_kind::numeric_bits)
                {
                    auto const copied{declared_value(declared)};
                    if(!copied.type.known || copied.type.kind!=requested.numeric_kind)
                    { result.status=copy_status::available;result.reason=wm::refusal::type_mismatch;return false; }
                    if(declared.kind==core_kind::v128) { value.carrier.bits=requested.bits; }
                    else
                    {
                        // Wire numeric bits are LE; native carrier bytes follow
                        // native integer representation, without evaluating FP.
                        auto const* first{reinterpret_cast<unsigned char const*>(requested.bits.data())};
                        auto const width{declared.kind==core_kind::i32 || declared.kind==core_kind::f32 ? 4u:8u};
                        // [owned16 byte input][first..first+width<=16] end
                        // [safe] fixed width BEFORE end pointer formation.
                        auto const* end{first+width}; // full width<=16 proof ABOVE, before this advance.
                        if(width==4u)
                        { ::std::uint32_t bits{};auto p=::fast_io::parse_by_scan(first,end,::fast_io::mnp::le_get<32u>(bits));
                          if(p.code!=::fast_io::parse_code::ok || p.iter!=end) { result.status=copy_status::invalid_data;return false; }
                          ::std::memcpy(value.carrier.bits.data(),::std::addressof(bits),4u); }
                        else
                        { ::std::uint64_t bits{};auto p=::fast_io::parse_by_scan(first,end,::fast_io::mnp::le_get<64u>(bits));
                          if(p.code!=::fast_io::parse_code::ok || p.iter!=end) { result.status=copy_status::invalid_data;return false; }
                          ::std::memcpy(value.carrier.bits.data(),::std::addressof(bits),8u); }
                    }
                }
                else
                {
                    if(declared.kind!=core_kind::reference) { result.status=copy_status::available;result.reason=wm::refusal::type_mismatch;return false; }
                    if(requested.source==wm::source_kind::null) { ref.kind=reference_kind::wasm_null; }
                    else if(requested.source==wm::source_kind::i31)
                    { ref=::uwvm2::object::global::make_wasm_i31_reference(static_cast<::std::int32_t>(requested.i31_bits)); }
                    else
                    {
                        if(requested.function_module>=state.modules.size()) { result.status=copy_status::out_of_range;return false; }
                        auto const& source{*state.modules[static_cast<::std::size_t>(requested.function_module)].module};
                        auto const imports{source.imported_function_vec_storage.size()};
                        if(requested.function_index<imports)
                        {
                            ref.kind=reference_kind::wasm_func_imported;
                            // [actual strong-pinned imported records0..imports]
                            // [safe] index<imports<=SIZE_MAX BEFORE narrowing,
                            // taking this native member address or installing it.
                            ref.storage.ptr=const_cast<void*>(static_cast<void const*>(::std::addressof(source.imported_function_vec_storage.index_unchecked(static_cast<::std::size_t>(requested.function_index)))));
                        }
                        else
                        {
                            if(requested.function_index-imports>=source.local_defined_function_vec_storage.size()) { result.status=copy_status::out_of_range;return false; }
                            ref.kind=reference_kind::wasm_func_defined;
                            // [actual strong-pinned local records0..locals]
                            // [safe] prior branch proves index>=imports; checked
                            // difference<locals<=SIZE_MAX BEFORE native address.
                            ref.storage.ptr=const_cast<void*>(static_cast<void const*>(::std::addressof(source.local_defined_function_vec_storage.index_unchecked(static_cast<::std::size_t>(requested.function_index-imports)))));
                        }
                    }
                }
            return true;
        }
        // In addition to the recipient module's canonical leases, the actual
        // object retains embedded foreign arenas independently. Exported objects
        // can outlive a receiving module. NONE of these branches enters shared
        // GC admission while this complete exclusive N lease remains live.
        [[nodiscard]] static bool mutation_embedded_retain(work& state,::std::size_t owner,
            store_type::object& destination,core_type declared,reference ref,
            ::uwvm2::uwvm::debugger::wasm_mutation::refusal& reason)
        {
            if(owner>=state.modules.size() || destination.owner!=state.modules[owner].store.get() ||
               !mutation_retain(state,owner,declared,ref,reason)) { return false; }
            ::std::shared_ptr<store_type const> issuer{};
            if(ref.kind==reference_kind::wasm_struct || ref.kind==reference_kind::wasm_array)
            {
                ::std::size_t source{},length{};::std::uint_least32_t type{};
                if(!aggregate_owner(state,ref,source,type,length) || source>=state.modules.size()) { return false; }
                issuer=state.modules[source].store; // Real strong pinned LOCAL owner, not a foreign reader.
            }
            else if(ref.kind==reference_kind::wasm_extern && store_type::bridge_token_shape(ref.storage.ptr))
            {
                store_type::bridge_guard guard{};
                auto const* entry{store_type::find_bridge_token_locked(ref.storage.ptr)};
                if(entry==nullptr) { return false; }
                for(auto const& pin:state.modules)
                {
                    if(entry->owner!=pin.store.get()) { continue; }
                    auto candidate{pin.store->weak_from_this().lock()};
                    if(same_owner(candidate,pin.store)) { issuer=::std::move(candidate); }break;
                }
                if(!issuer) { return false; }
            } // Release bridge lock BEFORE destination's private lease hold.
            else if(ref.kind==reference_kind::wasm_exn)
            {
                store_type::exn_guard guard{};
                auto const* entry{store_type::find_exn_locked(ref.storage.ptr)};
                if(entry==nullptr) { return false; }
                auto candidate{entry->owner.lock()};
                for(auto const& pin:state.modules)
                { if(same_owner(candidate,pin.store)) { issuer=::std::move(candidate);break; } }
                if(!issuer) { return false; }
            } // Release exn lock BEFORE destination's private lease hold.
            else { return true; } // null/i31/real functions/authentic opaque extern carriers.
            if(same_owner(issuer,state.modules[owner].store)) { return true; }
            if(!destination.value_leases.hold(::std::move(issuer)))
            { reason=::uwvm2::uwvm::debugger::wasm_mutation::refusal::retention_failed;return false; }
            return true;
        }
        [[nodiscard]] bool mutate_member(work& state,capture const& actual,
            ::uwvm2::uwvm::debugger::wasm_mutation::request const& requested,
            ::uwvm2::uwvm::debugger::wasm_mutation::result& result) const
        {
            namespace wm=::uwvm2::uwvm::debugger::wasm_mutation;
            native_value target{};target.initialized=false;
            if(!mutation_original(state,actual,requested.target_original,target))
            { result.status=state.copy_error;result.reason=wm::refusal::unavailable_source;return false; }
            if(target.type.kind!=core_kind::reference)
            { result.status=copy_status::available;result.reason=wm::refusal::type_mismatch;return false; }
            reference ref{};::std::memcpy(::std::addressof(ref),target.carrier.bits.data(),sizeof(ref));
            if(ref.kind!=reference_kind::wasm_struct && ref.kind!=reference_kind::wasm_array)
            {
                result.status=copy_status::available;
                result.reason=ref.kind==reference_kind::wasm_exn || ref.kind==reference_kind::wasm_extern ?
                    wm::refusal::immutable_member : wm::refusal::type_mismatch;return false;
            }
            ::std::size_t owner{},length{};::std::uint_least32_t type{};
            if(!aggregate_owner(state,ref,owner,type,length) || owner>=state.modules.size() ||
               requested.element>=length || requested.element>(::std::numeric_limits<::std::size_t>::max)())
            { result.status=copy_status::out_of_range;return false; }
            auto& store{*state.modules[owner].store};
            auto const kind{ref.kind==reference_kind::wasm_struct ? composite_kind::struct_ : composite_kind::array};
            auto const* layout{store.checked_type(type,kind)};
            if(layout==nullptr || layout->fields==nullptr ||
               layout->field_count>static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/
                   sizeof(::uwvm2::parser::wasm::standard::wasm3::type::field_type))
            { result.status=copy_status::invalid_data;return false; }
            auto const element{static_cast<::std::size_t>(requested.element)};
            auto const field_index{kind==composite_kind::struct_ ? element : 0u};
            if(field_index>=layout->field_count) { result.status=copy_status::out_of_range;return false; }
            // [real immutable canonical field allocation0..field_count]
            // [safe] complete field extent/index checked BEFORE dereference.
            auto const field{layout->fields[field_index]};
            if(!field.mutable_)
            { result.status=copy_status::available;result.reason=wm::refusal::immutable_member;return false; }
            // Compact numeric structs are immutable by actual allocator/layout
            // qualification, so they were refused ABOVE. LOCAL token lookup
            // compares registry nodes before reading any native object header.
            auto* object{store.checked_local_object(ref,kind)};
            if(object==nullptr || object->owner!=state.modules[owner].store.get() ||
               object->type_index!=type || object->length!=length || object->kind!=kind)
            { result.status=copy_status::invalid_data;return false; }
            auto declared{field.storage.value};
            if(field.storage.packed!=::uwvm2::parser::wasm::standard::wasm3::type::packed_kind::none)
            { declared.kind=core_kind::i32; }
            native_value value{};reference source{};
            if(!mutation_source(state,actual,requested,owner,declared,value,source,result)) { return false; }
            if(declared.kind==core_kind::reference)
            {
                if(value.type.kind!=core_kind::reference ||
                   !mutation_embedded_retain(state,owner,*object,declared,source,result.reason))
                { result.status=copy_status::available;if(value.type.kind!=core_kind::reference){result.reason=wm::refusal::type_mismatch;}return false; }
                ::std::memcpy(value.carrier.bits.data(),::std::addressof(source),sizeof(source));
            }
            else if(value.type.kind==core_kind::reference)
            { result.status=copy_status::available;result.reason=wm::refusal::type_mismatch;return false; }
            // Actual complete module/source/cohort/N/publication proof remains
            // live. Bookkeeping may have grown on failure, but no field has yet
            // changed. A freshly current check precedes the ONE native commit.
            if(!current_scope()) { result.status=copy_status::stale_stop_or_generation;return false; }
            {
                store_type::object_lock lock{*object};
                auto const packed{store_type::pack(value.carrier,field.storage.packed)};
                // [actual initialized object elements0..length][element<length]
                // [safe] extent/narrowing/layout authenticated ABOVE, before
                // this single indexed write. No ordinary foreign GC setter,
                // generic reference matcher or shared admission is invoked.
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                if(kind==composite_kind::array) { store_type::store_array_value(*object,element,packed,field.storage); }
                else { object->values[element]=packed; }
#else
                object->values[element]=packed;
#endif
            }
            // Existing collector scans the real typed object slot, while its
            // independent value_leases preserves any embedded foreign arena.
            result.status=copy_status::available;result.reason=wm::refusal::none;result.applied=true;
            result.runtime_epoch=epoch_;result.module=state.modules[owner].id;result.index=type;return true;
        }
        // Cold memory edits use the same genuine lexical ALL-cohort/hostclose/N/
        // publication guards as typed slot edits. No ordinary memory instruction
        // or mmap hot access path calls any helper in this section.
        template<bool Imported,typename Record>
        [[nodiscard]] static bool mutation_memory_owner(work const& state,Record const* token,
            ::std::size_t& owner,::std::size_t& index) noexcept
        {
            if(token==nullptr) { return false; }
            auto const supplied{reinterpret_cast<::std::uintptr_t>(token)};
            for(::std::size_t id{};id!=state.modules.size();++id)
            {
                auto const& entries{[&]() -> auto const& {
                    if constexpr(Imported) { return state.modules[id].module->imported_memory_vec_storage; }
                    else { return state.modules[id].module->local_defined_memory_vec_storage; }
                }()};
                auto const count{entries.size()};
                constexpr auto limit{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
                if(count==0u || count>limit/sizeof(Record) || entries.data()==nullptr) { continue; }
                auto const base{reinterpret_cast<::std::uintptr_t>(entries.data())};
                auto const extent{count*sizeof(Record)}; // Division bound BEFORE product.
                if(base>UINTPTR_MAX-extent || supplied<base || supplied-base>=extent || (supplied-base)%sizeof(Record)!=0u) { continue; }
                owner=id;index=static_cast<::std::size_t>((supplied-base)/sizeof(Record));
                // [actual strong-pinned owned vector][index<count] end
                // [safe] token only compared; pointee reads use OWNED index.
                return true;
            }
            return false;
        }
        template<typename Type>
        [[nodiscard]] static bool mutation_same_memory_type(Type const& a,Type const& b) noexcept
        {
            return a.address64==b.address64 && a.shared==b.shared && a.limits.min==b.limits.min &&
                a.limits.present_max==b.limits.present_max && (!a.limits.present_max || a.limits.max==b.limits.max);
        }
        template<::uwvm2::parser::wasm::concepts::wasm_feature... Features>
        [[nodiscard]] static bool mutation_memory_declarations(module_pin const& pin,
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Features...> const& parsed) noexcept
        {
            namespace f=::uwvm2::parser::wasm::standard::wasm1::features;
            namespace op=::uwvm2::parser::wasm::concepts::operation;
            using external=::uwvm2::parser::wasm::standard::wasm1::type::external_types;
            auto const& module{*pin.module};
            auto const& memories{op::get_first_type_in_tuple<f::memory_section_storage_t<Features...>>(parsed.sections).memories};
            auto const& imports{op::get_first_type_in_tuple<f::import_section_storage_t<Features...>>(parsed.sections)};
            if(imports.importdesc.size()<=2u || memories.size()!=module.local_defined_memory_vec_storage.size()) { return false; }
            auto const& group{imports.importdesc.index_unchecked(2u)}; // actual bounded memory import group
            if(group.size()!=module.imported_memory_vec_storage.size()) { return false; }
            for(::std::size_t i{};i!=memories.size();++i)
            {
                // [equal real parser/runtime arrays][i<count] end
                // [safe] compare pointers BEFORE any memory-type dereference.
                if(module.local_defined_memory_vec_storage.index_unchecked(i).memory_type_ptr!=::std::addressof(memories.index_unchecked(i))) { return false; }
            }
            for(::std::size_t i{};i!=group.size();++i)
            {
                auto const* original{census_owned_declaration(imports.imports,group.index_unchecked(i))};
                auto const& actual{module.imported_memory_vec_storage.index_unchecked(i)};
                auto const* active{actual.import_type_ptr==original ? original : census_owned_declaration(module.rewritten_import_vec_storage,actual.import_type_ptr)};
                // Both source/rewrite memberships BEFORE active-union reads.
                if(original==nullptr || active==nullptr || original->imports.type!=external::memory || active->imports.type!=external::memory ||
                   !mutation_same_memory_type(original->imports.storage.memory,active->imports.storage.memory)) { return false; }
            }
            return true;
        }
        [[nodiscard]] static bool mutation_resolve_memory(work const& state,::std::size_t bound,
            ::std::size_t& owner,::std::size_t& local) noexcept
        {
            if(owner>=state.modules.size()) { return false; }
            auto const& module{*state.modules[owner].module};auto const imports{module.imported_memory_vec_storage.size()};
            if(local>=imports) { local-=imports;return local<module.local_defined_memory_vec_storage.size(); }
            auto const* next{::std::addressof(module.imported_memory_vec_storage.index_unchecked(local))};
            // The complete declaration preflight established this exact active
            // owned import pointer before reading its expected address type.
            auto const& expected{next->import_type_ptr->imports.storage.memory};
            bool const address64{expected.address64},shared{expected.shared};
            for(::std::size_t hop{};hop!=bound;++hop)
            {
                ::std::size_t record{};
                if(!mutation_memory_owner<true>(state,next,owner,record)) { return false; }
                // [canonical imported memory vector][record<count] end
                // [safe] true owned membership BEFORE union/link/type read.
                auto const& actual{state.modules[owner].module->imported_memory_vec_storage.index_unchecked(record)};
                using kind=typename ::std::remove_cvref_t<decltype(actual)>::imported_memory_link_kind;
                auto const& declaration{actual.import_type_ptr->imports.storage.memory};
                if(declaration.address64!=address64 || declaration.shared!=shared) { return false; }
                if(actual.link_kind==kind::defined)
                {
                    if(!mutation_memory_owner<false>(state,actual.target.defined_ptr,owner,local)) { return false; }
                    auto const& defined{state.modules[owner].module->local_defined_memory_vec_storage.index_unchecked(local)};
                    return defined.memory_type_ptr->address64==address64 && defined.memory_type_ptr->shared==shared;
                }
                if(actual.link_kind!=kind::imported) { return false; }
                next=actual.target.imported_ptr; // Compare membership again BEFORE reading next pointee.
            }
            return false; // Complete charged bound, including aliases/cycles.
        }
        [[nodiscard]] static bool mutation_memory_closure(work& state,::std::size_t& memory_records) noexcept
        {
            memory_records=0u;
            if(g_import_call_cache.size()!=state.modules.size()) { return false; }
            for(::std::size_t owner{};owner!=state.modules.size();++owner)
            {
                auto const& pin{state.modules[owner]};auto const& module{*pin.module};
                auto const imports{module.imported_memory_vec_storage.size()},locals{module.local_defined_memory_vec_storage.size()};
                if(memory_records>record_limit || imports>record_limit-memory_records) { return false; }
                memory_records+=imports; // preflight bounded sum BEFORE advance.
                if(locals>record_limit-memory_records) { return false; }memory_records+=locals;
                auto const* file{pin.source->actual_validated_file(static_cast<::std::size_t>(pin.id),current_runtime_generation(),pin.module)};
                if(file==nullptr || !file->has_owned_source_image() || file->binfmt_ver!=1u || !declaration_origin(pin) ||
                   !pin.source->actual_no_unadapted_native_memory_provider(static_cast<::std::size_t>(pin.id),current_runtime_generation(),pin.module) ||
                   !mutation_memory_declarations(pin,file->wasm_module_storage.wasm_binfmt_ver1_storage)) { return false; }
                // Closing counted host entry cannot revoke a writable mmap
                // descriptor already handed to an unknown native provider.
                // This genuine whole resolved execution closure admits ONLY
                // actual source-owned defined-Wasm leaves; names/flag3 binding
                // DATA are never treated as an escaped-writer adapter.
                auto const& cache{g_import_call_cache.index_unchecked(owner)};
                if(!checkpoint_effect_actual_full_owner(owner) || cache.size()!=module.imported_function_vec_storage.size()) { return false; }
                for(auto const& target:cache)
                { if(target.origin_module_id!=owner || checkpoint_effect_actual_cached_leaf(target)!=checkpoint_resolved_effect::defined_wasm) { return false; } }
                for(::std::size_t index{};index!=module.imported_global_vec_storage.size();++index)
                { auto actual_owner{owner},actual_index{index};if(!resolve_resource<false>(state,actual_owner,actual_index)) { return false; } }
                for(::std::size_t index{};index!=module.imported_table_vec_storage.size();++index)
                { auto actual_owner{owner},actual_index{index};if(!resolve_resource<true>(state,actual_owner,actual_index)) { return false; } }
            }
            for(::std::size_t owner{};owner!=state.modules.size();++owner)
            {
                auto const& module{*state.modules[owner].module};
                for(::std::size_t index{};index!=module.imported_memory_vec_storage.size();++index)
                { auto actual_owner{owner},actual_index{index};if(!mutation_resolve_memory(state,memory_records,actual_owner,actual_index)) { return false; } }
            }
            return true;
        }
        [[nodiscard]] bool mutation_memory_extent(::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t const& actual,
            ::std::byte* begin,::std::size_t length,::uwvm2::uwvm::debugger::wasm_mutation::request const& requested,
            ::uwvm2::uwvm::debugger::wasm_mutation::result& result) const noexcept
        {
            auto const& declaration{*actual.memory_type_ptr}; // whole closure proved exact parser pointer.
            auto const bounds{::uwvm2::uwvm::runtime::storage::checkpoint_effective_memory_bounds(declaration.address64,actual.effective_limits)};
            if(actual.memory.custom_page_size_log2!=16u || length%65536u!=0u || length/65536u<bounds.minimum || length/65536u>bounds.maximum ||
               length>static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) || (length!=0u && begin==nullptr) ||
               length>UINTPTR_MAX-reinterpret_cast<::std::uintptr_t>(begin) || (declaration.shared && !declaration.limits.present_max))
            { result.status=copy_status::invalid_data;return false; }
            ::std::size_t offset{};
            if(requested.element>(::std::numeric_limits<::std::size_t>::max)() || requested.element>length)
            { result.status=copy_status::out_of_range;return false; }
            offset=static_cast<::std::size_t>(requested.element);
            if(requested.memory_size>length-offset || (!declaration.address64 && (requested.element>UINT32_MAX ||
               requested.memory_size>UINT64_C(4294967296)-requested.element)))
            { result.status=copy_status::out_of_range;return false; }
            if(!current_scope()) { result.status=copy_status::stale_stop_or_generation;return false; }
            // [actual retained committed memory: offset<=length][size<=length-offset] end
            // [safe] actual extent/address width/PTRDIFF/uintptr and payload size
            // fully checked BEFORE begin+offset and the ONE complete byte copy.
            // ALL genuine managed participants are stopped; actual host/N/pub
            // and nonwaiting backend stabilization remain live through commit.
            ::fast_io::freestanding::my_memcpy(begin+offset,requested.memory_bytes.data(),requested.memory_size);
            result.runtime_epoch=epoch_;result.memory_size=requested.memory_size;
            result.address_bytes=declaration.address64 ? 8u:4u;
            result.status=copy_status::available;result.applied=true;return true;
        }
        template<typename Memory>
        [[nodiscard]] bool mutation_memory_backend(::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t const& actual,
            Memory const& memory,::uwvm2::uwvm::debugger::wasm_mutation::request const& requested,
            ::uwvm2::uwvm::debugger::wasm_mutation::result& result) const noexcept
        {
            if constexpr(Memory::can_mmap)
            {
                if(memory.memory_length_p==nullptr) { result.status=copy_status::invalid_data;return false; }
                // Stable mmap base, genuine ALL-stop/closedhost closure excludes
                // payload stores/grow. No ordinary software access guard added.
                return mutation_memory_extent(actual,memory.memory_begin,memory.memory_length_p->load(::std::memory_order_acquire),requested,result);
            }
            else if constexpr(Memory::support_multi_thread)
            {
                if(memory.growing_flag_p==nullptr || memory.active_ops_p==nullptr) { result.status=copy_status::invalid_data;return false; }
                census_allocator_quiescence guard{memory.growing_flag_p};
                // NEVER wait on a backend operation while ONE cohort is held.
                // A busy descriptor must be retried at a fresh management stop.
                if(guard.flag==nullptr || memory.active_ops_p->load(::std::memory_order_acquire)!=0u)
                { result.status=copy_status::unavailable_foreign_host_state;return false; }
                return mutation_memory_extent(actual,memory.memory_begin,memory.memory_length,requested,result);
            }
            else { return mutation_memory_extent(actual,memory.memory_begin,memory.memory_length,requested,result); }
        }
        [[nodiscard]] bool mutate_memory(work& state,::uwvm2::uwvm::debugger::wasm_mutation::request const& requested,
            ::uwvm2::uwvm::debugger::wasm_mutation::result& result) const noexcept
        {
            ::std::size_t memory_records{};
            if(!mutation_memory_closure(state,memory_records)) { result.status=copy_status::unavailable_foreign_host_state;return false; }
            if(requested.module>=state.modules.size() || requested.index>(::std::numeric_limits<::std::size_t>::max)())
            { result.status=copy_status::out_of_range;return false; }
            auto owner{static_cast<::std::size_t>(requested.module)},local{static_cast<::std::size_t>(requested.index)};
            if(!mutation_resolve_memory(state,memory_records,owner,local)) { result.status=copy_status::out_of_range;return false; }
            auto const& module{*state.modules[owner].module};auto const imports{module.imported_memory_vec_storage.size()};
            if(imports>UINT64_MAX || local>UINT64_MAX-static_cast<::std::uint64_t>(imports))
            { result.status=copy_status::out_of_range;return false; }
            result.module=state.modules[owner].id;result.index=static_cast<::std::uint64_t>(imports)+static_cast<::std::uint64_t>(local);
            auto const& actual{module.local_defined_memory_vec_storage.index_unchecked(local)}; // owned local bound above.
            return mutation_memory_backend(actual,actual.memory,requested,result);
        }
        [[nodiscard]] ::uwvm2::uwvm::debugger::wasm_mutation::result mutate_selected(
            ::uwvm2::uwvm::debugger::wasm_mutation::request const& requested) const noexcept
        {
            namespace wm=::uwvm2::uwvm::debugger::wasm_mutation;
            namespace ws=::uwvm2::uwvm::debugger::wasm_state;
            wm::result result{};result.target=requested.target;result.module=requested.module;result.index=requested.index;result.element=requested.element;
            if(!wm::valid(requested) || requested.source==wm::source_kind::original_path || requested.target==wm::destination::member_path) { result.status=copy_status::invalid_selection;return result; }
            if(!current_scope()) { result.status=copy_status::stale_stop_or_generation;return result; }
            try
            {
                work state{}; // strong native source/store pins destruct AFTER cohort unlock.
                store_type::cohort_guard stores{};
                if(!complete_modules(state,profile_,epoch_)) { result.status=state.copy_error;return result; }
                capture const* actual{};
                for(auto const& owner:captures_) { if(owner->participant_==requested.participant) { actual=owner.get();break; } }
                if(actual==nullptr) { result.status=copy_status::missing_participant;return result; }
                if(requested.target==wm::destination::memory)
                {
                    ::std::lock_guard table_lock{g_runtime.debug_table_mutex};
                    static_cast<void>(mutate_memory(state,requested,result));return result;
                }
                if(requested.target==wm::destination::member)
                {
                    // Resource roots used by target/source rematerialization
                    // share the same live-table mutex as normal debug queries.
                    ::std::lock_guard table_lock{g_runtime.debug_table_mutex};
                    static_cast<void>(mutate_member(state,*actual,requested,result));return result;
                }
                if(requested.module>=state.modules.size() || requested.index>(::std::numeric_limits<::std::size_t>::max)())
                { result.status=copy_status::out_of_range;return result; }
                // All execution/host participants are already genuinely parked
                // or excluded. Share the exact mutex used by debug-full JIT
                // indirect/ref call snapshots; guest bridges are not invoked.
                ::std::lock_guard table_lock{g_runtime.debug_table_mutex};
                auto owner{static_cast<::std::size_t>(requested.module)},index{static_cast<::std::size_t>(requested.index)};
                bool const table{requested.target==wm::destination::table};
                if(!(table ? resolve_resource<true>(state,owner,index) : resolve_resource<false>(state,owner,index)) ||
                   !declaration_origin(state.modules[owner]))
                { result.status=copy_status::unavailable_foreign_host_state;return result; }
                // Actual initializer registry + canonical source + real guards
                // authenticate this mutable object; never cast a supplied ptr.
                auto& module{*const_cast<module_type*>(state.modules[owner].module)};
                // Result locations use Core3's actual module index spaces,
                // including imported entries. The alias resolver returned a
                // local-defined ordinal, so bound BOTH additions before any
                // slot change or narrowing. A reply must identify the same
                // real slot that a subsequent global/table query addresses.
                auto const imported_count{table ? module.imported_table_vec_storage.size() : module.imported_global_vec_storage.size()};
                if(index > (::std::numeric_limits<::std::size_t>::max)()-imported_count ||
                   index > (::std::numeric_limits<::std::uint64_t>::max)() ||
                   imported_count > (::std::numeric_limits<::std::uint64_t>::max)()-static_cast<::std::uint64_t>(index))
                { result.status=copy_status::out_of_range;return result; }
                auto const resolved_logical_index{static_cast<::std::uint64_t>(imported_count)+static_cast<::std::uint64_t>(index)};
                core_type declared{};::std::byte* destination_bytes{};
                ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t* destination_table{};
                ::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t* destination_global{};
                if(table)
                {
                    if(index>=module.local_defined_table_vec_storage.size()) { result.status=copy_status::out_of_range;return result; }
                    auto& target{module.local_defined_table_vec_storage.index_unchecked(index)};
                    if(requested.element>=target.elems.size() || requested.element>(::std::numeric_limits<::std::size_t>::max)())
                    { result.status=copy_status::out_of_range;return result; }
                    auto const& declaration{*target.table_type_ptr};
                    declared.kind=core_kind::reference;declared.nullable=true;
                    if(declaration.has_core_type) { declared=declaration.core_type; }
                    else if(static_cast<unsigned>(declaration.reftype)==0x70u) { declared.heap.code=static_cast<::std::int_least64_t>(heap_kind::func); }
                    else if(static_cast<unsigned>(declaration.reftype)==0x6fu) { declared.heap.code=static_cast<::std::int_least64_t>(heap_kind::extern_); }
                    else { result.status=copy_status::invalid_data;return result; }
                    destination_table=::std::addressof(target);
                }
                else
                {
                    if(index>=module.local_defined_global_vec_storage.size()) { result.status=copy_status::out_of_range;return result; }
                    auto& target{module.local_defined_global_vec_storage.index_unchecked(index)};
                    if(!target.global_type_ptr->is_mutable || !target.global.is_mutable)
                    { result.status=copy_status::available;result.reason=wm::refusal::immutable_global;return result; }
                    declared=::uwvm2::parser::wasm::standard::wasm1p1::features::core3_initializer_type_details::declared_value_type(*target.global_type_ptr);
                    copied_value old{};native_value checked_old{};
                    if(!copy_global(state,owner,static_cast<::std::size_t>(resolved_logical_index),old,::std::addressof(checked_old)))
                    { result.status=state.copy_error;return result; }
                    destination_global=::std::addressof(target);
                    using global_kind=::uwvm2::object::global::global_type;
                    switch(target.global.kind)
                    {
                        case global_kind::wasm_i32:destination_bytes=reinterpret_cast<::std::byte*>(::std::addressof(target.global.storage.i32));break;
                        case global_kind::wasm_i64:destination_bytes=reinterpret_cast<::std::byte*>(::std::addressof(target.global.storage.i64));break;
                        case global_kind::wasm_f32:destination_bytes=reinterpret_cast<::std::byte*>(::std::addressof(target.global.storage.f32));break;
                        case global_kind::wasm_f64:destination_bytes=reinterpret_cast<::std::byte*>(::std::addressof(target.global.storage.f64));break;
                        case global_kind::wasm_v128:destination_bytes=reinterpret_cast<::std::byte*>(::std::addressof(target.global.storage.v128));break;
                        case global_kind::wasm_ref:destination_bytes=reinterpret_cast<::std::byte*>(::std::addressof(target.global.storage.ref));break;
                    }
                }
                native_value value{};reference ref{};
                if(!mutation_source(state,*actual,requested,owner,declared,value,ref,result)) { return result; }
                if(declared.kind==core_kind::reference)
                {
                    if(value.type.kind!=core_kind::reference || !mutation_retain(state,owner,declared,ref,result.reason))
                    { result.status=copy_status::available;if(value.type.kind!=core_kind::reference){result.reason=wm::refusal::type_mismatch;}return result; }
                    ::std::memcpy(value.carrier.bits.data(),::std::addressof(ref),sizeof(ref));
                }
                else if(table || value.type.kind==core_kind::reference)
                { result.status=copy_status::available;result.reason=wm::refusal::type_mismatch;return result; }
                // Last fail-closed current proof BEFORE the only slot write.
                // Partial recipient lease bookkeeping may remain on refusal,
                // but the original global/table value has not been modified.
                result.reason=wm::refusal::none;
                if(!current_scope()) { result.status=copy_status::stale_stop_or_generation;return result; }
                if(destination_table!=nullptr)
                {
                    // [actual initialized slots0..size][element<size+SIZE_MAX]
                    // [safe] checked BEFORE narrowing/index; table size cannot
                    // change within this complete cohort+N+table mutex scope.
                    destination_table->elems.index_unchecked(static_cast<::std::size_t>(requested.element))=
                        ::uwvm2::uwvm::runtime::storage::runtime_table_slot_from_gc_reference(ref);
                    // debug-full indirect calls resolve the live table under
                    // this mutex; its existing compact refresh hook is no-op.
                }
                else
                {
                    if(destination_global==nullptr || destination_bytes==nullptr) { result.status=copy_status::invalid_data;return result; }
                    auto const width{declared.kind==core_kind::reference ? sizeof(reference) : declared.kind==core_kind::v128 ? 16u :
                        declared.kind==core_kind::i32 || declared.kind==core_kind::f32 ? 4u:8u};
                    // [real complete declared native union member][width<=16]
                    // [safe] authenticated exact kind BEFORE memcpy, no float evaluation.
                    ::std::memcpy(destination_bytes,value.carrier.bits.data(),width);
                }
                // The now-real typed slot is visited by the existing all-cohort
                // GC root census; arena retention alone was never an object root.
                result.status=copy_status::available;result.reason=wm::refusal::none;result.applied=true;result.runtime_epoch=epoch_;
                result.module=state.modules[owner].id;result.index=resolved_logical_index;return result;
            }
            catch(...) { result.status=copy_status::allocation_failed;return result; }
        }

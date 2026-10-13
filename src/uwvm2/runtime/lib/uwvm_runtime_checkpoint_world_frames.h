// Include ONLY inside the private world transaction. These owned frame/root
// carriers are preparation DATA. The separate private cold root scope can
// install/remove genuine TLS records without fabricating a guest activation.
#pragma once
using frame_checkpoint = ::uwvm2::runtime::checkpoint::native_value;
struct prepared_world_frame
{
    object_id wire{};
    ::std::size_t owner{SIZE_MAX};
    ::uwvm2::runtime::checkpoint::sealed_function_plan::owner plan{};
    ::uwvm2::runtime::checkpoint::caller_return_projection::owner child_projection{};
    ::std::uint64_t site{};
    ::std::uintptr_t entry{}; // private exact fresh-engine entry; never returned
    ::std::vector<frame_checkpoint> values{};
    ::std::vector<::std::byte> payload{}, output{};
    ::std::vector<::std::uint8_t> flags{};
    ::std::vector<::std::size_t> result_offsets{}, result_widths{};
    ::std::vector<core_type> result_types{};
    ::std::vector<reference> input_roots{}, result_roots{};
    ::std::vector<::uwvm2::runtime::exception::instance_root> input_root_owners{};
};
struct prepared_world_thread
{
    object_id wire{};
    ::std::vector<prepared_world_frame> frames{};
    ::std::vector<::uwvm2::runtime::gc::root_frame> root_records{};
};
// Declared after engines: these borrowed plans/entries/carriers die first,
// before pending ranges, engines, staged GC shells and source/type owners.
::std::vector<prepared_world_thread> prepared_threads_{};
::std::vector<reference> retained_roots_{};
::std::vector<::uwvm2::runtime::exception::instance_root> retained_root_owners_{};
::std::size_t prepared_frames_{}, prepared_root_carriers_{};

[[nodiscard]] bool prepare_frame_value(checkpoint_world_data::cp::value const& saved,
    ::std::size_t owner, ::uwvm2::runtime::checkpoint::typed_slot expected,
    prepared_world_frame& frame, ::std::size_t slot)
{
    namespace ck=::uwvm2::runtime::checkpoint;
    if(slot>=frame.values.size() || slot>=frame.plan->get().sites[frame.site-1u].slots.size() ||
       !slot_type_matches(saved.type,owner,{expected.type}) ||
       (expected.initialized && !saved.initialized) ||
       (!saved.initialized && (slot>=frame.flags.size() || expected.type.kind!=checkpoint_world_data::rt::value_kind::reference)))
    { return fail(preparation_status::type_mismatch); }
    native_value relocated{};
    if(!relocate_value(saved,owner,relocated,true)) { return fail(preparation_status::type_mismatch); }
    auto& value{frame.values[slot]};
    value.declaration={expected.type,saved.initialized};value.bits=relocated.bits;
    // [owned complete fixed16 slot][bounded payload slot*16] end
    // [safe] frame allocation bound proved before slot/product/byte advance.
    ::fast_io::freestanding::my_memcpy(frame.payload.data()+slot*ck::native_slot_bytes,
        value.bits.data(),ck::native_slot_bytes);
    if(slot<frame.flags.size()) { frame.flags[slot]=saved.initialized?1u:0u; }
    if(saved.initialized && expected.type.kind==checkpoint_world_data::rt::value_kind::reference)
    {
        auto const ref{relocated.template as<reference>()};
        if(!::uwvm2::runtime::gc::frame_root_details::runtime_kind(ref)) { return fail(preparation_status::type_mismatch); }
        ::uwvm2::runtime::exception::instance_root root{};
        if(!staged_gc_.pending_reference_root(modules_[owner].actual->gc_store,ref,root)) { return fail(preparation_status::type_mismatch); }
        frame.input_roots.push_back(ref); // capacity for ALL slots charged first
        frame.input_root_owners.push_back(::std::move(root));
    }
    return true;
}

[[nodiscard]] bool prepare_one_world_frame(object_id id, bool has_child, prepared_world_frame& next)
{
    namespace cp=checkpoint_world_data::cp;namespace ck=::uwvm2::runtime::checkpoint;
    namespace emit=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
    auto const* frame{lookup(id)};
    if(frame==nullptr || frame->kind!=cp::object_kind::frame || frame->links.empty() ||
       frame->flags!=(has_child?1u:0u)) { return fail(preparation_status::unavailable_adapter); }
    auto const* function{lookup(frame->links.front())};auto const* native_function{native(frame->links.front())};
    if(function==nullptr || function->kind!=cp::object_kind::function || function->flags!=0u ||
       native_function==nullptr || native_function->owner>=modules_.size()) { return fail(preparation_status::source_mismatch); }
    auto const owner{native_function->owner};auto const dense{modules_[owner].new_actual_id};auto const local{native_function->local};
    if(dense>=engines_.size() || !engines_[dense] || local>=engines_[dense]->locals_.size() ||
       local>=engines_[dense]->entries_.size() || local>=engines_[dense]->generations_.size() ||
       local>=modules_[owner].actual->local_defined_function_vec_storage.size()) { return fail(preparation_status::source_mismatch); }
    auto const& engine{*engines_[dense]};auto const plan{engine.locals_[local].checkpoint_plan};
    if(!plan || plan->get().module!=dense || plan->get().function!=function->words[0u] ||
       plan->get().function_generation!=function->words[1u] || engine.generations_[local]!=frame->words[5u] ||
       plan->get().profile.get()!=actual_prepared_profile_.get() ||
       plan->get().profile.owner_before(actual_prepared_profile_) || actual_prepared_profile_.owner_before(plan->get().profile) ||
       plan->get().resume_abi_revision!=ck::native_resume_abi_revision || engine.entries_[local].resume_raw==0u)
    { return fail(preparation_status::source_mismatch); }
    // Wire offsets are DATA. Match exactly ONE freshly compiler-sealed site,
    // never treat an opcode offset or a saved old ordinal as an entry address.
    ::std::uint64_t site_id{};
    for(auto const& site:plan->get().sites)
    {
        if(site.opcode_offset!=frame->words[0u] || static_cast<unsigned>(site.phase)!=frame->flags ||
           site.local_count!=frame->words[1u] || site.operand_count!=frame->words[2u] ||
           site.controls.size()!=frame->words[3u] || site.handlers.size()!=frame->words[4u] ||
           site.caller_return_offset!=frame->words[6u]) { continue; }
        if(site_id!=0u) { return fail(preparation_status::type_mismatch); }
        site_id=site.identifier;
    }
    if(site_id==0u || site_id>plan->get().sites.size()) { return fail(preparation_status::type_mismatch); }
    bool installed{};for(auto const actual:plan->get().resume_sites) { installed=installed || actual==site_id; }
    if(!installed) { return fail(preparation_status::unavailable_adapter); }
    auto const& site{plan->get().sites[site_id-1u]};
    if(site.local_count>frame->values.size() || site.operand_count!=frame->values.size()-site.local_count ||
       site.controls.size()>frame->links.size()-1u || site.handlers.size()!=frame->links.size()-1u-site.controls.size() ||
       site.slots.size()>static_cast<::std::size_t>(PTRDIFF_MAX)/ck::native_slot_bytes ||
       site.slots.size()>::uwvm2::runtime::gc::frame_root_details::max_capacity || site.controls.empty())
    { return fail(preparation_status::type_mismatch); }
    auto const& declaration{modules_[owner].actual->local_defined_function_vec_storage.index_unchecked(local)};
    if(declaration.function_type_ptr==nullptr) { return fail(preparation_status::type_mismatch); }
    auto const abi{emit::get_runtime_wasm_call_abi_layout(*declaration.function_type_ptr)};
    auto const& results{site.controls.front().declared_results};
    if(!abi.valid || abi.result_bytes>static_cast<::std::size_t>(PTRDIFF_MAX) || results.size()!=abi.result_count ||
       results.size()>::uwvm2::runtime::gc::frame_root_details::max_capacity)
    { return fail(preparation_status::type_mismatch); }
    if(!charge_native(site.slots.size(),sizeof(frame_checkpoint)+ck::native_slot_bytes+sizeof(reference)+sizeof(::uwvm2::runtime::exception::instance_root)) ||
       !charge_native(site.local_count,1u) || !charge_native(abi.result_bytes,1u) ||
       !charge_native(results.size(),sizeof(core_type)+2u*sizeof(::std::size_t)+sizeof(reference))) { return false; }
    next.wire=id;next.owner=owner;next.plan=plan;next.site=site_id;next.entry=engine.entries_[local].resume_raw;
    next.values.resize(site.slots.size());next.payload.resize(site.slots.size()*ck::native_slot_bytes);
    next.flags.resize(site.local_count);next.output.resize(abi.result_bytes);next.input_roots.reserve(site.slots.size());
    next.input_root_owners.reserve(site.slots.size());
    next.result_types=results;next.result_offsets.reserve(results.size());next.result_widths.reserve(results.size());
    next.result_roots.resize(results.size()); // null carriers, never fake returned results
    ::std::size_t offset{};
    for(::std::size_t n{};n<results.size();++n)
    {
        auto const width{emit::get_runtime_wasm_value_type_abi_size(declaration.function_type_ptr->result.begin[n])};
        auto const typed{emit::get_runtime_wasm_value_type_abi_size(emit::checkpoint_packet_physical_carrier(results[n]))};
        if(width==0u || width!=typed || width>ck::native_slot_bytes || offset>abi.result_bytes || width>abi.result_bytes-offset)
        { return fail(preparation_status::type_mismatch); }
        next.result_offsets.push_back(offset);next.result_widths.push_back(width);offset+=width;
    }
    if(offset!=abi.result_bytes) { return fail(preparation_status::type_mismatch); }
    for(::std::size_t n{};n<frame->values.size();++n)
    { if(!prepare_frame_value(frame->values[n],owner,site.slots[n],next,n)) { return false; } }
    auto const live{frame->values.size()};::std::size_t saved_parameters{};
    for(::std::size_t n{};n<site.controls.size();++n)
    {
        auto const& layout{site.controls[n]};auto const* control{lookup(frame->links[1u+n])};
        auto const kind{layout.kind==ck::control_kind::function?3u:layout.kind==ck::control_kind::block?0u:
            layout.kind==ck::control_kind::loop?1u:2u};
        if(control==nullptr || control->kind!=cp::object_kind::control || control->flags!=kind ||
           control->words[0u]!=layout.entry_offset || control->words[1u]!=layout.end_offset ||
           control->words[2u]!=layout.saved_parameter_count || control->words[3u]!=layout.declared_results.size() ||
           control->values.size()!=layout.saved_parameter_count || layout.first_saved_parameter!=saved_parameters ||
           saved_parameters>site.saved_parameter_count || layout.saved_parameter_count>site.saved_parameter_count-saved_parameters)
        { return fail(preparation_status::type_mismatch); }
        for(::std::size_t k{};k<control->values.size();++k)
        {
            auto const slot{live+saved_parameters+k};
            if(slot>=site.slots.size() || !control->values[k].initialized || !prepare_frame_value(control->values[k],owner,site.slots[slot],next,slot)) { return false; }
        }
        saved_parameters+=layout.saved_parameter_count;
    }
    if(saved_parameters!=site.saved_parameter_count) { return fail(preparation_status::type_mismatch); }
    ::std::size_t tag_first{},tag_imported{},tag_local{};
    if(!wire_space<4u>(owner,tag_first,tag_imported,tag_local)) { return fail(preparation_status::source_mismatch); }
    auto const* instance{lookup(modules_[owner].instance)};
    for(::std::size_t n{};n<site.handlers.size();++n)
    {
        auto const& layout{site.handlers[n]};auto const* handler{lookup(frame->links[1u+site.controls.size()+n])};
        if(handler==nullptr || handler->kind!=cp::object_kind::handler || layout.target_control>=site.controls.size() ||
           handler->flags!=static_cast<unsigned>((layout.catch_all?2u:0u)|(layout.with_reference?1u:0u)) ||
           handler->words[0u]!=site.controls.size()-1u-layout.target_control || handler->words[1u]!=layout.target_offset ||
           handler->words[2u]!=0u || handler->links.size()!=(layout.catch_all?0u:1u) ||
           (!layout.catch_all && (layout.tag_index>=tag_imported+tag_local || handler->links[0u]!=instance->links[tag_first+layout.tag_index])))
        { return fail(preparation_status::type_mismatch); }
    }
    if(has_child)
    {
        if(site_id==UINT64_MAX || site_id>=plan->get().sites.size()) { return fail(preparation_status::type_mismatch); }
        auto const& after{plan->get().sites[site_id]};
        if(after.operand_count<site.operand_count || live>after.slots.size() ||
           after.operand_count-site.operand_count>after.slots.size()-live) { return fail(preparation_status::type_mismatch); }
        auto const count{after.operand_count-site.operand_count};
        if(!charge_native(1u,sizeof(ck::caller_return_projection)) || !charge_native(count,2u*sizeof(core_type))) { return false; }
        ::std::vector<core_type> expected{};expected.reserve(count);
        for(::std::size_t n{};n<count;++n) { expected.push_back(after.slots[live+n].type); }
        next.child_projection=ck::caller_return_projection::seal_compiler_data(plan,site_id,site_id+1u,expected);
        if(!next.child_projection) { return fail(preparation_status::unavailable_adapter); }
    }
    return true;
}

[[nodiscard]] bool prepare_all_world_frames()
{
    namespace cp=checkpoint_world_data::cp;
    if(phase_!=preparation_status::resources_prepared || !actual_prepared_profile_ || engines_.size()!=modules_.size() ||
       !prepared_threads_.empty() || !retained_roots_.empty()) { return fail(preparation_status::source_mismatch); }
    ::std::size_t count{};for(auto const& object:saved_.objects) { if(object.kind==cp::object_kind::thread) { ++count; } }
    if(count==0u || count>prepared_threads_.max_size() || !charge_native(count,sizeof(prepared_world_thread)))
    { return fail(preparation_status::quota_exceeded); }
    prepared_threads_.reserve(count);
    for(::std::size_t index{};index<saved_.objects.size();++index)
    {
        auto const& thread{saved_.objects[index]};if(thread.kind!=cp::object_kind::thread) { continue; }
        if(thread.flags!=0u || thread.links.empty() || thread.links.size()>actual_prepared_profile_->limits().frames ||
           thread.words[0u]!=prepared_threads_.size()+1u || thread.words[1u]!=prepared_threads_.size())
        { return fail(preparation_status::unavailable_adapter); }
        if(!charge_native(thread.links.size(),sizeof(prepared_world_frame))) { return false; }
        prepared_threads_.emplace_back();auto& next{prepared_threads_.back()};next.wire=index+1u;next.frames.resize(thread.links.size());
        for(::std::size_t n{};n<thread.links.size();++n)
        { if(!prepare_one_world_frame(thread.links[n],n+1u<thread.links.size(),next.frames[n])) { return false; } }
        if(next.frames.size()>(SIZE_MAX-1u)/2u ||
           !charge_native(1u+2u*next.frames.size(),sizeof(::uwvm2::runtime::gc::root_frame))) { return fail(preparation_status::quota_exceeded); }
        next.root_records.resize(1u+2u*next.frames.size()); // Fixed native lifetime before any TLS publication.
        for(::std::size_t n{};n+1u<next.frames.size();++n)
        {
            auto const& parent{next.frames[n]};auto const& child{next.frames[n+1u]};
            if(!parent.child_projection || parent.child_projection->result_types().size()!=child.result_types.size())
            { return fail(preparation_status::type_mismatch); }
            auto const expected{parent.child_projection->result_types()};
            for(::std::size_t k{};k<expected.size();++k)
            {
                if(!checkpoint_world_data::st::gc_object_store::canonical_value_type_matches(
                    modules_[child.owner].actual->gc_store.get(),child.result_types[k],
                    modules_[parent.owner].actual->gc_store.get(),expected[k])) { return fail(preparation_status::type_mismatch); }
            }
        }
    }
    if(saved_.retained_roots.size()>retained_roots_.max_size() ||
       !charge_native(saved_.retained_roots.size(),sizeof(reference)+sizeof(::uwvm2::runtime::exception::instance_root))) { return false; }
    retained_roots_.reserve(saved_.retained_roots.size());retained_root_owners_.reserve(saved_.retained_roots.size());
    for(auto const& value:saved_.retained_roots)
    {
        native_value relocated{};
        if(value.type.kind!=cp::value_kind::reference || !relocate_value(value,0u,relocated)) { return fail(preparation_status::type_mismatch); }
        auto const ref{relocated.template as<reference>()};::uwvm2::runtime::exception::instance_root root{};
        if(!staged_gc_.pending_reference_root(modules_[0u].actual->gc_store,ref,root)) { return fail(preparation_status::type_mismatch); }
        retained_roots_.push_back(ref);retained_root_owners_.push_back(::std::move(root));
    }
    // Complete counters only after EVERY packet, edge and retained root passed.
    prepared_root_carriers_=retained_roots_.size();
    for(auto const& thread:prepared_threads_)
    {
        if(thread.frames.size()>SIZE_MAX-prepared_frames_) { return fail(preparation_status::quota_exceeded); }
        prepared_frames_+=thread.frames.size();
        for(auto const& frame:thread.frames)
        {
            if(frame.input_roots.size()>SIZE_MAX-prepared_root_carriers_) { return fail(preparation_status::quota_exceeded); }
            prepared_root_carriers_+=frame.input_roots.size();
        }
    }
    return true;
}

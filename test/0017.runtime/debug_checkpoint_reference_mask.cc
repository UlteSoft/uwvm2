// Sealed type decoding DATA: all actual carriers/flags are still validated.
#include <uwvm2/runtime/checkpoint/shadow_ledger.h>
#include <uwvm2/runtime/lib/uwvm_runtime_debug_operand_workspace.h>
#include <cstdio>
#include <cstdlib>
namespace cp=uwvm2::runtime::checkpoint;
using kind=uwvm2::object::global::wasm_ref_kind;
static void require(bool value,char const* text)
{ if(!value) { std::fprintf(stderr,"reference mask: %s\n",text);std::abort(); } }
// Independent truth sets expressed as lists, not production bit arithmetic.
static bool accepted(cp::types::core_value_type type,kind tag)
{
    if(tag==kind::wasm_null) { return type.nullable; }
    if(type.heap.is_defined())
    { return tag==kind::wasm_func_imported || tag==kind::wasm_func_defined || tag==kind::wasm_struct || tag==kind::wasm_array; }
    using heap=cp::types::abstract_heap_type;
    switch(static_cast<heap>(type.heap.code))
    {
        case heap::any:return tag==kind::wasm_struct || tag==kind::wasm_array || tag==kind::wasm_i31 || tag==kind::wasm_extern;
        case heap::eq:return tag==kind::wasm_struct || tag==kind::wasm_array || tag==kind::wasm_i31;
        case heap::i31:return tag==kind::wasm_i31;
        case heap::struct_:return tag==kind::wasm_struct;
        case heap::array:return tag==kind::wasm_array;
        case heap::func:return tag==kind::wasm_func_imported || tag==kind::wasm_func_defined;
        case heap::extern_:return tag==kind::wasm_extern;
        case heap::exn:return tag==kind::wasm_exn;
        case heap::none:case heap::nofunc:case heap::noextern:case heap::noexn:return false;
    }
    return false;
}
static auto make_plan(cp::compilation_profile::owner profile,cp::types::core_value_type type)
{
    cp::function_plan p{};p.profile=profile;p.expression_bytes=16;p.function_generation=1;
    cp::safepoint_layout s{};s.identifier=1;s.local_count=1;s.slots.push_back({type,type.nullable});
    cp::control_layout control{};control.end_offset=15;s.controls.push_back(control);p.sites.push_back(s);
    s.identifier=2;s.opcode_offset=3;s.caller_return_offset=5;s.phase=cp::frame_phase::awaiting_call_return;
    s.slots[0].initialized=true;p.sites.push_back(s);
    return cp::sealed_function_plan::seal_compiler_metadata(std::move(p));
}
int main()
{
    auto profile=cp::compilation_profile::create_for_trusted_observer();std::size_t cases{},rejected{};
    constexpr std::array<std::int_least64_t,15> heaps{-12,-13,-14,-15,-16,-17,-18,-19,-20,-21,-22,-23,0,7,UINT32_MAX};
    constexpr std::array<unsigned,15> tags{0,1,2,3,4,5,6,7,8,9,15,31,32,255,UINT32_MAX};
    using storage=uwvm2::runtime::lib::details::debug_operand_workspace_stack;
    for(auto heap:heaps)for(bool nullable:{false,true})
    {
        cp::types::core_value_type type{cp::types::value_kind::reference,{heap},nullable};auto plan=make_plan(profile,type);
        require(bool(plan),"complete source plan sealed");
        require(plan->packet_shape(1).reference_slots.size()==1 && plan->packet_shape(1).reference_slots[0].index==0,
            "reference ordinal remains exact");
        for(auto tag:tags)
        {
            auto carrier=uwvm2::object::global::make_wasm_i31_reference(-7);carrier.kind=static_cast<kind>(tag);
            std::array<std::byte,cp::native_slot_bytes> payload{};std::memcpy(payload.data(),&carrier,sizeof(carrier));
            std::array<std::uint8_t,1> flags{1};bool expected=accepted(type,carrier.kind);
            std::vector<cp::native_value> decoded{{{{cp::types::value_kind::i32},true},{}}};decoded[0].bits[0]=std::byte{0x71};
            auto snapshot=decoded;
            auto result=cp::shadow_ledger::decode_compiler_packet(plan,2,payload,flags,decoded);
            require(result==(expected ? cp::status::ok:cp::status::invalid_reference),"every bounded/unbounded tag matches independent truth set");
            if(!expected) { ++rejected;require(decoded[0].declaration==snapshot[0].declaration && decoded[0].bits==snapshot[0].bits,"rejection does not mutate prior values"); }
            // This unchanged typed-DATA commit uses the old envelope predicate.
            cp::shadow_ledger old(profile);cp::activation_identity parent{1,0,1,1};
            require(old.enter(parent,plan)==cp::status::ok,"old predicate activation admitted");
            cp::native_value value{{type,true},payload};
            require(old.materialize_validated_native_data(parent,plan,2,{&value,1})==result,"unchanged full type predicate agrees");
            // Genuine bounded LIFO slots, parent admission and root visitor.
            storage stack;auto bytes=stack.enter(cp::observer_local_metadata_bytes+cp::native_slot_bytes,cp::observer_site_bytes);
            auto owner=stack.current_storage();cp::shadow_ledger live(profile);
            require(live.compiler_enter_observer(parent,plan)==cp::status::ok && live.bind_observer_storage(parent,owner)==cp::status::ok,"actual parent workspace bound");
            bytes[1]=std::byte{1};std::memcpy(bytes.data()+cp::observer_local_metadata_bytes,payload.data(),payload.size());
            std::uint64_t site=2;std::memcpy(owner.data(),&site,sizeof(site));
            unsigned visits{};auto census=live.visit_recorded_native_references([&](auto got){++visits;return got.kind==carrier.kind;});
            require(census==result && visits==unsigned(expected),"whole packet checked before any visitor");
            auto child=cp::activation_identity{2,1,2,1};auto entry=live.compiler_enter_observer(child,plan);
            require(entry==result,"awaiting parent repeats current carrier check");
            if(expected)require(live.leave(child,false)==cp::status::ok,"actual child retires");
            live.unbind_observer_storage(owner.data());require(stack.leave(reinterpret_cast<std::uintptr_t>(bytes.data())),"exact parent buffer retires");
            ++cases;
        }
        std::array<std::byte,cp::native_slot_bytes> payload{};std::array<std::uint8_t,1> bad{2};std::vector<cp::native_value> values;
        require(cp::shadow_ledger::decode_compiler_packet(plan,2,payload,bad,values)==cp::status::invalid_layout,"bad initialization tag checked before carrier");
        bad[0]=0;require(cp::shadow_ledger::decode_compiler_packet(plan,2,payload,bad,values)==cp::status::invalid_layout,"required local flag cannot be unset");
        bad[0]=1;require(cp::shadow_ledger::decode_compiler_packet(plan,2,std::span<std::byte const>{payload}.first(15),bad,values)==cp::status::invalid_layout,"short payload cannot expose carrier");
    }
    require(cases==450 && rejected!=0,"complete nullable/abstract/defined/invalid-tag matrix");
    std::printf("PASS 450 carrier cases: old predicate, manual truth sets, fresh parent admission, pre-visitor rejection; %zu rejected\n",rejected);
}

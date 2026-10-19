// Sealed packet decoding DATA and LIFO storage checks, not pause/restore authority.
#include <uwvm2/runtime/checkpoint/shadow_ledger.h>
#include <uwvm2/runtime/lib/uwvm_runtime_debug_operand_workspace.h>
#include <cstdio>
#include <cstdlib>
namespace cp=uwvm2::runtime::checkpoint;
void check(bool value,char const* message) { if(!value) { std::fprintf(stderr,"FAIL packet shape: %s\n",message);std::abort(); } }
cp::sealed_function_plan::owner make_plan(bool mixed,bool varying)
{
    cp::function_plan p{};p.profile=cp::compilation_profile::create_for_trusted_observer();p.expression_bytes=32;p.function_generation=1;
    cp::safepoint_layout first{};first.identifier=1;first.local_count=4;
    first.slots={{{cp::types::value_kind::i32},true},{{cp::types::value_kind::f64},true},{{cp::types::value_kind::v128},true},{{cp::types::value_kind::i64},true}};
    if(mixed) first.slots[3]={ {cp::types::value_kind::reference,{static_cast<std::int_least64_t>(cp::types::abstract_heap_type::i31)},false},false};
    cp::control_layout control{};control.end_offset=31;first.controls.push_back(control);p.sites.push_back(first);
    auto second=first;second.identifier=2;second.opcode_offset=3;second.operand_count=1;second.slots.push_back({{cp::types::value_kind::f32},true});
    if(mixed)second.slots[3].initialized=true;
    if(varying)second.slots[0].type.kind=cp::types::value_kind::i64;
    p.sites.push_back(std::move(second));return cp::sealed_function_plan::seal_compiler_metadata(std::move(p));
}
int main()
{
    std::uint64_t random=0x2bdef495c73a89ull;
    auto next=[&] {random^=random<<13;random^=random>>7;random^=random<<17;return random;};
    for(bool defer:{false,true})for(bool mixed:{false,true})for(bool varying:{false,true})
    {
        auto plan=make_plan(mixed,varying);check(bool(plan),"valid sealed metadata");check(plan->invariant_local_types()==!varying,"invariant prefix is sealed, never caller asserted");
        check(plan->packet_shape(0).reference_slots.size()==static_cast<std::size_t>(mixed),"only exact reference indices");
        cp::shadow_ledger ledger{plan->get().profile};cp::activation_identity identity{1,0,1,1};check(ledger.enter(identity,plan)==cp::status::ok,"activation entry");
        std::array<std::byte,80> payload{};std::array<std::uint8_t,4> flags{1,1,1,1};
        std::vector<cp::native_value> oracle{};
        for(unsigned iteration{};iteration!=4000;++iteration)
        {
            auto const site=iteration%2u+1u;
            for(auto& byte:payload)byte=static_cast<std::byte>(next());
            flags[3]=mixed && site==1u && iteration%4u==0u ? 0u:1u;
            if(mixed)
            {
                std::memset(payload.data()+48,0,16);
                if(flags[3]) { auto const ref=uwvm2::object::global::make_wasm_i31_reference(static_cast<std::int32_t>(next()));std::memcpy(payload.data()+48,&ref,sizeof(ref)); }
            }
            auto bytes=std::span<std::byte const>{payload}.first(plan->get().sites[site-1u].slots.size()*cp::native_slot_bytes);
            check(cp::shadow_ledger::decode_compiler_packet(plan,site,bytes,flags,oracle)==cp::status::ok,"standalone full declaration decode");
            check(ledger.materialize_compiler_native_data(identity,plan,site,bytes,flags,defer)==cp::status::ok,"activation reuse decode");
            std::memset(payload.data(),0xa5,payload.size()); // the real producer may reuse its workspace immediately
            auto const frames=ledger.frames_while_actually_stopped();auto const& values=frames.back().values;check(values.size()==oracle.size(),"exact current size");
            for(std::size_t i{};i!=values.size();++i)check(values[i].declaration==oracle[i].declaration && values[i].bits==oracle[i].bits,"all declarations/flags/bytes match full decode");
        }
        auto preserved=oracle;flags[0]=2u;
        check(cp::shadow_ledger::decode_compiler_packet(plan,2,payload,flags,oracle)==cp::status::invalid_layout,"invalid flag rejected in both mask paths");
        for(std::size_t i{};i!=oracle.size();++i)check(oracle[i].declaration==preserved[i].declaration && oracle[i].bits==preserved[i].bits,"invalid packet changes no prior values");
    }
    uwvm2::runtime::lib::details::debug_operand_workspace_stack stack;
    auto outer=stack.enter(512);auto base=reinterpret_cast<std::uintptr_t>(outer.data());auto payload=base+4*cp::observer_local_metadata_bytes;
    auto owned=stack.current_local_snapshot(payload,4,cp::observer_local_metadata_bytes,cp::native_slot_bytes,5);
    check(owned.valid && owned.values.size()==80 && owned.flags.size()==4 && owned.flags.data()==reinterpret_cast<std::uint8_t const*>(outer.data()+4),"actual owner gives disjoint exact spans");
    check(!stack.current_local_snapshot(payload+1,4,cp::observer_local_metadata_bytes,16).valid,"interior forged payload refused");
    check(!stack.current_local_snapshot(payload,4,cp::observer_local_metadata_bytes,16,SIZE_MAX).valid,"overflow extent refused");
    auto inner=stack.enter(64);check(!stack.current_local_snapshot(payload,4,cp::observer_local_metadata_bytes,16).valid,"outer owner cannot nominate current inner activation");
    check(stack.leave(reinterpret_cast<std::uintptr_t>(inner.data())) && stack.current_local_snapshot(payload,4,cp::observer_local_metadata_bytes,16).valid,"genuine child retirement restores parent owner");
    check(stack.leave(base) && !stack.current_local_snapshot(payload,4,cp::observer_local_metadata_bytes,16).valid,"retired owner never borrowed");
    std::puts("PASS sealed packet shapes: 32000 eager/deferred full-byte/type/flag transitions and actual LIFO owner bounds");
}

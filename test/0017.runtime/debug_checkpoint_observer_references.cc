// Live observer DATA: typed cold reads and GC census, never capture/restore rights.
#include <uwvm2/runtime/checkpoint/shadow_ledger.h>
#include <uwvm2/runtime/lib/uwvm_runtime_debug_operand_workspace.h>
#include <cstdio>
#include <cstdlib>
namespace cp=uwvm2::runtime::checkpoint;
static void check(bool okay,char const* what)
{ if(!okay) { std::fprintf(stderr,"observer references: %s\n",what);std::abort(); } }
static auto plan(cp::compilation_profile::owner profile)
{
    cp::function_plan p{};p.profile=profile;p.expression_bytes=32;p.function_generation=1;
    cp::safepoint_layout s{};s.identifier=1;s.local_count=3;s.operand_count=1;
    auto const heap=cp::types::heap_type{static_cast<std::int_least64_t>(cp::types::abstract_heap_type::i31)};
    s.slots={{{cp::types::value_kind::i32},true},{{cp::types::value_kind::reference,heap,true},true},
             {{cp::types::value_kind::reference,heap,false},false},{{cp::types::value_kind::reference,heap,false},true}};
    cp::control_layout control{};control.end_offset=31;s.controls.push_back(control);p.sites.push_back(s);
    s.identifier=2;s.opcode_offset=3;s.phase=cp::frame_phase::awaiting_call_return;s.caller_return_offset=5;p.sites.push_back(s);
    return cp::sealed_function_plan::seal_compiler_metadata(std::move(p));
}
static constexpr auto bytes=3*cp::observer_local_metadata_bytes+4*cp::native_slot_bytes;
static void publish(std::span<std::byte> storage,std::uint64_t site,bool initialized,int value)
{
    auto workspace=storage.subspan(cp::observer_site_bytes);
    std::fill(workspace.begin(),workspace.end(),std::byte{});
    workspace[3]=workspace[4]=std::byte{1};workspace[5]=initialized ? std::byte{1}:std::byte{};
    auto payload=workspace.subspan(3*cp::observer_local_metadata_bytes);
    std::int32_t integer=value;std::memcpy(payload.data(),&integer,sizeof(integer));
    for(unsigned i=1;i!=4;++i)
    {
        if(i==2 && !initialized) { continue; }
        auto ref=uwvm2::object::global::make_wasm_i31_reference(value+static_cast<int>(i));
        std::memcpy(payload.data()+i*cp::native_slot_bytes,&ref,sizeof(ref));
    }
    std::memcpy(storage.data(),&site,sizeof(site));
}
int main()
{
    auto profile=cp::compilation_profile::create_for_trusted_observer();auto source=plan(profile);check(bool(source),"sealed typed sites");
    using storage_stack=uwvm2::runtime::lib::details::debug_operand_workspace_stack;
    storage_stack storage;cp::shadow_ledger ledger(profile);
    auto root=cp::activation_identity{1,0,1,1};check(ledger.enter(root,source)==cp::status::ok,"root admission");
    auto workspace=storage.enter(bytes,cp::observer_site_bytes);auto owner=storage.current_storage();
    check(ledger.bind_observer_storage(root,owner)==cp::status::ok,"actual workspace binding");
    for(unsigned iteration{};iteration!=2000;++iteration)
    {
        bool const initialized=iteration%2!=0;publish(owner,2,initialized,static_cast<int>(iteration));
        auto payload=workspace.subspan(3*cp::observer_local_metadata_bytes,4*cp::native_slot_bytes);
        auto flags=std::span<std::uint8_t const>{reinterpret_cast<std::uint8_t const*>(workspace.data()+3),3};
        check(ledger.compiler_observer_packet(root,2,payload,flags),"bound typed packet cold path");
        check(!ledger.compiler_observer_packet(root,1,payload,flags) &&
              !ledger.compiler_observer_packet(root,2,payload.subspan(1),flags),"foreign site/range cannot borrow");
        std::vector<cp::native_value> oracle;
        check(cp::shadow_ledger::decode_compiler_packet(source,2,payload,flags,oracle)==cp::status::ok,"full decoder oracle");
        unsigned roots{};check(ledger.visit_recorded_native_references([&](auto ref)
        { ++roots;return ref.kind==uwvm2::object::global::wasm_ref_kind::wasm_i31; })==cp::status::ok && roots==(initialized ? 3u:2u),"actual live references, unset local skipped");
        auto frames=ledger.frames_while_actually_stopped();check(frames.size()==1 && frames[0].values.size()==4,"cold typed expansion");
        for(std::size_t i{};i!=4;++i)check(frames[0].values[i].declaration==oracle[i].declaration && frames[0].values[i].bits==oracle[i].bits,"every type, flag and carrier matches eager oracle");
        // A previously expanded vector must never mask the new current slots.
        publish(owner,2,!initialized,static_cast<int>(iteration+100));roots=0;
        check(ledger.visit_recorded_native_references([&](auto){++roots;return true;})==cp::status::ok && roots==(!initialized ? 3u:2u),"census uses current slots after a previous inspection");
    }
    publish(owner,2,true,37);auto child=cp::activation_identity{2,1,2,1};
    check(ledger.enter(child,source)==cp::status::ok,"typed awaiting parent remains qualified");
    auto inner=storage.enter(bytes,cp::observer_site_bytes);auto inner_owner=storage.current_storage();
    check(ledger.bind_observer_storage(child,inner_owner)==cp::status::ok,"child bound");publish(inner_owner,1,false,71);
    unsigned roots{};check(ledger.visit_recorded_native_references([&](auto){++roots;return true;})==cp::status::ok && roots==5,"parent and child references both censused");
    ledger.unbind_observer_storage(inner_owner.data());check(storage.leave(reinterpret_cast<std::uintptr_t>(inner.data())) && ledger.leave(child,false)==cp::status::ok,"genuine child retirement");
    ledger.unbind_observer_storage(owner.data());check(storage.leave(reinterpret_cast<std::uintptr_t>(workspace.data())) && ledger.leave(root,false)==cp::status::ok,"root storage retires after unbind");
    check(ledger.executable_restore_capability()==cp::status::unavailable_resume,"observation never grants executable restore");
    for(unsigned bad{};bad!=5;++bad)
    {
        cp::shadow_ledger rejected(profile);check(rejected.enter(root,source)==cp::status::ok,"rejection entry");
        auto buffer=storage.enter(bytes,cp::observer_site_bytes);auto full=storage.current_storage();
        check(rejected.bind_observer_storage(root,full)==cp::status::ok,"rejection bind");publish(full,1,false,11);
        auto payload=buffer.subspan(3*cp::observer_local_metadata_bytes);
        if(bad==0)buffer[5]=std::byte{2};
        else if(bad==1)payload[2*cp::native_slot_bytes]=std::byte{1}; // unset nonnullable carrier must be entirely zero
        else if(bad==2) { auto invalid=uwvm2::object::global::make_wasm_i31_reference(7);invalid.kind=uwvm2::object::global::wasm_ref_kind::wasm_func;std::memcpy(payload.data()+3*cp::native_slot_bytes,&invalid,sizeof(invalid)); }
        else if(bad==3) { std::memset(payload.data()+3*cp::native_slot_bytes,0,cp::native_slot_bytes); }
        else { std::uint64_t invalid=3;std::memcpy(full.data(),&invalid,sizeof(invalid)); }
        unsigned visits{};auto result=rejected.visit_recorded_native_references([&](auto){++visits;return true;});
        check(result!=cp::status::ok && visits==0,"entire packet validated before any GC callback");
        check(!rejected.has_complete_materialized_frames(),"malformed packet grants no completeness");
        static_cast<void>(rejected.frames_while_actually_stopped());check(rejected.failure()!=cp::status::ok,"cold failure sticky");
        rejected.unbind_observer_storage(full.data());check(storage.leave(reinterpret_cast<std::uintptr_t>(buffer.data())),"poisoned borrow discarded before release");
        static_cast<void>(rejected.frames_while_actually_stopped());
    }
    std::puts("PASS typed observer slots: 2000 complete carrier/flag transitions, live parent/child census, five pre-callback rejection cases");
}

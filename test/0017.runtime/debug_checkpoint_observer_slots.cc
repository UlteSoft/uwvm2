// Activation-owned DATA regression; no binding grants native/GC/resume rights.
#include <uwvm2/runtime/checkpoint/shadow_ledger.h>
#include <uwvm2/runtime/lib/uwvm_runtime_debug_operand_workspace.h>
#include <array>
#include <cstdio>
#include <cstdlib>
namespace cp=uwvm2::runtime::checkpoint;
using storage_stack=uwvm2::runtime::lib::details::debug_operand_workspace_stack;
static void check(bool ok,char const* message)
{ if(!ok) { std::fprintf(stderr,"observer slots: %s\n",message); std::abort(); } }
static auto plan(cp::compilation_profile::owner profile)
{
    cp::function_plan p{}; p.profile=std::move(profile); p.expression_bytes=32; p.function_generation=1;
    cp::safepoint_layout s{}; s.identifier=1; s.local_count=9;
    for(unsigned i{};i!=9;++i) { s.slots.push_back({{cp::types::value_kind::i64},true}); }
    cp::control_layout c{}; c.end_offset=31; s.controls.push_back(c); p.sites.push_back(s);
    s.identifier=2; s.opcode_offset=3; s.phase=cp::frame_phase::awaiting_call_return; s.caller_return_offset=5; p.sites.push_back(s);
    s.identifier=3; s.opcode_offset=5; s.phase=cp::frame_phase::before_opcode; s.operand_count=1;
    s.slots.push_back({{cp::types::value_kind::v128},true}); p.sites.push_back(s);
    return cp::sealed_function_plan::seal_compiler_metadata(std::move(p));
}
static void publish(std::span<std::byte> storage,std::uint64_t site,std::byte pattern)
{
    auto const prefix=cp::observer_site_bytes+9*cp::observer_local_metadata_bytes;
    for(std::size_t i{};i!=9;++i) { storage[cp::observer_site_bytes+9+i]=std::byte{1}; }
    std::fill(storage.begin()+prefix,storage.end(),pattern);
    std::memcpy(storage.data(),&site,sizeof(site));
}
int main()
{
    auto profile=cp::compilation_profile::create_for_trusted_observer(); auto source=plan(profile); check(bool(source),"sealed plan");
    constexpr auto bytes=9*cp::observer_local_metadata_bytes+10*cp::native_slot_bytes;
    storage_stack storage{}; cp::shadow_ledger ledger(profile);
    std::array<std::span<std::byte>,80> owned{}; std::array<cp::activation_identity,80> identities{};
    for(std::size_t depth{};depth!=owned.size();++depth)
    {
        auto id=cp::activation_identity{depth+1,depth,depth+1,1}; identities[depth]=id;
        check(ledger.enter(id,source)==cp::status::ok,"child accepts exact deferred awaiting parent");
        auto workspace=storage.enter(bytes,cp::observer_site_bytes); owned[depth]=storage.current_storage();
        check(workspace.size()==bytes && owned[depth].size()==bytes+cp::observer_site_bytes,"bounded site prefix");
        check(ledger.bind_observer_storage(id,owned[depth])==cp::status::ok,"bind actual data allocation");
        publish(owned[depth],2,static_cast<std::byte>(depth+17));
        auto payload=workspace.subspan(9*cp::observer_local_metadata_bytes,9*cp::native_slot_bytes);
        auto flags=std::span<std::uint8_t const>{reinterpret_cast<std::uint8_t const*>(workspace.data()+9),9};
        check(ledger.compiler_observer_packet(id,2,payload,flags),"exact bound numeric packet needs no detached copy");
        check(!ledger.compiler_observer_packet(id,3,payload,flags) &&
            !ledger.compiler_observer_packet(id,2,payload.subspan(1),flags),"foreign shape or range cannot bypass copy");
    }
    check(ledger.has_complete_materialized_frames(),"deferred slots complete without typed expansion");
    unsigned roots{};
    check(ledger.visit_recorded_native_references([&](auto){++roots;return true;})==cp::status::ok && roots==0,"numeric current slots grant no reference roots");
    auto frames=ledger.frames_while_actually_stopped(); check(frames.size()==80,"deep cold expansion");
    for(std::size_t d{};d!=frames.size();++d)
    {
        check(frames[d].site==2 && frames[d].values.size()==9,"exact waiting site and declarations");
        for(auto const& value:frames[d].values) for(auto byte:value.bits)
        { check(byte==static_cast<std::byte>(d+17),"all bytes survive workspace-stack vector growth"); }
    }
    for(std::size_t d{owned.size()};d!=0;--d)
    {
        publish(owned[d-1],3,std::byte{0xa5});
        auto current=ledger.frames_while_actually_stopped().back();
        check(current.site==3 && current.values.size()==10 && current.values.back().declaration.type.kind==cp::types::value_kind::v128,"latest returned shape replaces cold copy");
        ledger.unbind_observer_storage(owned[d-1].data());
        check(storage.leave(reinterpret_cast<std::uintptr_t>(owned[d-1].data()+cp::observer_site_bytes)),"storage retires after unbind");
        check(ledger.leave(identities[d-1],false)==cp::status::ok,"exact activation retires");
    }
    check(storage.size()==0 && ledger.size()==0,"no dangling activation borrows");
    for(unsigned bad{};bad!=4;++bad)
    {
        cp::shadow_ledger rejected(profile); auto id=cp::activation_identity{1,0,1,1}; check(rejected.enter(id,source)==cp::status::ok,"rejection entry");
        auto workspace=storage.enter(bytes,cp::observer_site_bytes); auto full=storage.current_storage();
        check(rejected.bind_observer_storage(id,full)==cp::status::ok,"rejection binding"); publish(full,3,std::byte{0x73});
        if(bad<2) { full[cp::observer_site_bytes+9+8]=static_cast<std::byte>(bad ? 2:0); }
        else { std::uint64_t forged=bad==2 ? 4:UINT64_MAX; std::memcpy(full.data(),&forged,sizeof(forged)); }
        check(!rejected.has_complete_materialized_frames(),"malformed marker/flag cannot claim completeness");
        check(rejected.visit_recorded_native_references([](auto){return true;})==cp::status::invalid_layout,"malformed packet cannot hide roots");
        static_cast<void>(rejected.frames_while_actually_stopped()); check(rejected.failure()==cp::status::invalid_layout,"cold failure sticky");
        rejected.unbind_observer_storage(full.data()); check(storage.leave(reinterpret_cast<std::uintptr_t>(workspace.data())),"poisoned data storage still retires");
        static_cast<void>(rejected.frames_while_actually_stopped()); // No read of released storage.
    }
    check(ledger.executable_restore_capability()==cp::status::unavailable_resume,"slots never grant restore");
    std::puts("observer slots DATA PASS");
}

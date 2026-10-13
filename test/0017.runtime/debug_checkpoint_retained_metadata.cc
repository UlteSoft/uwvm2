// Scope-local immutable metadata DATA. No cached owner grants activation,
// native memory, root, code publication or continuation permission.
#include <uwvm2/runtime/checkpoint/shadow_ledger.h>
#include <uwvm2/runtime/lib/uwvm_runtime_debug_operand_workspace.h>
#include <cstdio>
#include <cstdlib>
namespace cp=uwvm2::runtime::checkpoint;
static void check(bool okay,char const* what)
{ if(!okay) { std::fprintf(stderr,"retained metadata: %s\n",what);std::abort(); } }
static auto source(cp::compilation_profile::owner const& profile,bool wide=false,bool oversized=false)
{
    cp::function_plan p{};p.profile=profile;p.expression_bytes=16;p.function_generation=1;
    if(oversized) { p.sites.reserve(10000u); }
    cp::safepoint_layout site{};site.identifier=1;site.local_count=1;
    site.slots.push_back({{wide ? cp::types::value_kind::i64 : cp::types::value_kind::i32},true});
    cp::control_layout control{};control.end_offset=15;site.controls.push_back(control);p.sites.push_back(site);
    site.identifier=2;site.opcode_offset=2;site.caller_return_offset=4;site.phase=cp::frame_phase::awaiting_call_return;
    p.sites.push_back(site);return cp::sealed_function_plan::seal_compiler_metadata(std::move(p));
}
int main()
{
    auto profile=cp::compilation_profile::create_for_trusted_observer();auto plan=source(profile);
    check(plan && plan->observer_retention_bytes()!=0 && plan->observer_retention_bytes()<=16384u,"complete bounded payload accounting");
    auto different=source(profile,true);std::array<std::byte,cp::native_slot_bytes> payload{};payload[0]=std::byte{42};
    {
        cp::shadow_ledger ledger(profile);
        for(std::uint64_t i=1;i<=2000;++i)
        {
            auto id=cp::activation_identity{i,0,i,1};
            check(ledger.compiler_enter_observer(id,plan)==cp::status::ok,"actual new identity admitted");
            check(plan.use_count()==2,"repeated exact owner is reused without another strong copy");
            auto frames=ledger.frames_while_actually_stopped();
            check(frames.size()==1 && frames[0].identity==id && frames[0].values.empty() && !frames[0].materialized,
                "each reused slot starts with fresh identity and no prior state");
            check(ledger.materialize(id,1,payload)==cp::status::ok && ledger.leave(id,false)==cp::status::ok,"exact fresh materialization and return");
            check(plan.use_count()==2 && ledger.size()==0 && ledger.frames_while_actually_stopped().empty() &&
                ledger.compiler_top_plan(id)==nullptr,"inactive metadata has no state or live view");
            unsigned roots{};check(ledger.visit_recorded_native_references([&](auto){++roots;return true;})==cp::status::ok && roots==0,
                "retained metadata contributes no recorded roots");
        }
        auto id=cp::activation_identity{3001,0,3001,2};
        check(ledger.compiler_enter_observer(id,different)==cp::status::ok && plan.use_count()==1 && different.use_count()==2,
            "different metadata releases previous owner and replaces declarations");
        check(ledger.materialize(id,1,payload)==cp::status::ok &&
            ledger.frames_while_actually_stopped()[0].values[0].declaration.type.kind==cp::types::value_kind::i64,
            "previous i32 declaration cannot survive replacement");
        check(ledger.leave(id,false)==cp::status::ok,"replacement retires");
        cp::sealed_function_plan::owner alias{different.get(),[](auto const*){}};
        id={3002,0,3002,2};
        check(ledger.compiler_enter_observer(id,alias)==cp::status::ok && different.use_count()==1 && alias.use_count()==2,
            "equal pointer with another control block replaces rather than reuses owner DATA");
        check(ledger.leave(id,false)==cp::status::ok,"different control block retires");
        ledger.poison(cp::status::invalid_activation);check(ledger.discard_failed_recording() && alias.use_count()==1,
            "sticky failure discard releases every retained metadata owner");
        check(ledger.compiler_enter_observer({3003,0,3003,2},plan)==cp::status::invalid_activation,
            "discard cannot revive failed recording");
    }
    check(plan.use_count()==1 && different.use_count()==1,"scope destruction releases cache");
    {
        cp::shadow_ledger ledger(profile);std::array<cp::activation_identity,80> identities{};
        for(std::size_t i=0;i<identities.size();++i)
        {
            auto id=cp::activation_identity{i+1,i,i+1,1};identities[i]=id;
            check(ledger.compiler_enter_observer(id,plan)==cp::status::ok && ledger.materialize(id,2,payload)==cp::status::ok,
                "deep exact awaiting caller admission");
        }
        for(std::size_t i=identities.size();i!=0;--i) { check(ledger.leave(identities[i-1],false)==cp::status::ok,"deep retirement"); }
        check(plan.use_count()==33,"inactive owner retention is capped at 32, independent of peak depth");
    }
    check(plan.use_count()==1,"deep cache drops with owning scope");
    auto large=source(profile,false,true);check(large && large->observer_retention_bytes()==0,"oversized allocated capacities are not retained");
    {
        cp::shadow_ledger ledger(profile);auto id=cp::activation_identity{1,0,1,1};
        check(ledger.compiler_enter_observer(id,large)==cp::status::ok && ledger.leave(id,false)==cp::status::ok && large.use_count()==1,
            "large legal plan executes and releases immediately");
    }
    {
        cp::shadow_ledger ledger(profile);auto id=cp::activation_identity{1,0,1,1};
        check(ledger.enter(id,plan)==cp::status::ok,"generic DATA entry admission");
        // A by-value shared_ptr argument may be destroyed at the end of the
        // full expression; inspect retirement after that expression completes.
        check(ledger.leave(id,false)==cp::status::ok && plan.use_count()==1,
            "generic DATA entry preserves immediate owner retirement");
        uwvm2::runtime::lib::details::debug_operand_workspace_stack storage;
        auto workspace=storage.enter(cp::observer_local_metadata_bytes+cp::native_slot_bytes,cp::observer_site_bytes);
        check(ledger.compiler_enter_observer({2,0,2,1},plan)==cp::status::ok &&
            ledger.bind_observer_storage({2,0,2,1},storage.current_storage())==cp::status::ok,"real bounded caller storage bound");
        workspace[1]=std::byte{2};std::uint64_t awaiting=2;
        std::memcpy(storage.current_storage().data(),&awaiting,sizeof(awaiting));
        check(ledger.compiler_enter_observer({3,2,3,1},plan)==cp::status::invalid_layout && ledger.size()==1,
            "cached metadata cannot skip current caller packet/flag validation");
        ledger.unbind_observer_storage(storage.current_storage().data());
        check(storage.leave(reinterpret_cast<std::uintptr_t>(workspace.data())) && ledger.discard_failed_recording(),"poisoned borrows retire before storage");
    }
    cp::shadow_ledger invalid(cp::compilation_profile::owner{});check(invalid.compiler_enter_observer({1,0,1,1},plan)==cp::status::invalid_profile,
        "invalid profile is rejected before dereference");
    auto resumable=cp::compilation_profile::create_for_trusted_manager();cp::shadow_ledger resume(resumable);
    check(resume.compiler_enter_observer({1,0,1,1},source(resumable))==cp::status::invalid_profile,
        "observer cache cannot select resumable admission");
    std::puts("PASS retained metadata DATA: 2000 fresh admissions, exact control blocks, 80-deep/32-owner bound, oversized capacities, caller validation, discard, no roots or resume");
}

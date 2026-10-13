// Metadata cache DATA checks: no key grants pause, native or restore rights.
#include <uwvm2/runtime/lib/uwvm_runtime_checkpoint_entry_cache.h>
#include <cstdio>
#include <cstdlib>
namespace cp = uwvm2::runtime::checkpoint;
using cache = uwvm2::runtime::lib::details::checkpoint_entry_plan_cache;
static void check(bool ok, char const* message)
{ if(!ok) { std::fprintf(stderr,"entry cache: %s\n",message); std::abort(); } }
static auto make_plan()
{
    cp::function_plan plan{}; plan.profile=cp::compilation_profile::create_for_trusted_observer();
    plan.expression_bytes=1; plan.function_generation=1;
    cp::safepoint_layout site{}; site.identifier=1;
    cp::control_layout control{}; control.end_offset=0; site.controls.push_back(control);
    plan.sites.push_back(site); return cp::sealed_function_plan::seal_compiler_metadata(std::move(plan));
}
int main()
{
    auto plan=make_plan(); check(bool(plan),"sealed metadata");
    int record{},publication{},other{}; cache memo{};
    cache::key key{&record,&publication,1,0,7,1,16,32};
    check(!memo.find(key,plan),"empty cache"); memo.remember_validated(key,plan);
    check(memo.find(key,plan),"same actual binding and control block");
    for(unsigned field{};field!=8;++field)
    {
        auto changed=key;
        switch(field)
        {
        case 0:changed.record=&other;break;case 1:changed.publication=&other;break;
        case 2:++changed.epoch;break;case 3:++changed.module;break;case 4:changed.function+=cache::entry_count;break;
        case 5:++changed.generation;break;case 6:++changed.raw_entry;break;case 7:++changed.typed_entry;break;
        }
        check(!memo.find(changed,plan),"every actual binding field invalidates");
    }
    auto different=make_plan(); check(!memo.find(key,different),"replaced plan");
    cp::sealed_function_plan::owner alias{plan.get(),[](auto const*){}};
    check(!memo.find(key,alias),"same address with a different control block");
    auto collision=key; collision.function+=cache::entry_count; memo.remember_validated(collision,different);
    check(!memo.find(key,plan) && memo.find(collision,different),"collision replaces only metadata row");
    cache fresh{}; check(!fresh.find(collision,different),"new execution scope has no inherited binding");
    auto invalid=key;invalid.epoch=0;fresh.remember_validated(invalid,plan);
    check(!fresh.find(invalid,plan),"zero epoch is never admitted");
    {
        cache targets{};
        for(std::uint64_t i{};i!=256u;++i)
        {
            auto actual=key;actual.function=i;actual.raw_entry=16u*(i+1u);actual.typed_entry=32u*(i+1u);
            targets.remember_validated(actual,plan);
        }
        for(std::uint64_t i{};i!=256u;++i)
        {
            auto actual=key;actual.function=i;actual.raw_entry=16u*(i+1u);actual.typed_entry=32u*(i+1u);
            check(targets.find(actual,plan),"all 256 round-robin bindings survive the complete target cycle");
        }
    }
    {
        cache modules{};auto other_module=key;++other_module.module;
        modules.remember_validated(key,plan);modules.remember_validated(other_module,different);
        check(!modules.find(key,plan) && modules.find(other_module,different),
            "a module collision must miss rather than reuse another module's metadata");
    }
    check(sizeof(cache)<=32768u,"fixed cache DATA storage stays within 32 KiB");
    std::puts("checkpoint entry cache epoch/publication/entries/control-block/collision/256-target/module DATA PASS");
}

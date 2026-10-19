// Standalone header cold allocation failure. Owned DATA only, never capture authority.
#include <uwvm2/runtime/checkpoint/shadow_ledger.h>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <source_location>
static volatile bool reject_next_allocation{};
void* operator new(std::size_t bytes)
{
    if(reject_next_allocation) { reject_next_allocation=false; throw std::bad_alloc{}; }
    if(auto p=std::malloc(bytes ? bytes : 1u)) { return p; }
    throw std::bad_alloc{};
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer,std::size_t) noexcept { std::free(pointer); }
static void check(bool okay,std::source_location where=std::source_location::current())
{ if(!okay) { std::fprintf(stderr,"cold allocation failure check: %u\n",where.line());std::abort(); } }
int main()
{
    namespace cp=uwvm2::runtime::checkpoint;
    auto profile=cp::compilation_profile::create_for_trusted_observer();
    cp::function_plan declaration{};declaration.profile=profile;declaration.expression_bytes=32;declaration.function_generation=1;
    cp::safepoint_layout site{};site.identifier=1;site.local_count=1;
    site.slots.push_back({{cp::types::value_kind::i64},true});
    cp::control_layout control{};control.end_offset=31;site.controls.push_back(control);declaration.sites.push_back(site);
    auto plan=cp::sealed_function_plan::seal_compiler_metadata(std::move(declaration));check(bool(plan));
    std::array<std::byte,cp::observer_site_bytes+cp::observer_local_metadata_bytes+cp::native_slot_bytes> storage{};
    std::uint64_t identifier=1;
    for(bool failing:{false,true})
    {
        cp::shadow_ledger ledger(profile);cp::activation_identity identity{1,0,1,1};
        check(ledger.compiler_enter_observer(identity,plan)==cp::status::ok);
        // The allocation binds empty; only the genuine producer then publishes.
        storage.fill(std::byte{});
        check(ledger.bind_observer_storage(identity,storage)==cp::status::ok);
        std::memcpy(storage.data(),&identifier,sizeof(identifier));
        storage[cp::observer_site_bytes+1]=std::byte{1};
        storage[cp::observer_site_bytes+cp::observer_local_metadata_bytes]=std::byte{42};
        reject_next_allocation=failing;
        auto frames=ledger.frames_while_actually_stopped();reject_next_allocation=false;
        if(failing)
        {
            check(frames.empty() && ledger.failure()==cp::status::quota_exceeded);
            check(!ledger.has_complete_materialized_frames() && ledger.compiler_top_plan(identity)==nullptr);
            check(ledger.discard_failed_recording() && ledger.frames_while_actually_stopped().empty());
            check(ledger.compiler_enter_observer({2,0,2,2},plan)==cp::status::quota_exceeded);
        }
        else
        {
            check(ledger.failure()==cp::status::ok && frames.size()==1 && frames[0].values.size()==1);
            check(frames[0].values[0].bits[0]==std::byte{42});
            ledger.unbind_observer_storage(storage.data());check(ledger.leave(identity,false)==cp::status::ok);
        }
    }
    std::puts("cold allocation failure DATA PASS");
}

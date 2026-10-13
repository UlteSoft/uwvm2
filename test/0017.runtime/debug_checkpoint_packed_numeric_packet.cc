// Owned packet DATA only: no fabricated native memory, GC or resume authority.
#include <uwvm2/runtime/checkpoint/shadow_ledger.h>
#include <cstdio>
#include <cstdlib>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif
namespace cp = uwvm2::runtime::checkpoint;
static void check(bool value, char const* message)
{ if(!value) { std::fprintf(stderr,"FAIL packed packet: %s\n",message); std::abort(); } }
static cp::sealed_function_plan::owner plan(cp::compilation_profile::owner profile, std::size_t locals = 5)
{
    cp::function_plan source{}; source.profile=std::move(profile); source.expression_bytes=32; source.function_generation=1;
    cp::safepoint_layout site{}; site.identifier=1; site.local_count=locals;
    cp::types::value_kind kinds[]{cp::types::value_kind::i32,cp::types::value_kind::i64,
        cp::types::value_kind::f32,cp::types::value_kind::f64,cp::types::value_kind::v128};
    for(std::size_t i{};i!=locals;++i) { site.slots.push_back({{kinds[i%5]},true}); }
    cp::control_layout control{};control.end_offset=31;site.controls.push_back(control);source.sites.push_back(site);
    site.identifier=2;site.opcode_offset=3;site.phase=cp::frame_phase::awaiting_call_return;site.caller_return_offset=5;
    source.sites.push_back(site);
    site.identifier=3;site.opcode_offset=5;site.phase=cp::frame_phase::before_opcode;site.operand_count=1;
    site.slots.push_back({{cp::types::value_kind::i64},true});source.sites.push_back(site);
    return cp::sealed_function_plan::seal_compiler_metadata(std::move(source));
}
static void same(cp::logical_frame const& frame, cp::sealed_function_plan::owner const& source,
    std::uint64_t site, std::span<std::byte const> expected)
{
    check(frame.materialized && !frame.numeric_packet_pending && frame.site==site,"complete cold expanded frame");
    auto const& slots=source->get().sites[site-1].slots;check(frame.values.size()==slots.size(),"exact current slot count");
    for(std::size_t i{};i!=slots.size();++i)
    {
        check(frame.values[i].declaration==slots[i],"exact sealed numeric types");
        check(std::memcmp(frame.values[i].bits.data(),expected.data()+i*cp::native_slot_bytes,cp::native_slot_bytes)==0,"all sixteen bytes preserved");
    }
}
int main()
{
    auto observer=cp::compilation_profile::create_for_trusted_observer();auto source=plan(observer);check(bool(source),"sealed numeric plan");
    std::array<std::byte,96> payload{};for(std::size_t i{};i!=payload.size();++i)payload[i]=static_cast<std::byte>(i*29u+13u);
    // Includes unrestricted f32/f64 signaling NaNs and every v128 byte; this
    // test compares bytes rather than performing floating-point arithmetic.
    std::uint32_t const snan32{0x7f812345u};std::uint64_t const snan64{0x7ff0123456789abcu};
    std::memcpy(payload.data()+32,&snan32,sizeof(snan32));std::memcpy(payload.data()+48,&snan64,sizeof(snan64));
    auto expected=payload;std::array<std::uint8_t,5> flags{1,1,1,1,1};
    cp::activation_identity outer{1,0,1,1},inner{2,1,2,1};
    cp::shadow_ledger ledger(observer);check(ledger.enter(outer,source)==cp::status::ok,"outer entry");
    check(ledger.materialize_compiler_native_data(outer,source,2,std::span{payload}.first(80),flags,true)==cp::status::ok,"owned awaiting-call packet");
    check(ledger.enter(inner,source)==cp::status::ok,"pending packed caller supports genuine child entry");
    payload.fill(std::byte{0x77});auto inner_expected=payload;
    check(ledger.materialize_compiler_native_data(inner,source,3,payload,flags,true)==cp::status::ok,"child owned complete numeric operands");
    check(ledger.has_complete_materialized_frames(),"pending values count as fully captured data");
    unsigned visits{};
    check(ledger.visit_recorded_native_references([&](cp::native_reference){++visits;return true;})==cp::status::ok && visits==0,"GC does not expand or infer reference roots from numeric bits");
    payload.fill(std::byte{0xee});flags.fill(2);
    auto frames=ledger.frames_while_actually_stopped();check(frames.size()==2,"cold nested expansion");
    same(frames[0],source,2,std::span{expected}.first(80));same(frames[1],source,3,inner_expected);
    check(ledger.leave(inner,false)==cp::status::ok,"child retires exact LIFO identity");
    // Fixed producer packets also own their bytes before native storage reuse.
    check(ledger.materialize(outer,1,std::span{expected}.first(80),true)==cp::status::ok,"fixed small numeric capture");
    same(ledger.frames_while_actually_stopped()[0],source,1,std::span{expected}.first(80));
    check(ledger.executable_restore_capability()==cp::status::unavailable_resume,"packing never mints resume rights");
    check(ledger.leave(outer,false)==cp::status::ok && ledger.size()==0,"no stale packet after native retirement");
    // Repeated cold-less replacement changes extents and bytes. Only the final
    // complete owned packet may appear; never retain stale operand cells.
    check(ledger.enter(outer,source)==cp::status::ok,"recycled entry");flags.fill(1);
    for(unsigned i{};i!=1000;++i)
    {
        auto site=i%2 ? 1u:3u;payload.fill(static_cast<std::byte>(i));
        check(ledger.materialize_compiler_native_data(outer,source,site,std::span{payload}.first(site==1?80:96),flags,true)==cp::status::ok,"overwrite exact numeric packet");
    }
    expected=payload;payload.fill(std::byte{});
    same(ledger.frames_while_actually_stopped()[0],source,1,std::span{expected}.first(80));
    // Reject all malformed flags/extents/sites BEFORE committing over a valid
    // pending packet. Sticky failure cannot lend a new producer plan.
    for(unsigned bad{};bad!=5;++bad)
    {
        cp::shadow_ledger rejected(observer);check(rejected.enter(outer,source)==cp::status::ok,"rejection entry");flags.fill(1);
        check(rejected.materialize_compiler_native_data(outer,source,3,expected,flags,true)==cp::status::ok,"valid pending predecessor");
        payload.fill(std::byte{0x99});auto site=3u;std::size_t bytes=96,flag_count=5;
        if(bad==0)flags[4]=2;else if(bad==1)flags[4]=0;else if(bad==2)bytes=80;else if(bad==3)flag_count=4;else site=0;
        check(rejected.materialize_compiler_native_data(outer,source,site,std::span{payload}.first(bytes),std::span{flags}.first(flag_count),true)!=cp::status::ok,"invalid packet rejected");
        same(rejected.frames_while_actually_stopped()[0],source,3,expected);
        check(rejected.compiler_top_plan(outer)==nullptr && !rejected.has_complete_materialized_frames(),"failure remains sticky");
    }
    // Replacing a pending packet, inspecting it, committing eagerly, then
    // returning to packing must keep the latest exact count/type/bytes. This
    // includes empty locals and an operand disappearing at the next opcode.
    for(std::size_t count:{0u,5u,7u})
    {
        auto changing=plan(observer,count);check(bool(changing),"changing extent plan");
        cp::shadow_ledger transitions(observer);
        std::array<std::byte,128> bytes{};std::array<std::uint8_t,7> initialized{};initialized.fill(1);
        for(unsigned round{};round!=16;++round)
        {
            check(transitions.enter(outer,changing)==cp::status::ok,"transition fresh activation");
            bytes.fill(static_cast<std::byte>(round+17));
            check(transitions.materialize_compiler_native_data(outer,changing,3,std::span{bytes}.first((count+1)*16),std::span{initialized}.first(count),true)==cp::status::ok,"pending packet with operand");
            bytes.fill(static_cast<std::byte>(round+83));auto saved=bytes;
            check(transitions.materialize_compiler_native_data(outer,changing,1,std::span{bytes}.first(count*16),std::span{initialized}.first(count),true)==cp::status::ok,"pending replacement drops operand");
            bytes.fill(std::byte{0xff});
            same(transitions.frames_while_actually_stopped()[0],changing,1,std::span{saved}.first(count*16));
            bytes.fill(static_cast<std::byte>(round+149));saved=bytes;
            check(transitions.materialize(outer,3,std::span{bytes}.first((count+1)*16),false)==cp::status::ok,"eager replacement after inspection");
            bytes.fill(std::byte{});
            same(transitions.frames_while_actually_stopped()[0],changing,3,std::span{saved}.first((count+1)*16));
            bytes.fill(static_cast<std::byte>(round+211));saved=bytes;
            check(transitions.materialize_compiler_native_data(outer,changing,1,std::span{bytes}.first(count*16),std::span{initialized}.first(count),true)==cp::status::ok,"packed replacement clears old decoded values");
            bytes.fill(std::byte{});
            same(transitions.frames_while_actually_stopped()[0],changing,1,std::span{saved}.first(count*16));
            check(transitions.leave(outer,false)==cp::status::ok && transitions.size()==0,"transition retirement drops all captured state");
        }
    }
    // Resumable purpose and larger packets deliberately retain the complete
    // synchronous decoder. A caller's defer hint cannot change that policy.
    for(bool resumable:{false,true})
    {
        auto profile=resumable?cp::compilation_profile::create_for_trusted_manager({}):observer;
        auto large=plan(profile,resumable?5:9);check(bool(large),"fallback sealed plan");cp::shadow_ledger eager(profile);
        check(eager.enter(outer,large)==cp::status::ok,"fallback entry");
        std::array<std::byte,160> storage{};std::array<std::uint8_t,9> availability{};availability.fill(1);
        check(eager.materialize_compiler_native_data(outer,large,1,std::span{storage}.first((resumable?5:9)*16),std::span{availability}.first(resumable?5:9),true)==cp::status::ok,"fallback current decode");
        auto const decoded=eager.frames_while_actually_stopped();check(decoded[0].values.size()==(resumable?5u:9u),"fallback full state available");
    }
    for(std::size_t count:{0u,8u})
    {
        auto bounded=plan(observer,count);check(bool(bounded),"zero/exact upper bound plan");cp::shadow_ledger edge(observer);
        check(edge.enter(outer,bounded)==cp::status::ok,"boundary entry");
        std::array<std::byte,128> bytes{};bytes.fill(std::byte{0xd3});auto saved=bytes;
        std::array<std::uint8_t,8> initialized{};initialized.fill(1);
        check(edge.materialize_compiler_native_data(outer,bounded,1,std::span{bytes}.first(count*16),std::span{initialized}.first(count),true)==cp::status::ok,"empty/exact buffer capture");
        bytes.fill(std::byte{});same(edge.frames_while_actually_stopped()[0],bounded,1,std::span{saved}.first(count*16));
    }
    // Every short packet extent, every producer alignment, both fixed and
    // dynamic producers. Overwrite native storage before inspecting the copy.
    for(std::size_t count{};count<=8;++count)
    for(std::size_t alignment{};alignment!=16;++alignment)
    for(bool dynamic:{false,true})
    {
        auto const bounded=plan(observer,count);check(bool(bounded),"copy extent plan");
        std::array<std::byte,144> storage{};
        auto actual=std::span{storage}.subspan(alignment,count*16);
        for(std::size_t i{};i!=actual.size();++i)actual[i]=static_cast<std::byte>(i*37u+alignment+count);
        std::array<std::byte,128> saved{};std::copy(actual.begin(),actual.end(),saved.begin());
        std::array<std::uint8_t,8> initialized{};initialized.fill(1);
        cp::shadow_ledger copied(observer);check(copied.enter(outer,bounded)==cp::status::ok,"unaligned copy entry");
        auto result=dynamic?copied.materialize_compiler_native_data(outer,bounded,1,actual,std::span{initialized}.first(count),true):
            copied.materialize(outer,1,actual,true);
        check(result==cp::status::ok,"every exact short extent/alignment captured");
        storage.fill(std::byte{0xee});
        same(copied.frames_while_actually_stopped()[0],bounded,1,std::span{saved}.first(count*16));
    }
    // Every short flag length, every position and every invalid byte value.
    // Both eager and packed paths must reject before replacing owned state.
    for(std::size_t count=1;count<=9;++count)
    {
        auto bounded=plan(observer,count);check(bool(bounded),"short flag plan");
        std::array<std::byte,144> saved{};saved.fill(std::byte{0x6d});
        for(bool deferred:{false,true})
        {
            cp::shadow_ledger valid(observer);check(valid.enter(outer,bounded)==cp::status::ok,"valid short flags entry");
            std::array<std::uint8_t,9> flags_exact{};flags_exact.fill(1);
            auto const* borrowed=valid.compiler_top_plan(outer);check(borrowed!=nullptr,"exact plan reference borrow");
            check(valid.materialize_compiler_native_data(outer,*borrowed,1,std::span{saved}.first(count*16),std::span{flags_exact}.first(count),deferred)==cp::status::ok,"all short lengths exact-owner fast path");
            same(valid.frames_while_actually_stopped()[0],bounded,1,std::span{saved}.first(count*16));
            // Equal pointee with another control block is not the same plan.
            cp::sealed_function_plan::owner alias(bounded.get(),[](auto const*){});
            check(valid.materialize_compiler_native_data(outer,alias,1,std::span{saved}.first(count*16),std::span{flags_exact}.first(count),deferred)==cp::status::invalid_plan,"foreign control block rejected");
            for(std::size_t position{};position!=count;++position)
            for(unsigned byte{};byte!=256;++byte)
            {
                if(byte==1)continue;
                cp::shadow_ledger rejected(observer);check(rejected.enter(outer,bounded)==cp::status::ok,"flag rejection entry");
                flags_exact.fill(1);
                check(rejected.materialize_compiler_native_data(outer,bounded,1,std::span{saved}.first(count*16),std::span{flags_exact}.first(count),deferred)==cp::status::ok,"valid short predecessor");
                flags_exact[position]=static_cast<std::uint8_t>(byte);
                check(rejected.materialize_compiler_native_data(outer,bounded,1,std::span{saved}.first(count*16),std::span{flags_exact}.first(count),deferred)==cp::status::invalid_layout,"every non-one flag rejected");
                same(rejected.frames_while_actually_stopped()[0],bounded,1,std::span{saved}.first(count*16));
            }
        }
    }
#if defined(__unix__) || defined(__APPLE__)
    // Place the exact flag range immediately before an unreadable page. Even
    // a one-byte overread by a widened short load would be a real fault here.
    auto const page=static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));check(page>=4096,"native page size");
    auto const mapping=::mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    check(mapping!=MAP_FAILED,"flag boundary mapping");
    auto const base=static_cast<std::uint8_t*>(mapping);
    check(::mprotect(base+page,page,PROT_NONE)==0,"unreadable flag boundary");
    for(std::size_t count{};count<=9;++count)
    for(bool deferred:{false,true})
    {
        auto const bounded=plan(observer,count);check(bool(bounded),"guarded sealed plan");
        std::span<std::uint8_t> boundary{base+page-count,count};
        for(auto& flag:boundary)flag=1;
        std::array<std::byte,144> bytes{};
        cp::shadow_ledger guarded(observer);check(guarded.enter(outer,bounded)==cp::status::ok,"guarded entry");
        check(guarded.materialize_compiler_native_data(outer,bounded,1,std::span{bytes}.first(count*16),boundary,deferred)==cp::status::ok,"exact flags before unreadable page");
        if(count)
        {
            boundary.back()=2;
            check(guarded.materialize_compiler_native_data(outer,bounded,1,std::span{bytes}.first(count*16),boundary,deferred)==cp::status::invalid_layout,"invalid final byte at protected boundary");
        }
    }
    // Readable payload ends exactly at PROT_NONE. The zero-length case also
    // proves that copying an empty packet does not touch an unreadable pointer.
    for(std::size_t count{};count<=8;++count)
    for(bool dynamic:{false,true})
    {
        auto const bounded=plan(observer,count);check(bool(bounded),"protected payload plan");
        std::span<std::byte> actual{reinterpret_cast<std::byte*>(base+page-count*16),count*16};
        for(std::size_t i{};i!=actual.size();++i)actual[i]=static_cast<std::byte>(i*19u+count);
        std::array<std::byte,128> saved{};std::copy(actual.begin(),actual.end(),saved.begin());
        std::array<std::uint8_t,8> initialized{};initialized.fill(1);
        cp::shadow_ledger guarded(observer);check(guarded.enter(outer,bounded)==cp::status::ok,"protected payload entry");
        auto result=dynamic?guarded.materialize_compiler_native_data(outer,bounded,1,actual,std::span{initialized}.first(count),true):
            guarded.materialize(outer,1,actual,true);
        check(result==cp::status::ok,"no short payload overread");
        for(auto& byte:actual)byte=std::byte{0xee};
        same(guarded.frames_while_actually_stopped()[0],bounded,1,std::span{saved}.first(count*16));
    }
    check(::munmap(mapping,page*2)==0,"release flag mapping");
#endif
    std::puts("PASS packed numeric ownership, nested caller, exact 16-byte values, replacement, rejection atomicity, GC and fallback policies");
}

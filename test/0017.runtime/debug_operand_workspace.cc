// Native storage DATA regression; no copied ID/PC/owner grants preview.
#include <uwvm2/runtime/lib/uwvm_runtime_debug_operand_workspace.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <fast_io.h>
#include <array>
#include <limits>
namespace lib = ::uwvm2::runtime::lib::details;
namespace cp = ::uwvm2::runtime::checkpoint;
static void require(bool valid,char const* message)
{ if(!valid) { ::fast_io::io::perrln("debug operand workspace: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
int main()
{
    require(!cp::native_workspace_fits(1001u,1003u) && cp::observer_workspace_fits(1001u,1003u),
        "heap observation preserves native resume workspace limit");
    require(!cp::observer_workspace_fits((::std::numeric_limits<::std::size_t>::max)(),
        (::std::numeric_limits<::std::size_t>::max)()),"flags and slot extent overflow rejected");
    lib::debug_operand_workspace_stack storage{};
    require(storage.size()==0u && storage.enter(0u).empty() &&
        storage.enter((::std::numeric_limits<::std::size_t>::max)()).empty(),"empty/overflow allocation rejected without ownership change");
    ::std::array<::std::byte*,80u> addresses{};
    constexpr ::std::size_t bytes{10001u*2u + 10003u*cp::native_slot_bytes};
    for(::std::size_t n{}; n!=addresses.size(); ++n)
    {
        auto const workspace{storage.enter(bytes)};
        require(workspace.size()==bytes && workspace.front()==::std::byte{} && workspace.back()==::std::byte{},"full heap extent initialized");
        addresses[n]=workspace.data();workspace.back()=::std::byte{static_cast<unsigned char>(n+1u)};
        for(::std::size_t i{}; i<=n; ++i)
        { require(addresses[i][bytes-1u]==::std::byte{static_cast<unsigned char>(i+1u)},"nested frame vector growth retains each actual heap buffer/value"); }
    }
    require(!storage.leave(0u) && !storage.leave(reinterpret_cast<::std::uintptr_t>(addresses[0u])) && storage.size()==80u,
        "wrong or non-top owner cannot retire any buffer");
    // Storage cleanup stays possible after diagnostic identity quotas; the
    // data-only class cannot grant a complete Wasm capture at that depth.
    for(::std::size_t n{addresses.size()};n!=0u;--n)
    { require(storage.leave(reinterpret_cast<::std::uintptr_t>(addresses[n-1u])) && storage.size()==n-1u,"return/tail/EH producer pops the exact top native buffer"); }
    require(!storage.leave(reinterpret_cast<::std::uintptr_t>(addresses[0u])),"retired owner cannot release twice");
    auto const fresh{storage.enter(1u)};require(fresh.size()==1u && storage.leave(reinterpret_cast<::std::uintptr_t>(fresh.data())),"zero-slot function still owns one unique buffer");
    // Real flags/payload MUST start cleared after reuse, including a smaller
    // function followed by a larger function and rejected non-top retirement.
    for(::std::size_t n{};n!=128u;++n)
    {
        auto const reused{storage.enter(n%2u ? 321u : 129u)};
        for(auto byte:reused) { require(byte==::std::byte{},"recycled storage cannot preserve a previous local flag/reference/value"); }
        for(auto& byte:reused) { byte=::std::byte{0xffu}; }
        auto const inner{storage.enter(97u)};
        require(!storage.leave(reinterpret_cast<::std::uintptr_t>(reused.data())),"recycled parent label cannot retire the live child");
        require(storage.leave(reinterpret_cast<::std::uintptr_t>(inner.data())) &&
            storage.leave(reinterpret_cast<::std::uintptr_t>(reused.data())) && storage.size()==0u,"recycled exact LIFO cleanup");
    }
    // A real local packet may contain operand slots beyond the local prefix.
    // A retained vector's old address alone must never expose an inactive slot.
    auto const parent{storage.enter(4u+4u*cp::native_slot_bytes)};
    auto const payload{reinterpret_cast<::std::uintptr_t>(parent.data()+4u)};
    parent[2u]=::std::byte{1u};parent[3u]=::std::byte{0u};parent[4u]=::std::byte{42u};
    auto const capture{storage.current_local_snapshot(payload,2u,2u,cp::native_slot_bytes,4u)};
    require(capture.valid && capture.values.size()==4u*cp::native_slot_bytes && capture.flags.size()==2u &&
        capture.flags[0u]==1u && capture.flags[1u]==0u && capture.values[0u]==::std::byte{42u},"complete local flags and extra operand payload");
    require(!storage.current_local_snapshot(payload,2u,1u,cp::native_slot_bytes).valid &&
        !storage.current_local_snapshot(payload,2u,2u,0u).valid &&
        !storage.current_local_snapshot(payload,2u,2u,cp::native_slot_bytes,1u).valid &&
        !storage.current_local_snapshot(payload,2u,2u,cp::native_slot_bytes,5u).valid &&
        !storage.current_local_snapshot(payload,2u,2u,cp::native_slot_bytes,(::std::numeric_limits<::std::size_t>::max)()).valid &&
        !storage.current_local_snapshot(payload,(::std::numeric_limits<::std::size_t>::max)(),2u,cp::native_slot_bytes).valid &&
        !storage.current_local_snapshot(payload+1u,2u,2u,cp::native_slot_bytes).valid,"invalid extents/metadata/labels reject before a pointer is exposed");
    auto const child{storage.enter(97u)};
    require(!storage.current_local_snapshot(payload,2u,2u,cp::native_slot_bytes).valid,"suspended parent storage is not current");
    require(storage.leave(reinterpret_cast<::std::uintptr_t>(child.data())) &&
        storage.current_local_snapshot(payload,2u,2u,cp::native_slot_bytes).valid,"real child return restores parent storage visibility");
    require(storage.leave(reinterpret_cast<::std::uintptr_t>(parent.data())) &&
        !storage.current_local_snapshot(payload,2u,2u,cp::native_slot_bytes).valid,"inactive retained slot cannot expose a previous packet");
    // Cross the retention depth and repeatedly grow the physical metadata.
    // Reuse a bounded inactive pool across more than 32 actual depths;
    // every reactivated byte is zero and active ownership remains exact.
    ::std::array<::std::byte*,40u> shallow{};
    for(::std::size_t wave{};wave!=4u;++wave)
    {
        for(::std::size_t depth{};depth!=shallow.size();++depth)
        {
            auto const frame{storage.enter(513u)};
            shallow[depth]=frame.data();
            for(auto byte:frame) { require(byte==::std::byte{},"every reactivated flag/payload/padding byte starts cleared"); }
            for(auto& byte:frame) { byte=::std::byte{0xa5u}; }
        }
        for(::std::size_t depth{shallow.size()};depth!=0u;--depth)
        { require(storage.leave(reinterpret_cast<::std::uintptr_t>(shallow[depth-1u])) && storage.size()==depth-1u,"active depth excludes retained physical metadata"); }
    }
    // A busy leaf must also reuse capacity with more than 32 live callers.
    // A limit on cached depth would allocate/free on each of these calls.
    ::std::array<::std::byte*,64u> callers{};
    for(auto& address:callers)
    { auto const frame{storage.enter(513u)};address=frame.data();frame.back()=::std::byte{0x7fu}; }
    ::std::byte* leaf_address{};
    for(::std::size_t call{};call!=128u;++call)
    {
        auto const leaf{storage.enter(129u)};
        if(call!=0u) { require(leaf.data()==leaf_address,"deep busy leaf reuses storage within the same fixed inactive-byte quota"); }
        leaf_address=leaf.data();
        for(auto byte:leaf) { require(byte==::std::byte{},"deep reused leaf fully clears flags and payload"); }
        for(auto& byte:leaf) { byte=::std::byte{0xffu}; }
        require(storage.leave(reinterpret_cast<::std::uintptr_t>(leaf.data())) && storage.size()==callers.size(),"busy leaf return retains exactly the actual callers");
        for(auto address:callers) { require(address[512u]==::std::byte{0x7fu},"deep leaf reuse cannot overwrite a live caller"); }
    }
    for(::std::size_t depth{callers.size()};depth!=0u;--depth)
    { require(storage.leave(reinterpret_cast<::std::uintptr_t>(callers[depth-1u])) && storage.size()==depth-1u,"deep callers retire in exact LIFO order"); }
    // Changing a reused depth's private site prefix must neither preserve old
    // flags nor confuse the address used to retire the allocation. Shrink and
    // regrow the complete extent, including bytes hidden before the payload.
    for(::std::size_t call{};call!=256u;++call)
    {
        auto const prefix{call%3u ? 24u : 8u};
        auto const extent{call%2u ? 513u : 65u};
        auto const frame{storage.enter(extent,prefix)};
        auto const full{storage.current_storage()};
        require(storage.size()==1u && full.size()==extent+prefix &&
            full.data()+prefix==frame.data(),"reused depth publishes only the current complete extent and prefix");
        for(auto byte:full) { require(byte==::std::byte{},"site prefix, flags, padding and regrown payload all start zero"); }
        for(auto& byte:full) { byte=::std::byte{0xffu}; }
        require(!storage.leave(reinterpret_cast<::std::uintptr_t>(full.data())) &&
            !storage.leave(reinterpret_cast<::std::uintptr_t>(frame.data()+1u)) && storage.size()==1u,
            "allocation base and interior label cannot retire a prefixed payload");
        require(storage.enter(1u,(::std::numeric_limits<::std::size_t>::max)()).empty() &&
            storage.size()==1u && storage.current_storage().data()==full.data(),"invalid nested extent leaves current ownership intact");
        require(storage.leave(reinterpret_cast<::std::uintptr_t>(frame.data())) && storage.size()==0u &&
            storage.current_storage().empty(),"retained record has no live extent after exact retirement");
    }
    // Retained constructed storage is larger than a later active frame. An
    // apparently in-bounds physical suffix must not extend a current packet.
    {
        auto const large{storage.enter(513u,24u)};
        for(auto& byte:storage.current_storage()) { byte=::std::byte{0xa5u}; }
        require(storage.leave(reinterpret_cast<::std::uintptr_t>(large.data())),"physical suffix setup retires");
        auto const small{storage.enter(4u+2u*cp::native_slot_bytes,8u)};
        auto const base{reinterpret_cast<::std::uintptr_t>(small.data()+4u)};
        require(storage.current_storage().size()==8u+4u+2u*cp::native_slot_bytes &&
            storage.current_local_snapshot(base,2u,2u,cp::native_slot_bytes,2u).valid &&
            !storage.current_local_snapshot(base,2u,2u,cp::native_slot_bytes,3u).valid,
            "physical retained suffix never widens the actual current packet");
        require(storage.leave(reinterpret_cast<::std::uintptr_t>(small.data())) &&
            storage.current_storage().empty() && !storage.current_local_snapshot(base,2u,2u,cp::native_slot_bytes,2u).valid,
            "old payload label has no active extent after retirement");
        auto const grown{storage.enter(2049u,8u)};
        for(auto byte:storage.current_storage()) { require(byte==::std::byte{},"cold growth clears old prefix plus all new bytes"); }
        require(storage.leave(reinterpret_cast<::std::uintptr_t>(grown.data())),"cold growing allocation retires");
    }
    {
        auto const frame{storage.enter(65u,8u)};
        frame.back()=::std::byte{0x7fu};
        lib::debug_operand_workspace_stack moved{::std::move(storage)};
        require(storage.size()==0u && storage.current_storage().empty() && moved.size()==1u &&
            moved.current_storage().data()+8u==frame.data() && frame.back()==::std::byte{0x7fu},
            "move transfers exact active ownership and leaves the old depth inactive");
        auto const fresh{storage.enter(129u,24u)};
        for(auto byte:storage.current_storage()) { require(byte==::std::byte{},"moved-from stack may be reused without stale extent or flags"); }
        require(storage.leave(reinterpret_cast<::std::uintptr_t>(fresh.data())),"moved-from stack remains usable");
        storage=::std::move(moved);
        require(moved.size()==0u && moved.current_storage().empty() && storage.size()==1u &&
            storage.current_storage().data()+8u==frame.data(),"move assignment preserves live payload identity");
        require(storage.leave(reinterpret_cast<::std::uintptr_t>(frame.data())),"transferred exact owner still retires");
    }
    {
        for(::std::size_t bytes:{1u,42u,64u,26u,63u})
        {
            auto const frame{storage.enter_small(bytes)};auto const full{storage.current_storage()};
            require(frame.size()==bytes && full.size()==bytes+8u && full.data()+8u==frame.data(),"small reserve never widens active extent");
            for(auto byte:full)require(byte==::std::byte{},"small entry completely clears site and payload");
            for(auto& byte:full)byte=::std::byte{0xa5u};
            if(bytes==42u)
            {
                auto const local=reinterpret_cast<::std::uintptr_t>(frame.data()+10u);
                require(storage.current_local_snapshot(local,1u,10u,16u,2u).valid &&
                    !storage.current_local_snapshot(local,1u,10u,16u,3u).valid,"padded physical suffix grants no larger packet");
            }
            require(!storage.leave(reinterpret_cast<::std::uintptr_t>(full.data())) &&
                storage.leave(reinterpret_cast<::std::uintptr_t>(frame.data())),"small reserve keeps exact LIFO owner");
        }
        require(storage.enter_small(0u).empty() && storage.enter_small(65u).empty() &&
            storage.enter_small((::std::numeric_limits<::std::size_t>::max)()).empty() && storage.size()==0u,
            "small reserve rejects empty, oversized and overflowing requests");
    }
    ::fast_io::io::println("debug operand workspace heap/LIFO/80-frames/overflow/retained-depth/packet-visibility DATA PASS");
}

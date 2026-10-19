#include <cassert>
#include <thread>
#include <vector>
#include "../../src/uwvm2/runtime/checkpoint/observation_probe_hints.h"

int main()
{
    using hints=uwvm2::runtime::checkpoint::observation_probe_hints;
    int current{},retired{};
    hints::bitmap none{},selected{};
    hints::configure(nullptr);
    auto const initial_token{hints::full_observation_token()};
    assert((initial_token&1u)!=0u);
    assert(!hints::publish(&retired,none));
    assert(hints::full_observation_token()==initial_token);
    for(unsigned i{};i!=8192u;++i) { assert(hints::may_observe(i%3u,i%17u,i)); }
    hints::configure(&current);
    assert(hints::publish(&current,none));
    assert((hints::full_observation_token()&1u)==0u);
    for(unsigned i{};i!=8192u;++i) { assert(!hints::may_observe(i%3u,i%17u,i)); }
    // Hash collisions are conservative: every requested triple must observe,
    // while the controller still performs its exact identity comparison.
    for(unsigned i{};i!=256u;++i)
    {
        auto const b=hints::bucket(i%3u,i%17u,i*13u);
        selected[b/64u]|=std::uint64_t{1u}<<(b%64u);
    }
    assert(hints::publish(&current,selected));
    for(unsigned i{};i!=256u;++i) { assert(hints::may_observe(i%3u,i%17u,i*13u)); }
    if constexpr(std::atomic<std::uint8_t>::is_always_lock_free && sizeof(std::atomic<std::uint8_t>)==1u)
    {
        for(unsigned b{};b!=hints::bucket_count;++b)
        {
            auto const* byte=static_cast<std::atomic<std::uint8_t> const*>(hints::byte_address(b));
            assert(byte && byte->load(std::memory_order_acquire)==((selected[b/64u]>>(b%64u))&1u));
        }
    }
    // A late retired observer can neither clear demand nor cancel the current
    // observer's publication, even while configuration is being replaced.
    std::vector<std::thread> publishers;
    for(unsigned n{};n!=4u;++n)
    { publishers.emplace_back([&]{for(unsigned i{};i!=20000u;++i) { assert(!hints::publish(&retired,none)); }}); }
    for(unsigned i{};i!=10000u;++i)
    {
        hints::configure(&current);
        for(unsigned j{};j!=64u;++j) { assert(hints::may_observe(0u,1u,j)); }
        assert(hints::publish(&current,selected));
        assert(hints::may_observe(0u,0u,0u));
    }
    for(auto& thread:publishers) { thread.join(); }
    publishers.clear();
    assert(hints::publish(&current,none));
    auto const partial_token{hints::full_observation_token()};
    // A real shutdown first makes every gate effective. Current and retired
    // publishers must not clear that latch after its publication returns.
    std::atomic<bool> forced{};
    for(unsigned n{};n!=4u;++n)
    {
        publishers.emplace_back([&]{
            while(!forced.load(std::memory_order_acquire)) { assert(hints::publish(&current,none)); }
            for(unsigned i{};i!=2000u;++i)
            {
                assert(hints::publish(&current,i%2u ? selected : none));
                assert(!hints::publish(&retired,none));
                for(unsigned j{};j!=64u;++j) { assert(hints::may_observe(i%3u,i%17u,j)); }
            }
        });
    }
    hints::force_collection_until_configured();forced.store(true,std::memory_order_release);
    for(auto& thread:publishers) { thread.join(); }
    assert((hints::full_observation_token()&1u)!=0u && hints::full_observation_token()!=partial_token);
    for(unsigned i{};i!=8192u;++i) { assert(hints::may_observe(i%3u,i%17u,i)); }
    // Reset occurs only at actual quiescent configuration. A fresh observer
    // can then select no demand again; the former forced token is not reused.
    auto const closing_token{hints::full_observation_token()};
    hints::configure(&current);assert(hints::publish(&current,none));
    assert(hints::full_observation_token()!=closing_token);
    for(unsigned i{};i!=8192u;++i) { assert(!hints::may_observe(i%3u,i%17u,i)); }
    hints::require_all(&current);
    auto const full_token{hints::full_observation_token()};
    assert((full_token&1u)!=0u && full_token!=initial_token);
    for(unsigned i{};i!=8192u;++i) { assert(hints::may_observe(i%3u,i%17u,i)); }
    hints::configure(nullptr);
    assert(hints::full_observation_token()!=full_token);
    assert(!hints::publish(&current,none));
    for(unsigned op: {0x00u,0x06u,0x08u,0x10u,0x11u,0x12u,0x13u,0x25u,0x28u,0x36u,
                     0x3fu,0x40u,0x6du,0x6eu,0x6fu,0x70u,0x7fu,0x80u,0x81u,0x82u,0xa8u,0xabu,0xaeu,0xb1u,0xfcu,0xfdu})
    { assert(!hints::plain_numeric_opcode(static_cast<std::uint8_t>(op))); }
    for(unsigned op: {0x01u,0x02u,0x05u,0x0bu,0x0cu,0x0eu,0x0fu,0x1au,0x20u,0x24u,0x41u,0x6cu,0x71u,0x7eu,0x83u,0xa7u,0xacu,0xadu,0xb2u,0xc4u})
    { assert(hints::plain_numeric_opcode(static_cast<std::uint8_t>(op))); }
    assert(hints::byte_address(hints::bucket_count)==nullptr);
    for(unsigned op: {0x28u,0x29u,0x2au,0x35u,0x36u,0x3eu,0x6du,0x70u,0x7fu,0x82u,0xa8u,0xabu,0xaeu,0xb1u})
    { assert(hints::demand_numeric_opcode(static_cast<std::uint8_t>(op))); }
    for(unsigned op: {0x00u,0x08u,0x10u,0x11u,0x12u,0x13u,0x25u,0x26u,0x3fu,0x40u,0xfbu,0xfcu,0xfdu,0xfeu})
    { assert(!hints::demand_numeric_opcode(static_cast<std::uint8_t>(op))); }
}

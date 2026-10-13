#include <uwvm2/uwvm/debugger/breakpoint_index.h>
#include <array>
#include <cassert>
#include <cstdint>
int main()
{
    using index = uwvm2::uwvm::debugger::breakpoint_index;
    index table{};
    struct key { std::uint64_t module, function, offset; bool enabled; };
    std::array<key,index::capacity> keys{};
    std::uint64_t seed{7u};
    auto random{[&]() { seed ^= seed << 13u; seed ^= seed >> 7u; seed ^= seed << 17u; return seed; }};
    for(unsigned trial{}; trial != 1000u; ++trial)
    {
        table.clear();
        for(auto& k : keys) { k = {random()%4u,random()%16u,random()%32u,(random()&3u)!=0u}; }
        // Duplicate launch/user breakpoint location and deliberate hash collisions.
        keys[0] = keys[255]; keys[0].enabled = keys[255].enabled = true;
        for(std::size_t i{keys.size()}; i != 0u;)
        { auto const& k{keys[--i]}; if(k.enabled) { table.insert(i,k.module,k.function,k.offset); } }
        for(auto const& query : keys)
        {
            std::array<std::size_t,index::capacity> expected{}, actual{};
            std::size_t n{}, m{}, visited{};
            for(std::size_t i{}; i != keys.size(); ++i)
            { auto const& k{keys[i]}; if(k.enabled && k.module == query.module && k.function == query.function && k.offset == query.offset) { expected[n++]=i; } }
            for(auto p{table.first(query.module,query.function,query.offset)}; p != 0u; p=table.next(p))
            {
                assert(++visited <= keys.size()); auto const i{p-1u}; auto const& k{keys[i]};
                if(k.enabled && k.module == query.module && k.function == query.function && k.offset == query.offset) { actual[m++]=i; }
            }
            assert(n==m && expected==actual);
        }
    }
    table.clear(); assert(table.first(0u,0u,0u)==0u);
}

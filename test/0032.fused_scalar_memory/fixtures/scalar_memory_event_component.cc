#include <initializer_list>
#include <fast_io.h>
#include <uwvm2/validation/standard/wasm3/scalar_memory_event.h>
namespace v=uwvm2::validation::standard::wasm3;
constexpr bool schema_controls() noexcept
{
    unsigned rows{}, mixed12{};
    for(unsigned opcode{0x28u}; opcode!=0x3fu; ++opcode)
    {
        v::scalar_memory_descriptor descriptor{};
        if(!v::describe_scalar_memory_operation(opcode,descriptor)) { return false; }
        for(auto address: {v::storage_address_type::i32,v::storage_address_type::i64})
        {
            v::typed_memory_argument checked{};
            checked.address_type=address; checked.immediate.alignment=descriptor.max_alignment;
            checked.immediate.memory_index=7u; checked.immediate.offset=address==v::storage_address_type::i64 ? 0x1'0000'0001ull : 0xffff'ffffull;
            v::validated_scalar_memory_event event{};
            if(!v::complete_scalar_memory_event(event,opcode,checked,17uz,13uz,3uz,true)) { return false; }
            unsigned const address_bytes{address==v::storage_address_type::i64 ? 8u : 4u};
            if(event.opcode!=opcode || event.memory_index!=7u || event.offset!=checked.immediate.offset ||
               event.address_type!=address || event.value_kind!=descriptor.value_kind ||
               event.access_bytes!=descriptor.access_bytes || event.value_bytes!=descriptor.value_bytes ||
               event.store!=descriptor.store || event.signed_load!=descriptor.signed_load || !event.reachable ||
               event.source_offset!=17uz || event.source_bytes!=13uz || event.control_depth!=3uz ||
               event.pop_bytes!=address_bytes+(descriptor.store?descriptor.value_bytes:0u) ||
               event.push_bytes!=(descriptor.store?0u:descriptor.value_bytes) ||
               event.popped!=(descriptor.store?2u:1u) || event.pushed!=(descriptor.store?0u:1u)) { return false; }
            if(opcode==0x36u && address==v::storage_address_type::i64)
            { if(event.pop_bytes!=12u) { return false; } ++mixed12; }
            ++rows;
        }
    }
    v::typed_memory_argument checked{};checked.address_type=v::storage_address_type::i32;
    v::validated_scalar_memory_event unchanged{.opcode=999u};
    checked.immediate.offset=0x1'0000'0000ull;
    if(v::complete_scalar_memory_event(unchanged,0x28u,checked,0uz,3uz,1uz,true) || unchanged.opcode!=999u) { return false; }
    checked.immediate.offset=0u;checked.immediate.alignment=3u;
    if(v::complete_scalar_memory_event(unchanged,0x28u,checked,0uz,3uz,1uz,true) || unchanged.opcode!=999u) { return false; }
    checked.immediate.alignment=0u;
    if(v::complete_scalar_memory_event(unchanged,0x27u,checked,0uz,3uz,1uz,true) || unchanged.opcode!=999u) { return false; }
    if(v::complete_scalar_memory_event(unchanged,0x28u,checked,0uz,0uz,1uz,true) || unchanged.opcode!=999u) { return false; }
    return rows==46u && mixed12==1u;
}
static_assert(schema_controls());
int main()
{
    if(!schema_controls()) { return 1; }
    fast_io::io::print("scalar-memory-event: rows=46 mixed-memory64-i32-store-bytes=12 representation-negative-controls=4\n");
}

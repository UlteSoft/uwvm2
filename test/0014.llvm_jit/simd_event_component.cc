#include <initializer_list>
#include <fast_io.h>
#include <uwvm2/validation/standard/wasm3/simd_event.h>
namespace v = ::uwvm2::validation::standard::wasm3;
constexpr bool semantic_data_controls() noexcept
{
    using kind = v::simd_event_kind;
    using scalar = v::simd_event_scalar_kind;
    using value = v::simd_event_value_kind;
    struct row {kind k; scalar s; ::std::size_t lanes; unsigned align, pop, push, inbytes, outbytes;};
    constexpr row rows[]{
        {kind::constant,scalar::none,0,0,0,1,0,16},
        {kind::shuffle,scalar::none,0,0,2,1,32,16},
        {kind::splat,scalar::i32,0,0,1,1,4,16},
        {kind::splat,scalar::i64,0,0,1,1,8,16},
        {kind::splat,scalar::f32,0,0,1,1,4,16},
        {kind::splat,scalar::f64,0,0,1,1,8,16},
        {kind::extract_lane,scalar::i32,16,0,1,1,16,4},
        {kind::extract_lane,scalar::i64,2,0,1,1,16,8},
        {kind::extract_lane,scalar::f32,4,0,1,1,16,4},
        {kind::extract_lane,scalar::f64,2,0,1,1,16,8},
        {kind::replace_lane,scalar::i32,16,0,2,1,20,16},
        {kind::replace_lane,scalar::i64,2,0,2,1,24,16},
        {kind::replace_lane,scalar::f32,4,0,2,1,20,16},
        {kind::replace_lane,scalar::f64,2,0,2,1,24,16},
        {kind::unary,scalar::none,0,0,1,1,16,16},
        {kind::binary,scalar::none,0,0,2,1,32,16},
        {kind::ternary,scalar::none,0,0,3,1,48,16},
        {kind::test,scalar::none,0,0,1,1,16,4},
        {kind::shift,scalar::none,0,0,2,1,20,16},
    };
    for(auto const row:rows)
    {
        v::validated_simd_event e{.opcode=12u};e.lane=static_cast<unsigned>(row.lanes ? row.lanes-1uz:0uz);
        for(::std::size_t i{}; i!=16uz; ++i) { e.vector_bytes[i]=static_cast<::std::byte>(i); }
        if(!v::complete_simd_event(e,{row.k,row.s,row.lanes,row.align},7uz,19uz,3uz,true) ||
           e.popped!=row.pop || e.pushed!=row.push || e.pop_bytes!=row.inbytes || e.push_bytes!=row.outbytes ||
           e.source_offset!=7uz || e.source_bytes!=19uz || e.control_depth!=3uz || !e.reachable) { return false; }
        auto const copied{e};e.vector_bytes[0uz]=::std::byte{255u};
        if(copied.vector_bytes[0uz]!=::std::byte{} || copied.vector_bytes[15uz]!=::std::byte{15u}) { return false; }
    }
    // Every memory access width, both address widths, ordinary/lane load and store.
    // Independent expected byte effects distinguish i64 address from v128 value.
    for(unsigned alignment{}; alignment!=5u; ++alignment)
    {
        for(unsigned address_bytes: {4u,8u})
        {
            for(unsigned mode{}; mode!=3u; ++mode)
            {
                if(mode==1u && alignment==4u) { continue; } // No 128-bit SIMD memory-lane opcode.
                v::validated_simd_event e{.opcode=0u};
                e.memory.address_type=address_bytes==8u ? v::storage_address_type::i64:v::storage_address_type::i32;
                e.memory.immediate.memory_index=7u;e.memory.immediate.alignment=alignment;
                e.memory.immediate.offset=address_bytes==8u ? 0x1'0000'0001ull:0xffff'ffffull;
                auto const lanes{mode==1u ? 16uz>>alignment:0uz};e.lane=static_cast<unsigned>(lanes?lanes-1uz:0uz);
                auto const k{mode==2u?kind::memory_store:kind::memory_load};
                if(!v::complete_simd_event(e,{k,scalar::none,lanes,alignment},11uz,17uz,2uz,false) ||
                   e.pop_bytes!=address_bytes+(mode?16u:0u) || e.push_bytes!=(mode==2u?0u:16u) ||
                   e.popped!=(mode?2u:1u) || e.pushed!=(mode==2u?0u:1u) ||
                   e.access_bytes!=(1u<<alignment) || !e.has_memory || e.reachable ||
                   e.memory.immediate.memory_index!=7u || e.inputs[0]!=(address_bytes==8u?value::i64:value::i32)) { return false; }
            }
        }
    }
    v::validated_simd_event e{};
    if(v::complete_simd_event(e,{kind::splat,scalar::none,0uz,0u},0uz,2uz,1uz,true)) { return false; }
    if(v::complete_simd_event(e,{kind::constant,scalar::none,0uz,0u},0uz,1uz,1uz,true)) { return false; }
    e.lane=16u;
    if(v::complete_simd_event(e,{kind::extract_lane,scalar::i32,16uz,0u},0uz,3uz,1uz,true)) { return false; }
    e.lane=0u;e.memory.address_type=v::storage_address_type::i32;e.memory.immediate.offset=0x1'0000'0000ull;
    if(v::complete_simd_event(e,{kind::memory_load,scalar::none,0uz,4u},0uz,7uz,1uz,true)) { return false; }
    e.memory.immediate.offset=0u;e.memory.immediate.alignment=5u;
    if(v::complete_simd_event(e,{kind::memory_load,scalar::none,0uz,4u},0uz,7uz,1uz,true)) { return false; }
    return true;
}
static_assert(semantic_data_controls());
int main()
{
    if(!semantic_data_controls()) { return 1; }
    ::fast_io::io::println("SIMD-event DATA: kinds19 memory-effects28 invalid-descriptions5; not a compiler/VM execution qualification");
}

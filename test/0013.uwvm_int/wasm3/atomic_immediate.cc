#include <uwvm2/validation/standard/wasm3/threads.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort(); } } while(false)
namespace wasm3=uwvm2::validation::standard::wasm3;
using kind=wasm3::atomic_instruction_kind;
using error=wasm3::atomic_immediate_error;
using bytes=std::vector<std::byte>;
void leb(bytes& out,std::uint64_t value)
{
    do { auto b=unsigned(value&127);value>>=7;out.push_back(std::byte(b|(value?128:0))); } while(value);
}
struct sample { unsigned opcode,alignment;kind family;bool value64,result64;unsigned operands;bool result; };
// Representative instructions from every family and each integer width in the
// normative FE binary grammar. These are decoder tests, not runtime acceptance.
constexpr sample cases[]{
    {0x00,2,kind::notify,false,false,2,true}, {0x01,2,kind::wait32,false,false,3,true},
    {0x02,3,kind::wait64,true,false,3,true},
    {0x10,2,kind::load,false,false,1,true}, {0x11,3,kind::load,true,true,1,true},
    {0x12,0,kind::load,false,false,1,true}, {0x13,1,kind::load,false,false,1,true},
    {0x14,0,kind::load,true,true,1,true}, {0x15,1,kind::load,true,true,1,true}, {0x16,2,kind::load,true,true,1,true},
    {0x17,2,kind::store,false,false,2,false}, {0x1d,2,kind::store,true,false,2,false},
    {0x22,0,kind::add,true,true,2,true}, {0x2a,1,kind::sub,true,true,2,true},
    {0x31,1,kind::and_,true,true,2,true}, {0x37,0,kind::or_,true,true,2,true},
    {0x3f,1,kind::xor_,true,true,2,true}, {0x46,1,kind::exchange,true,true,2,true},
    {0x4e,2,kind::compare_exchange,true,true,3,true}};
int main()
{
    unsigned checked{};
    for(auto row:cases)
    {
        auto d=wasm3::describe_atomic_instruction(row.opcode);
        CHECK(d.kind==row.family && d.natural_alignment==row.alignment && d.value_i64==row.value64);
        CHECK(d.result_i64==row.result64 && d.operand_count==row.operands && d.has_result==row.result);
        for(bool indexed:{false,true}) for(bool padded:{false,true})
        {
            bytes input{};
            input.push_back(std::byte(row.opcode|(padded?128:0)));
            if(padded) {input.push_back(std::byte{});}
            auto alignment_pos=input.size();
            leb(input,row.alignment|(indexed?64:0));
            if(indexed) {leb(input,129);}
            leb(input,0xffff'ffffu);
            std::byte const* const begin=input.data();
            auto const end=begin+input.size();
            auto cursor=begin;
            auto r=wasm3::scan_atomic_instruction(cursor,end,indexed);
            CHECK(r.error==error::ok && cursor==end && r.opcode==row.opcode);
            CHECK(r.memory.memory_index==(indexed?129:0) && r.memory.offset==0xffff'ffffu);
            // Prefix truncation never commits an incomplete subopcode/memarg.
            for(std::size_t length{};length!=input.size();++length)
            {
                cursor=begin;r=wasm3::scan_atomic_instruction(cursor,begin+length,indexed);
                CHECK(r.error!=error::ok && cursor==begin);
            }
            if(indexed)
            {
                cursor=begin;r=wasm3::scan_atomic_instruction(cursor,end,false);
                CHECK(r.error==error::memory_argument && r.memory.error==wasm3::memory_immediate_error::feature_disabled && cursor==begin);
            }
            // Atomic alignment is exact, including rejection of underspecified
            // alignment that would be legal for a non-atomic load/store.
            input[alignment_pos]=std::byte((row.alignment+1)|(indexed?64:0));
            cursor=begin;r=wasm3::scan_atomic_instruction(cursor,end,indexed);
            CHECK(r.error==error::alignment && cursor==begin);
            if(row.alignment)
            {
                input[alignment_pos]=std::byte((row.alignment-1)|(indexed?64:0));
                cursor=begin;r=wasm3::scan_atomic_instruction(cursor,end,indexed);
                CHECK(r.error==error::alignment && cursor==begin);
            }
            ++checked;
        }
    }
    for(unsigned opcode:{4u,15u,0x4fu,0xffff'ffffu})
    {
        bytes input{};leb(input,opcode);leb(input,0);leb(input,0);
        std::byte const* cursor=input.data();auto begin=cursor;
        CHECK(wasm3::scan_atomic_instruction(cursor,begin+input.size(),true).error==error::opcode && cursor==begin);
    }
    for(auto encoding:{bytes{std::byte{3},std::byte{}},bytes{std::byte{0x83},std::byte{},std::byte{}}})
    {
        auto const* begin=encoding.data();auto const* end=begin+encoding.size();auto cursor=begin;
        CHECK(wasm3::scan_atomic_fence_immediate(cursor,end) && cursor==end);
        encoding.back()=std::byte{0x80};cursor=begin;
        CHECK(!wasm3::scan_atomic_fence_immediate(cursor,end) && cursor==begin);
    }
    std::printf("PASS atomic immediate: %u family/width/index/padded variants, exact alignment and transactional truncation\n",checked);
}

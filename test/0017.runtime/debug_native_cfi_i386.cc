// Owned 32-bit CFI DATA; integer labels grant no live stack permission.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <fast_io.h>
namespace cfi = ::uwvm2::runtime::compiler::llvm_jit::details;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("i386 CFI failure line=", __LINE__); return 1; } } while(false)
int main()
{
    using K = cfi::native_debug_cfi_rule_kind;
    cfi::native_debug_cfi_row row{}; row.usable=true; row.cfa_register=4u; row.cfa_offset=16;
    row.registers[8u]={K::cfa_memory,0u,-4}; row.registers[5u]={K::cfa_memory,0u,-8};
    row.registers[3u]={K::same,0u,0};
    ::std::array<::std::uint64_t,9u> registers{}; registers[4u]=0x1000u; registers[3u]=42u;
    ::std::array<cfi::native_debug_cfi_i386_owned_word,2u> words{{{0x100cu,0x2000u},{0x1008u,0xaaaau}}};
    cfi::native_debug_cfi_i386_caller out{};
    auto evaluate=[&](auto const& r,auto known,auto slots) noexcept
    { return cfi::evaluate_native_debug_cfi_i386_sparse(r,registers,known,0x1000u,0x1010u,slots,out); };
    CHECK(evaluate(row,1u<<4u,::std::span{words}));
    CHECK(out.return_pc==0x2000u && out.cfa==0x1010u && out.registers[4u]==0x1010u &&
          out.registers[5u]==0xaaaau && !(out.known&(1u<<3u)));
    CHECK(evaluate(row,(1u<<4u)|(1u<<3u),::std::span{words}) && out.registers[3u]==42u);
    CHECK(!evaluate(row,0u,::std::span{words}) && out.known==0u && out.return_pc==0u);
    auto bad=row; bad.usable=false; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    bad=row; bad.cfa_register=8u; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    bad=row; bad.cfa_register=5u; CHECK(!evaluate(bad,1u<<4u,::std::span{words}));
    bad=row; bad.cfa_offset=17; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    bad=row; bad.cfa_offset=0; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    bad=row; bad.cfa_offset=-16; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    bad=row; bad.registers[8u]={}; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    bad=row; bad.registers[8u]={K::cfa_memory,0u,-8}; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    bad=row; bad.registers[8u]={K::same,0u,0}; registers[8u]=0x2000u;
    CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    CHECK(!evaluate(row,UINT32_MAX,::std::span{words}.subspan(1u)));
    auto duplicate=words; duplicate[1u]=duplicate[0u]; CHECK(!evaluate(row,UINT32_MAX,::std::span{duplicate}));
    auto overlap=words; overlap[1u].address=words[0u].address-1u; CHECK(!evaluate(row,UINT32_MAX,::std::span{overlap}));
    auto outside=words; outside[1u].address=0x1010u; CHECK(!evaluate(row,UINT32_MAX,::std::span{outside}));
    outside=words; outside[1u].address=0xfffu; CHECK(!evaluate(row,UINT32_MAX,::std::span{outside}));
    outside=words; outside[1u].address=0x100fu; CHECK(!evaluate(row,UINT32_MAX,::std::span{outside}));
    auto zero=words; zero[0u].value=0u; CHECK(!evaluate(row,UINT32_MAX,::std::span{zero}));
    // Unavailable and volatile values remain unknown, even with a real-looking rule.
    bad=row; bad.registers[5u]={K::register_memory,8u,0};
    bad.registers[0u]={K::cfa_memory,0u,-4}; bad.registers[1u]={K::same,0u,0};
    CHECK(evaluate(bad,UINT32_MAX,::std::span{words}) && !(out.known&((1u<<0u)|(1u<<1u)|(1u<<5u))));
    registers[3u]=::std::uint64_t{1u}<<32u;
    CHECK(evaluate(row,UINT32_MAX,::std::span{words}) && !(out.known&(1u<<3u)));
    registers[4u]=(::std::uint64_t{1u}<<32u)+0x1000u;
    CHECK(!evaluate(row,UINT32_MAX,::std::span{words}));
    registers[4u]=0x1001u; CHECK(!evaluate(row,UINT32_MAX,::std::span{words}));
    registers[4u]=UINT32_MAX-3u;
    CHECK(!cfi::evaluate_native_debug_cfi_i386_sparse(row,registers,UINT32_MAX,UINT32_MAX-3u,UINT32_MAX,{},out));
    // Exact four-byte reader: a four-byte frame must not touch the adjacent word.
    registers={}; registers[4u]=0x1000u; row.cfa_offset=4;
    unsigned reads{};
    CHECK(cfi::evaluate_native_debug_cfi_i386_bounded(row,registers,1u<<4u,0x1000u,0x1004u,
        [&](::std::uintptr_t address,::std::uint32_t& value) noexcept
        { ++reads; if(address!=0x1000u) { return false; } value=0x12345678u; return true; },out));
    CHECK(reads==1u && out.return_pc==0x12345678u && out.known==((1u<<4u)|(1u<<8u)));
    ::fast_io::io::println("i386 CFI owned DATA: PASS; slot-width=4 adjacent-word-reads=0 live-caller-qualified=false native-finish-qualified=false");
}

// ARM-state AAPCS32 owned DATA, not a live Wasm/VM stack permission.
#include <uwvm2/utils/macro/push_macros.h>
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/native_debug_cfi_arm.h>
#include <fast_io.h>
namespace cfi = ::uwvm2::runtime::compiler::llvm_jit::details;
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("ARM CFI failure line=", __LINE__); return 1; } } while(false)
int main()
{
    unsigned checks{};
    using K = cfi::native_debug_cfi_rule_kind;
    cfi::native_debug_cfi_row row{}; row.usable=true; row.cfa_register=13u; row.cfa_offset=16;
    row.registers[14u]={K::cfa_memory,0u,-4}; row.registers[11u]={K::cfa_memory,0u,-8};
    row.registers[4u]={K::same,0u,0};
    ::std::array<::std::uint64_t,16u> registers{}; registers[13u]=0x1000u; registers[4u]=42u;
    ::std::array<cfi::native_debug_cfi_arm_owned_word,2u> words{{{0x100cu,0x2000u},{0x1008u,0xaaaau}}};
    cfi::native_debug_cfi_arm_caller out{};
    auto evaluate=[&](auto const& r,auto known,auto slots) noexcept
    { return cfi::evaluate_native_debug_cfi_arm_sparse(r,registers,known,0x1000u,0x1010u,slots,out); };
    auto cleared=[&]() noexcept
    {
        if(out.known!=0u || out.cfa!=0u || out.return_pc!=0u) { return false; }
        for(auto value:out.registers) { if(value!=0u) { return false; } } return true;
    };
    CHECK(evaluate(row,1u<<13u,::std::span{words}));
    CHECK(out.return_pc==0x2000u && out.cfa==0x1010u && out.registers[13u]==0x1010u &&
          out.registers[11u]==0xaaaau && !(out.known&(1u<<4u)));
    CHECK(evaluate(row,(1u<<13u)|(1u<<4u),::std::span{words}) && out.registers[4u]==42u);
    CHECK(!evaluate(row,0u,::std::span{words}) && cleared());
    auto bad=row; bad.usable=false; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}) && cleared());
    bad=row; bad.cfa_register=15u; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    bad=row; bad.cfa_register=32u; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    bad=row; bad.cfa_register=11u; CHECK(!evaluate(bad,1u<<13u,::std::span{words}));
    registers[11u]=0x1008u; bad.cfa_offset=8;
    CHECK(evaluate(bad,(1u<<13u)|(1u<<11u),::std::span{words}) && out.cfa==0x1010u);
    for(auto offset:{-16,0,4,17,32,INT32_MIN,INT32_MAX})
    { bad=row; bad.cfa_offset=offset; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}) && cleared()); }
    bad=row; bad.registers[14u]={}; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    // A PC rule never substitutes for the actual LR return rule.
    bad.registers[15u]=row.registers[14u]; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    CHECK(!evaluate(row,UINT32_MAX,::std::span{words}.subspan(1u)) && cleared());
    auto duplicate=words; duplicate[1u]=duplicate[0u]; CHECK(!evaluate(row,UINT32_MAX,::std::span{duplicate}));
    for(auto address:{0xffcu,0xfffu,0x1009u,0x100bu,0x100fu,0x1010u})
    { auto outside=words; outside[1u].address=address; CHECK(!evaluate(row,UINT32_MAX,::std::span{outside}) && cleared()); }
    for(auto value:{0u,0x2001u,0x2002u,0x2003u})
    { auto wrong_state=words; wrong_state[0u].value=value; CHECK(!evaluate(row,UINT32_MAX,::std::span{wrong_state}) && cleared()); }
    // Never infer volatile, PC or platform/TLS r9 from plausible CFI rules.
    bad=row;
    for(unsigned reg:{0u,1u,2u,3u,9u,12u,15u}) { bad.registers[reg]={K::cfa_memory,0u,-4}; }
    CHECK(evaluate(bad,UINT32_MAX,::std::span{words}) &&
          out.known==((1u<<4u)|(1u<<11u)|(1u<<13u)|(1u<<14u)));
    for(unsigned reg:{0u,1u,2u,3u,9u,12u,15u}) { CHECK(out.registers[reg]==0u); }
    // Unknown/invalid bases do not become native reads; missing saved r11 is unknown.
    bad=row; bad.registers[11u]={K::register_memory,15u,0};
    CHECK(evaluate(bad,UINT32_MAX,::std::span{words}) && !(out.known&(1u<<11u)));
    bad.registers[11u]={K::register_memory,10u,0}; registers[10u]=0x1008u;
    CHECK(evaluate(bad,1u<<13u,::std::span{words}) && !(out.known&(1u<<11u)));
    CHECK(evaluate(bad,(1u<<13u)|(1u<<10u),::std::span{words}) && out.registers[11u]==0xaaaau);
    bad.registers[11u]={K::register_value,4u,-43};
    CHECK(evaluate(bad,UINT32_MAX,::std::span{words}) && !(out.known&(1u<<11u)));
    registers[4u]=(::std::uint64_t{1u}<<32u)+42u;
    CHECK(evaluate(row,UINT32_MAX,::std::span{words}) && !(out.known&(1u<<4u)));
    registers[13u]=(::std::uint64_t{1u}<<32u)+0x1000u;
    CHECK(!evaluate(row,UINT32_MAX,::std::span{words}) && cleared());
    registers[13u]=0x1001u; CHECK(!evaluate(row,UINT32_MAX,::std::span{words}));
    registers[13u]=UINT32_MAX-3u;
    CHECK(!cfi::evaluate_native_debug_cfi_arm_sparse(row,registers,UINT32_MAX,UINT32_MAX-3u,UINT32_MAX,{},out));
    // A leaf with untouched LR needs no stack slot or allocation.
    registers={}; registers[13u]=0x1000u; registers[14u]=0x2000u;
    row={}; row.usable=true; row.cfa_register=13u; row.registers[14u]={K::same,0u,0};
    CHECK(cfi::evaluate_native_debug_cfi_arm_sparse(row,registers,(1u<<13u)|(1u<<14u),0x1000u,0x1000u,{},out));
    CHECK(out.known==((1u<<13u)|(1u<<14u)) && out.cfa==0x1000u && out.return_pc==0x2000u);
    CHECK(!cfi::evaluate_native_debug_cfi_arm_sparse(row,registers,1u<<13u,0x1000u,0x1000u,{},out));
    registers[14u]=::std::uint64_t{1u}<<32u;
    CHECK(!cfi::evaluate_native_debug_cfi_arm_sparse(row,registers,UINT32_MAX,0x1000u,0x1000u,{},out));
    // Word-aligned intermediate SP is legal; the caller CFA is eight-aligned.
    // The reader's type and admitted addresses require exactly four-byte slots.
    registers={}; registers[13u]=0x1004u; row.cfa_offset=12;
    row.registers[14u]={K::cfa_memory,0u,-4}; row.registers[11u]={K::cfa_memory,0u,-8};
    unsigned reads{},unexpected{};
    auto reader=[&](::std::uintptr_t address,::std::uint32_t& value) noexcept
    {
        ++reads;
        if(address==0x100cu) { value=0x2000u; return true; }
        if(address==0x1008u) { value=0xaaaau; return true; }
        ++unexpected; return false;
    };
    CHECK(cfi::evaluate_native_debug_cfi_arm_bounded(row,registers,1u<<13u,0x1004u,0x1010u,reader,out));
    CHECK(reads==2u && unexpected==0u && out.cfa==0x1010u);
    // Rules outside [SP,CFA), misaligned slots and oversized/overflow bases
    // are refused before the reader can touch a neighbouring frame.
    for(auto offset:{-16,-9,-1,0,4,INT32_MIN,INT32_MAX})
    {
        bad=row; bad.registers[14u].offset=offset; reads=unexpected=0u;
        CHECK(!cfi::evaluate_native_debug_cfi_arm_bounded(bad,registers,1u<<13u,0x1004u,0x1010u,reader,out));
        CHECK(unexpected==0u && reads==1u && cleared());
    }
    // Recover the complete supported set from eight distinct word slots.
    registers={}; registers[13u]=0x1000u; row={}; row.usable=true; row.cfa_register=13u; row.cfa_offset=32;
    ::std::array<cfi::native_debug_cfi_arm_owned_word,8u> all{};
    ::std::array<unsigned,8u> preserved{4u,5u,6u,7u,8u,10u,11u,14u};
    for(unsigned n{};n!=all.size();++n)
    { row.registers[preserved[n]]={K::cfa_memory,0u,static_cast<::std::int32_t>(n*4u)-32}; all[n]={0x1000u+n*4u,n==7u?0x2000u:100u+n}; }
    CHECK(cfi::evaluate_native_debug_cfi_arm_sparse(row,registers,1u<<13u,0x1000u,0x1020u,all,out));
    for(unsigned n{};n!=preserved.size();++n) { CHECK(out.registers[preserved[n]]==all[n].value); }
    ::std::array<cfi::native_debug_cfi_arm_owned_word,9u> too_many{};
    CHECK(!cfi::evaluate_native_debug_cfi_arm_sparse(row,registers,UINT32_MAX,0x1000u,0x1020u,too_many,out) && cleared());
    if constexpr(sizeof(::std::uintptr_t)>4u)
    {
        auto const above{static_cast<::std::uintptr_t>(::std::uint64_t{1u}<<32u)};
        CHECK(!cfi::evaluate_native_debug_cfi_arm_sparse(row,registers,UINT32_MAX,0x1000u,above,{},out) && cleared());
    }
    ::fast_io::io::println("ARM CFI owned DATA: PASS checks=",checks,
        " pointer-bits=",sizeof(void*)*8u," slot-width=4 adjacent-frame-reads=0 platform-r9-known=false",
        " live-caller-qualified=false native-finish-qualified=false");
}

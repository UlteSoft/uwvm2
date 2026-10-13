// N64 private CFI DATA only. Integer labels never authorize a native read.
// Use LLVM's documented header-only ADT mode; this fixture calls no LLVM API
// and must not link a host SDK into the target executable.
#define LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING 1
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <llvm/TargetParser/Triple.h>
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/native_debug_cfi.h>
#include <fast_io.h>
namespace cfi = ::uwvm2::runtime::compiler::llvm_jit::details;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("N64 CFI failure line=", __LINE__); return 1; } } while(false)
int main()
{
    using K = cfi::native_debug_cfi_rule_kind;
    cfi::native_debug_cfi_row row{}; row.usable=true; row.cfa_register=29u; row.cfa_offset=32;
    row.registers[31u]={K::cfa_memory,0u,-8}; row.registers[30u]={K::cfa_memory,0u,-16};
    row.registers[16u]={K::same,0u,0};
    ::std::array<::std::uint64_t,32u> registers{}; registers[29u]=0x1000u;
    ::std::array<cfi::native_debug_cfi_owned_word,2u> words{{{0x1018u,0x2000u},{0x1010u,0xaaaa}}};
    cfi::native_debug_cfi_mips64_caller out{};
    auto evaluate=[&](auto const& r,auto known,auto slots) noexcept
    { return cfi::evaluate_native_debug_cfi_mips64_sparse(r,registers,known,0x1000u,0x1020u,slots,out); };
    CHECK(evaluate(row,1u<<29u,::std::span{words}));
    CHECK(out.return_pc==0x2000u && out.cfa==0x1020u && out.registers[29u]==0x1020u &&
          out.registers[30u]==0xaaaa && (out.known&(1u<<30u)) && !(out.known&(1u<<16u)));
    CHECK(!evaluate(row,0u,::std::span{words}) && out.known==0u && out.return_pc==0u);
    auto bad=row; bad.cfa_register=32u; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    bad=row; bad.cfa_register=30u; CHECK(!evaluate(bad,1u<<29u,::std::span{words}));
    bad=row; bad.cfa_offset=40; CHECK(!evaluate(bad,1u<<29u,::std::span{words}));
    bad=row; bad.registers[31u]={}; CHECK(!evaluate(bad,1u<<29u,::std::span{words}));
    bad=row; bad.registers[31u]={K::cfa_memory,0u,0}; CHECK(!evaluate(bad,1u<<29u,::std::span{words}));
    bad=row; bad.registers[31u]={K::register_memory,26u,0}; CHECK(!evaluate(bad,1u<<29u,::std::span{words}));
    CHECK(!evaluate(row,1u<<29u,::std::span{words}.subspan(1u)));
    auto duplicate=words; duplicate[1u].address=duplicate[0u].address;
    CHECK(!evaluate(row,1u<<29u,::std::span{duplicate}));
    auto overlap=words; overlap[1u].address=duplicate[0u].address-4u;
    CHECK(!evaluate(row,1u<<29u,::std::span{overlap}));
    auto outside=words; outside[1u].address=0x1020u; CHECK(!evaluate(row,1u<<29u,::std::span{outside}));
    auto misaligned=words; misaligned[0u].value=0x2001u; CHECK(!evaluate(row,1u<<29u,::std::span{misaligned}));
    auto zero=words; zero[0u].value=0u; CHECK(!evaluate(row,1u<<29u,::std::span{zero}));
    auto leaf=row; leaf.cfa_offset=0; leaf.registers[31u]={K::same,0u,0}; registers[31u]=0x3000u;
    CHECK(cfi::evaluate_native_debug_cfi_mips64_sparse(leaf,registers,(1u<<29u)|(1u<<31u),0x1000u,0x1000u,{},out));
    CHECK(out.return_pc==0x3000u && out.cfa==0x1000u && !(out.known&(1u<<30u)));
    CHECK(!cfi::evaluate_native_debug_cfi_mips64_sparse(leaf,registers,1u<<29u,0x1000u,0x1000u,{},out));
    CHECK(!cfi::evaluate_native_debug_cfi_mips64_sparse(leaf,registers,UINT32_MAX,0x1000u,0x1000u,words,out));
    bad=row; bad.cfa_offset=-16; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    // All eleven N64 preserved columns can survive; platform/volatile state cannot.
    auto saved=row;
    for(unsigned n{};n!=32u;++n) { registers[n]=n+0xabc0u; saved.registers[n]={K::same,0u,0}; }
    registers[29u]=0x1000u; saved.registers[31u]=row.registers[31u];
    CHECK(evaluate(saved,UINT32_MAX,::std::span{words}));
    constexpr unsigned preserved[]{16u,17u,18u,19u,20u,21u,22u,23u,28u,30u,31u};
    ::std::uint32_t expected{1u<<29u}; for(unsigned n:preserved) { expected|=1u<<n; }
    CHECK(out.known==expected && out.registers[0u]==0u && out.registers[25u]==0u && out.registers[26u]==0u && out.registers[27u]==0u);
    // A bounded reader receives ONLY the requested saved words in this frame.
    unsigned reads{}; bool escaped{};
    auto reader=[&](auto address,auto& value) noexcept {
        ++reads; escaped=escaped || address<0x1000u || address>=0x1020u;
        for(auto const& w:words) { if(w.address==address) { value=w.value; return true; } }
        return false;
    };
    CHECK(cfi::evaluate_native_debug_cfi_mips64_bounded(row,registers,UINT32_MAX,0x1000u,0x1020u,reader,out));
    CHECK(reads==2u && !escaped);
    reads=0u; bad=row; bad.registers[31u].offset=0;
    CHECK(!cfi::evaluate_native_debug_cfi_mips64_bounded(bad,registers,UINT32_MAX,0x1000u,0x1020u,reader,out));
    CHECK(reads==1u && !escaped && out.known==0u);
    registers[29u]=0x1008u; CHECK(!evaluate(row,UINT32_MAX,::std::span{words}));
    registers[29u]=UINT64_MAX-15u;
    CHECK(!cfi::evaluate_native_debug_cfi_mips64_sparse(row,registers,UINT32_MAX,UINTPTR_MAX-15u,UINTPTR_MAX,{},out));
    ::fast_io::io::println("MIPS64 N64 CFI owned DATA: PASS; actual Wasm caller/finish qualification remains separate");
}

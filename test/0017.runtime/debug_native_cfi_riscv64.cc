// Owned DATA tests for RV64 CFI. No case authorizes a native memory read.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <fast_io.h>
namespace cfi = ::uwvm2::runtime::compiler::llvm_jit::details;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("RV64 CFI failure line=",__LINE__);return 1; } } while(false)
int main()
{
    using K=cfi::native_debug_cfi_rule_kind;
    cfi::native_debug_cfi_row row{};row.usable=true;row.cfa_register=2u;row.cfa_offset=32;
    row.registers[1u]={K::cfa_memory,0u,-8};row.registers[8u]={K::cfa_memory,0u,-16};
    row.registers[9u]={K::same,0u,0};
    ::std::array<::std::uint64_t,32u> registers{};registers[2u]=0x1000u;
    ::std::array<cfi::native_debug_cfi_owned_word,2u> words{{{0x1018u,0x2000u},{0x1010u,0xaaaa}}};
    cfi::native_debug_cfi_riscv64_caller out{};
    auto evaluate=[&](auto const& r,auto known,auto slots) noexcept
    { return cfi::evaluate_native_debug_cfi_riscv64_sparse(r,registers,known,0x1000u,0x1020u,slots,out); };
    CHECK(evaluate(row,1u<<2u,::std::span{words}));
    CHECK(out.return_pc==0x2000u && out.cfa==0x1020u && out.registers[2u]==0x1020u &&
          out.registers[8u]==0xaaaa && (out.known&(1u<<8u)) && !(out.known&(1u<<9u)) && !(out.known&(1u<<6u)));
    CHECK(!evaluate(row,0u,::std::span{words}));
    CHECK(out.known==0u && out.return_pc==0u);
    auto bad=row;bad.cfa_register=32u;CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    bad=row;bad.cfa_register=6u;CHECK(!evaluate(bad,1u<<2u,::std::span{words}));
    bad=row;bad.cfa_offset=0;CHECK(!evaluate(bad,1u<<2u,::std::span{words}));
    bad=row;bad.cfa_offset=40;CHECK(!evaluate(bad,1u<<2u,::std::span{words}));
    bad=row;bad.registers[1u]={};CHECK(!evaluate(bad,1u<<2u,::std::span{words}));
    bad=row;bad.registers[1u]={K::cfa_memory,0u,0};CHECK(!evaluate(bad,1u<<2u,::std::span{words}));
    CHECK(!evaluate(row,1u<<2u,::std::span{words}.subspan(1u)));
    auto duplicate=words;duplicate[1u].address=duplicate[0u].address;
    CHECK(!evaluate(row,1u<<2u,::std::span{duplicate}));
    auto outside=words;outside[1u].address=0x1020u;CHECK(!evaluate(row,1u<<2u,::std::span{outside}));
    auto zero=words;zero[0u].value=0u;CHECK(!evaluate(row,1u<<2u,::std::span{zero}));
    // A real leaf's same-RA rule consumes only a known kernel register. An
    // unavailable RA never turns into a guessed zero or a copied stack slot.
    auto leaf=row;leaf.registers[1u]={K::same,0u,0};registers[1u]=0x3000u;
    CHECK(evaluate(leaf,(1u<<2u)|(1u<<1u),::std::span{words}) && out.return_pc==0x3000u);
    CHECK(!evaluate(leaf,1u<<2u,::std::span{words}));
    // A 32-bit known mask must cover high RV64 GPRs without signed shifts.
    auto high=row;high.cfa_register=31u;registers[31u]=0x1000u;
    CHECK(evaluate(high,(1u<<2u)|(1u<<31u),::std::span{words}) && out.cfa==0x1020u);
    CHECK(!evaluate(high,1u<<2u,::std::span{words}));
    // The original x86-64 evaluator remains a separate register/RA contract.
    cfi::native_debug_cfi_row x64{};x64.usable=true;x64.cfa_register=7u;x64.cfa_offset=32;
    x64.registers[16u]={K::cfa_memory,0u,-8};
    ::std::array<::std::uint64_t,17u> xregs{};xregs[7u]=0x1000u;
    cfi::native_debug_cfi_caller xout{};
    CHECK(cfi::evaluate_native_debug_cfi_x64_sparse(x64,xregs,1u<<7u,0x1000u,0x1020u,words,xout) &&
          xout.return_pc==0x2000u && xout.registers[7u]==0x1020u);
    ::fast_io::io::println("RV64 CFI owned DATA: PASS; live Wasm callers require separate qualification");
}

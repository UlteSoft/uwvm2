// Owned LoongArch64 CFI DATA. These integer labels authorize no native read.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <fast_io.h>
namespace cfi = ::uwvm2::runtime::compiler::llvm_jit::details;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("LoongArch64 CFI failure line=", __LINE__); return 1; } } while(false)
int main()
{
    using K = cfi::native_debug_cfi_rule_kind;
    cfi::native_debug_cfi_row row{}; row.usable=true; row.cfa_register=3u; row.cfa_offset=32;
    row.registers[1u]={K::cfa_memory,0u,-8}; row.registers[22u]={K::cfa_memory,0u,-16};
    row.registers[23u]={K::same,0u,0};
    ::std::array<::std::uint64_t,32u> registers{}; registers[3u]=0x1000u;
    ::std::array<cfi::native_debug_cfi_owned_word,2u> words{{{0x1018u,0x2000u},{0x1010u,0xaaaa}}};
    cfi::native_debug_cfi_loongarch64_caller out{};
    auto evaluate=[&](auto const& r,auto known,auto slots) noexcept
    { return cfi::evaluate_native_debug_cfi_loongarch64_sparse(r,registers,known,0x1000u,0x1020u,slots,out); };
    CHECK(evaluate(row,1u<<3u,::std::span{words}));
    CHECK(out.return_pc==0x2000u && out.cfa==0x1020u && out.registers[3u]==0x1020u &&
          out.registers[22u]==0xaaaa && (out.known&(1u<<22u)) && !(out.known&(1u<<23u)));
    CHECK(!evaluate(row,0u,::std::span{words}) && out.known==0u && out.return_pc==0u);
    auto bad=row; bad.cfa_register=32u; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    bad=row; bad.cfa_register=22u; CHECK(!evaluate(bad,1u<<3u,::std::span{words}));
    bad=row; bad.cfa_offset=40; CHECK(!evaluate(bad,1u<<3u,::std::span{words}));
    bad=row; bad.registers[1u]={}; CHECK(!evaluate(bad,1u<<3u,::std::span{words}));
    bad=row; bad.registers[1u]={K::cfa_memory,0u,0}; CHECK(!evaluate(bad,1u<<3u,::std::span{words}));
    bad=row; bad.registers[1u]={K::register_memory,21u,0}; CHECK(!evaluate(bad,1u<<3u,::std::span{words}));
    CHECK(!evaluate(row,1u<<3u,::std::span{words}.subspan(1u)));
    auto duplicate=words; duplicate[1u].address=duplicate[0u].address;
    CHECK(!evaluate(row,1u<<3u,::std::span{duplicate}));
    auto overlap=words; overlap[1u].address=overlap[0u].address-4u;
    CHECK(!evaluate(row,1u<<3u,::std::span{overlap}));
    auto outside=words; outside[1u].address=0x1020u; CHECK(!evaluate(row,1u<<3u,::std::span{outside}));
    auto misaligned=words; misaligned[0u].value=0x2001u; CHECK(!evaluate(row,1u<<3u,::std::span{misaligned}));
    auto zero=words; zero[0u].value=0u; CHECK(!evaluate(row,1u<<3u,::std::span{zero}));
    // A zero-sized leaf uses only an actual known RA; no memory slot fits.
    auto leaf=row; leaf.cfa_offset=0; leaf.registers[1u]={K::same,0u,0}; registers[1u]=0x3000u;
    CHECK(cfi::evaluate_native_debug_cfi_loongarch64_sparse(leaf,registers,(1u<<3u)|(1u<<1u),0x1000u,0x1000u,{},out));
    CHECK(out.return_pc==0x3000u && out.cfa==0x1000u && !(out.known&(1u<<22u)));
    CHECK(!cfi::evaluate_native_debug_cfi_loongarch64_sparse(leaf,registers,1u<<3u,0x1000u,0x1000u,{},out));
    CHECK(!cfi::evaluate_native_debug_cfi_loongarch64_sparse(leaf,registers,UINT32_MAX,0x1000u,0x1000u,words,out));
    // CFA arithmetic cannot wrap, and public volatile/platform state is absent.
    bad=row; bad.cfa_offset=-16; CHECK(!evaluate(bad,UINT32_MAX,::std::span{words}));
    auto saved=row; saved.registers[0u]={K::cfa_memory,0u,-8}; saved.registers[21u]={K::same,0u,0};
    registers[21u]=0xfeedu; registers[23u]=0xbbbbu;
    CHECK(evaluate(saved,UINT32_MAX,::std::span{words}) && out.registers[23u]==0xbbbbu &&
          out.registers[0u]==0u && out.registers[21u]==0u &&
          !(out.known&((1u<<0u)|(1u<<21u))));
    registers[3u]=0x1008u; CHECK(!evaluate(row,UINT32_MAX,::std::span{words}));
    registers[3u]=UINT64_MAX-15u;
    CHECK(!cfi::evaluate_native_debug_cfi_loongarch64_sparse(row,registers,UINT32_MAX,UINTPTR_MAX-15u,UINTPTR_MAX,{},out));
    ::fast_io::io::println("LoongArch64 CFI owned DATA: PASS; live Wasm caller/finish qualification is separate");
}


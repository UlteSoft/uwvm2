// Include-order regression for the real Windows SDK / LLVM COFF vocabulary.
// Linux supplies deliberately different macro values to catch accidental use
// of caller macros as LLVM wire constants. This test issues no native owner.
#include <cstdint>
#if defined(_WIN32) && !defined(__CYGWIN__)
# include <windows.h>
#else
# define IMAGE_REL_I386_DIR32 0x5ad1
# define IMAGE_REL_AMD64_ADDR64 0x5ad2
# define IMAGE_REL_ARM_ADDR32 0x5ad3
# define IMAGE_REL_ARM64_ADDR64 0x5ad4
#endif
constexpr auto prior_i386{IMAGE_REL_I386_DIR32};
constexpr auto prior_amd64{IMAGE_REL_AMD64_ADDR64};
constexpr auto prior_arm{IMAGE_REL_ARM_ADDR32};
#ifdef IMAGE_REL_ARM64_ADDR64
# define UWVM_TEST_PRIOR_ARM64_RELOCATION_MACRO 1
constexpr auto prior_arm64{IMAGE_REL_ARM64_ADDR64};
#endif
#include <uwvm2/runtime/lib/uwvm_runtime_native_owner_table_object_graph.h>
static_assert(IMAGE_REL_I386_DIR32==prior_i386);
static_assert(IMAGE_REL_AMD64_ADDR64==prior_amd64);
static_assert(IMAGE_REL_ARM_ADDR32==prior_arm);
#ifdef UWVM_TEST_PRIOR_ARM64_RELOCATION_MACRO
static_assert(IMAGE_REL_ARM64_ADDR64==prior_arm64);
#else
# ifdef IMAGE_REL_ARM64_ADDR64
#  error "COFF wrapper introduced an absent caller macro"
# endif
#endif
#include <fast_io.h>
namespace constants=uwvm2::runtime::compiler::llvm_jit::win64_coff_constants;
namespace graph=uwvm2::runtime::lib::details::native_owner_object_graph::detail;
static_assert(constants::relocation_i386_dir32==0x0006u);
static_assert(constants::relocation_amd64_addr64==0x0001u);
static_assert(constants::relocation_arm64_addr64==0x000eu);
static_assert(constants::relocation_arm_addr32==0x0001u);
int main()
{
    unsigned checks{};bool good{true};
    auto require=[&](bool condition,char const* label)
    { ++checks;good&=condition;fast_io::io::println("COFF_CHECK ",checks," ",fast_io::mnp::os_c_str(label)," ",fast_io::mnp::os_c_str(condition?"PASS":"FAIL")); };
    using arch=llvm::Triple::ArchType;
    require(graph::coff_absolute(arch::x86,4u,constants::relocation_i386_dir32),"i386 exact absolute relocation");
    require(!graph::coff_absolute(arch::x86,8u,constants::relocation_i386_dir32),"i386 width refusal");
    require(graph::coff_absolute(arch::x86_64,8u,constants::relocation_amd64_addr64),"amd64 exact absolute relocation");
    require(!graph::coff_absolute(arch::x86_64,4u,constants::relocation_amd64_addr64),"amd64 width refusal");
    require(!graph::coff_absolute(arch::x86_64,8u,constants::relocation_amd64_addr32nb),"amd64 relative relocation refusal");
    require(graph::coff_absolute(arch::aarch64,8u,constants::relocation_arm64_addr64),"arm64 exact absolute relocation data");
    require(!graph::coff_absolute(arch::aarch64,4u,constants::relocation_arm64_addr64),"arm64 width refusal");
    require(graph::coff_absolute(arch::arm,4u,constants::relocation_arm_addr32),"arm exact absolute relocation data");
    require(graph::coff_absolute(arch::thumb,4u,constants::relocation_arm_addr32),"thumb exact absolute relocation data");
    require(!graph::coff_absolute(arch::arm,8u,constants::relocation_arm_addr32),"arm width refusal");
    require(!graph::coff_absolute(arch::UnknownArch,8u,constants::relocation_amd64_addr64),"unknown architecture refusal");
    require(!graph::coff_absolute(arch::x86_64,8u,0xffffu),"unknown relocation refusal");
    if(!good) { return 1; }
    fast_io::io::println("llvm_jit_coff_object_graph_macros PASS checks=",checks);
}

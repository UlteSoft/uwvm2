/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)            *
 * Licensed under the APL-2.0 License (see LICENSE file).      *
 *************************************************************/
// Compile with the actual target libc and LLVM headers. Linux <elf.h>
// precedes the production wrapper just as it can through signal/fast_io.
#include <elf.h>
#include <fast_io.h>

inline constexpr auto original_dynamic_null{DT_NULL};
inline constexpr auto original_dynamic_needed{DT_NEEDED};
inline constexpr auto original_dynamic_plt_rel{DT_PLTREL};
inline constexpr auto original_version_definition{SHT_GNU_verdef};
inline constexpr auto original_version_requirement{SHT_GNU_verneed};
inline constexpr auto original_version_symbols{SHT_GNU_versym};

#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/elf_headers.h>
// Subsequent real users must see the original libc macro definitions, and
// LLVM Object's transitive ELF header must already have parsed successfully.
#include <llvm/Object/ELFObjectFile.h>
#include <uwvm2/runtime/lib/uwvm_runtime_native_owner_table_object_graph.h>
static_assert(DT_NULL == original_dynamic_null);
static_assert(DT_NEEDED == original_dynamic_needed);
static_assert(DT_PLTREL == original_dynamic_plt_rel);
static_assert(SHT_GNU_verdef == original_version_definition);
static_assert(SHT_GNU_verneed == original_version_requirement);
static_assert(SHT_GNU_versym == original_version_symbols);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::machine_ppc64 == EM_PPC64);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::section_progbits == SHT_PROGBITS);

static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_386_32 == R_386_32);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_390_64 == R_390_64);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_aarch64_abs32 == R_AARCH64_ABS32);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_aarch64_abs64 == R_AARCH64_ABS64);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_arm_abs32 == R_ARM_ABS32);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_larch_32 == R_LARCH_32);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_larch_64 == R_LARCH_64);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_mips_32 == R_MIPS_32);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_mips_64 == R_MIPS_64);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_ppc64_addr64 == R_PPC64_ADDR64);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_ppc_addr32 == R_PPC_ADDR32);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_riscv_32 == R_RISCV_32);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_riscv_64 == R_RISCV_64);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_sparc_32 == R_SPARC_32);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_sparc_64 == R_SPARC_64);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_sparc_ua32 == R_SPARC_UA32);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_sparc_ua64 == R_SPARC_UA64);
static_assert(::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::relocation_x86_64_64 == R_X86_64_64);

int main()
{
    ::fast_io::io::println("PASS actual libc/LLVM ELF headers and restored macros");
}

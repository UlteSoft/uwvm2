// Regression: a real system ELF vocabulary precedes LLVM on PPC signal paths.
// Preserve all C macros while parsing LLVM and using cold typed ELF aliases.
#include <elf.h>
constexpr auto original_elf_ident{EI_NIDENT};
constexpr auto original_elf_machine{EM_PPC64};
constexpr auto original_group_comdat{GRP_COMDAT};
constexpr auto original_property{GNU_PROPERTY_STACK_SIZE};
#define UWVM_RUNTIME_LLVM_JIT
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/elf_headers.h>
static_assert(EI_NIDENT == original_elf_ident && EM_PPC64 == original_elf_machine);
static_assert(GRP_COMDAT == original_group_comdat && GNU_PROPERTY_STACK_SIZE == original_property);
static_assert(ELF64_ST_BIND(ELF64_ST_INFO(STB_LOCAL, STT_FUNC)) == STB_LOCAL);
namespace constants = ::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants;
static_assert(constants::machine_ppc64 == EM_PPC64 && constants::flag_alloc == SHF_ALLOC);
static_assert(constants::type_relocatable == ET_REL && constants::ppc64_addr64 == R_PPC64_ADDR64);
// Parse the production relocation classifier after restoring libc macros.
#include <uwvm2/runtime/lib/uwvm_runtime_native_owner_table_object_graph.h>
#include <fast_io.h>
int main() { ::fast_io::io::println("PASS real system ELF macros restored and LLVM aliases coexist"); }

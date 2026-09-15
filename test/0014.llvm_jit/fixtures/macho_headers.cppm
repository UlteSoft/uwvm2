// Compile with --precompile, the project's src include directory and the
// matching LLVM include directory. This is a global-fragment/include-order
// regression, not a substitute for building the complete uwvm modules.
module;
#include <mach/mach.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/macho_headers.h>

export module uwvm2_test_macho_headers;
static_assert(VM_PROT_READ == 1);
static_assert(VM_PROT_WRITE == 2);
static_assert(VM_PROT_EXECUTE == 4);
static_assert(CPU_TYPE_ARM64 == (CPU_TYPE_ARM | CPU_ARCH_ABI64));
static_assert(CPU_SUBTYPE_POWERPC_603e == 4);
static_assert(CPU_SUBTYPE_POWERPC_603ev == 5);
static_assert(ARM_THREAD_STATE64_COUNT > 0);

// Genuine ARM header/module-global-fragment include ordering. This component
// requests no native execution, stack, register or Wasm debugger authority.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/arm_ehabi_registration.h>
#include <llvm/Object/ELFObjectFile.h>
#include <fast_io.h>
#if !defined(__linux__) || !defined(__arm__) || !defined(UWVM_USE_DEFAULT_JIT) || \
    !defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) || UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT != 1
# error This fixture needs the real ARM default-LLVM EHABI header configuration.
#endif
#ifndef SHT_REL
# error glibc ELF macros must remain visible after loading the real LLVM headers.
#endif
#ifndef R_ARM_ABS32
# error glibc ARM relocation macros must remain visible.
#endif
static_assert(SHT_REL == 9u && R_ARM_ABS32 == 2u);
int main()
{
    ::fast_io::io::println("PASS ARM EHABI header-first order with default LLVM selection and intact Linux ELF macros");
}

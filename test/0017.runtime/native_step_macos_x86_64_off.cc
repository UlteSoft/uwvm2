// Compile/link three actual x86-64 macOS controls: macro omitted, =0, =2.
// This component must have no Mach/MIG/Security/LLVM MC symbol dependencies.
#include <uwvm2/uwvm/debugger/native_step.h>
#include <uwvm2/uwvm/debugger/native_disassembly.h>
#if !defined(__APPLE__) || !defined(__x86_64__) || !TARGET_OS_OSX
# error These OFF controls require an actual x86-64 macOS target SDK.
#endif
#if defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1
# error The OFF dependency controls cannot qualify the enabled backend.
#endif
static_assert(!::uwvm2::uwvm::debugger::native_step::platform_available());
static_assert(!::uwvm2::uwvm::debugger::native_step::install());
static_assert(::uwvm2::uwvm::debugger::native_disassembly::decode_copied(0x1234u, {}).size == 0u);
int main() { return 0; }

#include <uwvm2/runtime/lib/uwvm_runtime_native_unwind.h>

// An allow-listed native backend can replace generated logical frames after its runtime capability/live check.
// Native POSIX CFI support does not authorize frame-pointer or raw-stack reconstruction.
static_assert(UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES == UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE);
static_assert(UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN == UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE);

// `unwind-uncheck` may skip the live probe, never the authoritative-frame-replacement requirement.
inline constexpr bool unwind_uncheck_available{
    UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE && UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES};
static_assert(unwind_uncheck_available == (UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES != 0));

int main() {}

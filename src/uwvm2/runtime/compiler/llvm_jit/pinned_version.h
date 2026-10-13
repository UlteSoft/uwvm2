#pragma once
#include <llvm/Config/llvm-config.h>

// ROS deliberately has a narrower toolchain contract than ordinary uwvm2.
// xmake verifies/builds the vendored source and links its patched archives.
// This additional check catches manually configured header/module builds that
// accidentally pick up a different system LLVM before link time. A matching
// version alone is NOT proof that any patch is installed; use xmake's source
// manifest and static-archive checks. Check the downstream suffix as well:
// upstream 23.1.1 or ROS .4 lacks the AArch64 RuntimeDyld fixes required by
// ROS's native target gate. Checking major/minor/patch alone admitted both.
// Do not relax this to LLVM_VERSION_MAJOR >= 23: early 23.0 development builds
// had a different OptimizationLevel API and the X86 mixed-signedness combine
// defect. llvm-config.h is a required public header, not the removed tool.
#if LLVM_VERSION_MAJOR != 23 || LLVM_VERSION_MINOR != 1 || LLVM_VERSION_PATCH != 1
# error "uwvm2-ros requires its bundled LLVM 23.1.1 with the documented downstream repairs; configure through xmake"
#endif

// Revision 12 requires the actual X86 aligned-frame callee-pop capability.
// The exact provider/source/archive checks remain authoritative; a version
// string or a caller supplied define alone is not a patched dependency.
#if !defined(LLVM_UWVM_X86_TAILCC_ALIGNED_FRAME_FIXED) || LLVM_UWVM_X86_TAILCC_ALIGNED_FRAME_FIXED != 1
# error "uwvm2-ros requires its bundled aligned-frame X86 TailCC capability; configure through xmake"
#endif

// No standard-library include is needed in the header/module global fragment.
// This is compile-time only, with no VM-entry or generated-Wasm overhead.
static_assert([]() constexpr
{
    constexpr char expected[]{"23.1.1-uwvm-ros.13"};
    constexpr char actual[]{LLVM_VERSION_STRING};
    if constexpr(sizeof(actual) != sizeof(expected)) { return false; }
    else
    {
        for(decltype(sizeof(0)) index{}; index != sizeof(expected); ++index)
        {
            if(actual[index] != expected[index]) { return false; }
        }
        return true;
    }
}(), "uwvm2-ros requires the exact bundled LLVM 23.1.1-uwvm-ros.13 headers; configure through xmake");

// Revision 9 includes the qualified RISC-V callee-pop TailCC ABI and the
// incoming-stack musttail repair. Ordinary uwvm2 requires an explicit opt-in;
// this capability is automatic only after the exact ROS dependency guard.
#define UWVM_LLVM_RISCV_TAILCC_FIXED 1

// LoongArch64 requires the backend capability as well as the version label.
// A pre-repair SDK with the same revision must not select this private ABI.
#if defined(__loongarch_grlen) && __loongarch_grlen == 64
# if !defined(LLVM_UWVM_ROS_LOONGARCH64_TAILCC) || LLVM_UWVM_ROS_LOONGARCH64_TAILCC != 1
#  error "uwvm2-ros LoongArch64 requires rebuilt bundled LLVM TailCC archives and matching headers"
# endif
# define UWVM_LLVM_LOONGARCH64_TAILCC_FIXED 1
#endif

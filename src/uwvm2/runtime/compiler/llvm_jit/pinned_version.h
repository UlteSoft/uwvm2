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

// No standard-library include is needed in the header/module global fragment.
// This is compile-time only, with no VM-entry or generated-Wasm overhead.
static_assert([]() constexpr
{
    constexpr char expected[]{"23.1.1-uwvm-ros.6"};
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
}(), "uwvm2-ros requires the exact bundled LLVM 23.1.1-uwvm-ros.6 headers; configure through xmake");

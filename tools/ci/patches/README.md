# LLVM 23 Windows C++20 module workaround

`llvm23-clang22-module-irbuilder-default-callback.patch` applies to an LLVM
source checkout whose `llvm/include/llvm/IR/IRBuilder.h` has three
`CreateIntrinsic` default arguments of the form
`function_ref<void(CallInst *)> SetFn = [](CallInst *) {}`. From the LLVM
checkout root, use `git apply --check /absolute/path/to/this/patch` before
`git apply /absolute/path/to/this/patch`, then rebuild LLVM and every UWVM
module PCM compiled against it. Do not apply it to a different LLVM header
without reviewing the default-argument declarations and its build provenance.

Clang 22 targeting Win64 rejected UWVM's `translate.cppm` after importing
`native_exception_landingpad.cppm`: the default lambda in `IRBuilder.h`
produced duplicate `llvm::function_ref` callback definitions with the same
mangled name across two C++20 modules. Changing the three defaults to the
same named no-op callback leaves the `CreateIntrinsic` signature and runtime
behavior intact. On the ROS bundled LLVM 23 header (pre-patch SHA-256
`45b957e68916e45dce547c992f7565194e244ae5eb4a6ad85fb71261b3232abb`),
the corresponding header after patch has SHA-256
`97c0745bca383dc6302e6a01a4a3c0b66ad61ca543d20a2c6df0edf808d8687b`.
The isolated Win64 Clang 22 rebuild of the landingpad and translate PCMs and
translate object passed at `-O3`; the unchanged header failed at both `-O3`
and `-O0`. This is a compiler/module ODR workaround, not a GC or runtime fix.

`llvm23-mingw-x64-compiler-helper-probes.patch` corrects a separate bundled
LLVM 23 cross-build issue. If CMake uses `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`,
`check_function_exists` can report unresolved MinGW stack-probe names as
present because the test never links an executable. On x86-64 MinGW, the
qualified SDK's libgcc exports `__alloca`, `___chkstk`, and
`___chkstk_ms`; it does not export `_alloca`, `__chkstk`, or
`__chkstk_ms`. The patch selects the real x64 GNU ABI names only for that
target, then leaves all other targets' original probes intact. It requires
regenerating `llvm/Config/config.h` and rebuilding LLVMSupport and dependent
archives. The bundled ROS `llvm/cmake/config-ix.cmake` SHA-256 changes from
`db72662a2b3b25b63de5255d3a9c92a2ae6c972ea771b5df04c38b3034bf2e28`
to `612618717f5cf52e8ee2f4d7b47c89d78b4999916fca591623a4a7049835ea1e`.
Apply it only after checking the LLVM source version and the target SDK's
actual symbol table.

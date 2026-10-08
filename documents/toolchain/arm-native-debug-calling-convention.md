# ARM32 Linux native debug: TailCC and EHABI

Eligible typed Wasm functions use ARM TailCC. A normal call marked notail still uses this calling convention. Upstream LLVM 23.1.1 ARM FastISel aborts on its normal-call/return paths with "Unsupported calling convention"; SelectionDAG already implements TailCC.

ROS now declines these TailCC FastISel paths before ABI assignment or instruction emission. SelectionDAG handles the unchanged ABI. Other calling conventions and the C host entry boundary are unchanged.

For external LLVM 23.1.1, apply the matching repair in that LLVM source tree:

    patch --dry-run -p1 < /path/to/uwvm2/tools/ci/patches/llvm23-linux-arm-tailcc-fastisel-fallback.patch
    patch -p1 < /path/to/uwvm2/tools/ci/patches/llvm23-linux-arm-tailcc-fastisel-fallback.patch

Rebuild the real ARM CodeGen library used by MCJIT and relink the runtime and CLI against that SDK, retaining the other native-owner/loader patches. Updating only headers or the bootstrap Clang does not repair the linked library.

Use --linux-native-debug=y with --execution-jit=llvm --use-llvm-compiler=y for both runtime and CLI. Qualification uses a fresh cache or -Rllvm-cache-path disable.

test/0017.runtime/native_mcjit_musttail.cc checks real C-to-TailCC and TailCC-to-TailCC normal calls, integer and void returns, exact numeric results and a volatile numeric side effect at O0 and optimized code generation. Existing C musttail recursion/cross-engine checks stay separate. Its private stack witness and numeric slot grant no debugger capabilities.

A Thumb-compiled host VM can debug ARM-state JIT code. The private signal adapter checks the saved CPSR T bit and aligned PC; Thumb-state captures and ISA-changing PC updates are refused. Actual object mapping metadata still rejects $t claims. Host compilation mode grants no JIT code authority.

ARM32 Linux uses its real EHABI ABI. The product bridge requires glibc 2.35 or newer, exports `__gnu_Unwind_Find_exidx`, and registers only finalized `.ARM.exidx` tables for live manager-owned JIT code. Lookup outside those exact code ranges uses glibc's loaded-object lookup. Engine retirement removes the tables before freeing code. A missing exported hook, malformed table, overlapping owners or an unqualified loader is refused before execution. No `__ARM_DWARF_EH__` override is needed.

GNU ARM EHABI `R_ARM_TARGET2` and `R_ARM_GOT_PREL` use GOT-indirect references. External LLVM needs the corresponding RuntimeDyld repair in addition to TailCC and the native-owner patches:

    patch --dry-run -p1 < /path/to/uwvm2/tools/ci/patches/llvm23-linux-arm-ehabi-target2-relocations.patch
    patch -p1 < /path/to/uwvm2/tools/ci/patches/llvm23-linux-arm-ehabi-target2-relocations.patch

Rebuild the actual RuntimeDyld archive and relink. The bridge checks the linked loader's `uwvm_llvm_arm_ehabi_target2_abi_v1` query; replacing headers alone does not qualify an old archive. `--linux-native-debug=y` supplies the ARM EHABI define and export linker option to both runtime and CLI targets.

`native_mcjit_unwind.cc` checks actual recursive JIT unwind, host-PC exclusion, engine retirement and missing-export refusal. `native_arm_ehabi_target2_relocations.cc` independently checks real ELF DATA relocations, signed addends, local symbols and independently remapped sections. Unsupported ARM relocations report a loader error rather than reaching another architecture's resolver.

Native stepping preserves private FP state and projects only exact typed Wasm values. Complete D-register carriers can preserve i64 bits; an f32 carrier explicitly zeros its unused high half. Disabled FP features keep split or stack locations refused. Product tests must separately prove large i64 values, scalar FP values and actual kernel captures; IR or relocation tests do not establish public debugger authority.

Exact live Wasm ownership and numeric provenance remain mandatory. Literal pools, VM/host code, raw stack, SP/FP/RA/TLS and unproved values remain hidden.

ARM ELF literal pools remain DATA even inside a Wasm function. Boundary
discovery uses the same live object's validated $a/$d mapping, skips only
proved aligned DATA islands and never decodes their bytes. It does not restart
after an unknown TEXT instruction, admit a pool byte as a stop/successor, or
change the separate typed numeric/code permissions. This lets real code after
a compiler-created pool remain reachable to the debugger without exposing it.

LLVM conservatively marks the HINT instruction family as reading/writing
memory and having unknown effects. Only the complete unconditional ARM-state
HINT #0 encoding and operand shape qualify as NOP. Other hints, predicates,
implicit register effects and control operations remain refused.

ARM Bcc metadata covers both conditional and AL branches. An AL branch gets
one successor only after its exact three-operand predicate shape and the
actual ARM target analysis agree; a conditional branch still requires both
taken and fallthrough boundaries. A successor in literal DATA remains refused.
The same copied mapping is rechecked when the runtime seals a breakpoint plan.

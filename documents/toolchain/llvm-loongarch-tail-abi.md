# LoongArch64 guaranteed tail-call ABI

The LoongArch64 backend must implement a callee-popped argument area before the
VM can use LLVM `tailcc` between different Wasm prototypes. Ordinary C sibling
calls cannot provide that guarantee. Apply
[`llvm-loongarch-tailcc.patch`](patches/llvm-loongarch-tailcc.patch) after the
existing LoongArch CFI repair, and rebuild all five target libraries together:
CodeGen, Desc, Info, AsmParser and Disassembler. Generated opcode/node tables and
the machine-function information layout changed; mixing old libraries is invalid.

Each tail carries `incoming_argument_bytes - outgoing_argument_bytes` through
register allocation to frame lowering. Outgoing fixed objects use that signed
delta. Growing argument areas reserve space before spill allocation, and incoming
loads finish before overlapping stores. The epilogue restores the frame and
transfers with a genuine no-link branch. Small/large symbolic transfers expand
before register allocation while retaining the delta; medium transfers retain
their existing later expansion. No ordinary-call fallback is introduced.

TailCC gives scalarized fixed vectors their own register/stack argument slots,
including vectors split into more than two integer pieces without LSX. It never
forwards an indirect pointer into an unrelated incoming prototype. Variadic and
byval tails remain refused. A function making an ordinary TailCC call with stack
arguments uses a stable frame pointer and an unreserved call frame. The callee
already pops that argument area, so the caller must not pop it again.

CFI names the caller SP before the incoming argument area. CFA and saved-register
offsets receive the same correction; physical frame indices still use entry SP.
Asynchronous epilogue rows track the actual restored registers and final SP.
TailCC functions and callers with TailCC stack arguments disable shrink wrapping
so an argument-area transfer cannot move ahead of its individual tail/return.

The ROS backend advertises `LLVM_UWVM_ROS_LOONGARCH64_TAILCC=1` in its generated
`llvm-config.h`. ROS requires that marker alongside its exact dependency version.
External uwvm2 providers may use the same marker or explicitly set
`UWVM_LLVM_LOONGARCH64_TAILCC_FIXED=1` after qualification. A version string alone
does not enable this ABI. Selection is limited to native LoongArch64; this evidence
does not qualify LoongArch32. Raw C++ entry pointers retain the C ABI.

Both products include the capability in their runtime cache fingerprint, even
when a provider reuses a version/source label. The qualified and unqualified ABIs
both retain normal cache configuration; their native objects cannot be mixed.

The private rebuilt provider passed 135 actual QEMU/MCJIT executions in the SSH
Linux 64 GiB, swap-zero cgroup: scalar/LSX/LASX, code-generation O1/O2/O3 and
small/medium/large code models. Cases execute two million spilled-parameter
rotations or four million growing/shrinking direct/indirect transfers. Mixed
float/vector bit patterns and 31 explicit result fields, native stack extrema,
exact caller-SP restoration and complete emitted function extents are checked.
The tail functions contain no link-setting call instructions.

Another 81 executions checked typed exception landing pads after zero-to-18
argument growth, direct stack-argument throws and ordinary returns. They require
the correct exception payload, live caller-stack canaries, a subsequent ordinary
call, actual registered unwind frames and restored SP; emitted CFI is retained.
See [the component receipt](../../test/0072.core3_codegen_cross/loongarch_tailcc_native.qualification.json).
These are native backend component results. The separate
[R7 CLI receipt](../../test/0072.core3_codegen_cross/loongarch_cli.qualification.json)
records 1,612 actual paired Wasm executions across all 124 current targeted
fixtures, including nine tail families, LLVM O1/O2/O3 and main lazy policies.
Signed persistent cache, T2 entry, debugger behavior and hardware throughput
have separate qualification scopes; this corpus does not cover all Core 3.

The [cache receipt](../../test/0072.core3_codegen_cross/loongarch_cache.qualification.json)
records another 1,764 signed cold/warm/warm-again processes, including 1,176
authenticated hits with unchanged object bytes. Both products cover full
O1/O2/O3 with instruction and unwind policies; main additionally covers lazy
policies and 36 actual numeric T2 processes. Default and explicit cache paths
are exercised independently. Its assembly inspection covers 456 complete
function extents. T1/T2 duplicate symbol names are resolved by identifying the
full T2 module and loop-reentry entries before inspecting the hot core.

Eighteen instruction-policy lazy process witnesses separately demand functions
2, 0 and 1, with the caller's compilation ending before its cold tail target is
demanded. They check fresh locals over two million transfers and three separate
signed objects. An unwind-policy group may materialize all reachable functions
together, so merely passing the large-body fixture is not a cold-target witness.

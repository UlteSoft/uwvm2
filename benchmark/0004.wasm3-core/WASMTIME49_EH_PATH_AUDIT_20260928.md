# Exact Wasmtime 49 exception path and a bounded UWVM implementation route

This is a source audit, not new performance qualification. Wasmtime is pinned to
49.0.1 commit `46c23a87dac1465986a8ad53ba6a7ae49372857b`; WAVM to
`4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09`. The companion
`WASMTIME49_EH_SOURCE_PINS_20260928.json` records raw file SHA-256 values, failed
path guesses, and the exact historical measurement inputs. Line numbers below
refer to those complete raw files, rather than the web renderer's collapsed
blank lines. No actual Wasmtime machine code or sampled profile was captured
for this audit, and no product source is changed by it.

## What the existing same-Wasm measurement establishes

The measured module has two functions: `$step` computes a numeric recurrence
and throws an i32 tag on one in sixteen iterations; `_start` catches that tag
with `try_table`, checks the final recurrence and checks 500,000 catches. It
does not use `catch_ref`, `catch_all_ref`, `throw_ref`, host imports or retained
exception references. Both batches executed the same Wasm SHA-256
`560243a56c6b29dbe0337548fa96626db2c168e56ea69d88208cffa4dfc48560`, from WAT
SHA-256 `106240b668266c97b8f7187a32683a038d47fae31eca2facf2ba580a04ab0aa4`.

The actual Wasmtime command was `taskset -c 0 <wasmtime49> run -C cache=n -W
exceptions=y <eh_throws.wasm>`. Its CLI SHA-256 is
`c36ef7e6548600b09bcb34cb3ee4c138f3e7b01893347ec7f48fdb8e6050aa94`.
Nine whole-process samples had medians 0.096419700 s in the ROS batch and
0.095713973 s in the ordinary batch. These include initialization and JIT;
they are below 100 ms and were collected with thermal drift, so they do not
isolate a per-throw cost or qualify a precise speed ratio. The paired UWVM
results belong only to old source IDs `c7f97991...62841` and `43a37bcf...a1c1a`,
before owned-payload, compact-trace, five-argument bridge and collector/root
changes. See [the complete measurement report](EXCEPTION_PERFORMANCE_20260928.md).

Function inlining was not enabled by that command. The pinned 49 source sets
`Inlining::No` in `crates/environ/src/tunables.rs:274`; CLI application changes
it only for an explicitly supplied option at `crates/cli-flags/src/lib.rs:1043`.
The public configuration also documents this default at `config.rs:2391`.
This rules out default intra-module inlining as a source-configuration
explanation; confirming the actual emitted callee still requires disassembly.
The collector was not explicitly selected in the EH command. The 49 source
documents Auto selecting Copying where compiled in, but that is a source
inference, not a saved per-run resolved-config record. Future EH comparisons
should record explicit inlining and collector choices. [Pinned tunables](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/environ/src/tunables.rs#L274),
[CLI configuration application](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/cli-flags/src/lib.rs#L1043),
[collector configuration](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/wasmtime/src/config.rs#L3564).

## The actual 49 propagation mechanism

| Fixed source and lines | Mechanism relevant to this fixture |
| --- | --- |
| `crates/cranelift/src/translate/code_translator.rs:638–699,4600–4668` | Legacy catch/rethrow/delegate operators are rejected. `try_table` constructs ordered catch targets. Catch entry receives an exnref via the exception ABI, unboxes a known tag's fields and branches to the Wasm label; reference variants additionally retain the exnref and mark it for stack maps. |
| `crates/cranelift/src/func_environ.rs:2407–2519` | A non-tail call with active handlers becomes `try_call`/`try_call_indirect`, with caller VM context, tags and a default handler in exception metadata. Normal results continue through ordinary SSA block parameters. |
| `crates/cranelift/src/func_environ/gc.rs:708–795` | `throw` allocates a collector-specific exception object and calls the throwing builtin. `throw_ref` reuses an exnref. Builtin callsites also carry the current handlers. |
| `crates/wasmtime/src/runtime/vm/libcalls.rs:1133–1136`; `runtime/store/gc.rs:505–547` | The builtin stores a rooted pending exception and returns an error sentinel. Tag resolution uses the defining instance plus tag index. This is not a C++ pointer exception. |
| `crates/cranelift/src/compiler.rs:320–424` | The Wasm-to-builtin trampoline saves exit FP/PC, calls the host helper, checks its failure sentinel and invokes the raising path after that helper has returned. |
| `crates/wasmtime/src/runtime/vm/throw.rs:18–129` | Handler lookup walks the delimited activation, finds each return PC's validated exception-table record, resolves imported tags dynamically and returns the matching handler's PC/SP/FP. |
| `crates/wasmtime/src/runtime/vm/traphandlers.rs:851–908,919–934` | A matched Wasm catch takes the pending exnref and exits the decision block **before** diagnostic backtrace capture. Uncaught errors continue to the entry trap and may capture a backtrace. Native execution resumes through `resume_tailcc`. |
| `crates/unwinder/src/arch/mod.rs:47–100`; `arch/x86.rs:23–42` | Resumption restores the handler PC/SP/FP and two payload registers under Cranelift's internal exception ABI. The x86 source has three control-transfer assembly instructions; this is source, not measured instruction cost. |

These paths are visible in the [translator](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/cranelift/src/translate/code_translator.rs#L638),
[call lowering](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/cranelift/src/func_environ.rs#L2407),
[exception allocation/lowering](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/cranelift/src/func_environ/gc.rs#L708),
[builtin trampoline](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/cranelift/src/compiler.rs#L320),
[tag-aware search](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/wasmtime/src/runtime/vm/throw.rs#L18),
[caught/uncaught decision](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/wasmtime/src/runtime/vm/traphandlers.rs#L851),
and [architecture contract](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/unwinder/src/arch/mod.rs#L47).

This is not an allocation-free scheme: each numeric `throw` first creates an
exception in the selected GC heap, and `UnwindReason::from` constructs a boxed
trap wrapper (`traphandlers.rs:700–710`). The copying compiler can emit bump
allocation, while retaining a collection/growth slow path. Therefore the
observed timing gap cannot be assigned wholly to diagnostics or wholly to
native unwind from source inspection alone. [Exception-object allocation](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/cranelift/src/func_environ/gc/copying.rs#L301).

## WAVM's distinct LLVM ABI path

The pinned WAVM source uses LLVM `invoke` for calls in a protected region
(`Lib/LLVMJIT/EmitContext.h`), `__gxx_personality_v0` on the non-Windows path
(`EmitModule.cpp:135–140`), and `landingpad` plus `__cxa_begin_catch`/
`__cxa_end_catch` (`EmitExceptions.cpp:175–200`). Its Windows path uses
`catchswitch`, `catchpad` and `catchret` (`:128–171`). Runtime `throwException`
and the throwing intrinsic throw a C++ `Exception*`; creation captures the
call stack, while readable names are rendered later (`Lib/Runtime/Exception.cpp:
185–208,228–252`). This supports the existing UWVM interoperability route; it
does not demonstrate Wasmtime's custom tail-call handler transfer or support
for every modern Core 3 EH operator. No throughput is assigned to syntax a
comparator rejects. [WAVM protected calls](https://github.com/WAVM/WAVM/blob/4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09/Lib/LLVMJIT/EmitContext.h),
[LLVM handlers](https://github.com/WAVM/WAVM/blob/4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09/Lib/LLVMJIT/EmitExceptions.cpp#L128),
[runtime exception](https://github.com/WAVM/WAVM/blob/4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09/Lib/Runtime/Exception.cpp#L197).

## Safe implementation sequence for UWVM

First measure the source-bound owned-payload and compact immutable trace
changes independently. Preserve the original diagnostic snapshot for
`throw_ref`, readable uncaught information, tag identity, reference roots and
allocation failure semantics. Collect CPU samples and actual JIT/bridge
objects for the same cross-callee fixture, separating payload creation,
diagnostic capture and C++ unwinding. A second walk is a concrete target;
removing it without preserving the snapshot contract is not an accepted fix.

A later Wasm-only transfer requires a new, explicitly delimited execution
protocol. All C++ payload and trace helpers must return normally, completing
their destructors, before generated code requests a transfer. Each eligible
Wasm callsite must describe its typed handler values and exact live roots;
handler entry must reconstruct LLVM-valid values and restore/unlink TLS GC
frames belonging to the discarded activations before any new safepoint.
Tail calls, OSR entries and exception-reference rethrows need the same protocol.
Unknown native-host, interpreter or tiered transitions must use a boundary
adapter that resumes genuine C++ unwinding with the owned pending exception.
Do not jump through a C++ frame with a live lease, lock, root-frame guard or
pending destructor.

LLVM 23 provides Itanium/Windows EH and SJLJ intrinsics, but its documented
`tailcc` guarantees tail-call optimization; it is not Cranelift's exception
ABI. `preserve_nonecc` is also not a portable complete register-restoration
contract. Stack maps describe selected live values; the runtime still owns
their meaning and resumption protocol. Thus changing SP/FP/PC under the current
`__gxx_personality_v0` landingpad IR would invalidate SSA, saved registers,
cleanup and GC state. A safe custom path needs either compiler-supported
handler metadata/restore lowering or an explicit continuation representation
whose frame/values are controlled by the VM. It must pass all target backends,
CFI/Windows unwind tests, nested host reentry and forced-collection root tests
before a performance comparison. [LLVM 23 EH](https://releases.llvm.org/23.1.0/docs/ExceptionHandling.html),
[calling conventions](https://releases.llvm.org/23.1.0/docs/LangRef.html#calling-conventions),
[stack-map contract](https://releases.llvm.org/23.1.0/docs/StackMaps.html).

Production release qualification remains blocked until automatic collection,
all live-root paths, cross-thread/cohort safety and RSS plateau pass. A faster
native component, caught exception or diagnostic path does not clear that gate.

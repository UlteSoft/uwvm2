# LLVM-full debugger: source, Wasm and native instruction levels

This is the implementation contract for `-m debug-jit` in both repositories.
The current product has authenticated host control, Wasm-offset breakpoints,
Wasm-instruction safe points, thread stops, Wasm locals and bounded linear-memory
reads. DWARF4/5 source-line lookup and `step source THREAD into|over|out` are
implemented and tested with real C, C++ and Rust `-g` Wasm modules. Native
single-instruction stepping is qualified on Linux x86-64 and macOS ARM64;
Windows product qualification remains in progress. Exact embedded-DWARF
file/line breakpoints use `break-source MODULE FILE:LINE`. Source-language
variables and inline caller frames remain unimplemented: `locals` reports
Wasm locals, and unavailable source mappings fail closed.

## Address and ownership model

The current safe-point location is `(module instance ID, public function index,
Wasm expression offset)`. Its source lookup adds a file, line and column.
Function generation, DWARF compilation unit, discriminator and inline call
chain belong to the planned richer stop record.
A native location adds `(JIT code owner, [start,end), native PC)`; only an
authenticated JIT owner may translate or disassemble that address. Hot
replacement publishes a new generation with a new map. Old code, CFI, line
tables and maps remain owned until all old frames have drained. The UI prints
module/function-relative addresses by default, never guest-controlled raw host
pointers.

The loader retains a bounded, immutable view of the original module image and
its `Code` section start/length. The parser already owns custom-section bytes;
the debugger must copy or pin only the `.debug_*` sections it needs before the
module image can be unmapped. Check section lengths, integer additions and all
DWARF references before making views. A DWARF address is relative to the
**contents of the Wasm Code section**, while the existing JIT safe-point offset
is relative to one function expression. Convert with the parser-proved body
start offset and verify the result lands on a decoded instruction boundary.
Neither a DWARF path nor an `external_debug_info` URL grants filesystem or
network access.

## Three step engines

`step wasm THREAD` is available without `-g` and uses the LLVM-full
instruction-safe-point instrumentation that is already launch-gated. The
existing `step THREAD` remains an alias. The stop record includes the actual
validated opcode offset and mnemonic. Into/over/out use the captured Wasm
frame depth and frame identity; a branch or tail call changes that identity.
Structural opcodes that lower to no executable code require an explicit
documented stop policy. The acceptance test enumerates each reachable opcode
and compares the reported offset with the Wasm decoder, including exceptions,
tail calls and thread handoff. A breakpoint on a non-executable offset is
rejected at registration rather than displayed as an active breakpoint.

`step source THREAD [into|over|out]` requires a validated DWARF line row for
the stopped Wasm instruction. All three policies serve C, C++ and Rust through
the same bounded DWARF4/5 line-table parser. `into` resumes through LLVM Wasm
safe points until the selected participant's function, file or line changes;
`over` also requires the current or a shallower captured guest-frame depth;
`out` requires a shallower depth. The Code-section-content offset is derived
from the parser's function expression span, and only an emitted decoded safe
point can supply that offset. Missing, duplicate, malformed or unmapped line
data fails closed. An unavailable or truncated frame trace makes `over`/`out`
unavailable before resuming. Source paths are display metadata and never
trigger filesystem or network access. A future source-variable evaluator needs
bounded location expressions and guest-memory reads; the present `locals`
command remains a Wasm-local view.

`step asm THREAD` executes exactly one native instruction of the selected JIT
activation. A stopped native PC must first be authenticated against its live
JIT code range and decoded using LLVM's target MC disassembler. The step
backend changes only that thread's native single-step state. The trap handler
records PC and generation into preallocated thread-local storage; it does not
allocate, acquire controller locks, run guest code, or mistake host-runtime
instructions for guest disassembly. It parks at a platform-defined safe
handoff and reports calls into host helpers as a boundary rather than showing
unrelated host C++ instructions. A native trap outside the owned JIT range is
passed to the platform's normal handler. W^X executable pages remain RX; the
stepper does not patch instructions or use LLDB/GDB injection.

Platform implementations share the same authenticated stop/step state machine:

| Platform | Native-step transport | Host-only control channel |
| --- | --- | --- |
| Linux | Architecture-qualified single-step trap with a per-thread `SIGTRAP`/context adapter; verify the exact PC and code owner before accepting it. | Existing inherited `SOCK_SEQPACKET` FD and optional owner-only Unix supervisor. |
| Windows | `EXCEPTION_SINGLE_STEP` through a registered vectored handler and checked `CONTEXT`; use a private inherited management handle or ACL-protected supervisor pipe, never a guest-openable VM listener. | Windows-specific host supervisor with live process/peer validation. |
| macOS | Mach exception/thread-state adapter (or a separately qualified platform trap adapter) for the selected JIT thread; fail closed on unsupported CPU/kernel combinations. | Inherited Unix socketpair and owner-only host supervisor. |

The platform backend must prove that a selected single step yields exactly one
native instruction in a tiny JIT function before it is exposed. A plain LLVM
IR `DebugLoc` is not a machine-step implementation. If the OS/CPU cannot safely
provide the trap and resume contract, `step asm` reports that exact limitation.

## UI, security and performance

The console grammar reserves `step wasm`, `step source`, `step asm`, `disassemble`
and source/native breakpoint forms. Every command uses the same bounded UWC1
request sequence and host authorization as current Wasm commands. Source and
machine operations require a fully stopped generation; resume invalidates
borrowed snapshots. The guest cannot grant itself debug access through a Wasm
import, WASI path, signal or loopback endpoint. The server owns credentials and
transport and may attach late only when the VM was launched with an explicit
debug capability. For all modes other than LLVM-full, entry to debug mode is
a fatal unsupported-mode error.

Only debug-enabled LLVM-full compilation emits Wasm safe points. The normal JIT
memory, call, exception and thread fast paths retain their existing machine
code from this source-mapping change; source lookup happens only in the host
debugger controller.
Native-unwind diagnostics and instruction-stack diagnostics remain separate
runtime policies. Source stepping reads Wasm DWARF; it does not turn unwind
into instruction tracing.

The 2026-10-02 DAP adapter repair supplies a single native instruction-position
label when the runtime has deliberately invalidated its prior Wasm trace.
The selected thread's PC, generation and origin must match fresh complete
`status` and `bt` replies. This is no reconstructed stack, source map or local
snapshot. Its opaque reference has no memory, breakpoint or disassembly
authority, and is retired on resume, stop change, error or session end. The
[DAP StackFrame contract](https://github.com/microsoft/debug-adapter-protocol/blob/main/specification.md#Types_StackFrame)
defines source-less labels with line/column zero. This adapter-only repair
still needs a Linux cgroup test receipt; it does not extend native platform
qualification or implement source-language variables.

The static non-debug code-generation check is precise: `install_debug_source_maps`
is called only in the `debug_jit` branch and the explicitly authorized
`debug_channel` branch of `run.h`. Its parser runs once before guest entry.
`controller::find_source_locked` is called only from stopped-state inspection
or the debug observer's selected source step. In full LLVM translation,
`emit_debug_safe_points` is set from the host-owned `debug_pause_control`;
the instruction-safe-point emitter returns without emitting an IR call when
that option is false. The source integration changes no LLVM memory-access
emitter and adds no per-instruction or per-memory operation to normal `-m run`.
This establishes code-path isolation; whole-program performance benchmarks
remain separate from this static check.

On 2026-09-23, ordinary and ROS O3 products passed real LLVM-full CLI source
`into` tests in the Linux 64 GiB cgroup with C/C++ DWARF4/5 and Rust `-g`
modules. A C `-g` provider called from a separate Wasm importer verified
instance module-ID alignment. Missing DWARF, duplicate `.debug_line`, and an
out-of-Code `DW_LNE_set_address` were rejected before guest resume; `over` and
`out` likewise stayed unavailable. Full debugger CLI regression passed 28
ordinary and 24 ROS processes.

On 2026-09-24, the later r16 O3 products passed 40 ordinary and 36 ROS Linux
debugger CLI processes. These include C/C++/Rust source `into`, `over`, and
`out`, rejected malformed DWARF, two successive Linux x86-64 native steps,
authenticated server control, and function replacement. The matching macOS
debugger qualification passed 33/33 checks in each repository under a 4 GiB
process limit. The r16 source-to-r17 source delta adds only pointer-safety
comments to two compiler files; Windows PE and full target-ISA qualification
remain separate acceptance work.

## Remaining acceptance plan

1. In the shared Linux 64 GiB cgroup, compile small C, C++ and Rust Wasm
   programs with and without `-g`. Check `.debug_line` and at least one inline
   frame against independent LLVM DWARF tools; step into/over/out, break by
   source line, inspect a live variable and report an optimized-out variable.
2. In that cgroup, execute the same Wasm programs without DWARF and step all
   reachable Wasm opcode families, including new Core 3 exceptions, typed
   references, atomics and tail calls, under both trace policies. Verify native
   object disassembly and exact one-instruction PC movement, including branches
   and calls, on each supported Linux CPU target.
3. Boot the existing disposable Windows VM under the **same parent 64 GiB
   cgroup** as the Linux build/test processes. Before boot, verify both child
   cgroups and CPU allocation, and leave the base disk immutable. Run the
   source/Wasm/native step suite and secure late-control tests in the guest;
   capture actual process exit codes. The old 32 GiB SIMD replay is not evidence
   of this debugger.
4. On macOS, run the debugger suite with a 4 GiB process-memory cap and an
   independent RSS monitor. Verify both repositories, native stepping on the
   host CPU, source mapping and late control. Record peak resident memory.
5. Repeat against a function replacement: old generation maps remain valid
   while retained, new stops identify the new generation, and an active target
   frame prevents publication. Compare ordinary non-debug JIT object bytes and
   a paired benchmark before/after the debug infrastructure changes.

The Wasm DWARF address rule follows the [WebAssembly DWARF convention](https://github.com/WebAssembly/tool-conventions/blob/main/Dwarf.md).
The [Core 3 specification](https://webassembly.github.io/spec/core/) defines
the Wasm instruction semantics; debug custom sections do not change them.
The Windows trap interface is [vectored exception handling](https://learn.microsoft.com/en-us/windows/win32/debug/using-a-vectored-exception-handler).

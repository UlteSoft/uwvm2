# Read-only native trap registers

The capture model below is private execution-proof data. Public Wasm-only projections and current CLI/DAP behavior are described in [README.native-register-projection.md](README.native-register-projection.md). SP/FP/control and unqualified physical values are unavailable publicly.

This change is source-only until the frozen inputs are rebuilt and the real
platform probes run. Earlier R3 native-window product receipts do not qualify
these new sources.

`native_step::with_owned_registers` authenticates the actual active session
before reading it. It requires a selected native thread, a completed native
trap, a PC inside the admitted live function range, and a captured PC equal to
that trap PC. The callback receives a fixed value copy and cannot write the
kernel context. The controller must additionally require its stopped target,
live frame/publication generation, selected top frame, and existing host
debugger credential. It must not reconstruct caller registers from this copy.
The callback must not re-enter native-step operations while transition
ownership is held.

The x86-64 register set is RAX–R15, RIP and RFLAGS. The synthetic debugger TF
bit is removed from the displayed flags. The AArch64 set is X0–X28, FP, LR,
SP, PC and CPSR; Darwin SDK accessors preserve its opaque and authenticated
pointer representation. Captured SP/FP/LR/PC values are display integers and
grant no host-memory access. SIMD, vector, floating-point, debug registers,
caller-frame reconstruction and register mutation remain unavailable.

No existing platform-admission switch is expanded. Linux x86-64 uses its
actual SIGTRAP ucontext. Windows x86-64 uses the actual VEH CONTEXT and keeps
its product qualification gate. macOS AArch64 uses the real parked thread
state; macOS x86-64 uses the exact checked protected-Mach reply and retains
its qualification gate and translated-process rejection. Other platforms
return unavailable rather than synthesizing a register stop.

The copy is made only in an admitted debugger's native trap handler. It adds
no ordinary LLVM-full polling, register read, or debugger syscall. Fresh
normal-mode generated-code inspection is still required before claiming a
measured absence of overhead.

## Qualification inputs

Run only through the designated Linux cgroup/guest keeper. Retain exact
source, compiler, binary, platform, target-process and command identities.

- `native_registers_model.cc`: canonical names, GDB-style `$pc/$sp/$fp/$ps`,
  architectural aliases, invalid names/indices, and retained-copy lifetime.
- `native_step_linux_x86_64.cc`: real selected thread, owner-bound PC, real
  RBX counter pointer and SP, normalized TF, first-gate/forged/retired query
  denial; keep the existing signal-forwarding and cancellation-race probes.
- `native_step_windows_x86_64.cc`: the same real VEH register/identity checks,
  plus existing hardware-breakpoint refusal and failed-gate child probes.
  Cross-compilation alone does not qualify real single stepping.
- `native_step_macos_x86_64.cc`: actual native TF/Mach transaction, exact MC
  boundaries, real RBX/SP/PC, TF retirement, forged/first-gate/retired denial.
  Rosetta, unavailable MIG ABI or security entitlements remain unavailable.
- `native_step_macos_arm64.cpp`: actual MOV W0,#1 followed by ADD W0,W0,#2;
  require snapshots X0=1 then X0=3 at exact successive PCs, and prove the
  first owned response stays X0=1 after the target advances and is retired.
  Also retain real bridge, cancellation, thread-port/gate retirement probes.
- Fresh LLVM-full console integration in both products: `info registers`
  before a real native trap, after `si`, during a nonzero selected frame,
  after resume/retirement and after replacement. Only the owned top native
  trap is eligible. Test the initial state and authenticated server as well
  as the direct console, and both instruction/unwind runtime policies.
- Fresh named-module compilation must include the new `:native_registers`
  partition and all three platform partitions. Check ordinary mode generated
  code independently of the cold native handler.

## Next native debugger increments

GDB distinguishes instruction step-in (`si`), instruction step-over (`ni`),
and frame step-out (`finish`). The existing retained trap permits exact
instruction stepping inside one authenticated function. `ni`, `finish` and
address breakpoints need a separate native execution plan with an admitted
callee/return owner, live code publication generations, VM-managed frame
identity and cancellation lifetime. A PC escaping the initial owner cannot
silently authorize host code or another function. Future breakpoint plans
must avoid modifying RX text or bypassing hot-replacement pinning.

References:

- [GDB register semantics](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Registers.html)
- [GDB machine code](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Machine-Code.html)
- [GDB to LLDB execution and register commands](https://lldb.llvm.org/use/map.html)
- [Darwin ARM64 register ABI/accessors](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/mach/arm/_structs.h)

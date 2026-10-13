# Proposed protected Mach x86-64 machine stepping

This independent source-only proposal is not applied, compiled or executed.
It does not alter the immutable R5/R3c packages or grant source-value access.
Only exact UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT=1 enables this
backend. xmake --macos-x64-native-step=y is default OFF and requires a macOS
x86-64 LLVM/Clang JIT build. Undefined, 0 and 2 select the existing unavailable
backend and no Mach/MIG/Security/MC helper. Existing AArch64 admission remains.

The current controller supplies the actual native execution participant,
private session, retained function owner and exact debug safepoint return PC.
The same protected per-thread exception port/thread_id and two gates used on
AArch64 select the transaction. The received port must equal our installed
private port. MIG only decodes buffers/counts; it does not authenticate a
sender, and the task-token is released rather than used as a permission.
This assumes Wasm has no Mach/native IPC/control-port capability; trusted
native host code in the same process lies outside Wasm isolation. Public PC, stop label and source
frame IDs grant no register/memory/source permission. An already installed
thread-specific debugger, existing TF or debug registers causes refusal.
Enhanced-security entitlement admission is preserved. Rosetta is declined:
Apple's sysctl.proc_translated must establish a native process (or ENOENT
according to Apple's documented native fallback); unknown errors decline.

The Mach SysV wrapper supplies return PC in R9, runs the host observer/park
with TF clear, and after its final call uses POPFQ immediately before RET.
The CPU traps after RET at the exact owner-authenticated guest PC with zero
guest instructions executed. The protected x86_THREAD_STATE64 reply copies
all kernel-provided registers and changes only TF. Continued replies request
one additional machine instruction; release clears TF and restores the port.
A branch/call may leave the current native owner: the callback copies only
integer after-PC. Controller/runtime copy/disassembly still rejects bytes
unproved by its live publication/owner; the PC is not a read credential.
No thread_set_state, writable code page, software breakpoint, DR mutation,
process injection or arbitrary native address resolver is added. Wrong
thread, Int3, non-SGL code, absent TF or incomplete state is not a host step.
The original handler receives unsupported exceptions via KERN_FAILURE.

A private assembler entry has explicit CFI for both host-call stack adjustments
and PUSHFQ/POPFQ; the ABI forwarding naked function tail-jumps without a new frame.
The parser observes only a bounded MIG old-state/reply buffer. All new POSIX
APIs bind actual Mach-O symbols through noexcept asm aliases; no C declaration
is called through a wrapper that hides its throwing function type. The normal
JIT IR/safepoint selector is unchanged: this bridge is reachable only through
the existing explicitly debug-enabled full compilation and real controller.

## Keeper qualification, pending

Do not run these on the current arm64 Mac as Intel qualification. Build in the
sole admitted Linux 64-GiB/no-swap lane with an actual x86-64 Mac SDK, libc++,
libc++abi/unwind provider and Mach-O LLVM23 X86/MC/DebugInfoDWARF archives.
Keep all main/RT/test nonidentity macros, generated headers, target/deployment,
SDK/sysroot and C++ ABI/EH flags identical; use the complete fresh current
source packet, not an old R5/R3 RT ABI. Record original argv/tool/log/source/
object/final-consumed-link SHA. Link Security/CoreFoundation from that SDK.
Generate the protected MIG server from the same SDK mach_exc.defs with the
actual -DMACH_EXC_SERVER_TASKIDTOKEN_STATE=1 command, then compile its C object
with -arch x86_64; an ARM64 server object is not this closure. LLVM MC requires
real X86Disassembler/MCDisassembler/MC as well as X86CodeGen/Desc/Info. Run only
a separately qualified Intel macOS host/guest; a remote guest requires actual
64-GiB cgroup admission and full owned process retirement. The current native
Mac observed physical <=4-GiB/no-swap rules remain in force if applicable.

1. Compile/link native_step_macos_x86_64_off.cc separately with macro omitted,
   =0 and =2 (UWVM_USE_LLVM_JIT=1), without any MIG/framework/LLVM libraries.
   Inspect actual undefined symbols and link maps: no Mach exception, Security,
   sysctl/errno alias, LLVM MC or generated native-step helper references.
2. Compile/link native_step_macos_x86_64.cc with macro exact1, real protected
   MIG object and actual X86/MC closure. Run the one fixed binary. Exit 77 is
   actual UNAVAILABLE, never PASS. Its static owned assembler leaf contains two
   lock-inc instructions. MC must decode both actual four-byte instructions;
   first trap counter=0/PC=RET target, later traps counter=1/2 and exact decoded
   boundaries. Wrong native ID, one-past owner, wrong thread, Int3, absent TF, wrong flavor,
   truncated state, other receive port, foreign/retired session are refusal controls. Released worker captures
   actual RFLAGS with TF clear, and both target/server threads must retire.
3. Inspect the actual Mach naked bridge disassembly and emitted CFI: no call or
   instruction between POPFQ and RET; correct SysV alignment and return slot.
   Only then build/run the fresh full product and official source/Wasm/native
   CLI/DAP tests under instruction and unwind policies. This backend component
   does not qualify actual guest EH, inline values, DAP panes or replacements.

Source-only inactive-branch projections are recorded separately and do not
replace actual preprocessing/link/SDK/native tests. No native test was run
while writing this proposal. Parent review must precede any live apply.

## Primary platform references

- [Intel SDM Vol3B 18.3.1.4 POPF/TF exception timing](https://cdrdv2-public.intel.com/782155/253669-sdm-vol-3b.pdf).
- [Apple XNU i386 user T_DEBUG uses EXC_I386_SGL](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/i386/trap.c).
- [Apple XNU protected exception reply and state installation](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/exception.c).
- [Apple XNU x86 register-state and RFLAGS transport](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/i386/pcb.c).
- [Apple SDK x86 state flavors and exact word counts](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/mach/i386/thread_status.h).
- [LLVM debugserver x86 RFLAGS.TF single stepping](https://github.com/llvm/llvm-project/blob/main/lldb/tools/debugserver/source/MacOSX/x86_64/DNBArchImplX86_64.cpp).
- [Apple documented Rosetta process detection](https://developer.apple.com/documentation/apple-silicon/about-the-rosetta-translation-environment).

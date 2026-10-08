# macOS ARM64 native instruction stepping

`native_step_macos.h` is an opt-in, host-only backend for LLVM-full
`step asm THREAD`. Normal JIT execution does not install a Mach exception
port, change debug registers, or branch through this backend. The controller
must retain the JIT code owner and execution lease, authenticate the
participant, native Mach thread port, exact return PC and owner interval, and
park every other guest before calling `request`.

The target gets a private, thread-scoped exception port with
`EXCEPTION_STATE_IDENTITY_PROTECTED | MACH_EXCEPTION_CODES` and
`ARM_DEBUG_STATE64`. `request` rejects any existing enabled hardware
breakpoint, watchpoint or single-step state, and rejects a prior
thread-specific breakpoint exception handler. The bridge calls
`arm_at_return` only after its cooperative wait is released. That cold
operation uses `thread_set_state` on the selected thread to arm one
hardware execute breakpoint at the authenticated JIT return PC. The final
ARM64 `RET` reaches that PC; the first Mach exception occurs **before** the
guest instruction at that PC.

The exception callback receives a protected task identity token and kernel
thread ID, never a task/thread control port. It checks the saved target
thread ID, reads the PC through `thread_get_state(ARM_THREAD_STATE64)`, and
checks the exact first PC against the owner. On the first reply it restores
the original disabled breakpoint slot and sets `MDSCR_EL1.SS`. XNU reports
the second exception after precisely one native instruction. XNU clears
the SS bit before that notification and puts zero in `code[1]`, so the
callback reads the stopped PC from the target's GPR state. A subsequent
`continue_from_trap` sets SS in the next protected reply; `release`
restores the original debug state and prior exception-port configuration.
The callback never calls `thread_set_state`, allocates, compiles, or
acquires the controller's host transition lock. An unexpected authenticated
PC or corrupted Mach gate terminates the process before guest execution can
resume. Unrelated SIGTRAPs retain the normal signal disposition; an
unrecognized Mach breakpoint returns failure to the task exception chain.

The initial arm still uses `thread_set_state` outside the exception
callback. Apple's Enhanced Security Mach IPC restrictions prohibit that
operation. `install` reads the process's
`com.apple.security.hardened-process.platform-restrictions` entitlement
through Security.framework and rejects level 1 or greater, unknown types,
or an entitlement-read failure. The host must also explicitly opt into
LLVM-full debug mode before installing the backend. Processes with that
entitlement deliberately have no macOS machine-step support; Wasm and source
stepping remain separate.

The protected callback requires the SDK's 64-bit `mach_exc_server` MIG
demultiplexer. The macOS ARM64 xmake rule generates its object with
`mig -DMACH_EXC_SERVER_TASKIDTOKEN_STATE=1` against the SDK's
`usr/include/mach/mach_exc.defs` and links Security and CoreFoundation.
The platform dispatcher now selects this backend only for macOS ARM64.
The AAPCS64 JIT safe-point bridge passes the authenticated return PC and
Mach thread identity; the disassembler copies exactly four owned bytes
before invoking LLVM MC. The ordinary `uwvm2` LLVM-full product passed
end-to-end `-m debug-jit` source, Wasm, and two consecutive AArch64 native
instruction steps under both `instruction` and `unwind` call-stack policies.
The test compiled a C/DWARF4 provider and a WAT importer and checked that
the first native result PC was the second native instruction's input PC.
`run_native_step_macos_debug_cli.py` records the exact commands, binary and
fixture hashes, instruction bytes and disassembly, and process-tree RSS.
The 2026-09-24 ordinary build used the opt-in O0 qualification profile;
the latest complete test peaked at 141,295,616 bytes, below its 4 GiB limit.
`run_native_step_macos_sources_cli.py` separately builds actual C DWARF4/5,
C++ DWARF4/5, and Rust DWARF5 Wasm under the same watchdog. All five
fixtures reached a source breakpoint, advanced to a distinct source line,
and exited normally; that complete runner peaked at 147,570,688 bytes.

Debug console input sealing on Darwin retains the host command descriptor
before guest admission. Pipes are paired with `proc_pidfdinfo` handles,
ordinary files with dev/inode/type, and PTY masters with the kernel's live
slave name. WASI `path_open` checks the opened descriptor before any
`O_TRUNC`; guest stdout/stderr aliases are rejected before the prompt.
`run_llvm_debug_sealed_macos_cli.py` verified real WASI attempts to reopen a
pipe through `/dev/fd/0`, a hardlink of a command file with `O_TRUNC`, and
the live PTY slave, plus same-file and same-pipe output aliases. Its five
CLI scenarios peaked at 136,822,784 bytes. The focused
`sealed_input_macos.cpp` test also checks an unrelated output pipe,
benign file truncation, and `/dev/null` with a PTY command input.
The latest ordinary build and transcript hashes are in
`build/wasm3-evidence/macos-debugger-r8`.

The `uwvm2-ros` product also built against its pinned bundled LLVM
23.1.1-uwvm-ros.9 on macOS ARM64. The same live CLI suites passed there:
both call-stack policies with source/Wasm/two native steps, C/C++ DWARF4/5
and Rust DWARF5 source steps, and WASI pipe/file/PTY input sealing. The
largest complete runner peaked at 179,339,264 bytes under the 4 GiB
watchdog. Its product SHA-256 and detailed JSON transcripts are stored in
`build/wasm3-evidence/macos-debugger-ros-r1` in that repository.

`python3 test/0017.runtime/run_native_step_macos_arm64.py` regenerates the
MIG server from the installed SDK and runs each build/sign/test command under
the 4 GiB process-tree RSS watchdog. On macOS ARM64 it verifies the first
stop at the guest PC with zero guest instructions executed, repeated
PC+4/PC+8 native steps, a BL→naked bridge→RET landing, exact owner/TID
rejection, unrelated SIGTRAP on selected and host threads, first-stop
release, 64 ready-cancellation rounds, and clean uninstall. An ad-hoc
signed binary with Enhanced Security level 1 must reject admission with
exit 2 instead of crashing. The qualified runs in both repositories on
2026-09-23 used at most 176,095,232 bytes of process-tree RSS during
compilation and approximately 5.9 MiB while executing.

The design follows Apple's [Mach IPC security restrictions](https://developer.apple.com/documentation/xcode/conforming-to-mach-ipc-security-restrictions),
the [XNU ARM64 breakpoint and single-step paths](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/arm64/sleh.c),
and [LLVM debugserver's ARM64 debug-register definitions](https://github.com/llvm-mirror/lldb/blob/master/tools/debugserver/source/MacOSX/arm64/DNBArchImplARM64.cpp).

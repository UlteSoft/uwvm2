# Linux generic syscall: native errno normalization

This is a source proposal, not a native receipt. Only the Linux generic
libc bridge changes. Actual i386 and PowerPC64 big-endian SDK header and
ELF-symbol metadata are copied into this proposal's evidence directory.
No compiler, linker, QEMU, product executable or profiler has been run.

The actual SDK declares `long syscall(long, ...)`. Its failure sentinel is
`-1`, with a native error in the SDK thread-local `errno` cell. Existing
fast_io Linux raw consumers instead expect a negative native error and
unchanged caller errno. The old generic bridge returned libc's `-1`
unmodified, so EBADF and ENOSYS could appear as error 1 to those consumers.

The private alias names the real `syscall` C symbol with the SDK's native
long and variadic ABI and an explicit noexcept declaration. A compile-time
signature check accepts both potentially throwing and noexcept SDK
declarations; C++ exception specifications do not determine the C ABI.
A macro-redirected/differently typed declaration requires a separate binding
and is rejected explicitly rather than silently bound to the plain symbol.

ONE syscall is issued. ONE real SDK errno cell is borrowed without pointer
advancement or escape. Its prior value is saved; a failing call's native
error is captured before restoring that cell and returned as negative long,
then cast to the existing caller-selected return type. Success retains the
complete native long result and also restores prior errno. There is no
retry, allocation, arbitrary pointer manipulation, logging or exception.
Failure with errno zero violates the SDK contract; the defensive result is
negative SDK EIO, never false success. This defensive branch is source-only
and is not presented as an observed real kernel error.

`system_call_no_return` uses the same exact private alias. A truly returning
exit-like operation reaches one cold compiler trap rather than undefined
behavior. A normal nonreturning syscall executes no additional instruction.
The fast_exit interface, inline_syscall rejection, argument packing, existing
callers and error tables remain unchanged. This bridge does not repair or
invent syscall-specific 32-bit split/alignment contracts.

The architecture selector and the amd64, AArch64, RISC-V64, LoongArch64 and
s390x direct implementations are byte-identical. Existing module wiring
already exports `system_call`, fast_exit and API12 fdatasync. No module or
shared POSIX provider leaf is edited; header and true fresh PCM tests use
that existing public surface. no_return remains header-only, as before.

Actual GNU SDK evidence provides the unredirected syscall declaration and
noexcept errno accessor plus default defined libc symbols for i386 and
PowerPC64 BE. This is metadata, not compiled ABI validation. uClibc, Bionic,
musl, mlibc, llvm-libc, PPC32, PPC64LE and other generic targets have not
received actual SDK/link/runtime qualification here. A matching function
type alone is insufficient to claim their plain-symbol/TLS closure or
absence of EH cleanup code. No absent provider is reclassified as musl.

## Fixtures and sole-keeper execution plan

All compilation, inspection and execution is queued only after root review,
in the existing sole keeper's Linux 64 GiB cgroup. No Mac compile/run is
permitted. Current core/runtime qualification has priority. A new process
must receive the fixture's exclusive disposable path; file ownership,
unlink, write, metadata and close use fast_io. The independent oracle alone
uses the actual GNU SDK `::syscall` declaration; it does not call the new
private alias, repeat its error normalization or guess a syscall number.

- `linux_generic_test_abi.h`: requires the actual generic selector and genuine
  bound GNU SDK. It rejects a direct x64 backend masquerading as generic.
- `linux_generic_syscall_nothrow.cc`: compares real success, close(-1) EBADF,
  SDK-proven invalid-number ENOSYS, sentinel errno preservation, actual retired
  fdatasync descriptor and invalid fd, six-byte file and real nonzero lseek.
  Actual long64 adds a >INT_MAX successful seek without allocating storage.
- `linux_generic_syscall_codegen.cc`: compile-only O3 callers for PID, close,
  API12 fdatasync and header-only cold trap. Inspect target IR/assembly for
  one alias call, native long result, saved/captured/restored errno, nounwind,
  and no invoke, landingpad, cleanup, retry or extra syscall. Compare caller
  IR with SDK bindings, not only the function's source noexcept spelling.
- `linux_generic_syscall_noreturn.cc`: separate new child only. Default getpid
  must reach the trap and terminate; measure the target's actual trap result.
  Do not assume x86 signal names on PPC. `UWVM_TEST_GENERIC_EXPECT_EXIT` must
  exit with status 93 through the real SDK NR_exit without returning.
- `linux_generic_fdatasync_enosys.cc`: only a NEW isolated short-lived child
  installs an actual owned seven-cell SDK BPF filter after no-new-privs.
  Named i386/PPC64 BE/PPC64LE audit constants are accepted only with matching
  actual compiler ABI; SDK absence is not guessed. The filter returns ENOSYS
  for actual SDK NR_fdatasync and allows fsync. It proves negative native
  error, saved errno, API12 preserving ENOSYS with no implicit fallback, and
  a separate explicit public fsync success. Never install this filter in the
  keeper, product/server, VM, debugger or supervisor. QEMU user-mode guest
  seccomp may be unavailable: installation failure remains pending/failure,
  never PASS. Use a real target/compat kernel process for this cell.

For EACH actual i386 and PPC64 BE closure and EACH product, record actual
compiler, target triple, SDK, libc, loader, stdlib and complete input hashes.
Build ordinary EH and `-fno-exceptions` forms at O3. Build both header and
fresh true fast_io PCM consumers with `UWVM_TEST_IMPORT_FAST_IO`; the test
oracle SDK headers precede import and fast_io itself is not preincluded.
Include actual SDK default and supported `_FILE_OFFSET_BITS=64` and
`_TIME_BITS=64` + FOB64 feature profiles without guessing missing support.
Verify all fixture and result hashes, measured native type widths and module
identity. QEMU normal functional checks do not qualify real guest seccomp.
no_return tests and expected-exit are header-only by existing export policy.

Actual WASI fd_datasync fallback and timestamp R4 consumers need their own
fresh execution against this exact bridge and actual raw nonzero coverage;
these primitive fixtures are not a substitute for product/consumer receipts.
Any unknown SDK, missing closure, absent UAPI or unavailable kernel row stays
explicitly pending. This document and source apply-checks cannot be called
native/ABI, performance, cross-platform or WASI standards acceptance.

Primary references: [GNU release 2.36 unistd.h](https://raw.githubusercontent.com/bminor/glibc/release/2.36/master/posix/unistd.h),
[GNU current unistd.h](https://raw.githubusercontent.com/bminor/glibc/master/posix/unistd.h),
and [Linux syscall(2)](https://man7.org/linux/man-pages/man2/syscall.2.html).
The actual target SDK text/symbol metadata remains the binding evidence.

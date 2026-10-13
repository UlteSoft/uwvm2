# Error-returning readlinkat: source candidate only

No files in the working repositories have been changed by this proposal. No
compiler, linker, product, guest, filesystem fixture, or performance check has
run. Header, module, EH/noEH, actual ABI, and transport qualifications are all
pending the single Linux cgroup keeper and the separately coordinated platforms.

The new `posix_readlinkat_nothrow(at, validated_native_NUL_path, writable_buffer,
count)` returns the existing `posix_read_result`. It performs one synchronous
native operation and returns positive native error codes in `error`. It does not
allocate, stat, add NUL, advance or retain pointers, retry, or change native
argument-validation order. Success with `transferred == count` may be truncation;
only the caller knows its intended complete link length. This low-level primitive
does not attempt to prove path immutability or eliminate TOCTOU races.

Linux uses the target's actual `__NR_readlinkat` and fast_io's existing syscall
primitive. Generic targets require the separate applied generic errno bridge,
plus real target execution qualification; source application is not that proof.
The kernel's fourth argument is `int`, preserving the existing raw call's native
conversion and error precedence rather than adding an early public size check.
If no target NR exists, a declared GNU SDK readlinkat interface on genuine
GNU >= 2.4 / __USE_ATFILE can use the exact `ssize_t,int,char*,size_t` SDK ABI and
noexcept asm symbol. Unknown libcs without an NR keep an uncallable public name
and retain the caller's original fallback. No BSD/Cygwin ABI is added.

Darwin SDK 13.3 and 26.4 `sys/unistd.h` declare readlinkat without a symbol redirect,
introduced at macOS 10.10/iOS 8; their `unistd.h` readlink also has no redirect.
The provider uses `_readlinkat` and `_readlink` noexcept aliases with actual SDK
ssize_t/size_t. The positive availability guard calls readlinkat on supported OS;
older OS preserve the existing AT_FDCWD-only path readlink fallback, otherwise
ENOSYS. No optional libc emulation is added on native failure.

`native.h` include and `host/posix.inc` using are proposal-only exact single-line
hunks rebased against the applied R6 timestamp foundation (native.h 5d46676c,
host/posix.inc d9769b63). Timestamp header 4a2a987a, generic bridge 9cb86a2e and
POSIX comment-only delta 87c45a99 are current source dependencies; no native
qualification is inherited. The options proposal shares that same wiring base.
The sole shared-provider owner must compose each approved include/using hunk;
never apply an entire snapshot over another newly integrated API.

The independent fixture accepts five arguments:

    DIRECTORY REGULAR_NAME LINK_NAME EXPECTED_TARGET MISSING_NAME

The keeper must prepare one isolated real directory with a regular file,
a symlink whose target matches EXPECTED_TARGET, and an absent MISSING_NAME.
REGULAR_NAME/LINK_NAME/MISSING_NAME must be relative names in that directory;
EXPECTED_TARGET must contain 1..512 characters. Fixture opens one readonly
O_DIRECTORY/O_CLOEXEC owner through fast_io and closes it exactly once. The
fixture itself does not create, mutate, unlink, or follow these input paths.
Setup failure or unavailable SDK/kernel operation is a failing/unavailable cell,
not a pass, and the isolated input directory is removed only after retirement.

The Linux oracle uses the independently declared actual SDK syscall entry and
actual NR, not the fast_io error normalizer. Darwin oracle uses an independent
SDK-typed noexcept symbol, with a genuine positive availability guard. Both
capture errno immediately and preserve the error as data before the candidate
call. Tests cover the regular-file EINVAL noEH process survival, full link bytes,
native one-byte truncation success, zero extent, missing relative path, invalid
relative directory plus zero/nonzero extent, actual closed directory, and two
sentinels/entire untouched suffix. No terminating NUL may overwrite the byte
immediately after a successful payload. No nullptr or unbounded guest pointer
is passed to either oracle or candidate.

Header mode includes the production umbrella normally. Named-module mode
includes only the independent SDK oracle/standard headers before `import
fast_io`; it may not preinclude fast_io, use a stub module, reuse a prechange PCM,
or export private ABI names. Run both products x EH/noEH with fresh provider
source/BMI/objects. Existing API14, generic, module public bridges and runtime
archive ABI form explicit source dependencies, not imported pass claims.

O3 assembly/IR inspection must prove one native readlinkat operation, no hidden
allocation/stat/retry/terminator work and no invoke/landingpad for the public
wrapper. A positive/negative/truncation run must occur on real Linux/compat
kernel targets; QEMU runs and Darwin platform runtime are distinct rows. This
fixture does not prove WASI guest rights/pointer behavior or the high-level
path-timestamp symlink probe. Those consumers remain separate narrow proposals.

The existing higher-level native_readlinkat uses stat-with-NOFOLLOW, allocates
exactly the observed size, reads once and rejects a different returned length
with EIO. Its suppressed EH errors become false is_symlink. Migrating that
consumer must preserve those semantics and publish an owned string only after
successful read; this primitive deliberately does not replace that transaction.

Primary sources consulted (source/SDK evidence, not runtime qualification):
- GNU native declarations: https://raw.githubusercontent.com/bminor/glibc/master/posix/unistd.h
- Linux implementation and int-buffer-size/error order: https://raw.githubusercontent.com/torvalds/linux/master/fs/stat.c
- Apple declarations: https://raw.githubusercontent.com/apple-oss-distributions/Libc/main/include/unistd.h
- Apple syscall implementation: https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/vfs/vfs_syscalls.c
- Actual selected Darwin 13.3/26.4 SDK declaration snapshots accompany the proposal.
The POSIX Issue 8 website returned HTTP 403 during this review; it was not used
as an invented successful source read. No upstream mutable URL is a source pin.

# Darwin read-advice error-return source proposal (API14)

This candidate adds only two specialized Darwin methods to the shared fast_io
nothrow header and paired module export. It is held for root source review;
neither vendor, consumer, old patch nor runtime has been changed by this proposal.
The before-image is the applied API13 R3 header/export in both products.

`posix_readahead_nothrow(observer, bool enabled)` performs one SDK F_RDAHEAD with
an int 0/1. `posix_readadvise_nothrow(observer, intfpos_t offset, int byte_count)`
constructs the real SDK radvisory and performs one F_RDADVISE. The latter requires
signed SDK off_t wide enough for intfpos_t. Both return posix_operation_result:
zero on native success, otherwise the native errno captured once through the
existing common helper. No allocation, exception, retry, FD ownership transfer,
arbitrary command, caller pointer, file-position change or software hint
emulation is added. Neither negative arguments nor errors are prevalidated.

The real SDK `__DARWIN_ALIAS_C(fcntl)` selects cancellation and UNIX conformance
suffixes; no `_fcntl` suffix is guessed. Public names and module using-declarations
have identical Apple/Mach/F_RDAHEAD/F_RDADVISE guards. The SDK exposes both commands
and radvisory only in its Darwin namespace: a strict POSIX namespace without
_DARWIN_C_SOURCE does not gain a fabricated fallback or Darwin declaration.

The complete native record byte representation is cleared using fast_io's
freestanding bytes_clear_n before its actual SDK fields are assigned. A single
synchronous native call borrows that owned record; no pointer advances or escapes.
This API does not grant arbitrary memory access, process control or FD acquisition.
The caller pins FD lifetime and serializes shared-open-file-description changes.

The unchanged WASI policy remains outside this API: sequential/random ignore
native hint errors; willneed first keeps its existing uint64-to-off_t/int range
skips, then maps only EBADF to EIO. Normal/dontneed/noreuse remain their previous
policy. Consumers must not merge error tables or replace existing range skips.

Native F_RDAHEAD changes the open file description's read-ahead state. Full
F_GETFL may therefore change; the tests compare SDK-observed flags after each
switch, and require unrelated access/append/nonblock/sync bits, FD_CLOEXEC,
identity, size and position to remain unchanged. F_RDADVISE is advisory:
filesystem rejection is returned faithfully, without promising physical prefetch.

The installed MacOSX13.3 and MacOSX26.4 SDK fcntl/type/cdefs/stat headers are
snapshot-hashed in this proposal. Apple source and manual references are:

- [Apple fcntl manual](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fcntl.2.html)
- [Apple XNU fcntl definitions](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/fcntl.h)
- [Apple XNU fcntl command implementation](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/kern_descrip.c)

These are source evidence, not compiler, symbol, ABI or runtime qualification.
SDK off_t is a signed Darwin 64-bit type; ra_count is actual int. XNU checks FD
lookup/type before negative read-advice arguments. A wrapper must preserve EBADF
on invalid or pipe descriptors even when offset/count is negative.

The independent native fixture owns a four-byte, immediately unlinked fresh file
through fast_io and a fast_io pipe. It compares native SDK/error behavior for
both switches, bounded hints, wide offsets, negative high-word truncation cases,
pipe and retired FD errors. It never allocates large sparse files or requests
unbounded kernel reads. Pass exactly one fresh path in a caller-owned isolated
directory; O_EXCL prevents overwriting. Its output, path creation, unlink and FD
ownership all use fast_io. The independent SDK aliases are oracle-only and exact
nothrow asm declarations; no production internal alias/conversion computes the
expected result. New module and availability fixtures require the same real SDK.

Pending execution plan: fresh Clang header and module builds, EH and noEH, for
actual x86_64 and AArch64 Darwin SDK targets; inspect optimized assembly for one
native call, intact offset/int payloads and no throwing cold path. A cross-build
or SDK static inventory is not a target-run pass. Linux keeper remains the sole
64 GiB lane and has priority LLVM repair work. This candidate schedules no test.
Any later macOS execution first needs independent Linux memory receipts and a
protective process cap below 4 GiB; no main-host unbounded build or run is allowed.

File preallocation and timestamps are separate pending API designs. Their
result/transaction and SDK time64/availability details are recorded in the
proposal's allocation-times-gap.md; no allocation/times symbol or fallback is
invented by API14.

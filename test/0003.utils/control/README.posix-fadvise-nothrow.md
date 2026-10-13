# Linux POSIX advisory error-return API13 candidate

This source-only revision 3 candidate adds `fast_io::posix_fadvise_nothrow(observer,
intfpos_t offset, intfpos_t length, int native_advice) -> posix_operation_result`
and its named-module export. It is unapplied and has no native qualification.
Both repositories must use identical header/export/test sources after root
review; the API12 before-images are pinned in the proposal manifest.

## Return contract and exact SDK ABI

The wrapper makes ONE synchronous SDK libc call and returns its POSIX result
as `.error`: zero means success; a positive native error means failure. It does
not inspect/write `errno`, retry, allocate, close an observer, mask advice,
clamp offsets, upgrade a hint, emulate an unsupported provider or prevalidate
negative values. This preserves the actual implementation's descriptor/error
precedence. A hint does not prove that cache pages were resident/discarded.
See the [Linux POSIX advisory contract](https://man7.org/linux/man-pages/man2/posix_fadvise.2.html).

The existing fast_io SDK offset alias is reused. For glibc it is `__off64_t`
with the `posix_fadvise64` symbol, regardless of FOB64/time64 selection. This
preserves a signed wide offset AND length on native i386 where `off_t` may be
32-bit. The [glibc public header](https://raw.githubusercontent.com/bminor/glibc/master/io/fcntl.h)
redirects FOB64 to this entry. Its [Linux implementation](https://raw.githubusercontent.com/bminor/glibc/master/sysdeps/unix/sysv/linux/posix_fadvise64.c)
handles aligned/reordered argument pairs; [s390-32](https://raw.githubusercontent.com/bminor/glibc/master/sysdeps/unix/sysv/linux/s390/s390-32/posix_fadvise64.c)
uses its real packed argument structure. These ABI details stay inside libc;
this candidate introduces no guessed raw syscall word order or kernel struct.
The modern 32-bit glibc wide-length symbol is the default GLIBC_2.3.3 ABI, not
the old compatibility entry with a `size_t` length. SDK major/minor macros
cannot distinguish glibc 2.3.0 from 2.3.3; automatic call selection therefore
conservatively begins with GNU glibc 2.4. Older GNU SDKs keep their previous
provider until independently bound instead of calling the wrong default ABI.

Bionic uses its real `off64_t` and `posix_fadvise64`; its [public header](https://android.googlesource.com/platform/bionic/%2B/master/libc/include/fcntl.h)
and [implementation](https://android.googlesource.com/platform/bionic/%2B/master/libc/bionic/posix_fadvise.cpp)
carry both offsets and preserve errno. Historical Bionic SDKs introduced the
entry in [API21](https://android.googlesource.com/platform/bionic/%2B/e0848bbf/libc/include/fcntl.h).
The public template name remains declared on all Linux targets, but is not
callable with Bionic API<21 or a missing Android API macro. No unavailable
symbol reference is instantiated there. Android execution remains unqualified.

An inactive alias for other Linux SDKs has their actual `off_t` and the
`posix_fadvise` symbol shape; this declaration is not symbol-availability
evidence and its public overload is unavailable in R3. The local
musl source at `../wasi-libc/libc-top-half/musl/src/fcntl/posix_fadvise.c` and
`include/alltypes.h.in` provides a source cross-check for the signed 64-bit
`off_t` and libc's raw argument adaptation; a WASI-hosted copy does NOT qualify
the actual Linux musl SDK/linker/runtime. Musl needs actual SDK/target binding
or a separately reviewed explicit capability before public calls can be
enabled; generic musl-like header markers do not establish that binding.
The new independent fixtures require the target SDK's own declarations and
types. Other libcs remain unqualified.

R3 selects only GNU glibc >=2.4 (explicitly excluding `__UCLIBC__`) and Bionic
API21. A public requires-clause permits calls only when the signed SDK offset can
represent ALL `intfpos_t` values and the SDK symbol availability condition is
satisfied. It returns no invented range error on a narrow SDK. Consumers must
use a dependent public requires-expression and retain their original fallback
for an unavailable overload; a non-template discarded branch is insufficient.
Darwin/BSD/Cygwin receive no new advisory API or emulation in this proposal.

## New independent source fixtures

`posix_nothrow_fadvise.cc` checks the actual SDK function signature independently
of the production alias and compares real libc results with the public API:
all six SDK advices, zero-length semantics, offsets/lengths beyond 32 bits,
maximum signed offsets, negative high-word truncation oracles, bad advice,
pipe errors, invalid and closed FDs, and invalid-FD precedence over bad scalar
inputs. A four-byte memfd stays in fast_io RAII ownership; descriptor flags,
status flags, position and file identity/size remain unchanged. It performs no
large sparse allocation, page-residency proof or general parser/IO emulation.

`posix_nothrow_fadvise_enosys.cc` is an isolated native Linux x86_64 LP64
process. A nine-cell filter uses only target SDK seccomp/audit/syscall constants,
kills nonnative/x32 ABI attempts, and returns actual kernel ENOSYS only for the
SDK fadvise64 syscall. Both the independent POSIX SDK oracle and public API must
return positive ENOSYS with errno unchanged. Unrelated fsync/close remain
explicit separate requests. Filter installation failure is failure, never PASS.
This fixture does not install a filter in a supervisor/product/server and is
not qualified for i386, QEMU or another syscall family.

`posix_nothrow_fadvise_module_consumer.cc` checks a fresh named-module export
and real EBADF; `posix_nothrow_fadvise_availability.cc` checks the public overload
against the actual SDK width/availability. Availability is a compile contract,
not native symbol or filesystem acceptance.

Run fresh EH and noEH header, named-module and native fixtures only through the
sole SSH Linux keeper's shared 64 GiB cgroup. Test native x86_64, actual i386
(native off_t32), i386 FOB64/time64, and available GNU-glibc AArch64/RISC-V64/ppc64 big-endian
QEMU closures with their real libc/SDK; do not substitute synthetic arch macros.
Keep every incomplete SDK/linker/runtime cell explicitly pending. Musl, uClibc,
mlibc and LLVM-libc require independent SDK binding before native API13 execution. Local Mac
compilation, execution and profiling are forbidden for this source proposal.

## Consumer and remaining public gaps

No WASI consumer is changed here. Its advice/error policy is separate: current
Linux paths map native EBADF to WASI EIO and mostly ignore other advisory
errors. The old 64-bit/noNR fallback ignores even the POSIX return; fixing it
requires a separate explicit root decision. Unsigned WASI high-bit/range
semantics must retain the original per-provider behavior and were not narrowed
by this API.

The vendor currently has no public fadvise/fallocate error-return interface.
API13 supplies only Linux advice. Darwin's current F_RDAHEAD/F_RDADVISE hints
need dedicated SDK-shape methods; they are not POSIX fadvise. Allocation needs
its own real ABI/error/semantics design: Linux fallocate, POSIX error-returning
posix_fallocate, and Darwin's multi-step fcntl/truncate transaction are distinct.
The existing public native_utimensat path throws (or terminates without EH);
there is no paired public nothrow FD futimens/at-times result in this vendor.
Time32/time64 and Darwin availability must be separately qualified. This
candidate changes none of these allocation/timestamp/provider policies.

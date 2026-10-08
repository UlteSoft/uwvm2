# WASIp1 portable UTF-8 validation and Linux reboot recovery

Date: 2026-10-05T14:32:11.607894+00:00. Repositories: `uwvm2` and `uwvm2-ros`.

This run fixes malformed portable guest mount/path metadata, restores the original Linux test cgroup after reboot, and restores the native X86 ros.11 MCJIT test SDK. The same source and regression changes are synchronized in both repositories. C++ file I/O, owned strings, printing and parsing continue to use FastIO.

## Fix and reproduction

The old implementation genuinely compiled and ran inside the recovered cgroup, printing `malformed_guest_path valid=1 encoded=1 decoded=1`. NUL-free resource strings were bounded, but their UTF-8 encoding was not checked by portable `valid()`.

Directory/file guest mount names and reopen paths now pass the existing bounded RFC 3629 UTF-8 validator before encoding, decoding or mount lookup. Invalid portable metadata returns `invalid_portable_snapshot`, including when ordinary target WASI paths allow raw bytes. Valid version 1 encoding is unchanged; empty directory-root paths remain supported. WASI argv/environment remain opaque NUL-free bytes.

Regressions cover all isolated high bytes, truncated/overlong encodings, surrogates, values above U+10FFFF, long paths at SIMD boundaries through byte 4095, valid 2/3/4-byte Unicode, raw argv/environment bytes, and invalid single/group wire data with recomputed SHA-256 digests. An invalid later group member leaves the caller's complete output unchanged. FastIO save creates no file for invalid resource metadata; FastIO load rejects validly checksummed malformed files without replacing output.

## Verified execution

| Native OS | uwvm2 | uwvm2-ros | Execution |
|---|---:|---:|---|
| Linux | 2,080 | 2,080 | Original 64 GiB cgroup |
| macOS | 2,080 | 2,080 | Local native Clang 23, authenticated process/RSS sampling |
| Windows | 2,079 | 2,079 | Actual Windows QEMU/KVM guest in original cgroup |
| FreeBSD | 2,080 | 2,080 | Actual FreeBSD QEMU/KVM guest in original cgroup |

Total: **16,638 format and filesystem assertions**. Windows omits the POSIX FIFO check. Every run requires its exact success line and exit 0. Both VM receipts record enabled KVM, unchanged base-image identity, per-test exit 0, exact check counts and normal QEMU exit 0. Target builds use the persistent genuine OS SDKs. These VM runs exercise the codec and FastIO file operations.

The genuine Linux LLVM-full runtime fixture additionally runs fresh-process save/restore under **instruction and unwind**, in both repositories: save 25 + restore 48 assertions per policy, **292 assertions total**. It uses the original owned-source/WASI initializer, real compiler publication, native cooperative pause and real typed capture owners. It verifies early invalid mount/path rejection, intact target text/FD cursor after failures, restored aliases/rights/cursor/argv/environment, preserved target file contents and resumed guest result. Fixture Wasm is parsed and validated by actual wasm-tools first.

The frozen DAP adapter protocol unit suite passes **28 tests per repository, 56 total**. These are protocol unit tests; the previous console/broker/DAP integration qualification remains in its separate historical report.

The macOS compiler/test aggregate RSS upper bound is **2,017,935,360 bytes**, below **2,147,483,648 bytes (2 GiB)**. Compiler descendants are identified by kernel birth/UID/process group, and completed runs have kernel-backed retirement checks. Test processes prohibit fork. Compile receipt arguments, compiler/binary hashes, dependency hashes and native logs are embedded in the results.

## Cgroup and SDK recovery

Another agent had already restarted the original `uwvm3-implementation` container. This run reused it without altering its limits or creating another 64 GiB group:

- Container: `bec3a6e4e013ba07c1583e46eeb81707c90c59375ae1c208a6bb87cc4f773f45`.
- Cgroup: `0::/system.slice/docker-bec3a6e4e013ba07c1583e46eeb81707c90c59375ae1c208a6bb87cc4f773f45.scope`.
- New boot: `fe09fe71-9493-4b3c-9295-694bcadeee5f`; authenticated anchor PID 9593, birth 18401.
- `memory.max=68719476736`; `memory.swap.max=0`; effective CPUs `0,2,4,6,16-31`.

An actual UID 1000 test process confirmed membership and limits. All successful Linux guards authenticate the boot/container anchor, retain caller-owned pidfds, check limits/affinity/headroom and retire only this task's descendants. Foreign agents' processes are observed and never adopted or signalled.

Reboot removed the previous temporary ros.11 LLVM SDKs. The native X86/Linux SDK was reconstructed from frozen genuine current ROS LLVM source, genuine CMake configuration and unchanged persistent base archives. **17 required source units** were rebuilt, including RuntimeDyld, AsmPrinter, RelocationResolver and X86AsmParser. The shared persistent base build remains unchanged. SDK configuration, source/dependency/archive hashes and compiler commands are captured; the real full-JIT fixture links and runs against that SDK. Other OS full LLVM SDKs were not reconstructed by this run.

A persistent archive preserves the recovered Linux source snapshot, SDK, compiled tests and evidence:

`/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-checkpoint-reboot-20261005-r3/test-environment-1791210330404465482.tar.gz`

Archive: 214,584,787 bytes; 19,856 files; SHA-256 `3cb41246b39b4a9e15aefd7b30c33cbd0feda8466753edeff9f71f6273b4e49f`. Qualification and the archive guard also reside in `/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-checkpoint-reboot-20261005-r3`. Disposable owned VM disks, ISO images and writable firmware copies are excluded. Restoring after another reboot requires authenticating the new boot and container anchor before running the guard; historical receipts retain their original identities. Persistent toolchains, OS sysroots and read-only VM bases remain prerequisites.

## Scope and concurrent edits

All six modified implementation/regression files still match the tested hashes at completion. Other agents' live edits were preserved. The only changed compiled dependency is `mcjit_target_support.h` in each repository: a PPC64 capability check changed after snapshotting. Its exact diff is recorded. The requested native X86 OS/runtime execution is qualified against the fixed snapshot; later whole-workspace edits are not silently treated as tested. Concurrent console/controller/source-expression changes did not occur in these compiled dependencies.

This patch does not implement or newly qualify whole-Wasm rollback or atomic joint Wasm/WASIp1 restoration. The existing reminder still requires capturing **Wasm and WASIp1 at the same cooperative stop**. Cross-OS state restoration continues to require independently configured target mounts/rebindings and excludes file contents. Earlier 64-way grouped checkpoint migration results remain historical; they are not presented as post-reboot results here.

The shared Linux persistent disk filled during final report publication. The incomplete upload was removed, and the final evidence is stored there as `wasip1_checkpoint_utf8_reboot_test_results.json.gz`, with its decompressed SHA-256 verified against the complete local JSON. Only this task's incomplete upload was removed. `README-evidence.txt` records the recovery instructions.

Machine-readable evidence: [wasip1_checkpoint_utf8_reboot_test_results.json](wasip1_checkpoint_utf8_reboot_test_results.json), SHA-256 `40e98b86e1a5a663572a6a854682697966eb3d57edef343944d9ce51e4c74dca`. Failed/superseded setup receipts are retained separately from successful totals.

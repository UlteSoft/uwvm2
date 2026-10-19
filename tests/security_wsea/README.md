# Embedded WSEA v2 regression tests

The current verifier consumes one WASM file with one through eight signatures
in a final `uwvm.wsea.v2`
custom section. The VM verifies only paths explicitly selected through
`--secure module` or `--secure named-module`. See the
[deployment commands and trust model](../../documents/security/wsea.md) and
[algorithm/QEMU recipes](ALGORITHM_MATRIX.md).

The completed [embedded-v2 qualification](results/20261018-embedded-v2/SUMMARY.md)
contains 27 actual VM builds and eight complete target cosign binaries. Its
`qualification.json`, `evidence.tar.gz` and `SHA256SUMS` retain source/binary
identities, exact commands, expected exit codes, guest witnesses, compiler and
runtime inputs, guarded process cleanup, successful tests and failed attempts.
These are historical observations of the recorded source cohort and environment.
They do not qualify later source changes or untested build configurations.

The separate [OpenSSL-free backend supplement](results/20261019-backend-disabled/SUMMARY.md)
covers the three complete x86_64 VMs with verification support disabled. Its
results supplement the original qualification without replacing it.

The [multi-signature/clock-commit/TLS results](results/20261019-multisignature/SUMMARY.md) separately
record the concurrent-caller time regression, default TLS products and their
current source cohort. Earlier results retain their original source identities.

## Current test entry points

| Entry point | Purpose |
| --- | --- |
| `algorithm_matrix.py` | Generate frozen embedded-v2 fixtures; run the full 277-case verifier matrix, optionally repeated through the actual VM. |
| `warm_key_matrix.py` | Repeat every case in one verifier process, including ordered rollback and revocation groups after RSA key-math memoization. |
| `embedded_receipt.py` | Invalidate issued receipts after policy, version, revocation, manifest, certificate or CRL changes/expiry. |
| `embedded_usage.py` | Private authenticated HTTPS simulation, first-gate refusal without publication, VM second-gate refusal and post-verification mutation. |
| `multi_module_program.py` | Independently selected modules under shared policy, all signing profiles, shared RSA8192 keys and a tampered final library. |
| `execution_policy.py` | Actual import capability enforcement, selected symlinks, ordinary opt-out and refused signing without overwriting the input. |
| `backend_disabled.py` | Real OpenSSL-free VM builds: ordinary guest execution remains available and explicit secure requests fail closed. |
| `concurrent_verifier.cpp`, `concurrent_verifier.py` | Eight production-verifier callers share key math while signatures, receipts and protected state remain independently checked; includes expiry and clock-floor controls. |
| `multisig_receipt.cpp` | An authenticated revocation of an additional signer invalidates an already issued genuine multi-signature execution receipt and refuses replay. |
| `multisig_usage.py` | 461 actual CLI/verifier/VM checks: all thirteen profiles and genuine RSA4096/8192 boundaries, eight distinct keys, quorum stripping, invalid extra signatures, permanent revocation and authenticated HTTPS deployment. |
| `native_sqrt.cpp` | Direct production square-root regression for native IEEE and extended-precision targets, including subnormals, signed zero and quiet NaN. |
| `startup_algorithms.py`, `startup_cosign.py`, `startup_sizes.py` | Separate process-startup observations, large modules and authenticated-prefix tail mutation. |
| `openssl_kat.cpp`, `arm_empty_concurrent_map.cpp`, `elf_audit_checks.py` | Digest/context-copy known answers, ARMv5 atomic sentinel regression and target ELF auditing. |
| `check_sync.py` | Source-only comparisons across the three repositories; it does not execute a candidate. |

`wasm-cosign/test/wsea/algorithm_interop.py` invokes the production Go command to
verify OpenSSL fixtures and sign genuine guests for the actual target C++
verifier and VM. Its `run_go_targets.py --full-cosign` mode builds the complete
cosign command tree. Supply `--go-subcommands-json '["wasm","secure"]'` for the
complete CLI; the lightweight direct entry point is recorded separately.

The complete profile executes 277 cases in each verifier/VM, hence 554 checks.
The explicit reduced product profile executes 47 cases in each, hence 94 checks.
A loader-only probe never qualifies as a guest-executing VM. Native Go against a
target VM is host/target interoperability; it is not target Go execution.
There are no standard Go Linux big-endian ARM ports.

## Execution and resource boundaries

Build and run candidates only inside the authorized SSH Linux cgroup, through
`run_bounded_v2.py`. The recorded execution uses CPU affinity `16,17`, an
unchanged 64 GiB cgroup limit with swap disabled, at most 8 GiB aggregate owned
RSS, and a 3 GiB free-space floor on the actual workload filesystem. The final
historical large builds used a 52 GiB whole-cgroup early-stop ceiling. The
multi-signature campaign initially used a 56 GiB raw-memory early-stop ceiling.
Its optional cache-aware launcher instead applies a 56 GiB working-set ceiling
under the unchanged 64 GiB hard cap, subtracting only `inactive_file` from
`memory.current`. Anonymous memory, tmpfs/shmem, active file and kernel memory
remain in this budget. No cache flush or cgroup configuration change is made,
and a qualified stage still requires unchanged OOM counters. Earlier attempts
retain their original ceilings. The 8 GiB owned-RSS, two-core and free-space
limits remain.
Do not change system clock, system trust, cgroup configuration or affinity policy.

The launcher records cgroup incarnation and ancestor limits, process birth
identities and membership, peak RSS/whole-cgroup memory, filesystem space,
before/after OOM counters and complete cleanup. A crash, signal, timeout, missing
producer or infrastructure error cannot count as a successful negative test.
Fixtures contain disposable private keys and must never become deployed trust
anchors. Fresh output directories and per-group private state are required;
never erase learned revocation or rollback state to obtain a passing result.

`tools/ci/run_wsea_embedded_targets.py` builds actual per-repository products and
runs the matrix, interoperability and warm-process suites through that launcher.
`tools/ci/run_wsea_usage_targets.py` adds HTTPS, multi-module or execution-policy
checks to completed, hash-bound products. The collector accepts only complete
matching rows and retains failed attempts with their original outcomes.

`tools/ci/run_wsea_thread_local.py` builds the default thread-local interpreter
configuration and runs matrix, full-cosign interoperability, warm-verifier,
HTTPS, multi-module and execution-policy suites. Its default targets are
x86_64/aarch64; `--all-qualified-targets` includes all 27 repository/target pairs
from the supplied original qualification. `--build-admission-mib` changes only
the pre-build wait threshold, retaining at least 4 GiB below the explicitly
configured bounded early-stop ceiling. `--working-set-limit-mib` opts into
cache-aware admission and requires the unchanged 64 GiB hard cap with at least
8 GiB of working-set reserve. `--bounded-launcher` selects its immutable driver.
`--resume` requires complete matching build dependencies, binary hashes, suite
records and successful closed guards. A complete guarded intermediate build
can also be reused while its execution suites run with fresh output/state.
Verifier concurrency is separate from multithreaded guest execution; neither
LLVM/JIT nor C++ language-module builds are qualified by these recipes.

For the separate OpenSSL-free build configuration, reuse an immutable qualified
cohort, compiler and QEMU runtime. On the SSH Linux host, with these absolute
paths already supplied by the administrator:

```sh
taskset -c 16,17 python3 "$REPO/tools/ci/run_wsea_backend_disabled.py" \
  --workspace "$WORKSPACE" --cohort "$COHORT" \
  --qualification "$QUALIFICATION_JSON" --support-source "$REPO" \
  --revision fresh_run --cg-limit-mib 53248
```

This controller serially builds all three full x86_64 VMs without OpenSSL,
executes their actual QEMU/loader prefixes, and retains independent build/test
guards. It does not modify the earlier cryptographic qualification. The build
helper's explicit `--disable-wsea` option omits OpenSSL headers, archives and
definitions; `backend_disabled.py` verifies the actual compiler dependencies
and 21 behavior/identity checks for each product.

## Historical migration artifacts

`run.py` generates detached-v1 fixtures and is retained for historical evidence;
it does not qualify current embedded-v2 verification. Its older reports,
`verifier_runner.cpp`, `cli_loader_runner.cpp`, `cli_initializer_runner.cpp` and
`run_bounded.py` must not be substituted for the current recipes above. A parser,
loader or initializer probe without a guest backend is explicitly limited to
those integration paths.

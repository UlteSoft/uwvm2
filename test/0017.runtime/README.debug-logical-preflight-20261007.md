# Native logical declaration preflight — 2026-10-07

Both repositories now check the declaration types of both native C/C++/C23
`&&` and `||` operands before evaluating either value. A skipped operand is
not read, including its extent callback or arithmetic. A selected value must
agree with the inferred width, signedness, floating/category representation,
known standard integer identity and known canonical declaration identity.
The existing shared numeric profile keeps its previous lazy contract.

| Expression in the finite native profile | Result |
| --- | --- |
| `0 && (1 / 0)` | false; no division or value callbacks |
| `1 || (1.0 % 1)` | unsupported type; no value callbacks |
| `0 && missing` | unavailable declaration; no value fallback |
| `0 && declared_local` | false if its declaration is supported, even when its copied value is unavailable |
| `left && right` | declaration checks for both; left value first; right value only when needed |

These are expression/component examples, not a new live `-Rdbg` session.
The controller's two existing expression paths already supply selected-frame
metadata inside their authentic stopped transactions; no new memory, pointer,
frame, host or ASM authority is introduced. C/C23 predicate results retain their
native signed-int rank/name `int`; C++ results retain `bool`.

This is the existing **finite numeric** expression model. Pointer truth,
user conversions, overloaded logical operators, calls/side effects and complete
native-language expression rules are still unqualified. Missing metadata is
explicitly unavailable. Numeric results cannot grant guest/host read access.

## Current qualification

All compiler, Python and executable checks ran on SSH Linux in the birth/PIDFD
verified original 64 GiB cgroup, swap disabled. DATA jobs kept the original
1 GiB aggregate owned RSS budget; controller frontends kept 16 GiB. Every output
class kept 64 MiB, files 8 MiB and logs 1 MiB. All three QEMU profiles shared
one cross output class. The hard 16 GiB filesystem and reserve guards stayed
unchanged; no OOM was observed.

- 5,083,900 repeated checks, including 196 native numeric type combinations per component, NaN/-0/infinity truth, selected-value mismatches and bounded type traversal.
- 40 genuine Wasm32 C17/C23/C++20/Objective-C/Objective-C++ producers: DWARF 4/5, O0/O2; 2,240 actual declaration probes and 240 new compiler type assertions.
- 36 independent native compiler refusals for six invalid expressions in C17/C23/C++20 across both repositories. Exact old/new header comparison fixes 12 wrongly accepted results.
- 148 DAP protocol tests plus the unchanged 595/330 syntax corpus; these use protocol DATA, not a genuine IDE/runtime session.
- 36 main regressions plus 2 unsigned-host-char followups across PPC64 big-endian, AArch64 and x86_64. Actual ELF identities and emulator/provider dependencies are pinned; outputs exactly match pinned current x86_64 component output.
- Native C witnesses contain 40,000 source iterations and 80,000 source-selected calls across both standards/repositories, with 28 static type assertions. The optimizer may reduce the fixed loops/calls; these are semantic witnesses, not an executed instruction/performance count.

| Repository | Qualification | Phases/components | Status |
| --- | --- | ---: | --- |
| uwvm2 | native | 81 | PASS |
| uwvm2 | actual-LLVM-metadata | 24 | PASS |
| uwvm2 | controller-frontend | 2 | PASS |
| uwvm2 | ppc64 | 6 | PASS |
| uwvm2 | x86_64 | 6 | PASS |
| uwvm2 | aarch64 | 6 | PASS |
| uwvm2-ros | native | 81 | PASS |
| uwvm2-ros | actual-LLVM-metadata | 24 | PASS |
| uwvm2-ros | controller-frontend | 2 | PASS |
| uwvm2-ros | ppc64 | 6 | PASS |
| uwvm2-ros | x86_64 | 6 | PASS |
| uwvm2-ros | aarch64 | 6 | PASS |
| uwvm2 | ppc64-unsigned-char-followup | 1 | PASS |
| uwvm2 | aarch64-unsigned-char-followup | 1 | PASS |

All current native, metadata, frontend and QEMU records use the fresh A5 entire immutable source manifests. Every current job was repeated after the host-char fixture correction. Production preflight bytes have not changed since A1. Frontends cover interpreter/JIT-disabled and LLVM-JIT/interpreter-disabled paths, with syntax checking only. No fresh linked full product, live frame transaction, full language/native parity, other architecture product or complete language debugger is claimed. ROS mode policy and the Wasm-only ASM boundary were unchanged.

Successful job work sums to 1216.822 seconds; all retained attempts sum to 2801.296 seconds (job-work sums, not elapsed wall time or performance measurements). DATA RSS peak: 791,691,264 bytes; frontend: 2,195,857,408 bytes.

## Retained failures and evidence

Five failed earlier jobs are retained: A1 new fixture asserted the wrong internal C predicate category; A2 historical C23 test expected an undeclared dead symbol to succeed; A3 baseline verifier used 3 instead of unavailable=4; A4 PPC64/AArch64 new fixture assigned a signed Wasm plain-char identity to native unsigned char. All production preflight bytes have stayed unchanged since A1. A5 corrects only copied fixture metadata for unsigned host char and freshly repeats every current native, LLVM metadata, frontend and QEMU job. Fifteen earlier jobs passed separately; 51 compiler/executable phases passed in failed native attempts and ten other target components passed in the two failed QEMU jobs.

The artifact collector also retained one failed compatibility attempt: historical failed QEMU rows lack the success-only stdout hash field. The corrected collector verifies every retained phase log hash and completed under the original guard; no implementation/fixture was changed. This is separate from the five earlier failed test jobs.

The Linux-only main archive is `/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/logical-preflight-a5/logical-preflight-evidence-a5.tar.xz` (102,382,776 bytes), SHA-256 `645f8e5cbfb5d6d917cd14e8e7729ec382b55856c6e02092e767e73f1619d422`. Every regular member hash and fsync were verified; it retains all attempt source reconstructions, logs, argv/dependency hashes, ELF/emulator and provider restoration evidence. External SDK archives remain separately pinned. Small reports/sidecars are on both hosts; no local copy of the main archive is claimed.

Remaining work includes exact `size_t` identity, complete aliases/CV/reference/glvalue and per-language syntax/semantics, native Go/Rust/Zig/AssemblyScript runtime features, fresh live product qualification and remaining architectures. This followup closes one correctness gap; it does not close the full language matrix.

## Primary contracts

C++ [logical AND](https://eel.is/c++draft/expr.log.and) and [logical OR](https://eel.is/c++draft/expr.log.or) require Boolean conversion and preserve left-to-right short circuit. [C23 draft N3096, 6.5.13/14](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf) requires scalar operands and `int` results. [GDB C/C++ operators](https://sourceware.org/gdb/current/onlinedocs/gdb.html/C-Operators.html) documents operand-type restrictions; no full GDB extension/overload parity is inferred.

## Later concurrent workspace changes

After all snapshot files matched the local checkout before report publication,
final review observed new unowned changes in six paths in each repository:
`runtime/wasm_threads/wait.h`, `utils/thread/cooperative_pause_domain.h`,
`uwvm/debugger/wasip1_portable_checkpoint.h`, `uwvm/wasm/loader/wasm_file.h`
(all under `src/uwvm2/`), and fast_io `fast_io_core_impl/mode.h` plus
`fast_io_hosted/platforms/nt.h`. These changes are preserved and are **not**
qualified by this followup. The ten owned implementation/test paths still match
the frozen A5 inputs; the source-cut results above must not be presented as a
qualification of the latest entire working tree. The small evidence sidecar
retains exact before/after hashes in `late-parallel-source-changes-a5.json`.

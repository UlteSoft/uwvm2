# sizeof extent contract followup — 2026-10-07

Both repositories now apply the same native C/C23/C++ extent contract to direct evaluated sizeof and declaration-only sizeof inference. The copied extent must be unsigned, non-floating, non-Boolean, guest-width numeric DATA and fit that width. A Boolean extent previously passed inside nested sizeof and dead logical/conditional arms although direct sizeof refused it. A Wasm32 extent above UINT32_MAX previously became a truncated result or passed declaration-only inference. Both now return unavailable without publishing a result or calling a dead operand/extent resolver.

This rejects a Boolean **extent carrier**, not sizeof of a Boolean source object. The actual controller's size callbacks construct unsigned guest-width byte sizes from the selected declaration. Historical two-argument metadata callbacks describe the scalar operand; signed/narrow/Boolean/f32 operand types remain valid. The shared_numeric/TinyGo/Go default contract remains unchanged. Zero and UINT32_MAX/UINT64_MAX extents remain supported as owned DATA; this does not grant a memory copy or make a real declared object available. No new runtime frame, pointer, location, write or host-ASM authority is added.

## Frozen A5 verification

All compiler/executable/functional Python tests ran on SSH Linux behind the original birth/PIDFD supervisor, inside the original 64 GiB cgroup, swap=0 and original CPU set. DATA retains its 1 GiB aggregate owned RSS budget, frontend 16 GiB; each original role retains the shared 64 MiB output cap (all QEMU profiles combined), 8 MiB file/1 MiB log caps, and admission/disk reserves. All successful owned process trees were retired/reaped. No OOM event changed.

| Repository | Route | Phases/components | Result |
|---|---|---:|---|
| uwvm2 | native | 43 | PASS |
| uwvm2 | index | 66 | PASS |
| uwvm2 | frontend | 2 | PASS |
| uwvm2 | QEMU-x86_64 | 8 | PASS |
| uwvm2 | QEMU-ppc64 | 8 | PASS |
| uwvm2 | QEMU-aarch64 | 8 | PASS |
| uwvm2 | QEMU-riscv64 | 8 | PASS |
| uwvm2-ros | native | 43 | PASS |
| uwvm2-ros | index | 66 | PASS |
| uwvm2-ros | frontend | 2 | PASS |
| uwvm2-ros | QEMU-x86_64 | 8 | PASS |
| uwvm2-ros | QEMU-ppc64 | 8 | PASS |
| uwvm2-ros | QEMU-aarch64 | 8 | PASS |
| uwvm2-ros | QEMU-riscv64 | 8 | PASS |

- 114 exact predecessor/fixed comparisons across both repositories: 36 Boolean type-only cases and 21 Wasm32 out-of-range cases per repository, including direct truncation. Each comparison isolates the exact captured pre-fix scalar header while holding current captured sibling/dependency bytes fixed.
- 2,265,860 repeated new extent/category/range/callback checks across native plus four QEMU architectures. These are repeated finite assertions, not distinct debugger features.
- 64 actual target ELF/QEMU components across x86_64, big-endian PPC64, AArch64 and newly added RISC-V64 coverage of this followup, both repositories. ELF machine/width/byte order, actual deps/link providers and emulator bytes are pinned; stdout matches the successful pinned native component logs. This is component coverage, not full runtime/hardware/native-language parity.
- 120 freshly compiled genuine Wasm producer modules across C17/C23/C++20/Objective-C/Objective-C++, DWARF 4/5, O0/O2; 960 Wasm32/64 embedded size-type proofs and 1040 target static type assertions. These are size/rank regressions, not authentic malformed live extents.
- 2,704,290 size_t rank checks, 5,083,900 logical preflight checks, 1,700,000 scalar properties, plus existing character/narrow/conditional/Boolean/Zig/DAP components. 148 DAP protocol tests and the 595-positive/330-negative grammar corpus.

The RISC-V sysroot is reused read-only from another work stream. Its 2606 files and 10 packages match the pinned provider manifest before/after; actual used deps/providers are separately hashed. No peer SDK or process was modified or removed. This does not qualify that stream's complete LLVM SDK/product.

## Retained attempts and environment

4 failed jobs and 9 earlier passed jobs remain in the evidence. A1's fixture used a raw char tab rejected by the updated fast_io print contract; it now uses string literals. A2 over-constrained shared_numeric dead logical type-query counts; its legacy no-query behavior remains unchanged. An A1 ROS index run aborted on a supervisor proc-exit observation race and was fully retired. A3 native aborted when the shared kernel memory.swap.max drifted to max; the guard retired only its owned tree. Docker's existing 64 GiB/swap=0 configuration was reapplied, observed swap use was zero, and the original limit was restored without signaling peer processes. A3's completed followup qualifies the earlier Boolean-only cut, not the later range fix. A5 freshly qualifies both fixes. The empty A4 transport attempt failed the unchanged 10 GiB transfer disk admission floor before writing bytes or launching tests; its observation is retained. A local administrative generator's missing json import affected only its final cohort record and was corrected; no test launched from that failed generator.

Earlier source/transport duplicates were retired only after content-addressed archive verification. Exact raw transport bytes and old source cuts are reconstructible from the final archive. New immutable source files share an inode only with identical files within the same A5 cut; no links to peers or older cuts were created. Cleanup retired 410,015,011 own duplicate bytes, while current A5 source, all logs/ELFs and external providers remain. The small sidecars are copied to both hosts; the large archive stays on Linux.

## Evidence and remaining work

- uwvm2 source ID: `sha256:8179056827908d1af707e72981543137d5459dc3c77148ef6099b132baa3efa3`
- uwvm2-ros source ID: `sha256:628d29f32f1d7e699982f60d35d8f0b6526a6ff9e29d7292b189050520656bdc`
- Linux-only main archive: `/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/size-extent-a5/size-extent-evidence-a5.tar.xz` (92,420,352 bytes), SHA-256 `e675406fbc24a08b3fd7de171f01a0c47e7bea11ff7a0c0899af90ac78c76c6c`; 5799 members verified and fsynced. Eight source cuts reconstruct directly or through the exact bundled early archive. External compiler/standard library/SDK provider files are separately pinned; this is not a self-contained SDK bundle.
- Machine record: [debug_size_extent_qualification_20261007.json](debug_size_extent_qualification_20261007.json).

Full scoped aliases/CV/reference/glvalue/overload grammar, WALI/other ABI profiles, user-type/VLA/function sizeof, language-specific Rust/Go/TinyGo/Zig/AssemblyScript semantics, a freshly linked live -Rdbg product and remaining architecture products are unfinished. ROS full uwvm-int/full LLVM-JIT policy and Wasm-generated-context-only ASM boundary are unchanged. Results qualify the stated source cut and finite routes; they do not qualify the latest whole workspace or imply all Wasm/language features.

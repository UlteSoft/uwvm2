# Exact original ABI checkpoint continuation

Source-only fixtures and tests: no native qualification is recorded in this packet.

`debug_checkpoint_actual_exact_abi_tail_runtime.cc` takes a freshly parsed modern WAT-derived Wasm and explicit `instruction` or `unwind`. Run separately against each direct, indirect, ref, and finite ordinary-recursive fixture. It performs actual full compilation, capture, refusal while original native execution exists, authentic retirement, true nested restore, real re-pause and second retirement, then a second actual restore. Expected result is 1176 and every original prefix remains 1. This proves only a real execution slice, not whole-instance rollback.

The new raw v2 contract remains seven arguments but first compares all actual buffer owners with the canonical dispatcher TLS. The typed clone has the original ABI and `musttail` remains immediate-call/return. Original ordinary selfcalls keep their public normal target. Saved original parameter locals are loaded only after complete owned bounds and all parameter initialization flags.

The LLVM component verifier checks C/Fast prototypes, return/parameter inreg, ordinary recursive callee identity, unchanged public zero inputs and selected private input getters. It does not mint VM restore authority.

Required native qualifications: fresh two products, four fixtures, both strategies, native x86-64 and available AArch64/RISC-V64/i386 targets inside the sole original 64 GiB cgroup. No-exceptions and unavailable-native-thread builds must compile/link with inert internal fallbacks. Verify generated native tails and precise GC/EH cleanup separately.

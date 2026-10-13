# GC and EH performance: source and measurement boundaries

This append-only revision records what the existing measurements establish.
It does not edit their archives, source contracts, product identities or
qualification fields. Later measurements belong in a new revision rather
than replacing this file. All native work remains with the sole Linux keeper;
this revision was produced using pure file/hash/JSON/arithmetic review.

| Evidence | Actual scope | Supported result | Remaining boundary |
| --- | --- | --- | --- |
| Original 1,024-root / 16M immutable GC ring | struct.new, exported table set/get, ref.cast, immutable struct.get | Historical two-product JIT observation around 117–118 ns/step | Not the new mutable-header component; fresh current product and same-Wasm remeasurement pending |
| New long component eighteen plain rows | 4M mutable-header allocations with native root slots; separate 16M mutable access controls | All eighteen semantic/timing rows qualify; single-CAS allocation pair reductions about 5.2% with actual sysfs series | Whole component gain, not this immutable Wasm, collector-only gain, multi-publisher acceptance or industry ranking |
| First long component pure-HW group | Four allocation rows plus a fifth numeric mutation attempt | Four allocation counter rows have raw enabled = running; original fifth row failed ownership transition | Overall group remains incomplete; fifth failed row is not requalified by a later run |
| New targeted two-row mutation HW group | Same original long source and baseline ELF; numeric 1get/1set and reference 2get/1set | Internal API 10.781090875 / 20.5804713125 ns/unit; actual final-guest raw PMU proof and two complete retirements | Whole-process counters include untimed work; differing workloads cannot isolate one barrier by subtraction; no terminal classifier branch triggered |
| Old R3c 64M EH cold/plain/HW | Old binary, actual two auto dispatch strategies and exact 4M catches | Ten qualified rows with independent source/guest/oracle proof; raw counters full enabled/running | Not a new paired-provider result or isolated throw latency; old native dispatch and auto dispatch remain distinct |
| Old EH hardware call-stack CSVs | Actual target-filtered profiled process/DSO samples | Real libunwind debug-opcode self samples and caller observations | Skipped-stack warnings prevent full caller-path percentages; no complete stack or exclusive throw-only attribution |

## Immutable original ring versus components

The original Wasm SHA is
`66874f9a4a0a5977b4ace1cc702f949c6c2627311dce0bcbe87c818c55516e3e`;
it runs 16M steps and returns `493211925`. It is the exact table/cast/get chain
requiring fresh JIT and matching host-callee assembly. Existing cast/get
witness and allocation co-location are already implemented. The next bounded
proposal must first establish whether actual code still repeats membership
authentication; it must preserve roots, owners, null/subtype/bounds traps,
debug/trace exclusions and the original per-Wasm failure sites.

The long native component fixture SHA is
`6c266f3d99f8430ab940c19cb9ad5da1307f08cd4b85b12de926b9edc0098c82`.
Its full supplied component identity is
`1ecc4c18ea2f52fdf7c221124baf9034708a31e3ddb021003f322ad9b06416ce`,
which includes benchmark/test/tool leaves. Its independently defined
production-only fingerprint is
`5012cc9a5b470cc00a6b5e56dc62f30e28546eee98717167bd97f23fa8351ca2`.
These two meanings must not be interchanged after changing a fixture or tool.
Both baseline and single-CAS native executables use the actual paired static
ABI1/__1 runtime closure, not a new full LLVM product qualification.

The eighteen-row long archive is 116,179 bytes with SHA
`6679fe7ac56b6a4402c1da1c503c38a5399baae295cd13f6293b3c3c159dd0cb`.
Its independent audit is retained in
`build/wasm3-evidence/current-native-GC-long-P0-independent-review-20261003-r1/`.
Single-CAS remains default off: real concurrent publisher/foreign handoff and
full-product same-Wasm gates remain necessary. Dense64/65 trace/collection
experiments and scalar packed-data endianness are separate axes, not fixes
for the whole immutable ring.

## New targeted counters, independent arithmetic

The 110,396-byte targeted archive SHA is
`547fac561ab97af369fdc7dd53882c773ed4214f39cb35fa71c38f13750eb7e1`.
The independent audit checks all 48 payload hashes/sizes, exact closure,
original process identity/ACK/retirement and affine-prefix/full-period LCG
oracles. Its dual-repository evidence lives in
`build/wasm3-evidence/current-native-GC-targeted-HW-independent-performance-review-20261003-r1/`.

Numeric timed API is 172,497,454 ns for 16M units; reference is 329,287,541 ns.
Numeric whole-process cycles/instructions are 961,600,818 / 2,907,594,187;
reference counts are 1,771,105,645 / 7,021,427,920. Both actual event groups have
enabled = running: 174,097,384 ns and 330,894,320 ns respectively. Each original
raw record retains the pending-attribution marker; qualified rows add actual
final-guest event-open proof without changing those raw measurements.

Observed sysfs medians are 5,470,885.5 and 5,295,209 kHz. These are not effective
ROI GHz and do not justify normalized cycles or latency. The measured numeric
and reference loops contain different fields and operations. Whole-process
counter totals also include final collection and qualification; they cannot
be called exact loop cycles/instructions. Neither actual run triggered the
new terminal-classifier branch. The old fifth-row failure stays false.

## Exception and provider evidence

Old EH source/build, plain timers, whole-process grouped counters and VTune
profiles remain separate. A collector/search helper's sampled self time is
not throw/catch latency, and an incomplete inclusive call stack cannot prove
all callers. The new paired Release libc++/libc++abi/libunwind provider has its
own source/configuration/ABI closure; assertions, version, compiler and build
configuration all differ from earlier data and cannot be attributed to one
flag without a controlled comparison.

The separately reviewed default-off phase-one observer proposal retains the
original complete eager trace path. It does not become a safe replacement
until a real provider protocol records every relevant unwind step, maintains
original-IP diagnostics and handles uncaught, catch/rethrow/throw_ref,
cleanup/RAII and nested/foreign outcomes. No arbitrary personality-only hook
proves complete frames. Its temporary-provider/source/native controls remain
separate from the current parent-owned full-product build.

## Next actual decision

Parent-owned R3 full-product build and exact immutable-ring JIT/host assembly
come first. The [finite next-cell plan](CURRENT_R3_IMMUTABLE_GC_ASM_HW_PLAN_20261003.md)
then keeps unprofiled wall/internal timers, whole-guest non-sampling counters
and hardware Hotspots/uarch in distinct namespaces. Existing completed native
component rows are not repeated. No Wasmtime, WAVM, Java or CLR comparison is
declared current until the same platform, actual pinned implementation,
feature/heap configuration and independently equivalent workload have been
executed.

# Distinct narrow conditional expressions — 2026-10-07

Both `uwvm2` and `uwvm2-ros` now accept the finite numeric `?:` cases whose
different declared integer widths/signs or Boolean/integer categories establish
a common arithmetic result. The earlier implementation rejected all pairs of
narrow operands, including cases that both C and C++ promote to signed 32-bit
integer on the supported guest ABI.

```text
print THREAD STOP 1 ? (signed char)-1 : (short)2
print THREAD STOP 0 ? true : (unsigned char)255
print THREAD STOP 1 ? (bool)255 : (signed char)-1
print THREAD STOP 1 ? (0 ? (signed char)1 : (short)2) : 0
```

These produce signed 32-bit values `-1`, `255`, `1` and `2`, respectively.
The existing stopped frame, participant, epoch and incarnation checks still
own reference resolution. Both arms are queried for type metadata; only the
selected arm is evaluated. An unavailable unselected value or unselected
arithmetic fault is unused, and a selected copy must match the declared type.
Neither numeric syntax nor this type helper grants a guest memory read token,
host pointer, live frame or permission to debug the VM.

The helper is shared by recursive inference and value evaluation. It uses
explicit integer/Boolean metadata from `from_dwarf_numeric` or parsed casts.
Unclassified narrow callbacks do not establish the new distinct-type proof.
Copied Boolean storage is normalized to truth during integer promotion.

Same narrow width/sign and two Boolean arms remain unavailable. In particular,
equal representation does not establish C++ exact type identity (`char` and
`signed char` can share it); same-type C++ results can retain their narrow type
or glvalue category, while C applies usual arithmetic conversions. The new
guard also refuses two classified Boolean arms even with wide storage. This
change does not implement enum/pointer/class results, bit-field rules,
overloaded conversions, reference/glvalue identity, or a complete evaluator
for any source language. Rust/Go/Zig/AssemblyScript retain their separately
documented language and producer limits.

The rule was checked against [C23 draft N3096 §6.5.15](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf),
[C++ conditional result rules](https://eel.is/c++draft/expr.cond) and
[integer promotions](https://eel.is/c++draft/conv.prom). C++ typed cast
prvalues are compared with compiler-computed `decltype` and values; C17/C23
use independent `_Generic` static assertions and runtime conversions.

## Qualified scope

All executable tests and builds ran on SSH Linux, in the reused 64 GiB/swap-0
cgroup, behind the owned-process/PIDFD supervisor. The Linux boot is
`d9ee997c-9129-43ea-a0f3-c9785344bba8`; keeper PID/birth are `9769/16929`.
The existing ext4 arena was remounted after reboot; its hard limit stays
16 GiB and filesystem ID stays `12191019222208619510`.

| Check | Result |
|---|---:|
| Successful bounded jobs | 6 |
| Paired native phases | 48 |
| Actual QEMU components (PPC64 big endian + x86_64) | 32 |
| C++ distinct-type fixture assertions | 72420 |
| Cast expressions per component, both guest widths | 2000 |
| Declared-reference expressions per component, both guest widths | 2000 |
| C17/C23 compiled type-pair assertions | 80 |
| C17/C23 runtime value checks | 1440 |
| Original old-header refusals reproduced | 8 |
| DAP frame protocol tests | 66 |
| DAP corpus per native job | 263 positive / 223 negative |
| Retained spacing cases / ABI rows | 6276 / 12552 |
| Retained scalar property checks | 1020000 |
| Input hash checks | 71852 |

Each QEMU job builds actual target ELF, validates machine/width/byte order,
runs eight components and compares outputs with separately pinned native
results. Existing conditional/metadata, scalar property, Boolean cast, Zig
preflight, cast spacing and DAP bridge regressions accompany the new matrix.
All regular input hashes and actual compiler dependencies are checked again.

This is component DATA qualification. It does not qualify a fresh full
`-Rdbg` executable, a real producer/stopped guest session, a native GDB
comparison, complete language feature parity or the other cross architectures
whose SDK providers have not been restored. Earlier full-product results
remain separate and keep their original source IDs and limitations.

Two failures are retained: the first source upload omitted the required
cgroup guard and stopped before compilation; the second passed the numeric
and compiler checks but used an outdated frame-protocol mock against the
already present bounded Wasm locals pagination protocol. A parallel task had
updated this mock. That byte-pinned test fixture was captured separately,
paired with the exact original adapter/corpus, and the native jobs were
rerun successfully. This follow-up does not claim the peer mock as its edit
and does not change the production DAP adapter.

Successful guarded jobs took 708.12 seconds in total;
all retained attempts took 832.06 seconds.
Peak owned aggregate RSS was 786006016 bytes
(749.59 MiB), below the 1 GiB component limit.
Maximum observed owned output was 55005722
bytes (52.46 MiB), below
64 MiB. The supervisor retains the 1 GiB filesystem and 25 GiB host disk
reserves, finite deadlines/file limits and birth-bound retirement checks.
OOM counters did not increase.

## Retained evidence

Ordinary source cut: `sha256:2367c6cb853f701535cee8240a34acb385c06c2d3bd3bd669d7fa9e4c012483c`.
ROS source cut: `sha256:87c42c8c8af038a168a0658ea3cc637fe282bffb6d88b6d9e6cf73f7c6326bd7`.
The five implementation/fixture/registry/corpus files are paired byte for byte;
mode scope and native/ASM boundaries are unchanged.

Linux evidence: `/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/conditional-distinct-a1/conditional-distinct-evidence-a1.tar.xz`.
Local evidence: `/Users/liyinan/.codex/artifacts/uwvm2-conditional-distinct-evidence-20261007-a1/conditional-distinct-evidence-a1.tar.xz`.
Archive: 31676736 bytes, 7159 regular members,
SHA-256 `9cccad09b4f6153d76752369bbe06333e09137d0d45bc0a8cb527f1226bdfe05`. Every member was verified on Linux and locally,
with both copies fsynced. Inputs include immutable source cuts, actual
native/target ELFs, dependency hashes, compiler controls, failed and passed
receipts, old source header, peer mock snapshots, pinned QEMU binaries and
the prior dual-verified provider archive reference.

After archiving, the 16 GiB image occupied
2001489920 physical bytes
(1.864 GiB).
Prior evidence, foreign output and foreign processes were preserved.

Machine-readable details:
`debug_conditional_distinct_qualification_20261007.json`.

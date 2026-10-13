# Real native DAP stepOut acceptance

`run_dap_native_finish.py` uses the actual secure broker, stdio DAP adapter and
an independently qualified full interpreter/full LLVM runtime. Its fixed
`fixtures/debug_native_finish_dap.wat` contains a recursive function called with
depth two from a repeating root. An actual root breakpoint first synchronizes
an asynchronous attach; three subsequent recursive entry breakpoints establish
the complete call chain before a genuine native SI trap is requested.

The driver requires three real physical returns, using omitted, statement and
instruction DAP granularity. Each stepOut is written in the same batch as old
register-scope, frame-scope and code-reference queries; those previously valid
handles must immediately fail. Current native frames support readonly register
watch queries: the driver first proves that `$pc` matches the actual native stop
and that `$sp` and `$fp` are unavailable, with no memory references. It then
queues a previously valid `$pc` watch along with the other old references;
all four must fail after each return. Equal recursive return PCs are allowed:
fresh stop IDs and
the actual expected parent identities establish movement. Output events must
contain the exact captured before/after PCs.

Fresh parent frames have only the Registers scope, with SP/FP unavailable and
no register memory references. A disassembly handle can return an explicit
invalid filler for private glue/spill/control instructions; a visible row must
name the captured PC. Native code references cannot read or write memory. A
root stepOut must refuse before entering a host caller and retain the real stop.
The original adapter and broker Popen objects are retired and waited; the broker
waits its original guest. This test does not claim a natural guest exit.

Run only through the existing keeper in its 64 GiB/no-swap Linux cgroup. First
parse the WAT with the qualified wasm-tools and validate with `--features=wasm3`.
The keeper command arguments are:

```sh
python3 test/0018.debugger/run_dap_native_finish.py \
  --uwvm "$QUALIFIED_FULL_RUNTIME" --wasm "$VALIDATED_WASM" \
  --policy instruction --out "$NEW_EVIDENCE_DIRECTORY"
```

Repeat with `--policy unwind`; add `--ros` for the ROS product. The external
keeper receipt must bind the actual full source, TU layout flags, SDK archives,
compiler dependency closures and link inputs. The driver's product hash alone
does not establish that qualification.

The October 6 R36 final acceptance used both immutable R35 qualified full runtime
products and the R36 current adapter/broker/test view. All four cases passed:
12 actual parent returns, 36 immediately queued register/scope/code stale
denials, 12 old-frame watch denials, 24 native-code memory denials and four root
finish refusals. The R36 watch denials used expired frames; they do not establish
that current native register watches are unsupported. The R37 driver adds
positive current-frame watch checks before and after each physical return. Both fixtures passed official Wasm 3 validation. This is Linux
x86_64 native acceptance; it does not qualify other operating systems, an actual
IDE UI, every Wasm 3 feature, or parallel C++ changes outside the R35 source cut.


`run_dap_native_finish_timeout.py` uses the separate
`fixtures/debug_native_finish_timeout_dap.wat`: a callee with a proved Wasm
parent loops until its actual Wasm global is changed. A real native stepOut
must reach its execution deadline without reporting a completed parent return.
The controller gives event/TLS retirement and all-participant Wasm parking one
separate shared two-second drain budget. It publishes a fresh stop ID only
after the actual cooperative pause is confirmed. DAP preserves the timeout
diagnostic, and four queued old references must immediately fail. The driver
then mutates only the real Wasm keep global, continues the guest, and requires
its actual exit reply and DAP exited/terminated events. Terminal events are
drained as actual framed messages after the response, rather than inferred
from the adapter's return code.

Use the same keeper arguments with `run_dap_native_finish_timeout.py` and the
separately parsed and validated timeout fixture. The October 6 R37 acceptance
passed both repositories and both instruction/unwind policies: four recursive
DAP cases, four actual nonreturning deadlines, 84 positive current native
register watches, 64 immediately queued stale-reference denials, 24 native-code
memory denials and four root refusals. All four timeout guests naturally exited
with code zero and emitted actual DAP exited events. The readonly console
`status` and `wait` replies now update DAP state immediately; an exit reply no
longer loses its exited event to a later closed-endpoint idle poll.

Both repositories also passed 12 genuine console scenarios and 246 DAP unit
tests each (one skipped test per repository). R37 rebuilt each main and console
TU with the full interpreter/full LLVM flags, using the immutable R35 source
cut plus the controller deadline repair. The original qualified runtime and
host-api objects were reused: their compiler dependency closures exclude
controller.h, and the repair changes no ABI fields. The one original qualified
SDK was reused. Current Python inputs were captured separately, including
concurrent float-literal validation edits; unrelated parallel C++ changes and
other native platforms remain outside this acceptance.


R37 qualification binds its immutable controller and Python snapshots. Later
parallel C++ or Python edits, including the subsequent WASIp1 acknowledgement
parser update, are preserved and are outside this acceptance. The native timeout
dispatch and complete-status observer repaired here retain their tested code.

The optional `capture_jit_symbols.py` helper reads only a caller-owned ordinary
process blocked at `UWVM_PROFILE_READY`. It does not use ptrace or GDB and does
not write target memory. The runner checks target admission, capabilities,
affinity and identity before releasing the gate. A metadata failure cannot
supply attribution; a critical safety failure prevents gate release.

`vtune_reports.hardware_reports(..., target_pid=target.pid)` exports raw summary
CSV/XML, a function report, and all PID/TID/logical-CPU/core-type/absolute-IP
hardware events twice: `values` and `samples`. Only the actual `Hardware Event
Sample Count:` columns qualify the minimum of 10 retired and 10 reference-clock
samples. The values report is scaled and must not be relabeled as samples or
exact retired instructions. The actual `runsa.options` target PID and sampling
backend are checked; emon/aggregated counting cannot qualify a guest-exclusive
profile. Reports use the runner's owned-process callback, or the same owned
process helper for standalone offline use.

For `uarch-detailed`, a PID-filtered VTune `exec-query` report uses
`row-by=Process/Thread/PMUCoreType` and `column-by=GEMetrics`. The recorded Thread
TID must also occur in the admitted target sample table. Retiring, Front-End
Bound, Bad Speculation and Back-End Bound are VTune's formulas, preserved with
the complete raw row; the helper does not calculate or renormalize them.
Default VTune summary CSV can omit these tables even when detailed events were
successfully sampled. `summary.xml` preserves the actual event/sample table.

To join a completed run, first generate qualification reports using the same
result. Then, inside the original admitted SSH Linux container:

```sh
python3 engines/join_jit_samples.py \
  --profile /absolute/profile/receipt.json \
  --qualification /absolute/report/hardware-qualification.json \
  --out /absolute/new-join-directory
```

The default metadata is `profile/jit-metadata/receipt.json`. The join performs no
target access or new collection. It binds PID, retained start-time, boot,
binary/Wasm SHA, actual module argv, capture helper SHA, captured executable
maps, registered ELF hashes and the exact sample export/result. Registered
ET_REL STT_FUNC ranges are rederived as section `sh_addr + st_value` and checked
against saved executable maps. Same-Wasm names come from that unchanged Wasm
file. Unmatched or ambiguous IPs remain explicit; coverage is reported per
event using actual sample counts. A failing input check retains a failure
receipt and never invents a symbol.

Only Wasm function index/name attribution is supported. This does not identify
JS/C# managed methods interpreted inside a runtime, source lines or inlined
source callees. Short setup probes demonstrate the mechanism only. The actual
AS setup probe `vtune-jit-symbols-setup-probe-r1` joined all 85 retired and 11
clock samples: index 21 `runKernel` had 72/9, and index 17 array `__set` had 13/2.
Its `performance_eligible=false` is retained. The original failed detailed
setup receipt is retained unchanged; its separate offline requalification
confirms target samples and VTune formulas without making it a final profile.

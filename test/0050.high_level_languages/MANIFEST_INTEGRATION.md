`prepare_cases.py` keeps the original default functional matrix unchanged.
Repeat `--additional-manifest PRODUCER_CASES.json` to append real producer
ordinary/Wasm-debugger cases after the defaults. Case names must be unique
across every input and must use letters, digits, dots, underscores or hyphens.
An added row cannot replace a default case.

Each added row requires `name`, an absolute existing `wasm` path, its exact
lowercase `wasm_sha256`, and string `expected_stdout`. The artifact must be a
core Wasm module. `args`, `load_args`, `features`, exit expectations, DBG
timeouts and other producer metadata remain as declared. Optional source or
oracle SHA fields are checked when present. A language tag, when supplied,
must be one of `java`, `csharp`, `assemblyscript`, `javascript`, `moonbit` or
`python`. No execution/debugger success is inferred by manifest merging.

`merge_timed_cases.py --source BASE_TIMED.json --source PYTHON_TIMED.json
--out NEW_TIMED.json` accepts the original five language identifiers, or those
five plus `python`. Missing an original language or adding an unknown one is
an error. The original six routes remain valid because MoonBit has two
backends; a Python route can bring the cohort to seven. Every timed row still
uses the existing Wasm identity, unique-name, stable-output and timer checks,
and must reference an existing generated source and independent oracle file.
These example names denote future real producer inputs, not supplied fixtures.

For Python timed cases, the current generic interface supports
`kernel_time="ns"`, `kernel_clock="monotonic"`, a declared
`clock_line_prefix`, stable `checks`, and string-array `args`. Benchmark
artifacts use `purpose="ordinary-long"`. Independent hardware artifacts use
`purpose="hardware-gated"`, exact READY/DONE markers on the declared stream,
single-byte G/D stdin gates, and stable semantic output only after D. The
timed interval excludes initialization, warmup and the gates. Preserve the
actual Python clock kind and producer oracle; do not relabel another clock.

The runners consume a normal WASIp1 command entry. UWVM passes `load_args`
before `--run`; external benchmark engines currently ignore `load_args`.
There is no generic case-level `entry`, environment, file-preopen or companion
module mapping. A self-contained Python command with embedded/frozen modules
fits the current interface. Any external resources or custom exported entry
require explicit per-engine recipes and actual compatibility checks before
comparison. Appending a script through `args` also requires real CLI checks
on each engine; manifest validation alone does not prove argument handling.

`summarize_benchmark.py` derives its case count from the manifest and its seven
engine columns, so seven ordinary routes produce 49 requested pairs. Ordinary
hardware profiling selects cases by name and can request 7 x 3 x 2 = 42 runs.
`engines/summarize_hardware.py` now requires the exact frozen `--manifest` and
derives the full route x three repositories x two analysis kinds set. Seven
routes therefore require 42 unique profiles. The matrix manifest, every
module/product/helper identity and every raw qualification report must match
that freeze. `--expected-boot` defaults to the current boot; supply a historical
boot explicitly only when summarizing that historical complete cohort. Missing,
duplicate, foreign or mixed-boot rows cannot qualify. Keep the seven completed
old-boot profiles and the interrupted attempt separate from any new cohort.

Only static integration has been prepared. No real Python manifest has yet
been merged or executed by this change, and no mock PASS evidence is created.
Wasm debugger checks do not establish Python source-line or Python-local
variable support; record those capabilities separately.

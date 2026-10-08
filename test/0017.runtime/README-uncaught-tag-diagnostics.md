# Core 3 uncaught tag and retained-trace diagnostics

This is a SOURCE-only regression closure until the Linux keeper runs it against
fresh binaries built from the exact coherent source cut. No native execution,
WAT parse/validation or rendered output has been performed for this proposal.
It does not qualify whole-VM checkpoint recovery or all exception state.

The two direct-throw fixtures instantiate two distinct tags with the exact same
explicit type 0 and payload i32=42, then throw tag 0 or tag 1 from the same
function 0. The report must show the genuine defining instance's public tag
index and original type index, rather than equating tags by signature/payload.
The third fixture catches tag 1 through `try_table/catch_all_ref`, returns its
actual nonnull `exnref`, and invokes `throw_ref` in function 3. Its immutable
throw-site trace must still start at original function 0 and exclude function 3.
The ordinary top-level entry is function 2 in that case. This fixture performs
real Wasm execution; no debug file, numeric count or fake native packet mints an
exception root, tag owner, code owner or restore permission.

Quoted Core 3 identifiers carry newline, ASCII ESC, quote and backslash into the
actual optional name section emitted by wasm-tools. Every raw name byte must be
escaped before diagnostic output. The runner checks original throw-function
name rendering and both enabled/disabled color modes. It strips only real ANSI
SGR sequences before checking text, and rejects any guest ESC that remains.
Quoted identifiers and escaped UTF-8 names are standard Core 3 text syntax:
[official text values](https://webassembly.github.io/spec/core/text/values.html#text-id).
Exception identities are instances, independent of a structural function type:
[official runtime instances](https://webassembly.github.io/spec/core/exec/runtime.html#exception-instances).

The candidate production helper moves the existing formatter verbatim except
for actual tag metadata and checked `frame_count/frame_at` output. This avoids
`frames()` allocating a second expanded vector and name copies inside a
noexcept fatal report. It does not change exception construction, normal calls,
frequently caught throws, generated IR, instruction/unwind selection, reference
payload decoding or existing WASIp1 behavior. Float payloads remain exact bits;
reference output remains opaque and does not dereference host/guest referents.
Source/instruction locations remain explicitly unavailable, not reconstructed
from a stack already unwound. This is not an allocation-failure execution test.

The runtime-private admission helper requires a real active entry and, on
threaded builds, the actual private execution-lease borrow before touching its
pinned registry. Each local-tag record is borrowed only after bounds checks;
its retained identity is compared before module/public index/type metadata is
used. Imported aliases display their same real defining provider. A retained
identity no longer present in the admitted current registry produces a clear
unavailable label. No pointer reinterpretation, signature substitution, old
world relabeling, parser tag-name fabrication or new public capability is used.
Current optional name storage has no tag-name map, so none is invented.

Run only after ROOT releases the coherent source to the existing SSH Linux
keeper and all compilation/runtime tasks remain in its <=64 GiB cgroup:

```
python3 test/0017.runtime/run_uncaught_exception_tag_diagnostics.py \
  --uwvm /absolute/fresh/uwvm --wasm-tools /absolute/wasm-tools \
  --compiler llvm-jit --mode full --out /absolute/new/result-dir
```

For ROS add `--ros`; ROS admits only full. To cover interpreter run a separate
new result directory with `--compiler int`. Ordinary can also use `--mode lazy`.
JIT defaults to instruction and unwind policies, each with both color settings;
three cases therefore yield 12 actual product cells per requested JIT mode.
Interpreter yields six per mode. `--wasmtime` optionally adds an independent
uncaught-exception oracle. All WATs first go through official wasm-tools parse
and validate; a parse/validation failure is a failure, not a skipped PASS.
The driver refuses macOS/other systems, checks the repository's real cgroup
preflight before launching tools, disables core dumps, and bounds each subprocess
to 30 seconds. JSON records binary/tool/input SHA values and every real command,
exit code, check and log. These pins are DATA receipts, not runtime permissions.
No benchmark, all-platform support or all input combinations are claimed.

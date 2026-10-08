`run_collection_pause_domain.py` tests the general host pause coordinator. It
does not prove Wasm VM enrollment, safepoints, GC root enumeration, tracing or
reclamation. The root contexts used by the fixture are ordinary native objects.

The checked header is SHA-256
`24d994ccf848429a1e9a0fa206ead3747fe4daaecceefbdb6582c5b02d8856cb`, and the
protocol fixture is SHA-256
`7cec5d4a91b8fc9acf82bc6aa9d834d53b8a90a7d009cb96e16c605d1e7fd326`.
The runner rejects a mismatch. Intentional revisions require explicitly passing
the new expected hashes and keeping their review and qualification evidence.

On SSH Linux, run inside the configured cgroup, after its current owner releases
the shared memory and CPU window:

```sh
UWVM_TEST_CPUSET=0,2,4,6,16-31 \
python3 test/0017.runtime/run_collection_pause_domain.py \
  --cxx /toolchain/bin/clang++ \
  --out build/wasm3-evidence/collection-pause-linux-r1
```

Linux requires the exact 64 GiB hard limit, no swap and the verified 4 P plus
16 E CPU set. The runner pins its work to E-core 16, builds serially, rejects
commands starting at 48 GB of cgroup memory, and kills only its own command
process group at 56 GB, a 2 GiB command RSS limit or the deadline. It records
the cgroup before/after and rejects any new OOM/OOM-kill event. Environment
library paths remain those of the qualified toolchain; the runner never changes
`HOME`.

Every command also requires at least 512 MiB of free disk space at startup and
is stopped if space falls below the 64 MiB evidence reserve. These thresholds
are explicit options; do not lower them to compete with another user's build.
Only this command's descendant processes are stopped, including a compiler
in a separate Mac watchdog session. Partial runs never count as qualification.

The Linux header fixture runs at O3 and at O1 with ASan/UBSan and address
sanitizer's leak detection enabled. Host builds use the product's 4 KiB stack
probe flags. `--include-tsan` adds a separate TSan build and run. Sanitizer
failures and timeouts fail qualification and retain their complete logs.

The module test precompiles the **production**
`uwvm2.utils.thread:collection_pause_domain` partition, a focused primary that
exports it, and their object files. Its consumer imports the primary and runs
the complete same protocol fixture without including the coordinator header.
The primary export is checked against the production `impl.cppm`. A separate
consumer compile without the PCM bindings must fail with a missing-module
diagnostic. The focused O1 module test does not build the full thread module
graph or the VM's optional backend graph.

Protocols include a self-initiating collector, 8 readers across 128 epochs,
rejection of a peer pretending to own the initiating lease, blocking roots,
admission while paused, timeout, competing collectors, close versus commit,
ticket drainage, RAII and native exception unwinding.

The explicitly authorized Mac portability check requires an explicit SDK and
limits every compiler/test command to at most 512 MiB RSS. Keep the whole
serial runner under the existing 4 GiB watchdog as well:

```sh
python3 test/0017.runtime/macos_rss_limit.py --limit-bytes 4294967296 -- \
  python3 test/0017.runtime/run_collection_pause_domain.py --platform macos \
  --cxx /Library/Developer/CommandLineTools/usr/bin/clang++ \
  --sdk /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
  --out build/wasm3-evidence/collection-pause-module-macos-r1
```

On Mac, leak detection is explicitly disabled because this profile does not
qualify LSan. O3, ASan/UBSan, actual exported-module execution and optional TSan
remain separate recorded checks. `summary.json` records actual argv, raw exit
statuses, commands/log hashes, direct source hashes before/after, observed
compiler dependency hashes, PCM/object/executable hashes and memory peaks.
Small direct source inputs are copied into the evidence directory before
compilation, so future edits cannot erase the actual runner/fixture version.
Existing output directories are never overwritten. A failed profile leaves a
failed summary; a later retry must use a fresh evidence directory.

The Mac module commands explicitly enable `-fcxx-modules`: the checked
AppleClang 21 driver otherwise treats standard module declarations as ordinary
unrecognized tokens under `-std=c++26`. The compiler's `clang++` invocation name
is preserved, even when it is a symlink to `clang`, so linking selects the C++
runtime. The resolved compiler executable is hashed separately.
The selected sanitizer runtime directory comes from the actual compiler's
`--print-runtime-dir` and is prepended to the child-only `DYLD_LIBRARY_PATH`.
This avoids a project environment selecting a different compiler's ASan/TSan
dylib. The inherited/effective paths, runtime hashes and raw child statuses are
recorded; the shell environment remains unchanged.

# Independent profiling guest

This source is a copy of `ProgramTimed.cs` with a separate stdin protocol. The
original semantic and timed producers do not include it. The semantic checks,
Kernel(1), clock resolution, and input-stream creation finish before the exact
`UWVM_PROFILE_READY` line. The host waits for that line before starting its
collection, then sends byte `G`. The existing kernel alone is timed with
Stopwatch. The guest emits exact `UWVM_PROFILE_DONE`, flushes, and waits for byte
`D`; the host stops collection before sending `D`. The original semantic and
kernel-clock lines print only after that acknowledgement. No debugger is
needed by this protocol. Physical profiler collection includes the small
stdin/marker control operations around the strictly bracketed kernel ticks.

Use only the existing admitted SSH Linux container, pinned official .NET
8.0.408/Mono WASI 8.0.15/wasi-sdk20 environment. Invoke `bash build-profile.sh`
with a new absolute `csharp/target/build-profile` output directory. Native .NET
and official Wasmtime must pass the same gated oracle before qualification.
Calibrate an ordinary kernel to about 2–15 seconds, up to 100 million
iterations. The new Wasm hash and calibrated manifest must be recorded after
the separate build; no unbuilt hash is claimed here.

`check_references.py` checks native .NET and Wasmtime at 1000, 20000 and 1000000
iterations by default, with an independent integer oracle. It proves both
gates actually block, requires exact original semantic output after `D`, and
validates positive monotonic ticks/frequency. Repeat it with the calibrated
iteration count, then use `write_manifest.py --references REPORT --iterations
COUNT --out MANIFEST` to write the common flat timed schema with gate fields.
The manifest writer requires both reference runners' passing hash-bound rows.

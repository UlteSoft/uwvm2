# Profile-only reference qualification

The independent producer built successfully in 11.02 seconds on CPUs 6–7 of
the existing SSH Linux 64 GiB cgroup. It retains the fixed official .NET
8.0.408 / Mono WASI 8.0.15 / wasi-sdk20 recipe and native Wasm symbols.

Native .NET and Wasmtime 42.0.1 passed at 1000, 20000 and 100000000 iterations
(six runs). Every run proved READY→G and DONE→D blocking, then produced the
exact original semantic line and valid positive ticks/frequency. An independent
integer oracle matched all checksums. At 100 million iterations, checksum was
3872049787; kernel ticks measured 0.136835725 seconds native and 1.165498391
seconds Wasmtime. These are reference results on CPUs 6–7 while other build
tasks had separate CPUs; they are not the final UWVM calibration or benchmark.

Wasm SHA256 is
`4281f6a2a6d380b8909708560ae7d5ed6121317df047daf3340ab15572fd2271`.
`profile-cases.json` uses the common flat timed schema with stdout gate fields,
monotonic ticks, exact semantic check, new hash, generated source and oracle.
It selects the existing maximum 100-million iteration argument; the root
runner must still calibrate UWVM ordinary execution in a quiet window.

`summary.json`, `build-result.json` and `references/report.json` bind source,
artifact and script hashes to the actual results. All owned compiler servers
and reference children retired. Approximately 106 MB of reproducible compiler
intermediates were removed; native output, Wasm/PDB, exact relink command and
receipts remain in `csharp/target/build-profile`. The original semantic and
timed Wasm hashes were rechecked as unchanged.

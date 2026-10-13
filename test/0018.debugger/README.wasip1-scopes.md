# Standard WASIp1 debugger scopes

The adapter exposes four lazy read-only scopes on the current cooperative
physical Wasm frame: `WASIp1 arguments`, `WASIp1 environment`,
`WASIp1 descriptors`, and `WASIp1 preopens`. A module with no admitted WASIp1
environment displays an explicit unavailable row. An empty admitted environment
returns an empty list. Native, inline and saved caller rows grant no new WASIp1
scope.

Obtain the `frameId` with `stackTrace`, then the reference with `scopes`.
The example IDs below must be replaced with IDs returned at the current stop:

```json
{"command":"scopes","arguments":{"frameId":14}}
{"command":"variables","arguments":{"variablesReference":23,"start":63,"count":80,"filter":"indexed"}}
```

`start` counts list entries, so the 64th descriptor may be `fd 82` after guest
FDs were closed. The returned name preserves that guest FD. The custom
`uwvm/wasip1State` request continues to use a numeric guest FD cursor for
`fds`/`preopens`; it accepts at most 64 rows. Standard scopes convert their
dense ordinal by scanning authenticated sorted guest metadata from zero.
Arguments/environment use their original vector index directly.

Omitted or zero `count` follows [DAP VariablesArguments](https://raw.githubusercontent.com/microsoft/debug-adapter-protocol/main/debugAdapterProtocol.json)
and returns all remaining entries within a 1024-row budget.
A start beyond the list is empty after a fresh authenticated probe. Larger
responses require explicit pages. Each actual producer query is limited to 64
rows and 4096 raw text bytes. Legal short text pages follow their continuation.
A standard reply has a byte budget below 1 MiB; dense FD scans have a 2048-query
budget. Budget exhaustion fails the whole request with a smaller-page
diagnostic, never a successful shortened list.

Every query refreshes the complete cooperative stop before and after copying.
A cached scope cannot rebind to a new stop. Pages must agree on module, runtime
epoch, shared-environment flag and total cardinality, with sorted/progressing
continuations. Resume, replacement, environment edits and failed protocol
copies retire references. A `named` filter refreshes status and returns no
indexed entries without borrowing environment state.

Values contain canonically escaped guest bytes and guest FD kind/rights/preopen
metadata. Every row is read-only with `variablesReference: 0`; no memory
reference, evaluate expression, host descriptor, host filesystem path or replay
authority is supplied. Editing remains an explicit `uwvm/wasip1Edit` operation.

The existing C++ producer uses fast_io for text storage/formatting. This change
adds Python DAP routing and detached pagination; it adds no C++ I/O API.

## Linux acceptance

Both repositories passed 310 unit tests each, with no skips, and 30 actual
Linux x86_64 broker/JIT cases in the original shared 64 GiB cgroup. Four new
WASIp1 cases cover both instruction/unwind policies in both repositories.
Each uses 160 arguments, 160 isolated environment entries and 160 guest
preopens of one empty directory owned by the test. Closing 20 actual guest
FDs leaves 143 descriptors and 140 preopens; standard dense pages, tails,
empty windows and all-remaining requests match actual custom guest queries.

The four new cases verify 48 whole-response refusals across environment edits,
external steps and mid-copy steps. A separately run old-adapter baseline
reproduces missing standard scopes against the same real C++ backend.

The 26 other cases cover seven typed Wasm scopes, ordinary locals, direct and
nested GC member pages, physical scope lifetime, native finish/timeout and
queued-before-exec broker startup. They retain 100 Wasm stale-reference
refusals and 84 positive authenticated native register watch queries.

Acceptance uses immutable 96-file adapter/test snapshots and previously
qualified R37 full-interpreter/full-LLVM C++ products. It rechecks 30,930
source/SDK paths and eight compiler dependency closures. This change performs
no new C++ build and makes no acceptance claim for concurrently edited C++
source, a graphical IDE, or other operating systems/architectures. The DAP
routing is shared code; the executed qualification is Linux x86_64.

At qualification, this task's remote root allocated 3.39 GiB,
with 94.45 GiB disk free and no cgroup OOM events.
Peak owned test RSS was 729.85 MiB. No new SDK
copy was created; all original test processes retired through their owned
Popen/PIDFD supervisors. Other agents' processes and files were preserved.

# Actual LLVM full pause lifecycle regression

Run `run_llvm_debug_lifecycle.py` only in the SSH Linux cgroup, supplying a frozen
source root, its `debug-pause-runtime-o1` build directory, wasm-tools, and a new
output directory. The runner reuses the actual runtime object and its recorded
link flags, hashes that object before/after, validates the generated Wasm module,
and records exact commands, source identity, Wasm and executable hashes.

Both ordinary and ROS frozen r218 runtimes passed both scenarios with instruction
and unwind policies: eight process executions total. Runtime and fixture builds
were O1, so this is functional evidence, not performance or sanitizer evidence.

The reset scenario parks an existing native loop, starts a second host entry and
observes its blocked OS thread before reset. Its guest marker cannot execute
while paused. Reset closes the pause domain, wakes the entry and cancels the old
execution generation. The marker must observe `stop_requested == true`, proving
that it was admitted under the old generation rather than entering only after
reset. It then deliberately holds the reset drain until released. Results 41/43
prove both native continuations finish before code/metadata retirement.

The re-entry scenario runs two rounds on the same OS thread with a capacity-one
domain. Wasm calls a host callback, which calls the public raw host API back into
Wasm. Each pause snapshot must contain exactly one participant at actual function
8, offset 8, whose byte in the parsed body is the loop opcode 0x03. The nested
function and outer caller execute real cross-function `throw`/`try_table` catches,
returning 146. After each outer host return, a new pause must capture no remaining
participant; the second round also proves the TLS participant was cleared.
Guest exceptions in this test are caught inside Wasm. An uncaught exception at
the public raw host boundary remains fatal and is not claimed to be recoverable.

Frozen runtime source IDs:

- ordinary: bcdecc8be64057b417f6a1c1bae9afc100b6ba06c0243f5777e182e5b885bd56
- ROS: e36bd60cfce132ed8dbfbef52088af1e8cb013d65f2550e4deeca697fbc277c4

Complete evidence is `build/wasm3-evidence/llvm-debug-lifecycle-r218-r1.tar.gz`,
SHA256 `4ff598b1bd0aa2b9a7adde7d6fbac964ff9c540f9949c9a895f63c326cf801cc`.
All 32 files (160,002,362 uncompressed bytes) were verified individually against
the remote SHA manifest. The archive includes both executables, test overlays,
WAT/wasm, build logs/commands and every execution log; runtime objects are also identified by their full hashes. The root pause-runtime
archive `llvm-debug-cooperative-pause-r218.tar.gz` (SHA-256
`fb33b220bc26575d3c9ba577f233dd5300b26c2a9e11c201de9c9a53aa4ca7e6`)
now retains those objects and both actual pause binaries, plus the O3/sanitizer
coordination and module-consumer evidence. All 92 files were verified against
the remote manifest before pruning 12 large remote files. The lifecycle archive
likewise retains both lifecycle executables after their verified remote pruning.

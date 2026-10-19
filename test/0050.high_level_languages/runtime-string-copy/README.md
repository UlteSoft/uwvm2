# String and control-flow runtime regressions

These import-free Wasm fixtures isolate operations used by real Javy and
MoonBit applications. They assert exact values; failure traps, while success
exits normally with no output. A small-fixture PASS does not replace the
complete unchanged producer artifacts.

| Fixture | Required behavior |
| --- | --- |
| `bulk_memory_strings.wat` | Byte copy, overlap, fill, and zero-length operations at the end of linear memory |
| `packed_array_strings.wat` | Immutable UTF-16 array construction, copying into mutable storage, initialization, fill, and end-of-array empty copy |
| `nested_array_string_element.wat` | Preserve a destination reference/index under a nested packed-source element read |
| `mixed_array_string_loop.wat` | Mixed reference/integer function parameters and two loop parameters while copying UTF-16 into i32 characters |
| `live_memory_base_if_loop.wat` | Preserve an older memory base across an if-result and its internal loop |
| `if_join_paths.wat` | Both arms of implicit/explicit end branches, plus an if without else; eight named assertions run in order |
| `control_end_edges.wat` | End-layout merges across conditional/table/reference branches, no-else entries, i64 and f64 values; 24 named assertions |

When either named-assertion fixture traps, later assertions have not executed.
Use its actual call stack to identify the failing named wrapper; do not count
the whole suite as passing from an earlier prefix.

Compile and run only in the existing SSH Linux 64 GiB cgroup, using a frozen
source/tool/product cohort and the task's owned-process/resource runner:

```sh
wasm-tools parse live_memory_base_if_loop.wat -o live_memory_base_if_loop.wasm
wasm-tools validate live_memory_base_if_loop.wasm
uwvm -Rint --run live_memory_base_if_loop.wasm

wasm-tools parse packed_array_strings.wat -o packed_array_strings.wasm
wasm-tools validate packed_array_strings.wasm
uwvm -Rint -WFE-gc -WFE-reference-types -WFE-function-references \
  --run packed_array_strings.wasm
```

Repeat for each fixture and each actual repository product, retaining tool,
source, executable and Wasm SHA256, arguments, complete outputs and exit status.
Run an independent implementation as an oracle for a newly reduced case.
Optimization-disabled runs remain diagnostic; they do not replace default
configuration acceptance or matched hardware-counter performance measurements.

# Independent concrete source scope path, source only

`source_scope_path.h/.cppm` selects the physical subprogram and actual nested
inline DIE instances outer-to-inner. Output contains owned DIE keys and owned
indices into the same immutable metadata input. It is LLVM-free and performs no
file loading, expression execution, memory read, source-owner authentication or
pause/resume operation. A Code-relative PC from a host caller is not a credential.
The future controller must first authenticate the actual source binding, fresh
captured ticket, location, code/runtime epoch and function generation.

Default budgets cover 65,536 scopes, 65,536 total ranges, 64 ancestor edges and
64 TOTAL output keys including the physical key. Range summation checks overflow
before addition. Every input parent must precede its child; invalid scope enums
and malformed ranges anywhere in the span reject the query. Exactly one physical
scope and one active inline child per level may match. Selected concrete keys
must be distinct; names and abstract origins cannot substitute for identity.
Explicitly empty/inactive lexical ranges exclude descendants, while absent range
attributes inherit. All errors clear both output arrays, including allocation
failure. No borrowed index/frame/native pointer escapes.

The predecessor frozen Stage2 snapshot remains unchanged on disk. The new
source candidate routes `query_inline_frames` through this selector; the current
product header and module aggregates import/export scope path and step policy.
Inline display preserves innermost-first owned strings and its original total
string budget. The output-key budget adds one physical key with an explicit
overflow check. Duplicate selected DIE identities and invalid scope enums now
reject display too; synthetic display fixtures supply distinct identities.
The numeric values query already calls `query_inline_frames`, so it receives the
same path validation before its separate variable/type/location/lexical checks.
Its value-capture permission, decoder and unavailable reasons are unchanged.

This is a derived source candidate, not a claim of completed product stepping.
Actual source next/finish still requires genuine debug-full activation events,
authenticated fresh captured pauses and same-owner generation checks. No such
credentials can be created by metadata indices, source strings or a stop label.
The independent scope-path unit and the updated inline display unit must be run
against this new closure on SSH Linux. Predecessor test results do not qualify it.

## Keeper unit qualification, remote controlled cgroup

Only source/static mirror checks have been done. Compilation/execution belongs
to the keeper's actual 64 GiB/no-swap/CPU cgroup, with fresh source and tool hashes:

```sh
bash "$ROOT/tools/ci/require_wasm3_test_cgroup.sh"
"$CXX" -std=c++23 -O1 -g0 -Wall -Wextra -Werror -DUWVM=2 \
  -I"$ROOT/src" -I"$ROOT/third-parties/fast_io/include" \
  "$ROOT/test/0017.runtime/debug_source_scope_path.cc" \
  -o "$EVIDENCE/debug_source_scope_path"
"$EVIDENCE/debug_source_scope_path"
```

These are planned commands, not an execution receipt. The pure synthetic unit
covers distinct concrete identities despite duplicate names, output ordering and
ownership, half-open physical/inline/lexical ranges, explicit empty lexical scope,
abstract non-concrete records, overlapping siblings/physical records, duplicate
selected keys, unit+offset identity, malformed parents/enums/ranges, all budgets
and arithmetic overflow. It does not prove actual source tickets or activation
IDs; product source stepping still requires the real debugger-only producer.

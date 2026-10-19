# Standalone language-debug adapter probe

This project compiles host debugger headers and runs a small native probe. It
checks language selection, scalar-parser availability, independent Go/TinyGo
selection, and refusal of incomplete container metadata without a guest-memory
read. It does not build UWVM or execute Wasm, and does not establish managed
source debugging, ROS SDK parity, performance, or a full platform matrix.

All fifteen language adapters remain enabled by default in every repository.
Keeping them enabled is recommended, including adapters for the native runtime
and libraries used by a managed application. Disabling a debugger adapter does
not ban legal Wasm from that language or grant WASI/ROS authority. See the
[language-option guide](../../../docs/debugger/language-options.md).

## Build and run

From the repository root, enter this standalone project. Select the compiler
and C++ standard library appropriate to your checkout; the recorded Linux
cohort uses its matching Clang 23/libc++ toolchain. Root execution additionally
requires the usual `XMAKE_ROOT=y` setting.

```sh
cd test/0050.high_level_languages/build-options
xmake f -c -m debug --debug-languages=all --matrix-expected-mask=32767
xmake build -r -j1 language_debug_probe
xmake run language_debug_probe
```

Expected stdout is exactly `LANGUAGE_BUILD_SELECTION_OK 32767` followed by a
newline. The probe exits 0 on success. A fresh configuration without language
overrides also selects all fifteen. When supplying an existing compiler
explicitly, preserve its matching compiler, linker and runtime flags in every
configuration; for example, the Linux cohort passes both
`--cxflags=-stdlib=libc++` and `--ldflags=-stdlib=libc++`.

Other examples:

```sh
# Only the Rust-specific adaptation; generic facilities remain separate.
xmake f -c -m release --debug-languages=rust --matrix-expected-mask=4
xmake build -r -j1 language_debug_probe
xmake run language_debug_probe

# No language-specific adaptations: shared numeric parsing still works.
xmake f -c -m release --debug-languages=none --matrix-expected-mask=0
xmake build -r -j1 language_debug_probe
xmake run language_debug_probe
```

The second and third examples print masks 4 and 0 respectively. Explicit
language lists override the individual Booleans. In individual mode, disabling
only Python keeps the other fourteen enabled and produces mask 31743:

```sh
xmake f -c -m release --debug-languages=individual \
  --debug-language-python=n --matrix-expected-mask=31743
```

## The expected-mask option is a test assertion

`--matrix-expected-mask=none` is this project's default. It omits the optional
numeric compile-time assertion; it does not select any language or disable
adapters. A decimal value from 0 through 32767 adds a static assertion against
the actual emitted language mask. A wrong expectation fails compilation.

The literal sentinel is deliberately `none`. Xmake reserves `auto`, normalizes
it to `nil`, and can reject an explicitly supplied readonly `auto`
configuration before an option's callback runs. `auto` is not an accepted
sentinel for this test option. This does not change the production
`debug-languages` interface or its default `individual` selection with all
fifteen Booleans enabled.

Unknown or duplicate language entries, empty lists and empty comma-separated
fields must fail for the language-list reason. Negative, non-decimal or
greater-than-32767 expected masks must fail for the expected-mask reason.
Checking only a nonzero exit could incorrectly accept an unrelated failure.

## What the incomplete-metadata check means

The synthetic C++ vector and Rust Vec records intentionally lack a complete
layout. An enabled adapter can decline the schema (`not_matched`) or recognize
it as unavailable. A disabled adapter must decline it. Neither case may format
a value successfully or invoke the supplied memory reader. Independent scalar
parser checks establish that enabled language paths are present; a container's
name alone is not a layout or read capability.

This avoids treating a stricter Rust schema refusal as a broken language
switch. It does not certify real Rust containers, C++ library ABIs, arbitrary
object formatting, or sandbox security. Those need actual producer metadata,
genuine stopped frames and separately recorded runtime tests.

## Matrix interpretation

The selected matrix has 35 configurations per repository and mode: defaults,
all, none, individual-all-off, list precedence, each language alone and each
language disabled while fourteen remain enabled. Debug/Release across three
repositories gives 210 combinations, rather than every possible subset.
Independent expected masks, actual compiler defines, complete probe output,
exit status and source hashes must agree. Reused closed results must identify
their exact source/compiler/ELF lineage and remain distinct from fresh builds.

The current configuration preflight checks 78 actual configure calls: 42
default/expected-mask acceptance and refusal cases plus 36 malformed language
lists. These calls do not count as target builds or native probe executions.
The earlier source-bound complete matrix and execution/size evidence remain
available in the [default-on packet](../results/native-budget/language-defaults-on-size-20261019/README.md).
Later dirty worktrees do not inherit that packet's qualification automatically.

All task qualification executes on SSH Linux in the original existing 64 GiB
cgroup with swap disabled. Compilation/probe times in this matrix are
correctness diagnostics and do not enter native/VTune performance tables.

The [current preflight evidence](../results/native-budget/language-debug-option-preflight-20261019/README.md)
contains all 78 outcomes, original helper failures, exact source/command hashes
and separate full-archive verification. It deliberately adds no target-build
or Guest/DBG result.

The [current 210-combination evidence](../results/native-budget/language-current-option-matrix-20261019/README.md)
now verifies every selected C++ source/define row and all 99 direct macro checks.
Its independently checked lineage is 34 r274, 68 r281, 59 r294 and 49 fresh r308
combinations. This remains a host-header probe matrix; complete VM and producer
DBG/performance workflows keep their separate qualification scopes.

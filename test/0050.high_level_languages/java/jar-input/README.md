# Existing JAR inputs to WASI

`produce_jar.py` accepts an existing application JAR and explicit dependency
JARs. It consumes JVM class-file IR through TeaVM 0.13.1's real Maven compile
goal and emits linear-memory WASI. A main class comes from the main manifest
section or `--main-class`. Optional original source roots and a source map
provide files for producer debug information.

Run only on SSH Linux inside the existing 64 GiB cgroup. Pass the suite root
using `--helpers`, the existing private DWARF/ordered-name/typed-local/environ
compiler repository using `--m2`, and its actual pinned receipt and patch root.
The script's `--help` describes the required tools and inputs. Each run needs a
fresh output directory; it records exact inputs, compiler commands, failures,
Wasm imports and owned-process retirement without changing the original JARs
or compiler repository.

`tests/future_linux.py` contains the real build/run orchestration for a manifest
JAR, an explicit main plus a separate dependency, Unicode/empty/equal-sign
arguments and environment, and missing-main/native-method/missing-dependency
failures. The positive Wasm is checked in Wasmtime and all three UWVM products.
Source-level DBG and VTune Hotspots/microarchitecture qualification are separate
required stages. The later [Kotlin workflow](../../kotlin/README.md) actually
exercises this converter with a real application JAR, explicit stdlib/compiler
plugin dependencies and both SIMPLE/FULL outputs. Its native JVM and Wasmtime
results do not qualify every separate future test scenario described above.

The converter binds the actual executable Java VM, its module image and runtime
libraries. It does not require unused bin/javac or bin/jar launcher files;
the provided modular JDK exposes those tools through java -m when a separate
source/compiler-plugin build needs them. The optional explicitly supplied
[WASI process-exit plugin](../wasi-process-exit/README.md) maps System.exit(int)
only for the legacy WASI target. The application JAR remains unchanged.

The default import ceiling is WASIp1. `--ros-backend` permits a named ROS import
namespace in the inventory and grants no Host policy. Actual ROS Guest calls
require the real SDK/provider and configured default-deny permissions. This
ordinary WASI producer does not enable the separate WASI Threads profile.

The optional service-client extension needs three explicit arguments:
`--ros-backend 2 --ros-client-abi /absolute/sdk/abi/service-client-v1.json
--ros-client-abi-sha256 <actual-64-character-lowercase-SHA256>` (select backend
`1` for ROS1). The catalog is an independently pinned input; it does not replace
the original business ABI. The producer admits only that backend's
`uwvm_rosN_client_v1` module and checks each imported function against the exact
`abi_version`, `features` and `service_available` Core signature. Unknown names,
duplicate optional imports, wrong signatures and unrepresentable type sections
are refused. This opt-in inventory currently accepts simple Core function type
sections; a GC/recursive type group is an explicit unsupported input, not an
inferred signature. The default WASI and business-only import ceilings remain
unchanged.

This check is a structural import inventory. The final artifact still needs a
real full Wasm validator, Runtime admission, native ROS delivery and DBG tests.
The Host separately authorizes the actual `service.available`/`service.call`
pair, source, Instance and lifetime generation; selecting this producer option
grants no permission. `tests/future_client_abi_linux.py` contains focused future
Linux parser-contract checks. They have not been run and do not qualify a real
JAR, ROS transport or application.

The implementation follows the official
[TeaVM 0.13.1 Maven plugin source](https://github.com/konsoletyper/teavm/blob/0.13.1/tools/maven/plugin/src/main/java/org/teavm/maven/TeaVMCompileMojo.java),
whose classpath explicitly admits SYSTEM dependencies.

# Explicit TeaVM WASI process-exit adapter

TeaVM 0.13.1 does not provide `java.lang.System.exit(int)` in the selected class
library. Kotlin's `exitProcess` exposes that gap. This compiler plugin maps only
an exact static `void(int)` call on `java.lang.System` (or its classlib alias) to
`wasi_snapshot_preview1.proc_exit(i32)`. It preserves the argument and changes
neither the original input JAR nor native JVM behavior.

The compiler must expose `TeaVMWasmHost` with `WasmRuntimeType.WASI`. Otherwise
the plugin installs no transform. Wrong names, descriptors, argument counts,
instances and receiver layouts are unchanged. Runtime.exit and System.gc are
not redirected. The implementation supplies an annotated Guest import, not a
JNI/native Host process operation. It adds no arbitrary import, mount, clock,
randomness or ROS permission.

The JAR's service declaration explicitly opts this trusted build tool into the
compiler classpath. It is not a Host UWVM feature or a language-debug option.
Keep its source, compiled JAR, upstream compiler identities and import inventory
in the build record. The [complete Kotlin tutorial](../../kotlin/README.md)
shows the actual module-based javac/jar commands and converter invocation.

Two real Kotlin SIMPLE/FULL workflows preserve native exit status 2 and exact
stderr for invalid inputs, with complete Wasm validation and Wasmtime execution.
The public stdlib preparation workflow repeats those actual results. Current
UWVM ordinary/DBG consumption is qualified separately; these producer tests
do not establish complete SDK/process lifecycle or performance parity.

`PluginContract.java` provides sixteen actual finite compiler-model controls:
non-Wasm and non-WASI admission, the WASI registration count, two exact mappings,
and near-miss refusal. They run compiled on the existing SSH Linux 64 GiB cgroup
with swap disabled. They never invoke the native import and are model tests,
separate from the real emitted-Wasm programs. The trusted compiler toolchain
remains outside the Wasm sandbox; the resulting module still uses standard
Host WASI admission and instance-exit handling.

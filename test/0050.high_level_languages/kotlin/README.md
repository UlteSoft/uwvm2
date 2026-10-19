# Kotlin JVM IR to standalone WASI

This route uses the real Kotlin 2.1.20 JVM compiler, then translates its class
IR with the pinned TeaVM 0.13.1 linear-memory WASI backend. The example checks
inheritance/virtual dispatch, data and sealed classes, collections, Unicode,
exceptions, arguments and a wrapping checksum. It is separate from
Kotlin/Native and the direct Kotlin/WasmGC/WASI backend; those routes are not
qualified by these results.

Actual SIMPLE/FULL translations pass full Wasm validation and ten Wasmtime
cases. The same original Kotlin application JAR passes five native JVM cases.
The repeated public stdlib-preparation workflow also passes all fifteen
outcomes. These are correctness results, not native performance measurements.

## Required tools

Select Kotlin 2.1.20, JDK21 and a private TeaVM 0.13.1 repository with the four
isolated DWARF, ordered-name, typed-local and WASI-environ patches. The
[Java producer documentation](../java/README.md) describes that toolchain;
`java/build_dwarf_fixed.py` accepts `--wasi-environ --ordered-names --typed-locals`
when constructing it from the original Maven repository. Keep its receipt and
the exact patches. Building that toolchain itself needs its documented complete
JDK/build prerequisites; consuming the JAR route only needs the actual Java VM.

The existing Linux JDK provides compiler/archiver modules without `bin/javac`
or `bin/jar`. The tested commands below invoke those modules through Java.
The JAR converter binds `bin/java`, `lib/modules`, `lib/server/libjvm.so` and
present runtime libraries/configuration instead of requiring unused launchers.
The adapter is compiled with Java17 source/class targets against the selected
JDK21 library; this is not a `--release17` API-compatibility certification.

Set writable, fresh absolute output paths and your installed tool paths:

```sh
export KOTLIN_HOME=/absolute/path/to/kotlinc-2.1.20
export JAVA_HOME=/absolute/path/to/jdk21
export TEAVM_PATCHED=/absolute/path/to/four-patch-toolchain
export MVN=/absolute/path/to/apache-maven-3.9.11/bin/mvn
export OUTPUT=/absolute/fresh/output/kotlin
export SUITE=/absolute/path/to/test/0050.high_level_languages
mkdir -p "$OUTPUT/plugin-classes/META-INF/services"
export JAVA_OPTS='-Xms64m -Xmx512m -XX:ActiveProcessorCount=1'
```

## Build the explicit WASI process-exit adapter

TeaVM 0.13.1's class library lacks `System.exit(int)`, which Kotlin's
`exitProcess` uses. The checked-in compiler plugin maps exactly that static
`void(int)` call to the real WASI `proc_exit(i32)` import. The input application
JAR and its native JVM semantics are unchanged. The plugin installs only when
the compiler's legacy Wasm runtime type is WASI; other targets are unchanged.
It supplies no JNI bridge, arbitrary Host import or permission grant.

```sh
TEAVM_CORE="$TEAVM_PATCHED/m2/org/teavm/teavm-core/0.13.1/teavm-core-0.13.1.jar"
TEAVM_INTEROP="$TEAVM_PATCHED/m2/org/teavm/teavm-interop/0.13.1/teavm-interop-0.13.1.jar"
"$JAVA_HOME/bin/java" -m jdk.compiler/com.sun.tools.javac.Main \
  -source 17 -target 17 -g -cp "$TEAVM_CORE:$TEAVM_INTEROP" \
  -d "$OUTPUT/plugin-classes" \
  "$SUITE/java/wasi-process-exit/WasiSystemExit.java" \
  "$SUITE/java/wasi-process-exit/WasiProcessExitPlugin.java"
cp "$SUITE/java/wasi-process-exit/META-INF/services/org.teavm.vm.spi.TeaVMPlugin" \
  "$OUTPUT/plugin-classes/META-INF/services/"
"$JAVA_HOME/bin/java" -m jdk.jartool/sun.tools.jar.Main --create \
  --file "$OUTPUT/wasi-process-exit-plugin.jar" -C "$OUTPUT/plugin-classes" .
```

Compiler plugins execute as trusted build tools. Admit and pin this source/JAR
as part of the build toolchain. That build-time trust is distinct from executing
the resulting Wasm sandbox. Sixteen actual compiler-model controls cover target
selection and exact-reference matching/near misses; real Kotlin Wasm runs
separately prove the emitted exit import returns the required status.

## Build the application and translate its class IR

```sh
bash "$KOTLIN_HOME/bin/kotlinc" HighLevelKotlin.kt -jvm-target 17 \
  -no-reflect -d "$OUTPUT/application.jar"
"$JAVA_HOME/bin/java" -cp "$OUTPUT/application.jar:$KOTLIN_HOME/lib/kotlin-stdlib.jar" \
  uwvm.hll.kotlin.HighLevelKotlin 17
python3 prepare_stdlib_base.py --input "$KOTLIN_HOME/lib/kotlin-stdlib.jar" \
  --out "$OUTPUT/stdlib-base-view.jar" --receipt "$OUTPUT/stdlib-view.json"
sha256sum "$TEAVM_PATCHED/receipt.json"
```

The strict JAR converter refuses Multi-Release JARs. The preparation CLI removes
only versioned module descriptors and the Multi-Release declaration; every base
class stays byte exact. Native JVM checks use the original stdlib. Alternative
versioned implementations/resources, signed/encrypted JARs, symlinks and unsafe
paths are rejected. Five actual negative CLI controls verify rejection before
output creation; this is not a general JAR rewriting/retargeting facility.

Set `TEAVM_RECEIPT_SHA` to the actual 64-character digest printed above. Run the
following under the admitted Linux CPU/cgroup; use a new output for every
translation and repeat with `--optimization FULL`:

```sh
taskset -c 20 python3 -I -B "$SUITE/java/jar-input/produce_jar.py" \
  --jar "$OUTPUT/application.jar" --dependency-jar "$OUTPUT/stdlib-base-view.jar" \
  --dependency-jar "$OUTPUT/wasi-process-exit-plugin.jar" \
  --main-class uwvm.hll.kotlin.HighLevelKotlin \
  --m2 "$TEAVM_PATCHED/m2" --toolchain-receipt "$TEAVM_PATCHED/receipt.json" \
  --toolchain-receipt-sha256 "$TEAVM_RECEIPT_SHA" --patch-root "$TEAVM_PATCHED" \
  --maven "$MVN" --java-home "$JAVA_HOME" --helpers "$SUITE" \
  --cpu 20 --optimization SIMPLE --max-heap-mib 128 --out "$OUTPUT/simple"
uwvm --run "$OUTPUT/simple/project/target/wasi/application.wasm" 17
```

Select the UWVM backend appropriate to your build before `--run`. The manual
LLVM ordinary product uses `-Rcm full -Rcc jit`, while stable/ROS use `-Raot`.
No arguments uses 1000 iterations. Invalid text, zero, values above 1000000 or
extra arguments print `kotlin invalid iterations` and exit 2 in both native JVM
and Wasm. No Java/Kotlin-style args/environment compatibility switch is needed.
The fixture requires no directories, clocks, randomness or ROS imports.

## Debug and capability scope

Class line/local attributes, TeaVM DWARF and source metadata are retained.
They do not themselves qualify Kotlin source breakpoints, inline/SMAP frames,
coroutines, object formatting or typed locals. Those need actual consumer DBG
checks in addition to the complete Wasm control workflow. Existing JARs that
use reflection, JNI/native methods or unsupported class-library APIs need their
own compatibility checks; no missing Host method is fabricated.

All fifteen Host language debugger options default on. Keeping all of them
enabled is recommended. `--debug-languages=individual --debug-language-kotlin=n`
trims Kotlin-specific Host adaptation wherever implemented after rebuilding;
legal Kotlin-produced Wasm continues to run. This does not remove the compiler
plugin, change WASI imports or grant/deny ROS authority. See the
[option guide](../../../docs/debugger/language-options.md).

All task compilation, program execution, debugging and profiling runs on SSH
Linux in the original 64 GiB cgroup with swap disabled. This basic non-ROS
example makes no complete standard-library, ROS SDK/API or native timing parity
claim. The compiler follows the official
[Kotlin command-line route](https://kotlinlang.org/docs/command-line.html).

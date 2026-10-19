#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
# Execute only inside the user-authorized SSH Linux 64 GiB cgroup.
: "${MVN:=mvn}"
export MAVEN_OPTS="${MAVEN_OPTS:--Xmx2g -XX:ActiveProcessorCount=2}"
"$MVN" --batch-mode --no-transfer-progress process-classes "$@"
"$MVN" --batch-mode --no-transfer-progress process-classes \
  -Dteavm.target=WEBASSEMBLY_WASI -Dteavm.output=wasi-debug \
  -Dteavm.optimization=SIMPLE "$@"
"$MVN" --batch-mode --no-transfer-progress process-classes \
  -Dteavm.target=WEBASSEMBLY_GC -Dteavm.output=wasm-gc "$@"
"$MVN" --batch-mode --no-transfer-progress process-classes \
  -Dteavm.target=WEBASSEMBLY_WASI -Dteavm.output=perf \
  -Dfixture.main=uwvm.hll.Perf -Dfixture.file=java-perf.wasm "$@"
"$MVN" --batch-mode --no-transfer-progress process-classes \
  -Dteavm.target=WEBASSEMBLY_WASI -Dteavm.output=known-gap \
  -Dfixture.main=uwvm.hll.ImplicitArithmetic -Dfixture.file=java-implicit-arithmetic.wasm "$@"

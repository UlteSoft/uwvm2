#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -lt 1 || "$#" -gt 2 ]]; then
  echo "usage: $0 /absolute/path/to/uwvm [int|aot|int-lazy|int-full|llvm-lazy|llvm-full|tiered]" >&2
  exit 2
fi

UWVM_BIN="$1"
BACKEND="${2:-int}"
if [[ ! -x "${UWVM_BIN}" ]]; then
  echo "not executable: ${UWVM_BIN}" >&2
  exit 2
fi
if ! command -v wat2wasm >/dev/null 2>&1; then
  echo "wat2wasm is required" >&2
  exit 2
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
FIXTURE_DIR="${SCRIPT_DIR}/fixtures"
WORK_DIR="$(mktemp -d /tmp/uwvm2-cross-module-funcref.XXXXXX)"
cleanup() { rm -rf -- "${WORK_DIR}"; }
trap cleanup EXIT

for source in "${FIXTURE_DIR}"/*.wat; do
  name="$(basename -- "${source}" .wat)"
  wat2wasm --enable-all "${source}" -o "${WORK_DIR}/${name}.wasm"
done

case "${BACKEND}" in
  int) BACKEND_OPTIONS=(--runtime-int) ;;
  aot|llvm-full) BACKEND_OPTIONS=(--runtime-aot) ;;
  int-lazy) BACKEND_OPTIONS=(-Rcm lazy -Rcc int) ;;
  int-full) BACKEND_OPTIONS=(-Rcm full -Rcc int) ;;
  llvm-lazy) BACKEND_OPTIONS=(--runtime-jit) ;;
  tiered) BACKEND_OPTIONS=(--runtime-tiered) ;;
  *)
    echo "unsupported backend: ${BACKEND}" >&2
    exit 2
    ;;
esac
COMMON=(--wasm-feature-wasm2 "${BACKEND_OPTIONS[@]}" -Rct 0 -Rllvm-cache-path disable)

run_global_consumer() {
  "${UWVM_BIN}" "${COMMON[@]}" \
    --wasm-set-main-module-name B \
    --wasm-preload-library "${WORK_DIR}/funcref_global_provider.wasm" A \
    --run "${WORK_DIR}/funcref_global_consumer.wasm"
}

run_element_consumer() {
  "${UWVM_BIN}" "${COMMON[@]}" \
    --wasm-set-main-module-name B \
    --wasm-preload-library "${WORK_DIR}/funcref_global_provider.wasm" A \
    --run "${WORK_DIR}/funcref_element_consumer.wasm"
}

run_forwarded_global_consumer() {
  "${UWVM_BIN}" "${COMMON[@]}" \
    --wasm-set-main-module-name B \
    --wasm-preload-library "${WORK_DIR}/funcref_import_leaf.wasm" P \
    --wasm-preload-library "${WORK_DIR}/funcref_import_provider.wasm" A \
    --run "${WORK_DIR}/funcref_global_consumer.wasm"
}

# Full uwvm2 lowers reference values and table.init. Only the separately maintained ROS backend intentionally rejects
# these operations; accepting its capability-rejection diagnostic here would conceal a Full code-generation regression.
run_global_consumer
run_element_consumer
run_forwarded_global_consumer

"${UWVM_BIN}" "${COMMON[@]}" --run "${WORK_DIR}/global_reference_storage.wasm"
"${UWVM_BIN}" "${COMMON[@]}" --run "${WORK_DIR}/global_v128_storage.wasm"

"${UWVM_BIN}" "${COMMON[@]}" \
  --wasm-set-main-module-name C \
  --wasm-preload-library "${WORK_DIR}/imported_table_owner.wasm" A \
  --wasm-preload-library "${WORK_DIR}/imported_table_writer.wasm" B \
  --run "${WORK_DIR}/imported_table_caller.wasm"

echo "OK: cross-module funcref identity and non-scalar global storage (${BACKEND})"

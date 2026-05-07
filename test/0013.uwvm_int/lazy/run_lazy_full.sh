#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

if [[ "$#" -gt 0 ]]; then
  exec "${SCRIPT_DIR}/run_lazy_basic.sh" "$@"
fi

export UWVM_LAZY_RUN_LABEL="lazy full"
exec "${SCRIPT_DIR}/run_lazy_basic.sh" \
  uwvm_int_lazy_split \
  uwvm_int_lazy_scheduler \
  uwvm_int_lazy_runtime \
  uwvm_int_lazy_full_mvp

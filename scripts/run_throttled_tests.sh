#!/usr/bin/env bash
set -euo pipefail

suite="${1:-default}"
jobs="${UWVM_TEST_JOBS:-1}"
log_root="${UWVM_TEST_LOG_ROOT:-/tmp/uwvm2-v2.0.2-logs/throttled}"
mkdir -p "$log_root/manifests" "$log_root/$suite"

export TMPDIR="${TMPDIR:-/home/macromodel/.cache/uwvm2-tmp}"
export UWVM_TRAP_MATRIX_STRICT="${UWVM_TRAP_MATRIX_STRICT:-1}"

sanitize_name() {
	local name="$1"
	name="${name//\//_}"
	name="${name//./_}"
	name="${name//-/_}"
	printf '%s\n' "$name"
}

regular_default_targets() {
	find test -name '*.cc' | sort | while IFS= read -r file; do
		case "$file" in
			test/0015.backend_fuzzer/*) continue ;;
			test/0009.libfuzzer/*) continue ;;
		esac
		basename "$file" .cc
	done
}

llvm_jit_strict_reuse_targets() {
	find test/0013.uwvm_int/strict -name '*.cc' | sort | while IFS= read -r file; do
		rel="${file#test/0013.uwvm_int/strict/}"
		rel="${rel%.cc}"
		printf 'llvm_jit_reuse_0013_%s\n' "$(sanitize_name "$rel")"
	done
}

llvm_jit_lazy_reuse_targets() {
	find test/0013.uwvm_int/lazy -name '*.cc' | sort | while IFS= read -r file; do
		case "$file" in
			*/uwvm_int_lazy_split.cc|*/uwvm_int_lazy_strategy_matrix.cc) continue ;;
		esac
		rel="${file#test/0013.uwvm_int/lazy/}"
		rel="${rel%.cc}"
		printf 'llvm_jit_lazy_reuse_0013_%s\n' "$(sanitize_name "$rel")"
	done
}

libfuzzer_targets() {
	find test/0009.libfuzzer -name '*.cc' | sort | while IFS= read -r file; do
		basename "$file" .cc
	done
}

manifest="$log_root/manifests/${suite}.targets"
case "$suite" in
	default)
		{
			regular_default_targets
			llvm_jit_strict_reuse_targets
			llvm_jit_lazy_reuse_targets
		} | awk 'NF && !seen[$0]++' > "$manifest"
		;;
	libfuzzer)
		libfuzzer_targets | awk 'NF && !seen[$0]++' > "$manifest"
		;;
	backend_fuzzer)
		printf 'backend_fuzzer\n' > "$manifest"
		;;
	*)
		printf 'unknown suite: %s\n' "$suite" >&2
		exit 2
		;;
esac

total="$(wc -l < "$manifest" | tr -d ' ')"
printf '[%s] targets=%s manifest=%s\n' "$suite" "$total" "$manifest"

if [[ "${UWVM_TEST_DRY_RUN:-0}" == "1" ]]; then
	exit 0
fi

start="${UWVM_TEST_START:-1}"
end="${UWVM_TEST_END:-$total}"
resume="${UWVM_TEST_RESUME:-0}"

idx=0
while IFS= read -r target; do
	idx=$((idx + 1))
	if (( idx < start || idx > end )); then
		continue
	fi
	log="$log_root/$suite/${idx}__${target}.log"
	printf '[%s] %s/%s %s\n' "$suite" "$idx" "$total" "$target"
	if [[ "$resume" == "1" ]] && grep -q '100%.*tests passed' "$log" 2>/dev/null; then
		printf '[%s] SKIP passed %s\n' "$suite" "$target"
		continue
	fi
	if ! xmake test -y -j "$jobs" "${target}/*" > "$log" 2>&1; then
		printf '[%s] FAILED %s log=%s\n' "$suite" "$target" "$log" >&2
		tail -120 "$log" >&2 || true
		exit 1
	fi
	if grep -q 'nothing to test' "$log"; then
		printf '[%s] NO TEST MATCH %s log=%s\n' "$suite" "$target" "$log" >&2
		exit 1
	fi
	xmake clean -y "$target" >/dev/null 2>&1 || true
	df -h /tmp | tail -1
done < "$manifest"

printf '[%s] OK targets=%s\n' "$suite" "$total"

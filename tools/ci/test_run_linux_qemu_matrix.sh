#!/usr/bin/env bash

# Lightweight policy/capability regression checks for run_linux_qemu_matrix.sh.
# All target and wat2wasm executions are mocks; this test never compiles or runs
# QEMU and is therefore safe to use while a real -j1 build is in progress.

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
RUNNER="${SCRIPT_DIR}/run_linux_qemu_matrix.sh"
TEST_TMP=$(mktemp -d "${TMPDIR:-/tmp}/uwvm-qemu-matrix-test.XXXXXX")
trap 'rm -rf -- "$TEST_TMP"' EXIT

fail()
{
    printf 'test_run_linux_qemu_matrix: FAIL: %s\n' "$*" >&2
    exit 1
}

assert_tsv_value()
{
    local file=$1
    local key=$2
    local expected=$3
    local actual
    actual=$(awk -F '\t' -v key="$key" '$1 == key { print $2; exit }' "$file")
    [[ $actual == "$expected" ]] || fail "${file}: ${key}: expected ${expected}, got ${actual:-<missing>}"
}

assert_tsv_columns()
{
    local file=$1
    ! awk -F '\t' 'NF != 20 { bad = 1 } END { exit !bad }' "$file" ||
        fail "unexpected TSV column count in ${file}"
}

assert_no_fail_rows()
{
    local file=$1
    assert_tsv_columns "$file"
    ! awk -F '\t' 'NR > 1 && $15 == "FAIL" { found = 1 } END { exit !found }' "$file" ||
        fail "unexpected FAIL row in ${file}"
}

assert_unique_metadata_keys()
{
    local file=$1
    ! awk -F '\t' 'seen[$1]++ { duplicate = 1 } END { exit !duplicate }' "$file" ||
        fail "duplicate metadata key in ${file}"
}

assert_mode_pass_count()
{
    local file=$1
    local mode=$2
    local expected=$3
    awk -F '\t' -v mode="$mode" -v expected="$expected" \
        'NR > 1 && $8 == mode && $15 == "PASS" { count++ } END { exit count != expected }' "$file" ||
        fail "${file}: ${mode}: expected ${expected} PASS rows"
}

test_sha256_file()
{
    local input=$1
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum -- "$input" | awk '{ print $1; exit }'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 -- "$input" | awk '{ print $1; exit }'
    else
        openssl dgst -sha256 <"$input" | awk '{ print $NF; exit }'
    fi
}

assert_profile_rejected()
{
    local mock_profile=$1
    local case_name=$2
    local output="${TEST_TMP}/out-${case_name}"
    local rc=0
    set +e
    PATH="${TEST_TMP}:$PATH" MOCK_PROFILE=$mock_profile "$RUNNER" \
        --uwvm "$MOCK_UWVM" --wat2wasm "$MOCK_WAT2WASM" --output "$output" \
        --label "$case_name" --profile ros --modes auto --policies logical \
        --max-rss-mib 0 --memory-budget-mib 0 >"${TEST_TMP}/${case_name}.log" 2>&1
    rc=$?
    set -e
    [[ $rc == 2 ]] || fail "${case_name}: expected profile rejection exit 2, got ${rc}"
    grep -q -- '--profile ros does not match the advertised reduced ROS runtime surface' \
        "${TEST_TMP}/${case_name}.log" || fail "${case_name}: missing precise ROS profile rejection"
}

MOCK_WAT2WASM="${TEST_TMP}/wat2wasm"
MOCK_UWVM="${TEST_TMP}/uwvm"
MOCK_REALPATH="${TEST_TMP}/realpath"

cat >"$MOCK_REALPATH" <<'MOCK'
#!/usr/bin/env bash
set -euo pipefail
if [[ ${1:-} == -m ]]; then shift; fi
/bin/realpath "$@"
MOCK

cat >"$MOCK_WAT2WASM" <<'MOCK'
#!/usr/bin/env bash
set -euo pipefail
if [[ ${1:-} == --version ]]; then
    printf 'mock wat2wasm 1.0\n'
    exit 0
fi
output=
while (($# != 0)); do
    if [[ $1 == -o ]]; then
        output=$2
        shift 2
    else
        shift
    fi
done
[[ -n $output ]]
: >"$output"
MOCK

cat >"$MOCK_UWVM" <<'MOCK'
#!/usr/bin/env bash
set -euo pipefail

profile=${MOCK_PROFILE:?}
if [[ ${1:-} == --version ]]; then
    printf 'Version: mock-1\n'
    printf 'Architecture: x86_64\n'
    printf 'OS: Linux\n'
    printf 'C Library: glibc\n'
    printf 'WASM Memory Model: Memory Map\n'
    case $profile in
        full-product|full-product-no-wasm2) printf 'Runtime Compiler: UWVM2-Interpreter, LLVM-JIT\n' ;;
        ros-product|ros-unsupported-wasm2|ros-*-remnant)
            printf 'Runtime Compiler: UWVM2-Interpreter, LLVM-AOT\n'
            ;;
        full-live|full-bad|full-posix-live|full-tiered*|lazy-only) printf 'Runtime Compiler: LLVM-JIT\n' ;;
        int-*) printf 'Runtime Compiler: UWVM2-Interpreter\n' ;;
        *) printf 'Runtime Compiler: LLVM-AOT\n' ;;
    esac
    case $profile in
        full-tiered-native|full-live|full-bad|full-posix-live) printf 'Call Stack Modes Support: instruction, unwind, unwind-uncheck\n' ;;
        full-product|full-product-no-wasm2|ros-product|ros-unsupported-wasm2|ros-*-remnant|ros-instruction|full-tiered*|lazy-only)
            printf 'Call Stack Modes Support: instruction, unwind-uncheck\n'
            ;;
        auto-none) printf 'Call Stack Modes Support: instruction\n' ;;
    esac
    exit 0
fi

if [[ ${1:-} == --help && ${2:-} == runtime ]]; then
    if [[ $profile == int-* ]]; then
        printf '%s\n' '--runtime-int'
        printf '%s\n' '--runtime-compile-threads'
        printf '%s\n' '--runtime-compiler-log'
    else
        case $profile in
            full-product|full-product-no-wasm2)
                printf '%s\n' '--runtime-int' '--runtime-aot' '--runtime-jit' '--runtime-tiered'
                printf '%s\n' '--runtime-custom-mode' '--runtime-custom-compiler'
                printf '%s\n' '--runtime-tiered-disable-uwvm-int-lazy-interpreter'
                printf '%s\n' '--runtime-tiered-disable-llvm-full-jit'
                ;;
            ros-product|ros-unsupported-wasm2)
                printf '%s\n' '--runtime-int' '--runtime-aot'
                ;;
            ros-custom-mode-remnant)
                printf '%s\n' '--runtime-int' '--runtime-aot' '--runtime-custom-mode'
                ;;
            ros-custom-compiler-remnant)
                printf '%s\n' '--runtime-int' '--runtime-aot' '--runtime-custom-compiler'
                ;;
            ros-tiered-disable-remnant)
                printf '%s\n' '--runtime-int' '--runtime-aot'
                printf '%s\n' '--runtime-tiered-disable-uwvm-int-lazy-interpreter'
                ;;
            ros-tiered-disable-t2-remnant)
                printf '%s\n' '--runtime-int' '--runtime-aot'
                printf '%s\n' '--runtime-tiered-disable-llvm-full-jit'
                ;;
            lazy-only)
                printf '%s\n' '--runtime-jit'
                ;;
            *)
                printf '%s\n' '--runtime-aot'
                if [[ $profile == full-tiered* ]]; then printf '%s\n' '--runtime-tiered'; fi
                if [[ $profile == full-live || $profile == full-bad || $profile == full-posix-live ]]; then
                    printf '%s\n' '--runtime-custom-mode' '--runtime-custom-compiler'
                fi
                if [[ $profile == ros-instruction ]]; then
                    # These longer options must not make the exact --runtime-tiered token
                    # appear advertised in a source-pruned runtime.
                    printf '%s\n' '--runtime-tiered-disable-uwvm-int-lazy-interpreter'
                    printf '%s\n' '--runtime-tiered-disable-llvm-full-jit'
                fi
                ;;
        esac
        printf '%s\n' '--runtime-llvm-jit-call-stack'
        case $profile in
            full-tiered-native|full-live|full-bad|full-posix-live) printf 'Usage: -Rllvm-call-stack [auto|instruction|none|unwind|unwind-uncheck]\n' ;;
            full-product|full-product-no-wasm2|ros-product|ros-unsupported-wasm2|ros-*-remnant|ros-instruction|full-tiered*|lazy-only)
                printf 'Usage: -Rllvm-call-stack [auto|instruction|none|unwind-uncheck]\n'
                ;;
            auto-none) printf 'Usage: -Rllvm-call-stack [auto|instruction|none]\n' ;;
        esac
    fi
    exit 0
fi
if [[ ${1:-} == --help && ${2:-} == wasm ]]; then
    if [[ $profile == int-* || $profile == full-product || $profile == ros-product || $profile == ros-unsupported-wasm2 || \
          $profile == ros-*-remnant || $profile == full-tiered* || \
          $profile == lazy-only ]]; then
        printf '%s\n' '--wasm-feature-wasm2'
    else
        printf 'mock wasm help\n'
    fi
    exit 0
fi

policy=auto
compiler_log=
wasm=
int_mode=0
compile_mode=unknown
compiler_backend=unknown
while (($# != 0)); do
    if [[ $profile == int-* && $1 == -Rllvm-* ]]; then
        printf 'int-only mock received forbidden LLVM argument: %s\n' "$1" >&2
        exit 97
    fi
    case $1 in
        -Rint)
            int_mode=1
            compile_mode=auto
            compiler_backend=int
            shift
            ;;
        -Raot)
            compile_mode=full
            compiler_backend=jit
            shift
            ;;
        -Rjit)
            compile_mode=lazy
            compiler_backend=jit
            shift
            ;;
        -Rtiered)
            compile_mode=lazy
            compiler_backend=tiered
            shift
            ;;
        -Rcm)
            compile_mode=$2
            shift 2
            ;;
        -Rcc)
            compiler_backend=$2
            shift 2
            ;;
        -Rllvm-call-stack)
            policy=$2
            shift 2
            ;;
        -Rclog)
            [[ ${2:-} == file ]]
            compiler_log=$3
            shift 3
            ;;
        --run)
            wasm=$2
            shift 2
            ;;
        *) shift ;;
    esac
done

[[ -n $compiler_log ]]
fixture=$(basename -- "$wasm")
if [[ $profile == ros-unsupported-wasm2 && $compiler_backend == jit ]]; then
    case $fixture in
        wasm2_bulk_memory.wasm)
            printf 'LLVM AOT capability preflight rejected module="mock": memory.init has no LLVM lowering\n' >&2
            exit 1
            ;;
        wasm2_multivalue.wasm)
            printf 'LLVM AOT capability preflight rejected module="mock": function signature has multiple results\n' >&2
            exit 1
            ;;
        wasm2_table_oob.wasm)
            printf 'LLVM AOT capability preflight rejected module="mock": table.copy has no LLVM lowering\n' >&2
            exit 1
            ;;
    esac
fi
if [[ $profile == int-* ]]; then
    ((int_mode)) || {
        printf 'int-only mock did not receive -Rint\n' >&2
        exit 98
    }
    effective=logical
elif [[ $compiler_backend == int ]]; then
    effective=logical
else
    case $profile:$policy in
        ros-instruction:unwind)
            printf 'call-stack mode unwind is not supported\n' >&2
            exit 2
            ;;
        auto-none:unwind|auto-none:unwind-uncheck)
            printf 'call-stack mode %s is not supported\n' "$policy" >&2
            exit 2
            ;;
    esac

    effective=$policy
    if [[ $policy == auto ]]; then
        case $profile in
            full-tiered-native|full-live|full-bad|full-posix-live) effective=unwind ;;
            ros-instruction|full-tiered*) effective=instruction ;;
            auto-none) effective=none ;;
        esac
    fi

    case $effective in
        instruction)
            backend=unwind.h
            check=off
            replace=yes
            frames=emit
            ;;
        unwind-uncheck)
            backend=unwind.h
            check=unchecked
            replace=yes
            frames=omit
            ;;
        unwind)
            if [[ $profile == full-bad ]]; then
                backend=unwind.h
                check=static
                replace=no
                frames=emit
            elif [[ $profile == full-posix-live ]]; then
                backend=unwind.h
                check=live
                replace=yes
                frames=omit
            else
                backend=win64-seh
                check=live
                replace=yes
                frames=omit
            fi
            ;;
        none)
            backend=unavailable
            check=off
            replace=no
            frames=omit
            ;;
    esac

    printf '[llvm-jit-full] optimize-start fn=0 call_stack=%s unwind_backend=%s unwind_check=%s unwind_replace_frames=%s call_stack_frames=%s\n' \
        "$effective" "$backend" "$check" "$replace" "$frames" >>"$compiler_log"
    if [[ $compiler_backend == jit && $compile_mode == lazy ]]; then
        printf '[llvm-jit-lazy] compile-end fn=0 cu=0 state=compiled\n' >>"$compiler_log"
    fi
fi

trap_kind=
stack=
case $fixture in
    oob_load.wasm)
        trap_kind='memory access out of bounds'
        stack='0 1 2 3'
        ;;
    float_to_int.wasm)
        trap_kind='invalid conversion to integer'
        stack='0 1 2 3'
        ;;
    wasm2_bulk_oob.wasm)
        trap_kind='memory access out of bounds'
        stack='0 1 2'
        ;;
    wasm2_table_oob.wasm)
        trap_kind='table access out of bounds'
        stack='0 1 2'
        ;;
    tiered_osr_oob.wasm)
        trap_kind='memory access out of bounds'
        stack='0 1 2'
        printf '[llvm-jit-lazy] tiered-osr-request fn=0 cu=0 lane=inline\n' >>"$compiler_log"
        printf '[llvm-jit-lazy] compile-end fn=0 cu=0 state=compiled\n' >>"$compiler_log"
        ;;
    tiered_full_ready_oob.wasm)
        trap_kind='memory access out of bounds'
        stack='0 1'
        printf '[llvm-jit-lazy] tiered-full-request module="mock" module_id=0 priority=0 reason=switch\n' >>"$compiler_log"
        if [[ $profile != full-tiered-no-ready ]]; then
            # Deliberately no T2 entry event: readiness must not be promoted to
            # execution/unwind coverage by the matrix's PASS classification.
            printf '[llvm-jit-lazy] tiered-full-ready module="mock" module_id=0 functions=2\n' >>"$compiler_log"
        fi
        ;;
esac

if [[ -n $trap_kind ]]; then
    if [[ $profile == int-bad-stack && $fixture == oob_load.wasm ]]; then
        stack='0 2 3'
    fi
    printf 'Runtime crash (%s)\n' "$trap_kind" >&2
    if [[ $effective != none ]]; then
        for index in $stack; do
            printf 'func_idx=%s\n' "$index" >&2
        done
    fi
    exit 1
fi
exit 0
MOCK

chmod +x "$MOCK_REALPATH" "$MOCK_WAT2WASM" "$MOCK_UWVM"

run_profile()
{
    local profile=$1
    local policies=$2
    local expected_rc=$3
    local modes=${4:-auto}
    local allow_unsupported=${5:-1}
    local case_name=${6:-$profile}
    local compile_threads=${7:-0}
    local product_profile=${8:-auto}
    local output="${TEST_TMP}/out-${case_name}"
    local rc=0
    local -a args=(
        --uwvm "$MOCK_UWVM"
        --wat2wasm "$MOCK_WAT2WASM"
        --output "$output"
        --label "$case_name"
        --profile "$product_profile"
        --modes "$modes"
        --jobs 1
        --compile-threads "$compile_threads"
        --timeout 5
        --max-rss-mib 0
        --memory-budget-mib 0
    )
    if [[ $policies != default ]]; then
        args+=(--policies "$policies")
    fi
    if ((allow_unsupported)); then
        args+=(--allow-unsupported)
    fi
    if PATH="${TEST_TMP}:$PATH" MOCK_PROFILE=$profile "$RUNNER" "${args[@]}" \
        >"${TEST_TMP}/${case_name}.log" 2>&1; then
        rc=0
    else
        rc=$?
    fi
    [[ $rc == "$expected_rc" ]] || {
        sed -n '1,200p' "${TEST_TMP}/${case_name}.log" >&2
        fail "${case_name}: expected exit ${expected_rc}, got ${rc}"
    }
    assert_tsv_columns "$output/results.tsv"
    printf '%s' "$output"
}

[[ -x $RUNNER ]] || fail "runner is not executable: $RUNNER"
! grep -Eq 'seeded-libunwind|resolved_jit_caller' "$RUNNER" || fail 'obsolete seeded-unwind requirement remains'

full_product_output=$(run_profile full-product logical,instruction 0 auto 0 full-product-auto 2 auto)
assert_tsv_value "$full_product_output/metadata.tsv" profile_requested auto
assert_tsv_value "$full_product_output/metadata.tsv" profile_resolved full
assert_tsv_value "$full_product_output/metadata.tsv" profile_source capability-auto-full
grep -q $'^capabilities\t.*custom_mode=1,custom_compiler=1,custom_selectors=1' "$full_product_output/metadata.tsv" ||
    fail 'Full product did not report both individual custom-selector capabilities'
assert_tsv_value "$full_product_output/metadata.tsv" selected_modes int-full,int-lazy,llvm-full,llvm-lazy,tiered
assert_tsv_value "$full_product_output/metadata.tsv" profile_n_a_modes none
assert_tsv_value "$full_product_output/metadata.tsv" release_qualified true
assert_tsv_value "$full_product_output/metadata.tsv" release_qualification_reason fail-and-skip-counts-zero-and-pass-count-positive
assert_tsv_value "$full_product_output/metadata.tsv" result_pass_count 42
assert_tsv_value "$full_product_output/metadata.tsv" result_fail_count 0
assert_tsv_value "$full_product_output/metadata.tsv" result_skip_count 0
assert_tsv_value "$full_product_output/metadata.tsv" result_n_a_count 0
assert_tsv_value "$full_product_output/metadata.tsv" runner_sha256 "$(test_sha256_file "$RUNNER")"
assert_tsv_value "$full_product_output/metadata.tsv" uwvm_sha256 "$(test_sha256_file "$MOCK_UWVM")"
assert_tsv_value "$full_product_output/metadata.tsv" wat2wasm_sha256 "$(test_sha256_file "$MOCK_WAT2WASM")"
grep -Eq $'^run_started_utc\t[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$' \
    "$full_product_output/metadata.tsv" || fail 'missing canonical UTC start timestamp'
grep -Eq $'^run_finished_utc\t[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$' \
    "$full_product_output/metadata.tsv" || fail 'missing canonical UTC finish timestamp'
fixture_manifest="$full_product_output/fixture-manifest.tsv"
[[ $(awk 'END { print NR }' "$fixture_manifest") == 11 ]] || fail 'fixture manifest does not contain exactly ten fixtures plus its header'
assert_tsv_value "$full_product_output/metadata.tsv" fixture_manifest "$fixture_manifest"
assert_tsv_value "$full_product_output/metadata.tsv" fixture_manifest_sha256 "$(test_sha256_file "$fixture_manifest")"
grep -Fq $'wat2wasm_command_template\t' "$full_product_output/metadata.tsv" || fail 'missing wat2wasm command template'
grep -Fq '%INPUT_WAT% -o %OUTPUT_WASM%' "$full_product_output/metadata.tsv" ||
    fail 'wat2wasm command template does not identify its input and output operands'
mvp_source=$(realpath "${SCRIPT_DIR}/../../test/0014.llvm_jit/nontrivial_start.wat")
mvp_wasm="$full_product_output/fixtures/mvp_smoke.wasm"
awk -F '\t' -v source="$mvp_source" -v source_sha="$(test_sha256_file "$mvp_source")" \
    -v wasm="$mvp_wasm" -v wasm_sha="$(test_sha256_file "$mvp_wasm")" \
    -v tool_sha="$(test_sha256_file "$MOCK_WAT2WASM")" \
    'NR > 1 && $1 == "mvp_smoke" && $2 == source && $3 == source_sha && $4 == source &&
        $5 == source_sha && $6 == wasm && $7 == wasm_sha && $9 == tool_sha { found = 1 }
     END { exit !found }' "$fixture_manifest" || fail 'MVP fixture provenance row is incomplete or incorrect'
assert_tsv_value "$full_product_output/metadata.tsv" selected_mode_arguments \
    'int-full=-Rcm full -Rcc int[canonical-explicit];int-lazy=-Rcm lazy -Rcc int[canonical-explicit];llvm-full=-Rcm full -Rcc jit[canonical-explicit];llvm-lazy=-Rcm lazy -Rcc jit[canonical-explicit];tiered=-Rtiered[canonical-retained-alias]'
assert_unique_metadata_keys "$full_product_output/metadata.tsv"
assert_mode_pass_count "$full_product_output/results.tsv" int-full 8
assert_mode_pass_count "$full_product_output/results.tsv" int-lazy 8
assert_mode_pass_count "$full_product_output/results.tsv" llvm-full 8
assert_mode_pass_count "$full_product_output/results.tsv" llvm-lazy 8
assert_mode_pass_count "$full_product_output/results.tsv" tiered 10
assert_no_fail_rows "$full_product_output/results.tsv"
grep -Fq -- '-Rcm full -Rcc int -Rct 2' "$full_product_output"/logs/*-int-full-logical-mvp_smoke.log ||
    fail 'canonical int-full did not use explicit full/int selectors'
grep -Fq -- '-Rcm lazy -Rcc int -Rct 2' "$full_product_output"/logs/*-int-lazy-logical-mvp_smoke.log ||
    fail 'canonical int-lazy did not use explicit lazy/int selectors'
grep -Fq -- '-Rcm full -Rcc jit -Rct 2' "$full_product_output"/logs/*-llvm-full-instruction-mvp_smoke.log ||
    fail 'canonical llvm-full did not use explicit full/jit selectors'
grep -Fq -- '-Rcm lazy -Rcc jit -Rct 2' "$full_product_output"/logs/*-llvm-lazy-instruction-mvp_smoke.log ||
    fail 'canonical llvm-lazy did not use explicit lazy/jit selectors'
grep -Fq -- '-Rtiered -Rct 2' "$full_product_output"/logs/*-tiered-instruction-mvp_smoke.log ||
    fail 'canonical tiered did not retain the tiered selector'
if grep -E -- '-Rllvm-' "$full_product_output"/logs/*-int-{full,lazy}-logical-*.log >/dev/null 2>&1; then
    fail 'canonical interpreter mode received a forbidden LLVM argument'
fi
if grep -Eq $'\t(tiered-no-t0|tiered-no-t2|tiered-no-t0-no-t2)\t' "$full_product_output/results.tsv"; then
    fail 'Full auto selected a diagnostic tiered variant'
fi

ros_product_output=$(run_profile ros-product logical,instruction 0 auto 0 ros-product-auto 0 auto)
assert_tsv_value "$ros_product_output/metadata.tsv" profile_requested auto
assert_tsv_value "$ros_product_output/metadata.tsv" profile_resolved ros
assert_tsv_value "$ros_product_output/metadata.tsv" profile_source capability-auto-ros
grep -q $'^capabilities\t.*custom_mode=0,custom_compiler=0,custom_selectors=0' "$ros_product_output/metadata.tsv" ||
    fail 'ROS product did not report both custom selectors as source-pruned'
assert_tsv_value "$ros_product_output/metadata.tsv" selected_modes int-full,llvm-full
assert_tsv_value "$ros_product_output/metadata.tsv" release_qualified true
assert_tsv_value "$ros_product_output/metadata.tsv" result_pass_count 16
assert_tsv_value "$ros_product_output/metadata.tsv" result_fail_count 0
assert_tsv_value "$ros_product_output/metadata.tsv" result_n_a_count 3
assert_tsv_value "$ros_product_output/metadata.tsv" profile_n_a_modes \
    int-lazy,llvm-lazy,tiered,full,lazy,tiered-no-t0,tiered-no-t2,tiered-no-t0-no-t2
assert_tsv_value "$ros_product_output/metadata.tsv" selected_mode_arguments \
    'int-full=-Rint[canonical-ros-alias];llvm-full=-Raot[canonical-ros-alias]'
assert_unique_metadata_keys "$ros_product_output/metadata.tsv"
assert_mode_pass_count "$ros_product_output/results.tsv" int-full 8
assert_mode_pass_count "$ros_product_output/results.tsv" llvm-full 8
awk -F '\t' 'NR > 1 && $15 == "N-A" { count++ } END { exit count != 3 }' "$ros_product_output/results.tsv" ||
    fail 'ROS auto did not record the three source-pruned product modes as N-A'
assert_no_fail_rows "$ros_product_output/results.tsv"
grep -Fq -- '-Rint -Rct 0' "$ros_product_output"/logs/*-int-full-logical-mvp_smoke.log ||
    fail 'ROS canonical int-full did not map to the retained -Rint alias'
grep -Fq -- '-Raot -Rct 0' "$ros_product_output"/logs/*-llvm-full-instruction-mvp_smoke.log ||
    fail 'ROS canonical llvm-full did not map to the retained -Raot alias'
if grep -R -E -- '-Rcm|-Rcc|-Rjit|-Rtiered' "$ros_product_output/logs" >/dev/null 2>&1; then
    fail 'ROS product auto used a source-pruned runtime selector'
fi

ros_na_output=$(run_profile ros-product logical,instruction 1 int-lazy,llvm-lazy,tiered,full,lazy 0 ros-product-n-a 0 ros)
awk -F '\t' 'NR > 1 && $15 == "N-A" { count++ } END { exit count != 5 }' "$ros_na_output/results.tsv" ||
    fail 'ROS removed-mode request did not produce five N-A rows'
if awk -F '\t' 'NR > 1 && ($11 != "capability" || $15 != "N-A") { bad = 1 } END { exit bad }' \
    "$ros_na_output/results.tsv"; then :; else
    fail 'ROS removed-mode matrix produced a non-capability or non-N-A row'
fi
if find "$ros_na_output/logs" -maxdepth 1 -type f \
    \( -name '*int-lazy*' -o -name '*llvm-lazy*' -o -name '*tiered-*' -o -name '*-full-*' -o -name '*-lazy-*' \) \
    -print -quit | grep -q .; then
    fail 'ROS source-pruned mode unexpectedly launched a fixture'
fi
assert_tsv_value "$ros_na_output/metadata.tsv" result_pass_count 0
assert_tsv_value "$ros_na_output/metadata.tsv" result_fail_count 0
assert_tsv_value "$ros_na_output/metadata.tsv" result_n_a_count 5
assert_tsv_value "$ros_na_output/metadata.tsv" release_qualified false
assert_tsv_value "$ros_na_output/metadata.tsv" release_qualification_reason no-passing-executed-case

ros_na_allowed_output=$(run_profile ros-product logical,instruction 1 int-lazy,llvm-lazy,tiered,full,lazy 1 ros-product-n-a-allowed 0 ros)
awk -F '\t' 'NR > 1 && $15 == "N-A" { count++ } END { exit count != 5 }' "$ros_na_allowed_output/results.tsv" ||
    fail '--allow-unsupported changed ROS N-A classification'
assert_tsv_value "$ros_na_allowed_output/metadata.tsv" release_qualified false

product_all_skip_output=$(run_profile full-product instruction 1 int-full 1 full-product-all-skip 0 full)
grep -q $'\tint-full\tinstruction\t-\tcapability\tcapability\tan applicable policy\t-\tSKIP\t' \
    "$product_all_skip_output/results.tsv" || fail 'product all-SKIP setup did not produce its expected SKIP row'
assert_tsv_value "$product_all_skip_output/metadata.tsv" result_pass_count 0
assert_tsv_value "$product_all_skip_output/metadata.tsv" result_fail_count 0
assert_tsv_value "$product_all_skip_output/metadata.tsv" result_skip_count 1
assert_tsv_value "$product_all_skip_output/metadata.tsv" release_qualified false
assert_tsv_value "$product_all_skip_output/metadata.tsv" release_qualification_reason skip-count-nonzero

product_mixed_skip_output=$(run_profile full-product-no-wasm2 logical,instruction 1 auto 1 full-product-mixed-skip 2 full)
awk -F '\t' 'NR > 1 && $15 == "PASS" { count++ } END { exit count == 0 }' "$product_mixed_skip_output/results.tsv" ||
    fail 'product mixed-SKIP setup did not execute any passing cases'
awk -F '\t' 'NR > 1 && $15 == "SKIP" { count++ } END { exit count == 0 }' "$product_mixed_skip_output/results.tsv" ||
    fail 'product mixed-SKIP setup did not produce an unsupported-policy SKIP'
assert_tsv_value "$product_mixed_skip_output/metadata.tsv" result_fail_count 0
assert_tsv_value "$product_mixed_skip_output/metadata.tsv" release_qualified false
assert_tsv_value "$product_mixed_skip_output/metadata.tsv" release_qualification_reason skip-count-nonzero

assert_profile_rejected ros-custom-mode-remnant ros-custom-mode-remnant-rejected
assert_profile_rejected ros-custom-compiler-remnant ros-custom-compiler-remnant-rejected
assert_profile_rejected ros-tiered-disable-remnant ros-tiered-disable-remnant-rejected
assert_profile_rejected ros-tiered-disable-t2-remnant ros-tiered-disable-t2-remnant-rejected

legacy_int_output=$(run_profile full-product logical 0 int 0 legacy-int 0 full)
grep -Fq -- '-Rint -Rct 0' "$legacy_int_output"/logs/*-int-logical-mvp_smoke.log ||
    fail 'legacy int alias changed arguments'
if grep -E -- '-Rcm|-Rcc' "$legacy_int_output"/logs/*-int-logical-mvp_smoke.log >/dev/null 2>&1; then
    fail 'legacy int alias was silently canonicalized'
fi
printf 'stale fixture that must never be reused' >"$legacy_int_output/fixtures/mvp_smoke.wasm"
legacy_int_rebuilt_output=$(run_profile full-product logical 0 int 0 legacy-int 0 full)
[[ $legacy_int_rebuilt_output == "$legacy_int_output" ]] || fail 'fixture rebuild check unexpectedly changed output directory'
[[ ! -s $legacy_int_rebuilt_output/fixtures/mvp_smoke.wasm ]] ||
    fail 'newer stale fixture was reused instead of being deterministically rebuilt'
assert_tsv_value "$legacy_int_rebuilt_output/metadata.tsv" fixture_manifest_sha256 \
    "$(test_sha256_file "$legacy_int_rebuilt_output/fixture-manifest.tsv")"
legacy_aot_output=$(run_profile full-product instruction 0 aot 0 legacy-aot 0 full)
grep -Fq -- '-Raot -Rct 0' "$legacy_aot_output"/logs/*-aot-instruction-mvp_smoke.log ||
    fail 'legacy aot alias changed arguments'
legacy_full_output=$(run_profile full-product instruction 0 full 0 legacy-full 0 full)
grep -Fq -- '-Rcm full -Rcc jit -Rct 0' "$legacy_full_output"/logs/*-full-instruction-mvp_smoke.log ||
    fail 'legacy full alias changed arguments'
legacy_lazy_output=$(run_profile full-product instruction 0 lazy 0 legacy-lazy 0 full)
grep -Fq -- '-Rjit -Rct 0' "$legacy_lazy_output"/logs/*-lazy-instruction-mvp_smoke.log ||
    fail 'legacy lazy alias changed arguments'
if grep -E -- '-Rcm|-Rcc' "$legacy_lazy_output"/logs/*-lazy-instruction-mvp_smoke.log >/dev/null 2>&1; then
    fail 'legacy lazy alias was silently canonicalized'
fi

broken_canonical_output=$(run_profile full-live logical 1 int-lazy 0 broken-canonical)
grep -q $'\tint-lazy\t-\t-\tcapability\tcapability\tsupported\t-\tFAIL\trequested runtime mode is not advertised' \
    "$broken_canonical_output/results.tsv" || fail 'unknown/broken Full mode was incorrectly classified N-A'

legacy_int_full_output=$(run_profile int-only logical 1 int-full 0 legacy-int-full-rejected)
grep -q $'\tint-full\t-\t-\tcapability\tcapability\tsupported\t-\tFAIL\trequested runtime mode is not advertised' \
    "$legacy_int_full_output/results.tsv" || fail 'canonical int-full fell back to the legacy -Rint auto selector'
if find "$legacy_int_full_output/logs" -maxdepth 1 -type f -name '*int-full*' -print -quit | grep -q .; then
    fail 'unsupported canonical int-full unexpectedly launched a fixture'
fi

conflict_output="${TEST_TMP}/out-canonical-conflict"
set +e
PATH="${TEST_TMP}:$PATH" MOCK_PROFILE=full-product "$RUNNER" \
    --uwvm "$MOCK_UWVM" --wat2wasm "$MOCK_WAT2WASM" --output "$conflict_output" \
    --label canonical-conflict --profile full --modes int-full --policies logical \
    --uwvm-arg -Rcm --uwvm-arg lazy --max-rss-mib 0 --memory-budget-mib 0 \
    >"${TEST_TMP}/canonical-conflict.log" 2>&1
conflict_rc=$?
set -e
[[ $conflict_rc == 2 ]] || fail "canonical selector conflict: expected exit 2, got ${conflict_rc}"
grep -q 'canonical modes reject a conflicting --uwvm-arg runtime selector: -Rcm' \
    "${TEST_TMP}/canonical-conflict.log" || fail 'canonical selector conflict lacked a precise diagnostic'

int_output=$(run_profile int-only default 0 auto 0 int-only-default)
assert_tsv_value "$int_output/metadata.tsv" selected_modes int
assert_tsv_value "$int_output/metadata.tsv" compile_threads 0
assert_tsv_value "$int_output/metadata.tsv" supported_policies logical
assert_tsv_value "$int_output/metadata.tsv" policy_probe_mode unavailable
assert_tsv_value "$int_output/metadata.tsv" auto_effective_policy unavailable
assert_tsv_value "$int_output/metadata.tsv" unwind_uncheck_replacement_status unavailable
grep -q $'\tint\tlogical\tlogical\tmvp_smoke\tnormal\tok\t0\tPASS\t' "$int_output/results.tsv" ||
    fail 'int-only MVP correctness row did not pass with logical stack policy'
grep -q $'\tint\tlogical\tlogical\toob_load\ttrap\tmemory access out of bounds\t1\tPASS\t' "$int_output/results.tsv" ||
    fail 'int-only trap/logical-stack row did not pass'
awk -F '\t' 'NR > 1 && $8 == "int" && $9 == "logical" && $15 == "PASS" { count++ } END { exit count != 8 }' \
    "$int_output/results.tsv" || fail 'int-only default matrix did not run all eight applicable fixtures'
if grep -R -E -- '-Rllvm-' "$int_output/logs" >/dev/null 2>&1; then
    fail 'int-only command or probe received a forbidden -Rllvm-* argument'
fi
assert_no_fail_rows "$int_output/results.tsv"

int_bad_stack_output=$(run_profile int-bad-stack logical 1 int 0)
assert_tsv_columns "$int_bad_stack_output/results.tsv"
grep -q $'\tint\tlogical\tlogical\toob_load\ttrap\tmemory access out of bounds\t1\tFAIL\tcall stack mismatch: expected 0,1,2,3, actual 0,2,3' \
    "$int_bad_stack_output/results.tsv" || fail 'missing interpreter logical frame was not rejected'

int_fail_output=$(run_profile int-only instruction 1 int 0 int-only-instruction-fail)
assert_tsv_columns "$int_fail_output/results.tsv"
grep -q $'\tint\tinstruction\t-\tcapability\tcapability\tan applicable policy\t-\tFAIL\tinterpreter mode requires the logical policy' \
    "$int_fail_output/results.tsv" || fail 'explicit int/instruction mismatch was not reported as FAIL'

int_skip_output=$(run_profile int-only instruction 0 int 1 int-only-instruction-skip)
grep -q $'\tint\tinstruction\t-\tcapability\tcapability\tan applicable policy\t-\tSKIP\tinterpreter mode requires the logical policy' \
    "$int_skip_output/results.tsv" || fail 'allowed int/instruction mismatch was not reported as SKIP'
assert_tsv_value "$int_skip_output/metadata.tsv" profile_resolved legacy
assert_tsv_value "$int_skip_output/metadata.tsv" release_qualified not-applicable
assert_no_fail_rows "$int_skip_output/results.tsv"

llvm_skip_output=$(run_profile full-live logical 0 full 1 full-logical-skip)
grep -q $'\tfull\tlogical\t-\tcapability\tcapability\tan applicable policy\t-\tSKIP\tLLVM runtime mode requires instruction' \
    "$llvm_skip_output/results.tsv" || fail 'allowed full/logical mismatch was not reported as SKIP'
assert_no_fail_rows "$llvm_skip_output/results.tsv"

lazy_probe_output=$(run_profile lazy-only unwind-uncheck 1 lazy 0)
assert_tsv_columns "$lazy_probe_output/results.tsv"
grep -q $'\tunwind-uncheck\tunavailable\tunwind-uncheck-replacement-probe\tcapability\tan advertised AOT/full policy-probe mode\t-\tFAIL\t' \
    "$lazy_probe_output/results.tsv" || fail 'missing native replacement probe did not produce an explicit failure'

tiered_serial_output=$(run_profile full-tiered instruction 0 tiered 0)
assert_no_fail_rows "$tiered_serial_output/results.tsv"
grep -q $'\ttiered\tinstruction\t-\ttiered_full_ready_oob\ttrap\tmemory access out of bounds\t-\tSKIP\ttier-2 background readiness requires --compile-threads >= 2' \
    "$tiered_serial_output/results.tsv" || fail 'tier-2 witness did not preserve the requested zero-worker cap'
if grep -R -E -- '-Rct [1-9]' "$tiered_serial_output/logs" >/dev/null 2>&1; then
    fail 'tiered matrix exceeded the requested zero-worker cap'
fi

tiered_ready_output=$(run_profile full-tiered-ready instruction 0 tiered 0 tiered-ready-only 2)
assert_no_fail_rows "$tiered_ready_output/results.tsv"
assert_tsv_value "$tiered_ready_output/metadata.tsv" compile_threads 2
assert_tsv_value "$tiered_ready_output/metadata.tsv" tier2_witness_scope publication-readiness-and-trap
assert_tsv_value "$tiered_ready_output/metadata.tsv" t2_entry_execution unverified
assert_tsv_value "$tiered_ready_output/metadata.tsv" t2_unwind_coverage unverified
grep -q $'\ttiered_full_ready_oob\ttrap\tmemory access out of bounds\t1\tPASS\t.*t2_publication=ready;t2_entry_execution=unverified;t2_unwind_coverage=unverified' \
    "$tiered_ready_output/results.tsv" || fail 'ready-only evidence was not explicitly limited to publication and trap'

tiered_no_ready_output=$(run_profile full-tiered-no-ready instruction 1 tiered 0 tiered-missing-ready 2)
grep -q $'\ttiered_full_ready_oob\ttrap\tmemory access out of bounds\t1\tFAIL\trequired tier-2 request/readiness evidence is missing' \
    "$tiered_no_ready_output/results.tsv" || fail 'trap without T2 readiness was accepted'

tiered_native_only_output=$(run_profile full-tiered-ready unwind-uncheck 0 tiered 0 tiered-native-only 2)
assert_no_fail_rows "$tiered_native_only_output/results.tsv"
assert_tsv_value "$tiered_native_only_output/metadata.tsv" tier2_witness_policy unavailable
grep -q $'\ttiered\t-\t-\ttiered_full_ready_oob\ttrap\tmemory access out of bounds\t-\tSKIP\tno compatible instruction policy' \
    "$tiered_native_only_output/results.tsv" || fail 'native unwind was treated as permitting background T2'

tiered_native_auto_output=$(run_profile full-tiered-native auto,instruction 0 tiered 0 tiered-native-auto 2)
assert_no_fail_rows "$tiered_native_auto_output/results.tsv"
assert_tsv_value "$tiered_native_auto_output/metadata.tsv" auto_effective_policy unwind
assert_tsv_value "$tiered_native_auto_output/metadata.tsv" tier2_witness_policy instruction
if grep -q $'\ttiered\tauto\t.*\ttiered_full_ready_oob\t' "$tiered_native_auto_output/results.tsv"; then
    fail 'auto resolved to native unwind was selected for T2 readiness'
fi

ros_output=$(run_profile ros-instruction auto,unwind,unwind-uncheck 0)
assert_tsv_value "$ros_output/metadata.tsv" selected_modes aot
assert_tsv_value "$ros_output/metadata.tsv" auto_effective_policy instruction
assert_tsv_value "$ros_output/metadata.tsv" unwind_uncheck_replacement_status verified
grep -q $'^capabilities\t.*unwind=0,unwind_uncheck=1' "$ros_output/metadata.tsv" ||
    fail 'unwind and unwind-uncheck capabilities were not separated'
grep -q $'unwind\tunsupported\tunwind-availability\tcapability\trejected\t2\tPASS' "$ros_output/results.tsv" ||
    fail 'unsupported unwind was not rejection-probed'
assert_no_fail_rows "$ros_output/results.tsv"

ros_unsupported_output=$(run_profile ros-unsupported-wasm2 logical,instruction 0 auto 0 ros-unsupported-wasm2 0 ros)
assert_no_fail_rows "$ros_unsupported_output/results.tsv"
awk -F '\t' 'NR > 1 && $8 == "llvm-full" && $15 == "N-A" { count++ } END { exit count != 3 }' \
    "$ros_unsupported_output/results.tsv" || fail 'ROS source-pruned LLVM capabilities were not recorded as three N-A rows'
grep -q $'\tllvm-full\tinstruction\tinstruction\twasm2_bulk_memory\tnormal\tok\t1\tN-A\tROS LLVM-AOT source-pruned capability: memory.init has no lowering\t' \
    "$ros_unsupported_output/results.tsv" || fail 'ROS memory.init preflight rejection was not classified precisely'

live_output=$(run_profile full-live auto 0)
assert_tsv_value "$live_output/metadata.tsv" selected_modes full
assert_tsv_value "$live_output/metadata.tsv" auto_effective_policy unwind
grep -q 'unwind_check=live;unwind_replace_frames=yes' "$live_output/results.tsv" ||
    fail 'authoritative auto unwind was not accepted from structured fields'
assert_no_fail_rows "$live_output/results.tsv"

bad_output=$(run_profile full-bad auto 1)
assert_tsv_value "$bad_output/metadata.tsv" auto_effective_policy invalid
grep -q $'auto-live-probe\ttrap\tinstruction-or-checked-unwind\t1\tFAIL' "$bad_output/results.tsv" ||
    fail 'unchecked/non-replacing auto unwind was not rejected'

posix_live_output=$(run_profile full-posix-live auto 0)
assert_tsv_value "$posix_live_output/metadata.tsv" auto_effective_policy unwind
grep -q 'auto=unwind;unwind_check=live;unwind_replace_frames=yes;call_stack_frames=omit' "$posix_live_output/results.tsv" ||
    fail 'checked POSIX unwind.h replacement was not accepted'
assert_no_fail_rows "$posix_live_output/results.tsv"

none_output=$(run_profile auto-none auto 1)
assert_tsv_value "$none_output/metadata.tsv" selected_modes aot
assert_tsv_value "$none_output/metadata.tsv" auto_effective_policy invalid
grep -q $'auto-live-probe\ttrap\tinstruction-or-checked-unwind\t1\tFAIL' "$none_output/results.tsv" ||
    fail 'impossible auto none resolution was not rejected'
grep -q 'invalid auto resolution to none' "$none_output/results.tsv" ||
    fail 'auto none rejection did not explain the invalid runtime state'

printf 'test_run_linux_qemu_matrix: PASS\n'

#!/usr/bin/env bash

# Run an already-built uwvm Linux binary, optionally through qemu-user or a
# wrapper. This script deliberately does not build uwvm, WABT, sysroots, or
# container images.

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd -- "${SCRIPT_DIR}/../.." && pwd)

usage()
{
    cat <<'EOF'
Usage:
  tools/ci/run_linux_qemu_matrix.sh --uwvm <path> [options]

Required:
  --uwvm <path>                 Existing target uwvm binary.

Target execution:
  --wrapper <words>             Prefix target commands, for example
                                  --wrapper 'qemu-aarch64 -L /opt/sysroot'
                                Shell operators and expansion are not evaluated.
  --wrapper-arg <arg>           Append one exact wrapper argument. Repeatable;
                                useful when an argument contains whitespace.
  --uwvm-arg <arg>              Add one uwvm argument before --run. Repeatable.
  --label <name>                Platform/build label used in TSV output.
  --profile <auto|full|ros>     Product surface used by canonical mode names.
                                auto recognizes a complete Full or reduced ROS
                                command surface and otherwise preserves legacy
                                capability selection. Default: auto.

Fixture and output:
  --wat2wasm <path>             Existing wat2wasm. Defaults to WAT2WASM, PATH,
                                or an existing build/test WABT checkout.
  --output <dir>                Output directory. Default:
                                  build/test/linux-qemu-matrix/<label>
  --expect-memory-model <name>  Assert version output reports this model. The
                                aliases mmap, multi-thread-alloc, and
                                single-thread-alloc are accepted.

Matrix and resource controls:
  --modes <csv|auto>            Runtime modes to exercise. Canonical values are
                                int-full, int-lazy, llvm-full, llvm-lazy, and
                                tiered. Full auto selects those five; ROS auto
                                selects int-full and llvm-full. Legacy int,
                                aot, full, and lazy aliases remain unchanged.
                                Explicit diagnostic values also include
                                tiered-no-t0, tiered-no-t2, and
                                tiered-no-t0-no-t2.
  --policies <csv>              logical, instruction, auto, unwind, and
                                unwind-uncheck. Default: all five. logical is
                                the implicit interpreter stack policy and never
                                adds an LLVM call-stack option. Unsupported
                                native policies are rejection-probed but not run
                                as matrix rows.
  --jobs <n>                    Concurrent target processes (default: 1).
  --compile-threads <n>         -Rct extra worker count for each target (default:
                                0, compilation stays on the calling thread).
                                The tier-2 background readiness witness needs
                                at least 2 workers and is SKIP below that cap.
                                Readiness plus a trap does not prove T2 entry
                                execution or T2 unwind coverage.
  --timeout <seconds>           Per probe/fixture/run timeout (default: 120).
  --max-rss-mib <MiB>           Sampled per-process-group soft RSS cap (default:
                                4096; 0 disables). Use a container/cgroup memory
                                limit as the hard cap for builds and QEMU runs.
  --memory-budget-mib <MiB>     Reject jobs * max-RSS above this (default: 8192;
                                0 disables the aggregate check).
  --allow-unsupported           Record unavailable required capabilities as
                                SKIP instead of FAIL.
  -h, --help                    Show this help.

Canonical Full modes use explicit -Rcm/-Rcc pairs, so -Rint's automatic compile
mode can never be mistaken for int-full. Canonical ROS int-full and llvm-full
map to the retained -Rint and -Raot aliases; removed ROS modes are N-A and are
never executed. Legacy aliases keep their old argument spelling and selection
semantics. Normal fixtures must exit zero. Trap fixtures must exit nonzero and
report the exact Wasm trap kind. Metadata, results.tsv, and per-run logs are
written below build/test by default.
The tiered_full_ready_oob case checks publication/readiness and the trap only;
its PASS explicitly retains t2_entry_execution=unverified. It uses instruction
or auto resolved to instruction, since native unwind disables background T2.
EOF
}

die()
{
    printf 'run_linux_qemu_matrix: error: %s\n' "$*" >&2
    exit 2
}

is_uint()
{
    [[ $1 =~ ^[0-9]+$ ]]
}

UWVM=
WAT2WASM_BIN=${WAT2WASM:-}
OUTPUT_DIR=
LABEL=
PROFILE_REQUESTED=${UWVM_QEMU_PROFILE:-auto}
EXPECT_MEMORY_MODEL=
MODE_CSV=${UWVM_QEMU_MODES:-auto}
POLICY_CSV=logical,instruction,auto,unwind,unwind-uncheck
JOBS=${UWVM_QEMU_JOBS:-1}
COMPILE_THREADS=${UWVM_QEMU_COMPILE_THREADS:-0}
TIMEOUT_SECONDS=${UWVM_QEMU_TIMEOUT:-120}
MAX_RSS_MIB=${UWVM_QEMU_MAX_RSS_MIB:-4096}
MEMORY_BUDGET_MIB=${UWVM_QEMU_MEMORY_BUDGET_MIB:-8192}
ALLOW_UNSUPPORTED=0
RUN_PREFIX=()
EXTRA_UWVM_ARGS=()

while (($# != 0)); do
    case $1 in
        --uwvm)
            (($# >= 2)) || die "--uwvm requires a path"
            UWVM=$2
            shift 2
            ;;
        --wrapper|--qemu)
            (($# >= 2)) || die "$1 requires a command prefix"
            read -r -a RUN_PREFIX <<<"$2"
            ((${#RUN_PREFIX[@]} != 0)) || die "$1 must not be empty"
            shift 2
            ;;
        --wrapper-arg)
            (($# >= 2)) || die "--wrapper-arg requires an argument"
            RUN_PREFIX+=("$2")
            shift 2
            ;;
        --uwvm-arg)
            (($# >= 2)) || die "--uwvm-arg requires an argument"
            EXTRA_UWVM_ARGS+=("$2")
            shift 2
            ;;
        --wat2wasm)
            (($# >= 2)) || die "--wat2wasm requires a path"
            WAT2WASM_BIN=$2
            shift 2
            ;;
        --output)
            (($# >= 2)) || die "--output requires a directory"
            OUTPUT_DIR=$2
            shift 2
            ;;
        --label)
            (($# >= 2)) || die "--label requires a value"
            LABEL=$2
            shift 2
            ;;
        --profile)
            (($# >= 2)) || die "--profile requires auto, full, or ros"
            PROFILE_REQUESTED=$2
            shift 2
            ;;
        --expect-memory-model)
            (($# >= 2)) || die "--expect-memory-model requires a value"
            EXPECT_MEMORY_MODEL=$2
            shift 2
            ;;
        --policies)
            (($# >= 2)) || die "--policies requires a comma-separated list"
            POLICY_CSV=$2
            shift 2
            ;;
        --modes)
            (($# >= 2)) || die "--modes requires auto or a comma-separated list"
            MODE_CSV=$2
            shift 2
            ;;
        --jobs)
            (($# >= 2)) || die "--jobs requires a value"
            JOBS=$2
            shift 2
            ;;
        --compile-threads)
            (($# >= 2)) || die "--compile-threads requires a value"
            COMPILE_THREADS=$2
            shift 2
            ;;
        --timeout)
            (($# >= 2)) || die "--timeout requires a value"
            TIMEOUT_SECONDS=$2
            shift 2
            ;;
        --max-rss-mib)
            (($# >= 2)) || die "--max-rss-mib requires a value"
            MAX_RSS_MIB=$2
            shift 2
            ;;
        --memory-budget-mib)
            (($# >= 2)) || die "--memory-budget-mib requires a value"
            MEMORY_BUDGET_MIB=$2
            shift 2
            ;;
        --allow-unsupported)
            ALLOW_UNSUPPORTED=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            break
            ;;
        *)
            die "unknown option: $1"
            ;;
    esac
done

(($# == 0)) || die "unexpected positional arguments: $*"
[[ -n $UWVM ]] || die "--uwvm is required"
[[ -f $UWVM ]] || die "uwvm binary does not exist: $UWVM"
case $PROFILE_REQUESTED in
    auto|full|ros) ;;
    *) die "--profile must be auto, full, or ros: $PROFILE_REQUESTED" ;;
esac

for numeric in "$JOBS" "$COMPILE_THREADS" "$TIMEOUT_SECONDS" "$MAX_RSS_MIB" "$MEMORY_BUDGET_MIB"; do
    is_uint "$numeric" || die "resource-control values must be non-negative integers"
done
((JOBS >= 1)) || die "--jobs must be at least 1"
((TIMEOUT_SECONDS >= 1)) || die "--timeout must be at least 1"
if ((MAX_RSS_MIB != 0 && MEMORY_BUDGET_MIB != 0 && JOBS * MAX_RSS_MIB > MEMORY_BUDGET_MIB)); then
    die "jobs * max RSS exceeds --memory-budget-mib (${JOBS} * ${MAX_RSS_MIB} > ${MEMORY_BUDGET_MIB})"
fi

if ((${#RUN_PREFIX[@]} == 0)); then
    [[ -x $UWVM ]] || die "uwvm is not executable and no wrapper was supplied: $UWVM"
else
    command -v -- "${RUN_PREFIX[0]}" >/dev/null 2>&1 || die "wrapper executable not found: ${RUN_PREFIX[0]}"
fi

if [[ -z $WAT2WASM_BIN ]]; then
    if command -v wat2wasm >/dev/null 2>&1; then
        WAT2WASM_BIN=$(command -v wat2wasm)
    else
        for candidate in \
            "${ROOT_DIR}/build/test/third-parties/wabt/build/wat2wasm" \
            "${ROOT_DIR}/build/test/third-parties/wabt/build/bin/wat2wasm" \
            "${ROOT_DIR}/build/test/third-parties/wabt/build/Release/wat2wasm" \
            "${ROOT_DIR}/wabt/build/wat2wasm" \
            "${ROOT_DIR}/wabt/build/bin/wat2wasm"; do
            if [[ -x $candidate ]]; then
                WAT2WASM_BIN=$candidate
                break
            fi
        done
    fi
fi
[[ -n $WAT2WASM_BIN && -x $WAT2WASM_BIN ]] || die "wat2wasm not found; pass --wat2wasm or set WAT2WASM"

UWVM=$(realpath -m -- "$UWVM")
WAT2WASM_BIN=$(realpath -m -- "$WAT2WASM_BIN")
RUNNER_PATH=$(realpath -m -- "${BASH_SOURCE[0]}")

if [[ -z $LABEL ]]; then
    LABEL=$(basename -- "$(dirname -- "$UWVM")")
    case $LABEL in
        debug|release|releasedbg)
            LABEL=$(basename -- "$(dirname -- "$(dirname -- "$UWVM")")")
            ;;
    esac
fi
LABEL=$(printf '%s' "$LABEL" | tr -c 'A-Za-z0-9._-' '_')
[[ -n $LABEL ]] || LABEL=target

if [[ -z $OUTPUT_DIR ]]; then
    OUTPUT_DIR="${ROOT_DIR}/build/test/linux-qemu-matrix/${LABEL}"
elif [[ $OUTPUT_DIR != /* ]]; then
    OUTPUT_DIR="${ROOT_DIR}/${OUTPUT_DIR}"
fi

LOG_DIR="${OUTPUT_DIR}/logs"
FIXTURE_DIR="${OUTPUT_DIR}/fixtures"
FRAGMENT_DIR="${OUTPUT_DIR}/.result-fragments"
RESULTS_TSV="${OUTPUT_DIR}/results.tsv"
METADATA_TSV="${OUTPUT_DIR}/metadata.tsv"
FIXTURE_MANIFEST_TSV="${OUTPUT_DIR}/fixture-manifest.tsv"
mkdir -p -- "$LOG_DIR" "$FIXTURE_DIR" "$FRAGMENT_DIR"
find "$FRAGMENT_DIR" -maxdepth 1 -type f -name '*.tsv' -delete

SHA256_TOOL=
if command -v sha256sum >/dev/null 2>&1; then
    SHA256_TOOL=sha256sum
elif command -v shasum >/dev/null 2>&1; then
    SHA256_TOOL=shasum
elif command -v openssl >/dev/null 2>&1; then
    SHA256_TOOL=openssl
else
    die "SHA-256 tool not found (tried sha256sum, shasum, and openssl)"
fi

sha256_file()
{
    local input=$1
    local digest=
    case $SHA256_TOOL in
        sha256sum) digest=$(sha256sum -- "$input" | awk '{ print $1; exit }') ;;
        shasum) digest=$(shasum -a 256 -- "$input" | awk '{ print $1; exit }') ;;
        openssl) digest=$(openssl dgst -sha256 <"$input" | awk '{ print $NF; exit }') ;;
        *) die "internal invalid SHA-256 tool: $SHA256_TOOL" ;;
    esac
    [[ $digest =~ ^[[:xdigit:]]{64}$ ]] || die "failed to compute SHA-256 for $input"
    printf '%s' "$digest"
}

clean_field()
{
    local value=$1
    value=${value//$'\t'/ }
    value=${value//$'\r'/ }
    value=${value//$'\n'/ }
    printf '%s' "$value"
}

RUN_STARTED_UTC=$(date -u '+%Y-%m-%dT%H:%M:%SZ')

RUN_RC=0
RUN_PEAK_RSS_KIB=0
RUN_ELAPSED_MS=0
RUN_LIMIT_REASON=exit

current_time_ms()
{
    local seconds nanos
    read -r seconds nanos <<<"$(date '+%s %N')"
    if [[ $seconds =~ ^[0-9]+$ && $nanos =~ ^[0-9]+$ ]]; then
        nanos=${nanos:0:3}
        while ((${#nanos} < 3)); do
            nanos+=0
        done
        printf '%s\n' "$((10#$seconds * 1000 + 10#$nanos))"
    else
        seconds=$(date '+%s')
        printf '%s\n' "$((10#$seconds * 1000))"
    fi
}

terminate_process_tree()
{
    local pid=$1
    local isolated=$2

    if ((isolated)); then
        kill -TERM -- "-${pid}" 2>/dev/null || true
        sleep 1
        kill -KILL -- "-${pid}" 2>/dev/null || true
    else
        if command -v pkill >/dev/null 2>&1; then
            pkill -TERM -P "$pid" 2>/dev/null || true
        fi
        kill -TERM "$pid" 2>/dev/null || true
        sleep 1
        if command -v pkill >/dev/null 2>&1; then
            pkill -KILL -P "$pid" 2>/dev/null || true
        fi
        kill -KILL "$pid" 2>/dev/null || true
    fi
}

run_limited()
{
    local output_log=$1
    shift
    local start_ms now_ms pid state rss_kib=0 peak_kib=0
    local isolated=0
    local forced_rc=
    local max_rss_kib=$((MAX_RSS_MIB * 1024))

    : >"$output_log"
    {
        printf '# command:'
        printf ' %q' "$@"
        printf '\n'
    } >>"$output_log"

    start_ms=$(current_time_ms)
    if command -v setsid >/dev/null 2>&1; then
        # Keep a shell as the session leader so a target trap signal becomes a
        # normal numeric exit status. Its diagnostic stays in the case log
        # instead of leaking from this script's job-control reporting.
        setsid bash -c '
            "$@" &
            child=$!
            if wait "$child"; then
                exit 0
            else
                rc=$?
                exit "$rc"
            fi
        ' uwvm-limited-runner "$@" >>"$output_log" 2>&1 &
        pid=$!
        isolated=1
    else
        "$@" >>"$output_log" 2>&1 &
        pid=$!
    fi

    while :; do
        state=$(ps -o stat= -p "$pid" 2>/dev/null || true)
        if [[ -z $state || $state == Z* ]]; then
            break
        fi

        if ((isolated)); then
            rss_kib=$(ps -o rss= --sid "$pid" 2>/dev/null | awk '{ total += $1 } END { print total + 0 }' || true)
        else
            rss_kib=$(ps -o rss= -p "$pid" 2>/dev/null | awk '{ total += $1 } END { print total + 0 }' || true)
        fi
        rss_kib=${rss_kib:-0}
        if ((rss_kib > peak_kib)); then
            peak_kib=$rss_kib
        fi

        now_ms=$(current_time_ms)
        if ((max_rss_kib != 0 && rss_kib > max_rss_kib)); then
            forced_rc=125
            RUN_LIMIT_REASON=rss-limit
            terminate_process_tree "$pid" "$isolated"
            break
        fi
        if ((now_ms - start_ms >= TIMEOUT_SECONDS * 1000)); then
            forced_rc=124
            RUN_LIMIT_REASON=timeout
            terminate_process_tree "$pid" "$isolated"
            break
        fi
        sleep 0.25
    done

    if wait "$pid"; then
        RUN_RC=0
    else
        RUN_RC=$?
    fi
    if [[ -n $forced_rc ]]; then
        RUN_RC=$forced_rc
    else
        RUN_LIMIT_REASON=exit
    fi
    now_ms=$(current_time_ms)
    RUN_ELAPSED_MS=$((now_ms - start_ms))
    RUN_PEAK_RSS_KIB=$peak_kib
    return 0
}

strip_ansi_file()
{
    local input=$1
    local output=$2
    LC_ALL=C sed $'s/\033\\[[0-9;?]*[ -\\/]*[@-~]//g' "$input" >"$output"
}

line_value()
{
    local key=$1
    local input=$2
    awk -v key="$key" '
        {
            prefix = key ":"
            position = index($0, prefix)
            if (position != 0) {
                line = substr($0, position + length(prefix))
                sub(/^[[:space:]]*/, "", line)
                print line
                exit
            }
        }
    ' "$input"
}

probe_target()
{
    local name=$1
    shift
    local log="${LOG_DIR}/probe-${name}.log"
    run_limited "$log" "${RUN_PREFIX[@]+"${RUN_PREFIX[@]}"}" "$UWVM" "$@"
    if ((RUN_RC != 0)); then
        die "target probe ${name} failed (rc=${RUN_RC}, reason=${RUN_LIMIT_REASON}, log=${log})"
    fi
    strip_ansi_file "$log" "${LOG_DIR}/probe-${name}.txt"
}

printf '[qemu-matrix] target=%s label=%s jobs=%s max_rss_mib=%s timeout=%ss\n' \
    "$UWVM" "$LABEL" "$JOBS" "$MAX_RSS_MIB" "$TIMEOUT_SECONDS"

probe_target version --version
probe_target runtime-help --help runtime
probe_target wasm-help --help wasm
run_limited "${LOG_DIR}/probe-wat2wasm.log" "$WAT2WASM_BIN" --version
((RUN_RC == 0)) || die "wat2wasm --version failed (log=${LOG_DIR}/probe-wat2wasm.log)"

VERSION_TEXT="${LOG_DIR}/probe-version.txt"
RUNTIME_HELP_TEXT="${LOG_DIR}/probe-runtime-help.txt"
WASM_HELP_TEXT="${LOG_DIR}/probe-wasm-help.txt"
VERSION=$(line_value Version "$VERSION_TEXT")
ARCHITECTURE=$(line_value Architecture "$VERSION_TEXT")
TARGET_OS=$(line_value OS "$VERSION_TEXT")
C_LIBRARY=$(line_value 'C Library' "$VERSION_TEXT")
MEMORY_MODEL=$(line_value 'WASM Memory Model' "$VERSION_TEXT")
RUNTIME_COMPILER=$(line_value 'Runtime Compiler' "$VERSION_TEXT")
LLVM_VERSION=$(line_value 'LLVM Version' "$VERSION_TEXT")
CALL_STACK_MODES=$(line_value 'Call Stack Modes Support' "$VERSION_TEXT")
WAT2WASM_VERSION=$(tail -n +2 "${LOG_DIR}/probe-wat2wasm.log" | tr '\t\r\n' '   ')

VERSION=${VERSION:-unknown}
ARCHITECTURE=${ARCHITECTURE:-unknown}
TARGET_OS=${TARGET_OS:-unknown}
C_LIBRARY=${C_LIBRARY:-unknown}
MEMORY_MODEL=${MEMORY_MODEL:-unknown}
RUNTIME_COMPILER=${RUNTIME_COMPILER:-unknown}
LLVM_VERSION=${LLVM_VERSION:-unknown}
CALL_STACK_MODES=${CALL_STACK_MODES:-none}
WAT2WASM_VERSION=${WAT2WASM_VERSION:-unknown}

RUNNER_SHA256=$(sha256_file "$RUNNER_PATH")
UWVM_SHA256=$(sha256_file "$UWVM")
WAT2WASM_SHA256=$(sha256_file "$WAT2WASM_BIN")
printf -v WAT2WASM_COMMAND_TEMPLATE '%q %%INPUT_WAT%% -o %%OUTPUT_WASM%%' "$WAT2WASM_BIN"

CAP_INT=0
CAP_FULL=0
CAP_AOT=0
CAP_LAZY=0
CAP_TIERED=0
CAP_CUSTOM_MODE=0
CAP_CUSTOM_COMPILER=0
CAP_CUSTOM_SELECTORS=0
CAP_TIERED_NO_T0=0
CAP_TIERED_NO_T2=0
CAP_CALL_STACK=0
CAP_INSTRUCTION=0
CAP_AUTO=0
CAP_UNWIND=0
CAP_UNWIND_UNCHECK=0
CAP_WASM2=0

option_advertised()
{
    local requested=$1
    grep -Eq "(^|[^[:alnum:]_-])${requested}([^[:alnum:]_-]|$)" "$RUNTIME_HELP_TEXT"
}

if option_advertised --runtime-int; then CAP_INT=1; fi
if option_advertised --runtime-aot; then CAP_AOT=1; fi
if option_advertised --runtime-jit; then CAP_LAZY=1; fi
if option_advertised --runtime-tiered; then CAP_TIERED=1; fi
if option_advertised --runtime-custom-mode; then CAP_CUSTOM_MODE=1; fi
if option_advertised --runtime-custom-compiler; then CAP_CUSTOM_COMPILER=1; fi
if ((CAP_CUSTOM_MODE && CAP_CUSTOM_COMPILER)); then CAP_CUSTOM_SELECTORS=1; fi
if [[ $RUNTIME_COMPILER == *LLVM-JIT* ]] && ((CAP_CUSTOM_SELECTORS)); then CAP_FULL=1; fi
if option_advertised --runtime-tiered-disable-uwvm-int-lazy-interpreter; then CAP_TIERED_NO_T0=1; fi
if option_advertised --runtime-tiered-disable-llvm-full-jit; then CAP_TIERED_NO_T2=1; fi
if option_advertised --runtime-llvm-jit-call-stack; then CAP_CALL_STACK=1; fi
CALL_STACK_USAGE=$(awk '
    /--runtime-llvm-jit-call-stack/ { in_call_stack = 1 }
    in_call_stack && /Usage:/ { print; exit }
' "$RUNTIME_HELP_TEXT")

call_stack_mode_advertised()
{
    local requested=$1
    printf '%s\n' "$CALL_STACK_MODES" |
        grep -Eq "(^|[[:space:],])${requested}([[:space:],()]|$)"
}

if ((CAP_CALL_STACK)) && [[ $CALL_STACK_USAGE == *instruction* ]] && call_stack_mode_advertised instruction; then CAP_INSTRUCTION=1; fi
if ((CAP_CALL_STACK)) && [[ $CALL_STACK_USAGE == *auto* ]]; then CAP_AUTO=1; fi
if ((CAP_CALL_STACK)) && [[ $CALL_STACK_USAGE =~ (^|[^[:alnum:]_-])unwind([^[:alnum:]_-]|$) ]] && \
    call_stack_mode_advertised unwind; then CAP_UNWIND=1; fi
if ((CAP_CALL_STACK)) && [[ $CALL_STACK_USAGE == *unwind-uncheck* ]] && \
    call_stack_mode_advertised unwind-uncheck; then CAP_UNWIND_UNCHECK=1; fi
if grep -q -- '--wasm-feature-wasm2' "$WASM_HELP_TEXT"; then CAP_WASM2=1; fi

FULL_PROFILE_CAPABLE=0
ROS_PROFILE_CAPABLE=0
if [[ $RUNTIME_COMPILER == *LLVM-JIT* ]] && \
    ((CAP_CUSTOM_SELECTORS && CAP_INT && CAP_AOT && CAP_LAZY && CAP_TIERED)); then
    FULL_PROFILE_CAPABLE=1
fi
if [[ $RUNTIME_COMPILER == *LLVM-AOT* && $RUNTIME_COMPILER != *LLVM-JIT* ]] && \
    ((CAP_INT && CAP_AOT && !CAP_CUSTOM_MODE && !CAP_CUSTOM_COMPILER && !CAP_LAZY && !CAP_TIERED &&
      !CAP_TIERED_NO_T0 && !CAP_TIERED_NO_T2)); then
    ROS_PROFILE_CAPABLE=1
fi

PROFILE_RESOLVED=legacy
PROFILE_SOURCE=capability-auto-legacy
case $PROFILE_REQUESTED in
    auto)
        if ((FULL_PROFILE_CAPABLE)); then
            PROFILE_RESOLVED=full
            PROFILE_SOURCE=capability-auto-full
        elif ((ROS_PROFILE_CAPABLE)); then
            PROFILE_RESOLVED=ros
            PROFILE_SOURCE=capability-auto-ros
        fi
        ;;
    full)
        ((FULL_PROFILE_CAPABLE)) || die "--profile full does not match the advertised complete Full runtime surface"
        PROFILE_RESOLVED=full
        PROFILE_SOURCE=explicit-full
        ;;
    ros)
        ((ROS_PROFILE_CAPABLE)) || die "--profile ros does not match the advertised reduced ROS runtime surface"
        PROFILE_RESOLVED=ros
        PROFILE_SOURCE=explicit-ros
        ;;
esac

CAPABILITIES="int=${CAP_INT},aot=${CAP_AOT},full=${CAP_FULL},lazy=${CAP_LAZY},tiered=${CAP_TIERED},tiered_no_t0=${CAP_TIERED_NO_T0},tiered_no_t2=${CAP_TIERED_NO_T2},custom_mode=${CAP_CUSTOM_MODE},custom_compiler=${CAP_CUSTOM_COMPILER},custom_selectors=${CAP_CUSTOM_SELECTORS},instruction=${CAP_INSTRUCTION},auto=${CAP_AUTO},unwind=${CAP_UNWIND},unwind_uncheck=${CAP_UNWIND_UNCHECK},wasm2=${CAP_WASM2}"

ALL_MODE_NAMES=(int-full int-lazy llvm-full llvm-lazy tiered int aot full lazy tiered-no-t0 tiered-no-t2 tiered-no-t0-no-t2)

mode_is_canonical()
{
    case $1 in
        int-full|int-lazy|llvm-full|llvm-lazy|tiered) return 0 ;;
        *) return 1 ;;
    esac
}

mode_is_profile_na()
{
    [[ $PROFILE_RESOLVED == ros ]] || return 1
    case $1 in
        int-lazy|llvm-lazy|tiered|full|lazy|tiered-no-t0|tiered-no-t2|tiered-no-t0-no-t2) return 0 ;;
        *) return 1 ;;
    esac
}

mode_supported()
{
    case $1 in
        int-full)
            if [[ $PROFILE_RESOLVED == ros ]]; then
                ((CAP_INT))
            else
                ((CAP_INT && CAP_CUSTOM_SELECTORS))
            fi
            ;;
        int-lazy) [[ $PROFILE_RESOLVED != ros ]] && ((CAP_INT && CAP_CUSTOM_SELECTORS)) ;;
        llvm-full)
            if [[ $PROFILE_RESOLVED == ros ]]; then ((CAP_AOT)); else ((CAP_FULL || CAP_AOT)); fi
            ;;
        llvm-lazy) [[ $PROFILE_RESOLVED != ros ]] && ((CAP_LAZY)) ;;
        int) ((CAP_INT)) ;;
        aot) ((CAP_AOT)) ;;
        full) ((CAP_FULL)) ;;
        lazy) ((CAP_LAZY)) ;;
        tiered) ((CAP_TIERED)) ;;
        tiered-no-t0) ((CAP_TIERED && CAP_TIERED_NO_T0)) ;;
        tiered-no-t2) ((CAP_TIERED && CAP_TIERED_NO_T2)) ;;
        tiered-no-t0-no-t2) ((CAP_TIERED && CAP_TIERED_NO_T0 && CAP_TIERED_NO_T2)) ;;
        *) return 1 ;;
    esac
}

join_csv()
{
    local joined=
    local value
    for value in "$@"; do
        if [[ -n $joined ]]; then joined+=,; fi
        joined+=$value
    done
    printf '%s' "$joined"
}

MODE_NAMES=()
ADVERTISED_MODE_NAMES=()
AUTO_SKIPPED_MODE_NAMES=()
PROFILE_NA_MODE_NAMES=()
for mode in "${ALL_MODE_NAMES[@]}"; do
    if mode_supported "$mode"; then
        ADVERTISED_MODE_NAMES+=("$mode")
    else
        AUTO_SKIPPED_MODE_NAMES+=("$mode")
    fi
    if mode_is_profile_na "$mode"; then PROFILE_NA_MODE_NAMES+=("$mode"); fi
done

MODE_SELECTION=explicit
if [[ ${MODE_CSV//[[:space:]]/} == auto ]]; then
    MODE_SELECTION=auto
    case $PROFILE_RESOLVED in
        full) MODE_NAMES=(int-full int-lazy llvm-full llvm-lazy tiered) ;;
        ros) MODE_NAMES=(int-full llvm-full) ;;
        legacy)
            if ((CAP_INT)); then MODE_NAMES+=(int); fi
            if ((CAP_FULL)); then
                MODE_NAMES+=(full)
            elif ((CAP_AOT)); then
                MODE_NAMES+=(aot)
            fi
            if ((CAP_LAZY)); then MODE_NAMES+=(lazy); fi
            if ((CAP_TIERED)); then MODE_NAMES+=(tiered); fi
            if ((CAP_TIERED && CAP_TIERED_NO_T0)); then MODE_NAMES+=(tiered-no-t0); fi
            if ((CAP_TIERED && CAP_TIERED_NO_T2)); then MODE_NAMES+=(tiered-no-t2); fi
            if ((CAP_TIERED && CAP_TIERED_NO_T0 && CAP_TIERED_NO_T2)); then MODE_NAMES+=(tiered-no-t0-no-t2); fi
            ;;
    esac
else
    IFS=',' read -r -a MODE_NAMES <<<"$MODE_CSV"
    ((${#MODE_NAMES[@]} != 0)) || die "--modes must not be empty"
    NORMALIZED_MODE_NAMES=()
    for index in "${!MODE_NAMES[@]}"; do
        mode=${MODE_NAMES[$index]//[[:space:]]/}
        case $mode in
            int-full|int-lazy|llvm-full|llvm-lazy|int|aot|full|lazy|tiered|tiered-no-t0|tiered-no-t2|tiered-no-t0-no-t2) ;;
            *) die "unsupported runtime mode: ${mode:-<empty>}" ;;
        esac
        for existing_mode in "${NORMALIZED_MODE_NAMES[@]+"${NORMALIZED_MODE_NAMES[@]}"}"; do
            [[ $existing_mode != "$mode" ]] || die "duplicate runtime mode: ${mode}"
        done
        NORMALIZED_MODE_NAMES+=("$mode")
    done
    MODE_NAMES=("${NORMALIZED_MODE_NAMES[@]}")
fi

CANONICAL_MODE_SELECTED=0
for mode in "${MODE_NAMES[@]+"${MODE_NAMES[@]}"}"; do
    if mode_is_canonical "$mode"; then CANONICAL_MODE_SELECTED=1; fi
done
if ((CANONICAL_MODE_SELECTED)); then
    for extra_arg in "${EXTRA_UWVM_ARGS[@]+"${EXTRA_UWVM_ARGS[@]}"}"; do
        case $extra_arg in
            -Rint|-Raot|-Rjit|-Rtiered|-Rcm|-Rcc|-Rtiered-disable-t0|-Rtiered-disable-t2|\
            --runtime-int|--runtime-aot|--runtime-jit|--runtime-tiered|--runtime-custom-mode|\
            --runtime-custom-compiler|--runtime-tiered-disable-uwvm-int-lazy-interpreter|\
            --runtime-tiered-disable-llvm-full-jit|--runtime-debug-int|\
            --runtime-custom-mode=*|--runtime-custom-compiler=*)
                die "canonical modes reject a conflicting --uwvm-arg runtime selector: $extra_arg"
                ;;
        esac
    done
fi

ADVERTISED_MODES=$(join_csv "${ADVERTISED_MODE_NAMES[@]+"${ADVERTISED_MODE_NAMES[@]}"}")
SELECTED_MODES=$(join_csv "${MODE_NAMES[@]+"${MODE_NAMES[@]}"}")
AUTO_SKIPPED_MODES=$(join_csv "${AUTO_SKIPPED_MODE_NAMES[@]+"${AUTO_SKIPPED_MODE_NAMES[@]}"}")
PROFILE_NA_MODES=$(join_csv "${PROFILE_NA_MODE_NAMES[@]+"${PROFILE_NA_MODE_NAMES[@]}"}")
ADVERTISED_MODES=${ADVERTISED_MODES:-none}
SELECTED_MODES=${SELECTED_MODES:-none}
AUTO_SKIPPED_MODES=${AUTO_SKIPPED_MODES:-none}
PROFILE_NA_MODES=${PROFILE_NA_MODES:-none}
WRAPPER_DISPLAY='<none>'
EXTRA_ARGS_DISPLAY='<none>'
if ((${#RUN_PREFIX[@]} != 0)); then printf -v WRAPPER_DISPLAY '%q ' "${RUN_PREFIX[@]}"; fi
if ((${#EXTRA_UWVM_ARGS[@]} != 0)); then printf -v EXTRA_ARGS_DISPLAY '%q ' "${EXTRA_UWVM_ARGS[@]}"; fi

{
    printf 'key\tvalue\n'
    printf 'label\t%s\n' "$LABEL"
    printf 'run_started_utc\t%s\n' "$RUN_STARTED_UTC"
    printf 'runner\t%s\n' "$RUNNER_PATH"
    printf 'runner_sha256\t%s\n' "$RUNNER_SHA256"
    printf 'uwvm\t%s\n' "$UWVM"
    printf 'uwvm_sha256\t%s\n' "$UWVM_SHA256"
    printf 'wrapper\t%s\n' "$WRAPPER_DISPLAY"
    printf 'extra_uwvm_args\t%s\n' "$EXTRA_ARGS_DISPLAY"
    printf 'profile_requested\t%s\n' "$PROFILE_REQUESTED"
    printf 'profile_resolved\t%s\n' "$PROFILE_RESOLVED"
    printf 'profile_source\t%s\n' "$PROFILE_SOURCE"
    printf 'version\t%s\n' "$VERSION"
    printf 'architecture\t%s\n' "$ARCHITECTURE"
    printf 'os\t%s\n' "$TARGET_OS"
    printf 'c_library\t%s\n' "$C_LIBRARY"
    printf 'wasm_memory_model\t%s\n' "$MEMORY_MODEL"
    printf 'runtime_compiler\t%s\n' "$RUNTIME_COMPILER"
    printf 'llvm_version\t%s\n' "$LLVM_VERSION"
    printf 'call_stack_modes\t%s\n' "$CALL_STACK_MODES"
    printf 'capabilities\t%s\n' "$CAPABILITIES"
    printf 'mode_selection\t%s\n' "$MODE_SELECTION"
    printf 'requested_modes\t%s\n' "$MODE_CSV"
    printf 'advertised_modes\t%s\n' "$ADVERTISED_MODES"
    printf 'selected_modes\t%s\n' "$SELECTED_MODES"
    printf 'auto_skipped_unadvertised_modes\t%s\n' "$AUTO_SKIPPED_MODES"
    printf 'profile_n_a_modes\t%s\n' "$PROFILE_NA_MODES"
    printf 'wat2wasm\t%s\n' "$WAT2WASM_BIN"
    printf 'wat2wasm_sha256\t%s\n' "$WAT2WASM_SHA256"
    printf 'wat2wasm_version\t%s\n' "$WAT2WASM_VERSION"
    printf 'wat2wasm_command_template\t%s\n' "$WAT2WASM_COMMAND_TEMPLATE"
    printf 'sha256_tool\t%s\n' "$SHA256_TOOL"
    printf 'jobs\t%s\n' "$JOBS"
    printf 'compile_threads\t%s\n' "$COMPILE_THREADS"
    printf 'timeout_seconds\t%s\n' "$TIMEOUT_SECONDS"
    printf 'max_rss_mib\t%s\n' "$MAX_RSS_MIB"
    printf 'memory_budget_mib\t%s\n' "$MEMORY_BUDGET_MIB"
} >"$METADATA_TSV"

printf '[qemu-matrix] arch=%s libc=%s memory_model=%s capabilities=%s\n' \
    "$ARCHITECTURE" "$C_LIBRARY" "$MEMORY_MODEL" "$CAPABILITIES"

case $EXPECT_MEMORY_MODEL in
    '') ;;
    mmap) EXPECT_MEMORY_MODEL='Memory Map' ;;
    multi-thread-alloc) EXPECT_MEMORY_MODEL='Multi-threaded Memory Allocator' ;;
    single-thread-alloc) EXPECT_MEMORY_MODEL='Single-threaded Memory Allocator' ;;
esac

IFS=',' read -r -a POLICIES <<<"$POLICY_CSV"
((${#POLICIES[@]} != 0)) || die "--policies must not be empty"
for index in "${!POLICIES[@]}"; do
    POLICIES[$index]=${POLICIES[$index]//[[:space:]]/}
    case ${POLICIES[$index]} in
        logical|instruction|auto|unwind|unwind-uncheck) ;;
        *) die "unsupported call-stack policy: ${POLICIES[$index]}" ;;
    esac
done

SUPPORTED_POLICY_NAMES=()
if ((CAP_INT)); then SUPPORTED_POLICY_NAMES+=(logical); fi
if ((CAP_INSTRUCTION)); then SUPPORTED_POLICY_NAMES+=(instruction); fi
if ((CAP_AUTO)); then SUPPORTED_POLICY_NAMES+=(auto); fi
if ((CAP_UNWIND)); then SUPPORTED_POLICY_NAMES+=(unwind); fi
if ((CAP_UNWIND_UNCHECK)); then SUPPORTED_POLICY_NAMES+=(unwind-uncheck); fi
SUPPORTED_POLICIES=$(join_csv "${SUPPORTED_POLICY_NAMES[@]+"${SUPPORTED_POLICY_NAMES[@]}"}")
SUPPORTED_POLICIES=${SUPPORTED_POLICIES:-none}
printf 'requested_policies\t%s\n' "$(join_csv "${POLICIES[@]}")" >>"$METADATA_TSV"
printf 'supported_policies\t%s\n' "$SUPPORTED_POLICIES" >>"$METADATA_TSV"

AUTO_REQUESTED=0
UNWIND_UNCHECK_REQUESTED=0
for policy in "${POLICIES[@]}"; do
    if [[ $policy == auto ]]; then AUTO_REQUESTED=1; fi
    if [[ $policy == unwind-uncheck ]]; then UNWIND_UNCHECK_REQUESTED=1; fi
done

FIXTURE_NAMES=(
    mvp_smoke
    wasm2_bulk_memory
    wasm2_multiple_tables
    wasm2_multivalue
    oob_load
    float_to_int
    wasm2_bulk_oob
    wasm2_table_oob
    tiered_osr_oob
    tiered_full_ready_oob
)
FIXTURE_CLASSES=(normal normal normal normal trap trap trap trap trap trap)
FIXTURE_FEATURES=(core wasm2 wasm2 wasm2 core core wasm2 wasm2 core core)
FIXTURE_EXPECTED=(
    ok
    ok
    ok
    ok
    'memory access out of bounds'
    'invalid conversion to integer'
    'memory access out of bounds'
    'table access out of bounds'
    'memory access out of bounds'
    'memory access out of bounds'
)
FIXTURE_STACKS=(- - - - '0,1,2,3' '0,1,2,3' '0,1,2' '0,1,2' '0,1,2' '0,1')

TIERED_OSR_TEMPLATE="${ROOT_DIR}/test/0014.llvm_jit/wat/tiered_osr_direct_trap.wat"
TIERED_OSR_GENERATED="${FIXTURE_DIR}/tiered_osr_direct_trap.padded.wat"
[[ -f $TIERED_OSR_TEMPLATE ]] || die "fixture source missing: $TIERED_OSR_TEMPLATE"
awk '
    /UWVM_TIERED_OSR_PAD_NOPS/ {
        for (i = 0; i != 1600; ++i) print "    nop"
        next
    }
    { print }
' "$TIERED_OSR_TEMPLATE" >"$TIERED_OSR_GENERATED"

FIXTURE_SOURCES=(
    "${ROOT_DIR}/test/0014.llvm_jit/nontrivial_start.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/wasm2_bulk_memory_native.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/wasm2_multiple_tables.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/wasm2_multivalue_typed.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/trap_matrix/oob_load.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/trap_matrix/invalid_conversion.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/trap_matrix/wasm2_memory_copy_oob.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/trap_matrix/wasm2_table_copy_oob.wat"
    "$TIERED_OSR_GENERATED"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/tiered_full_ready_oob.wat"
)
FIXTURE_ORIGIN_SOURCES=(
    "${ROOT_DIR}/test/0014.llvm_jit/nontrivial_start.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/wasm2_bulk_memory_native.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/wasm2_multiple_tables.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/wasm2_multivalue_typed.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/trap_matrix/oob_load.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/trap_matrix/invalid_conversion.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/trap_matrix/wasm2_memory_copy_oob.wat"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/trap_matrix/wasm2_table_copy_oob.wat"
    "$TIERED_OSR_TEMPLATE"
    "${ROOT_DIR}/test/0014.llvm_jit/wat/tiered_full_ready_oob.wat"
)
FIXTURE_MODE_SCOPES=(all all all all all all all all tiered-all tiered-t2)
FIXTURE_WASMS=()

printf 'fixture\tinput_wat\tinput_wat_sha256\torigin_wat\torigin_wat_sha256\twasm\twasm_sha256\twat2wasm\twat2wasm_sha256\tcommand\tcompile_log\n' \
    >"$FIXTURE_MANIFEST_TSV"

for index in "${!FIXTURE_NAMES[@]}"; do
    source_wat=${FIXTURE_SOURCES[$index]}
    origin_wat=${FIXTURE_ORIGIN_SOURCES[$index]}
    output_wasm="${FIXTURE_DIR}/${FIXTURE_NAMES[$index]}.wasm"
    compile_log="${LOG_DIR}/wat2wasm-${FIXTURE_NAMES[$index]}.log"
    [[ -f $source_wat ]] || die "fixture source missing: $source_wat"
    [[ -f $origin_wat ]] || die "fixture origin source missing: $origin_wat"
    output_wasm_tmp=$(mktemp "${output_wasm}.tmp.XXXXXX") || die "cannot create temporary fixture output for ${FIXTURE_NAMES[$index]}"
    run_limited "$compile_log" "$WAT2WASM_BIN" "$source_wat" -o "$output_wasm_tmp"
    if ((RUN_RC != 0)); then
        rm -f -- "$output_wasm_tmp"
        die "wat2wasm failed for ${FIXTURE_NAMES[$index]} (rc=${RUN_RC}, log=${compile_log})"
    fi
    mv -f -- "$output_wasm_tmp" "$output_wasm"

    source_wat_sha256=$(sha256_file "$source_wat")
    origin_wat_sha256=$(sha256_file "$origin_wat")
    output_wasm_sha256=$(sha256_file "$output_wasm")
    printf -v fixture_command '%q %q -o %q' "$WAT2WASM_BIN" "$source_wat" "$output_wasm"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$(clean_field "${FIXTURE_NAMES[$index]}")" \
        "$(clean_field "$source_wat")" \
        "$source_wat_sha256" \
        "$(clean_field "$origin_wat")" \
        "$origin_wat_sha256" \
        "$(clean_field "$output_wasm")" \
        "$output_wasm_sha256" \
        "$(clean_field "$WAT2WASM_BIN")" \
        "$WAT2WASM_SHA256" \
        "$(clean_field "$fixture_command")" \
        "$(clean_field "$compile_log")" >>"$FIXTURE_MANIFEST_TSV"
    FIXTURE_WASMS+=("$output_wasm")
done

FIXTURE_MANIFEST_SHA256=$(sha256_file "$FIXTURE_MANIFEST_TSV")
printf 'fixture_manifest\t%s\nfixture_manifest_sha256\t%s\n' \
    "$FIXTURE_MANIFEST_TSV" "$FIXTURE_MANIFEST_SHA256" >>"$METADATA_TSV"

fixture_applicable_to_mode()
{
    local fixture_index=$1
    local mode=$2
    case ${FIXTURE_MODE_SCOPES[$fixture_index]} in
        all) return 0 ;;
        tiered-all)
            case $mode in tiered|tiered-no-t0|tiered-no-t2|tiered-no-t0-no-t2) return 0 ;; esac
            ;;
        tiered-t2) [[ $mode == tiered ]] && return 0 ;;
    esac
    return 1
}

policy_supported()
{
    case $1 in
        logical) ((CAP_INT)) ;;
        instruction) ((CAP_INSTRUCTION)) ;;
        auto) ((CAP_AUTO)) ;;
        unwind) ((CAP_UNWIND)) ;;
        unwind-uncheck) ((CAP_UNWIND_UNCHECK)) ;;
        *) return 1 ;;
    esac
}

mode_uses_llvm()
{
    case $1 in
        llvm-full|llvm-lazy|aot|full|lazy|tiered|tiered-no-t0|tiered-no-t2|tiered-no-t0-no-t2) return 0 ;;
        *) return 1 ;;
    esac
}

mode_uses_interpreter()
{
    case $1 in
        int|int-full|int-lazy) return 0 ;;
        *) return 1 ;;
    esac
}

policy_applicable_to_mode()
{
    local mode=$1
    local policy=$2
    if mode_uses_interpreter "$mode"; then
        [[ $policy == logical ]]
    elif mode_uses_llvm "$mode"; then
        [[ $policy != logical ]]
    else
        return 1
    fi
}

set_mode_args()
{
    MODE_ARGS=()
    case $1 in
        int-full)
            if [[ $PROFILE_RESOLVED == ros ]]; then
                MODE_ARGS=(-Rint)
            else
                MODE_ARGS=(-Rcm full -Rcc int)
            fi
            ;;
        int-lazy) MODE_ARGS=(-Rcm lazy -Rcc int) ;;
        llvm-full)
            if [[ $PROFILE_RESOLVED == ros ]] || ((CAP_FULL == 0)); then
                MODE_ARGS=(-Raot)
            else
                MODE_ARGS=(-Rcm full -Rcc jit)
            fi
            ;;
        llvm-lazy)
            if ((CAP_CUSTOM_SELECTORS)); then
                MODE_ARGS=(-Rcm lazy -Rcc jit)
            else
                MODE_ARGS=(-Rjit)
            fi
            ;;
        int) MODE_ARGS=(-Rint) ;;
        aot) MODE_ARGS=(-Raot) ;;
        full) MODE_ARGS=(-Rcm full -Rcc jit) ;;
        lazy) MODE_ARGS=(-Rjit) ;;
        tiered) MODE_ARGS=(-Rtiered) ;;
        tiered-no-t0) MODE_ARGS=(-Rtiered -Rtiered-disable-t0) ;;
        tiered-no-t2) MODE_ARGS=(-Rtiered -Rtiered-disable-t2) ;;
        tiered-no-t0-no-t2) MODE_ARGS=(-Rtiered -Rtiered-disable-t0 -Rtiered-disable-t2) ;;
        *) return 1 ;;
    esac
}

mode_argument_origin()
{
    case $1 in
        int-full)
            if [[ $PROFILE_RESOLVED == ros ]]; then
                printf canonical-ros-alias
            else
                printf canonical-explicit
            fi
            ;;
        llvm-full)
            if [[ $PROFILE_RESOLVED == ros ]]; then
                printf canonical-ros-alias
            elif ((CAP_FULL)); then
                printf canonical-explicit
            else
                printf canonical-legacy-aot-alias
            fi
            ;;
        int-lazy) printf canonical-explicit ;;
        llvm-lazy)
            if ((CAP_CUSTOM_SELECTORS)); then printf canonical-explicit; else printf canonical-legacy-jit-alias; fi
            ;;
        tiered) printf canonical-retained-alias ;;
        int|aot|full|lazy) printf legacy-alias ;;
        tiered-no-t0|tiered-no-t2|tiered-no-t0-no-t2) printf explicit-diagnostic-alias ;;
        *) printf unknown ;;
    esac
}

SELECTED_MODE_ARGUMENTS=
for selected_mode in "${MODE_NAMES[@]+"${MODE_NAMES[@]}"}"; do
    if [[ -n $SELECTED_MODE_ARGUMENTS ]]; then SELECTED_MODE_ARGUMENTS+=';'; fi
    if mode_is_profile_na "$selected_mode"; then
        SELECTED_MODE_ARGUMENTS+="${selected_mode}=N-A(profile-ros)"
    elif set_mode_args "$selected_mode"; then
        printf -v selected_args_display '%q ' "${MODE_ARGS[@]}"
        selected_args_display=${selected_args_display% }
        SELECTED_MODE_ARGUMENTS+="${selected_mode}=${selected_args_display}[$(mode_argument_origin "$selected_mode")]"
    else
        SELECTED_MODE_ARGUMENTS+="${selected_mode}=unavailable"
    fi
done
printf 'selected_mode_arguments\t%s\n' "${SELECTED_MODE_ARGUMENTS:-none}" >>"$METADATA_TSV"

write_result()
{
    local id=$1
    local mode=$2
    local policy=$3
    local actual_policy=$4
    local fixture=$5
    local fixture_class=$6
    local expected=$7
    local exit_code=$8
    local outcome=$9
    shift 9
    local detail=$1
    local peak_rss_kib=$2
    local elapsed_ms=$3
    local output_log=$4
    local compiler_log=$5
    local fragment
    fragment=$(printf '%s/%06d.tsv' "$FRAGMENT_DIR" "$id")

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$id" \
        "$(clean_field "$LABEL")" \
        "$(clean_field "$VERSION")" \
        "$(clean_field "$ARCHITECTURE")" \
        "$(clean_field "$C_LIBRARY")" \
        "$(clean_field "$MEMORY_MODEL")" \
        "$(clean_field "$CAPABILITIES")" \
        "$(clean_field "$mode")" \
        "$(clean_field "$policy")" \
        "$(clean_field "$actual_policy")" \
        "$(clean_field "$fixture")" \
        "$(clean_field "$fixture_class")" \
        "$(clean_field "$expected")" \
        "$(clean_field "$exit_code")" \
        "$(clean_field "$outcome")" \
        "$(clean_field "$detail")" \
        "$peak_rss_kib" \
        "$elapsed_ms" \
        "$(clean_field "$output_log")" \
        "$(clean_field "$compiler_log")" >"$fragment"
}

next_id=0
allocate_id()
{
    next_id=$((next_id + 1))
    ALLOCATED_ID=$next_id
}

unsupported_outcome()
{
    if ((ALLOW_UNSUPPORTED)); then
        printf SKIP
    else
        printf FAIL
    fi
}

HAS_SELECTED_LLVM_MODE=0
for mode in "${MODE_NAMES[@]+"${MODE_NAMES[@]}"}"; do
    if ! mode_supported "$mode"; then continue; fi
    if mode_uses_llvm "$mode"; then
        HAS_SELECTED_LLVM_MODE=1
    fi
done

if [[ $MODE_SELECTION == explicit ]]; then
    for mode in "${MODE_NAMES[@]}"; do
        if ! mode_supported "$mode"; then
            allocate_id
            if mode_is_profile_na "$mode"; then
                write_result "$ALLOCATED_ID" "$mode" - - capability capability N-A - N-A \
                    "runtime mode is source-pruned from the ROS product profile" 0 0 - -
            else
                write_result "$ALLOCATED_ID" "$mode" - - capability capability supported - "$(unsupported_outcome)" \
                    "requested runtime mode is not advertised" 0 0 - -
            fi
        fi
    done
elif [[ $PROFILE_RESOLVED == ros ]]; then
    for mode in int-lazy llvm-lazy tiered; do
        allocate_id
        write_result "$ALLOCATED_ID" "$mode" - - capability capability N-A - N-A \
            "runtime mode is source-pruned from the ROS product profile" 0 0 - -
    done
elif ((${#MODE_NAMES[@]} == 0)); then
    allocate_id
    write_result "$ALLOCATED_ID" - - - capability capability 'an advertised int/AOT/full/lazy/tiered mode' - FAIL \
        "auto mode selection found no runnable compiler mode" 0 0 - -
fi

for mode in "${MODE_NAMES[@]}"; do
    if ! mode_supported "$mode"; then continue; fi
    has_applicable_policy=0
    for policy in "${POLICIES[@]}"; do
        if policy_applicable_to_mode "$mode" "$policy"; then
            has_applicable_policy=1
            break
        fi
    done
    if ((has_applicable_policy == 0)); then
        allocate_id
        if mode_uses_interpreter "$mode"; then
            incompatible_policy_detail='interpreter mode requires the logical policy and never accepts an LLVM call-stack policy'
        else
            incompatible_policy_detail='LLVM runtime mode requires instruction, auto, unwind, or unwind-uncheck; logical is interpreter-only'
        fi
        write_result "$ALLOCATED_ID" "$mode" "$(join_csv "${POLICIES[@]}")" - capability capability 'an applicable policy' - \
            "$(unsupported_outcome)" "$incompatible_policy_detail" 0 0 - -
    fi
done

if ((CAP_WASM2 == 0)); then
    for index in "${!FIXTURE_NAMES[@]}"; do
        if [[ ${FIXTURE_FEATURES[$index]} == wasm2 ]]; then
            allocate_id
            write_result "$ALLOCATED_ID" - - - "${FIXTURE_NAMES[$index]}" capability wasm2 - "$(unsupported_outcome)" \
                "--wasm-feature-wasm2 is not advertised" 0 0 - -
        fi
    done
fi

for policy in "${POLICIES[@]}"; do
    if ((HAS_SELECTED_LLVM_MODE)) && [[ $policy != logical && $policy != unwind && $policy != unwind-uncheck ]] && ! policy_supported "$policy"; then
        allocate_id
        write_result "$ALLOCATED_ID" - "$policy" - capability capability supported - "$(unsupported_outcome)" \
            "call-stack policy is not advertised" 0 0 - -
    fi
done

if [[ -n $EXPECT_MEMORY_MODEL && $MEMORY_MODEL != "$EXPECT_MEMORY_MODEL" ]]; then
    allocate_id
    write_result "$ALLOCATED_ID" capability - - memory-model capability "$EXPECT_MEMORY_MODEL" - FAIL \
        "target reports ${MEMORY_MODEL}" 0 0 "${LOG_DIR}/probe-version.log" -
fi

extract_trap_kind()
{
    local output_log=$1
    local line
    line=$(LC_ALL=C sed $'s/\033\\[[0-9;?]*[ -\\/]*[@-~]//g' "$output_log" | grep -a -m1 'Runtime crash (' || true)
    if [[ $line == *'Runtime crash ('* ]]; then
        line=${line#*Runtime crash (}
        printf '%s' "${line%%)*}"
    fi
}

extract_optimize_field()
{
    local compiler_log=$1
    local field=$2
    awk -v field="$field" '
        index($0, "[llvm-jit-full] optimize-start ") != 0 {
            for (i = 1; i <= NF; ++i) {
                prefix = field "="
                if (index($i, prefix) == 1) {
                    current = substr($i, length(prefix) + 1)
                    sub(/[,:;\r]+$/, "", current)
                    if (value != "" && current != value) inconsistent = 1
                    value = current
                    found = 1
                }
            }
        }
        END {
            if (inconsistent) print "inconsistent"
            else if (found) print value
            else print "unknown"
        }
    ' "$compiler_log" 2>/dev/null
}

extract_actual_policy()
{
    extract_optimize_field "$1" call_stack
}

extract_call_stack_frames()
{
    extract_optimize_field "$1" call_stack_frames
}

policy_declarations_match()
{
    local compiler_log=$1
    local expected_policy=$2
    local expected_check=$3
    local expected_replace=$4
    local expected_frames=$5
    local expected_backend=${6:-any}
    awk \
        -v expected_policy="$expected_policy" \
        -v expected_check="$expected_check" \
        -v expected_replace="$expected_replace" \
        -v expected_frames="$expected_frames" \
        -v expected_backend="$expected_backend" '
        function value_of(token, key, value) {
            if (index(token, key "=") != 1) return ""
            value = substr(token, length(key) + 2)
            sub(/[,:;\r]+$/, "", value)
            return value
        }
        index($0, "[llvm-jit-full] optimize-start ") != 0 {
            found = 1
            policy = check = replace = frames = backend = ""
            for (i = 1; i <= NF; ++i) {
                value = value_of($i, "call_stack")
                if (value != "") policy = value
                value = value_of($i, "unwind_check")
                if (value != "") check = value
                value = value_of($i, "unwind_replace_frames")
                if (value != "") replace = value
                value = value_of($i, "call_stack_frames")
                if (value != "") frames = value
                value = value_of($i, "unwind_backend")
                if (value != "") backend = value
            }
            if (policy != expected_policy || check != expected_check ||
                (expected_replace != "any" && replace != expected_replace) || frames != expected_frames ||
                (expected_backend != "any" && backend != expected_backend)) bad = 1
        }
        END { exit !(found && !bad) }
    ' "$compiler_log" 2>/dev/null
}

has_live_unwind_declaration()
{
    local compiler_log=$1
    local backend
    policy_declarations_match "$compiler_log" unwind live yes omit || return 1
    backend=$(extract_optimize_field "$compiler_log" unwind_backend)
    [[ $backend == unwind.h || $backend == win64-seh ]]
}

has_instruction_fallback_declaration()
{
    # unwind_replace_frames describes the compiled platform capability, not
    # whether the instruction policy selected it. Frame emission is decisive.
    policy_declarations_match "$1" instruction off any emit
}

has_none_declaration()
{
    local compiler_log=$1
    [[ $(extract_actual_policy "$compiler_log") == none && $(extract_call_stack_frames "$compiler_log") == omit && \
        $(extract_optimize_field "$compiler_log" unwind_check) == off ]]
}

has_unchecked_unwind_replacement_declaration()
{
    local compiler_log=$1
    local backend
    policy_declarations_match "$compiler_log" unwind-uncheck unchecked yes omit || return 1
    backend=$(extract_optimize_field "$compiler_log" unwind_backend)
    [[ $backend == unwind.h || $backend == win64-seh ]]
}

has_expected_policy_declaration()
{
    local compiler_log=$1
    local policy=$2
    case $policy in
        instruction) has_instruction_fallback_declaration "$compiler_log" ;;
        unwind) has_live_unwind_declaration "$compiler_log" ;;
        unwind-uncheck) has_unchecked_unwind_replacement_declaration "$compiler_log" ;;
        none) has_none_declaration "$compiler_log" ;;
        *) return 1 ;;
    esac
}

regular_case_requires_llvm_log()
{
    local mode=$1
    local fixture=${2:-}
    case $fixture in
        tiered_osr_oob|tiered_full_ready_oob) return 0 ;;
    esac
    case $mode in
        llvm-full|llvm-lazy|aot|full|lazy|tiered-no-t0|tiered-no-t0-no-t2) return 0 ;;
        tiered|tiered-no-t2) return 1 ;;
        *) return 1 ;;
    esac
}

has_tiered_witness_log()
{
    local mode=$1
    local fixture=$2
    local compiler_log=$3
    local request_marker request_line request_fn request_cu
    case $fixture in
        tiered_osr_oob)
            case $mode in
                tiered|tiered-no-t2)
                    request_marker='[llvm-jit-lazy] tiered-osr-request'
                    ;;
                tiered-no-t0|tiered-no-t0-no-t2)
                    request_marker='[llvm-jit-lazy] demand-request'
                    ;;
                *) return 1 ;;
            esac
            request_line=$(grep -aF -m1 "$request_marker" "$compiler_log" 2>/dev/null || true)
            [[ $request_line == *' lane=inline'* ]] || return 1
            request_fn=$(grep -o ' fn=[0-9][0-9]*' <<<"$request_line" | head -n1 | cut -d= -f2)
            request_cu=$(grep -o ' cu=[0-9][0-9]*' <<<"$request_line" | head -n1 | cut -d= -f2)
            [[ -n $request_fn && -n $request_cu ]] || return 1
            grep -aF '[llvm-jit-lazy] compile-end' "$compiler_log" |
                grep -Eq " fn=${request_fn} cu=${request_cu} state=compiled" || return 1
            if [[ $mode == tiered-no-t0 || $mode == tiered-no-t0-no-t2 ]]; then
                ! grep -aFq '[uwvm-int-lazy] demand-request' "$compiler_log" || return 1
            fi
            if [[ $mode == tiered-no-t2 || $mode == tiered-no-t0-no-t2 ]]; then
                ! grep -aEq 'tiered-full-(request|ready)' "$compiler_log" || return 1
            fi
            if [[ $mode == tiered-no-t0-no-t2 ]]; then
                ! grep -aEq 'tiered-(demand|osr)-request' "$compiler_log" || return 1
            fi
            return 0
            ;;
        tiered_full_ready_oob)
            # These are compiler/publication events, not T2 entry execution.
            # The same final trap and logical indices are possible in T0/T1.
            grep -aFq '[llvm-jit-lazy] tiered-full-request' "$compiler_log" &&
                grep -aFq '[llvm-jit-lazy] tiered-full-ready' "$compiler_log"
            ;;
        *) return 1 ;;
    esac
}

has_expected_llvm_log()
{
    local mode=$1
    local compiler_log=$2
    local fixture=${3:-}
    case $fixture in
        tiered_osr_oob|tiered_full_ready_oob)
            has_tiered_witness_log "$mode" "$fixture" "$compiler_log"
            return
            ;;
    esac
    case $mode in
        llvm-full|aot|full) grep -aFq '[llvm-jit-full] optimize-start' "$compiler_log" ;;
        llvm-lazy|lazy|tiered-no-t0|tiered-no-t0-no-t2) grep -aFq '[llvm-jit-lazy] compile-end' "$compiler_log" ;;
        tiered|tiered-no-t2) return 0 ;;
        *) return 1 ;;
    esac
}

extract_func_indices()
{
    local output_log=$1
    local match
    local indices=
    while IFS= read -r match; do
        if [[ -n $indices ]]; then indices+=,; fi
        indices+=${match#func_idx=}
    done < <(LC_ALL=C sed $'s/\033\\[[0-9;?]*[ -\\/]*[@-~]//g' "$output_log" |
        grep -ao 'func_idx=[0-9][0-9]*' 2>/dev/null || true)
    printf '%s' "$indices"
}

source_pruned_llvm_capability_detail()
{
    local mode=$1
    local fixture=$2
    local output_log=$3
    [[ $PROFILE_RESOLVED == ros && $mode == llvm-full ]] || return 1
    case $fixture in
        wasm2_bulk_memory)
            grep -aFq 'LLVM AOT capability preflight rejected module=' "$output_log" &&
                grep -aFq 'memory.init has no LLVM lowering' "$output_log" || return 1
            printf '%s' 'ROS LLVM-AOT source-pruned capability: memory.init has no lowering'
            ;;
        wasm2_multivalue)
            grep -aFq 'LLVM AOT capability preflight rejected module=' "$output_log" &&
                grep -aFq 'function signature has multiple results' "$output_log" || return 1
            printf '%s' 'ROS LLVM-AOT source-pruned capability: multiple results have no lowering'
            ;;
        wasm2_table_oob)
            grep -aFq 'LLVM AOT capability preflight rejected module=' "$output_log" &&
                grep -aFq 'table.copy has no LLVM lowering' "$output_log" || return 1
            printf '%s' 'ROS LLVM-AOT source-pruned capability: table.copy has no lowering'
            ;;
        *) return 1 ;;
    esac
}

POLICY_PROBE_MODE=
if [[ $PROFILE_RESOLVED == full ]] && mode_supported llvm-full; then
    POLICY_PROBE_MODE=llvm-full
elif [[ $PROFILE_RESOLVED == ros ]] && mode_supported llvm-full; then
    POLICY_PROBE_MODE=llvm-full
elif ((CAP_FULL)); then
    POLICY_PROBE_MODE=full
elif ((CAP_AOT)); then
    POLICY_PROBE_MODE=aot
fi
printf 'policy_probe_mode\t%s\n' "${POLICY_PROBE_MODE:-unavailable}" >>"$METADATA_TSV"

AUTO_EFFECTIVE_POLICY=unavailable
if ((HAS_SELECTED_LLVM_MODE && AUTO_REQUESTED && CAP_AUTO && CAP_CALL_STACK)) && [[ -n $POLICY_PROBE_MODE ]]; then
    allocate_id
    auto_probe_id=$ALLOCATED_ID
    auto_probe_log="${LOG_DIR}/$(printf '%06d' "$auto_probe_id")-auto-live-probe.log"
    auto_probe_compiler_log="${LOG_DIR}/$(printf '%06d' "$auto_probe_id")-auto-live-probe.compiler.log"
    : >"$auto_probe_compiler_log"
    set_mode_args "$POLICY_PROBE_MODE" || die "internal invalid policy probe mode: $POLICY_PROBE_MODE"
    run_limited "$auto_probe_log" \
        "${RUN_PREFIX[@]+"${RUN_PREFIX[@]}"}" "$UWVM" "${MODE_ARGS[@]}" -Rct "$COMPILE_THREADS" \
        -Rllvm-cache-path disable -Rllvm-call-stack auto -Rclog file "$auto_probe_compiler_log" \
        "${EXTRA_UWVM_ARGS[@]+"${EXTRA_UWVM_ARGS[@]}"}" --run "${FIXTURE_WASMS[4]}"

    AUTO_EFFECTIVE_POLICY=$(extract_actual_policy "$auto_probe_compiler_log")
    auto_probe_frames=$(extract_call_stack_frames "$auto_probe_compiler_log")
    auto_probe_stack=$(extract_func_indices "$auto_probe_log")
    auto_probe_trap=$(extract_trap_kind "$auto_probe_log")
    auto_probe_outcome=PASS
    auto_probe_detail="auto=${AUTO_EFFECTIVE_POLICY}"
    if [[ $RUN_LIMIT_REASON != exit ]]; then
        auto_probe_outcome=FAIL
        auto_probe_detail="auto probe hit ${RUN_LIMIT_REASON}"
    elif ((RUN_RC == 0)) || [[ $auto_probe_trap != 'memory access out of bounds' ]]; then
        auto_probe_outcome=FAIL
        auto_probe_detail="auto probe did not produce the expected OOB trap"
    else
        case $AUTO_EFFECTIVE_POLICY in
            unwind)
                if [[ $auto_probe_stack != '0,1,2,3' ]]; then
                    auto_probe_outcome=FAIL
                    auto_probe_detail="auto unwind stack mismatch: ${auto_probe_stack:-missing}"
                elif ! has_live_unwind_declaration "$auto_probe_compiler_log"; then
                    auto_probe_outcome=FAIL
                    auto_probe_detail='auto unwind lacks live checked native unwind with frame replacement'
                else
                    auto_probe_detail='auto=unwind;unwind_check=live;unwind_replace_frames=yes;call_stack_frames=omit'
                fi
                ;;
            none)
                # auto must preserve a logical stack.  It may select native
                # unwind only after the authoritative live probe succeeds;
                # otherwise the runtime falls back to Instruction frames.
                # Resolving auto to none is therefore an impossible runtime
                # state, not a third valid outcome for this test matrix.
                auto_probe_outcome=FAIL
                auto_probe_detail="invalid auto resolution to none; expected instruction or checked unwind (call_stack_frames=${auto_probe_frames})"
                ;;
            instruction)
                if [[ $auto_probe_stack != '0,1,2,3' ]]; then
                    auto_probe_outcome=FAIL
                    auto_probe_detail="auto instruction logical stack mismatch: ${auto_probe_stack:-missing}"
                elif ! has_instruction_fallback_declaration "$auto_probe_compiler_log"; then
                    auto_probe_outcome=FAIL
                    auto_probe_detail='auto instruction lacks non-replacing logical-frame declaration'
                else
                    auto_probe_detail='auto=instruction;unwind_check=off;unwind_replace_frames=no;logical_frames=verified'
                fi
                ;;
            *)
                auto_probe_outcome=FAIL
                auto_probe_detail="auto effective policy was not logged: ${AUTO_EFFECTIVE_POLICY}"
                ;;
        esac
    fi
    write_result "$auto_probe_id" "$POLICY_PROBE_MODE" auto "$AUTO_EFFECTIVE_POLICY" auto-live-probe trap \
        'instruction-or-checked-unwind' "$RUN_RC" "$auto_probe_outcome" "$auto_probe_detail" \
        "$RUN_PEAK_RSS_KIB" "$RUN_ELAPSED_MS" "$auto_probe_log" "$auto_probe_compiler_log"
    if [[ $auto_probe_outcome != PASS ]]; then AUTO_EFFECTIVE_POLICY=invalid; fi
fi
printf 'auto_effective_policy\t%s\n' "$AUTO_EFFECTIVE_POLICY" >>"$METADATA_TSV"

UNWIND_UNCHECK_REPLACEMENT_STATUS=unavailable
if ((HAS_SELECTED_LLVM_MODE && UNWIND_UNCHECK_REQUESTED && CAP_UNWIND_UNCHECK && CAP_CALL_STACK)) && [[ -n $POLICY_PROBE_MODE ]]; then
    allocate_id
    uncheck_probe_id=$ALLOCATED_ID
    uncheck_probe_log="${LOG_DIR}/$(printf '%06d' "$uncheck_probe_id")-unwind-uncheck-replacement-probe.log"
    uncheck_probe_compiler_log="${LOG_DIR}/$(printf '%06d' "$uncheck_probe_id")-unwind-uncheck-replacement-probe.compiler.log"
    : >"$uncheck_probe_compiler_log"
    set_mode_args "$POLICY_PROBE_MODE" || die "internal invalid policy probe mode: $POLICY_PROBE_MODE"
    run_limited "$uncheck_probe_log" \
        "${RUN_PREFIX[@]+"${RUN_PREFIX[@]}"}" "$UWVM" "${MODE_ARGS[@]}" -Rct "$COMPILE_THREADS" \
        -Rllvm-cache-path disable -Rllvm-call-stack unwind-uncheck -Rclog file "$uncheck_probe_compiler_log" \
        "${EXTRA_UWVM_ARGS[@]+"${EXTRA_UWVM_ARGS[@]}"}" --run "${FIXTURE_WASMS[4]}"

    uncheck_probe_policy=$(extract_actual_policy "$uncheck_probe_compiler_log")
    uncheck_probe_stack=$(extract_func_indices "$uncheck_probe_log")
    uncheck_probe_trap=$(extract_trap_kind "$uncheck_probe_log")
    uncheck_probe_outcome=PASS
    uncheck_probe_detail='unwind-uncheck=unchecked-native-replacement'
    if [[ $RUN_LIMIT_REASON != exit ]]; then
        uncheck_probe_outcome=FAIL
        uncheck_probe_detail="unwind-uncheck probe hit ${RUN_LIMIT_REASON}"
    elif ((RUN_RC == 0)) || [[ $uncheck_probe_trap != 'memory access out of bounds' ]]; then
        uncheck_probe_outcome=FAIL
        uncheck_probe_detail='unwind-uncheck probe did not produce the expected OOB trap'
    elif [[ $uncheck_probe_policy != unwind-uncheck ]]; then
        uncheck_probe_outcome=FAIL
        uncheck_probe_detail="unwind-uncheck effective policy mismatch: ${uncheck_probe_policy}"
    elif ! has_unchecked_unwind_replacement_declaration "$uncheck_probe_compiler_log"; then
        uncheck_probe_outcome=FAIL
        uncheck_probe_detail='unwind-uncheck did not declare an available unchecked native replacement backend'
    elif [[ $uncheck_probe_stack != '0,1,2,3' ]]; then
        uncheck_probe_outcome=FAIL
        uncheck_probe_detail="unwind-uncheck native stack mismatch: ${uncheck_probe_stack:-missing}"
    fi
    write_result "$uncheck_probe_id" "$POLICY_PROBE_MODE" unwind-uncheck "$uncheck_probe_policy" \
        unwind-uncheck-replacement-probe trap 'unchecked-native-unwind-replacement' "$RUN_RC" \
        "$uncheck_probe_outcome" "$uncheck_probe_detail" "$RUN_PEAK_RSS_KIB" "$RUN_ELAPSED_MS" \
        "$uncheck_probe_log" "$uncheck_probe_compiler_log"
    if [[ $uncheck_probe_outcome == PASS ]]; then
        UNWIND_UNCHECK_REPLACEMENT_STATUS=verified
    else
        UNWIND_UNCHECK_REPLACEMENT_STATUS=invalid
    fi
elif ((HAS_SELECTED_LLVM_MODE && UNWIND_UNCHECK_REQUESTED && CAP_UNWIND_UNCHECK)); then
    # An advertised lazy-only target cannot satisfy this full/AOT probe.  Do
    # not silently produce an empty successful matrix when every selected
    # policy needs a native replacement verification we could not perform.
    allocate_id
    write_result "$ALLOCATED_ID" capability unwind-uncheck unavailable unwind-uncheck-replacement-probe capability \
        'an advertised AOT/full policy-probe mode' - "$(unsupported_outcome)" \
        'cannot verify unchecked native unwind replacement without an advertised AOT/full mode and call-stack option' 0 0 - -
elif ((UNWIND_UNCHECK_REQUESTED == 0)); then
    UNWIND_UNCHECK_REPLACEMENT_STATUS=not-requested
fi
printf 'unwind_uncheck_replacement_status\t%s\n' "$UNWIND_UNCHECK_REPLACEMENT_STATUS" >>"$METADATA_TSV"

TIER2_WITNESS_POLICY=
# Native unwind intentionally disables background T2 in the current runtime.
# This witness is limited to publication/readiness under logical-stack policy;
# it cannot supply T2 unwind coverage, even after a successful final trap.
for preferred_policy in auto instruction; do
    for selected_policy in "${POLICIES[@]}"; do
        if [[ $selected_policy != "$preferred_policy" ]]; then continue; fi
        if ! policy_supported "$selected_policy"; then continue; fi
        if [[ $selected_policy == auto && $AUTO_EFFECTIVE_POLICY != instruction ]]; then continue; fi
        TIER2_WITNESS_POLICY=$selected_policy
        break 2
    done
done
printf 'tier2_witness_policy\t%s\n' "${TIER2_WITNESS_POLICY:-unavailable}" >>"$METADATA_TSV"
printf 'tier2_witness_scope\tpublication-readiness-and-trap\nt2_entry_execution\tunverified\nt2_unwind_coverage\tunverified\n' >>"$METADATA_TSV"

run_matrix_case()
{
    local id=$1
    local mode=$2
    local policy=$3
    local fixture_index=$4
    local fixture=${FIXTURE_NAMES[$fixture_index]}
    local fixture_class=${FIXTURE_CLASSES[$fixture_index]}
    local feature=${FIXTURE_FEATURES[$fixture_index]}
    local expected=${FIXTURE_EXPECTED[$fixture_index]}
    local expected_stack=${FIXTURE_STACKS[$fixture_index]}
    local wasm=${FIXTURE_WASMS[$fixture_index]}
    local stem
    local output_log compiler_log trap_kind func_indices observed_policy actual_policy call_stack_frames outcome detail
    local expected_effective_policy expect_stack=1 case_compile_threads=$COMPILE_THREADS capability_na_detail=
    local mode_args=()
    local command=()

    if [[ $fixture == tiered_full_ready_oob ]] && ((case_compile_threads < 2)); then
        write_result "$id" "$mode" "$policy" - "$fixture" "$fixture_class" "$expected" - SKIP \
            'tier-2 background readiness requires --compile-threads >= 2; requested worker cap preserved' 0 0 - -
        return 0
    fi

    if ! set_mode_args "$mode"; then
        write_result "$id" "$mode" "$policy" - "$fixture" "$fixture_class" "$expected" - FAIL \
            "internal unknown mode" 0 0 - -
        return 0
    fi
    mode_args=("${MODE_ARGS[@]}")
    if [[ $fixture == tiered_full_ready_oob ]]; then
        # Keep this witness focused on tier-2/full readiness without blocking
        # callers from selecting a scoped lazy policy for platforms whose T1
        # backend needs a conservative compile pipeline.
        mode_args+=(-Rllvm-full-policy pb-o3)
    fi

    stem=$(printf '%06d-%s-%s-%s' "$id" "$mode" "$policy" "$fixture")
    output_log="${LOG_DIR}/${stem}.log"
    compiler_log="${LOG_DIR}/${stem}.compiler.log"
    : >"$compiler_log"
    command=(
        "${RUN_PREFIX[@]+"${RUN_PREFIX[@]}"}" "$UWVM"
        "${mode_args[@]}"
        -Rct "$case_compile_threads"
    )
    if mode_uses_llvm "$mode"; then
        command+=(-Rllvm-cache-path disable -Rllvm-call-stack "$policy")
    fi
    command+=(-Rclog file "$compiler_log")
    command+=("${EXTRA_UWVM_ARGS[@]+"${EXTRA_UWVM_ARGS[@]}"}")
    if [[ $feature == wasm2 ]]; then
        command+=(--wasm-feature-wasm2)
    fi
    command+=(--run "$wasm")

    run_limited "$output_log" "${command[@]}"
    trap_kind=$(extract_trap_kind "$output_log")
    func_indices=$(extract_func_indices "$output_log")
    if ((RUN_RC != 0)); then
        capability_na_detail=$(source_pruned_llvm_capability_detail "$mode" "$fixture" "$output_log" || true)
    fi
    if mode_uses_interpreter "$mode"; then
        observed_policy=unknown
        actual_policy=logical
        expected_effective_policy=logical
        call_stack_frames=logical
    else
        observed_policy=$(extract_actual_policy "$compiler_log")
        call_stack_frames=$(extract_call_stack_frames "$compiler_log")
        if [[ $observed_policy != unknown ]]; then
            actual_policy=$observed_policy
        elif [[ $policy == auto ]]; then
            actual_policy=$AUTO_EFFECTIVE_POLICY
        else
            actual_policy=$policy
        fi
        if [[ $policy == auto ]]; then
            expected_effective_policy=$AUTO_EFFECTIVE_POLICY
        else
            expected_effective_policy=$policy
        fi
    fi
    outcome=PASS
    detail=ok

    if [[ $RUN_LIMIT_REASON == timeout ]]; then
        outcome=FAIL
        detail=timeout
    elif [[ $RUN_LIMIT_REASON == rss-limit ]]; then
        outcome=FAIL
        detail=rss-limit
    elif [[ -n $capability_na_detail ]]; then
        outcome=N-A
        detail=$capability_na_detail
    elif regular_case_requires_llvm_log "$mode" "$fixture" && ! has_expected_llvm_log "$mode" "$compiler_log" "$fixture"; then
        outcome=FAIL
        if [[ $fixture == tiered_full_ready_oob ]]; then
            detail='required tier-2 request/readiness evidence is missing'
        else
            detail='required LLVM JIT compiler evidence is missing'
        fi
    elif [[ $actual_policy == invalid || $actual_policy == unavailable || $actual_policy == inconsistent ]]; then
        outcome=FAIL
        detail="call-stack policy is unavailable: ${actual_policy}"
    elif [[ $mode == llvm-full || $mode == full || $mode == aot ]] && [[ $observed_policy == unknown ]]; then
        outcome=FAIL
        detail='LLVM full/AOT did not log the effective call-stack policy'
    elif [[ $observed_policy != unknown && $observed_policy != "$expected_effective_policy" ]]; then
        outcome=FAIL
        detail="call-stack policy mismatch: expected ${expected_effective_policy}, actual ${observed_policy}"
    elif [[ $mode == llvm-full || $mode == full || $mode == aot ]] && ! has_expected_policy_declaration "$compiler_log" "$actual_policy"; then
        outcome=FAIL
        detail="invalid ${actual_policy} optimize-start declaration"
    elif [[ $fixture_class == normal ]]; then
        if ((RUN_RC != 0)); then
            outcome=FAIL
            detail="normal case exited nonzero"
        fi
    else
        if ((RUN_RC == 0)); then
            outcome=FAIL
            detail="trap case exited zero"
        elif [[ $trap_kind != "$expected" ]]; then
            outcome=FAIL
            detail="trap kind mismatch: ${trap_kind:-missing}"
        else
            if regular_case_requires_llvm_log "$mode" "$fixture" && [[ $actual_policy == none ]]; then
                case $mode in
                    llvm-full|llvm-lazy|aot|full|lazy)
                        expect_stack=0
                        ;;
                    *)
                        # Tiered modes may still report UWVM-int/current-runtime frames while the LLVM JIT
                        # call-stack policy is none.  Treat those frames as runtime context, not as an
                        # automatic fallback from native unwind to Instruction.
                        expect_stack=2
                        ;;
                esac
            fi
            if ((expect_stack)) && [[ $func_indices != "$expected_stack" ]]; then
                if ((expect_stack == 2)); then
                    detail="trap=${trap_kind};funcs=${func_indices:-omitted};frames=${call_stack_frames};tiered_runtime_frames=allowed"
                else
                    outcome=FAIL
                    detail="call stack mismatch: expected ${expected_stack}, actual ${func_indices:-missing}"
                fi
            elif ((expect_stack == 0)) && [[ -n $func_indices ]]; then
                outcome=FAIL
                detail="auto none emitted forbidden Instruction frames: ${func_indices}"
            else
                detail="trap=${trap_kind};funcs=${func_indices:-omitted};frames=${call_stack_frames}"
            fi
        fi
    fi

    if [[ $fixture == tiered_full_ready_oob && $outcome == PASS ]]; then
        detail+=';t2_publication=ready;t2_entry_execution=unverified;t2_unwind_coverage=unverified'
    fi
    write_result "$id" "$mode" "$policy" "$actual_policy" "$fixture" "$fixture_class" "$expected" "$RUN_RC" "$outcome" "$detail" \
        "$RUN_PEAK_RSS_KIB" "$RUN_ELAPSED_MS" "$output_log" "$compiler_log"
    return 0
}

for unavailable_policy in unwind unwind-uncheck; do
    native_policy_requested=0
    native_policy_supported=0
    for policy in "${POLICIES[@]}"; do
        if [[ $policy == "$unavailable_policy" ]]; then native_policy_requested=1; fi
    done
    case $unavailable_policy in
        unwind) native_policy_supported=$CAP_UNWIND ;;
        unwind-uncheck) native_policy_supported=$CAP_UNWIND_UNCHECK ;;
    esac
    if ((HAS_SELECTED_LLVM_MODE && native_policy_requested && native_policy_supported == 0)); then
        allocate_id
        guard_id=$ALLOCATED_ID
        guard_log="${LOG_DIR}/$(printf '%06d' "$guard_id")-unavailable-${unavailable_policy}-guard.log"
        guard_compiler_log="${LOG_DIR}/$(printf '%06d' "$guard_id")-unavailable-${unavailable_policy}-guard.compiler.log"
        : >"$guard_compiler_log"
        if ((CAP_CALL_STACK)) && [[ -n $POLICY_PROBE_MODE ]]; then
            set_mode_args "$POLICY_PROBE_MODE" || die "internal invalid policy probe mode: $POLICY_PROBE_MODE"
            run_limited "$guard_log" \
                "${RUN_PREFIX[@]+"${RUN_PREFIX[@]}"}" "$UWVM" "${MODE_ARGS[@]}" -Rct "$COMPILE_THREADS" \
                -Rllvm-cache-path disable -Rllvm-call-stack "$unavailable_policy" -Rclog file "$guard_compiler_log" \
                "${EXTRA_UWVM_ARGS[@]+"${EXTRA_UWVM_ARGS[@]}"}" --run "${FIXTURE_WASMS[0]}"
            guard_output=$(tail -n +2 "$guard_log")
            guard_outcome=PASS
            guard_detail="explicit unavailable ${unavailable_policy} was rejected"
            if [[ $RUN_LIMIT_REASON != exit ]]; then
                guard_outcome=FAIL
                guard_detail="${unavailable_policy} rejection probe hit ${RUN_LIMIT_REASON}"
            elif ((RUN_RC == 0)); then
                guard_outcome=FAIL
                guard_detail="unavailable ${unavailable_policy} was silently accepted"
            elif [[ $guard_output == *'Runtime crash ('* ]]; then
                guard_outcome=FAIL
                guard_detail="unavailable ${unavailable_policy} reached runtime instead of CLI rejection"
            elif ! grep -Eqi 'unwind|call-stack|invalid|usage|support|available' <<<"$guard_output"; then
                guard_outcome=FAIL
                guard_detail="unavailable ${unavailable_policy} rejection diagnostic was not recognized"
            fi
            write_result "$guard_id" capability "$unavailable_policy" unsupported unwind-availability capability rejected "$RUN_RC" \
                "$guard_outcome" "$guard_detail" "$RUN_PEAK_RSS_KIB" "$RUN_ELAPSED_MS" "$guard_log" "$guard_compiler_log"
        else
            write_result "$guard_id" capability "$unavailable_policy" unsupported unwind-availability capability rejected - "$(unsupported_outcome)" \
                'cannot run rejection guard without an advertised AOT/full mode and call-stack option' 0 0 - -
        fi
    fi
done

active_pids=()
reap_first_task()
{
    local pid=${active_pids[0]}
    if ! wait "$pid"; then
        die "internal matrix worker failed: pid=${pid}"
    fi
    active_pids=("${active_pids[@]:1}")
}

schedule_case()
{
    run_matrix_case "$@" &
    active_pids+=("$!")
    if ((${#active_pids[@]} >= JOBS)); then
        reap_first_task
    fi
}

for mode in "${MODE_NAMES[@]+"${MODE_NAMES[@]}"}"; do
    if ! mode_supported "$mode"; then continue; fi
    if [[ $mode == tiered && -z $TIER2_WITNESS_POLICY ]]; then
        allocate_id
        write_result "$ALLOCATED_ID" "$mode" - - tiered_full_ready_oob trap 'memory access out of bounds' - SKIP \
            'no compatible instruction policy for tier-2 readiness; native unwind disables background T2;t2_entry_execution=unverified;t2_unwind_coverage=unverified' \
            0 0 - -
    fi
    for policy in "${POLICIES[@]}"; do
        if ! policy_applicable_to_mode "$mode" "$policy"; then continue; fi
        if ! policy_supported "$policy"; then continue; fi
        if [[ $policy == auto && $AUTO_EFFECTIVE_POLICY == invalid ]]; then continue; fi
        if [[ $policy == unwind-uncheck && $UNWIND_UNCHECK_REPLACEMENT_STATUS != verified ]]; then continue; fi
        for fixture_index in "${!FIXTURE_NAMES[@]}"; do
            if [[ ${FIXTURE_FEATURES[$fixture_index]} == wasm2 && $CAP_WASM2 == 0 ]]; then continue; fi
            if ! fixture_applicable_to_mode "$fixture_index" "$mode"; then continue; fi
            if [[ ${FIXTURE_NAMES[$fixture_index]} == tiered_full_ready_oob && $policy != "$TIER2_WITNESS_POLICY" ]]; then continue; fi
            allocate_id
            schedule_case "$ALLOCATED_ID" "$mode" "$policy" "$fixture_index"
        done
    done
done

while ((${#active_pids[@]} != 0)); do
    reap_first_task
done

{
    printf 'id\tlabel\tversion\tarchitecture\tc_library\tmemory_model\tcapabilities\tmode\tpolicy\tactual_policy\tfixture\tclass\texpected\texit_code\toutcome\tdetail\tpeak_rss_kib\telapsed_ms\tlog\tcompiler_log\n'
    shopt -s nullglob
    fragments=("${FRAGMENT_DIR}"/*.tsv)
    if ((${#fragments[@]} != 0)); then
        cat "${fragments[@]}"
    fi
    shopt -u nullglob
} >"$RESULTS_TSV"

PASS_COUNT=$(awk -F '\t' 'NR > 1 && $15 == "PASS" { count++ } END { print count + 0 }' "$RESULTS_TSV")
FAIL_COUNT=$(awk -F '\t' 'NR > 1 && $15 == "FAIL" { count++ } END { print count + 0 }' "$RESULTS_TSV")
SKIP_COUNT=$(awk -F '\t' 'NR > 1 && $15 == "SKIP" { count++ } END { print count + 0 }' "$RESULTS_TSV")
NA_COUNT=$(awk -F '\t' 'NR > 1 && $15 == "N-A" { count++ } END { print count + 0 }' "$RESULTS_TSV")

RELEASE_QUALIFIED=not-applicable
RELEASE_QUALIFICATION_REASON=legacy-profile-preserves-compatible-exit-status
if [[ $PROFILE_RESOLVED == full || $PROFILE_RESOLVED == ros ]]; then
    if ((FAIL_COUNT != 0)); then
        RELEASE_QUALIFIED=false
        RELEASE_QUALIFICATION_REASON=fail-count-nonzero
    elif ((SKIP_COUNT != 0)); then
        RELEASE_QUALIFIED=false
        RELEASE_QUALIFICATION_REASON=skip-count-nonzero
    elif ((PASS_COUNT > 0)); then
        RELEASE_QUALIFIED=true
        RELEASE_QUALIFICATION_REASON=fail-and-skip-counts-zero-and-pass-count-positive
    else
        RELEASE_QUALIFIED=false
        RELEASE_QUALIFICATION_REASON=no-passing-executed-case
    fi
fi
RUN_FINISHED_UTC=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
{
    printf 'run_finished_utc\t%s\n' "$RUN_FINISHED_UTC"
    printf 'result_pass_count\t%s\n' "$PASS_COUNT"
    printf 'result_fail_count\t%s\n' "$FAIL_COUNT"
    printf 'result_skip_count\t%s\n' "$SKIP_COUNT"
    printf 'result_n_a_count\t%s\n' "$NA_COUNT"
    printf 'release_qualification_rule\tFAIL=0, SKIP=0, and PASS>0 for resolved full/ros; legacy exit compatibility preserved\n'
    printf 'release_qualified\t%s\n' "$RELEASE_QUALIFIED"
    printf 'release_qualification_reason\t%s\n' "$RELEASE_QUALIFICATION_REASON"
} >>"$METADATA_TSV"

printf '[qemu-matrix] PASS=%s FAIL=%s SKIP=%s N-A=%s release_qualified=%s profile=%s results=%s metadata=%s fixtures=%s\n' \
    "$PASS_COUNT" "$FAIL_COUNT" "$SKIP_COUNT" "$NA_COUNT" "$RELEASE_QUALIFIED" "$PROFILE_RESOLVED" \
    "$RESULTS_TSV" "$METADATA_TSV" "$FIXTURE_MANIFEST_TSV"

if ((FAIL_COUNT != 0)); then
    exit 1
fi
if [[ $PROFILE_RESOLVED != legacy && $RELEASE_QUALIFIED != true ]]; then
    exit 1
fi

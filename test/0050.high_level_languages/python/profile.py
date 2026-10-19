import sys
import time
import hll_common as common


def main():
    rounds = common.arguments()
    if rounds is None:
        return common.invalid()
    prefix = common.semantics()
    warm_values, warm_accumulator = common.setup()
    common.kernel(warm_values, warm_accumulator, 100)
    values, accumulator = common.setup()
    stream = sys.stdin.buffer
    time.monotonic_ns()
    print("UWVM_PROFILE_READY", file=sys.stderr, flush=True)
    if stream.read(1) != b"G":
        print("python invalid profile release", file=sys.stderr)
        return 2
    started = time.monotonic_ns()
    checksum = common.kernel(values, accumulator, rounds)
    elapsed = time.monotonic_ns() - started
    print("UWVM_PROFILE_DONE", file=sys.stderr, flush=True)
    if stream.read(1) != b"D":
        print("python invalid profile finish", file=sys.stderr)
        return 2
    common.output(prefix, rounds, checksum)
    print(f"Kernel ns: {elapsed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

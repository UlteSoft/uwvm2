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
    time.monotonic_ns()  # Initialize the guest clock path before measurement.
    started = time.monotonic_ns()
    checksum = common.kernel(values, accumulator, rounds)
    elapsed = time.monotonic_ns() - started
    common.output(prefix, rounds, checksum)
    print(f"Kernel ns: {elapsed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

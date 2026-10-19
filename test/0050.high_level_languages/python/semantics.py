import hll_common as common


def main():
    rounds = common.arguments()
    if rounds is None:
        return common.invalid()
    prefix = common.semantics()
    values, accumulator = common.setup()
    common.output(prefix, rounds, common.kernel(values, accumulator, rounds))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

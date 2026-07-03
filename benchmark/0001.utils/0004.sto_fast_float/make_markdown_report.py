#!/usr/bin/env python3
import csv
import math
import pathlib
import sys


ALGORITHMS = ("fast_float", "fast_io_public", "fast_io_mask", "fast_io_scalar")
ALGORITHM_LABELS = {
    "fast_float": "fast_float",
    "fast_io_public": "fast_io_public",
    "fast_io_mask": "fast_io_mask",
    "fast_io_scalar": "fast_io_scalar",
}
ALPHA_CASES = ("lower", "mixed", "upper")
NORMAL_CASES = ("normal",)
EXTRA_BUFFERS = (0, 64)
LIMIT_U64 = (1 << 64) - 1


def max_digits_u64(base):
    value = 1
    digits = 0
    while True:
        digits += 1
        if value > LIMIT_U64 // base:
            break
        value *= base
    return digits


def expected_case_keys():
    keys = []
    for base in range(2, 37):
        cases = ALPHA_CASES if base >= 11 else NORMAL_CASES
        for length in range(1, max_digits_u64(base) + 1):
            for case in cases:
                for extra_buffer in EXTRA_BUFFERS:
                    keys.append((base, length, case, extra_buffer))
    return keys


def read_tsv(path):
    by_case = {}
    raw_rows = []
    options = set()
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f, delimiter="\t")
        required = {
            "base",
            "length",
            "case",
            "extra_buffer",
            "algorithm",
            "ns_per_parse",
            "checksum",
            "samples",
            "iters",
            "repeats",
        }
        missing = required.difference(reader.fieldnames or ())
        if missing:
            raise SystemExit(f"{path}: missing TSV columns: {', '.join(sorted(missing))}")
        for row in reader:
            base = int(row["base"])
            length = int(row["length"])
            case = row["case"]
            extra_buffer = int(row["extra_buffer"])
            algorithm = row["algorithm"]
            ns_per_parse = float(row["ns_per_parse"])
            checksum = int(row["checksum"])
            samples = int(row["samples"])
            iters = int(row["iters"])
            repeats = int(row["repeats"])
            if algorithm not in ALGORITHMS:
                raise SystemExit(f"{path}: unknown algorithm: {algorithm}")
            key = (base, length, case, extra_buffer)
            case_rows = by_case.setdefault(key, {})
            if algorithm in case_rows:
                raise SystemExit(f"{path}: duplicate row for {key} / {algorithm}")
            data = {
                "ns_per_parse": ns_per_parse,
                "checksum": checksum,
                "samples": samples,
                "iters": iters,
                "repeats": repeats,
            }
            case_rows[algorithm] = data
            raw_rows.append((key, algorithm, data))
            options.add((samples, iters, repeats))
    return by_case, raw_rows, sorted(options)


def validate_dataset(by_case, raw_rows):
    expected = expected_case_keys()
    expected_set = set(expected)
    actual_set = set(by_case)
    missing_cases = sorted(expected_set.difference(actual_set))
    unexpected_cases = sorted(actual_set.difference(expected_set))
    if missing_cases:
        raise SystemExit(f"missing input cases: {missing_cases[:8]} ... total={len(missing_cases)}")
    if unexpected_cases:
        raise SystemExit(f"unexpected input cases: {unexpected_cases[:8]} ... total={len(unexpected_cases)}")
    for key in expected:
        got_algorithms = set(by_case[key])
        missing_algorithms = set(ALGORITHMS).difference(got_algorithms)
        unexpected_algorithms = got_algorithms.difference(ALGORITHMS)
        if missing_algorithms or unexpected_algorithms:
            raise SystemExit(
                f"{key}: algorithm mismatch, missing={sorted(missing_algorithms)}, "
                f"unexpected={sorted(unexpected_algorithms)}"
            )

    expected_rows = len(expected) * len(ALGORITHMS)
    if len(raw_rows) != expected_rows:
        raise SystemExit(f"row count mismatch: got={len(raw_rows)} expected={expected_rows}")

    algorithm_checksum_mismatches = 0
    for key in expected:
        checksums = {by_case[key][algorithm]["checksum"] for algorithm in ALGORITHMS}
        if len(checksums) != 1:
            algorithm_checksum_mismatches += 1

    extra_checksum_mismatches = 0
    for base in range(2, 37):
        cases = ALPHA_CASES if base >= 11 else NORMAL_CASES
        for length in range(1, max_digits_u64(base) + 1):
            for case in cases:
                key0 = (base, length, case, 0)
                key64 = (base, length, case, 64)
                for algorithm in ALGORITHMS:
                    if by_case[key0][algorithm]["checksum"] != by_case[key64][algorithm]["checksum"]:
                        extra_checksum_mismatches += 1

    return {
        "expected_cases": len(expected),
        "expected_rows": expected_rows,
        "algorithm_checksum_mismatches": algorithm_checksum_mismatches,
        "extra_checksum_mismatches": extra_checksum_mismatches,
    }


def ns(by_case, key, algorithm):
    return by_case[key][algorithm]["ns_per_parse"]


def checksum(by_case, key):
    return by_case[key][ALGORITHMS[0]]["checksum"]


def geomean(values):
    values = list(values)
    if not values:
        return float("nan")
    return math.exp(sum(math.log(value) for value in values) / len(values))


def ratio(by_case, key, algorithm):
    return ns(by_case, key, algorithm) / ns(by_case, key, "fast_float")


def fmt_ns(value):
    return f"{value:.3f}"


def fmt_ratio(value):
    return f"{value:.3f}"


def fmt_count_ratio(count, total):
    return f"{count}/{total}"


def fastest_algorithm(by_case, key):
    return min(ALGORITHMS, key=lambda algorithm: ns(by_case, key, algorithm))


def summary_for_keys(by_case, keys):
    total = len(keys)
    rows = []
    for algorithm in ALGORITHMS[1:]:
        ratios = [ratio(by_case, key, algorithm) for key in keys]
        rows.append(
            (
                algorithm,
                geomean(ratios),
                sum(1 for value in ratios if value > 1.03),
                sum(1 for value in ratios if value < 0.97),
                total,
            )
        )
    return rows


def fastest_counts(by_case, keys):
    counts = {algorithm: 0 for algorithm in ALGORITHMS}
    for key in keys:
        counts[fastest_algorithm(by_case, key)] += 1
    return counts


def markdown_table(headers, aligns, rows):
    out = []
    out.append("| " + " | ".join(headers) + " |")
    align_cells = []
    for align in aligns:
        if align == "right":
            align_cells.append("---:")
        elif align == "center":
            align_cells.append(":---:")
        else:
            align_cells.append("---")
    out.append("|" + "|".join(align_cells) + "|")
    for row in rows:
        out.append("| " + " | ".join(str(cell) for cell in row) + " |")
    return out


def input_label(key):
    _base, length, case, extra_buffer = key
    label = f"length{length}"
    if case != "normal":
        label += f" {case}"
    if extra_buffer:
        label += f" +{extra_buffer}B ext"
    return label


def case_sort_index(case):
    if case == "normal":
        return 0
    return ALPHA_CASES.index(case)


def sorted_expected_keys():
    return sorted(expected_case_keys(), key=lambda key: (key[0], key[1], case_sort_index(key[2]), key[3]))


def append_overall(out, by_case, keys):
    out.append("## Overall")
    out.append("")
    rows = []
    for algorithm, geo, slower, faster, total in summary_for_keys(by_case, keys):
        rows.append((algorithm, fmt_ratio(geo), fmt_count_ratio(slower, total), fmt_count_ratio(faster, total)))
    out.extend(
        markdown_table(
            ("algorithm", "geomean vs fast_float", "slower than fast_float by >3%", "faster than fast_float by >3%"),
            ("left", "right", "right", "right"),
            rows,
        )
    )
    out.append("")

    counts = fastest_counts(by_case, keys)
    rows = [(algorithm, counts[algorithm]) for algorithm in ALGORITHMS]
    out.extend(markdown_table(("fastest algorithm", "cases"), ("left", "right"), rows))
    out.append("")


def append_by_extra(out, by_case, keys):
    out.append("## By Extra Buffer")
    out.append("")
    rows = []
    for extra_buffer in EXTRA_BUFFERS:
        selected = [key for key in keys if key[3] == extra_buffer]
        for algorithm, geo, slower, faster, total in summary_for_keys(by_case, selected):
            rows.append((extra_buffer, algorithm, fmt_ratio(geo), fmt_count_ratio(slower, total), fmt_count_ratio(faster, total)))
    out.extend(
        markdown_table(
            ("extra_buffer", "algorithm", "geomean vs fast_float", "slower >3%", "faster >3%"),
            ("right", "left", "right", "right", "right"),
            rows,
        )
    )
    out.append("")

    rows = []
    for algorithm in ALGORITHMS:
        ratios = []
        for key in keys:
            base, length, case, extra_buffer = key
            if extra_buffer != 0:
                continue
            key64 = (base, length, case, 64)
            ratios.append(ns(by_case, key64, algorithm) / ns(by_case, key, algorithm))
        rows.append((algorithm, fmt_ratio(geomean(ratios))))
    out.extend(markdown_table(("algorithm", "geomean ns ratio extra64 / extra0"), ("left", "right"), rows))
    out.append("")


def append_by_base(out, by_case, keys):
    out.append("## By Base")
    out.append("")
    rows = []
    for base in range(2, 37):
        selected = [key for key in keys if key[0] == base]
        summary = {algorithm: geo for algorithm, geo, _slower, _faster, _total in summary_for_keys(by_case, selected)}
        counts = fastest_counts(by_case, selected)
        rows.append(
            (
                base,
                len(selected),
                fmt_ratio(summary["fast_io_public"]),
                fmt_ratio(summary["fast_io_mask"]),
                fmt_ratio(summary["fast_io_scalar"]),
                "/".join(str(counts[algorithm]) for algorithm in ALGORITHMS),
            )
        )
    out.extend(
        markdown_table(
            ("base", "input cases", "public/ff", "mask/ff", "scalar/ff", "fastest counts ff/public/mask/scalar"),
            ("right", "right", "right", "right", "right", "right"),
            rows,
        )
    )
    out.append("")

    out.append("## By Base And Extra Buffer")
    out.append("")
    rows = []
    for base in range(2, 37):
        for extra_buffer in EXTRA_BUFFERS:
            selected = [key for key in keys if key[0] == base and key[3] == extra_buffer]
            summary = {algorithm: geo for algorithm, geo, _slower, _faster, _total in summary_for_keys(by_case, selected)}
            counts = fastest_counts(by_case, selected)
            rows.append(
                (
                    base,
                    extra_buffer,
                    len(selected),
                    fmt_ratio(summary["fast_io_public"]),
                    fmt_ratio(summary["fast_io_mask"]),
                    fmt_ratio(summary["fast_io_scalar"]),
                    "/".join(str(counts[algorithm]) for algorithm in ALGORITHMS),
                )
            )
    out.extend(
        markdown_table(
            ("base", "extra_buffer", "input cases", "public/ff", "mask/ff", "scalar/ff", "fastest counts ff/public/mask/scalar"),
            ("right", "right", "right", "right", "right", "right", "right"),
            rows,
        )
    )
    out.append("")


def append_worst_slowdowns(out, by_case, keys):
    out.append("## Worst Slowdowns")
    out.append("")
    for algorithm in ALGORITHMS[1:]:
        out.append(f"### {algorithm} vs fast_float")
        out.append("")
        ranked = sorted(((ratio(by_case, key, algorithm), key) for key in keys), reverse=True)
        rows = []
        for value, key in ranked[:20]:
            base, length, case, extra_buffer = key
            rows.append(
                (
                    fmt_ratio(value),
                    base,
                    length,
                    case,
                    extra_buffer,
                    fmt_ns(ns(by_case, key, "fast_float")),
                    fmt_ns(ns(by_case, key, algorithm)),
                )
            )
        out.extend(
            markdown_table(
                ("ratio", "base", "length", "case", "extra_buffer", "fast_float", algorithm),
                ("right", "right", "right", "left", "right", "right", "right"),
                rows,
            )
        )
        out.append("")


def append_complete_per_base_tables(out, by_case, keys):
    out.append("## Complete Per-Base Tables")
    out.append("")
    out.append(
        "Each row is one input case. The `+64B ext` rows use the same parsed byte range as the non-ext row; "
        "`last` remains `first + length` and the extra 64 bytes are only trailing storage."
    )
    out.append("")
    headers = (
        "input",
        "length",
        "case",
        "extra_buffer",
        "fast_float",
        "fast_io_public",
        "public/ff",
        "fast_io_mask",
        "mask/ff",
        "fast_io_scalar",
        "scalar/ff",
        "checksum",
    )
    aligns = ("left", "right", "left", "right", "right", "right", "right", "right", "right", "right", "right", "right")
    for base in range(2, 37):
        selected = [key for key in keys if key[0] == base]
        out.append(f"### Base {base}")
        out.append("")
        out.append(f"- Max length: {max_digits_u64(base)}")
        out.append(f"- Input cases: {len(selected)}")
        out.append("")
        rows = []
        for key in selected:
            _base, length, case, extra_buffer = key
            rows.append(
                (
                    input_label(key),
                    length,
                    case,
                    extra_buffer,
                    fmt_ns(ns(by_case, key, "fast_float")),
                    fmt_ns(ns(by_case, key, "fast_io_public")),
                    fmt_ratio(ratio(by_case, key, "fast_io_public")),
                    fmt_ns(ns(by_case, key, "fast_io_mask")),
                    fmt_ratio(ratio(by_case, key, "fast_io_mask")),
                    fmt_ns(ns(by_case, key, "fast_io_scalar")),
                    fmt_ratio(ratio(by_case, key, "fast_io_scalar")),
                    checksum(by_case, key),
                )
            )
        out.extend(markdown_table(headers, aligns, rows))
        out.append("")


def append_raw_rows(out, by_case, keys):
    out.append("## Full Raw Rows")
    out.append("")
    rows = []
    for key in keys:
        base, length, case, extra_buffer = key
        for algorithm in ALGORITHMS:
            data = by_case[key][algorithm]
            rows.append(
                (
                    base,
                    length,
                    case,
                    extra_buffer,
                    algorithm,
                    f'{data["ns_per_parse"]:.6f}',
                    data["checksum"],
                )
            )
    out.extend(
        markdown_table(
            ("base", "length", "case", "extra_buffer", "algorithm", "ns_per_parse", "checksum"),
            ("right", "right", "left", "right", "left", "right", "right"),
            rows,
        )
    )
    out.append("")


def generate_report(tsv_path):
    by_case, raw_rows, options = read_tsv(tsv_path)
    validation = validate_dataset(by_case, raw_rows)
    keys = sorted_expected_keys()

    option_text = ", ".join(f"samples={samples} iters={iters} repeats={repeats}" for samples, iters, repeats in options)
    out = []
    out.append("# Full fast_io sto vs fast_float integer parsing benchmark")
    out.append("")
    out.append("Machine used for this checked-in result:")
    out.append("")
    out.append("- Target/host: aarch64-apple-darwin, Apple M4")
    out.append("- Compiler: clang++ 23.0.0git")
    out.append("- Result TSV: `results/sto_fast_float_m4_full.tsv`")
    out.append("- Bases: 2 through 36")
    out.append("- Lengths: 1 through the maximum non-overflowing `uint64_t` digit length for each base")
    out.append("- Cases: bases 2-10 use `normal`; bases 11-36 use `lower`, `mixed`, and `upper`")
    out.append("- Extra trailing buffer: `0` and `64` bytes after each parsed string; `last` is always `first + length`")
    out.append("- Timing unit: best-of-repeats ns/parse")
    out.append(f"- Benchmark options in TSV: {option_text}")
    out.append("")
    out.append("Fairness notes:")
    out.append("")
    out.append("- All algorithms parse the same generated byte buffers for a given base, length, case, and extra-buffer setting.")
    out.append("- The parsed range is always `[first, first + length)`; the 64-byte extra-buffer rows only add bytes after `last`.")
    out.append(
        "- Generated inputs are non-overflowing `uint64_t` values with a nonzero first digit and no sign or base prefix."
    )
    out.append(
        "- `fast_io_public` is exactly `fast_io::parse_by_scan(first, last, "
        "fast_io::manipulators::base_get<base, true, true>(value))`."
    )
    out.append(
        "- In fast_io, `base_get` parameters are `<base, noskipws, skipzero, prefix, oct_c2y>`, so this benchmark uses "
        "`noskipws=true`, `skipzero=true`, `prefix=false`, and `oct_c2y=false`."
    )
    out.append(
        "- `fast_io_mask` and `fast_io_scalar` are direct internal core diagnostics, not public API replacements. "
        "The public API comparison is `fast_io_public` vs `fast_float`."
    )
    out.append(
        "- No force-inline attribute was added to `parse_by_scan` or `parse_by_scan_impl`; the concept-dispatch core is kept intact."
    )
    out.append(
        "- The retained public-path optimization is limited to the concrete integer "
        "`scan_contiguous_define(... scalar_manip_t<...>)` overload."
    )
    out.append("")
    out.append("Build command:")
    out.append("")
    out.append("```sh")
    out.append("clang++ -o /tmp/sto_fast_float_bench_full \\")
    out.append("  benchmark/0001.utils/0004.sto_fast_float/sto_fast_float_bench.cc \\")
    out.append("  -Ofast -fno-exceptions -flto -s -march=native -std=c++26 -fno-rtti \\")
    out.append("  -I \"$MACROMODEL/src/uwvm2/third-parties/fast_io/include\" \\")
    out.append("  -I \"$MACROMODEL/src/uwvm2/third-parties/fast_float/include\" \\")
    out.append("  --sysroot=\"$SYSROOT\" -fuse-ld=lld")
    out.append("```")
    out.append("")
    out.append("Run command:")
    out.append("")
    out.append("```sh")
    out.append("/tmp/sto_fast_float_bench_full --samples=16384 --iters=16 --repeats=5 \\")
    out.append("  > benchmark/0001.utils/0004.sto_fast_float/results/sto_fast_float_m4_full.tsv")
    out.append("```")
    out.append("")
    out.append("Markdown regeneration command:")
    out.append("")
    out.append("```sh")
    out.append("python3 benchmark/0001.utils/0004.sto_fast_float/make_markdown_report.py \\")
    out.append("  benchmark/0001.utils/0004.sto_fast_float/results/sto_fast_float_m4_full.tsv \\")
    out.append("  benchmark/0001.utils/0004.sto_fast_float/results/sto_fast_float_m4_full.md")
    out.append("```")
    out.append("")
    out.append("Correctness and coverage checks:")
    out.append("")
    out.append(f"- Rows: {len(raw_rows)}")
    out.append(f"- Expected rows: {validation['expected_rows']}")
    out.append(f"- Input cases: {validation['expected_cases']}")
    out.append(f"- Algorithm checksum mismatches: {validation['algorithm_checksum_mismatches']}")
    out.append(f"- 0-byte vs 64-byte buffer checksum mismatches: {validation['extra_checksum_mismatches']}")
    out.append("")

    append_overall(out, by_case, keys)
    append_by_extra(out, by_case, keys)
    append_by_base(out, by_case, keys)
    append_worst_slowdowns(out, by_case, keys)
    append_complete_per_base_tables(out, by_case, keys)
    append_raw_rows(out, by_case, keys)
    return "\n".join(out)


def main(argv):
    if len(argv) != 3:
        raise SystemExit("usage: make_markdown_report.py INPUT.tsv OUTPUT.md")
    tsv_path = pathlib.Path(argv[1])
    output_path = pathlib.Path(argv[2])
    report = generate_report(tsv_path)
    output_path.write_text(report + "\n", encoding="utf-8")


if __name__ == "__main__":
    main(sys.argv)

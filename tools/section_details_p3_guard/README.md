# section_details_p3_guard

Regression guard for the Wasm `section-details` P3 context-printing path.

It keeps the P3.2 stabilization checks in one reproducible place:

- old/new normalized output comparison
- interleaved timing mean/median
- `write`/`writev` syscall counts with `strace`
- binary size comparison
- optional `-fstack-usage` rebuild and stack-frame summary
- optional `perf stat` runs

## Usage

Use a clean old worktree as baseline and the current worktree as new:

```bash
python3 tools/section_details_p3_guard/p3_guard.py \
  --old-root /tmp/uwvm2-section-details-old \
  --new-root . \
  --work-dir /tmp/uwvm2_p3_guard \
  --skip-build \
  --runs 30 \
  --warmup 6
```

To rebuild both trees before measuring:

```bash
python3 tools/section_details_p3_guard/p3_guard.py \
  --old-root /tmp/uwvm2-section-details-old \
  --new-root . \
  --work-dir /tmp/uwvm2_p3_guard \
  --runs 80 \
  --warmup 8
```

To collect stack-usage summaries, add `--stack-usage`. This reconfigures both
trees with `-fstack-usage` and LTO disabled before rebuilding.

To collect hardware counters, add `--perf`. Perf permissions vary by host; the
script records a skipped status if `perf stat` cannot run.


# Thread management performance fixture

`run_creation_performance.py` runs a small paired benchmark inside the required
remote Linux cgroup. It measures existing thread-management utilities; it does
not execute a Wasm module or establish Wasm memory throughput.

- `prepared_batch_create_run_join`: 64 preconstructed coroutine tasks with the
  same checksum work. The product `native_thread_pool` is compared with a
  `std::thread` implementation using the same atomic index, memory ordering,
  caller participation, and `scheduled_task_batch::resume_and_destroy` operation.
  Worker allocation, creation, joining, and worker-container reclamation are
  included. Coroutine preparation is outside both timers.
- `host_thread_domain_create_run_join`: identical host-created `std::thread`s,
  with or without one `execution_domain` admission lease per worker. Each runs
  the same out-of-line kernel over its private 8 KiB buffer, for 64 passes.
  Domain generation setup is outside timing. The kernel timer starts after
  admission and ends before releasing the lease. Its mean per-worker duration
  is reported separately; parallel body durations are never subtracted from
  total wall time.

Each profile uses one and four workers, two warmup pairs, and nine measured
AB/BA pairs of 32 rounds. Reports retain every sample, each variant's median,
and the median of the nine paired ratios. A ratio of medians can differ from
the median paired ratio when scheduling changes; neither should be hidden.
The exact checksums must agree. A separate unmeasured binary wraps native
`pthread_create` calls to verify the product pool created the requested workers
instead of taking its serial failure fallback. The timed binary has no wrapper.

The runner records source and executable hashes, compiler information, actual
CPU affinity, and cgroup memory/CPU/pid state before and after execution. For the
current x86-64 qualification it also saves optimized assembly of the shared
kernel and rejects function calls, locked instructions, or explicit fences in
that body. This inspection does not qualify another architecture or the Wasm
engine's generated memory instructions.

Run only on the remote host, inside the configured container:

```sh
UWVM_TEST_CPUSET=0,2,4,6,16-31 CXX=/toolchain/bin/clang++ \
  python3 test/0003.utils/thread/run_creation_performance.py --out /dev/shm/thread-perf
```

The container must already provide the toolchain library search path and the
64 GiB/no-swap cgroup. `--base-source-root` supports a small frozen source overlay
over a complete repository. Every output directory must be new. This is a
measurement with a reproducible baseline, not a performance pass/fail threshold;
inspect raw ranges, scheduling noise, and concurrent host load before drawing a
regression conclusion.

# Foreign bridge final-lease teardown

`wasm3_gc_bridge_teardown.cc` creates one object in a source store and wraps
it in a different receiver store. It then releases the source's host owner
and the receiver's independent lease roots. The wrapper's `inner_owner` is
the remaining source owner when the receiver store is released.

This release order matches `wasm_module_storage_t`: `gc_lease_roots` is
declared after `gc_store`, so C++ destroys the roots first. A destructor
that deletes the wrapper while holding the global bridge lock recursively
enters the source store destructor and attempts to acquire the same lock.
This is a one-direction teardown deadlock, distinct from mutually retained
store cycles and from absent unreachable-object collection.

The fixture prints `TEARDOWN_BEGIN last foreign inner_owner pin` before the
critical release. Success requires both stores' weak pointers to expire and
the final `PASS one-direction foreign bridge teardown` marker. The runner
records a five-second timeout as failure, kills and reaps the test process,
and exits 2. A timeout without an ASan report must not be described as an
ASan crash or a passing sanitizer check.

Run only after the shared Linux resource window is available. Supply the
actual frozen checkout's source ID; the runner verifies it before and after.

```sh
python3 test/0017.runtime/run_gc_bridge_teardown.py \
  --source-root "$CORE3_SOURCE" --expected-source-id "$CORE3_SOURCE_ID" \
  --out "$CORE3_EVIDENCE/foreign-bridge-teardown"
```

An isolated fix can be checked with `--overlay-root` and
`--expected-overlay-sha256`. The overlay must contain only
`uwvm2/uwvm/runtime/storage/gc_object.h`. Its selection is verified from the
compiler dependency file. The runner never edits product source.

Linux requires the established 64 GiB, swap-free cgroup and pins compilation
and execution to E core 16. It enables ASan, UBSan and LSan. On macOS the
default aggregate child-process RSS limit is 512 MiB; ASan and UBSan are
enabled, while leak detection remains disabled because this is not Linux
LSan qualification. Neither run measures performance or proves a collector.

The current product header `21f90b8edb9427f12feb710ea643011d5fba3a672ccd5a0ac76b0e1e16802a2d`
and isolated registry v3 `1080f56b8b723002342ac7131aea4a28d9c2f7445694a4d96d5825d502ddaeeb`
both timed out in source-bound native Mac probes. The separate cold-teardown
overlay `a24fa96e3e45eb1ec2e70936ce2da06d1439cf269a0d175bfa7ed38661017c49`
completed in both repository contexts under ASan/UBSan. These are isolated
Mac correctness results, not a claim that the product contains the fix.

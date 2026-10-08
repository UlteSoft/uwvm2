# Genuine original owned-directory capsule lifecycle

`debug_wasip1_environment_owned_capsule_runtime.cc` and `fixtures/debug_wasip1_environment_owned_capsule.wat` specialize the real native environment capsule fixture. They use original WASIp1 mount initialization and renumber to create an actual owned preopen at guest FD151. The original compiled guest calls the real tracked `fd_close` after the genuine capture/edits, without a native FD injection, direct FD-table update, native resource pointer output, or private capsule constructor bypass.

The trusted native fixture keeps only a FastIO observer of the originally issued directory handle. After the guest has closed FD151, `fast_io::status` must still observe that owned directory because the authentic capsule pins its RC. After real VM reset and the last authentic native capsule owner release, status must fail. Mount-root's separate duplicate is not that same native handle. No native open occurs between those two lifetime probes. The fixture never reads a capsule's private resource fields.

Keeper: after exact ROOT integration, official wasm-tools parse+validate modern GC WAT; fresh exact-cut runtime fixture build, then independently run `debug_wasip1_environment_owned_capsule_runtime fixture.wasm instruction TEMP_DIRECTORY` and `... unwind TEMP_DIRECTORY` for both products inside the original guarded64GiB cgroup. TEMP_DIRECTORY is a sandbox-owned existing directory. Full runtime must be freshly built; no private overlay or old runtime reuse. The actual original provider cannot issue an owned preopen for some legacy targets; those return77 with explicit UNSUPPORTED and are not counted as this owned lifecycle PASS.

Expected exit0 and stdout `debug_wasip1_environment_owned_capsule_runtime PASS actual owned directory survives guest close then final capsule release closes it; no resource restore/replay claim`. Source/native/official WAT qualification remains absent until keeper evidence. Original stdio ownership, mutable text, canonical owner/refusal/quotas/registry capacity and historical DATA checks are retained from the genuine base. This does not prove kernel-offset/flags rollback, pipe/socket external-state recovery, persistent capsule rehydration or new-world permission.

The final status failure must specifically match FastIO's typed portable POSIX
`EBADF` equivalence, including native Win32/NT invalid-handle mapping; arbitrary
I/O failure or interruption is not a close oracle. There are no OS opens between
the before/after lifetime probes. If a concurrently reused descriptor makes the
final probe succeed, the test fails rather than claiming a close. Native results
remain required; source inspection cannot substitute for this lifetime test.

Stdio oracles follow the original initializer, without changing it: on native
platforms where the original initializer uses `io_dup`, the three stdio rows
are owned files and the capsule retains their existing RC, with zero additional
observer duplicates. Its original fallback observer platforms require three
actual capsule duplicates and retain observer metadata. The CPP asserts both
that exact count and all three descriptor kinds under the same original platform
condition. Owned-preopen lifecycle remains unsupported on its original legacy
provider branch and is never counted as a native owned-resource PASS.

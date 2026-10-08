# WASIp1 private dispatch workers — R30

Both repositories pass the four native OS matrix: **48 program runs and
14,472 counted assertions**, plus **8 Linux complete-instance/preload regressions**
and **2 Linux memory-binding unit runs / 20 assertions**.

| Repository | Native OS | WASIp1 runs | Counted assertions |
|---|---|---:|---:|
| uwvm2 | linux | 6 | 1814 |
| uwvm2 | windows | 6 | 1794 |
| uwvm2 | freebsd | 6 | 1814 |
| uwvm2 | macos | 6 | 1814 |
| uwvm2-ros | linux | 6 | 1814 |
| uwvm2-ros | windows | 6 | 1794 |
| uwvm2-ros | freebsd | 6 | 1814 |
| uwvm2-ros | macos | 6 | 1814 |

## Implemented behavior

Private preparation creates two genuine `fast_io::native_thread` workers. Each
visits every import-visible module and uses the actual scoped environment and
memory binding. It checks explicit null masking, nested environment/module
switching and restoration. For shared environments with different module
memories, the workers hold both TLS scopes concurrently until all arrivals have
been inspected. The full cohort is released, scope restoration is checked and
both native workers join before private candidate destruction.

`maximum_private_wasip1_workers` defaults to 2. A smaller bound returns
`wasip1_preparation_declined` / `resource_limit` before native thread creation.
An unknown custom memory resolver returns `unsupported_resource`; a spy in the
real fixture confirms zero callback invocations. Failure results keep the
dispatch verification fields zero/false, including the later root-stage refusal.

Successful preparation reports `verified_wasip1_dispatch_workers`,
`verified_wasip1_dispatch_module_visits` and `wasip1_dispatch_tls_restored`.
The main success result in every run certifies two joined workers: **96 primary
verified workers / 160 primary module visits** across this matrix. Further
positive preparations inside the fixtures are not included in those totals.

Checkpoint/alias runs check one actual module; environment-group runs check
three actual modules, two genuine environments and one shared environment alias.
Both LLVM exception policies (`instruction`, `unwind`) run on each native OS.
Windows/FreeBSD use QEMU with KVM and read-only base disks; Linux and all cross
compilation run in the original 64 GiB cgroup, swap disabled. Native macOS is
arm64 under the 2 GiB aggregate controller/test bound and a verified no-fork
profile. Its largest recorded upper bound is 348045312 bytes (331.92 MiB).
C++ thread and I/O operations use FastIO.

## Source and storage qualification

The immutable integration cut contains 8771 files. Its manifest SHA-256 is
`76daefa677abdd3903f24233aa355e2c78240df867d91a2fc6a34ded5ce92848` and source archive SHA-256 is
`bcbfba7bfe80b841676d35c9dbbd294ac00497faab9ccad1432ac6cddf54a1e9`. Source dependencies, objects, binaries, SDK inputs,
native logs and guard receipts are verified. Concurrent peers' root-worker and
per-call WASIp1 resolver changes are preserved. At delivery, 2 of
the 12 relevant workspace source paths differ from this cut; all R30 additions
remain present. Later workspace edits are outside the measured qualification.

The Linux hot filesystem is bounded at 8 GiB, with 6 GiB admission and 7 GiB
stopping thresholds. Closing allocated usage is 5781237760
bytes (5.384 GiB); volume free space is
2665099264 bytes (2.482 GiB).
Existing Linux cold evidence retains its checked 2 GiB cap. A Windows VM
admission was refused before the VM or its test child started when hot allocated usage
reached 6,617,608,192 bytes. Two own R29 Linux product archives (272,601,225
bytes) were fully recovered in the remaining space of the prior local 1 GiB
region, then retired by a dedicated cleanup-only cgroup guard under the existing
7 GiB running stop. The 6 GiB test admission, 7 GiB stopping threshold, memory
limits and all other reserves remain unchanged. The prior six archives and
their original immutable qualification are preserved; the extension has separate
qualification and retirement receipts.

The ROS native macOS admission also refused to launch while free space was
below its unchanged 4 GiB reserve. Two already-passing, archived uwvm2 macOS
duplicates (562,957,904 bytes) were removed after complete archive readback.
Their source, test receipts and recoverable compiled products remain available.

All eight R30 compiled-product archives have whole-file and every-payload
readback qualification in `/Users/liyinan/Documents/MacroModel/wasip1-r30-owned-cold-evidence`: 771387484 bytes / checked 1 GiB cap.
Together with the prior local cold region, 1814183676 bytes /
checked 2 GiB cap. Only this task's matching remote archive copies and raw
products are retired after authenticated cgroup verification and local recovery
qualification. Source, SDK evidence, logs and receipts remain available. Cold
caps are checked limits, not separate physical filesystem quotas.

## Remaining work

Complete joint world restore/publication and restored guest replay remain
unfinished. These are finite private preparation workers and do not issue a
restored execution credential. Actual old-world worker retirement, installation
of new execution workers/roots, and atomic publication of source, stores,
engines, generation and the private WASIp1 cache still need integration and
end-to-end restoration tests. This round does not change the portable WASIp1
wire format or claim new directed OS migrations. A Wasm checkpoint still
requires simultaneous WASIp1 capture at the same cooperative stop to include
FD/environment state; that reminder remains in the tested runtime logs.

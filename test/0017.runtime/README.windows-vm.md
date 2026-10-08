# Windows VM qualification

Use the existing Windows image only with its original boot controller. Before
starting QEMU, run `verify_windows_vm_disk_model.py` against the authoritative
image launcher and the proposed guest launcher. It requires one
`virtio-scsi-pci` controller with one `scsi-hd` boot disk attached to that
controller and records the launcher and fresh overlay SHA-256 values. Pass the
expected overlay digest from the overlay preparation manifest. The check
rejects NVMe: the same image reached WinRE with `INACCESSIBLE_BOOT_DEVICE`
when an older test launcher substituted an NVMe device.

```sh
python3 test/0017.runtime/verify_windows_vm_disk_model.py \
  --reference-launcher /path/to/original/start-windows-x64.sh \
  --candidate-launcher /path/to/qualified/qemu-launcher.sh \
  --guest-overlay /path/to/fresh/disk.qcow2 \
  --expected-overlay-sha256 FROZEN_LOWERCASE_SHA256 \
  --output /path/to/qualification/disk-model.json
```

Keep the VM inside the shared 64 GiB, swap-free test cgroup and record its
live QEMU threads with `record_windows_vm_cgroup.py`. A PE cross-build or a
WinRE boot does not count as a Win11 guest execution result; retain guest
stdout, stderr, exit codes, test fixture hashes, and the executable's embedded
source ID for each accepted run.

Before making an ISO or starting the guest, inspect both linked PE import
tables. Bind `uwvm.exe` to its embedded source ID and the broker to its build
manifest SHA-256. A PE that imports `libgcc_s_seh-1.dll` cannot run from an
artifact set without that exact runtime; the qualification gate rejects it.

```sh
python3 test/0017.runtime/verify_windows_pe_imports.py \
  --pe /path/to/uwvm.exe --source-id sha256:SOURCE_FINGERPRINT \
  --output /path/to/qualification/uwvm-imports.json
python3 test/0017.runtime/verify_windows_pe_imports.py \
  --pe /path/to/uwvm-debug-server.exe --expected-sha256 BROKER_SHA256 \
  --output /path/to/qualification/broker-imports.json
```

When TCG makes HTTP transfer of the large PE impractical, mount the frozen
artifact directory as a read-only ISO. The Core 3 and debugger PowerShell
runners accept `-ArtifactRoot D:\artifact-subdirectory`: they verify the
qualification and each file SHA-256, execute the PE from the ISO, copy only
small fixtures to the guest work directory, and post the result to the same
host artifact server. Record the ISO SHA-256 and its file manifest alongside
the PE/build/guest result. A loader exit such as `0xC0000135` happens before
Wasm execution and must be reported separately from a Wasm semantic failure.

For a short Win64 SEH qualification before the full debugger matrix, stage
`uwvm.exe`, `eh-cross-function-catch-ref.wasm`,
`eh-cross-function-win64-seh.wasm`, and
`run_windows_cross_eh_smoke_vm.ps1`. Its `qualification.json` contains schema
`1`, the exact `source_id`, `product_sha256`, `build_sha256`,
`runner_sha256`, independent `oracle_sha256`, and `files_sha256` for the PE
and both Wasm modules. Run the script with `-ArtifactRoot` and its
qualification SHA-256. Verify the posted ten-case result using
`verify_windows_cross_eh_smoke.py` before accepting instruction/unwind EH.

Schema `2` extends this qualification with `repository` (`ordinary` or
`ros`), `multi_object_required=true`, and
`eh-multi-object-win64-seh.wasm` in `files_sha256`. The new fixture has
three independent cross-function handlers, including a GC reference payload.
The guest runs it with `-Rct 1` under both stack policies, first uncached,
then with an empty private cache, then by loading that cache. All six rows
must report at least two actually loaded objects, finalized native EH and
no body fallback; cache-load rows must contain a real object-cache hit.
This gives 16 guest cases including the existing feature-off controls.

Mirror all `<case>.stdout.log` and `<case>.stderr.log` files from the guest
evidence directory, then pass `--raw-log-root` to the Python verifier.
Schema 2 requires these actual bytes and verifies their recorded lengths
and hashes before inspecting the compiler receipts. A successful exit or
a claimed object count alone is insufficient. The same qualification applies
to both products; their PowerShell runners select their respective full-JIT
CLI entry. Schema 1 remains readable for historical ten-case evidence.

Stage a current product with `stage_windows_cross_eh_smoke_vm.py --schema 2
--repository ordinary` (or `ros`), `--product`, `--build-receipt`,
`--uwvm-imports`, `--oracle-manifest`, `--runner`, and a fresh `--output`.
The build receipt records `passed=true`, the repository, identical before/after
`source_id`/`source_id_after`, target `x86_64-w64-windows-gnu`, exact
`product_sha256`, zero before/after `oom` and `oom_kill`, and actual
`cgroup_memory_max=68719476736`, `cgroup_swap_max=0`,
`cgroup_cpuset=0,2,4,6,16-31` (limit fields are strings). It must derive from
the real current cross-build; staging is not a compilation receipt.
The current oracle status is
`wasm-tools-parse-validate-and-wasmtime49-outcomes-passed`, with all three
fixtures independently parsed, validated and executed. Historical schema 1
continues to require its original static-libgcc link/broker and Wasmtime 48
receipts. The two schemas do not substitute for one another.

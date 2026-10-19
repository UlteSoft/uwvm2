# WASIp1 directory-page boundaries

`partial_dirent.cc` invokes the actual production wasm32/wasm64 API with a
two-page native linear memory. It is a small native unit, with no LLVM/UWVM
backend or guest startup. The Linux-only driver compiles each width against
each repository, serially, using exact before/candidate header overlays; it
does not replace the original arena sources.

Forty private files have 125-byte names. Regardless of directory order, the
synthetic `.`/`..` records take 51 bytes, then 27 normal records take 4023
bytes. Thus a 4096-byte page has a genuine 22-byte partial header at byte4074.
The baseline must emit exactly `EXPECTED_FALSE_EOF used=4074 capacity=4096
complete=29 total=42 remaining_header=22` and exit42. Arbitrary crashes or
padding differences cannot qualify that negative control.

Candidate checks cover every header/name prefix for `.`/`..`/normal/last
normal entries, zero/exact/full buffers, tails1..23 after a complete record,
same-cookie full retries, EOF and beyond-end cookies, complete libc-style
pagination, explicit LE fields/zero padding, both guards of data/result,
and valid partial buffers ending exactly at linear-memory end. A separate
owned subprocess requests one byte beyond memory end and must reach the
production explicit trap; its intentional signal and core0 are retained.
Both widths execute on the 64-bit Linux host; this is not a narrower-host
portability proof.

The Preview1 contract permits a truncated last entry and treats
`bufused < buflen` as EOF. Consumers advance their cookie only after a complete
entry. Sources: [WASI snapshot-01](https://raw.githubusercontent.com/WebAssembly/WASI/snapshot-01/phases/snapshot/docs.md),
[pinned wasi-libc readdir](https://github.com/WebAssembly/wasi-libc/blob/b9ef79d7dbd47c6c5bafdae760823467c2f60b70/libc-bottom-half/cloudlibc/src/libc/dirent/readdir.c).

After coordinator review/release, copy only this test directory and the
current `owned_process.py` into the arena test copy. Run in the original
container, CPU5; the driver requires the exact existing64GiB/swap0 cgroup,
core0, a 512MiB disk reserve, owned aggregate RSS below2GiB and shared anon
below50GiB. Crossing a guard stops only owned pidfds and leaves failed
evidence. These are polling guards, not a newly imposed resource limit.

```sh
ssh linux 'docker exec -i uwvm-debug-core3-v48-20261011 chroot /host taskset -c 5 /usr/bin/python3 -u /tmp/high-level-20261017/uwvm2-ros/test/0050.high_level_languages/wasi-readdir/run_units.py --arena /tmp/high-level-20261017 --sdk /home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm --out /tmp/high-level-20261017/final/fd-readdir-boundaries-r1'
```

The output directory must not already exist. Expected outcomes are six
specific baseline witnesses, six candidate boundary passes and six explicit
OOB traps, plus12 compile receipts. Hashes/commands/raw logs/owned retirement,
before/after admission, failures and a synced final receipt are retained.
No old source, SDK, products, or unrelated artifacts are cleaned up.
Full stock-Python guest validation requires separately rebuilt products and
is outside this small-unit qualification.

`qualify_units.py --run RUN --out NEW` is an offline-only second qualification
on CPU5. It requires the exact production OOB diagnostic, complete unique
matrix, all raw log hashes, unchanged inputs, matching boot/cgroup/events and
absence of every retained owned process identity. See `RESULTS.md` for the
actual completed run; the executed r1 sources are preserved in `evidence-r1`.

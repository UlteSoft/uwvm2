# Verified Linux evidence archive

`archive_evidence.py` is an archive and verification tool. It never deletes
source evidence, SDKs, VM products, repositories, or other existing artifacts.
Do not run it during a build, benchmark, debugger test, or collection: selected
inputs must remain quiescent through the final second inventory.

Run the script only through the original SSH Linux container and its existing
64-GiB cgroup. It imports the adjacent `build_products.py` admission helper and
also checks the exact original cgroup membership and unchanged boot identity.
It does not create a cgroup or change cgroup limits. The shared admission helper
disables core dumps for this invocation. The output parent must already exist
and have at least 512 MiB free.

Choose every input explicitly with repeated `--include`, relative to `--arena`.
There is no default selection. Absolute paths, `..`, empty components, symlink
directory ancestors and special files are rejected. Overlapping selections are
deduplicated by their within-arena member names. A selected symlink is archived
as a link, including its original target; its target is never traversed. Links
may therefore still point outside a future extraction directory. Parent
directories outside the selected paths are not added as evidence entries.

The following is a command template; replace the input selections and output
with the final evidence paths. Selecting `other/artifacts` does not implicitly
select reports elsewhere in the arena. Include receipts, logs, manifests,
source maps, producer sources, product identities and raw VTune result
directories deliberately when they are needed for the final record.

```sh
ssh linux 'docker exec uwvm-debug-core3-v48-20261011 chroot /host \
  /usr/bin/python3 /tmp/high-level-20261017/uwvm2-ros/test/0050.high_level_languages/archive_evidence.py \
  --arena /tmp/high-level-20261017 \
  --include REPLACE_WITH_RESULT_DIRECTORY \
  --include REPLACE_WITH_OTHER_RESULT_DIRECTORY \
  --out /home/macromodel/Documents/REPLACE_WITH_NEW_ARCHIVE_NAME.tar.gz'
```

The output must be outside every selected directory. Its archive, receipt,
inventory directory and tar log must all have fresh names. Existing evidence
is never overwritten. The inventory preserves original relative member names,
file SHA-256 and size, permissions, numeric ownership, modification times, and
source identity metadata. Hard-linked input files are stored as ordinary file
contents using GNU tar `--hard-dereference`. `ARCHIVE-INVENTORY.json` is reserved
as the inventory member in the archive.

GNU tar uses NUL filelists with `--verbatim-files-from`, `--no-recursion`, pax
metadata and gzip compression. Only the inventoried members are requested.
Python subsequently reads the gzip archive in streaming `r|gz` mode, rejects
duplicate/unexpected/missing members, and checks every member's type, size,
permissions, ownership, modification time, link target and regular-file bytes.
An additional bounded gzip read to EOF checks the trailer's CRC and ISIZE;
compressed-archive and uncompressed-tar SHA-256 are retained. A second complete
input inventory checks hashes and identity metadata again.
The verified temporary archive is published atomically without overwriting an
existing output. `passed: true` is published only after these checks, unchanged
boot/admission, and archive/directory fsync.

Progress is printed during hash, archive, verify and rehash phases. While its
own tar process is active, the script checks output-filesystem free space every
250 ms. Below 512 MiB it stops only that owned process group and removes only
its partial archive and temporary filelists. Inventory, tar log and failure
receipt remain available. It does not reserve disk space against other writers
or implement an atomic filesystem snapshot; quiescent inputs and available
space remain necessary. Inventory metadata is retained in memory. ACLs and
extended attributes are outside this verification contract; this is an
evidence-content archive rather than a complete filesystem backup. Atomic
publication uses a same-directory hard link and therefore requires a filesystem
that supports ordinary hard links.

The retained outputs are:

- `NAME.tar.gz`: published only after successful archive/input verification.
- `NAME.tar.gz.receipt.json`: final `passed` state, archive SHA/size and checks.
- `NAME.tar.gz.inventory/ARCHIVE-INVENTORY.json`: independently inspectable inventory.
- `NAME.tar.gz.tar.log`: GNU tar diagnostics, including changed-file failures.

Do not clean source artifacts merely because the archive file exists. Require
the matching receipt's `passed: true`, retain the inventory/receipt with the
archive, and review whether the explicit includes cover everything that must
survive. Source deletion is a separate, deliberate operation; this tool has no
cleanup option. Extract only into a suitable separate directory, taking account
of preserved symlink targets and ownership metadata.

The 2026-10-17 Linux functional probe passed in the original container. It
covered nested/empty directories, newline/option-like/Unicode filenames,
symlink files/directories without traversal, hardlinks stored as regular
contents, and overlapping includes. All 11 members were stream-verified and
the input inventory rehash passed; the archive is 1733 bytes. The passed
receipt and archive were retained before removing only that driver's tiny
probe input directory. See
[probe validation](engines/gated/evidence-r1/archive-probe-validation.json) and
[the retained archive receipt](engines/gated/evidence-r1/archive-probe-r1.tar.gz.receipt.json).
This verifies the small functional path; no low-disk fault injection or final
large evidence archive has been attempted.

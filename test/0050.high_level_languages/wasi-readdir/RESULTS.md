# Linux boundary qualification

The original existing64GiB/swap0 Docker cgroup, CPU5, boot
`33464c92-7dae-4410-b9f7-92e3edf249b9` completed the serial native units.
No UWVM guest, benchmark, VTune collector or large runtime was started.

| Evidence class | Result |
| --- | --- |
| Before/candidate native compilations | 12/12 exit0 |
| Original premature-EOF witnesses | 6/6 exact4074/4096,29/42,22-byte-header-tail; expected exit42 |
| Candidate wasm32/64 across3repositories | 6/6 exit0;119013 assertions each |
| Intentional OOB subprocesses | 6/6 SIGILL; exact production memory[0] boundary0x20000 diagnostic |

The OOB rows are intentional negative tests, separate from the six normal
positive runs. A signal alone was not accepted by the subsequent strict
offline qualifier. The regression checks names, inodes, types, LE fields,
zero padding, all partial headers/name prefixes, last-complete-cookie retry,
EOF/beyond-end, adjacent guards, exact memory end and complete pagination.
The original failure witness does not depend on zero padding or an arbitrary
crash.

Linux receipts remain at `arena/final/fd-readdir-boundaries-r1/receipt.json`
(SHA256`87fcd53f84935cfcf2d614d010a806a8c40566335fe5377ce3e828d608d99a1d`)
and `arena/final/fd-readdir-boundaries-strict-qualification-r1/receipt.json`
(SHA256`785897cdd38d635ddce43e8545e6c665e45f16aec128cb0d53843b0894e9c48e`).
Here `arena` is `/tmp/high-level-20261017`, resolving to the persistent arena.
All92 small receipt/log files were copied into `evidence-r1` with exact
remote SHA256/size verification; tested script/source copies are retained.
Native binaries and fixtures remain on Linux for the final artifact archive.

All retained owned identities were absent after retirement; all command
ownership receipts are closed and leaders reaped. Memory events stayed
unchanged with OOM0. Observed owned peak RSS was753946624bytes, below2GiB;
the shared-anon50GiB and disk512MiB guards did not fire. Every original arena
header still matched its beforeSHA; candidates were tested via overlays.
The strict qualifier rehashed every recorded input/tested binary and raw log.

The six production candidate files are byte-identical by width across the
repositories, as were their original inputs. `candidate-files.json` records
each before/afterSHA. Native Linux units are not full stock-Python startup
qualification, a wasm64 guest execution test or a narrower-host proof.
Those require separately rebuilt products. The executed r1 driver remains
unchanged. Its cleanup-error path can raise before a per-command receipt is
saved; none occurred here, and the final/strict receipts prove complete
retirement for this actual run. Future driver revisions should preserve
cleanup failures before raising and use only owned file/directory fsyncs.

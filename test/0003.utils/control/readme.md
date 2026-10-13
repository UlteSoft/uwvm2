# Control session and Linux launch-channel checks

Run `python3 test/0003.utils/control/run_session.py --out <new-directory>` inside
the authorized SSH Linux cgroup. The runner rejects an unrestricted environment.
It builds/runs O3, ASan+UBSan with leak checking, and no-C++-exceptions profiles,
then precompiles the protocol/session/Linux-channel/impl modules and links/runs an
importing consumer. It does not link the VM or claim any debugger/patch operation.

Both repositories passed the r217 portable session tests with 9,062 checks in
each profile. The r2 Linux test passed ten packet scenarios and creator death in
each profile. The final r3 test overlay added ancillary truncation cleanup for 64
passed descriptors and actual exec closure: both repositories passed all eleven
packet scenarios, creator death and exec closure in all three profiles, with
127 parent assertions per profile plus enforced child assertions. The production
headers are unchanged between r2 and r3. Both scoped module dependency audits and
real module consumers passed.

The tests include an actual inherited same-UID sender FD, an attempted forged PID
rejected by the kernel with EPERM, an injection from the VM process itself, an
empty permit despite a valid launch peer, unexpected/oversized/truncated/replayed
messages, and owner destruction. A pidfd for the original launcher rejects its
death even if a correctly formed packet was already queued. The receive path
closes delivered SCM_RIGHTS before rejecting ancillary messages; the test counts
real open descriptors to catch leaks.

The earlier session-only archive is
`build/wasm3-evidence/host-control-session-r217-r1.tar.gz`, SHA256
`bd006b65582a9a6b4716a7ec5a7d33bb6e136b85eaf2b91280708654566c28e2`
(120 individually verified files).
The complete Linux transport/module tests and final test overlay are in
`build/wasm3-evidence/host-control-linux-transport-r217-r2-r3.tar.gz`, SHA256
`10970c457e85213aa544b26498c3aa8e49ff5d97fc9e3ed3f13bf4eff29c7cb9`
(215 individually verified files; 61,568,194 uncompressed bytes).

The Linux transport is currently a pre-fork embedding launch channel. Tests do
not establish CLI exec bootstrapping, a late-attach filesystem/TCP server, runtime
pause/resume, instruction stepping, or function replacement. Those require
separate integration and execution evidence. The portable `authenticated_launch_peer`
type alone is not evidence of OS authentication; only the concrete Linux adapter
uses kernel credentials and the retained process handle.

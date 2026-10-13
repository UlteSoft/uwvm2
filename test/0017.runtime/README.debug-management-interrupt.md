# Synchronous debugger keyboard interruption (SOURCE only)

This proposal adds a bounded trusted host flag query to controller.execute.
It never runs controller code from a signal callback and never starts a
recursive control-stream request. wait/source stepping poll the flag in 20ms
management wait slices, cancel automatic source stepping, and request normal
cooperative pause. A timeout or incomplete participant park cannot create a stop.

Native hooks use the frozen NI r2 controller preimage SHA
0985860310f5cc54efb5000146b6a3863975e0786bce8d269dbaaf653a8cb09e.
They require that packet's actual observer try-lock/released ACK integration.
The native cancellation path retains the cooperative ticket while disabling
and retiring the real event/cursor/session. Its released ACK cannot itself be
canceled. Only a subsequent real all-participant Wasm park/capture mints a new
public stop lifetime. Failure keeps actual owners; no fake paused label, old VM
entry, early clear or forced guest join is introduced. An already pending flag
rechecks the actual original stop under controller and backend ownership.

ROOT must merge only the small management hunks into its current controller;
the NI preimage predates state/WASIp1/GC-member routes. Do not replace ROOT's
whole current controller with this candidate. All other hook preimages are the
actual current source including the applied Windows NOWAIT owner/DLL helper.
Module imports and exports cover management_wait_interrupt.h/.cppm.

The actual product PTY runner has mandatory successful build proof/source ID/
binary SHA and original 64GiB/swap0 cgroup pins. It checks real running/idle
Ctrl+C, same-UID external SIGINT DURING execute(wait), fresh captured stop,
<1.5s response, history/repeat/terminal restoration and self guest proc_raise
DFL termination in instruction/unwind. Each process has a finite 20s ceiling.
It does not claim native NI cancellation, source into/over/out, DAP or full VM
retirement/finalizers are tested. Those need current exact full-product cases.

No native compile/run was performed for this source proposal. macOS native
execution is forbidden here. No existing WASIp1 syscall or memory hot path is
modified. The callback is a synchronous host borrow and cannot be installed by
Wasm or copied into persisted checkpoint/control-wire data.

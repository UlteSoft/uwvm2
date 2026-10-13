# LLVM full observer, preparation and owning guest stacks

The `llvm_debug_observer.cc` fixture uses the actual runtime object and generated
LLVM code. `run_llvm_debug_observer.py` links it using the recorded runtime flags;
run only inside the SSH Linux test cgroup with a matching frozen source tree and
runtime build. The observer and compiler option layouts must match that runtime.

Both ordinary r220 and ROS r220b passed instruction/unwind/none policies, nine
executed instruction stops per process. This covers `try_table`, a nested `throw`,
the handler continuation and the final arithmetic result 74. At the throwing
callee the owning guest stack is exactly `[callee, caller]`; after catching it is
exactly `[caller]`. `none` reports no stack, and management-thread stack capture is
always rejected. Preparation compiles the complete registry but calls no guest
warm function or marker import and creates no fictional guest participant/PC.
The configured observer retains its context until reset drains execution; copied
stack/name snapshots remain valid after that context and code registry retire.

Source identities (SHA-256):

- ordinary r220: 48e1a9c3d097b91885fb9274aaf42bc7541c089d004cd8e3723e8f808eb4fc9d
- ROS r220b: b1e94415b224e909809eac095f70bcfd0846c98a133320ed62c6d1f7d5991263

The initial r219 build rejected a cache-key ternary that decayed string literals
to a pointer; r220 uses explicit bounded string views. The first ROS r220 build
rejected the ordinary product's bridge-depth accessor name; r220b uses ROS's own
accessor. Both failed builds remain recorded; neither is a runtime qualification.

The later r221 fixture captures in `on_before_park`, not `on_safe_point`. The
pause domain calls this cold hook after observing a pause request and before
acquiring its mutex. Therefore a request arriving just after the ordinary
observer returns cannot park that poll without crossing the capture hook. The
manager still waits for the matching pause ticket before reading its snapshot.
The runtime rejects reset, preparation and configuration from either callback.
Both r221 products passed the same three actual-VM policies with this cold hook.
The separate pause-domain regression exercises the deterministic
pause-after-observer window, zero capture on the running path, and real module
consumption, with O3 and ASan/UBSan/leak checks.

All actual runtime/fixture builds here are host O1 functional qualification.
They do not measure enabled-debugger overhead or qualify a server, late attach,
local-variable recovery, retained exception references or function replacement.
The compiler's separate instruction/IR/object checks and default-disabled machine
code comparisons are in `documents/runtime/llvm-debug-safe-point-codegen.md`.

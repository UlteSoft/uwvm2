# PPC64 ELFv1 loaded-function range component

The production helper reads the exact real ELF STT_FUNC descriptor symbol, its
first object relocation and exact body st_size. It never reads a native .opd
pointer, guesses size from the next symbol, grants code access or changes guest IR.
The actual relocation table is indexed once per loaded object, sorted by exact
section identity and offset, then searched in logarithmic time per descriptor;
duplicate keys decline. Scratch allocation remains under the real Hot14 pending
listener boundary. The former ordinary text-symbol path remains unchanged;
this path applies only
64-bit PPC ET_REL with ABI flag zero/one and an allocated PROGBITS .opd section.

Build `real_ppc64_elfv1_object_ranges.cc` with each configured product LLVM closure
in the original Linux 64 GiB cgroup. Assemble each source with the actual matching
`llvm-mc -triple=powerpc64-unknown-linux-gnu -filetype=obj` and with
`-triple=powerpc64le-unknown-linux-gnu`. Preserve actual ELF/symbol/relocation output
from llvm-readobj; `.abiversion` in the fixture intentionally pins legacy/one/two.
Run the component with each actual object and catalog `expect`. This allocates
actual object sections through RuntimeDyld and consumes only genuine LoadedObjectInfo.
It deliberately does not resolve/call foreign machine instructions, manufacture a
LoadedObjectInfo subclass, or publish a VM code permission. Big/little endian must
both be run. An absent PowerPC target/parser/loader closure is unavailable, not PASS.

The four positives cover descriptor-only symbol representation and signed
relocation addends; the five negatives cover descriptor truncation, absent entry
relocation, body overrun, data target and explicit ELFv2 mismatch. No native pointer
input or fake execution certificate appears. Fixed marker symbol size is an
independent real-object comparator; it is never supplied to runtime authorization.

Further qualification remains separate: generate a real ELFv1 object with LLVM
codegen, verify descriptor symbol size + entry relocations, then execute actual
uwvm2/ROS full debug-JIT in PPC64BE QEMU with instruction/unwind, normal/raw/resume
four genuine engine Function pointers and exact pending ownership. Hot replacement,
checkpoint generation, source/state ABI, loaded/unwind publication and ASM endpoint
permissions must retain their own real provenance checks. This component alone does
not prove a descriptor callable or platform debugger support.

All compiler/object parser/RuntimeDyld/endian/QEMU/native/performance outcomes are
unrun in this SOURCE-only packet. No actual collision/overflow/execution claim is
made. RuntimeDyld or LLVM fatal OOM is not claimed recoverable. ROOT applies only
narrow current hunks after immutable Hot14R2 prerequisite, never an old whole leaf.

The RISC-V split-probe CFI regression
====================================

Real full-JIT Wasm functions with 10000 locals could reach an opcode breakpoint
and then abort on managed quit. Both instruction and unwind call-stack policies
failed in both products. The shutdown signal was thrown by the genuine runtime
poll, but GNU _Unwind_RaiseException returned _URC_END_OF_STACK (5) before the
outer C++ catch. This was not a failure of the catch's ownership checks.

RISCVFrameLowering::allocateStack receives Offset for the current allocation
and RealStackSize for the full frame. A large probe loop used Offset as the CFA
offset and omitted the earlier CSR-save allocation. In the captured full Wasm
function, that earlier part was 496 bytes: SP needed a CFA offset of 203584,
while the registered FDE advertised 203088. The temporary probe-loop register
also omitted the same 496 bytes. Unwinding consequently read the wrong RA slot.

The fix includes RealStackSize - Offset in the temporary-register CFA, then
uses RealStackSize after the residual allocation. Unrolled probes already used
the complete size. Stack probing remains enabled; no frame pointer workaround
or generated execution instruction was added. The ROS backend source is fixed;
ordinary uwvm2 carries the same external-LLVM patch in:
tools/ci/patches/llvm23-linux-riscv-split-probe-cfi.patch

For an external LLVM source checkout, first use git apply --check with that
patch from the checkout root, apply it, then rebuild LLVMRISCVCodeGen and relink
the VM. Reusing an old archive preserves the bug even if its version string is
23.1.1-uwvm-ros.11. The paired cross-build recipes now reject the known bad
source shape; their check is not binary or execution qualification.

The source SHA-256 changes from
176b8ad5d1188832d6ce5e801c27019db6f0a2e208733279e0c85382aac2395b to
2a1f132a511474b23adda753869ce4a0aa900c4f59526f800668a6c6aa40438a.

Regression drivers
------------------

run_wasm_managed_quit_cli.py uses the actual full VM, authentic Wasm opcode
breakpoints, and a strict exit-code-0 oracle. See
[the managed-quit driver notes](../0018.debugger/README.wasm-managed-quit.md)
for its QEMU and ROS options.

native_mcjit_foreign_cleanup.cc is a private developer diagnostic, not a public
ASM command. Compile it with the actual target-host LLVM SDK and each product's
headers. Run every invocation in the verified Linux test cgroup:
  PROBE small OUTPUT-small.o
  PROBE large-frame OUTPUT-large.o
  PROBE long-code OUTPUT-long.o
  PROBE large-probed-frame-fp OUTPUT-probed-fp.o
  PROBE large-probed-frame OUTPUT-probed.o

The last two cases add 2048-byte stack probes to the same large recursive
function. Only the fp control forces a frame pointer. Each successful case
requires a real native backtrace with three recursive frames and one root,
four actual landing-pad cleanups, and the foreign signal caught by C++.
The unprobed and fp controls alone cannot qualify the large no-fp probe loop.

Before the fix, the no-fp probe case crashed in both products; the other
controls passed. After the fix all five cases pass in both products. Eight
before/after object comparisons show identical .text bytes; only the no-fp
large-probe .eh_frame changes. Public ASM remains constrained to actual Wasm
code owners; the external GDB diagnosis never grants the product access to VM
implementation code. These regressions do not qualify every architecture,
all concurrent thread behaviors, or checkpoint restoration.

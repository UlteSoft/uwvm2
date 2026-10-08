Native instruction display containment

Both disassembly commands use one authenticated complete current-function copy
(at most 64 KiB), one text decoder and one same-target semantic decoder. Public
output remains bounded to 32 instructions. Plain output disables symbol naming;
the authenticated function bounds remain available for mandatory containment.

The public projection verifies complete source bytes against that private copy
and its guest provenance. Unknown decoding, memory/address operands, implicit or
indirect transfers, calls, returns, system operations, stack/frame operands and
unproved address-sized immediates are unavailable in DTOs and console/TUI/DAP.
Private execution selection keeps its separate policy and immutable full image.

An admitted direct branch requires an exact decoded target boundary inside the
same function with guest provenance for the complete target instruction. Outside,
zero, scaffold and unaligned targets suppress the whole source row; bytes and
text would otherwise disclose the displacement. resolveSymbols=false never
disables containment. Annotation printing independently checks ownership before
numeric output. Visible instruction byte/text arrays have zeroed unused tails.

The projection covers range/plain display and executed/refused si/ni replies.
Stale stop, participant, function generation, epoch, replacement and reset checks
remain in the private runtime/controller path. Missing proof withholds display.

Verification: native_public_instruction_projection.cc checks real MC synthetic
DATA, alternate operand/control forms, provenance and array tails;
native_branch_display_formatter.cc checks production formatting defenses;
debug_native_branch_display_runtime.cc/.wat checks isolation/reset at a genuine
trap under instruction/unwind policies. Positive branch-display qualification
requires a visible current-owner branch; missing provenance reports exit 77 after
the isolation checks, rather than claiming that DATA proves runtime coverage.
Module producer/consumer checks remain separate. test_dap_native_public_projection.py and the native-window
suite cover production origin headers and explicit unavailable current rows.

Builds/tests run only in the original constrained SSH Linux cgroup. Display grants
no memory, breakpoint or execution authority and qualifies no other OS, architecture,
libc or missing native-continuation backend.

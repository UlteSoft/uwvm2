# Composable debugger windows

Start with `uwvm -Rdbg app.wasm`. `tui enable` enters the full-screen view;
`tui disable` restores the ordinary command prompt. `run`, `start`, `startw`,
and `starti` preserve the editor, history, colors, and TUI state while retiring
the previous guest and loading fresh Wasm memories and WASIp1 state.

```text
startw
layout wasm locals stack memory
focus stack
layout horizontal src asm
layout wasip1 io
layout src
layout wasm
layout asm
```

`layout` accepts up to eight panes in the requested order. `horizontal` selects
columns; the default arrangement uses rows. Panes are `src`, `wasm`, `asm`,
`regs` (also `register` or `registers`), `locals`, `stack` (also `operands`),
`memory`, `wasip1`, and `io`.
The register pane shows the current native PC first, followed by available
numeric register values with distinct name/type/value colors. Missing mappings
are omitted; unknown high bits remain `??`. At a source/Wasm stop the pane
clears previous values and suggests `si`/`ni`. Selecting a layout does not step.
`focus PANE`, Page Up and Page Down
select and scroll individual panes; a focused pane remains accessible on a
small terminal. `memory MODULE MEMORY OFFSET COUNT` selects 1–256 bytes.

The Wasm pane uses the compiler's current instruction boundaries and displays
the module filename, byte offsets, instruction names, immediate operands and symbolic function/global names. C++/Rust symbols use LLVM
demangling when supported; imports/exports and embedded DWARF provide fallbacks.
Undecoded operands retain their encoded bytes. Instruction mnemonics, registers,
numbers and symbols have distinct colours. Locals and stack display only
`type [index] = value`; an intermediate native stop keeps a short snapshot label.
Locals, operand stack and memory are separate live queries.
Unavailable state is reported explicitly, rather than reusing a prior stop.
Source `=>` marks a source statement stop; `N>` marks an intermediate Wasm or
native position within that statement. The asm pane marks the authenticated
JIT resume PC with `=>`, both at a physical native stop and at a cooperative
Wasm stop. Register capture and PC relocation still require an actual native
trap with compatible captured state. Native code is copied only from the
authenticated current JIT function; rendering never follows an arbitrary
address into host libraries.

Guest stdout/stderr are captured in a small IO window while the TUI is enabled.
`layout io` expands it. `input TEXT` queues a newline-terminated guest input;
bare `input` queues a blank line. An empty queue reads as EOF. The guest cannot consume debugger command keystrokes.
With the TUI disabled, guest output is mixed with command output normally.
Queues and history are bounded. Unconsumed input belongs to the debugger;
WASIp1 descriptors and guest memory are recreated at restart.

The WASIp1 descriptor view queries the retained live OS handle for its current
path (Linux `/proc/self/fd`, macOS `F_GETPATH`, Windows
`GetFinalPathNameByHandleW`). It does not reuse the creation pathname. A failed
or inconsistent query reports an unavailable path. As with any live OS query,
a subsequent rename can change the path after the observation.

`bs` and `bw` complete loaded module IDs, including after leading or repeated
spaces. `bi` takes a verified instruction offset within the current JIT function.
Source/Wasm/native step families have explicit level forms:
`step src|wasm|asm`, `next src|wasm|asm`, and `finish src|wasm|asm`, with
`ss/ns/fs`, `sw/nw/fw`, and `si/ni/fi` aliases. Native operations retain their
platform and control-flow admission checks; a command's presence does not
grant permission to leave JIT-owned code. Help and Tab completion list these
same forms. Outside the TUI, completion candidates append below the input in
the normal terminal scrollback. The TUI keeps its completion overlay.

The regression drivers use actual PTYs and a Windows ConPTY, sending keys,
Tab, execution commands and layout switches. `run_debug_composable_wasip1.py`
also renames an already open file, checks all three guest streams, tests source
markers, and restarts the WASI environment. Windows runs inside a private QEMU
overlay; Linux tests and the VM must be admitted to the existing 64 GiB,
zero-swap cgroup. Local product builds may use up to `xmake -j3`. Remote Linux and QEMU Windows
qualification of the latest UI changes is pending the restored Linux SDK build.
Do not treat a macOS result as Linux or Windows acceptance.

Source files use `fast_io::native_file_loader` mappings without an artificial size ceiling.
Only the visible source rows are copied into the pane.

Stock LLVM does not produce UWVM's native owner table. The build enables that
contract only for an identified compatible ROS LLVM SDK, so an ordinary LLVM
installation does not cause every debug module to fail JIT materialization.
Use a matching ROS SDK when native stepping is required.

## Pointer selection, nested layouts and saved configuration

Only the title label (`[src]`, `[memory]`, etc.) starts a window drag. Drop it
on another pane's left/right/top/bottom edge to dock there. Drag horizontally on the same title row to resize columns; moving below that
row starts docking. Shift-drag also resizes across pane boundaries. Body drags select UTF-8 text across lines;
release copies it through the terminal OSC 52 clipboard protocol (macOS also
uses the fixed `/usr/bin/pbcopy` executable, with the text passed through a pipe).
`selection range PANE ROW COLUMN ROW COLUMN` selects visible body cells starting
at 1; `selection copy|show|clear` provides the corresponding typed operations.

```text
layout tree v70(h50(src,memory),io)
layout move memory below src
window resize memory 12
layout config save /tmp/my layout.conf
layout config load /tmp/my layout.conf
```

Both `layout config save <filePath>` and `layout config load <filePath>` require
an explicit path. Omitting it reports usage and leaves the layout unchanged;
there is no default file. Tab completes paths, including from an empty path.

The bounded split tree stores only pane names, axes and exact ratios.
`layout horizontal src wasm asm` starts with equal thirds. Dragging or
`window resize PANE CELLS` preserves exact cell widths through save/load.
Source scrolling stops with the last real line at the top; ASM scrolling fills
the pane and stops at the first/last instruction of the same JIT function.
Fast wheel bursts are combined before refresh, without discarding input. Invalid files
leave the current layout unchanged. Pane sizes clamp to their minimums. A small
terminal temporarily shows fewer panes and restores the tree when enlarged.
Terminal size is checked while waiting for input; a resize clears the old frame
and redraws without requiring a key, preserving the edited command.

Memory rows show fixed-width Wasm addresses, byte hex and ASCII. Drag in either
byte column to select a range and inspect its interpretation. `memory format
TYPE` accepts `all char wc utf8 utf16 utf32 i8 u8 i16 u16 i32 u32 i64 u64 f32 f64`.
Wasm numbers and Unicode code units use little endian; `wc` means UTF-32.
`memory select OFFSET COUNT` uses the same copied bytes. New stops, restarts,
range changes and unavailable queries invalidate those bytes.

Hover source identifiers or JIT registers for a transient current-stop query;
Wasm value rows show their captured values, and local opcodes can query their
local index. Typed equivalents are `inspect src EXPR`, `inspect wasm local INDEX`,
`inspect wasm stack INDEX` and `inspect asm REGISTER`. No native memory address
is dereferenced. Moving away or changing the stop removes the hover.

`help wasip1 io` documents `io show`, `io clear`, `io save PATH`, `io input TEXT`
and `io follow on|off`. Captured records include their stream and local ISO 8601 timestamp
from `fast_io::local(fast_io::posix_clock_gettime(realtime))`.

`debug_rich_tui.cc` tests geometry, encodings and stale views.
`run_debug_rich_tui.py` exercises production PTY selection/copy, memory byte
hit-testing, nested docking, saved layouts, idle resize, IO and cross-level views.

The renderer composes styled cells in memory and sends only changed spans in a
single synchronized terminal update. Idle frames emit no bytes. Terminal resize
invalidates the full frame; typing, hover and selection never clear the screen.

Only the pane title starts a drag. The nearest target edge determines left/right/
above/below docking; Shift-title-drag resizes the immediate split. `layout add
wasm right src`, `layout add asm below wasm`, and `layout remove memory` provide
incremental keyboard composition. Explicit layouts survive all start/run commands.

Ctrl+C copies an active selection; otherwise it retains its interrupt behavior.
Ctrl+V in TUI pastes copied text (macOS reads the system clipboard). Line breaks
become spaces and a separate Enter is required. Native terminal paste also works.
Every nonempty submitted command enters history, including syntax and state errors.
Hover shows compact colored values; unavailable expressions do not create a popup.

## Local native movement and calls

Drag the two cells of an exact `=>` marker onto a code row in the same pane to
request `jump src LINE`, `jump wasm OFFSET`, or `jump asm *0xADDRESS`. Dragging
`N>` or ordinary body text remains text selection. A new stop, resize or keyboard
operation cancels an unfinished marker drag.

On macOS ARM64, native relocation authenticates the current kernel trap and
complete current JIT function. Both PCs must be complete decoded instructions;
the interval must preserve SP/FP/LR/TLS and contain no call, branch, return or
unmodelled effect. The kernel PC is written while the exception gate is closed
and read back before the new stop is published. Failed validation retains the
original PC and stop identifier. This does not undo memory or external I/O.
Source/Wasm movement requires a compatible operand/control-stack restore entry;
the current observation-only compilation has no such entry and rejects it.

`si` now distinguishes an authenticated typed Wasm callee from an opaque runtime
call on macOS ARM64. Guest calls land at the callee's first guest-instruction JIT
trap. `ni` uses the caller continuation. `fi` follows real activation-return events
and installs a fresh native trap at the caller's next guest instruction. It can
stop earlier at a breakpoint/catchpoint; root/unknown callers remain refused.
These transitions do not single-step or expose native runtime/host bodies.
`nw`/`fw` and source `n`/`finish` preserve their activation policy when leaving a
native stop, instead of always performing one Wasm step into a callee.

`native_step_macos_arm64.cpp` verifies the real Mach relocation gate and rejection
of stale/unaligned/out-of-owner writes. `run_debug_native_macos_interaction.py`
exercises production command and SGR mouse relocation, SI versus NI, finish,
activation retirement, Wasm handoff, and transactional failure through a PTY.

`run_debug_native_linux_interaction.py` exercises the Linux x86-64 perf backend
through more than four runtime continuations, SI/NI guest-call distinctions,
native finish and Wasm handoff. Completed perf breakpoint descriptors are retired
only after the matching trap; code and activation ownership remain until worker
acknowledgement. Disabled perf descriptors still occupy hardware breakpoint slots.

`run_debug_restart_materialization.py` repeatedly launches the product with named
and export-only modules and submits `starti`, `ni`, `startw`, `nw`, `starti`, `ni`.
It catches lifetime errors in private snapshot helper names as well as stale
restart state. LLVM Twine concatenations are materialized in the same full
expression while all child Twines are alive. Each iteration must produce real
stop replies and retire cleanly; it has a bounded deadline and retains transcripts.

On Windows, `debug_composable_windows.cc` runs against actual ConPTY, including
live handle paths after rename, explicit layout configuration files, physical
terminal resize, isolated IO and restarts. Pane assertions reconstruct painted
cells because ConPTY may omit unchanged title characters from its output.
`debug_native_windows_interaction.cc` uses the same owned-job harness to exercise
guest SI/NI calls, native finish, root-frame refusal and native-to-Wasm handoff.
Both were run inside the original 64 GiB Linux cgroup using a private Windows 11
QEMU overlay; the user's original disk was not changed.


## macOS debug runtime cost

The default macOS ARM64 session keeps the IR optimization pipeline disabled and
retains every Wasm safepoint and typed value capture. Native instruction
selection/register allocation uses the light backend for modules whose functions
have at most 4096 IR instructions. Larger modules retain the O0 backend because
register coalescing on their instrumentation can greatly increase startup time.
This budget only selects a compilation strategy: functions, instructions, source
text and captured values remain unrestricted by it. Explicit JIT policies retain
their precedence. Other native backends retain their existing default policy.

Each activation reuses its owned value storage. Flags, reference envelopes,
identities and exact extents are validated before updating any previous values;
all value bits are still copied at every opcode. The public immutable packet
factory and actual native-stop requirements remain unchanged.

`run_debug_codegen_pty.py` checks real native steps, effective compilation policy
and the complete opcode trace, including constants without a native instruction.
`run_debug_materialization_pty.py` checks numeric/NaN/SIMD bits, references,
loops, calls, tail calls, exceptions, initialization and 1000-local frames.
`run_debug_native_pane_markers.py` covers the ASM resume marker without granting
relocation permissions at a cooperative stop. `run_debug_fft_benchmark.py`
compares one identical Wasm file, reports guest kernel time separately from
startup, and checks its deterministic checksum on every run. Build
`fixtures/debug_fft_f64.cc` with the same WASI toolchain/flags for both modes.

The 2026-10-10 macOS measurements and limitations are recorded in
[the qualification receipt](debug_tui_performance_macos_20261010.json).
They use the existing O0 host test build and are not optimized-release or PMU
measurements. The subsequent [Linux/GDB/VTune performance report](README.debug-performance-20261010.md)
records the newer optimization and qualification; Windows/QEMU remains separate.

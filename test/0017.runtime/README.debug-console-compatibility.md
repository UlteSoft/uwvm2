# Console keyboard and command compatibility

The console keeps its own bounded FastIO input buffers. An exact command with
arguments wins completion over longer names (`break<Tab>` becomes `break `,
while `info <Tab>` lists candidates). Completion edits input and grants no VM
permissions. Dynamic paths and current source symbols are described in
[Console TUI and dynamic completion](README.debug-console-tui-completion.md).

In addition to history, search, kill/yank and undo, the console accepts:

- Ctrl+T: transpose adjacent UTF8 characters.
- Ctrl+V: quote the next printable input byte; quoted newline/tab becomes a
  space. Other control bytes reject the entire line rather than affect terminal
  rendering. Ctrl+C still cancels.
- Ctrl+O: accept the current input and preload the next actual history record.
- Ctrl/Alt+Left/Right: move by word (`CSI 1;5D/C`, `CSI 1;3D/C`).
- Alt+< / Alt+>: first history record / current draft.
- Alt+digits: repeat input edits, bounded to 1024; a negative argument reverses
  cursor/history motion. Counts cannot execute a command repeatedly.
- Bracketed paste (`CSI 200~` / `CSI 201~`): newline/tab becomes a space, and
  the closing marker never executes input. Press Enter separately to submit.
  Oversized paste, unknown control sequences and incomplete markers reject the
  entire command. A paste is one undo group.

Line capacity follows `console_editing::maximum_bytes`, including the current
WASIp1 input allowance. Raw memory and legacy command grammars retain their own
smaller limits. Numeric arguments have an independent 1024-edit limit.

Unique command prefixes are accepted (`cont`, `hel`, `info reg`, `info break`).
Ambiguous prefixes such as `st` or `thread step-i` are rejected. GDB aliases
include `backtrace`, `f`, `d`/`del`, and the existing stepping/printing spellings.
Arguments, names and paths are never abbreviated.

LLDB noun/verb aliases map to existing authenticated operations:

| Spelling | Operation |
| --- | --- |
| `process continue` / `process continue &` | foreground / background continue |
| `process interrupt`, `process status` | pause, status |
| `thread list`, `thread backtrace` | thread status, backtrace |
| `thread step-in`, `thread step-over`, `thread step-out` | source into/over/out |
| `thread step-inst`, `thread step-inst-over` | owned Wasm JIT instruction step/next |
| `frame variable`, `frame variable NAME`, `frame select N` | source locals, print, select frame |
| `register read [NAME]` | owned frame-zero registers |
| `breakpoint list`, `breakpoint set M F OFFSET`, `breakpoint delete ID` | existing Wasm breakpoints |

Source/native shorthand resolves a thread only from a fresh manager inspection
with exactly one stopped Wasm thread. Native commands still require the same
private JIT owner, current stop identifier, actual trap and Wasm activation.
They never authorize inspecting or stepping the VM, runtime, host functions, or
arbitrary native addresses. Thread ordinal selection remains unavailable. The
console TUI uses the same current-state checks. Exact unambiguous embedded DWARF
function names support `b MODULE SYMBOL`; numeric and file/line forms remain
`b MODULE FUNCTION OFFSET` and `b MODULE FILE:LINE`, keeping the module explicit.

`debug_console_compatibility.cc` covers editing, paste rejection, undo, history,
normalization, ambiguity, foreground policy, unsupported native/selection syntax
and missing current-thread labels. `debug_console_editor_readline_family.cc`
keeps the original exact-completion regression. Compile and run these together
with the existing editor/keyboard suite only in the original Linux cgroup.
Production PTY tests must use a binary freshly built from the same source cut;
component grammar tests alone do not establish live Wasm Ctrl+C behavior.

ROS keeps only full-module uwvm-int and llvm-jit backends. Its launch shortcuts
are `-Rint` and `-Raot`; `-Rdbg` uses LLVM full. Command aliases do not add any
runtime mode.

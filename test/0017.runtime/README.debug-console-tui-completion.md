# Console TUI and dynamic completion

Use the interactive full LLVM debugger (`-Rdbg`; ROS uses full `-Raot`).
The console supports `tui enable`, `tui disable` and these layouts:

| Command | Display |
| --- | --- |
| `layout src` | Actual stop/source frame metadata and highlighted local source text |
| `layout asm` | Current authenticated Wasm JIT instructions |
| `layout split` | Source and asm panes |
| `layout regs` | Asm and genuine native frame-zero registers |
| `layout wasm` | Current Wasm position, typed locals and operand values |
| `layout wasip1` | Current module's WASIp1 descriptor view |
| `layout next` | Next layout |

`Ctrl+X A` toggles TUI while preserving the partially edited command. `Ctrl+X 1`
and `Ctrl+X 2` select source/split. `focus code` and `focus cmd` select the code
pane or command history; `focus next/prev` switches between them. PgUp/PgDn
scroll the focused content. Ctrl+L redraws and rereads terminal dimensions.
TUI uses an alternate screen and restores it, cursor visibility, bracketed
paste and native input modes on ordinary console exit/EOF. Pipes retain the
plain console; `tui enable` requires an interactive terminal.

Views require one currently stopped Wasm thread. Every query uses the existing
controller authentication and formatter. Registers require a genuine native
trap; use `step asm THREAD` first. A safe point can lack an admitted native
instruction; a refusal retains the stop. Only real Wasm stepping can move to
another candidate, and the UI does not override that refusal. Asm never expands inspection to VM/runtime/
host code or arbitrary addresses. Unsupported/stale state produces an explicit
unavailable view. Resuming clears displayed stopped-state panels.

Source text is read with FastIO from the current embedded DWARF path, or an
explicit `tui source PATH` override. Only regular files up to 1 MiB are read;
FIFO/device/symlink failures are recoverable. File identity/size/timestamps are
checked around the read. Source text is display data, not an executable or
runtime identity proof. A replaced function's retired DWARF is not reused.

Tab retains static command completion and adds:

- Live directory completion for `replace MODULE FUNCTION GENERATION PATH` and
  `tui source PATH` and `breaksource MODULE PATH:LINE`; directory candidates end with `/`. Spaces are literal path
  bytes, matching these commands' existing whole-tail grammar.
- Current scoped source variables for `print`, `p`, `ptype`, `source-value`,
  `source-type` and `frame variable`. Explicit THREAD STOP [FRAME] selectors
  remain in the authenticated query. `packet.fi<Tab>` and
  `pointer->se<Tab>` query the corresponding scoped type for member names.
- Live embedded DWARF function names for `bw MODULE PREFIX<Tab>` and
  `breakwasm MODULE PREFIX<Tab>`. `bw MODULE SYMBOL` requires an exact, unambiguous
  concrete function and resolves to an existing emitted Wasm safe point. The
  normal controller still authenticates the resulting numeric breakpoint.

A unique candidate or common prefix edits the current token; another Tab lists
ambiguity. Completion preserves the expression suffix and is one undo group.
It never executes input, resumes the guest or grants a stop/native capability.
Each Tab rereads its source; there is no persistent stale symbol/path cache.
Control-bearing names are rejected. Directory scans stop at 4096 entries;
results stop at 128 candidates / 64 KiB. A truncated query never claims a unique
match. Function lookup has an independent bounded metadata-work budget.

`debug_console_tui_completion.cc DIRECTORY` tests real directory completion,
editing/undo, caps, source-file bounds/FIFO rejection, TUI rendering and RAII.
`run_debug_console_tui_completion.py` builds a real `-g` Wasm fixture and exercises
both instruction and unwind policies through an actual product PTY. Run it with
`--uwvm BINARY --source-root REPO --out NEW_DIRECTORY --cgroup ORIGINAL_CGROUP
--clang CLANGXX --wasm-tools WASM_TOOLS` and `--ros` for ROS. It checks source/
member/function completion, layouts, focus/scroll/resize, current native trap
registers, every disassembled PC inside the Wasm code owner, stale cooperative
completion rejection and terminal restoration. All compilation and tests must
run in the original SSH Linux keeper cgroup.

The current default build keeps `UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2` off.
That backend therefore refuses native owner/code/register admission, and these
TUI panes explicitly show unavailable. To test this default contract, pass
`--native-unavailable --build-receipts ACTUAL_SUPERVISOR_RECEIPTS_JSON`; the runner
checks all three genuine compile records and the actual ELF build proof. Those
results explicitly set `native_positive_coverage=false` and do not qualify a
genuine native trap or successful asm/register pane. Omit that flag only for an
independently qualified enabled native-owner build; the positive checks remain
mandatory and fail if no genuine trap is reached.

### Color, source highlighting and transient Tab candidates

TUI uses the same `diagnostic_color` palette and `NO_COLOR`/`--log-color` policy
as UWVM verbose diagnostics. Each code pane and the commands pane has a border;
the focused pane has a colored `*`. Keywords are purple, types blue, numeric
literals cyan, strings green, comments gray, and the stopped source marker yellow.

A bounded linear lexical scanner supports C/C++, Rust, Go, Zig, JavaScript,
TypeScript/AssemblyScript and WAT/WAST by filename extension. It carries block
comment and multiline/raw-string state from the beginning of the file into the
visible window. Rust and WAT nested comments are supported. This is lexical
coloring, not a language parser: script template interpolation is colored as
string text, and unknown file extensions remain plain. Existing regular-file,
1 MiB, stable-read and terminal-control sanitization checks still apply.

Ambiguous Tab candidates are temporary UI data, never command-log/history entries.
In TUI they cover the commands pane until the next key. With TUI disabled they
open a temporary alternate-screen popup so the primary screen and scrollback
remain intact. The next key, Enter, Ctrl+C, EOF, new output, `tui enable/disable`,
or Ctrl+X A dismisses them and restores the actual input/caret. Long candidate
lists show a bounded preview and ask for a longer prefix.

`debug_console_tui_color.cc` checks lexical state, palette, sanitization and
log separation. `run_debug_console_tui_color.py` checks physical PTYs in both
modes with/without NO_COLOR, static and real DWARF variable completion, split
panes, arrow/toggle/cancel cleanup and terminal restoration. Its real LLVM-full
Wasm test verifies UI/completion actions retain the same authenticated stop.
All executions require the original Linux 64 GiB cgroup.

# Debugger command experience

Start an LLVM-full build with `uwvm -Rdbg --run app.wasm`. Build the guest with
`-g` to use source frames, variables and source stepping. The initial prompt is
prepared: it has not run a Wasm instruction and has no call stack.

```text
(uwvm-debug) up
error: No stack yet: the program has not started.
Set a breakpoint, then use "run". Type "help break" for an example.
(uwvm-debug) help up
```

`help` lists command classes. `help language`, `help wasm`, `help asm` and
`help wasip1` narrow the index. `help up` explains the primary syntax, required
arguments and an example; `help up full` gives all forms and notes. Follow the
explicit `Next: help ...` command if a topic needs another page. `h` and command
aliases work in help selectors. Bare command prefixes open their respective
menus. Interactive help and errors use UWVM's diagnostic colors and obey
`NO_COLOR`; redirected output remains plain.

Unique command abbreviations also work with argument Tab completion: type
`pri coun` then Tab to complete `counter`, `pt rec` then Tab to complete the
type query, or `fr v record.co` then Tab to complete a member. Completion keeps
the typed command, explicit thread/stop selectors and surrounding expression
unchanged; it can replace a token while the cursor is in its middle.
Ambiguous inputs list the matching commands, for example `st` lists `status`
and `step`, while `info w` lists `info wasm-events` and `info wasip1`.
A single typo such as `wat`, `wiat` or `waitt` suggests `wait`. Suggestions and
ambiguous inputs never execute a command or change the current stop.
With `start`, `startw` and `starti` available, `sta` lists those commands and `status`;
use `stat` then Tab for an unambiguous status query. Candidate listing preserves
the input and undo history instead of selecting a command.

Subcommand typos have the same recovery: `thr lsit` suggests `thread list`,
`reg reed` suggests `register read`, and `set wasm memroy ...` suggests
`set wasm memory`. Each unique suggestion links directly to its help page.
Hyphenated command words also work (`thr step-ovre`, `set wasip1 fd-dupp`).
Suggestions are limited to static children of the nearest command menu;
argument values, expressions and paths are not spell-corrected. The console
never applies a suggestion automatically. A recognized child with extra
arguments (`thr list extra`) shows its leaf usage instead of an unknown-child
message. Unrelated or overlong child names still point to the nearest menu.

Help accepts the same unique command prefixes: `help pri`, `help i lo` and
`help thread step-ov` open their command pages. Category prefixes work one level
at a time, such as `help wasm set memo`. `help st`, `help info w` and
`help wasm set m` list complete, copyable help candidates without choosing one.
Full details and page selectors still work, for example `help pri full 2`.

Misspelled help selectors offer up to three copyable corrections instead of
opening a guessed page: `help thr lsit` offers `help thread list`, and
`help wasm set memroy` offers `help wasm set memory`. Command names containing
digits work too: `info waspi1 args` points to `help info wasip1`, while
`help i waspi1 args` offers the specific `help info wasip1 args` page.
Suggestions preserve a requested `full` and page suffix. Only one command-word
edit is allowed; invalid pages, arguments and multiple typos receive no guessed
correction. A suggestion never executes or rewrites the submitted command.

Help Tab completion ignores repeated separators using FastIO's
`char_category::c_space::char_is`, including leading whitespace and all C-space
characters in token matching. For example, `help  asm   reg` + Tab completes
`registers`; `  h   wasm    set    memo` completes `memory`. Only the caret's
token changes: previous spaces, following `full`/page selectors and cursor
placement survive. One undo restores the original token, spaces and caret.
Candidate lists show copyable single-space spelling for the next help level.

Use `apropos register` to find commands by purpose, or `apropos native register`
to require both words. `apr` is an accepted unique prefix, and Tab completes
the command name. The results link to short help pages and paginate to the
terminal width/height; follow the explicit `Next: apropos ...` command.
This searches only static help text, ignoring case, with at most 128 ASCII
bytes. It uses literal words rather than GDB's regular expressions. Searches
are available before execution, while stopped and after exit, and do not read
guest state, evaluate source expressions or advance the stopped program.

Incomplete command families are navigation: `thread`/`thr`, `process`/`proc`,
`breakpoint`/`br`, `register`/`reg`, `trace`, `catch` and `path` show the next
commands with their purposes. `set` first offers `set wasm` and `set wasip1`;
`info wasip1` (also `info wasi`) narrows inspection to arguments, env, fds and
preopens. `help thread`/`help thr` show the same menu. Menus are paged, support
the existing color policy and do not require a live source/native capture.
An unknown child such as `thr nonsense` points to `help thread`; a recognized
leaf with missing arguments still prints its specific usage.

These menus are console documentation, not new transport/DAP/script operations.
They do not intercept `r`, `c`, `s`, `frame`, `thread list`, `register read` or
complete edits. Enter after a menu does not repeat an earlier command. Adding
the existing `path` command to abbreviation lookup makes `pa` ambiguous between
`path` and `pause`; use the full name. Path subcommands also have Tab completion.

Command Tab completion follows the same hierarchy: `set ` offers only `wasm`
and `wasip1`, then the next Tab can list the selected level's commands.
`thr li` + Tab becomes `thr list`, `reg re` becomes `reg read `, and
`i wasi ar` becomes `i wasi args `. Completed aliases are retained. Candidate
lists show one command word at a time, rather than every descendant form.
Wasm edits, WASIp1 checkpoints/files/import/export, and both trace levels have
completion routes. Exact words take precedence (`arg` does not turn into
`arg-insert`; `thread step-in` does not turn into `thread step-inst`).
Completion can replace a command word while the caret is inside it, preserves
following selectors/hex/expression bytes, and is undone with one Ctrl+_ action.
Guest symbol and path arguments still use their existing providers; numeric,
hex and script arguments are left for the command parser.

The ordinary GDB conventions are retained:

| Action | Commands | Meaning |
| --- | --- | --- |
| Start/restart | `run`, `r` | Run from the entry point; restart an already running or exited guest. |
| Start at Wasm entry | `startw`, `stw` | Reload and stop before the first real Wasm opcode, with a current Wasm location and state. |
| Start at native entry | `starti`, `sti` | Retain the real trap before stepping the first owned JIT instruction at the initial Wasm safe point. JIT prologue/debug setup precede it. |
| Start at main | `start`, `st` | Use a temporary published-main breakpoint; startup code may execute first. |
| End the guest | `kill` | Retire real workers and keep the prompt, with no new instance until a launch command. |
| Resume | `continue`, `c` | Continue the current execution. Use `run` after exit. |
| Source stepping | `step`, `s`; `next`, `n`; `finish` | Enter calls, step over calls, or return from the selected source frame. |
| Frame selection | `frame 0`, `up`, `down` | Inspect a different frame. #0 is the current function; increasing numbers are callers. Selection does not execute code. |
| Source inspection | `bt`, `info locals`, `print EXPR`, `ptype EXPR` | Inspect the stopped Wasm guest using its source information. |
| Native stepping | `si`, `ni`, `finish asm` | Step within the owned Wasm JIT context. |
| Native inspection | `info registers`, `disas` | Registers require a genuine native instruction stop; start with `si` from a Wasm stop. |

The design references GDB's [frame selection](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Selection.html),
[help navigation](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Help.html)
and [stepping commands](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Continuing-and-Stepping.html).
UWVM has its own stopped-thread labels and Wasm commands. It does not implement
GDB's arbitrary native memory or VM frame inspection. At a native instruction
stop, source queries explain how to return to a Wasm/source breakpoint; they do
not fabricate source frames from VM or host state.

Errors distinguish a program that has not started, is running, has a pending
pause, has exited or has closed. A running-program error directs the user to
Ctrl+C/`pause`; an exited-program error suggests `run`. Stale STOP labels and
missing THREAD labels have separate messages. `up/down` at the stack boundary
name the outermost/innermost frame. An invalid argument shows the expected
syntax and the relevant help topic. Missing source information suggests `-g`
and Wasm stepping. Successful shorthand frame selection prints the selected
frame only. Successful resume acknowledgements are quiet; guest output and
stop/exit events remain visible.

Source inspection shorthand presents the copied values directly:

```text
(uwvm-debug) print counter
counter = 2
(uwvm-debug) ptype counter
type = volatile int
(uwvm-debug) info locals
counter = 2
```

Parameters are marked `[parameter]` in `info locals`. Objects use an indented
member tree; guest pointers keep the `guest:` address label. Unavailable values,
unknown offsets/sizes, omitted elements and truncated output are explicit. No
GDB `$N` value history is implied. `bt` lists copied physical Wasm frames; a
partial stack is marked and available inline metadata points to `bt THREAD`.
Use explicit `print THREAD STOP EXPR`, `ptype THREAD STOP NAME`,
`locals source THREAD` or `bt THREAD` for the detailed provenance/layout view.
Authenticated transports and DAP keep their existing output grammar.

After the guest exits, use `run` before editing its live breakpoint policy.
Valid breakpoints are reinstalled on restart. This differs from GDB's ability
to configure breakpoints after exit; the CLI reports the exited state explicitly.

`console_messages.h` formats copied replies only. It does not authorize a
native read, select a frame, admit a stop or alter runtime owners. Authenticated
text transports and DAP retain their existing reply grammar. Script children
use the same human CLI error presentation, with bounded output and explicit
command headings.

A native stop can have only a proved PC, with other register values unavailable.
The CLI explains the missing Wasm value mapping instead of implying that zero
or host register values are guest values. A disassembly window with no safe
displayable instruction produces one explanation and the relevant help topic.
Mixed available/unavailable windows retain their individual rows.

The regression suite uses both component and actual product evidence:

* `debug_console_command_completion.cc`: command hierarchy and aliases,
  exact words, mid-word replacement, argument preservation and single-step
  undo; menu choices have matching completion routes.
* `debug_console_messages.cc`: execution-state matrix, frame bounds, stale
  labels, missing source info, register prerequisites, script child messages,
  transport compatibility and terminal-control sanitization.
* `debug_console_help.cc` / `run_debug_console_help.py`: all 98 leaf pages,
  aliases and accepted example syntax; real PTY paging, Tab completion, colors,
  `NO_COLOR`, TUI input and terminal restoration. Help abbreviation, ambiguity,
  literal keyword search and bounded search paging are also checked.
* `run_debug_user_experience.py`: a real `-g` Wasm guest, production LLVM-full
  CLI and physical PTY. It navigates all 130 catalogue entries, then exercises
  execution, breakpoints, language frames/values, Wasm state/tracing/scripts,
  WASIp1 state/edits, checkpoints, body replacement, native debugging, TUI and
  inspection classes, including rejected commands and restart/exit handling.
* `run_debug_run_cli.py`: actual `run/r` restart and quiet output regression.
  Its pure Wasm guest also verifies a real memory edit and that restart clears it.
  An independent native GDB fixture provides a reference for no-stack,
  frame-boundary and ordinary-exit behavior.

Run the Linux suites inside the approved 64 GiB cgroup. The current experience
fixture covers native x86_64 LLVM-full command classes in both repositories.
This scope is distinct from every Wasm proposal, every language/debug format
or cross-architecture native single-step qualification.

The DWARF/WASIp1 experience guest deliberately retains its real `fd_write` host
import. Its memory edit may be refused by the runtime's requirement for a closed
Wasm resource graph; the test verifies the refusal text and unchanged bytes.
The result records this separately from the positive pure-Wasm memory-edit test,
and separately records instruction/register visibility at its first native stop.

Breakpoint commands: `bw` is the alias for `breakwasm`, taking
`bw MODULE_ID FUNCTION BYTE_OFFSET` or `bw MODULE_ID SYMBOL` for Wasm/function
breakpoints. `bs` is the alias for `breaksource`, taking `bs MODULE_ID FILE:LINE`.
`bw` or `bs` alone shows the
first bounded module page; `info modules 2` shows the next page, and `bs 0`
explains module 0 and its source breakpoint availability. The table identifies the actual loaded file and display name;
only the numeric ID selects a module, even when a module is named `0` or `123`.
A module without usable source locations suggests checking `-g` or equivalent
source maps, stripped symbols and optimized/unmapped code. Malformed metadata,
replacement and a wrong FILE:LINE are diagnosed separately. Listings copy
controller-owned setup metadata; they grant no runtime memory/code access.
`debug_console_module_help.cc` checks grammar, paging, numeric names and terminal
control sanitization; `run_debug_module_help_cli.py` exercises these behaviors
with actual debug/no-debug Wasm and multiple preloads in the Linux cgroup.

Command routing and argument scanning use `fast_io::char_category::c_space::char_is`, including repeated/leading/trailing separators. `layout  regs`, `tui   enable`, `focus   cmd`, numeric Wasm/WASIp1 queries, source selectors, breakpoint `if`/`ignore` and path completion follow word boundaries. Internal spaces in paths, expressions and scripts stay literal. `debug_console_whitespace.cc` checks every C whitespace character plus invalid extra arguments and near-prefix command names; the physical CLI regression checks spaced commands and the registers TUI layout.

## Three breakpoint levels

Only `breaksource` (`bs`), `breakwasm` (`bw`) and `breakasm` (`bi`) set breakpoints.
`break`, `b`, `break-source`, `break-name` and `breakpoint set` are removed.

```
info modules
bs 0 main.cc:20
bw 0 1 0
run
si
disas
bi 128
continue
info breakpoints
delete 3
```

For `bi`, replace 128 with a decimal `function-offset` of a **visible instruction**
in `disas`. The controller independently checks the live private JIT owner,
publication, physical activation, complete guest byte mask and instruction boundary.
Absolute addresses, VM/adapter bytes, host symbols and raw stack/memory targets are
rejected. An asm breakpoint uses genuine native stepping under the existing
pre-execution successor guards; it reports a hit only at a real kernel trap with
matching PC. The search is bounded by the command timeout and 65536 instructions.
An unproved boundary is refused. These points apply to the current physical
activation and retire on its release, including run/restart. Management IDs share
`delete`, `enable`, `disable`, `ignore`; source conditions on native points are
unavailable. Normal Wasm and source breakpoints retain their existing policy.

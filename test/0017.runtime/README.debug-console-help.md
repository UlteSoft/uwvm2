# Hierarchical debugger help

The prompt opens a short index instead of printing every command:

```text
help                       topic index
help language              source-level topics
help wasm values           typed Wasm inspection commands
help asm                   native stepping and register commands
help wasip1                WASIp1 inspection and edits
help run                   short purpose, required arguments and one example
help run full              all forms, examples and restart behavior
help asm registers         register usage and access limits
help wasm set global full 2 explicitly read the second full-detail page
help replace               function-body replacement requirements
```

`h` is an alias for `help`. Command aliases select the same documentation, so
`help r`, `help si`, `help i r` and `help register read` work. Tab completes the
next topic or command. For example, type `help asm`, press Tab, then type `reg`
and press Tab. Bare `help` completion lists categories rather than every alias.

Each leaf starts with its purpose, primary usage, prerequisites, one example and
required argument definitions. `help COMMAND full` adds alternative forms,
other examples and notes. Long pages show `Next: help TOPIC [full] N`.
At most 18 body rows appear at once; small
terminals reduce the row count and wrap text to their width. There is no implicit
full-catalogue dump. Example IDs are illustrative; use current IDs from `status`.

Ordinary interactive help uses the same palette and `diagnostic_color` policy
as `uwvm -h` and verbose logging. `NO_COLOR` (including an empty value) and
`--log-color no` suppress colors. TUI and authenticated management transports
receive plain text; the existing TUI sanitizer remains in effect. FastIO handles
strings, formatting and page-number scanning.

`debug_console_help.cc` tests catalogue topology, selector bounds, aliases,
all leaf sections across pages, accepted example syntax, width limits, color
policy and nested editor completion. Its PTY mode uses the real production
keyboard, line reader and help renderer. `run_debug_console_help.py` checks
topic/command navigation, explicit paging, errors, Tab completion, ordinary and
TUI input, `NO_COLOR` and terminal-mode restoration. The controller parser,
display parser and TUI command parser validate their respective examples;
function-name breakpoint resolution still requires live module metadata.

Bare `info` lists inspection commands; bare `set` lists Wasm edits and links to
WASIp1 edits. Errors name the current execution state and suggest a usable next
command. See [the real CLI experience audit](README.debug-console-user-experience.md)
for command-class coverage and GDB comparisons.

Run both repositories' fixtures only inside the approved Linux cgroup. These
are help/input component tests; they do not claim guest execution, JIT admission
or cross-architecture native-debug coverage. Existing editor, compatibility and
cursor unit tests provide regressions for the shared input path.

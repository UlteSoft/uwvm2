# Extra bounded console editing (SOURCE only)

Ctrl+L redraws only the console display and retains its current text/cursor.
Tab expands static built-in command prefixes, including multiword names. With
ambiguous names it expands only their common prefix, then lists the matching
built-in names. It does not enumerate paths, Wasm/native symbols or guest state.
Completion only edits host input; normal parsing and real stop authorization
still happen before execution. Mid-line and argument completion are conservative.

Ctrl+R incrementally searches the 32 real owned host-history records; another
Ctrl+R selects an earlier match. Ctrl+G restores the pre-search draft and cursor.
Enter explicitly accepts a matching record. Failed or whole oversized queries
cannot execute a stale earlier match, including on EOF. Queries, lines and
history are fixed 512-byte buffers; UTF8 query deletion removes a codepoint.
Ctrl+C still discards the whole line/search and repeat candidate. Search may
select a previously mutating command only for explicit Enter/normal command
admission; it cannot make mutations eligible for implicit empty-line replay.

This package modifies existing console_line_editor.h/.cppm and a narrow renderer
hook/help section in console.h. Existing module import/export closure is retained.
ROOT must merge narrow console hunks if synchronous-interrupt hooks have already
changed its preimage. No existing WASIp1 syscall/strings or VM/JIT/memory hot path
is changed. The new finite C++ component is source only and must be compiled and
run inside the keeper cgroup against the exact current source. Product PTY,
Windows terminal, BSD/mac keyboard and GDB-equivalence are not yet qualified.

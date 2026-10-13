# Source stepping with real full-JIT event identities

The controller source candidate now consumes the runtime's actual before-park
activation capture and `llvm_jit_debug_query_source_activation_host_api`.
This integration has not yet been compiled or exercised against a fresh product.
The earlier Linux scope/policy component PASS and DAP formatter PASS do not
qualify these new runtime/controller semantics.

A generated full-debug body reports real entry, return, typed tail, host tail
and qualified frame-exiting native EH events. Every entry has a new incarnation;
a typed tail inherits only its retiring continuation. Source stepping never
creates identity from symbol depth, a native CFA, offset zero, or function names.
The private capture retains the real participant/ticket/location. A current
query verifies its canonical control block, actual full code owner, runtime and
per-function generations, and the actual source binding and Code-relative PC
in one execution lease -> ONE pause-domain guard -> publication guard.
Supplied shared pointers are compared against runtime-owned canonical objects
before any dereference. A foreign source/capture address rejects the whole joint
query. An absent or legitimately outdated source mapping remains unavailable.
No query grants memory, expression, register, native-address or caller-frame read
permission.

The manager requests a genuine instruction stop, waits without its controller
mutex until all participants park, then evaluates the owned metadata policy.
Intermediate stops retire their ticket/captures before resuming. Only the final
stop completes the wire step request. The command deadline and a limit of
65,536 genuine stopped queries bound each source-step operation; reaching that
limit preserves the actual current pause. Peer loss/explicit continue cancel
pending step requests even between stops.
The ordinary interpreter/full JIT execution paths never run this policy.

`query_concrete_scope_path` selects a unique physical subprogram plus concrete
inline DIE instances from outer to inner, using actual Code-relative PC.
It validates ranges/parents/depth/keys, excludes explicitly empty lexical scopes,
and never substitutes names or abstract-origin keys for identity. The same
selector serves inline display and finite numeric-variable selection. All paths
and stop tokens are bounded owned metadata, not read credentials.

`into` observes statement file/line/column/discriminator and concrete inline
context, including a genuine new recursive or typed-tail entry. `over` skips
actual deeper physical calls and inline descendants. Inline `out` waits until
that concrete inline instance is left; physical `out` requires the actual
returned caller's fresh source position. Both follow a typed tail's continuation
until it really returns, rather than treating equal native depth as the same
activation. Missing/ambiguous metadata, unknown host islands, stale generations,
quota overflow and incomplete chains decline conservatively. There is no
fabricated caller PC.

A real compiler prologue/epilogue may have no line or concrete-scope row. The
controller marks metadata ready only after that actual module's line and scope
tables parse successfully. With its canonical source owner, fresh complete
activation chain, unchanged generations and consistent continuation/depth, the
policy can advance through this hole without creating a source label. Each next
opcode obtains a new joint stopped query. Entirely missing DWARF, foreign owners,
unknown chains, native positions, malformed/ambiguous metadata and stale code
still decline. This new source-only hole handling needs fresh native tests.

The LLVM-free `debug_source_step_policy.cc` tests metadata semantics only,
including recursive chains, tail continuations, exact ancestors, same-depth
reuse, epoch/generation changes and quotas. Synthetic owners/events in that test
are not genuine runtime credentials. `debug_activation_runtime.cc` uses actual
owning parse/initialization/full-fusion publication, native entries/recursion/
Core 3 tail calls, generation-two replacement and fresh joint queries; it also
checks unreadable shared-pointer aliases and post-resume invalidation. That new
witness still requires keeper compilation and actual execution. No local native
or VM test was run.

Remaining acceptance includes official compiler-produced C/C++/Rust -g inline
chains with repeated callee names, statement/discriminator transitions,
recursion, tails, local/cross-frame EH, host reentry, replacement, concurrent
participants, source/wasm/native stepping, both diagnostic policies, real IDE
sessions and Linux/macOS/Windows native backends. The default-mode IR/assembly
and unprofiled timing/counters need independent fresh qualification.

References checked during implementation:

- [WebAssembly DWARF conventions](https://github.com/WebAssembly/tool-conventions/blob/main/Dwarf.md): source PC is a Code-section-relative instruction offset; numeric local locations are separate from native addresses.
- [LLVM call/musttail rules](https://llvm.org/docs/LangRef.html#call-instruction): instrumentation retires the activation before musttail, preserving matching calling conventions and the required immediate return.

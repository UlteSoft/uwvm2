# Exact normal call-return projection, independent candidate 1

`caller_return_projection.h` checks immutable metadata and reorders complete
typed native DATA from a waiting caller and a normally returned child into the
actual post-call site. It is not an issuer for an actual child return, a paused
cohort, GC roots, resource census or generated code execution. Its execution
authority is always false. A public tuple or shared pointer cannot bypass the
future private runtime dispatcher. No save/restore/reverse command is exposed.

The fused validation/translation walk must record the waiting site after call
arguments and any table selector/function reference have been consumed. The
next site is the instruction after this exact call. Both sites belong to one
retained sealed function plan and preserve locals, operand prefix, saved
control parameters and the exact declared controls. The validated call type
provides the result tuple, including Core 3 heap and nullability. LLVM carrier
types or the more specific actual target function type cannot reconstruct this
semantic call type. A genuine callee return must first pass the runtime's
canonical actual ABI/type and continuation checks.

The transformation keeps the caller's locals and operand prefix, inserts the
normal return tuple at the operand tail, then keeps the saved control-entry
parameters after the operands. Numeric values preserve their exact native
bits, including v128 and floating-point NaNs. An unset nondefaultable local
keeps its original unavailable zero slot. Null and canonical i31 references
are supported without dereferencing a host pointer. Live GC, function, extern
and exception references decline until actual retained root/store/resource
producers exist. Active handlers, exception/trap returns and typed-tail
transfers require separate real continuation logic and do not take this path.

Every count, dense ordinal, exact plan owner/control block, type and value is
checked before detached output allocation/copy. Failure leaves the previous
result frame intact. The result remains typed DATA after return; the future
dispatcher must retain the actual returned-child event, same logical
continuation, source/code generation, immutable native input ownership and
coherent instance transaction while selecting the already installed LLVM
post-call landing. It must not repeat the original call. No native PC/SP/GPR or
stack image is restored.

The standalone DATA unit exercises original slot order, complete v128/i31,
unset local preservation, control-block aliases, wrong ordinals, counts,
nullability and noncanonical carriers, handler rejection and failure atomicity.
It is not a native VM pause/restore test. All actual compilation and execution
must run in the authorized remote cgroup lane. The real LLVM caller component
source uses a generated child with a call counter and captures its actual normal
result tuple. Its expected counter sequence is one after ordinary execution,
two after separately executing the actual child to return its tuple, still two
after selecting the caller's post-call landing, and three after the next normal
entry. This proves the restored parent does not repeat the returned child only
when the native test actually passes. Native component acceptance remains pending.

The representation follows the official
[Core 3 function invocation and return semantics](https://webassembly.github.io/spec/core/exec/instructions.html#function-calls)
and [runtime frame/configuration model](https://webassembly.github.io/spec/core/exec/runtime.html).
LLVM [PHI predecessors and dominance](https://llvm.org/docs/LangRef.html#phi-instruction)
must be verified in the complete emitted function after every real control
edge is connected; metadata sealing alone does not prove native dominance.

# Integer comparison same-walk slice

PRIVATE SOURCE-only successor of width56 AFTER, not a LIVE overlay. No WAT parser, C++ compiler, BMI, LLVM verifier, native, ASM or performance run. Full-tiered READY and fullCore3 dual output remain incomplete.

The22 immediate-free integer comparisons0x45..0x5a share one actual first-arity/Bot transition in pure wasm3 and full/lazy INT/LLVM. Every result is i32; unary eqz pops1, other comparisons pop2 of the exact input integer width. Entire arity preflight occurs before concrete mismatch and preserves entries below the current frame. Reference-only bottom is not numeric value Bot. No preliminary pure validation pass is inserted.

The original22 ICmp/coerce LLVM expressions are mechanically copied; the old LLVM path already emitted these inline. Original INT opfuncs, pending comparison/branch fusion and cached-ring bodies remain byte-identical. New event callbacks carry only already typed DATA. Default comparepolicy=false preserves the optional native slice; unlisted legal operations decline optional SSA and keep the authoritative ring admission.

The genuine called-start positive executes every22 result, signed/unsigned limits, param32/param64/pair64 prototype checks and actual GC/memory64/tail/dead lexical children (outer branch bypasses traps). DATA tests cover binary firstarity priority, current-frame prefix, empty and partly-concrete unreachable suffix, value Bot versus heap Bot and pop-order diagnostics. Five modern unused-body negatives must match exact code diagnostic offsets rather than CLI/I/O/signal failures. Fresh18 real cached-ring combine/delay recipes plus official+component and product fourmode runners are supplied; no runner itself certifies all native layouts/musttail or compiles them.

Only SOURCE recipe/AST/hash/mechanical proof is available. Successful optional module-IR quota is not an attempted work/RSS/OOM guarantee. Linux native belongs to the solekeeper64GiB cgroup; Mac execution0. Authoritative Core3: https://webassembly.github.io/spec/core/valid/instructions.html#numeric-instructions and https://webassembly.github.io/spec/core/exec/numerics.html#integer-operations.

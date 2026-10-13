The generator produces Core 3 GC workloads with 1, 64, 1024, or 65536 real,
exported table roots. It separates the original newborn set/get loop from reads
of older allocations. The remaining cases exercise mutable scalar fields,
packed arrays, recursive cyclic graphs, reference arrays, references carried
through a cross-function exception, and a memory64 regression.

The modular affine oracle checks the final state and, for older-root workloads,
the sum of previously stored values. The latter includes initial root values and
the actual retention distance. Cyclic graphs additionally check both edges and
both values; packed arrays check truncation; memory64 checks the stored value.
Setup allocations are recorded separately from aggregate allocations per
measured step. Exception-record and native EH allocations must be measured in
each implementation; the generator only records the actual number of throws.
These are observable workloads, not evidence that a collector actually ran.

Generate, assemble, and validate on SSH Linux inside the existing 64 GiB cgroup.
First run a small development sample against Wasmtime and each current product.
Every module must use only its listed features; preserve encoded bytes, exact
commands, build identities, logs and any rejection. In particular, the exception
case must not be called a successful managed-GC test if the runtime's real root
closure gate rejects collection. Defined tags, reference payloads and native
exception owners require their own qualified closure.

Formal timing uses the existing isolated P-core protocol after these checks.
Compare identical modules and work counts, distinguish instruction and unwind
policies, inspect actual LLVM IR and machine code, and retain both fast-path and
fallback counters, collection counts, reclaimed objects and peak memory. Do not
combine the newborn loop, older reads, larger arrays or cyclic objects into one
speedup. Source generation alone supplies no assembly, VM, GC or performance
qualification.

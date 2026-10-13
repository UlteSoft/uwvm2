The ordinary product's tiered checked-IR owner now specializes tagged static
LOCAL `call` targets after ALL retained definitions are merged. This is PRIVATE
source only. R4-r9 and whole-owner T2R3 remain immutable prerequisites. No LIVE
source has been written and no C++ compile/BMI/LLVM verifier/native/performance
test has run for this package.

The source producer tags only genuine tiered retained-sink local calls. The
ordinary full/null sink and plain lazy have no new metadata, state field, option
field, guest IR call, hot poll, lock, or memory-access guard. ROS has no tiered
runtime adapter: it receives only the identical independent LLVM IR helper,
component source, and full-only modern CLI inputs. That is not ROS tiered support.

The private whole owner authenticates current source pointers, runtime epoch,
exact original emission options, all private same-walk records and all native
IR availability. It binds actual same-module typed definitions and hidden OSR
cores to their original typed signatures/calling conventions. The low-level
transform checks dense identity, exact source slot symbol/address, native
pointer width, original Acquire/Volatile/alignment/GEP index, original branch
and complete typed call SSA users before changing any instruction. Global
Function/integer metadata survives the actual bitcode writer/reader; no local
SSA metadata or arbitrary requested pointer is treated as authority.

Recognized slot bases are the actual generated external symbol, native integral
constant pointer, owned private address-carrier load, and exact existing RV64
`li` inline-asm producer. Carrier initializers alone do not establish
immutability: every use must be a readonly load, with no store/address escape.
A dense LLVM pointer set checks each shared carrier once. Every tagged target
read is replaced by LLVM's own ptrtoint of the actual same-module Function,
then erased. Dead GEP/pointer/address-producer auxiliaries are removed only after
all preflight and only when use_empty. All shared auxiliaries are deduplicated
before any deletion. The final LLVM module verifier is mandatory. Any failure
keeps the guest's prior typed admission valid, declines the unpublished T2
artifact, and cannot reopen raw Wasm bytes.

No imported call, foreign module, mutable table/ref target, runtime raw target,
`call_ref`, `call_indirect`, or tail resolver is specialized by this producer.
The existing authenticated staged import-route selection remains intact and
its typed slot reads are retained. No function pointer/native address/debugger,
CFI/restore permission is issued by the helper. Public typed wrappers and
NoInline OSR core layout remain the original layout. The candidate does not
claim the old direct-layout T2 costs or a measured gain.

Cold work is bounded by the parent <=65536-function/128MiB retained payload
quota plus <=262144 tagged replacements and at most three auxiliary pointers
per replacement. These are compiler payload/operation bounds, not a claim that
all LLVM scratch allocations or total process RSS are recoverably bounded.
C++ allocation failures follow the existing callback cleanup; LLVM allocator
fatal behavior is not newly claimed recoverable. The actual Linux 64GiB cgroup
remains mandatory. No Mac native execution is authorized.

Testing source

* static_local_target_owned_ir.cc: real LLVM bitcode round trips for 28 positive
  shape cases (32/64 bits, little/big endian, zero/nonzero GEP indices, symbol /
  constant / private carrier / RV producer). Fourteen negative cases require
  module text unchanged before mutation. A real host TM/DL, optimizer, MCJIT
  and result42 must prove the native component. Endian/RV shape positives are
  not target-native architecture qualifications.
* fused_static_local_target.cc: actual parser + original fused two-function
  admission emits nonzero tagged reads in retained LLVM fragments; actual
  private whole consumption removes them, retains genuine loop metadata and
  later native MCJIT must return42. Its real WAT exercises i31, a memory64
  declaration and typed select. It uses the complete matching runtime/provider
  closure; do not treat synthetic DATA corpus as its substitute.
* run_static_local_target_modern.py: finite independent wasm-tools parse/validate
  and normal-product mode checks. The unused invalid last memory64 body must
  produce a real code-validation error at startup; official oracle must provide
  `(at offset 0x...)`, not feature/CLI/I/O/signal failure. The legal program uses
  recursion, later local calls, typed call_ref, i31 and return_call, and traps
  on a wrong answer. A real try_table/throw/catch positive and unused mismatched
  catch-label negative additionally cover exception admission and later lazy
  materialization. ROS uses only its two supported full modes.

Keeper qualification still required

Freeze a genuinely composed source cut, fresh ALL3 product TUs and BMI/header
modules under the actual same LLVM provider/ABI. Compile/run the two independent
components, finite modern mode matrix, then the genuine tiered runtime runner
from T2R3 under instruction/unwind with T0 on/off. Preserve old failures and
source/product/provider/IR/object pins. Inspect actual optimized IR/ASM for
slot-read removal, direct actual callee and unchanged call/frame/EH/musttail
behavior. Measure P-core wall time and hardware counters plus cold compile
latency/RSS against exactly the parent after cut. This source package supplies
no numeric speedup or whole-Wasm3 conformance qualification.

Single typing walk remains unfinished: see DUAL_SINK_SINGLE_TYPED_WALK_DESIGN.md.
Current tiered INT admission and LLVM admission still each run their original
backend walker. The T2 third walk is removed; that does not equal one tiered
validator walk.

Primary references read:
https://webassembly.github.io/spec/core/valid/instructions.html
https://webassembly.github.io/spec/core/exec/instructions.html#function-instructions
https://llvm.org/docs/LangRef.html#metadata
https://llvm.org/docs/ProgrammersManual.html#replacing-an-instruction-with-another-value

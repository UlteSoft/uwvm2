# Actual-engine native assembly view

This PRIVATE source candidate separates cold MC viewing from native OS stepping. It does not qualify any OS or architecture by changing an enum/gate. All native execution is still pending the sole Linux cgroup keeper; macOS native execution remains disabled.

The two owned code replies now retain target format version 1, complete bounded triple/CPU/features, actual pointer width and byte order, and actual MCAsmInfo maximum instruction size/minimum alignment. Their producer finds the exact current full or committed replacement engine by the authenticated code-owner comparison token and function generation under the existing execution lease, ONE stopped-domain transaction and publication guard. No target triple is synthesized from host preprocessor macros in the production controller. The macros select only the LLVM registry family linked for this executable, matching the production target initializer.

Bounds are triple 255 bytes plus NUL, CPU 255 plus NUL, features 16383 plus NUL, one decoded instruction at most 32 bytes. Overlong metadata, absent actual MC provider, inconsistent native pointer width/endian or unsupported extent return unavailable. These limits are explicit cold reply limits, not normative ISA maxima. The ordinary compiler/cache key/guest memory path has no new calls or IR.

A noinline debug safepoint bridge on GNU/Clang obtains only its own level-zero extracted return address. It does not read a supplied SP/PC, scan another frame, invent native-thread identity, or authorize SI/NI. It must match the actual current generated function code range and private event/publication identity. Unsupported compiler return-address support leaves the site unavailable. Pure MSVC and architecture-specific return-address correctness remain unqualified.

Cooperative assembly VIEW uses one <=64KiB owned function image and its same-operation target description. Outside live locks, actual-target MC decodes forward from the exact function entry, bounded by 8192 rows, and accepts the cooperative reference PC only at an exact decoded boundary. No backward resynchronization or independently queried target metadata is combined. The formatter prints origin=safepoint-code-view. Failed/truncated/SoftFail decoding is explicitly unavailable. This view cannot display native registers or become an instruction stop.

Actual native traps keep the existing <=480 owned current-PC path and genuine trap/session/OS adapter checks. The formatter prints origin=native-instruction-stop. SI/NI and call/return/branch refusal are unchanged except that both actual MC decoders now consume the actual engine description. The i386 TF candidate is separate; this candidate does not implement A64/RV64/BSD hardware stepping.

The historical parameter-free DATA test decoders remain for existing fixtures. The runtime controller never uses them. Only constructors receiving the same private query's exact target are used by production SI preflight, display and semantic annotations. Supplied descriptions and decoded classifications remain DATA; they cannot mint read/control/source/local/restore rights.

All changed runtime and reply layouts require fresh runtime/LLVM interface/main and fresh BMIs. Header-only AST inspection is not module or native qualification. Existing Windows/ELF/MachO qualifications do not transfer to this change.

Meaningful pending tests:
- native_target_metadata_data.cc: format/NUL/extent/alignment negative DATA corpus.
- native_actual_target_decoder.cc: an actual finalized LLVM engine owns the TM; explicit target CPU/features/endianness/extent feed real C API and semantic MC decoders. First byte corpus covers x86/A64/RV. Other ISAs remain explicit corpus gaps.
- debug_native_actual_target_view_runtime.cc plus its Core3 GC-declaration/non-null-local/return_call WAT: real source initialization, fused full compilation, real controller/callback pause, private code image, exact boundary, actual formatter, refusal of regs/NI, stale/reset/drain; instruction and unwind separately.
- native_actual_target_metadata_module.cppm plus its consumer: genuine two-primary imports, no global FastIO header masking.

Primary references:
- https://gcc.gnu.org/onlinedocs/gcc/Return-Address.html
- https://llvm.org/docs/LangRef.html#llvm-returnaddress-intrinsic
- LLVM23 source: llvm/lib/MC/MCDisassembler/Disassembler.cpp, include/llvm/Target/TargetMachine.h, include/llvm/MC/MCAsmInfo.h, lib/ExecutionEngine/MCJIT/MCJIT.h.


Current R2 rebase preserves all current R1 strict SI successor proof, CALL64 continuation classification/default-OFF product gate, dynamically retained physical activation ledger, actual event retirement/released ACK, host observation latch and current FP14 command/formatter changes. Both actual production SI proof sites now construct their two MC decoders from the SAME immutable function image target. No old preflight code or controller body is overlaid. Original four memory-copy choices (three runtime native copy calls and one controller name copy) remain byte-for-byte current calls; this change introduces no unrelated FastIO rewrite.

Both cooperative-origin resolver and actual cold copies additionally reject raw_entry/wrapper_entry roles before read. An authentic current plain Wasm code origin is separately recorded from OS single-step permission; Linux SIGTRAP preflight/native-thread identity remain in the original qualified-step wrapper. Closed/unknown owner ranges remain unavailable. This candidate does not resolve checkpoint typedclone endpoint/PPC descriptor qualification HOLD, nor implement cross-function SI/finish or additional OS trap adapters. Native compilation/execution and same-cut real instruction/unwind/strict six bridges/DAP/FP qualification remain pending; nothing here is a new native PASS.


R3 handles the verified SDK shape difference: LLVM21 nullable pointer getters versus vendored23 reference getters are normalized from the SAME actual leased TargetMachine, then both SDK providers are nonnull-checked before dereference. The genuine engine decoder component uses the same shape-adaptive acquisition but an independent code path. This is SOURCE SDK compatibility only; neither LLVM21 nor23 fresh build/runtime qualification is inferred from it.

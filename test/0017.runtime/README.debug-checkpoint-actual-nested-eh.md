# Actual nested native exception continuation regression

This fixture exercises the real parser/initializer, the SAME fused LLVM validator/translater, private current-ticket capture, actual execution retirement/drain, and actual saved native parent/child continuation. It is NOT whole-instance/database/new-epoch restore and does not attempt to reexecute saved nonnull input references without the joint new-world issuer.

The original child pauses before its first allocation. Its parent is genuinely inside BOTH an inner `catch_all_ref` and an outer named `catch_ref`. Original retirement must unwind the private control signal past both guest handlers, run real frame/root/shadow cleanup, drain the actual execution domain, and leave the original host result sentinel untouched.

Restored native child creates a GC object with field42 and throws the actual tag with scalar37 and that object. Parent catches as `catch_all_ref`, rethrows the same immutable native exception value, and obtains the exact named payload via `catch_ref`. The first fresh actual stop checks current ONE-cohort/N-owned typed locals/operand, actual tag identity, local exn aliases, object aliases and field42. It then mutates the shared object to84 and uses `throw_ref` again; the second fresh current stop checks the original and second caught payloads still refer to that same mutated aggregate, scalar37 is preserved, and exactly three genuine guest catches ran. Normal resumed return must equal37+84+84=205.

The driver locates UNIQUE exact runs of four/five `nop` bytes ONLY within the actual bounded owned root expression diagnostic copy. This is a fixture PC selector, not a new bytecode validator or a restore/source/frame permission. Actual native `point` and synchronous current-ticket `before_park` remain the only capture issuer. The sole Linux keeper should cross-check the official assembler output and these two markers; the test never manufactures a compiled plan or a frame from these copied bytes.

Prerequisites:

- Both repositories' coherent ROOT-composed selected resume policy17 / raw-entry ABI2, full nested-control/EH producer, original-ABI native clone/getter context, legal returned-reference root handoff, and managed canonical execution classification.
- Actual scalar retirement must admit the emitted native lexical handler layout under the current sealed plan and typed current-owner proof. It MUST keep nonnull saved-input restrictions until the real staged new-world graph/issuer exists. The known old `handlers.empty()` refusals in capture/retirement/dispatcher are expected to fail this test until this prerequisite is actually connected; the test does not silently substitute normal execution.
- Complete actual GC/root/source/publication closure and typed state query producer; all root frames remain native/current during each observation.
- Official Core3 assembler for `struct.new`, `try_table`, `catch_all_ref`, `catch_ref`, `throw_ref`.

Run the built driver with `checkpoint_actual_nested_eh.wasm instruction`, then `checkpoint_actual_nested_eh.wasm unwind`. Use only the sole Linux keeper's original 64GiB cgroup and authorized CPU/domain lane. This packet ran NO compiler, LLVM verifier, assembler, native process or platform qualification.

Byte order: every copied debug numeric value is decoded with bounded FastIO `le_get<32>`; only the final real native ABI scalar is copied with FastIO `my_memcpy`. This design makes LE/BE expectations explicit, but this packet does NOT claim a big-endian execution pass.

Identity limitation: current `make_exn_reference` may mint a new carrier token on another catch of the same immutable exception record. Query-local object numbers currently deduplicate actual tokens. The test checks actual within-token aliases, canonical tag and immutable payload/GC aliases, but does NOT falsely use cross-query IDs or same-tag/payload equality as proof that a full checkpoint canonicalized the immutable exception instance. The complete census/restore issuer must separately preserve that semantic exception identity.

Primary normative references: https://webassembly.github.io/spec/core/text/instructions.html and https://webassembly.github.io/spec/core/exec/instructions.html (Core3, read2026-10-04), plus https://llvm.org/docs/ExceptionHandling.html. The private C++ control signal is a foreign native unwind, not a Wasm exception value, and must never be consumed by guest catch-all.

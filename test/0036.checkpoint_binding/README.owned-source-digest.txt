PRIVATE source-only helper R2, not production snapshot integration.

R1 allowed unparsed sources. Real full_source_instance still exposes a complete
mutable wasm_file_t via file_for_native_initialization() before its genuine
initializer seal, so the same source control block can legally replace that image
at native setup. Retaining only the source then does not pin the previously
hashed allocation. R2 refuses unsealed sources using initialized_from_actual_state:
it observes private initialized/main-module plus the actual published initializer
serial and per-module phase. No caller ready bool/epoch is accepted. Normal native
borrow contracts still require setup references not to escape into execution.

R2 hashes the SAME exclusively owned full original bytes after that real seal;
the bytes had to be frozen BEFORE parsing through owned_file_image adoption.
Hashing later that SAME unmodified private image is safe; this never hashes a
mutable mmap after parsing or reopens a pathname. All custom/debug/data bytes are
included. The cold result retains exact initialized source+SHA+length; it remains
DATA and does not prove full validation, selected runtime generation, build/cache
engine, imports, a complete stop/census or VM capture/restore capability.

Private initialized-source publisher must later authenticate source/module/plan/
profile/current ticket and retain this object. Generate once in cold compilation
publication and reuse; no generated memory access barrier, hashing, lock or other
hot-path operation is added. Public DATA/status cannot mint that publisher.

Actual CLI prepare_owned_full_cli_source still passes default image={} at
run/owned_source.h68; its owner can therefore have a mapped origin. Strict snapshot
must decline it. Future cold CLI source setup must read immutable private bytes
BEFORE parsing and pass the SAME image to load_wasm_file. Ordinary mapped cache
and execution semantics are not changed by this proposal.

The new storage partition is unregistered. No shared impl, CLI, runtime or cache
leaf is edited. The finite fixture uses true fast_io owned-file factories and
adoption, actual mmap negative origin, foreign control block and the new genuine
unsealed-source rejection. A/B SHA is byte DATA only. Positive source_digest::ok
requires real parser+initializer+seal and the current selected registry; it is
explicitly NOT mocked here and remains pending fresh full-native integration.
No compile/native/fixture has run. Tests only by solekeeper in original64GiB CG;
modern Wasm A/B from checkpoint-binding R2 should use official wasm-tools first.
Header/noEH first, then registered modules. No local/Mac compilation.

Cache accepted metadata is a separate issuer: source/complete context/ISA/profile
and actual owned signed blob/uncompressed object DATA must all be retained; only
successful full-engine finalization plus all entry resolution may publish its
complete ordered accepted bundle. Miss/fallback/test capture/store success are
not accepted-cache code. Debug-full remains OFF/debug_process_binding until real
relocation-safe qualification; do not invent object rows from fresh compilation.

Exact build remains unavailable without all six actual private product/provider
issuer digests. Runtime source/git/version macros or reopened same-name files do
not establish the loaded build. Source and compile/link/provider closure requires
an explicit non-self-reference manifest rule. This primitive never makes those
missing authorities appear qualified or selects the strong checkpoint codec.

R3 native pointer precondition: source must be a valid, live native pointee
created by the real factory. Existing has_canonical_owner dereferences it to
read its weak control-block identity; this helper is not an arbitrary-pointer
registry admission checker. The foreign-control-block negative uses the SAME
valid live source pointer, never a fabricated/dangling pointee. Before calling,
the future private publication issuer compares the actual canonical runtime
source registry owner's get/control-block identity under its real existing lease.
Those authorities cannot be minted by this helper or by an input ready boolean.
Fixture PTRDIFF bound uses numeric_limits, not a macro unexported by modules.

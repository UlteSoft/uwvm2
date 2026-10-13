# Same-byte EH reference addition, source v2

This adds three Wasmtime reference commands to the previous fixed inventory/memory64 source list. The six inventory commands and original memory64 commands are unchanged. No tool or native workload was run by this source preparation.

`current_wasm_reference_inner_commands_20261003_v2.json` binds the existing 177-byte plain-normal, 262-byte EH-normal and 275-byte caught-throw fixtures to their full hashes and scalar checksum/catches. Ordinary cold success does not qualify a reference engine. The two normal fixtures use 200M steps/checksum1231817216; throws uses 8M steps/500000 catches/checksum2464256.

The inner commands select cache=n, compiler=cranelift and collector=copying, with exceptions=n for the plain control and exceptions=y for the two EH controls. Confirm actual installed help/ELF/DSO first, then identical `_start` cold self-checks. Wasmtime v49.0.1 [CLI source](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v49.0.1/crates/cli-flags/src/lib.rs) defines these options; its [configuration source](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v49.0.1/crates/wasmtime/src/config.rs) has modern exceptions. These sources do not substitute for actual installed binary capability.

Run through the existing reference plain multi-TID protocol, retaining every TID's UID/P0/exact-cgroup checks, fresh original PIDFD authority, actual environment/capture-clear witness and real wait4. Whole-process wall/user/sys/RSS is the common first comparison; no Wasmtime internal Wasm timer is invented, and its result cannot be compared directly with uwvm's execution-only timer. Short or unknown identity/frequency samples stay unqualified. Temperature is observation only.

WAVM's pinned official candidate lacks modern try_table/throw_ref, so it receives no EH command. A feature rejection is unsupported, never a slow score. Existing WAVM inventory/memory64 commands remain the immediate serial next step after the current EH VTune retirement. Native execution remains solely the Linux keeper's responsibility.

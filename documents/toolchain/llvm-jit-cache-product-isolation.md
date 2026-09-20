# LLVM-JIT Cache v5: Product Isolation Between UWVM2 and ROS

2026-09-17. Both products raise the cache format version from 4 to 5 and the
runtime ABI schema version to 12. This is an intentional incompatible upgrade:
old cache entries are neither migrated nor executed; recompilation is sufficient,
and users' old cache files are not deleted.

| Layer | UWVM2 | ROS |
| --- | --- | --- |
| Default Unix root | `$XDG_CACHE_HOME/uwvm2/llvm-jit` | `$XDG_CACHE_HOME/uwvm2ros/llvm-jit` |
| Object location below any root | `uwvm2/objects/HH/uwvm2-HASH.uwvm-ljc` | `uwvm2ros/objects/HH/uwvm2ros-HASH.uwvm-ljc` |
| File magic | `UWVMLJC` + `01` | `UWVMROS` + `01` |
| Required context field | `product=uwvm2` | `product=uwvm2ros` |
| ABI schema | `uwvm2-runtime-abi-v12` | `uwvm2ros-runtime-abi-v12` |
| Signature derivation domain | `uwvm2-llvm-jit-cache-ed25519-seed-v2` | `uwvm2ros-llvm-jit-cache-ed25519-seed-v2` |

`HH` is a hash shard. The path-hash domain is also upgraded to
`uwvm-ljc-path-key-v2`. ROS uses `uwvm2ros` for macOS
`Library/Caches`, Windows `LOCALAPPDATA`, temporary directories, and fallback
paths used when environment variables are unavailable.

The product subdirectory is added by the **store layer**, rather than only by
the CLI or default-directory logic. The two product-name components in a default
path are intentional: an embedded program that constructs `cache_context`
directly, or either product explicitly given the same root directory, cannot
bypass the isolation. `cache_file_path` and the actual relative-directory open
path must remain synchronized; asynchronous writes reuse the same path flow.

Product identity is a reader/writer constant compiled into the binary; callers
cannot provide it. Even when both products have identical source IDs, cache
keys, and LLVM/CPU/ABI/codegen strings, their contexts differ. Changing a file
name or magic value cannot make a foreign object pass context validation.
When UWVM2 explicitly disables signature verification, the context must still
match exactly; the ROS CLI continues to sign and verify. Do not add
cross-product directory probing, skipped metadata comparisons, or automatic
native-object migration in the name of backward compatibility.

## Security Boundary

This is product, format, and ABI compatibility isolation; it is **not file-system
access control or a security sandbox**. The current signature seed is a
deterministic integrity identity, not a secret key protected from attackers.
It must not be claimed to defend against an attacker who can rewrite caches for
the same OS account, re-sign files, or manipulate directory links. Cache
directory ownership, permissions, and the host trust boundary remain important;
this change does not claim to provide directory ACL or symbolic-link hardening.

## Regression Test

Build `test/0014.llvm_jit/cache_product_isolation.cc` separately with each
product's headers, then run:

```sh
python3 test/0014.llvm_jit/check_cache_product_isolation.py \
  --full /path/to/full-fixture --ros /path/to/ros-fixture --out /new/result-directory
```

The Linux Clang 22 plus bundled LLVM 23.1.1 test passes 20 invocations. It
covers a shared root directory, bidirectional product swapping, modified magic,
signed and unsigned files, rejection of v4, damaged signed payloads, and normal
self-reads. The test payload is non-executable eight-byte data. Real MCJIT cache
hits and CFI re-registration are covered by a separate ROS CLI integration test.

When upgrading the schema, also update the deliberately corrupted ABI marker in
ROS `llvm_jit_cache_integration.cc`. Otherwise, the test will fail to find the
UWVM2 schema and will no longer execute its intended post-corruption fallback
check.

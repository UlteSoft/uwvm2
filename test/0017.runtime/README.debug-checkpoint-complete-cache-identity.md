# Complete checkpoint compilation tuple in the object-cache identity

The actual `compilation_profile::cache_identity()` contains ten scalar fields.
The old full-runtime callers serialized only indexes zero through eight, omitting
`native_workspace_limit`. Both products now delegate canonical decimal/slash
formatting to the profile's single serializer, which traverses the entire tuple.
Its static overload formats DATA and cannot create a compilation profile or mint
execution, stop, snapshot or restore authority.

The cache file format remains v5 and the runtime ABI remains v27. The tuple's
field/revision values are unchanged. Adding its tenth value changes the existing
`checkpoint-typed-entry-policy` value; mandatory context comparison and the path
key already include that value. Old nine-field context is rejected independently
of signature policy, and a different workspace cap obtains a different path.
A global cache-format/ABI bump would also invalidate unrelated ordinary caches
and is unnecessary for this precise correction.

The component fixture exercises the shared instance serializer, all ten individual
field mutations, a different workspace limit, the old nine-field value, actual
`make_context_metadata`/`cache_key_hash`, and full uint64 numeric formatting. It
performs no file IO and executes no native JIT code. Run header/noEH and module
configurations separately against both products, only through the existing Linux
64 GiB cgroup keeper. No local compiler/native execution is authorized by this
source proposal. These fixtures are new, not claimed run or passing.

The current debug-full path still disables object cache read/write because cached
host observer addresses lack rebinding proof. This change does not relax it.
Exact raw-Wasm, complete product/build/codegen identity, accepted-object provenance,
and persisted checkpoint identity require their own reviewed design. This tuple
correction alone does not satisfy whole-VM checkpoint or restore acceptance.

# Captured local availability: v5 source qualification

The runtime/code-generator owner preserves original local indices and declared
types when a fused validation/compiler stop has not proved initialization.
Its private packet carries 16-byte payload slots followed by explicit per-slot
availability flags. A false flag prevents the code generator from loading an
uninitialized LLVM local; controller capture and language queries must not infer
availability from a zero payload or a numeric type.

The source-only `copied_numeric_local` model adds `bool available{false}`.
Controller/runtime integration must set it from the genuine flag associated with
the same private stop ticket and original local index. False flags cannot be
overridden by metadata. Direct numeric values, `DW_OP_fbreg` frame bases and
`DW_OP_piece`/`DW_OP_bit_piece` local atoms check availability before copying any
carrier bytes. Their respective `local_unavailable` reasons identify the missing
value without manufacturing zero, a guest offset or a host pointer.

Later original local indices remain intact. A false flag leaves only its own
composite fragment unknown; independent available fragments retain their bytes
and known-bit masks. A scalar assembled from incomplete fragments remains
unavailable. Constants and absolute static offsets do not acquire any local-slot
dependency. Whole-object and selected-object displays still require the existing
same-ticket runtime root-copy/source-generation proof.

Hand-built components explicitly mark genuinely supplied test carriers available.
New cases cover unavailable direct values with unchanged later indices,
unavailable frame bases with stale numeric bytes, and incomplete local fragments
without loss of the next available fragment. These component tests do not prove
the actual compiler packet ABI or uninitialized LLVM-load elimination. Those
need a fresh joint compiler/runtime/controller test owned by the Linux keeper in
the existing 64 GiB/swap0 cgroup, including actual Wasm3 nondefaultable locals.
Old v1/v2/v3/v4 source freezes remain immutable; no old executable can qualify the
new packet/model semantics.

The runtime values witness now checks all producer flags before copying any
payload, clears each destination slot and preserves its original declared type
and index. Only true flags permit a payload copy. Its positive parameter case
requires an actual true flag before decoding the owned native i32 carrier.

`fixtures/debug_source_values_nondefaultable.wat` adds a genuine Core3
nondefaultable `(ref $node)` at original index 1 between a numeric parameter and
a later defaultable numeric local. The witness's `availability` scenario requires
the initial unreadable reference, later readable reference after `local.set`,
the same retained producer type, and the later numeric slot's genuine initial
and written values. False reference slots leave only cleared owned bytes; a
synthetic numeric query reports `local_unavailable`, and after a true flag still
reports `carrier_mismatch` rather than interpreting a reference as an integer.
The runner parses and validates this new syntax with wasm-tools, then queues
both instruction and unwind policies on a fresh candidate runtime object.

These flags describe the current fused validator's readability proof.
Conservative block/branch scope rules may make an already assigned local
unavailable again; the flag is neither an optimized-out diagnosis nor an exact
dynamic initialization/checkpoint state. The fixture's straight-line first-set
case specifically avoids that ambiguity. The new native cases remain pending
until the Linux keeper executes the fresh matched compiler/runtime/controller
candidate inside the required cgroup; source changes alone do not qualify them.

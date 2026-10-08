This cold DATA fixture exercises `owned_file_image::copy_owned_bytes` without
opening a file, parsing a module, executing guest code or issuing a checkpoint
capture/restore token. It belongs beside the existing file-read fixture; the
existing read/provider behavior is unchanged.

Compile and run `owned_bytes_copy.cc` only through the Linux keeper in the
existing 64 GiB cgroup, using the matching product sources and ordinary header
or named-module configuration. Both product fixtures are identical. This
private source proposal has not been compiled or executed; the named-module
path also remains unverified. The current image partition already imports
`<span>`, so this change requires no standard-library import migration.

The fixture covers a copied allocation that survives caller alias mutation and
input lifetime expiry, a second independent copy, all 256 byte values, exact
limit/too-small/zero/over-maximum/empty-input failures, immutable unique
ownership, owner move stability and preservation of the old image after
failed copies. It adopts the image into a genuine unparsed source owner and
rejects a distinct shared control block around that same live source pointer.
The image itself uses `unique_ptr<const owned_file_image>`; it does not have a
shared control block. Copied images retain zero host observation status
(`file_type::none`), never a synthesized regular-file/inode observation.

The caller must keep a valid native input extent stable for the synchronous
copy. This API cannot validate arbitrary machine addresses, concurrent writes
to the input, an already parsed pointer graph, source/build/cache identity,
native code, type closure or a live stopped-world restore transaction. A
successful byte copy grants none of those authorities. Preparing an isolated
fresh runtime, unpublished GC/reference fixups and actual old-world retirement
are separate prerequisites, covered by the fresh-world API source audit.

# First declarations of checkpoint source and restoration types

SOURCE-only current paired repair. No compiler or native test was run here.
ROOT alone applies live changes; the Linux keeper performs fresh BMI/header
qualification in the original cgroup. This packet is independent of stagedengine
R2's factory/include hunks and the separate module dependency repair packet.

In the actual storage `:wasm_module` interface, `wasm_module.h` includes
`gc_object.h` in the named module purview. Its first forward declaration of
`full_source_instance` was not exported. `:storage` imports `:wasm_module`,
and `:full` imports `:storage`; the same storage-attached class is then defined
inside an exported namespace in `full.h`. The first declaration must therefore
be exported; exporting a later class declaration cannot repair its introduction.

`gc_object.h` also first declares `restoration_context` in an existing
`extern "C++"` block without export. That declaration is attached to the global
module. The initializer `:init` imports the storage primary interface and exports
both its matching forward declaration and full definition. C++ linkage does not
make an earlier non-exported introduction exported. Its actual first storage
forward declaration now has export too, preserving the original global-module
attachment and qualified friend identity. Later non-exported redeclarations in
full.h inherit that export; they are not new owning-module declarations.

Only the two early namespace export markers and explanatory comments change.
The global runtime world-transaction/GC-borrow friends stay non-exported as
before, because their definitions are also private. `full_source_instance`
is not moved to a C++ linkage block; its class ownership stays the storage
named module. No constructor, method, layout, native byte/source/epoch authority,
ordinary IO, execution, validator or instruction code changes.

The actual source-derived rule is [module.interface]/6, with attachment and
same-entity rules from [module.unit]/7 and [basic.link]/8. References:
https://eel.is/c++draft/module.interface
https://eel.is/c++draft/module.unit
https://eel.is/c++draft/basic.link

Keeper positive controls: independently precompile actual storage wasm_module,
storage and full partitions and initializer init, then their primary interfaces,
under each product's genuine module configuration. Precompile the new first-storage
capsule and compile its consumer, with only fresh matched dependencies. The
fixture obtains the original exported storage source and global restoration
names, then checks the actual initializer full definition is the same class.
Private GC stage/source/initializer constructors remain unavailable; no fake
friend class grants access. Header-only controls compile the existing real
`debug_checkpoint_restoration_context_api.cc` in both products/include orders.

Independent isolated negatives: restore ONLY the first source forward to a
non-exported declaration and rebuild storage/full from fresh BMIs; restore ONLY
the first restoration forward to non-export and rebuild initializer/init with
the source forward repair retained. They must reject exported redeclaration
after non-exported introduction. Do not treat Python/grep or compiler accepting
only a forward declaration as a successful all-product module/runtime build.
This scope does not qualify global/header mixed build ABI, snapshot world restore,
LLVM/OS/EH/CFI/ASM code ranges or dynamic native authority.

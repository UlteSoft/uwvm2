# Current checkpoint module dependency repair (SOURCE-only)

This packet is an exact-current narrow prerequisite for staged LLVM engine R2,
not an engine/world-restoration implementation. ROOT is the sole publisher.
No compiler, BMI builder, WAT parser, VM or native test was run by this author.

The runtime factory stores `staged_compiler_module_owner` by value. Its actual
complete definition is exported from `uwvm2.uwvm.runtime.initializer:init` and
re-exported by the initializer primary interface. Compiler translation's
`extern "C++"` declaration is incomplete; storage and the debugger aggregate do
not import that definition. The textual native implementation now includes
`initializer/init.h`; the runtime implementation module directly imports the
initializer primary interface after its own module declaration. Both dependencies
use the identical factory feature guard and leave the runtime primary API free
of an initializer import. There is no initializer -> runtime interface dependency
in the actual current import closure (including conditional lexical edges).

`managed_collection.h` directly names the collection pause domain and
`current_root_frames`. `collection_transaction` imports their modules without
re-export, so its importer cannot find those names merely because their
definitions are reachable. The leaf now imports the actual thread and frame-root
owners directly. No execution, memory, GC/root or permission behavior changes.

Apply the include/import hunks against their pinned current preimages; the
unapplied engine R2 separately adds its default.cpp tail include and compiler
scope. Those tail/compiler hunks do not overlap this packet. Do not overwrite a
current default.cpp with an old candidate or run these fixtures against stale
BMIs. The fixture requires the engine R2 compiler scope when LLVM is selected.

The Linux keeper must compile actual current managed_collection.cppm, actual
initializer init.cppm, LLVM translate.cppm, runtime primary cppm and runtime
implementation module as independent inputs under the same product feature
macros, compiler and fresh dependency BMIs. In header mode compile the actual
runtime.default.cpp independently; including initializer in a test alone is
not a positive control for the production factory closure. Then precompile the
new topology capsule and syntax/object-compile its consumer, both repositories.
The capsule checks actual complete source/storage/initializer/checkpoint types
and keeps native source, GC staging, restoration and object scope construction
private. It does not define an impersonating transaction friend.

Required negative compiler controls, isolated from production: remove only the
runtime initializer dependency with the engine enabled, which must diagnose the
incomplete by-value owner; remove only the GC leaf's direct imports, which must
diagnose its directly used pause-domain/frame-root names. These are not byte
source-string checks or successful snapshot/native tests. Run on SSH Linux under
the original 64GiB cgroup via the sole native keeper. Native execution/restore,
unwind/CFI, ASM endpoint authority and cross-platform qualification stay separate.

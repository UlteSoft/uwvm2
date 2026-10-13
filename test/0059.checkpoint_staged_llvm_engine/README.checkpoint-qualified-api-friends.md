# Qualified friends of global runtime API functions

PRIVATE current SOURCE repair only; no compile, BMI, link, execution or restore
qualification was performed by its author. ROOT is the sole live publisher.

The genuine runtime primary interface declares its host API functions inside
`extern "C++"` declarations. These functions are attached to the global module
by [module.unit]/7.2.4. Its source-binding, activation-capture, native-cursor and
checkpoint capture-wrapper classes are instead attached to `uwvm2.runtime`.
Their implementations previously used unqualified function friend declarations
outside a linkage specification. Such friends are attached to the containing
runtime module by [module.unit]/7.3, rather than the existing global functions.
This is an attachment conflict for the corresponding declarations under
[basic.link]/8 and /10; inheritance of C++ language linkage alone cannot fix it.

Each selected friend now uses a fully qualified declarator-id that names the
actual prior namespace API, as prescribed by [module.unit]/7.1. The trailing
return form retains the same return, parameter and noexcept function types and
avoids parsing a class return type as part of a nested-name-specifier. The
containing classes keep their original runtime module attachment. Constructor
privacy, canonical owner registries, all runtime checks, layout, generated IR,
function names and native ABI are unchanged. Existing qualified class friends
and private global classes declared inside C++ linkage blocks are untouched.

Primary references:
https://eel.is/c++draft/module.unit
https://eel.is/c++draft/basic.link
https://eel.is/c++draft/dcl.link

Keeper controls, independently for both products, using fresh matched BMIs and
the actual enabled native-thread/LLVM/exception configuration:

1. Precompile the genuine runtime primary interface and its dependency closure,
   then COMPILE the actual `uwvm_runtime.module.cpp` implementation. The fixture
   alone cannot qualify these private class declarations.
2. Precompile the new capsule and compile/link its consumer against that actual
   runtime implementation. The consumer takes the original exported API
   addresses but never calls them or grants VM/debug/checkpoint permission.
3. Header control: compile the actual `uwvm_runtime.default.cpp` with the same
   production configuration, and compile/link existing genuine API consumers.
4. Isolated negative: restore ONLY one prior unqualified source-binding friend
   in an otherwise fixed temporary source cut and rebuild the real runtime
   implementation with fresh BMIs. The corresponding global/runtimemodule
   declarations must reject. Repeat one actual checkpoint wrapper friend,
   leaving its class ownership and extern C++ API prototype untouched.

No hypothetical synthetic friend class is used in the positive capsule. This
packet does not qualify mixed header/module ABI, all platform module frontends,
permission boundaries, GC, EH, owner endpoints, world restore or performance.

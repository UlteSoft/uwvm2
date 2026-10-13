// Header/import smoke only: no loader, guest, LLVM materialization or timing.
// Compile this translation unit under the actual gate-undefined/0/1/2 recipes.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <type_traits>
namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
// Existing lazy metadata assignment must remain legal even when the full-only
// observation fields are compiled in. This does not enable lazy recording.
static_assert(::std::is_copy_constructible_v<compiler::local_func_storage_t>);
static_assert(::std::is_copy_assignable_v<compiler::local_func_storage_t>);
static_assert(::std::is_copy_constructible_v<compiler::compile_option>);
int main()
{
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    compiler::compile_option options{};
    // All permissions/requests remain absent by default. Compile-only smoke
    // need not execute this branch to check the real field and type interfaces.
    return options.record_native_eh_leaf_observations || options.native_eh_leaf_source_owner ||
        options.native_eh_leaf_active_attempt ? 1 : 0;
#else
    return 0;
#endif
}

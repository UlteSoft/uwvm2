// Compile/import smoke only. No source loading, guest or materialization.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <type_traits>
namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
static_assert(::std::is_copy_constructible_v<compiler::local_func_storage_t>);
static_assert(::std::is_copy_assignable_v<compiler::local_func_storage_t>);
static_assert(::std::is_copy_constructible_v<compiler::compile_option>);
int main()
{
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
    compiler::compile_option options{};
    return options.stage_native_eh_private_leaf ? 1 : 0;
#else
    return 0;
#endif
}

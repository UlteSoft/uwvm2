#include <type_traits>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

// Real compiler API surface, not a fabricated initialized-source/native-entry
// test. Positive private scope construction requires the genuine source owner
// and remains reserved for the integrated world-transaction fixture.
int main()
{
    using scope = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::private_host_object_emission_scope;
    using owner = ::uwvm2::uwvm::runtime::initializer::staged_compiler_module_owner;
    static_assert(!::std::is_default_constructible_v<scope>);
    static_assert(!::std::is_constructible_v<scope, owner const&, ::llvm::Module const&>);
    static_assert(!::std::is_constructible_v<scope, ::llvm::Module const*>);
    static_assert(!::std::is_copy_constructible_v<scope> && !::std::is_move_constructible_v<scope>);
    static_assert(!::std::is_copy_assignable_v<scope> && !::std::is_move_assignable_v<scope>);
    ::llvm::LLVMContext context{};
    ::llvm::Module ordinary{"staged-scope-default", context}, foreign{"staged-scope-foreign", context};
    if(scope::select(&ordinary) != scope::selection::ordinary ||
       scope::select(&foreign) != scope::selection::ordinary ||
       scope::select(nullptr) != scope::selection::ordinary) { return 1; }
    return 0;
}

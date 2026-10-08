// Actual product module topology fixture; no invented friend or restore owner.
module;
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#if defined(UWVM_RUNTIME_LLVM_JIT)
// The SDK declaration is global; importing a module does not export its GMF names.
#include <llvm/IR/Module.h>
#endif
export module uwvm2test.checkpoint_module_topology;
import uwvm2.runtime.checkpoint;
import uwvm2.runtime.gc.managed_collection;
import uwvm2.uwvm.runtime.storage;
import uwvm2.uwvm.runtime.initializer;
#if defined(UWVM_RUNTIME_LLVM_JIT)
import uwvm2.runtime.compiler.llvm_jit.compile_all_from_uwvm;
#endif
export namespace uwvm2test::checkpoint_module_topology
{
    using source = ::uwvm2::uwvm::runtime::full::full_source_instance;
    using compiler_owner = ::uwvm2::uwvm::runtime::initializer::staged_compiler_module_owner;
    using restore_context = ::uwvm2::uwvm::runtime::initializer::restoration_context;
    using gc_stage = ::uwvm2::uwvm::runtime::storage::checkpoint_gc_staging;
    using plan = ::uwvm2::runtime::checkpoint::sealed_function_plan;
    using packet = ::uwvm2::runtime::checkpoint::logical_frame;
    inline constexpr bool actual_owner_complete{sizeof(compiler_owner) != 0u};
    inline constexpr bool actual_owner_copyable{::std::is_copy_constructible_v<compiler_owner>};
    inline constexpr bool source_nonmoving{!::std::is_copy_constructible_v<source> &&
        !::std::is_move_constructible_v<source>};
    inline constexpr bool restore_private{!::std::is_default_constructible_v<restore_context> &&
        !::std::is_constructible_v<restore_context, source::mutable_owner, gc_stage&>};
    inline constexpr bool stage_private{!::std::is_default_constructible_v<gc_stage>};
    static_assert(actual_owner_complete && actual_owner_copyable && source_nonmoving && restore_private && stage_private);
    static_assert(::std::is_same_v<decltype(::std::declval<compiler_owner const&>().source()), source::owner const&>);
    static_assert(::std::is_same_v<decltype(::std::declval<compiler_owner const&>().module()),
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const*>);
#if defined(UWVM_RUNTIME_LLVM_JIT)
    using object_scope = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::private_host_object_emission_scope;
    inline constexpr bool object_scope_private{!::std::is_default_constructible_v<object_scope> &&
        !::std::is_constructible_v<object_scope, compiler_owner const&, ::llvm::Module const&> &&
        !::std::is_copy_constructible_v<object_scope> && !::std::is_move_constructible_v<object_scope>};
    static_assert(object_scope_private);
#endif
}

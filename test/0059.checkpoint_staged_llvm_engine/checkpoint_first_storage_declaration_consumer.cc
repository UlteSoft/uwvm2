// Import the real full definition after obtaining its opaque exported name
// through the actual storage module. Both declarations must be one entity.
#include <memory>
#include <type_traits>
import uwvm2test.checkpoint_first_storage_declaration;
import uwvm2.uwvm.runtime.initializer;
namespace first = ::uwvm2test::checkpoint_first_storage_declaration;
namespace initialization = ::uwvm2::uwvm::runtime::initializer;
static_assert(::std::is_same_v<first::restoration, initialization::restoration_context>);
static_assert(sizeof(first::restoration) != 0u);
static_assert(::std::is_same_v<first::source::owner, ::std::shared_ptr<first::source const>>);
static_assert(!::std::is_constructible_v<first::restoration, first::source::mutable_owner, first::stage&>);
static_assert(!::std::is_default_constructible_v<initialization::staged_compiler_module_owner>);
static_assert(::std::is_copy_constructible_v<initialization::staged_compiler_module_owner>);
static_assert(first::source_nonmoving && first::stage_private);
// Compilation only; this source constructs no transaction/restoration stage,
// emits no VM execution, and issues no runtime or saved-state permission.

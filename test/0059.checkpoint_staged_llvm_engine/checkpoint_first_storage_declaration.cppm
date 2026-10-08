// The actual first storage declarations must be usable across a module
// boundary. No fresh global/source/friend declaration is synthesized here.
module;
#include <type_traits>
export module uwvm2test.checkpoint_first_storage_declaration;
import uwvm2.uwvm.runtime.storage;
export namespace uwvm2test::checkpoint_first_storage_declaration
{
    using source = ::uwvm2::uwvm::runtime::full::full_source_instance;
    using restoration = ::uwvm2::uwvm::runtime::initializer::restoration_context;
    using stage = ::uwvm2::uwvm::runtime::storage::checkpoint_gc_staging;
    inline constexpr bool source_nonmoving{!::std::is_copy_constructible_v<source> &&
        !::std::is_move_constructible_v<source>};
    inline constexpr bool stage_private{!::std::is_default_constructible_v<stage>};
    static_assert(sizeof(source) != 0u && source_nonmoving && stage_private);
}

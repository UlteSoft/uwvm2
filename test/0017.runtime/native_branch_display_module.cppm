module;
#include <cstddef>
#include <cstdint>
export module uwvm2test.native_branch_display;
import uwvm2.uwvm.debugger;
export constexpr bool branch_display_is_optional_data() noexcept
{
    using namespace ::uwvm2::uwvm::debugger;
    native_branch_display::annotation unknown{};
    return !unknown && native_branch_display::resolve_current_owner(unknown, 0u, 0u, true).resolution ==
        native_branch_display::symbol_resolution::no_owned_function_range;
}
static_assert(branch_display_is_optional_data());

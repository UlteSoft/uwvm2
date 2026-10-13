module;
#include <cstddef>
#include <cstdint>
#include <type_traits>
export module uwvm2test.native_actual_target_metadata;
import uwvm2.runtime;
import uwvm2.uwvm.debugger;
export constexpr bool actual_target_format_is_owned_data() noexcept
{
    ::uwvm2::runtime::lib::llvm_jit_debug_native_target target{};
    target.description_version = 1u;
    return !::uwvm2::uwvm::debugger::native_target_metadata::valid(target) &&
           target.triple.size() == 256u && target.features.size() == 16384u;
}
static_assert(actual_target_format_is_owned_data());

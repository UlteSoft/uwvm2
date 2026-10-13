module;
#include <cstddef>
#include <cstdint>
export module uwvm2test.native_branch_destination;
import uwvm2.uwvm.debugger;
export constexpr bool branch_destination_data_not_permission() noexcept
{
    using namespace ::uwvm2::uwvm::debugger;
    native_branch_destination::result unknown{};
    native_owned_instruction_semantics::decoded_instruction decoded{};
    return !unknown && !decoded.destination() && !decoded.safe_for_single_instruction();
}
static_assert(branch_destination_data_not_permission());

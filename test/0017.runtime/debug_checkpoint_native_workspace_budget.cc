// A native-workspace quota is separate from heap metadata/whole-stack limits.
// Component DATA never grants a runtime stop, resource export or restore ticket.
#include <uwvm2/runtime/checkpoint/shadow_ledger.h>
#include <fast_io.h>
#include <limits>
namespace cp = ::uwvm2::runtime::checkpoint;
static void require(bool valid, char const* message)
{
    if(!valid) { ::fast_io::io::perrln("checkpoint native workspace: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static_assert(cp::native_workspace_limit == 32768u);
static_assert(cp::native_workspace_fits(961u, 961u));
static_assert(!cp::native_workspace_fits(962u, 962u));
static_assert(cp::native_workspace_fits(0u, 2044u));
static_assert(!cp::native_workspace_fits(0u, 2045u));
static_assert(!cp::native_workspace_fits(1u, 0u));
static_assert(!cp::native_workspace_fits((::std::numeric_limits<::std::size_t>::max)(),
                                      (::std::numeric_limits<::std::size_t>::max)()));
static_assert(!cp::native_workspace_fits(0u, (::std::numeric_limits<::std::size_t>::max)()));
int main()
{
    auto const profile{cp::compilation_profile::create_for_trusted_manager()};
    require(bool(profile) && profile->limits().slots_per_frame > 1000u, "heap metadata budget is not native-frame permission");
    auto const key{profile->cache_identity()};
    require(key.size() == 10u && key[1u] == 17u && key[9u] == cp::native_workspace_limit,
        "cache identity retains exact new compilation policy and native workspace quota");
    cp::function_plan declined{}; declined.profile = profile; declined.function_generation = 1u; declined.expression_bytes = 3u;
    declined.producer_availability = cp::status::quota_exceeded;
    require(cp::validate_plan(declined) == cp::status::ok && declined.compiler_failure == cp::status::ok && declined.sites.empty(),
        "valid compiled function can retain a precise recording decline without rejecting guest code");
    auto const sealed{cp::sealed_function_plan::seal_compiler_metadata(declined)};
    require(bool(sealed) && sealed->get().producer_availability == cp::status::quota_exceeded,
        "declined metadata remains immutable and inspectable without an invented entry site");
    cp::shadow_ledger ledger{profile};
    cp::activation_identity const actual_component_identity{1u, 0u, 2u, 3u};
    require(ledger.enter(actual_component_identity, sealed) == cp::status::quota_exceeded &&
        ledger.frames_while_actually_stopped().empty() && ledger.failure() == cp::status::quota_exceeded,
        "recording refuses before frame values are created and the decline is sticky");
    require(ledger.executable_restore_capability() == cp::status::unavailable_resume,
        "a budget result is DATA and cannot authorize restore");
    auto invalid{declined}; invalid.producer_availability = cp::status::ok;
    require(cp::validate_plan(invalid) == cp::status::invalid_plan,
        "empty available plan cannot masquerade as complete materialization");
    invalid = declined; invalid.compiler_failure = cp::status::invalid_layout;
    require(cp::validate_plan(invalid) == cp::status::invalid_layout,
        "real malformed producer metadata is never hidden by a resource decline");
    ::fast_io::io::println("checkpoint native-workspace quota DATA PASS; native frame/whole-instance restore acceptance=false");
}

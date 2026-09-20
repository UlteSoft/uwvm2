// Isolate the production callback from the CLI registry/LLVM build. Only its
// surrounding parameter/state declarations and usage-printer result are test
// doubles; the callback body and fast_io's real lockable native sink are used.
// This is a driver fixture (.cpp), not an automatically registered .cc unit.
#include <fast_io.h>
#include <fast_io_dsal/string_view.h>
#include <array>
#include <thread>
#include <uwvm2/utils/macro/push_macros.h>
#define UWVM 2
#include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>

namespace uwvm2::utils::cmdline
{
    enum class parameter_return_type { def, return_m1_imme };
    enum class parameter_parsing_results_type { arg, occupied_arg };
    struct parameter_parsing_results
    {
        parameter_parsing_results_type type{parameter_parsing_results_type::arg};
        fast_io::u8string_view str{};
    };
    inline constexpr fast_io::u8string_view print_usage(int) { return fast_io::u8string_view{u8"USAGE"}; }
}
namespace uwvm2::uwvm::utils::ansies { inline bool put_color{}; }
namespace uwvm2::uwvm::io
{
    inline fast_io::basic_io_lockable_nonmovable<fast_io::u8native_file> u8log_output{fast_io::io_dup, fast_io::u8err()};
}
namespace uwvm2::uwvm::cmdline::params { inline constexpr int runtime_llvm_jit_full_policy{}; }
namespace uwvm2::uwvm::runtime::runtime_mode
{
    enum class runtime_llvm_jit_full_policy_t { auto_policy, debug, legacy_light, passbuilder_o1, passbuilder_o2, passbuilder_o3 };
    inline runtime_llvm_jit_full_policy_t global_runtime_llvm_jit_full_policy{};
    inline bool runtime_llvm_jit_policy_existed{};
}

// Skip the umbrella includes only. The real callback code is still compiled.
#define UWVM_MODULE
#define UWVM_RUNTIME_LLVM_JIT
#ifndef UWVM_TEST_POLICY_HEADER
# define UWVM_TEST_POLICY_HEADER <uwvm2/uwvm/cmdline/callback/runtime_llvm_jit_full_policy.h>
#endif
#include UWVM_TEST_POLICY_HEADER

static int exercise(unsigned scenario)
{
    using namespace uwvm2::utils::cmdline;
    using namespace uwvm2::uwvm::runtime::runtime_mode;
    constexpr fast_io::u8string_view policies[]{u8"auto", u8"debug", u8"legacy-light", u8"pb-o1", u8"pb-o2", u8"pb-o3"};
    parameter_parsing_results args[2]{};
    args[1].str = scenario < 6u ? policies[scenario] : fast_io::u8string_view{u8"broken-policy"};
    auto const result{uwvm2::uwvm::cmdline::params::details::runtime_llvm_jit_full_policy_callback(
        args, args, args + (scenario == 7u ? 1u : 2u))};
    if(scenario < 6u)
    {
        if(result != parameter_return_type::def || global_runtime_llvm_jit_full_policy != static_cast<runtime_llvm_jit_full_policy_t>(scenario))
        { return 92; }
    }
    else if(result != parameter_return_type::return_m1_imme) { return 93; }
    bool const consumed{args[1].type == parameter_parsing_results_type::occupied_arg};
    if(consumed != (scenario < 7u)) { return 94; }
    return 0;
}

int main(int argc, char** argv)
{
    if(argc != 3 && argc != 4) { return 90; }
    unsigned const scenario{static_cast<unsigned>(argv[2][0] - '0')};
    if(scenario > 8u) { return 91; }
    uwvm2::uwvm::utils::ansies::put_color = argv[1][0] == '1';
    uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_policy_existed = scenario == 8u;
    if(argc == 3) { return exercise(scenario); }
    if(scenario != 6u) { return 95; }
    // All shared policy/color state is read-only on this error path. If the
    // outer stream lock is lost, the four print chunks can interleave here.
    std::array<std::thread, 4> workers;
    std::array<int, 4> results{};
    for(unsigned i{}; i != workers.size(); ++i)
    {
        workers[i] = std::thread{[&, i]
        {
            for(unsigned n{}; n != 64u; ++n)
            { if(auto const result{exercise(6u)}; result != 0) { results[i] = result; } }
        }};
    }
    for(auto& worker: workers) { worker.join(); }
    for(auto result: results) { if(result != 0) { return result; } }
    return 0;
}

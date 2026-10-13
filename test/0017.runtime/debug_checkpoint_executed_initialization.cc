// Independent logical-state component only. Real compiler native local-store
// instrumentation / coherent frame restoration remain separate qualification.
#include <uwvm2/runtime/checkpoint/executed_initialization.h>
#include <fast_io.h>
namespace cp = ::uwvm2::runtime::checkpoint;
static void require(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("checkpoint initialization: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static cp::sealed_function_plan::owner plan()
{
    cp::function_plan p{}; p.profile = cp::compilation_profile::create_for_trusted_manager();
    p.expression_bytes = 16u; p.function_generation = 1u;
    cp::safepoint_layout entry{}; entry.identifier = 1u; entry.local_count = 4u;
    // Nonnull parameter, nonnull declared local, numeric, nullable reference.
    entry.slots = {{{cp::types::value_kind::reference, {0}, false}, true},
                   {{cp::types::value_kind::reference, {0}, false}, false},
                   {{cp::types::value_kind::i32}, true},
                   {{cp::types::value_kind::reference, {0}, true}, true}};
    entry.controls = {{cp::control_kind::function}}; p.sites.push_back(::std::move(entry));
    return cp::sealed_function_plan::seal_compiler_metadata(::std::move(p));
}
int main()
{
    auto const source{plan()}; require(bool(source), "immutable exact entry metadata");
    cp::executed_local_initialization wrong_parameters{source, 0u}, unset_parameter{source, 2u}, excess_parameters{source, 5u};
    require(wrong_parameters.state() == cp::status::invalid_plan && wrong_parameters.flags().empty(),
        "nonnull initialized slot cannot be classified as an unset declared local");
    require(unset_parameter.state() == cp::status::invalid_plan && unset_parameter.flags().empty(),
        "nonnull incoming parameter cannot be classified as unset");
    require(excess_parameters.state() == cp::status::invalid_plan && excess_parameters.flags().empty(),
        "actual parameter count cannot exceed complete locals extent");
    cp::executed_local_initialization left{source, 1u}, right{source, 1u};
    require(left.state() == cp::status::ok && left.readable(0u) && !left.readable(1u) && left.readable(2u) && left.readable(3u),
        "parameters/defaultable locals initialized; nondefaultable declared local unset");
    require(left.record_executed_assignment(1u) == cp::status::ok && left.readable(1u) && !right.readable(1u),
        "only the actually executed assignment path changes the flag");
    // No conservative validator checkpoint rollback operation exists here:
    // a merge/else/end is not execution of a new local allocation.
    require(left.readable(1u), "executed assignment survives a validation-proof merge");
    require(left.record_executed_assignment(4u) == cp::status::invalid_layout && left.readable(1u),
        "out-of-range original local cannot change state");
    auto const saved{::std::vector<::std::uint8_t>{left.flags().begin(), left.flags().end()}};
    require(left.reset_after_actual_new_activation() == cp::status::ok && !left.readable(1u) && left.readable(0u),
        "actual new activation/self-tail resets declared locals while retaining initialized parameters");
    require(left.replace_compatible_flag_data(saved) == cp::status::ok && left.readable(1u), "bounded compatible executed-state flag data");
    auto corrupt{saved}; corrupt[0u] = 0u;
    require(left.replace_compatible_flag_data(corrupt) == cp::status::invalid_layout && left.readable(0u) && left.readable(1u),
        "no fabricated uninitialized parameter and failure is atomic");
    corrupt = saved; corrupt[3u] = 0u;
    require(left.replace_compatible_flag_data(corrupt) == cp::status::invalid_layout && left.readable(3u), "nullable defaultable local cannot become unset");
    corrupt = saved; corrupt[1u] = 2u;
    require(left.replace_compatible_flag_data(corrupt) == cp::status::invalid_layout && left.readable(1u), "noncanonical flag cannot forge state");
    require(left.replace_compatible_flag_data({saved.data(), saved.size() - 1u}) == cp::status::invalid_layout && left.readable(1u),
        "truncated initialized-state bytes rejected before replacement");
    require(left.executable_restore_capability() == cp::status::unavailable_resume, "flag compatibility is not executable restore authorization");
    ::fast_io::io::println("checkpoint executed-local-init component PASS; actual native producer/whole restore acceptance=false");
}

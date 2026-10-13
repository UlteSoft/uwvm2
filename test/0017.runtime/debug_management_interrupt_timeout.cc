// Actual cooperative-domain/controller management timeout component.
// This does NOT simulate JIT frames/native traps or qualify source step/NI.
#include <uwvm2/uwvm/debugger/controller.h>
#include <chrono>
#include <cstdint>
#include <fast_io.h>
namespace dbg = ::uwvm2::uwvm::debugger;
namespace ctl = ::uwvm2::utils::control;
struct flag { unsigned queries{}; bool value{true}; };
static bool transient_query(void* context) noexcept
{
    auto& state{*static_cast<flag*>(context)}; ++state.queries;
    auto value{state.value}; state.value = false; return value;
}
static void check(bool value, char const* message) noexcept
{
    if(!value) { ::fast_io::io::perrln("debug_management_interrupt_timeout: FAIL ",
        ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static ctl::launch_config config()
{
    ctl::launch_config value{}; value.debug_enabled = true;
    value.compiler = ctl::backend::llvm; value.mode = ctl::compile_mode::full;
    value.origin = ctl::launch_origin::console; value.instance[0] = 1u;
#if defined(_WIN32) && !defined(__CYGWIN__)
    value.vm_process = ::fast_io::win32::GetCurrentProcessId();
#elif defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
    value.vm_process = static_cast<::std::uint64_t>(dbg::posix_abi::getpid_noexcept());
#else
# error actual host process identity adapter is required for this component
#endif
    return value;
}
int main()
{
    for(bool interrupt : {false, true})
    {
        auto owner{dbg::controller::create(config(), 1u)};
        check(owner && owner->status() == ctl::error::none, "real host console authority");
        auto domain{owner->domain()};
        // This HOST component really admits one current-thread participant and
        // deliberately does not call poll during execute. No fabricated trace,
        // JIT location, external_park or caller-selected capture ticket is used.
        auto participant{domain->enter()}; check(static_cast<bool>(participant), "real admission");
        flag input{};
        auto command{dbg::parse_console_command("pause")};
        auto reply{owner->execute(command, ::std::chrono::milliseconds{40},
            interrupt ? dbg::management_wait_interrupt{&input, &transient_query} : dbg::management_wait_interrupt{})};
        check(reply.timed_out, "uncooperative real participant reaches deadline");
        check(reply.execution != dbg::execution_status::stopped && reply.stop_identifier == 0u,
              "timeout cannot manufacture a complete stop or public stop label");
        check(domain->pause_requested() == interrupt,
              "observed Ctrl+C retains actual ticket; ordinary timeout keeps existing resume policy");
        check(reply.execution == (interrupt ? dbg::execution_status::stopping : dbg::execution_status::running),
              "real controller snapshot matches incomplete-domain admission state");
        if(interrupt) { check(input.queries == 1u && !input.value, "first observed transient HOST flag stays latched"); }
        participant.reset(); // genuine lifetime release, not a fake parked frame
        auto actual{owner->inspect()};
        check(actual.execution == (interrupt ? dbg::execution_status::stopped : dbg::execution_status::running),
              "only actual domain capture after participant retirement admits empty-cohort stop");
        if(interrupt)
        {
            check(actual.stop_identifier != 0u, "actual retained pause has a current public label");
            auto resumed{owner->execute(dbg::parse_console_command("continue"))};
            check(resumed.execution == dbg::execution_status::running && !domain->pause_requested(),
                  "explicit host continue releases the retained actual request");
        }
    }
    ::fast_io::io::println("debug_management_interrupt_timeout: PASS");
}

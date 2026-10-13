// Real Linux descriptors; no guest, controller, or endpoint identity stubs.
#include <uwvm2/utils/control/sealed_input.h>
#include <fast_io.h>
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#if !defined(__linux__)
# error "This fixture qualifies Linux console socket admission only."
#endif
namespace ctl = ::uwvm2::utils::control;
#define CHECK(value) do { if(!(value)) { ::fast_io::io::perrln("FAIL Linux console socket seal line=", __LINE__, " errno=", errno); return 1; } } while(false)
int main()
{
    CHECK(!ctl::console_input_sealed());
    for(int type : {SOCK_STREAM, SOCK_SEQPACKET})
    {
        int endpoints[2]{-1, -1};
        CHECK(::socketpair(AF_UNIX, type | SOCK_CLOEXEC, 0, endpoints) == 0);
        ::fast_io::native_file input{endpoints[0]}, guest_output{endpoints[1]};
        auto const before{::fcntl(input.native_handle(), F_GETFD)};
        CHECK(before >= 0);
        // Distinct peer endpoint identities cannot prove ordinary console isolation.
        CHECK(ctl::seal_console_input_host_api(input.native_handle()) == ctl::sealed_input_status::unsupported_platform);
        CHECK(!ctl::console_input_sealed());
        CHECK(::fcntl(input.native_handle(), F_GETFD) == before);
    }
    int endpoints[2]{-1, -1};
    CHECK(::pipe2(endpoints, O_CLOEXEC) == 0);
    ::fast_io::native_file input{endpoints[0]}, guest_output{endpoints[1]};
    CHECK(ctl::seal_console_input_host_api(input.native_handle()) == ctl::sealed_input_status::ok);
    CHECK(ctl::console_input_sealed());
    CHECK(ctl::inspect_guest_file_host_api(input.native_handle()) == ctl::sealed_input_decision::denied);
    CHECK(ctl::inspect_guest_output_host_api(guest_output.native_handle()) == ctl::sealed_input_decision::denied);
    // Independent SDK oracle proves this rejected output really feeds this input.
    char const expected{'x'}; char observed{};
    CHECK(::write(guest_output.native_handle(), &expected, 1u) == 1);
    CHECK(::read(input.native_handle(), &observed, 1u) == 1 && observed == expected);
    ctl::unseal_console_input_after_guest_drain_host_api(); // No guest has entered.
    CHECK(!ctl::console_input_sealed());
    ::fast_io::io::println("PASS ordinary stream/seqpacket console sockets refused; pipe input/output aliases denied");
}

// Finite actual-target qualification: parent must supply a real stdin pipe.
// Linux's separately owned nonblocking provider is deliberately NOT simulated.
#include <uwvm2/uwvm/debugger/console_keyboard.h>
#include <fast_io.h>
#if (defined(__unix__) || defined(__APPLE__)) && !defined(__linux__)
# if defined(__APPLE__)
extern "C" int keyboard_refusal_sigismember_noexcept(::sigset_t const*, int) noexcept __asm__("_sigismember");
# else
extern "C" int keyboard_refusal_sigismember_noexcept(::sigset_t const*, int) noexcept __asm__("sigismember");
# endif
static void check(bool value, ::fast_io::string_view message)
{ if(!value) { ::fast_io::io::perrln(::fast_io::err(), "debug_console_nonlinux_pipe_refusal: FAIL ", message); ::fast_io::fast_terminate(); } }
#endif
int main()
{
#if (defined(__unix__) || defined(__APPLE__)) && !defined(__linux__)
    namespace keyboard = ::uwvm2::uwvm::debugger::console_keyboard;
    auto const input{::fast_io::posix_io_observer{::fast_io::in().native_handle()}};
    auto const before{::fast_io::posix_status_nothrow(input)};
    auto const flags{::fast_io::posix_getfl_nothrow(input)};
    check(static_cast<bool>(before) && before.value.type == ::fast_io::file_type::fifo && static_cast<bool>(flags), "fixture requires actual pipe identity and flags");
    struct ::sigaction prior{}, during{}, after{};
    check(keyboard::details::sigaction_noexcept(SIGINT, nullptr, ::std::addressof(prior)) == 0, "actual predecessor query");
    {
        keyboard::native_session session{};
        check(!session.ready() && !session.interactive() && session.read_byte() == -2, "unqualified synchronous pipe rejected before any read");
        check(keyboard::details::sigaction_noexcept(SIGINT, nullptr, ::std::addressof(during)) == 0, "actual disposition while rejected session exists");
    }
    auto const final{::fast_io::posix_status_nothrow(input)};
    auto const final_flags{::fast_io::posix_getfl_nothrow(input)};
    check(static_cast<bool>(final) && static_cast<bool>(final_flags) && before.value.dev == final.value.dev &&
        before.value.ino == final.value.ino && final.value.type == before.value.type && flags.flags == final_flags.flags,
        "original stdin owner/OFD flags unchanged");
    check(keyboard::details::sigaction_noexcept(SIGINT, nullptr, ::std::addressof(after)) == 0, "actual post-destruction disposition");
    for(auto const* action : {::std::addressof(during), ::std::addressof(after)})
    {
        check(action->sa_flags == prior.sa_flags && ((prior.sa_flags & SA_SIGINFO) != 0 ?
            action->sa_sigaction == prior.sa_sigaction : action->sa_handler == prior.sa_handler), "predecessor handler and flags unchanged");
# if defined(__FreeBSD__)
        // FreeBSD NSIG covers old signals only (32 on the actual 15.1 SDK).
        // Include SIGTHR/SIGLIBRT and the actual realtime range in the oracle.
        constexpr int maximum_signal{SIGRTMAX};
# elif defined(NSIG)
        constexpr int maximum_signal{NSIG - 1};
# else
#  error actual target must provide its complete bounded signal range
# endif
        for(int number{1}; number <= maximum_signal; ++number)
        {
            int const expected{keyboard_refusal_sigismember_noexcept(::std::addressof(prior.sa_mask), number)};
            int const observed{keyboard_refusal_sigismember_noexcept(::std::addressof(action->sa_mask), number)};
            // Identical invalid-number results also remain unchanged; every valid
            // signal in the actual SDK range must preserve its membership bit.
            check(observed == expected, "complete predecessor signal mask unchanged");
        }
    }
    ::fast_io::io::println(::fast_io::out(), "debug_console_nonlinux_pipe_refusal: PASS actual pipe refused, original OFD/handler/mask preserved; bounded pipe support remains unavailable");
#else
    ::fast_io::io::println(::fast_io::out(), "debug_console_nonlinux_pipe_refusal: UNSUPPORTED this fixture requires actual nonLinux POSIX target");
    return 77;
#endif
}

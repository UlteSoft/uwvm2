// Finite Linux/BSD/macOS component qualification; not a VM pause test.
#include <uwvm2/uwvm/debugger/console_keyboard.h>
#include <atomic>
#include <thread>
#include <pthread.h>
#include <string_view>
#include <fast_io.h>
#if defined(__unix__) || defined(__APPLE__)
namespace keyboard = uwvm2::uwvm::debugger::console_keyboard;
namespace editing = uwvm2::uwvm::debugger::console_editing;
#if defined(__linux__)
extern "C" int pthread_sigmask_noexcept(int, ::sigset_t const*, ::sigset_t*) noexcept __asm__("pthread_sigmask");
extern "C" int sigaddset_noexcept(::sigset_t*, int) noexcept __asm__("sigaddset");
#endif
inline ::std::atomic<unsigned> calls{}, observed{}, code{}, sender{}, uid{};
static void prior(int) noexcept { calls.fetch_add(1u, ::std::memory_order_relaxed); }
static void changed_prior(int) noexcept { calls.fetch_add(10u, ::std::memory_order_relaxed); }
static void probe(int, ::siginfo_t* info, void*) noexcept
{
    if(info == nullptr) { return; }
    code.store(static_cast<unsigned>(info->si_code), ::std::memory_order_relaxed);
    sender.store(static_cast<unsigned>(info->si_pid), ::std::memory_order_relaxed);
    uid.store(static_cast<unsigned>(info->si_uid), ::std::memory_order_relaxed);
    calls.fetch_add(1u, ::std::memory_order_relaxed); observed.store(1u, ::std::memory_order_release);
}
int main(int argc, char** argv)
{
    // Protocol stdout uses the native observer: READY must reach the manager
    // before this child blocks for its input. The default C stdout sink may
    // buffer these records when the manager redirects stdout to a pipe.
    ::std::string_view mode{argc > 1 ? argv[1] : "edit"};
    struct ::sigaction action{}; static_cast<void>(keyboard::details::sigemptyset_noexcept(::std::addressof(action.sa_mask)));
    if(mode == "origin" || mode == "self-info") { action.sa_flags = SA_SIGINFO; action.sa_sigaction = probe; }
    else if(mode == "self-default") { action.sa_handler = SIG_DFL; }
    else if(mode == "self-ignore") { action.sa_handler = SIG_IGN; }
    else if(mode == "self-reset") { action.sa_flags = SA_RESETHAND; action.sa_handler = prior; }
    else { action.sa_handler = prior; }
    if(keyboard::details::sigaction_noexcept(SIGINT, ::std::addressof(action), nullptr) != 0) { return 10; }
    if(mode == "origin")
    {
        ::fast_io::io::println(::fast_io::out(), "PROBE READY pid=", ::fast_io::mnp::dec(keyboard::details::getpid_noexcept()));
        for(unsigned tick{}; tick != 100u; ++tick)
        {
            if(observed.load(::std::memory_order_acquire) != 0u)
            {
                ::fast_io::io::println(::fast_io::out(), "PROBE code=", ::fast_io::mnp::dec(static_cast<int>(code.load())),
                    " pid=", ::fast_io::mnp::dec(sender.load()), " uid=", ::fast_io::mnp::dec(uid.load()),
#if defined(SI_KERNEL)
                    " SI_KERNEL=", ::fast_io::mnp::dec(static_cast<int>(SI_KERNEL)),
#endif
                    " SI_USER=", ::fast_io::mnp::dec(static_cast<int>(SI_USER))); return 0;
            }
            static_cast<void>(keyboard::details::poll_noexcept(nullptr, 0u, 50));
        }
        return 11;
    }
    bool ready{}, interactive{};
    {
        keyboard::native_session session{}; ready = session.ready(); interactive = session.interactive();
        if(!ready) { return 12; }
        ::fast_io::io::println(::fast_io::out(), "KEYBOARD READY interactive=", ::fast_io::mnp::dec(static_cast<unsigned>(interactive)),
            " display=", ::fast_io::mnp::dec(static_cast<unsigned>(session.interactive_display())), " pid=", ::fast_io::mnp::dec(keyboard::details::getpid_noexcept()));
        if(mode.starts_with("self-"))
        {
            static_cast<void>(keyboard::details::raise_noexcept(SIGINT));
            ::fast_io::io::println(::fast_io::out(), "SELF calls=", ::fast_io::mnp::dec(calls.load()), " pending=", ::fast_io::mnp::dec(static_cast<unsigned>(session.consume_interrupt())));
            if(mode == "self-reset") { static_cast<void>(keyboard::details::raise_noexcept(SIGINT)); return 14; }
            if((mode == "self-ignore" && calls.load() != 0u) || (mode == "self-custom" && calls.load() != 1u) ||
               (mode == "self-info" && (calls.load() != 1u || sender.load() != static_cast<unsigned>(keyboard::details::getpid_noexcept())))) { return 15; }
        }
#if defined(__linux__)
        else if(mode == "fifo-description")
        {
            auto const original{::fast_io::posix_getfl_nothrow(::fast_io::in())};
            auto reopened{keyboard::details::independent_fifo_input(::fast_io::in())};
            ::fast_io::io::perrln("FIFO DIAG setup original.error=", ::fast_io::mnp::dec(original.error),
                " original.flags=", ::fast_io::mnp::dec(original.flags),
                " reopened.error=", ::fast_io::mnp::dec(reopened.error));
            if(!original || !reopened) { return 21; }
            auto const observer{::fast_io::posix_io_observer{reopened.file.native_handle()}};
            auto const independent_flags{::fast_io::posix_getfl_nothrow(observer)};
            ::fast_io::io::perrln("FIFO DIAG independent.error=", ::fast_io::mnp::dec(independent_flags.error),
                " independent.flags=", ::fast_io::mnp::dec(independent_flags.flags));
            ::fast_io::io::println(::fast_io::out(), "FIFO READY");
            ::pollfd descriptor{observer.native_handle(), POLLIN, 0};
            auto const observed_poll{keyboard::details::poll_noexcept(::std::addressof(descriptor), 1u, 1000)};
            auto const poll_error{errno};
            ::fast_io::io::perrln("FIFO DIAG poll.return=", ::fast_io::mnp::dec(observed_poll),
                " poll.revents=", ::fast_io::mnp::dec(descriptor.revents), " poll.errno=", ::fast_io::mnp::dec(poll_error));
            if(observed_poll <= 0 || (descriptor.revents & POLLIN) == 0) { return 22; }
            char byte{};
            // The original description actually consumes the ready byte.
            // The independently reopened description must then return EAGAIN,
            // even while the original writer remains alive and open.
            auto const stolen{::fast_io::posix_read_nothrow(::fast_io::in(), ::std::addressof(byte), 1u)};
            auto const after{::fast_io::posix_read_nothrow(observer, ::std::addressof(byte), 1u)};
            auto const unchanged{::fast_io::posix_getfl_nothrow(::fast_io::in())};
            ::fast_io::io::perrln("FIFO DIAG stolen.transferred=", ::fast_io::mnp::dec(stolen.transferred),
                " stolen.error=", ::fast_io::mnp::dec(stolen.error), " after.transferred=", ::fast_io::mnp::dec(after.transferred),
                " after.error=", ::fast_io::mnp::dec(after.error), " unchanged.error=", ::fast_io::mnp::dec(unchanged.error),
                " unchanged.flags=", ::fast_io::mnp::dec(unchanged.flags), " expected.EAGAIN=", ::fast_io::mnp::dec(EAGAIN));
            if(!stolen || stolen.transferred != 1u || after || after.error != EAGAIN || !unchanged || unchanged.flags != original.flags)
            { return 23; }
            ::fast_io::io::println(::fast_io::out(), "FIFO EAGAIN original-flags-unchanged=1");
        }
        else if(mode == "pipe-worker")
        {
            ::sigset_t blocked{}, saved{};
            static_cast<void>(keyboard::details::sigemptyset_noexcept(::std::addressof(blocked)));
            // SIGINT is masked on the reader, so the actual owner kill must be
            // delivered to the worker. Only the atomic pending flag can wake
            // the reader's finite poll/read loop; no manager EINTR is relied on.
            if(sigaddset_noexcept(::std::addressof(blocked), SIGINT) != 0) { return 24; }
            if(pthread_sigmask_noexcept(SIG_BLOCK, ::std::addressof(blocked), ::std::addressof(saved)) != 0) { return 24; }
            ::std::atomic<unsigned> worker_ready{}, done{};
            ::std::jthread worker{[&]
            {
                if(pthread_sigmask_noexcept(SIG_UNBLOCK, ::std::addressof(blocked), nullptr) != 0)
                { worker_ready.store(2u, ::std::memory_order_release); return; }
                worker_ready.store(1u, ::std::memory_order_release);
                for(unsigned tick{}; tick != 100u && done.load(::std::memory_order_acquire) == 0u; ++tick)
                { static_cast<void>(keyboard::details::poll_noexcept(nullptr, 0u, 20)); }
            }};
            for(unsigned tick{}; tick != 1000u && worker_ready.load(::std::memory_order_acquire) == 0u; ++tick)
            { static_cast<void>(keyboard::details::poll_noexcept(nullptr, 0u, 1)); }
            if(worker_ready.load(::std::memory_order_relaxed) != 1u)
            {
                done.store(1u, ::std::memory_order_release); worker.join();
                static_cast<void>(pthread_sigmask_noexcept(SIG_SETMASK, ::std::addressof(saved), nullptr)); return 25;
            }
            ::fast_io::io::println(::fast_io::out(), "PIPE WORKER READY");
            auto const interrupted{session.read_byte() == keyboard::interrupted};
            done.store(1u, ::std::memory_order_release); worker.join();
            if(pthread_sigmask_noexcept(SIG_SETMASK, ::std::addressof(saved), nullptr) != 0 || !interrupted) { return 26; }
            ::fast_io::io::println(::fast_io::out(), "PIPE WORKER INTERRUPTED reader-sigint-blocked=1");
        }
#endif
        else if(mode == "change-handler")
        {
            action.sa_handler = changed_prior;
            if(keyboard::details::sigaction_noexcept(SIGINT, ::std::addressof(action), nullptr) != 0) { return 19; }
        }
        else
        {
            editing::editor editor{}; editor.begin();
            for(unsigned events{}; events != 4096u; ++events)
            {
                auto const result{editor.feed(session.read_byte())};
                if(result == editing::action::interrupted) { ::fast_io::io::println(::fast_io::out(), "KEYBOARD INTERRUPTED size=", ::fast_io::mnp::dec(editor.line().size)); continue; }
                if(result == editing::action::complete)
                {
                    ::fast_io::io::println(::fast_io::out(), "LINE ", editor.line().view());
                    if(editor.line().view() == "quit") { break; }
                    editor.remember(editor.line().view(), true); editor.begin();
                }
                else if(result == editing::action::end) { ::fast_io::io::println(::fast_io::out(), "KEYBOARD EOF"); break; }
                else if(result == editing::action::oversized) { ::fast_io::io::println(::fast_io::out(), "KEYBOARD REJECTED"); editor.begin(); }
                else if(result == editing::action::invalid) { return 16; }
            }
        }
    }
    struct ::sigaction restored{};
    if(keyboard::details::sigaction_noexcept(SIGINT, nullptr, ::std::addressof(restored)) != 0) { return 17; }
    auto const same{(restored.sa_flags & SA_SIGINFO) == (action.sa_flags & SA_SIGINFO) &&
        ((action.sa_flags & SA_SIGINFO) != 0 ? restored.sa_sigaction == action.sa_sigaction : restored.sa_handler == action.sa_handler)};
    keyboard::native_session second{};
    if(second.ready()) { return 20; } // predecessor publication is single-install process lifetime
    ::fast_io::io::println(::fast_io::out(), "KEYBOARD RESTORED same=", ::fast_io::mnp::dec(static_cast<unsigned>(same)), " calls=", ::fast_io::mnp::dec(calls.load()));
    return same ? 0 : 18;
}
#else
int main() { ::fast_io::io::println(::fast_io::out(), "debug_console_keyboard: POSIX origin component requires native POSIX provider"); return 77; }
#endif

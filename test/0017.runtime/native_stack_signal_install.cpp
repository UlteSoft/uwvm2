// Linux regression fixture; link with -Wl,--wrap=sigaction. This is deliberately
// not a normal .cc unit target, because the interposition flag is part of the test.
#include <uwvm2/runtime/lib/uwvm_runtime_native_stack_guard.h>
#include <cstdio>
#include <initializer_list>
#include <sys/resource.h>
#include <sys/wait.h>

namespace ns = uwvm2::runtime::lib::native_stack;
extern "C" int __real_sigaction(int, struct sigaction const*, struct sigaction*);
static int interrupt_install{};
static volatile sig_atomic_t forwarded{};
static bool inspect_mask{};
static void previous_handler(int signal)
{
    forwarded = signal;
    if(inspect_mask)
    {
        sigset_t mask{};
        if(::sigprocmask(SIG_SETMASK, nullptr, &mask) != 0 ||
           ::sigismember(&mask, SIGUSR1) != 1 || ::sigismember(&mask, signal) != 0)
        { forwarded = -1; }
    }
}

extern "C" int __wrap_sigaction(int signal, struct sigaction const* action, struct sigaction* previous)
{
    struct sigaction saved{};
    int const result{__real_sigaction(signal, action, &saved)};
    if(result == 0 && signal == interrupt_install && action != nullptr &&
       (action->sa_flags & SA_SIGINFO) && action->sa_sigaction == ns::signal_handler)
    {
        // Model a signal delivered after the kernel exposes the new handler but
        // before sigaction copies the old disposition to the caller's storage.
        // An ordinary stress loop rarely lands in this small publication window.
        interrupt_install = 0;
        ::raise(signal);
    }
    if(result == 0 && previous != nullptr) { *previous = saved; }
    return result;
}

int main()
{
    struct rlimit no_core{};
    if(::setrlimit(RLIMIT_CORE, &no_core) != 0) { return 1; }
    bool ok{true};
    for(int signal: {SIGSEGV, SIGBUS})
    {
      for(int scenario: {0, 1, 2})
      {
        auto const child{::fork()};
        if(child < 0) { return 1; }
        if(child == 0)
        {
            struct sigaction action{};
            action.sa_handler = previous_handler;
            ::sigemptyset(&action.sa_mask);
            if(scenario == 1)
            {
                action.sa_flags = SA_NODEFER | SA_RESTART;
                ::sigaddset(&action.sa_mask, SIGUSR1);
                inspect_mask = true;
            }
            if(scenario == 2) { action.sa_flags = SA_RESETHAND; }
            if(__real_sigaction(signal, &action, nullptr) != 0) { ::_exit(2); }
            if(scenario == 0) { interrupt_install = signal; }
            if(!ns::install_handlers()) { ::_exit(3); }
            if(scenario == 1)
            {
                struct sigaction installed{};
                if(__real_sigaction(signal, nullptr, &installed) != 0 || !(installed.sa_flags & SA_RESTART)) { ::_exit(5); }
                ::raise(signal);
            }
            if(scenario == 2)
            {
                ::raise(signal);
                if(forwarded != signal) { ::_exit(6); }
                // The old host disposition was one-shot. A second delivery
                // must take SIG_DFL, not invoke the saved callback a second time.
                ::raise(signal);
                ::_exit(7);
            }
            ::_exit(forwarded == signal ? 0 : 4);
        }
        int status{};
        if(::waitpid(child, &status, 0) != child) { return 1; }
        bool const passed{scenario == 2 ? WIFSIGNALED(status) && WTERMSIG(status) == signal
                                       : WIFEXITED(status) && WEXITSTATUS(status) == 0};
        std::printf("signal-install scenario=%d signal=%d status=%d %s\n", scenario, signal, status, passed ? "PASS" : "FAIL");
        ok &= passed;
      }
    }
    return ok ? 0 : 1;
}

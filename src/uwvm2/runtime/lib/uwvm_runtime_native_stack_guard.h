/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)
 * Copyright (c) 2025-present UlteSoft. All rights reserved.
 * Licensed under the APL-2.0 License (see LICENSE file).
 *************************************************************/
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <atomic>
#include <new>

// This header is shared by the header and named-module runtime entry points.
// Native signal state must be per OS thread even when the VM's optional TLS
// fast path is disabled: a signal handler must never lock the VM thread map.
#if (defined(__linux__) || defined(__APPLE__)) && !defined(_WIN32)
# include <pthread.h>
# include <signal.h>
# include <sys/mman.h>
# include <unistd.h>
# if defined(__APPLE__)
#  include <sys/ucontext.h>
# else
#  include <ucontext.h>
#  include <sys/resource.h>
# endif
#endif

namespace uwvm2::runtime::lib::native_stack
{
    // Require the exact native guard address and a nonzero SP below the known
    // stack top. Do not require SP to be close to the guard: ARM probes ahead
    // before adjusting SP, while RISC-V may probe at a positive SP offset.
    [[nodiscard]] inline constexpr bool is_guard_fault(::std::uintptr_t low,
                                                       ::std::uintptr_t high,
                                                       ::std::size_t page,
                                                       ::std::uintptr_t address,
                                                       ::std::uintptr_t sp) noexcept
    {
        return page != 0u && low >= page && high > low &&
               address >= low - page && address < low && sp != 0u && sp < high;
    }

#if (defined(__linux__) || defined(__APPLE__)) && !defined(_WIN32)
    struct bounds
    {
        ::std::uintptr_t low{};
        ::std::uintptr_t high{};
        ::std::size_t page{};
    };

# if defined(__linux__)
    struct bounds_cache
    {
        bounds value{};
        ::rlim_t stack_limit{};
        bool fixed_extent{};
        bool valid{};
    };
    inline thread_local bounds_cache cached_bounds{};

    [[nodiscard]] inline bool read_bounds(::std::uintptr_t here, ::std::size_t page, bounds& result) noexcept
    {
        // pthread_getattr_np may parse /proc/self/maps for the main thread.
        // Native pthread stack extents are stable; refresh on RLIMIT_STACK
        // changes, and never accept a fiber outside the cached native range.
        auto& cache{cached_bounds};
        bool const in_cached_stack{cache.valid && cache.value.page == page &&
                                   here >= cache.value.low && here < cache.value.high};
        // An explicitly guarded pthread stack is a fixed allocation, not the
        // main thread's RLIMIT-governed grow-down mapping. Its hot entry needs
        // neither a syscall nor a libc stack-attribute query. Zero-guard/custom
        // stacks take the conservative limit-sensitive path below.
        if(in_cached_stack && cache.fixed_extent)
        {
            result = cache.value;
            return true;
        }
        struct ::rlimit limit{};
        bool const have_limit{::getrlimit(RLIMIT_STACK, &limit) == 0};
        if(in_cached_stack && have_limit && cache.stack_limit == limit.rlim_cur)
        {
            result = cache.value;
            return true;
        }
        cache.valid = false;
        ::pthread_attr_t attributes{};
        if(::pthread_getattr_np(::pthread_self(), &attributes) != 0) { return false; }
        void* base{};
        ::std::size_t size{};
        auto const status{::pthread_attr_getstack(&attributes, &base, &size)};
        ::std::size_t guard_bytes{};
        bool const fixed_extent{::pthread_attr_getguardsize(&attributes, &guard_bytes) == 0 && guard_bytes >= page};
        ::pthread_attr_destroy(&attributes);
        auto const low{reinterpret_cast<::std::uintptr_t>(base)};
        if(status != 0 || size > (::std::numeric_limits<::std::uintptr_t>::max)() - low) { return false; }
        result = {low, low + size, page};
        if(here < result.low || here >= result.high) { return false; }
        if(have_limit || fixed_extent)
        {
            cache.value = result;
            cache.stack_limit = limit.rlim_cur;
            cache.fixed_extent = fixed_extent;
            cache.valid = true;
        }
        return true;
    }
# endif

    inline constexpr ::std::size_t alternate_stack_bytes{128u * 1024u};
    struct alternate_cache_entry
    {
        ::std::size_t mapping_bytes;
        void* payload;
    };
    // pthread TSD owns the mapping, and clears the TLS fast pointer before
    // releasing it. Once TSD
    // teardown begins, late host-destructor re-entry uses the uncached path:
    // pthreads only repeats destructors a bounded number of times, so caching
    // a fresh mapping during the final pass would leak it on thread exit.
    inline thread_local alternate_cache_entry* reusable_alternate{};
    inline thread_local bool alternate_cache_retired{};
    inline ::pthread_key_t alternate_cache_key{};
    inline alternate_cache_entry retired_alternate{};

    inline void release_cached_alternate(void* pointer) noexcept
    {
        auto* record{static_cast<alternate_cache_entry*>(pointer)};
        if(record == nullptr) { return; }
        alternate_cache_retired = true;
        if(reusable_alternate == record) { reusable_alternate = nullptr; }
        // A trivial C++ TLS flag alone is insufficient on Darwin: libpthread
        // can destroy and recreate the Mach-O TLV storage between successive
        // host TSD destructor passes. Keep a resource-free retirement marker
        // in the pthread key as well, and re-arm it on each destructor pass.
        // The final marker needs no destructor/free when pthreads stops.
        // A valid, already populated key normally reuses its existing slot;
        // if the platform cannot retain it, terminate rather than resume with
        // a silently lost ownership/retirement record.
        if(::pthread_setspecific(alternate_cache_key, &retired_alternate) != 0) { ::_exit(126); }
        if(record == &retired_alternate) { return; }
        auto const bytes{record->mapping_bytes};
        ::stack_t current{};
        // Never free a stack still registered with the kernel. If teardown
        // cannot query/disable it, retain the mapping rather than risk UAF.
        if(::sigaltstack(nullptr, &current) != 0) { return; }
        if(!(current.ss_flags & SS_DISABLE) && current.ss_sp == record->payload)
        {
            if(current.ss_flags & SS_ONSTACK) { return; }
            current.ss_flags = SS_DISABLE;
            if(::sigaltstack(&current, nullptr) != 0) { return; }
        }
        ::munmap(record, bytes);
    }

    [[nodiscard]] inline alternate_cache_entry* acquire_cached_alternate(::std::size_t page) noexcept
    {
        if(reusable_alternate != nullptr) { return reusable_alternate; }
        // Keep this check after the live-pointer fast path. Normal warm VM
        // entries gain no extra TLS load/branch from destructor-order safety.
        if(alternate_cache_retired) { return nullptr; }
        static bool const have_key{::pthread_key_create(&alternate_cache_key, release_cached_alternate) == 0};
        if(!have_key || page < sizeof(alternate_cache_entry) ||
           page > ((::std::numeric_limits<::std::size_t>::max)() - alternate_stack_bytes) / 3u) { return nullptr; }
        // Only cold entries reach this lookup. A warm entry returned through
        // the TLS pointer above; destructor safety adds no pthread call there.
        if(::pthread_getspecific(alternate_cache_key) == &retired_alternate)
        {
            alternate_cache_retired = true;
            return nullptr;
        }
        // RW metadata | inaccessible guard | RW signal stack | guard.
        // Keeping ownership metadata outside the signal stack avoids a heap
        // allocation, and leaves the entire advertised stack span usable.
        auto const bytes{alternate_stack_bytes + page * 3u};
        void* allocation{::mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)};
        if(allocation == MAP_FAILED) { return nullptr; }
        auto* payload{static_cast<::std::byte*>(allocation) + page * 2u};
        if(::mprotect(allocation, page, PROT_READ | PROT_WRITE) != 0 ||
           ::mprotect(payload, alternate_stack_bytes, PROT_READ | PROT_WRITE) != 0)
        {
            ::munmap(allocation, bytes);
            return nullptr;
        }
        auto* record{::new(allocation) alternate_cache_entry{bytes, payload}};
        if(::pthread_setspecific(alternate_cache_key, record) != 0)
        {
            ::munmap(allocation, bytes);
            return nullptr;
        }
        reusable_alternate = record;
        return record;
    }

    inline thread_local ::std::atomic<bounds const*> active_bounds{};
    inline struct ::sigaction previous_segv{};
    inline struct ::sigaction previous_bus{};
    inline ::std::atomic<bool> previous_actions_published{};
    inline ::std::atomic<bool> previous_segv_reset{}, previous_bus_reset{};

    [[nodiscard]] inline ::std::uintptr_t fault_stack_pointer(void* context) noexcept
    {
        if(context == nullptr) { return 0u; }
        [[maybe_unused]] auto const* uc{static_cast<::ucontext_t const*>(context)};
# if defined(__linux__) && defined(__x86_64__)
        return static_cast<::std::uintptr_t>(uc->uc_mcontext.gregs[REG_RSP]);
# elif defined(__linux__) && defined(__i386__)
        return static_cast<::std::uintptr_t>(uc->uc_mcontext.gregs[REG_ESP]);
# elif defined(__linux__) && defined(__aarch64__)
        return static_cast<::std::uintptr_t>(uc->uc_mcontext.sp);
# elif defined(__linux__) && defined(__arm__)
        return static_cast<::std::uintptr_t>(uc->uc_mcontext.arm_sp);
# elif defined(__linux__) && defined(__riscv)
        return static_cast<::std::uintptr_t>(uc->uc_mcontext.__gregs[2u]);
# elif defined(__linux__) && defined(__loongarch__)
        return static_cast<::std::uintptr_t>(uc->uc_mcontext.__gregs[3u]);
# elif defined(__linux__) && defined(__mips__)
        return static_cast<::std::uintptr_t>(uc->uc_mcontext.gregs[29u]);
# elif defined(__linux__) && defined(__powerpc64__)
        return static_cast<::std::uintptr_t>(uc->uc_mcontext.gp_regs[1u]);
# elif defined(__linux__) && defined(__powerpc__)
        auto const* regs{uc->uc_mcontext.uc_regs};
        return regs == nullptr ? 0u : static_cast<::std::uintptr_t>(regs->gregs[1u]);
# elif defined(__linux__) && (defined(__s390__) || defined(__s390x__))
        return static_cast<::std::uintptr_t>(uc->uc_mcontext.gregs[15u]);
# elif defined(__APPLE__) && defined(__aarch64__)
        return static_cast<::std::uintptr_t>(uc->uc_mcontext->__ss.__sp);
# elif defined(__APPLE__) && defined(__x86_64__)
        return static_cast<::std::uintptr_t>(uc->uc_mcontext->__ss.__rsp);
# else
        return 0u; // No guessed register layout on other libc/ISA combinations.
# endif
    }

    inline void forward_signal(int signal, ::siginfo_t* info, void* context, struct ::sigaction const& previous,
                               ::std::atomic<bool>& reset) noexcept
    {
        // SA_RESETHAND belongs to the saved host callback, not to our permanent
        // stack guard. Let exactly one asynchronous delivery consume that
        // callback; copying SA_RESETHAND onto the wrapper would remove native
        // stack protection after the first unrelated host signal.
        if(previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN &&
           (previous.sa_flags & SA_RESETHAND) && reset.exchange(true, ::std::memory_order_relaxed))
        {
            struct ::sigaction action{};
            action.sa_handler = SIG_DFL;
            // Darwin exposes sigemptyset as a function-like macro; do not add
            // namespace qualification to this POSIX call.
            static_cast<void>(sigemptyset(&action.sa_mask));
            ::sigaction(signal, &action, nullptr);
            ::raise(signal);
            return;
        }
        if(previous.sa_handler == SIG_DFL)
        {
            ::sigaction(signal, &previous, nullptr);
            ::raise(signal);
        }
        else if(previous.sa_handler != SIG_IGN)
        {
            if(previous.sa_flags & SA_SIGINFO) { previous.sa_sigaction(signal, info, context); }
            else { previous.sa_handler(signal); }
        }
    }

    inline void signal_handler(int signal, ::siginfo_t* info, void* context) noexcept
    {
        auto const* b{active_bounds.load(::std::memory_order_acquire)};
        if(b != nullptr && info != nullptr && info->si_code > 0 &&
           is_guard_fault(b->low, b->high, b->page, reinterpret_cast<::std::uintptr_t>(info->si_addr), fault_stack_pointer(context)))
        {
            // The ordinary stack is unusable. Avoid allocators, C++ streams,
            // unwinding and general runtime diagnostics in this signal path.
            constexpr char message[]{"uwvm: [fatal] Runtime crash: call stack exhausted (native stack guard).\n"};
            [[maybe_unused]] auto const written{::write(STDERR_FILENO, message, sizeof(message) - 1u)};
            ::_exit(127);
        }
        // Installing a process-wide disposition exposes it to threads which
        // have not entered scope(), so the local-static initialization guard
        // alone cannot publish the saved actions to this asynchronous reader.
        // Never wait here: this signal may have interrupted the installer.
        if(!previous_actions_published.load(::std::memory_order_acquire)) { ::_exit(126); }
        forward_signal(signal, info, context, signal == SIGSEGV ? previous_segv : previous_bus,
                       signal == SIGSEGV ? previous_segv_reset : previous_bus_reset);
    }

    [[nodiscard]] inline bool install_handlers() noexcept
    {
        if constexpr(!decltype(previous_actions_published)::is_always_lock_free) { return false; }
        // The local static serializes installers and publishes completion to
        // VM entrants; the separate atomic publishes saved host actions to
        // signal handlers running on any thread during installation itself.
        static bool const installed{[]() noexcept
        {
            // Do not install the new handler and fetch the old one in the same
            // sigaction call. The kernel may expose the new disposition before
            // copying the old one back to user memory. A concurrent signal in
            // that window would read a zero/partial action and could terminate
            // the process instead of chaining to its host handler.
            // Host code must serialize its own process-wide handler changes
            // with VM initialization; no query/install pair can arbitrate an
            // unrelated concurrent sigaction replacement.
            if(::sigaction(SIGSEGV, nullptr, &previous_segv) != 0 ||
               ::sigaction(SIGBUS, nullptr, &previous_bus) != 0) { return false; }
            previous_actions_published.store(true, ::std::memory_order_release);
            struct ::sigaction action{};
            action.sa_sigaction = signal_handler;
            // Forwarding by calling the saved function does not ask the kernel
            // to reapply its mask or syscall-restart policy. Preserve those
            // properties on each wrapper disposition itself. The fatal guard
            // path exits immediately, so it does not need different masks.
            action.sa_flags = SA_SIGINFO | SA_ONSTACK | (previous_segv.sa_flags & (SA_NODEFER | SA_RESTART));
            action.sa_mask = previous_segv.sa_mask;
            if(::sigaction(SIGSEGV, &action, nullptr) != 0) { return false; }
            action.sa_flags = SA_SIGINFO | SA_ONSTACK | (previous_bus.sa_flags & (SA_NODEFER | SA_RESTART));
            action.sa_mask = previous_bus.sa_mask;
            if(::sigaction(SIGBUS, &action, nullptr) != 0)
            {
                ::sigaction(SIGSEGV, &previous_segv, nullptr);
                return false;
            }
            return true;
        }()};
        return installed;
    }

    class scope
    {
        bounds current{};
        bounds const* previous_bounds{};
        ::stack_t previous_alt{};
        void* mapping{MAP_FAILED};
        ::std::size_t mapping_bytes{};
        bool owns_alt{};
        bool entered{};
        bool initialized{};

    public:
        scope() noexcept
        {
            // A signal path must not fall back to libatomic's internal locks.
            if constexpr(!decltype(active_bounds)::is_always_lock_free) { return; }
            auto const here{reinterpret_cast<::std::uintptr_t>(this)};
            auto const* active{active_bounds.load(::std::memory_order_acquire)};
            if(active != nullptr && here >= active->low && here < active->high)
            {
                initialized = true;
                return; // Nested public raw re-entry uses the outer signal stack.
            }

            // The base page size does not change during the process lifetime.
            static auto const page{::sysconf(_SC_PAGESIZE)};
            if(page <= 0) { return; }
            current.page = static_cast<::std::size_t>(page);
# if defined(__APPLE__)
            current.high = reinterpret_cast<::std::uintptr_t>(::pthread_get_stackaddr_np(::pthread_self()));
            auto const size{::pthread_get_stacksize_np(::pthread_self())};
            if(current.high < size) { return; }
            current.low = current.high - size;
# else
            if(!read_bounds(here, current.page, current)) { return; }
# endif
            // A user-switched fiber is not necessarily the pthread's stack.
            // Never register bounds which do not contain this actual entry.
            if(here < current.low || here >= current.high) { return; }
            if(::sigaltstack(nullptr, &previous_alt) != 0 || (previous_alt.ss_flags & SS_ONSTACK)) { return; }
            constexpr ::std::size_t alt_bytes{alternate_stack_bytes};
            if((previous_alt.ss_flags & SS_DISABLE) || previous_alt.ss_size < alt_bytes)
            {
                ::stack_t alternate{};
                if(auto* cached{acquire_cached_alternate(current.page)}; cached != nullptr)
                {
                    alternate.ss_sp = cached->payload;
                }
                else
                {
                    // Exhausted pthread keys or a cache allocation failure
                    // must not disable the existing uncached safe path.
                    if(current.page > ((::std::numeric_limits<::std::size_t>::max)() - alt_bytes) / 2u) { return; }
                    mapping_bytes = alt_bytes + current.page * 2u;
                    mapping = ::mmap(nullptr, mapping_bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                    if(mapping == MAP_FAILED) { return; }
                    auto* payload{static_cast<::std::byte*>(mapping) + current.page};
                    if(::mprotect(payload, alt_bytes, PROT_READ | PROT_WRITE) != 0) { return; }
                    alternate.ss_sp = payload;
                }
                alternate.ss_size = alt_bytes;
                if(::sigaltstack(&alternate, nullptr) != 0) { return; }
                owns_alt = true;
            }
            if(!install_handlers()) { return; }
            previous_bounds = active_bounds.load(::std::memory_order_relaxed);
            active_bounds.store(&current, ::std::memory_order_release);
            entered = true;
            initialized = true;
        }

        scope(scope const&) = delete;
        scope& operator=(scope const&) = delete;

        ~scope()
        {
            if(entered) { active_bounds.store(previous_bounds, ::std::memory_order_release); }
#if defined(__APPLE__)
            // Darwin validates ss_size even when SS_DISABLE is set. A queried
            // initially disabled stack has size zero and cannot be passed back
            // verbatim; its inactive address/size have no operational meaning.
            if((previous_alt.ss_flags & SS_DISABLE) && previous_alt.ss_size < MINSIGSTKSZ)
            { previous_alt.ss_size = MINSIGSTKSZ; }
#endif
            if(owns_alt && ::sigaltstack(&previous_alt, nullptr) != 0)
            {
                // Never unmap memory the kernel still considers a signal stack.
                ::_exit(126);
            }
            if(mapping != MAP_FAILED) { ::munmap(mapping, mapping_bytes); }
        }

        [[nodiscard]] bool ready() const noexcept { return initialized; }
    };

    [[noreturn]] inline void setup_failed() noexcept
    {
        constexpr char message[]{"uwvm: [fatal] cannot establish native stack fault handling.\n"};
        [[maybe_unused]] auto const written{::write(STDERR_FILENO, message, sizeof(message) - 1u)};
        ::_exit(126);
    }
#else
    class scope
    {
    public:
        [[nodiscard]] constexpr bool ready() const noexcept { return true; }
    };
#endif
}

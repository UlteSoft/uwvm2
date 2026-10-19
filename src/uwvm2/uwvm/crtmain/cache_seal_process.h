// Explicit process bootstrap only. Never called by cache-path selection or Guest code.
#pragma once
#ifndef UWVM_MODULE
# include <cerrno>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# if defined(__linux__)
#  include <sys/prctl.h>
# endif
# include <fast_io.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::crtmain
{
#if defined(UWVM_RUNTIME_LLVM_JIT)
    // False means the invocation must stop before Source initialization, JIT,
    // cache I/O or Guest execution. The failure latch is one-way for this process.
    [[nodiscard]] UWVM_GNU_COLD inline bool apply_explicit_llvm_cache_process_seal() noexcept
    {
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(mode::global_runtime_llvm_jit_cache_seal_process_failed)
        {
            mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                u8"[llvm-jit-cache] process-seal-failed reason=prior-seal-failure persistent-cache=disabled\n");
            return false;
        }
        if(!mode::global_runtime_llvm_jit_cache_seal_process_requested) { return true; }
        mode::global_runtime_llvm_jit_cache_seal_process_requested = false;
        auto const reject{[](char8_t const* phase, int error, int observed) noexcept
        {
            mode::global_runtime_llvm_jit_cache_seal_process_failed = true;
            mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                u8"[llvm-jit-cache] process-seal-failed phase=", ::fast_io::mnp::os_c_str(phase),
                u8" errno=", error, u8" observed=", observed,
                u8" persistent-cache=disabled guest-started=0\n");
            return false;
        }};
# if defined(__linux__) && defined(PR_SET_DUMPABLE) && defined(PR_GET_DUMPABLE)
        errno = 0;
        if(::prctl(PR_SET_DUMPABLE, 0ul, 0ul, 0ul, 0ul) != 0)
        {
            int const error{errno};
            return reject(u8"set", error, -1);
        }
        errno = 0;
        int const observed{::prctl(PR_GET_DUMPABLE, 0ul, 0ul, 0ul, 0ul)};
        int const error{errno};
        if(observed != 0) { return reject(u8"verify", observed < 0 ? error : 0, observed); }
        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
            u8"[llvm-jit-cache] process-sealed dumpable=0 explicit-cli-request=1 persistence=subject-to-origin-admission\n");
        return true;
# else
        // Recognized parameter, explicit unsupported result. No fake success or
        // pathname/binary-identity fallback on Windows and other platforms.
        return reject(u8"unsupported-platform", 0, -1);
# endif
    }
#endif
}

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

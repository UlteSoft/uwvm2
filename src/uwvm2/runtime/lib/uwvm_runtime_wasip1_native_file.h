// Native storage for debugger-owned WASIp1 files. No guest FD or checkpoint
// authority lives here; callers must hold the original cooperative host gate.
#pragma once
#ifndef UWVM_MODULE
#include <fast_io.h>
#include <fast_io_device.h>
#include <fast_io_dsal/string.h>
#endif

#include "uwvm_runtime_wasip1_mount_identity.h"
#include "uwvm_runtime_wasip1_resource_identity.h"

namespace uwvm2::runtime::lib::wasip1_native_file
{
    inline constexpr bool supported{
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || (defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__))
        true
#else
        false
#endif
    };
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
    // Capture WASI-visible status flags, not immutable kernel bookkeeping.
    // Darwin adds a "was written" bit after pwrite; F_SETFL cannot restore it.
    inline constexpr int flag_mask{O_APPEND|O_NONBLOCK
#ifdef O_SYNC
        |O_SYNC
#endif
#ifdef O_DSYNC
        |O_DSYNC
#endif
#ifdef O_RSYNC
        |O_RSYNC
#endif
    };
#endif
    [[nodiscard]] inline int flags(::fast_io::native_file const& file)
    {
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
        auto const result{::fast_io::posix_getfl_nothrow(::fast_io::native_io_observer{file})};
        if(!result) { ::fast_io::throw_posix_error(result.error); }
        return result.flags&flag_mask;
#else
        (void)file;
        // Windows managed files have the fixed ordinary read/write mode issued
        // by the factory. WASI cannot set append/sync/nonblock on this handle.
        if constexpr(!supported) { ::fast_io::throw_posix_error(ENOTSUP); }
        return 0;
#endif
    }
    inline void set_flags(::fast_io::native_file const& file, int value)
    {
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
        if((value&~flag_mask)!=0) { ::fast_io::throw_posix_error(ENOTSUP); }
        auto const current{::fast_io::posix_getfl_nothrow(::fast_io::native_io_observer{file})};
        if(!current) { ::fast_io::throw_posix_error(current.error); }
        auto const result{::fast_io::posix_setfl_nothrow(::fast_io::native_io_observer{file},(current.flags&~flag_mask)|value)};
        if(!result) { ::fast_io::throw_posix_error(result.error); }
        if(flags(file)!=value) { ::fast_io::throw_posix_error(ENOTSUP); }
#else
        (void)file;
        if(!supported || value!=0) { ::fast_io::throw_posix_error(ENOTSUP); }
#endif
    }
    inline void read_content(::fast_io::native_file const& file, ::std::byte* first, ::std::byte* last)
    {
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__)
        // NT positioned reads on a synchronous handle update its shared file
        // pointer. A checkpoint/query must leave the live cursor unchanged.
        auto const offset{::fast_io::operations::io_stream_seek_bytes(file,0,::fast_io::seekdir::cur)};
        try { ::fast_io::operations::pread_all_bytes(file,first,last,0); }
        catch(...)
        {
            ::fast_io::operations::io_stream_seek_bytes(file,offset,::fast_io::seekdir::beg);
            throw;
        }
        ::fast_io::operations::io_stream_seek_bytes(file,offset,::fast_io::seekdir::beg);
#else
        ::fast_io::operations::pread_all_bytes(file,first,last,0);
#endif
    }
    [[nodiscard]] inline ::fast_io::native_file create()
    {
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__)
        // FastIO native_file is NT on modern Windows; its io_temp constructor
        // is provided by win32_file. Transfer that genuinely owned handle.
        ::fast_io::win32_file temporary{::fast_io::io_temp};
        return ::fast_io::native_file{temporary.release()};
#elif defined(__APPLE__) || defined(__FreeBSD__)
        // FastIO POSIX io_temp currently supports only Linux O_TMPFILE. Use a
        // pinned directory, OS entropy, exclusive 0600 creation and immediate
        // unlink BEFORE any payload is written or the guest FD is published.
        ::fast_io::dir_file directory{u8"/tmp", ::fast_io::open_mode::follow};
        ::fast_io::native_white_hole entropy{};
        ::std::uint_least64_t random[2u]{};
        auto* const first{reinterpret_cast<::std::byte*>(random)};
        ::fast_io::operations::read_all_bytes(entropy,first,first+sizeof(random));
        auto const name{::fast_io::u8concat_fast_io(u8"uwvm-wasi-", ::fast_io::mnp::hex(random[0u]),
                                          u8"-", ::fast_io::mnp::hex(random[1u]))};
        ::fast_io::native_file temporary{::fast_io::at(directory), name,
            ::fast_io::open_mode::in|::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl,
            static_cast<::fast_io::perms>(0600)};
        ::fast_io::native_unlinkat(::fast_io::at(directory), name);
        return temporary;
#elif defined(__linux__)
        ::fast_io::native_file temporary{::fast_io::io_temp};
        // Linux FastIO io_temp includes O_APPEND, which changes pwrite.
        set_flags(temporary, flags(temporary)&~O_APPEND);
        return temporary;
#else
        ::fast_io::throw_posix_error(ENOTSUP);
#endif
    }
}

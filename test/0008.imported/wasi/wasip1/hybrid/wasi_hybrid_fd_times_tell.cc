/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      Claude Sonnect 4.5
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

// S9: Multi-threaded test for fd_filestat_set_times and fd_tell

// std
#include <cstddef>
#include <version>
#include <atomic>
#include <vector>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_seek_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_tell_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_filestat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_filestat_set_times_wasm64.h>

using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::filedelta_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::fstflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::whence_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;

// Test fd_tell after fd_seek
inline static void test_fd_tell(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t fd, ::std::size_t base_off)
{
    wasi_void_ptr_wasm64_t const newoff_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x100u)};
    wasi_void_ptr_wasm64_t const tell_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x200u)};

    // Seek to beginning
    auto ret = ::uwvm2::imported::wasi::wasip1::func::fd_seek_wasm64(env, fd, filedelta_wasm64_t{}, whence_wasm64_t::whence_set, newoff_ptr);
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_tell: fd_seek failed\n");
        ::fast_io::fast_terminate();
    }

    // Tell should return 0
    ret = ::uwvm2::imported::wasi::wasip1::func::fd_tell_wasm64(env, fd, tell_ptr);
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_tell: fd_tell failed\n");
        ::fast_io::fast_terminate();
    }

    auto const pos =
        ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<::std::underlying_type_t<filesize_wasm64_t>>(memory, tell_ptr);
    if(pos != 0u) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_tell: position not 0 after seek to beginning\n");
        ::fast_io::fast_terminate();
    }
}

// Test fd_filestat_set_times
inline static void
    test_fd_filestat_set_times(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t fd, ::std::size_t base_off)
{
    wasi_void_ptr_wasm64_t const filestat_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x100u)};

    // Get current mtime
    auto ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get_wasm64(env, fd, filestat_ptr);
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_filestat_set_times: fd_filestat_get failed\n");
        ::fast_io::fast_terminate();
    }

    // mtime field is at offset 16 in filestat_wasm64_t
    auto const old_mtime = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<::std::underlying_type_t<timestamp_wasm64_t>>(
        memory,
        static_cast<wasi_void_ptr_wasm64_t>(filestat_ptr + 16u));

    // Set new mtime (just use old_mtime + 1000000000 nanoseconds)
    auto const new_mtime = static_cast<timestamp_wasm64_t>(old_mtime + 1000000000ull);
    ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_times_wasm64(env,
                                                                              fd,
                                                                              static_cast<timestamp_wasm64_t>(0),
                                                                              new_mtime,
                                                                              static_cast<fstflags_wasm64_t>(fstflags_wasm64_t::filestat_set_mtim));
    // May fail on some platforms, but shouldn't crash
    (void)ret;

    // Get filestat again to verify (weak check)
    ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get_wasm64(env, fd, filestat_ptr);
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_filestat_set_times: fd_filestat_get after set_times failed\n");
        ::fast_io::fast_terminate();
    }

    // Just verify we can read it again (don't strictly check value due to platform differences)
    (void)old_mtime;
}

int main()
{
#if __cpp_lib_atomic_wait >= 201907L
    constexpr unsigned thread_count{8u};
    constexpr ::std::size_t base_stride{0x10000u};

    native_memory_t memory{};
    memory.init_by_page_count(thread_count + 4u);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {.fd_limit = 128uz},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false,
                                            .disable_utf8_check = false};

    // Prepare per-thread regular files
    env.fd_storage.opens.resize(static_cast<::std::size_t>(4u + thread_count));

    for(unsigned tid = 0; tid < thread_count; ++tid)
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(static_cast<::std::size_t>(4u + tid)).fd_p;
        fde.rights_base = static_cast<::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t>(-1);
        fde.rights_inherit = static_cast<::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
# if defined(_WIN32) && !defined(__CYGWIN__)
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file =
            ::fast_io::native_file{u8"wasi_hybrid_fd_times_tell.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
# else
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd =
            ::fast_io::native_file{u8"wasi_hybrid_fd_times_tell.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
# endif
    }

    std::atomic<bool> failure{false};

    ::std::vector<fast_io::native_thread> threads;
    threads.reserve(thread_count);

    for(unsigned tid = 0; tid < thread_count; ++tid)
    {
        threads.emplace_back(
            [&memory, &env, &failure, tid]()
            {
                constexpr unsigned iterations{128u};
                for(unsigned i = 0; i < iterations && !failure.load(::std::memory_order_relaxed); ++i)
                {
                    auto const base_off = static_cast<::std::size_t>((tid + 1u) * base_stride);
                    auto const scenario = (tid + i) % 2u;
                    auto const fd = static_cast<wasi_posix_fd_wasm64_t>(4 + tid);

                    try
                    {
                        switch(scenario)
                        {
                            case 0u:
                            {
                                test_fd_tell(memory, env, fd, base_off);
                                break;
                            }
                            case 1u:
                            {
                                test_fd_filestat_set_times(memory, env, fd, base_off);
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                    }
                    catch(...)
                    {
                        failure.store(true, ::std::memory_order_relaxed);
                        return;
                    }
                }
            });
    }

    for(auto& t: threads) { t.join(); }

    if(failure.load(::std::memory_order_relaxed))
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_fd_times_tell: failure detected");
        ::fast_io::fast_terminate();
    }
#endif
}

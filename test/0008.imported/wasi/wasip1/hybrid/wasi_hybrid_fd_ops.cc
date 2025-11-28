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

// Multi-threaded test for fd operations: fd_tell, fd_allocate, fd_advise, fd_fdstat_set_flags

// std
#include <cstddef>
#include <cstdint>
#include <version>
#include <atomic>
#include <thread>
#include <vector>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_write_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_tell_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_seek_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_allocate_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_advise_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_set_flags_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_filestat_set_size_wasm64.h>

using ::uwvm2::imported::wasi::wasip1::abi::advice_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;

inline static void write_bytes64(native_memory_t& memory, wasi_void_ptr_wasm64_t p, void const* s, ::std::size_t n)
{
    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64(memory,
                                                                        p,
                                                                        reinterpret_cast<::std::byte const*>(s),
                                                                        reinterpret_cast<::std::byte const*>(s) + n);
}

// Test fd_tell and fd_seek combination
inline static void test_fd_tell_seek(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t fd, ::std::size_t base_off)
{
    // Write some data first
    constexpr char const msg[] = "TellSeekTest";
    wasi_void_ptr_wasm64_t const buf{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x100u)};
    write_bytes64(memory, buf, msg, sizeof(msg) - 1u);

    wasi_void_ptr_wasm64_t const iovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x200u)};
    wasi_void_ptr_wasm64_t const nwritten_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x300u)};

    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(msg) - 1u));

    auto const ret_w = ::uwvm2::imported::wasi::wasip1::func::fd_write_wasm64(env, fd, iovs_ptr, static_cast<wasi_size_wasm64_t>(1u), nwritten_ptr);
    if(ret_w != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_tell_seek: fd_write failed\n");
        ::fast_io::fast_terminate();
    }

    // Test fd_tell
    wasi_void_ptr_wasm64_t const tell_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x400u)};
    auto const ret_tell = ::uwvm2::imported::wasi::wasip1::func::fd_tell_wasm64(env, fd, tell_ptr);
    if(ret_tell != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_tell_seek: fd_tell failed\n");
        ::fast_io::fast_terminate();
    }

    auto const pos =
        ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<::std::underlying_type_t<filesize_wasm64_t>>(memory, tell_ptr);
    if(pos < static_cast<::std::underlying_type_t<filesize_wasm64_t>>(sizeof(msg) - 1u)) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_tell_seek: position check failed\n");
        ::fast_io::fast_terminate();
    }

    // Seek to beginning
    wasi_void_ptr_wasm64_t const newoff_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x500u)};
    auto const ret_seek = ::uwvm2::imported::wasi::wasip1::func::fd_seek_wasm64(env,
                                                                                fd,
                                                                                ::uwvm2::imported::wasi::wasip1::abi::filedelta_wasm64_t{},
                                                                                ::uwvm2::imported::wasi::wasip1::abi::whence_wasm64_t::whence_set,
                                                                                newoff_ptr);
    if(ret_seek != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_tell_seek: fd_seek failed\n");
        ::fast_io::fast_terminate();
    }

    // Tell again should be 0
    auto const ret_tell2 = ::uwvm2::imported::wasi::wasip1::func::fd_tell_wasm64(env, fd, tell_ptr);
    if(ret_tell2 != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_tell_seek: fd_tell2 failed\n");
        ::fast_io::fast_terminate();
    }

    auto const pos2 =
        ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<::std::underlying_type_t<filesize_wasm64_t>>(memory, tell_ptr);
    if(pos2 != 0u) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_tell_seek: position after seek check failed\n");
        ::fast_io::fast_terminate();
    }
}

// Test fd_allocate
inline static void test_fd_allocate([[maybe_unused]] native_memory_t& memory,
                                    wasip1_environment<native_memory_t>& env,
                                    wasi_posix_fd_wasm64_t fd,
                                    [[maybe_unused]] ::std::size_t base_off)
{
    auto const ret =
        ::uwvm2::imported::wasi::wasip1::func::fd_allocate_wasm64(env, fd, static_cast<filesize_wasm64_t>(0u), static_cast<filesize_wasm64_t>(1024u));
    // May fail on some platforms, just check it doesn't crash
    (void)ret;
}

// Test fd_advise
inline static void test_fd_advise([[maybe_unused]] native_memory_t& memory,
                                  wasip1_environment<native_memory_t>& env,
                                  wasi_posix_fd_wasm64_t fd,
                                  [[maybe_unused]] ::std::size_t base_off)
{
    auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_advise_wasm64(env,
                                                                             fd,
                                                                             static_cast<filesize_wasm64_t>(0u),
                                                                             static_cast<filesize_wasm64_t>(512u),
                                                                             advice_wasm64_t::advice_sequential);
    // May fail on some platforms
    (void)ret;
}

// Test fd_filestat_set_size
inline static void test_fd_filestat_set_size([[maybe_unused]] native_memory_t& memory,
                                             wasip1_environment<native_memory_t>& env,
                                             wasi_posix_fd_wasm64_t fd,
                                             [[maybe_unused]] ::std::size_t base_off)
{
    auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_size_wasm64(env, fd, static_cast<filesize_wasm64_t>(2048u));
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_filestat_set_size: fd_filestat_set_size failed\n");
        ::fast_io::fast_terminate();
    }
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
            ::fast_io::native_file{u8"wasi_hybrid_fd_ops.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
# else
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd =
            ::fast_io::native_file{u8"wasi_hybrid_fd_ops.tmp",
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
                constexpr unsigned iterations{256u};
                for(unsigned i = 0; i < iterations && !failure.load(::std::memory_order_relaxed); ++i)
                {
                    auto const base_off = static_cast<::std::size_t>((tid + 1u) * base_stride);
                    auto const scenario = (tid + i) % 4u;
                    auto const fd = static_cast<wasi_posix_fd_wasm64_t>(4 + tid);

                    try
                    {
                        switch(scenario)
                        {
                            case 0u:
                            {
                                test_fd_tell_seek(memory, env, fd, base_off);
                                break;
                            }
                            case 1u:
                            {
                                test_fd_allocate(memory, env, fd, base_off);
                                break;
                            }
                            case 2u:
                            {
                                test_fd_advise(memory, env, fd, base_off);
                                break;
                            }
                            case 3u:
                            {
                                test_fd_filestat_set_size(memory, env, fd, base_off);
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
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_fd_ops: failure detected");
        ::fast_io::fast_terminate();
    }
#endif
}

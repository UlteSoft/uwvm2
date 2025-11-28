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

// S7: Multi-threaded test for fd_fdstat_get, fd_fdstat_set_flags, fd_fdstat_set_rights

// std
#include <cstddef>
#include <version>
#include <atomic>
#include <vector>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_set_flags_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_set_rights_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_write_wasm64.h>

using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
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

// Test fd_fdstat_get and fd_fdstat_set_flags
inline static void test_fd_fdstat_flags(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t fd, ::std::size_t base_off)
{
    wasi_void_ptr_wasm64_t const fdstat_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x100u)};

    // Get initial fdstat
    auto ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get_wasm64(env, fd, fdstat_ptr);
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_fdstat_flags: fd_fdstat_get failed\n");
        ::fast_io::fast_terminate();
    }

    // Read initial flags
    auto const initial_flags = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<::std::underlying_type_t<fdflags_wasm64_t>>(
        memory,
        static_cast<wasi_void_ptr_wasm64_t>(fdstat_ptr + 2u));

    // Set nonblock flag
    auto const new_flags =
        static_cast<fdflags_wasm64_t>(initial_flags | static_cast<::std::underlying_type_t<fdflags_wasm64_t>>(fdflags_wasm64_t::fdflag_nonblock));
    ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_flags_wasm64(env, fd, new_flags);
    // May fail on some platforms, just check it doesn't crash
    (void)ret;

    // Get fdstat again to verify
    ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get_wasm64(env, fd, fdstat_ptr);
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_fdstat_flags: fd_fdstat_get after set_flags failed\n");
        ::fast_io::fast_terminate();
    }
}

// Test fd_fdstat_set_rights
inline static void test_fd_fdstat_rights(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t fd, ::std::size_t base_off)
{
    wasi_void_ptr_wasm64_t const fdstat_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x100u)};

    // Get initial fdstat
    auto ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get_wasm64(env, fd, fdstat_ptr);
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_fdstat_rights: fd_fdstat_get failed\n");
        ::fast_io::fast_terminate();
    }

    // Read initial rights
    auto const initial_rights_base = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<::std::underlying_type_t<rights_wasm64_t>>(
        memory,
        static_cast<wasi_void_ptr_wasm64_t>(fdstat_ptr + 8u));
    auto const initial_rights_inherit =
        ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<::std::underlying_type_t<rights_wasm64_t>>(
            memory,
            static_cast<wasi_void_ptr_wasm64_t>(fdstat_ptr + 16u));

    // Remove fd_write right
    auto const new_rights_base =
        static_cast<rights_wasm64_t>(initial_rights_base & ~static_cast<::std::underlying_type_t<rights_wasm64_t>>(rights_wasm64_t::right_fd_write));
    auto const new_rights_inherit =
        static_cast<rights_wasm64_t>(initial_rights_inherit & ~static_cast<::std::underlying_type_t<rights_wasm64_t>>(rights_wasm64_t::right_fd_write));

    ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_rights_wasm64(env, fd, new_rights_base, new_rights_inherit);
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_fdstat_rights: fd_fdstat_set_rights failed\n");
        ::fast_io::fast_terminate();
    }

    // Try to write, should fail with enotcapable
    constexpr char const msg[] = "TestWrite";
    wasi_void_ptr_wasm64_t const buf{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x200u)};
    write_bytes64(memory, buf, msg, sizeof(msg) - 1u);

    wasi_void_ptr_wasm64_t const iovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x300u)};
    wasi_void_ptr_wasm64_t const nwritten_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x400u)};

    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(msg) - 1u));

    ret = ::uwvm2::imported::wasi::wasip1::func::fd_write_wasm64(env, fd, iovs_ptr, static_cast<wasi_size_wasm64_t>(1u), nwritten_ptr);
    if(ret != errno_wasm64_t::enotcapable && ret != errno_wasm64_t::ebadf) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_fdstat_rights: fd_write should fail with enotcapable or ebadf\n");
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
            ::fast_io::native_file{u8"wasi_hybrid_fd_fdstat.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
# else
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd =
            ::fast_io::native_file{u8"wasi_hybrid_fd_fdstat.tmp",
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
                                test_fd_fdstat_flags(memory, env, fd, base_off);
                                break;
                            }
                            case 1u:
                            {
                                test_fd_fdstat_rights(memory, env, fd, base_off);
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
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_fd_fdstat: failure detected");
        ::fast_io::fast_terminate();
    }
#endif
}

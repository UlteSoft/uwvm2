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

// S11: Multi-threaded test for fd_pwrite and fd_pread

// std
#include <cstddef>
#include <cstring>
#include <version>
#include <atomic>
#include <vector>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_pwrite_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_pread_wasm64.h>

using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
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

inline static void read_bytes64(native_memory_t& memory, wasi_void_ptr_wasm64_t p, void* dest, ::std::size_t n)
{
    ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm64(memory,
                                                                         p,
                                                                         reinterpret_cast<::std::byte*>(dest),
                                                                         reinterpret_cast<::std::byte*>(dest) + n);
}

// Test fd_pwrite + fd_pread cycle
inline static void test_pwrite_pread(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t fd, ::std::size_t base_off)
{
    constexpr char const msg[] = "ABC";
    constexpr ::std::size_t offset{128u};
    wasi_void_ptr_wasm64_t const buf_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x100u)};
    wasi_void_ptr_wasm64_t const iovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x200u)};
    wasi_void_ptr_wasm64_t const nwritten_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x300u)};

    // Write test data to memory
    write_bytes64(memory, buf_ptr, msg, sizeof(msg) - 1u);

    // Setup iovec for write
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf_ptr);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(msg) - 1u));

    // Perform pwrite at offset K
    auto ret = ::uwvm2::imported::wasi::wasip1::func::fd_pwrite_wasm64(env,
                                                                       fd,
                                                                       iovs_ptr,
                                                                       static_cast<wasi_size_wasm64_t>(1u),
                                                                       static_cast<filesize_wasm64_t>(offset),
                                                                       nwritten_ptr);
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_pwrite_pread: fd_pwrite failed\n");
        ::fast_io::fast_terminate();
    }

    auto const nwritten = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, nwritten_ptr);
    if(nwritten != sizeof(msg) - 1u) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_pwrite_pread: wrong number of bytes written\n");
        ::fast_io::fast_terminate();
    }

    // Setup for pread
    wasi_void_ptr_wasm64_t const read_buf_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x400u)};
    wasi_void_ptr_wasm64_t const read_iovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x500u)};
    wasi_void_ptr_wasm64_t const nread_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x600u)};

    // Clear read buffer
    char zeros[3] = {0, 0, 0};
    write_bytes64(memory, read_buf_ptr, zeros, 3u);

    // Setup iovec for read
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, read_iovs_ptr, read_buf_ptr);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(read_iovs_ptr + 8u),
                                                                                    static_cast<wasi_size_wasm64_t>(3u));

    // Perform pread at same offset K
    ret = ::uwvm2::imported::wasi::wasip1::func::fd_pread_wasm64(env,
                                                                 fd,
                                                                 read_iovs_ptr,
                                                                 static_cast<wasi_size_wasm64_t>(1u),
                                                                 static_cast<filesize_wasm64_t>(offset),
                                                                 nread_ptr);
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_pwrite_pread: fd_pread failed\n");
        ::fast_io::fast_terminate();
    }

    auto const nread = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, nread_ptr);
    if(nread != 3u) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_pwrite_pread: wrong number of bytes read\n");
        ::fast_io::fast_terminate();
    }

    // Verify data
    char read_data[3];
    read_bytes64(memory, read_buf_ptr, read_data, 3u);
    if(::std::memcmp(read_data, msg, 3u) != 0) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_pwrite_pread: data mismatch\n");
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
            ::fast_io::native_file{u8"wasi_hybrid_fd_pread_pwrite.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
# else
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd =
            ::fast_io::native_file{u8"wasi_hybrid_fd_pread_pwrite.tmp",
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
                    auto const fd = static_cast<wasi_posix_fd_wasm64_t>(4 + tid);

                    try
                    {
                        test_pwrite_pread(memory, env, fd, base_off);
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
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_fd_pread_pwrite: failure detected");
        ::fast_io::fast_terminate();
    }
#endif
}

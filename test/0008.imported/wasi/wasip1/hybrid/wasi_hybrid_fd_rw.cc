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

// Multi-threaded test for fd read/write operations: fd_read, fd_write, fd_pread, fd_pwrite

// std
#include <cstddef>
#include <cstdint>
#include <version>
#include <atomic>
#include <thread>
#include <vector>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_write_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_read_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_pread_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_pwrite_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_seek_wasm64.h>

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

// Test fd_write and fd_read
inline static void test_fd_write_read(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t fd, ::std::size_t base_off)
{
    constexpr char const msg[] = "WriteReadTest123";
    wasi_void_ptr_wasm64_t const write_buf{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x100u)};
    write_bytes64(memory, write_buf, msg, sizeof(msg) - 1u);

    wasi_void_ptr_wasm64_t const iovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x200u)};
    wasi_void_ptr_wasm64_t const nwritten_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x300u)};

    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, write_buf);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(msg) - 1u));

    // Write
    auto const ret_w = ::uwvm2::imported::wasi::wasip1::func::fd_write_wasm64(env, fd, iovs_ptr, static_cast<wasi_size_wasm64_t>(1u), nwritten_ptr);
    if(ret_w != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_write failed in test_fd_write_read");
        ::fast_io::fast_terminate();
    }

    // Seek to beginning
    wasi_void_ptr_wasm64_t const newoff_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x400u)};
    auto const ret_seek = ::uwvm2::imported::wasi::wasip1::func::fd_seek_wasm64(env,
                                                                                fd,
                                                                                ::uwvm2::imported::wasi::wasip1::abi::filedelta_wasm64_t{},
                                                                                ::uwvm2::imported::wasi::wasip1::abi::whence_wasm64_t::whence_set,
                                                                                newoff_ptr);
    if(ret_seek != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_seek failed in test_fd_write_read");
        ::fast_io::fast_terminate();
    }

    // Read
    wasi_void_ptr_wasm64_t const read_buf{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x500u)};
    wasi_void_ptr_wasm64_t const riovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x600u)};
    wasi_void_ptr_wasm64_t const nread_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x700u)};

    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, riovs_ptr, read_buf);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(riovs_ptr + 8u),
                                                                                    static_cast<wasi_size_wasm64_t>(64u));

    auto const ret_r = ::uwvm2::imported::wasi::wasip1::func::fd_read_wasm64(env, fd, riovs_ptr, static_cast<wasi_size_wasm64_t>(1u), nread_ptr);
    if(ret_r != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_read failed in test_fd_write_read");
        ::fast_io::fast_terminate();
    }

    auto const nread = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, nread_ptr);
    if(nread < static_cast<wasi_size_wasm64_t>(sizeof(msg) - 1u)) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"insufficient bytes read in test_fd_write_read");
        ::fast_io::fast_terminate();
    }
}

// Test fd_pwrite and fd_pread
inline static void test_fd_pwrite_pread(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t fd, ::std::size_t base_off)
{
    constexpr char const msg1[] = "PWrite1";
    constexpr char const msg2[] = "PWrite2";

    wasi_void_ptr_wasm64_t const buf1{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x800u)};
    wasi_void_ptr_wasm64_t const buf2{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x900u)};
    write_bytes64(memory, buf1, msg1, sizeof(msg1) - 1u);
    write_bytes64(memory, buf2, msg2, sizeof(msg2) - 1u);

    wasi_void_ptr_wasm64_t const iovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0xa00u)};
    wasi_void_ptr_wasm64_t const nwritten_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0xb00u)};

    // First iovec
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf1);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(msg1) - 1u));

    // pwrite at offset 0
    auto const ret_pw1 = ::uwvm2::imported::wasi::wasip1::func::fd_pwrite_wasm64(env,
                                                                                 fd,
                                                                                 iovs_ptr,
                                                                                 static_cast<wasi_size_wasm64_t>(1u),
                                                                                 static_cast<filesize_wasm64_t>(0u),
                                                                                 nwritten_ptr);
    if(ret_pw1 != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pwrite failed at offset 0");
        ::fast_io::fast_terminate();
    }

    // Second iovec
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf2);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(msg2) - 1u));

    // pwrite at offset 100
    auto const ret_pw2 = ::uwvm2::imported::wasi::wasip1::func::fd_pwrite_wasm64(env,
                                                                                 fd,
                                                                                 iovs_ptr,
                                                                                 static_cast<wasi_size_wasm64_t>(1u),
                                                                                 static_cast<filesize_wasm64_t>(100u),
                                                                                 nwritten_ptr);
    if(ret_pw2 != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pwrite failed at offset 100");
        ::fast_io::fast_terminate();
    }

    // pread from offset 0
    wasi_void_ptr_wasm64_t const read_buf{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0xc00u)};
    wasi_void_ptr_wasm64_t const riovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0xd00u)};
    wasi_void_ptr_wasm64_t const nread_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0xe00u)};

    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, riovs_ptr, read_buf);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(riovs_ptr + 8u),
                                                                                    static_cast<wasi_size_wasm64_t>(32u));

    auto const ret_pr = ::uwvm2::imported::wasi::wasip1::func::fd_pread_wasm64(env,
                                                                               fd,
                                                                               riovs_ptr,
                                                                               static_cast<wasi_size_wasm64_t>(1u),
                                                                               static_cast<filesize_wasm64_t>(0u),
                                                                               nread_ptr);
    if(ret_pr != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread failed");
        ::fast_io::fast_terminate();
    }
}

// Test multiple iovecs
inline static void
    test_fd_write_multiple_iovecs(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t fd, ::std::size_t base_off)
{
    constexpr char const msg1[] = "Part1";
    constexpr char const msg2[] = "Part2";
    constexpr char const msg3[] = "Part3";

    wasi_void_ptr_wasm64_t const buf1{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1000u)};
    wasi_void_ptr_wasm64_t const buf2{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1100u)};
    wasi_void_ptr_wasm64_t const buf3{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1200u)};

    write_bytes64(memory, buf1, msg1, sizeof(msg1) - 1u);
    write_bytes64(memory, buf2, msg2, sizeof(msg2) - 1u);
    write_bytes64(memory, buf3, msg3, sizeof(msg3) - 1u);

    wasi_void_ptr_wasm64_t const iovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1300u)};
    wasi_void_ptr_wasm64_t const nwritten_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1400u)};

    // Three iovecs
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf1);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(msg1) - 1u));

    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 16u), buf2);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 24u),
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(msg2) - 1u));

    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 32u), buf3);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 40u),
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(msg3) - 1u));

    auto const ret_w = ::uwvm2::imported::wasi::wasip1::func::fd_write_wasm64(env, fd, iovs_ptr, static_cast<wasi_size_wasm64_t>(3u), nwritten_ptr);
    if(ret_w != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_write failed with multiple iovecs");
        ::fast_io::fast_terminate();
    }

    auto const nwritten = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, nwritten_ptr);
    if(nwritten < static_cast<wasi_size_wasm64_t>(sizeof(msg1) + sizeof(msg2) + sizeof(msg3) - 3u)) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"insufficient bytes written with multiple iovecs");
        ::fast_io::fast_terminate();
    }
}

// Test alternating read/write
inline static void test_fd_alternating_rw(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t fd, ::std::size_t base_off)
{
    for(unsigned i = 0; i < 4u; ++i)
    {
        // Write
        char msg[16];
        ::std::snprintf(msg, sizeof(msg), "Iter%u", i);
        wasi_void_ptr_wasm64_t const write_buf{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1500u)};
        write_bytes64(memory, write_buf, msg, ::std::strlen(msg));

        wasi_void_ptr_wasm64_t const iovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1600u)};
        wasi_void_ptr_wasm64_t const nwritten_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1700u)};

        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, write_buf);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                        static_cast<wasi_size_wasm64_t>(::std::strlen(msg)));

        auto const ret_w = ::uwvm2::imported::wasi::wasip1::func::fd_write_wasm64(env, fd, iovs_ptr, static_cast<wasi_size_wasm64_t>(1u), nwritten_ptr);
        if(ret_w != errno_wasm64_t::esuccess) [[unlikely]]
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_write failed in alternating test");
            ::fast_io::fast_terminate();
        }

        // Seek back
        wasi_void_ptr_wasm64_t const newoff_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1800u)};
        auto const ret_seek = ::uwvm2::imported::wasi::wasip1::func::fd_seek_wasm64(
            env,
            fd,
            static_cast<::uwvm2::imported::wasi::wasip1::abi::filedelta_wasm64_t>(
                -static_cast<::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filedelta_wasm64_t>>(::std::strlen(msg))),
            ::uwvm2::imported::wasi::wasip1::abi::whence_wasm64_t::whence_cur,
            newoff_ptr);
        if(ret_seek != errno_wasm64_t::esuccess) [[unlikely]]
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_seek failed in alternating test");
            ::fast_io::fast_terminate();
        }

        // Read
        wasi_void_ptr_wasm64_t const read_buf{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1900u)};
        wasi_void_ptr_wasm64_t const riovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1a00u)};
        wasi_void_ptr_wasm64_t const nread_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1b00u)};

        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, riovs_ptr, read_buf);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(riovs_ptr + 8u),
                                                                                        static_cast<wasi_size_wasm64_t>(16u));

        auto const ret_r = ::uwvm2::imported::wasi::wasip1::func::fd_read_wasm64(env, fd, riovs_ptr, static_cast<wasi_size_wasm64_t>(1u), nread_ptr);
        if(ret_r != errno_wasm64_t::esuccess) [[unlikely]]
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_read failed in alternating test");
            ::fast_io::fast_terminate();
        }
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
            ::fast_io::native_file{u8"wasi_hybrid_fd_rw.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
# else
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd =
            ::fast_io::native_file{u8"wasi_hybrid_fd_rw.tmp",
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
                                test_fd_write_read(memory, env, fd, base_off);
                                break;
                            }
                            case 1u:
                            {
                                test_fd_pwrite_pread(memory, env, fd, base_off);
                                break;
                            }
                            case 2u:
                            {
                                test_fd_write_multiple_iovecs(memory, env, fd, base_off);
                                break;
                            }
                            case 3u:
                            {
                                test_fd_alternating_rw(memory, env, fd, base_off);
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
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_fd_rw: failure detected");
        ::fast_io::fast_terminate();
    }
#endif
}

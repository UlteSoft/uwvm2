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

// Multi-threaded test for fd_renumber and fd_close operations

// std
#include <cstddef>
#include <version>
#include <atomic>
#include <vector>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_renumber_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_write_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open_wasm64.h>

using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;
using ::uwvm2::utils::container::u8string;

inline static void write_cu8str64(native_memory_t& memory, wasi_void_ptr_wasm64_t p, char8_t const* s)
{
    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64(memory,
                                                                        p,
                                                                        reinterpret_cast<::std::byte const*>(s),
                                                                        reinterpret_cast<::std::byte const*>(s) + ::std::char_traits<char8_t>::length(s));
}

inline static void write_bytes64(native_memory_t& memory, wasi_void_ptr_wasm64_t p, void const* s, ::std::size_t n)
{
    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64(memory,
                                                                        p,
                                                                        reinterpret_cast<::std::byte const*>(s),
                                                                        reinterpret_cast<::std::byte const*>(s) + n);
}

inline static void set_dirfd_root(wasip1_environment<native_memory_t>& env, ::std::size_t idx, rights_t base_rights, rights_t inherit_rights)
{
    auto& fd = *env.fd_storage.opens.index_unchecked(idx).fd_p;
    fd.rights_base = base_rights;
    fd.rights_inherit = inherit_rights;
    fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
    auto& ds = fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
    ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
    entry.ptr->dir_stack.storage.file = ::fast_io::dir_file{u8"."};
    ds.dir_stack.push_back(::std::move(entry));
}

// Test fd_renumber
inline static void
    test_fd_renumber(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t dirfd, unsigned tid, ::std::size_t base_off)
{
    u8string fname{u8"renumber_test_"};
    fname.push_back(static_cast<char8_t>('0' + static_cast<int>(tid % 10u)));

    wasi_void_ptr_wasm64_t const path_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x100u)};
    wasi_void_ptr_wasm64_t const fd_out_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x200u)};

    write_cu8str64(memory, path_ptr, fname.c_str());

    // Open a file
    auto const ret_open = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                  dirfd,
                                                                                  static_cast<lookupflags_wasm64_t>(0),
                                                                                  path_ptr,
                                                                                  static_cast<wasi_size_wasm64_t>(fname.size()),
                                                                                  static_cast<oflags_wasm64_t>(oflags_wasm64_t::o_creat),
                                                                                  rights_wasm64_t::right_fd_write | rights_wasm64_t::right_fd_read,
                                                                                  static_cast<rights_wasm64_t>(0),
                                                                                  static_cast<fdflags_wasm64_t>(0),
                                                                                  fd_out_ptr);
    if(ret_open != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"path_open failed in test_fd_renumber");
        ::fast_io::fast_terminate();
    }

    auto const fd_from = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_posix_fd_wasm64_t>(memory, fd_out_ptr);

    // Choose a target fd (use a high number to avoid conflicts)
    auto const fd_to = static_cast<wasi_posix_fd_wasm64_t>(100 + tid);

    // Renumber
    auto const ret_renumber = ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env, fd_from, fd_to);
    if(ret_renumber != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber failed");
        ::fast_io::fast_terminate();
    }

    // Write to the renumbered fd
    constexpr char const msg[] = "Renumbered";
    wasi_void_ptr_wasm64_t const buf{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x300u)};
    write_bytes64(memory, buf, msg, sizeof(msg) - 1u);

    wasi_void_ptr_wasm64_t const iovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x400u)};
    wasi_void_ptr_wasm64_t const nwritten_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x500u)};

    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(msg) - 1u));

    auto const ret_w = ::uwvm2::imported::wasi::wasip1::func::fd_write_wasm64(env, fd_to, iovs_ptr, static_cast<wasi_size_wasm64_t>(1u), nwritten_ptr);
    if(ret_w != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_write failed");
        ::fast_io::fast_terminate();
    }

    // Close the renumbered fd
    auto const ret_close = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, fd_to);
    if(ret_close != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_close failed");
        ::fast_io::fast_terminate();
    }
}

// Test multiple open/close cycles
inline static void test_fd_open_close_cycles(native_memory_t& memory,
                                             wasip1_environment<native_memory_t>& env,
                                             wasi_posix_fd_wasm64_t dirfd,
                                             unsigned tid,
                                             ::std::size_t base_off)
{
    u8string fname{u8"cycle_test_"};
    fname.push_back(static_cast<char8_t>('0' + static_cast<int>(tid % 10u)));

    wasi_void_ptr_wasm64_t const path_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x600u)};
    wasi_void_ptr_wasm64_t const fd_out_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x700u)};

    write_cu8str64(memory, path_ptr, fname.c_str());

    for(unsigned i = 0; i < 8u; ++i)
    {
        // Open
        auto const ret_open = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                      dirfd,
                                                                                      static_cast<lookupflags_wasm64_t>(0),
                                                                                      path_ptr,
                                                                                      static_cast<wasi_size_wasm64_t>(fname.size()),
                                                                                      static_cast<oflags_wasm64_t>(oflags_wasm64_t::o_creat),
                                                                                      rights_wasm64_t::right_fd_write,
                                                                                      static_cast<rights_wasm64_t>(0),
                                                                                      static_cast<fdflags_wasm64_t>(0),
                                                                                      fd_out_ptr);
        if(ret_open != errno_wasm64_t::esuccess) [[unlikely]]
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"path_open failed in cycle test");
            ::fast_io::fast_terminate();
        }

        auto const opened_fd = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_posix_fd_wasm64_t>(memory, fd_out_ptr);

        // Close
        auto const ret_close = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, opened_fd);
        if(ret_close != errno_wasm64_t::esuccess) [[unlikely]]
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_close failed in cycle test");
            ::fast_io::fast_terminate();
        }
    }
}

// Test close on already closed fd (should fail)
inline static void
    test_fd_double_close(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t dirfd, unsigned tid, ::std::size_t base_off)
{
    u8string fname{u8"double_close_"};
    fname.push_back(static_cast<char8_t>('0' + static_cast<int>(tid % 10u)));

    wasi_void_ptr_wasm64_t const path_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x800u)};
    wasi_void_ptr_wasm64_t const fd_out_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x900u)};

    write_cu8str64(memory, path_ptr, fname.c_str());

    auto const ret_open = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                  dirfd,
                                                                                  static_cast<lookupflags_wasm64_t>(0),
                                                                                  path_ptr,
                                                                                  static_cast<wasi_size_wasm64_t>(fname.size()),
                                                                                  static_cast<oflags_wasm64_t>(oflags_wasm64_t::o_creat),
                                                                                  rights_wasm64_t::right_fd_write,
                                                                                  static_cast<rights_wasm64_t>(0),
                                                                                  static_cast<fdflags_wasm64_t>(0),
                                                                                  fd_out_ptr);
    if(ret_open != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"path_open failed in test_fd_double_close");
        ::fast_io::fast_terminate();
    }

    auto const opened_fd = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_posix_fd_wasm64_t>(memory, fd_out_ptr);

    // First close
    auto const ret_close1 = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, opened_fd);
    if(ret_close1 != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"first fd_close failed");
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
                                            .fd_storage = {.fd_limit = 256uz},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false,
                                            .disable_utf8_check = false};

    env.fd_storage.opens.resize(static_cast<::std::size_t>(4u));
    set_dirfd_root(env, 3uz, static_cast<rights_t>(-1), static_cast<rights_t>(-1));

    std::atomic<bool> failure{false};

    ::std::vector<fast_io::native_thread> threads;
    threads.reserve(thread_count);

    for(unsigned tid = 0; tid < thread_count; ++tid)
    {
        threads.emplace_back(
            [&memory, &env, &failure, tid]()
            {
                constexpr unsigned iterations{64u};
                for(unsigned i = 0; i < iterations && !failure.load(::std::memory_order_relaxed); ++i)
                {
                    auto const base_off = static_cast<::std::size_t>((tid + 1u) * base_stride);
                    auto const scenario = (tid + i) % 3u;
                    auto const dirfd = static_cast<wasi_posix_fd_wasm64_t>(3);

                    try
                    {
                        switch(scenario)
                        {
                            case 0u:
                            {
                                test_fd_renumber(memory, env, dirfd, tid, base_off);
                                break;
                            }
                            case 1u:
                            {
                                test_fd_open_close_cycles(memory, env, dirfd, tid, base_off);
                                break;
                            }
                            case 2u:
                            {
                                test_fd_double_close(memory, env, dirfd, tid, base_off);
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
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_fd_renumber: failure detected");
        ::fast_io::fast_terminate();
    }
#endif
}

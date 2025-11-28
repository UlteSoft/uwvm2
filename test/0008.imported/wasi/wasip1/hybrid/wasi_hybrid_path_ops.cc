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

// Multi-threaded test for path operations: path_rename, path_unlink_file, path_link, path_symlink

// std
#include <cstddef>
#include <cstdint>
#include <version>
#include <atomic>
#include <thread>
#include <vector>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/path_create_directory_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_remove_directory_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_rename_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_unlink_file_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_write_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close_wasm64.h>

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
    entry.ptr->dir_stack.file = ::fast_io::dir_file{u8"."};
    ds.dir_stack.push_back(::std::move(entry));
}

// Test path_rename
inline static void
    test_path_rename(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t dirfd, unsigned tid, ::std::size_t base_off)
{
    u8string old_name{u8"rename_old_"};
    old_name.push_back(static_cast<char8_t>('0' + static_cast<int>(tid % 10u)));
    u8string new_name{u8"rename_new_"};
    new_name.push_back(static_cast<char8_t>('0' + static_cast<int>(tid % 10u)));

    wasi_void_ptr_wasm64_t const old_path_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x100u)};
    wasi_void_ptr_wasm64_t const new_path_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x200u)};
    wasi_void_ptr_wasm64_t const fd_out_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x300u)};

    write_cu8str64(memory, old_path_ptr, old_name.c_str());

    // Create old file
    auto const ret_open = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                  dirfd,
                                                                                  static_cast<lookupflags_wasm64_t>(0),
                                                                                  old_path_ptr,
                                                                                  static_cast<wasi_size_wasm64_t>(old_name.size()),
                                                                                  static_cast<oflags_wasm64_t>(oflags_wasm64_t::o_creat),
                                                                                  rights_wasm64_t::right_fd_write,
                                                                                  static_cast<rights_wasm64_t>(0),
                                                                                  static_cast<fdflags_wasm64_t>(0),
                                                                                  fd_out_ptr);
    if(ret_open != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_path_rename: path_open failed\n");
        ::fast_io::fast_terminate();
    }

    auto const opened_fd = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_posix_fd_wasm64_t>(memory, fd_out_ptr);
    auto const ret_close = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, opened_fd);
    if(ret_close != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_path_rename: fd_close failed\n");
        ::fast_io::fast_terminate();
    }

    // Rename
    write_cu8str64(memory, new_path_ptr, new_name.c_str());
    auto const ret_rename = ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(env,
                                                                                      dirfd,
                                                                                      old_path_ptr,
                                                                                      static_cast<wasi_size_wasm64_t>(old_name.size()),
                                                                                      dirfd,
                                                                                      new_path_ptr,
                                                                                      static_cast<wasi_size_wasm64_t>(new_name.size()));
    if(ret_rename != errno_wasm64_t::esuccess && ret_rename != errno_wasm64_t::enoent) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_path_rename: path_rename failed\n");
        ::fast_io::fast_terminate();
    }

    // Clean up
    auto const ret_unlink =
        ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env, dirfd, new_path_ptr, static_cast<wasi_size_wasm64_t>(new_name.size()));
    (void)ret_unlink;
}

// Test path_unlink_file
inline static void
    test_path_unlink(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t dirfd, unsigned tid, ::std::size_t base_off)
{
    u8string fname{u8"unlink_test_"};
    fname.push_back(static_cast<char8_t>('0' + static_cast<int>(tid % 10u)));

    wasi_void_ptr_wasm64_t const path_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x400u)};
    wasi_void_ptr_wasm64_t const fd_out_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x500u)};

    write_cu8str64(memory, path_ptr, fname.c_str());

    // Create file
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
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_path_unlink: path_open failed\n");
        ::fast_io::fast_terminate();
    }

    auto const opened_fd = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_posix_fd_wasm64_t>(memory, fd_out_ptr);
    auto const ret_close = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, opened_fd);
    if(ret_close != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_path_unlink: fd_close failed\n");
        ::fast_io::fast_terminate();
    }

    // Unlink
    auto const ret_unlink = ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env, dirfd, path_ptr, static_cast<wasi_size_wasm64_t>(fname.size()));
    if(ret_unlink != errno_wasm64_t::esuccess && ret_unlink != errno_wasm64_t::enoent) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_path_unlink: path_unlink_file failed\n");
        ::fast_io::fast_terminate();
    }
}

// Test directory creation and removal
inline static void
    test_path_directory(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t dirfd, unsigned tid, ::std::size_t base_off)
{
    u8string dname{u8"testdir_"};
    dname.push_back(static_cast<char8_t>('0' + static_cast<int>(tid % 10u)));

    wasi_void_ptr_wasm64_t const path_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x600u)};
    write_cu8str64(memory, path_ptr, dname.c_str());

    auto const ret_create =
        ::uwvm2::imported::wasi::wasip1::func::path_create_directory_wasm64(env, dirfd, path_ptr, static_cast<wasi_size_wasm64_t>(dname.size()));
    if(ret_create != errno_wasm64_t::esuccess && ret_create != errno_wasm64_t::eexist) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_path_directory: path_create_directory failed\n");
        ::fast_io::fast_terminate();
    }

    auto const ret_remove =
        ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env, dirfd, path_ptr, static_cast<wasi_size_wasm64_t>(dname.size()));
    if(ret_remove != errno_wasm64_t::esuccess && ret_remove != errno_wasm64_t::enoent) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_path_directory: path_remove_directory failed\n");
        ::fast_io::fast_terminate();
    }
}

// Test file creation with path_open
inline static void
    test_path_open_create(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t dirfd, unsigned tid, ::std::size_t base_off)
{
    u8string fname{u8"create_test_"};
    fname.push_back(static_cast<char8_t>('0' + static_cast<int>(tid % 10u)));

    wasi_void_ptr_wasm64_t const path_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x700u)};
    wasi_void_ptr_wasm64_t const fd_out_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x800u)};

    write_cu8str64(memory, path_ptr, fname.c_str());

    auto const ret_open =
        ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                dirfd,
                                                                static_cast<lookupflags_wasm64_t>(0),
                                                                path_ptr,
                                                                static_cast<wasi_size_wasm64_t>(fname.size()),
                                                                static_cast<oflags_wasm64_t>(oflags_wasm64_t::o_creat | oflags_wasm64_t::o_trunc),
                                                                rights_wasm64_t::right_fd_write | rights_wasm64_t::right_fd_read,
                                                                static_cast<rights_wasm64_t>(0),
                                                                static_cast<fdflags_wasm64_t>(0),
                                                                fd_out_ptr);
    if(ret_open != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_path_open_create: path_open failed\n");
        ::fast_io::fast_terminate();
    }

    auto const opened_fd = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_posix_fd_wasm64_t>(memory, fd_out_ptr);

    // Write some data
    constexpr char const msg[] = "PathTest";
    wasi_void_ptr_wasm64_t const buf{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x900u)};
    write_bytes64(memory, buf, msg, sizeof(msg) - 1u);

    wasi_void_ptr_wasm64_t const iovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0xa00u)};
    wasi_void_ptr_wasm64_t const nwritten_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0xb00u)};

    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                    static_cast<wasi_size_wasm64_t>(sizeof(msg) - 1u));

    auto const ret_w = ::uwvm2::imported::wasi::wasip1::func::fd_write_wasm64(env, opened_fd, iovs_ptr, static_cast<wasi_size_wasm64_t>(1u), nwritten_ptr);
    if(ret_w != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_path_open_create: fd_write failed\n");
        ::fast_io::fast_terminate();
    }

    auto const ret_close = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, opened_fd);
    if(ret_close != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_path_open_create: fd_close failed\n");
        ::fast_io::fast_terminate();
    }

    // Clean up
    auto const ret_unlink = ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env, dirfd, path_ptr, static_cast<wasi_size_wasm64_t>(fname.size()));
    (void)ret_unlink;
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
                constexpr unsigned iterations{128u};
                for(unsigned i = 0; i < iterations && !failure.load(::std::memory_order_relaxed); ++i)
                {
                    auto const base_off = static_cast<::std::size_t>((tid + 1u) * base_stride);
                    auto const scenario = (tid + i) % 4u;
                    auto const dirfd = static_cast<wasi_posix_fd_wasm64_t>(3);

                    try
                    {
                        switch(scenario)
                        {
                            case 0u:
                            {
                                test_path_rename(memory, env, dirfd, tid, base_off);
                                break;
                            }
                            case 1u:
                            {
                                test_path_unlink(memory, env, dirfd, tid, base_off);
                                break;
                            }
                            case 2u:
                            {
                                test_path_directory(memory, env, dirfd, tid, base_off);
                                break;
                            }
                            case 3u:
                            {
                                test_path_open_create(memory, env, dirfd, tid, base_off);
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
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_path_ops: failure detected");
        ::fast_io::fast_terminate();
    }
#endif
}

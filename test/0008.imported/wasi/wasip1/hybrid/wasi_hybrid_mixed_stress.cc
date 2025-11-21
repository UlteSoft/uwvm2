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

// Comprehensive stress test mixing all WASI operations in multi-threaded environment

// std
#include <cstddef>
#include <cstdint>
#include <version>
#include <atomic>
#include <thread>
#include <vector>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/args_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/args_sizes_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/environ_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/environ_sizes_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_write_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_read_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_pread_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_pwrite_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_seek_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_sync_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_datasync_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_filestat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_tell_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_create_directory_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_filestat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_remove_directory_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/clock_res_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/clock_time_get_wasm64.h>

using ::uwvm2::imported::wasi::wasip1::abi::clockid_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::filetype_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;
using ::uwvm2::utils::container::u8string;
using ::uwvm2::utils::container::u8string_view;
using ::uwvm2::utils::container::vector;

inline static void write_bytes64(native_memory_t& memory, wasi_void_ptr_wasm64_t p, void const* s, ::std::size_t n)
{
    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64(memory,
                                                                        p,
                                                                        reinterpret_cast<::std::byte const*>(s),
                                                                        reinterpret_cast<::std::byte const*>(s) + n);
}

inline static void write_cu8str64(native_memory_t& memory, wasi_void_ptr_wasm64_t p, char8_t const* s)
{ write_bytes64(memory, p, s, ::std::char_traits<char8_t>::length(s)); }

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

// Comprehensive stress scenario combining multiple operations
inline static void stress_scenario_comprehensive(native_memory_t& memory,
                                                 wasip1_environment<native_memory_t>& env,
                                                 wasi_posix_fd_wasm64_t fd,
                                                 wasi_posix_fd_wasm64_t dirfd,
                                                 unsigned tid,
                                                 ::std::size_t base_off)
{
    // 1. Check args
    wasi_void_ptr_wasm64_t const argc_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x10u)};
    wasi_void_ptr_wasm64_t const argv_buf_size_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x20u)};
    auto const ret_args = ::uwvm2::imported::wasi::wasip1::func::args_sizes_get_wasm64(env, argc_ptr, argv_buf_size_ptr);
    if(ret_args != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"args_sizes_get failed");
        ::fast_io::fast_terminate();
    }

    // 2. Check environ
    wasi_void_ptr_wasm64_t const envc_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x30u)};
    wasi_void_ptr_wasm64_t const envbuf_size_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x40u)};
    auto const ret_env = ::uwvm2::imported::wasi::wasip1::func::environ_sizes_get_wasm64(env, envc_ptr, envbuf_size_ptr);
    if(ret_env != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"environ_sizes_get failed");
        ::fast_io::fast_terminate();
    }

    // 3. Clock operations
    wasi_void_ptr_wasm64_t const res_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x50u)};
    auto const ret_clock_res = ::uwvm2::imported::wasi::wasip1::func::clock_res_get_wasm64(env, clockid_wasm64_t::clock_monotonic, res_ptr);
    if(ret_clock_res != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"clock_res_get failed");
        ::fast_io::fast_terminate();
    }

    wasi_void_ptr_wasm64_t const time_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x60u)};
    auto const ret_time =
        ::uwvm2::imported::wasi::wasip1::func::clock_time_get_wasm64(env, clockid_wasm64_t::clock_monotonic, static_cast<timestamp_wasm64_t>(0u), time_ptr);
    if(ret_time != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"clock_time_get failed");
        ::fast_io::fast_terminate();
    }

    // 4. File write operations
    constexpr char const msg[] = "StressTest";
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
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_write failed");
        ::fast_io::fast_terminate();
    }

    // 5. Sync operations
    auto const ret_sync = ::uwvm2::imported::wasi::wasip1::func::fd_sync_wasm64(env, fd);
    if(ret_sync != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_sync failed");
        ::fast_io::fast_terminate();
    }

    auto const ret_datasync = ::uwvm2::imported::wasi::wasip1::func::fd_datasync_wasm64(env, fd);
    if(ret_datasync != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_datasync failed");
        ::fast_io::fast_terminate();
    }

    // 6. Tell and seek
    wasi_void_ptr_wasm64_t const tell_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x400u)};
    auto const ret_tell = ::uwvm2::imported::wasi::wasip1::func::fd_tell_wasm64(env, fd, tell_ptr);
    if(ret_tell != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_tell failed");
        ::fast_io::fast_terminate();
    }

    wasi_void_ptr_wasm64_t const newoff_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x500u)};
    auto const ret_seek = ::uwvm2::imported::wasi::wasip1::func::fd_seek_wasm64(env,
                                                                                fd,
                                                                                ::uwvm2::imported::wasi::wasip1::abi::filedelta_wasm64_t{},
                                                                                ::uwvm2::imported::wasi::wasip1::abi::whence_wasm64_t::whence_set,
                                                                                newoff_ptr);
    if(ret_seek != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_seek failed");
        ::fast_io::fast_terminate();
    }

    // 7. Read back
    wasi_void_ptr_wasm64_t const read_buf{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x600u)};
    wasi_void_ptr_wasm64_t const riovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x700u)};
    wasi_void_ptr_wasm64_t const nread_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x800u)};

    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, riovs_ptr, read_buf);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                    static_cast<wasi_void_ptr_wasm64_t>(riovs_ptr + 8u),
                                                                                    static_cast<wasi_size_wasm64_t>(64u));

    auto const ret_r = ::uwvm2::imported::wasi::wasip1::func::fd_read_wasm64(env, fd, riovs_ptr, static_cast<wasi_size_wasm64_t>(1u), nread_ptr);
    if(ret_r != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_read failed");
        ::fast_io::fast_terminate();
    }

    // 8. Stat operations
    wasi_void_ptr_wasm64_t const stat_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x900u)};
    auto const ret_fstat = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get_wasm64(env, fd, stat_ptr);
    if(ret_fstat != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_get failed");
        ::fast_io::fast_terminate();
    }

    wasi_void_ptr_wasm64_t const fdstat_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0xa00u)};
    auto const ret_fdstat = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get_wasm64(env, fd, fdstat_ptr);
    if(ret_fdstat != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get failed");
        ::fast_io::fast_terminate();
    }

    // 9. pread/pwrite
    auto const ret_pw = ::uwvm2::imported::wasi::wasip1::func::fd_pwrite_wasm64(env,
                                                                                fd,
                                                                                iovs_ptr,
                                                                                static_cast<wasi_size_wasm64_t>(1u),
                                                                                static_cast<filesize_wasm64_t>(50u),
                                                                                nwritten_ptr);
    if(ret_pw != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pwrite failed");
        ::fast_io::fast_terminate();
    }

    auto const ret_pr = ::uwvm2::imported::wasi::wasip1::func::fd_pread_wasm64(env,
                                                                               fd,
                                                                               riovs_ptr,
                                                                               static_cast<wasi_size_wasm64_t>(1u),
                                                                               static_cast<filesize_wasm64_t>(50u),
                                                                               nread_ptr);
    if(ret_pr != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread failed");
        ::fast_io::fast_terminate();
    }

    // 10. Directory operations
    {
        static ::uwvm2::utils::mutex::mutex_t m{};
        ::uwvm2::utils::mutex::mutex_guard_t lg{m};

        u8string dname{u8"stress_dir_"};
        dname.push_back(static_cast<char8_t>('0' + static_cast<int>(tid % 10u)));

        wasi_void_ptr_wasm64_t const path_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0xb00u)};
        write_cu8str64(memory, path_ptr, dname.c_str());

        auto const ret_mkdir =
            ::uwvm2::imported::wasi::wasip1::func::path_create_directory_wasm64(env, dirfd, path_ptr, static_cast<wasi_size_wasm64_t>(dname.size()));
        if(ret_mkdir != errno_wasm64_t::esuccess && ret_mkdir != errno_wasm64_t::eexist) [[unlikely]]
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"path_create_directory failed");
            ::fast_io::fast_terminate();
        }

        wasi_void_ptr_wasm64_t const pstat_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0xc00u)};
        auto const ret_pstat = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get_wasm64(env,
                                                                                               dirfd,
                                                                                               static_cast<lookupflags_wasm64_t>(0),
                                                                                               path_ptr,
                                                                                               static_cast<wasi_size_wasm64_t>(dname.size()),
                                                                                               pstat_ptr);
        if(ret_pstat != errno_wasm64_t::esuccess) [[unlikely]]
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"path_filestat_get failed");
            ::fast_io::fast_terminate();
        }

        auto const ret_rmdir =
            ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env, dirfd, path_ptr, static_cast<wasi_size_wasm64_t>(dname.size()));
        if(ret_rmdir != errno_wasm64_t::esuccess && ret_rmdir != errno_wasm64_t::enoent) [[unlikely]]
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"path_remove_directory failed");
            ::fast_io::fast_terminate();
        }
    }

    // 11. File open/close
    u8string fname{u8"stress_file_"};
    fname.push_back(static_cast<char8_t>('0' + static_cast<int>(tid % 10u)));

    wasi_void_ptr_wasm64_t const fpath_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0xd00u)};
    wasi_void_ptr_wasm64_t const fd_out_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0xe00u)};

    write_cu8str64(memory, fpath_ptr, fname.c_str());

    auto const ret_open = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                  dirfd,
                                                                                  static_cast<lookupflags_wasm64_t>(0),
                                                                                  fpath_ptr,
                                                                                  static_cast<wasi_size_wasm64_t>(fname.size()),
                                                                                  static_cast<oflags_wasm64_t>(oflags_wasm64_t::o_creat),
                                                                                  rights_wasm64_t::right_fd_write,
                                                                                  static_cast<rights_wasm64_t>(0),
                                                                                  static_cast<fdflags_wasm64_t>(0),
                                                                                  fd_out_ptr);
    if(ret_open != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"path_open failed");
        ::fast_io::fast_terminate();
    }

    auto const opened_fd = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_posix_fd_wasm64_t>(memory, fd_out_ptr);
    auto const ret_close = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, opened_fd);
    if(ret_close != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_close failed");
        ::fast_io::fast_terminate();
    }
}

int main()
{
#if __cpp_lib_atomic_wait >= 201907L
    constexpr unsigned thread_count{16u};  // More threads for stress
    constexpr ::std::size_t base_stride{0x10000u};

    native_memory_t memory{};
    memory.init_by_page_count(thread_count + 8u);

    // Prepare argv/env
    u8string a0{u8"stress"};
    u8string a1{u8"test"};
    vector<u8string_view> args;
    args.push_back(u8string_view{a0.data(), a0.size()});
    args.push_back(u8string_view{a1.data(), a1.size()});

    u8string e0{u8"STRESS=1"};
    u8string e1{u8"TEST=MT"};
    vector<u8string_view> envs;
    envs.push_back(u8string_view{e0.data(), e0.size()});
    envs.push_back(u8string_view{e1.data(), e1.size()});

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = args,
                                            .envs = envs,
                                            .fd_storage = {.fd_limit = 256uz},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false,
                                            .disable_utf8_check = false};

    // Prepare fd table
    env.fd_storage.opens.resize(static_cast<::std::size_t>(4u + thread_count));

    set_dirfd_root(env, 3uz, static_cast<rights_t>(-1), static_cast<rights_t>(-1));

    // Prepare per-thread regular files
    for(unsigned tid = 0; tid < thread_count; ++tid)
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(static_cast<::std::size_t>(4u + tid)).fd_p;
        fde.rights_base = static_cast<rights_wasm64_t>(-1);
        fde.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
# if defined(_WIN32) && !defined(__CYGWIN__)
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file =
            ::fast_io::native_file{u8"wasi_stress.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
# else
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd =
            ::fast_io::native_file{u8"wasi_stress.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
# endif
    }

    std::atomic<bool> failure{false};
    std::atomic<unsigned> completed{0};

    ::std::vector<fast_io::native_thread> threads;
    threads.reserve(thread_count);

    for(unsigned tid = 0; tid < thread_count; ++tid)
    {
        threads.emplace_back(
            [&memory, &env, &failure, &completed, tid]()
            {
                constexpr unsigned iterations{128u};
                for(unsigned i = 0; i < iterations && !failure.load(::std::memory_order_relaxed); ++i)
                {
                    auto const base_off = static_cast<::std::size_t>((tid + 1u) * base_stride);
                    auto const fd = static_cast<wasi_posix_fd_wasm64_t>(4 + tid);
                    auto const dirfd = static_cast<wasi_posix_fd_wasm64_t>(3);

                    try
                    {
                        stress_scenario_comprehensive(memory, env, fd, dirfd, tid, base_off);
                    }
                    catch(...)
                    {
                        failure.store(true, ::std::memory_order_relaxed);
                        return;
                    }
                }
                completed.fetch_add(1u, ::std::memory_order_relaxed);
            });
    }

    for(auto& t: threads) { t.join(); }

    if(failure.load(::std::memory_order_relaxed))
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_mixed_stress: failure detected");
        ::fast_io::fast_terminate();
    }

    auto const total_completed = completed.load(::std::memory_order_relaxed);
    if(total_completed != thread_count)
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_mixed_stress: not all threads completed");
        ::fast_io::fast_terminate();
    }
#endif
}

/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT-5
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
#include <uwvm2/imported/wasi/wasip1/func/fd_pread_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_seek_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_sync_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_filestat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_datasync_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_create_directory_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_filestat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_remove_directory_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_read_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/clock_res_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/clock_time_get_wasm64.h>

using ::uwvm2::imported::wasi::wasip1::abi::clockid_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t;
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

inline static bool check_cu8str64(native_memory_t& memory, wasi_void_ptr_wasm64_t off, u8string_view expect)
{
    for(::std::size_t i{}; i < expect.size(); ++i)
    {
        auto const ch = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<unsigned char>(
            memory,
            static_cast<wasi_void_ptr_wasm64_t>(off + static_cast<wasi_void_ptr_wasm64_t>(i)));
        if(ch != static_cast<unsigned char>(expect[i])) { return false; }
    }
    auto const zero = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<unsigned char>(
        memory,
        static_cast<wasi_void_ptr_wasm64_t>(off + static_cast<wasi_void_ptr_wasm64_t>(expect.size())));
    return zero == 0u;
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

// S0: args_sizes_get_wasm64 -> args_get_wasm64
inline static void wasi_scenario_args(native_memory_t& memory, wasip1_environment<native_memory_t>& env, ::std::size_t base_off)
{
    wasi_void_ptr_wasm64_t const argc_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x10u)};
    wasi_void_ptr_wasm64_t const argv_buf_size_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x20u)};

    auto const ret_sz = ::uwvm2::imported::wasi::wasip1::func::args_sizes_get_wasm64(env, argc_ptr, argv_buf_size_ptr);
    if(ret_sz != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_mt: S0 args_sizes_get_wasm64 failed");
        ::fast_io::fast_terminate();
    }

    auto const got_argc = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, argc_ptr);
    if(got_argc != static_cast<wasi_size_wasm64_t>(env.argv.size())) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_mt: S0 argc mismatch");
        ::fast_io::fast_terminate();
    }

    wasi_void_ptr_wasm64_t const argv_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x100u)};
    wasi_void_ptr_wasm64_t const argv_buf_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x200u)};

    auto const ret_get = ::uwvm2::imported::wasi::wasip1::func::args_get_wasm64(env, argv_ptr, argv_buf_ptr);
    if(ret_get != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_mt: S0 args_get_wasm64 failed");
        ::fast_io::fast_terminate();
    }

    // Only check first two entries if present
    if(env.argv.size() >= 1uz)
    {
        auto const p0 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_void_ptr_wasm64_t>(memory, argv_ptr);
        if(!check_cu8str64(memory, p0, env.argv[0])) [[unlikely]]
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_mt: S0 argv[0] mismatch");
            ::fast_io::fast_terminate();
        }
    }
    if(env.argv.size() >= 2uz)
    {
        auto const p1 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_void_ptr_wasm64_t>(
            memory,
            static_cast<wasi_void_ptr_wasm64_t>(argv_ptr + static_cast<wasi_void_ptr_wasm64_t>(sizeof(wasi_void_ptr_wasm64_t))));
        if(!check_cu8str64(memory, p1, env.argv[1])) [[unlikely]]
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_mt: S0 argv[1] mismatch");
            ::fast_io::fast_terminate();
        }
    }
}

// S1: environ_sizes_get_wasm64 -> environ_get_wasm64
inline static void wasi_scenario_environ(native_memory_t& memory, wasip1_environment<native_memory_t>& env, ::std::size_t base_off)
{
    wasi_void_ptr_wasm64_t const envc_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x400u)};
    wasi_void_ptr_wasm64_t const envbuf_size_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x410u)};

    auto const ret_sz = ::uwvm2::imported::wasi::wasip1::func::environ_sizes_get_wasm64(env, envc_ptr, envbuf_size_ptr);
    if(ret_sz != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const got_envc = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, envc_ptr);
    if(got_envc != static_cast<wasi_size_wasm64_t>(env.envs.size())) [[unlikely]] { ::fast_io::fast_terminate(); }

    wasi_void_ptr_wasm64_t const env_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x500u)};
    wasi_void_ptr_wasm64_t const env_buf_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x600u)};

    auto const ret_get = ::uwvm2::imported::wasi::wasip1::func::environ_get_wasm64(env, env_ptr, env_buf_ptr);
    if(ret_get != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }

    for(::std::size_t i{}; i < env.envs.size(); ++i)
    {
        auto const ptr_off = static_cast<wasi_void_ptr_wasm64_t>(env_ptr + static_cast<wasi_void_ptr_wasm64_t>(i * sizeof(wasi_void_ptr_wasm64_t)));
        auto const p = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_void_ptr_wasm64_t>(memory, ptr_off);
        if(!check_cu8str64(memory, p, env.envs[i])) [[unlikely]] { ::fast_io::fast_terminate(); }
    }
}

// S2: fd_write_wasm64 -> fd_pread_wasm64 -> fd_seek_wasm64 -> fd_sync_wasm64
inline static void wasi_scenario_fd_io(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t fd, ::std::size_t base_off)
{
    constexpr char const msg1[] = "Hello";
    constexpr char const msg2[] = "Hybrid";

    wasi_void_ptr_wasm64_t const buf1{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1000u)};
    wasi_void_ptr_wasm64_t const buf2{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1100u)};
    write_bytes64(memory, buf1, msg1, sizeof(msg1) - 1u);
    write_bytes64(memory, buf2, msg2, sizeof(msg2) - 1u);

    wasi_void_ptr_wasm64_t const iovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1200u)};
    wasi_void_ptr_wasm64_t const nwritten_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1300u)};

    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf1);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(
        memory,
        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + static_cast<wasi_void_ptr_wasm64_t>(8u)),
        static_cast<wasi_size_wasm64_t>(sizeof(msg1) - 1u));
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(
        memory,
        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + static_cast<wasi_void_ptr_wasm64_t>(16u)),
        buf2);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(
        memory,
        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + static_cast<wasi_void_ptr_wasm64_t>(24u)),
        static_cast<wasi_size_wasm64_t>(sizeof(msg2) - 1u));

    auto const ret_w = ::uwvm2::imported::wasi::wasip1::func::fd_write_wasm64(env, fd, iovs_ptr, static_cast<wasi_size_wasm64_t>(2u), nwritten_ptr);
    if(ret_w != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const ret_sync = ::uwvm2::imported::wasi::wasip1::func::fd_sync_wasm64(env, fd);
    if(ret_sync != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }

    // pread from offset 0 to verify contents roughly match pattern length
    wasi_void_ptr_wasm64_t const read_buf{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1400u)};
    wasi_void_ptr_wasm64_t const nread_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1500u)};

    // single iovec for pread: { read_buf, 64 }
    wasi_void_ptr_wasm64_t const riovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1700u)};
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, riovs_ptr, read_buf);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(
        memory,
        static_cast<wasi_void_ptr_wasm64_t>(riovs_ptr + static_cast<wasi_void_ptr_wasm64_t>(8u)),
        static_cast<wasi_size_wasm64_t>(64u));

    auto const ret_r = ::uwvm2::imported::wasi::wasip1::func::fd_pread_wasm64(env,
                                                                              fd,
                                                                              riovs_ptr,
                                                                              static_cast<wasi_size_wasm64_t>(1u),
                                                                              static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t>(0u),
                                                                              nread_ptr);
    if(ret_r != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }

    // seek to end just to exercise fd_seek
    auto const ret_seek = ::uwvm2::imported::wasi::wasip1::func::fd_seek_wasm64(env,
                                                                                fd,
                                                                                ::uwvm2::imported::wasi::wasip1::abi::filedelta_wasm64_t{},
                                                                                ::uwvm2::imported::wasi::wasip1::abi::whence_wasm64_t::whence_end,
                                                                                static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1600u));
    if(ret_seek != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }
}

// S3: fd_filestat_get_wasm64 -> fd_fdstat_get_wasm64 -> fd_datasync_wasm64
inline static void wasi_scenario_fd_stat(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t fd, ::std::size_t base_off)
{
    wasi_void_ptr_wasm64_t const stat_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1800u)};
    auto const ret_fs = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get_wasm64(env, fd, stat_ptr);
    if(ret_fs != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const ft = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<::std::underlying_type_t<filetype_wasm64_t>>(
        memory,
        static_cast<wasi_void_ptr_wasm64_t>(stat_ptr + 16u));
    if(ft != static_cast<::std::underlying_type_t<filetype_wasm64_t>>(filetype_wasm64_t::filetype_regular_file)) [[unlikely]] { ::fast_io::fast_terminate(); }

    wasi_void_ptr_wasm64_t const fdstat_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x1900u)};
    auto const ret_fdstat = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get_wasm64(env, fd, fdstat_ptr);
    if(ret_fdstat != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const ret_dsync = ::uwvm2::imported::wasi::wasip1::func::fd_datasync_wasm64(env, fd);
    if(ret_dsync != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }
}

// S4: path_create_directory_wasm64 -> path_filestat_get_wasm64 -> path_remove_directory_wasm64
inline static void
    wasi_scenario_dir(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t dirfd, unsigned tid, ::std::size_t base_off)
{
    // directory name: "hybrid_mt_dir_<tid>"
    u8string name{u8"hybrid_mt_dir_"};
    name.push_back(static_cast<char8_t>('0' + static_cast<int>(tid % 10u)));

    wasi_void_ptr_wasm64_t const path_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x2000u)};
    write_cu8str64(memory, path_ptr, name.c_str());

    auto const ret_create =
        ::uwvm2::imported::wasi::wasip1::func::path_create_directory_wasm64(env, dirfd, path_ptr, static_cast<wasi_size_wasm64_t>(name.size()));
    if(ret_create != errno_wasm64_t::esuccess && ret_create != errno_wasm64_t::eexist) [[unlikely]] { ::fast_io::fast_terminate(); }

    wasi_void_ptr_wasm64_t const stat_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x2100u)};
    auto const ret_pfs = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get_wasm64(env,
                                                                                         dirfd,
                                                                                         static_cast<lookupflags_wasm64_t>(0),
                                                                                         path_ptr,
                                                                                         static_cast<wasi_size_wasm64_t>(name.size()),
                                                                                         stat_ptr);
    if(ret_pfs != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const ret_rm = ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env, dirfd, path_ptr, static_cast<wasi_size_wasm64_t>(name.size()));
    if(ret_rm != errno_wasm64_t::esuccess && ret_rm != errno_wasm64_t::enoent) [[unlikely]] { ::fast_io::fast_terminate(); }
}

// S5: path_open_wasm64 -> fd_read_wasm64 -> fd_close_wasm64
inline static void
    wasi_scenario_open_read_close(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t dirfd, ::std::size_t base_off)
{
    // Precreated source file
    u8string fname{u8"wasi_hybrid_src.txt"};

    wasi_void_ptr_wasm64_t const path_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x2400u)};
    write_cu8str64(memory, path_ptr, fname.c_str());

    wasi_void_ptr_wasm64_t const fd_out_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x2500u)};

    auto const ret_po = ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(env,
                                                                                dirfd,
                                                                                static_cast<lookupflags_wasm64_t>(0),
                                                                                path_ptr,
                                                                                static_cast<wasi_size_wasm64_t>(fname.size()),
                                                                                static_cast<oflags_wasm64_t>(0),
                                                                                rights_wasm64_t::right_fd_read,
                                                                                static_cast<rights_wasm64_t>(0),
                                                                                static_cast<fdflags_wasm64_t>(0),
                                                                                fd_out_ptr);
    if(ret_po != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const opened_fd = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_posix_fd_wasm64_t>(memory, fd_out_ptr);

    // iovec and backing buffer
    wasi_void_ptr_wasm64_t const iovs_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x2600u)};
    wasi_void_ptr_wasm64_t const read_buf{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x2800u)};
    wasi_void_ptr_wasm64_t const nread_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x2700u)};

    // Build a single iovec pointing to read_buf
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, read_buf);
    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(
        memory,
        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + static_cast<wasi_void_ptr_wasm64_t>(8u)),
        static_cast<wasi_size_wasm64_t>(32u));

    auto const ret_rd = ::uwvm2::imported::wasi::wasip1::func::fd_read_wasm64(env, opened_fd, iovs_ptr, static_cast<wasi_size_wasm64_t>(1u), nread_ptr);
    if(ret_rd != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const ret_close = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, opened_fd);
    if(ret_close != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }
}

// S6: clock_res_get_wasm64 -> clock_time_get_wasm64
inline static void wasi_scenario_clock(native_memory_t& memory, wasip1_environment<native_memory_t>& env, ::std::size_t base_off)
{
    wasi_void_ptr_wasm64_t const res_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x3000u)};
    auto const ret_res = ::uwvm2::imported::wasi::wasip1::func::clock_res_get_wasm64(env, clockid_wasm64_t::clock_monotonic, res_ptr);
    if(ret_res != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const got_ns =
        ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<::std::underlying_type_t<timestamp_wasm64_t>>(memory, res_ptr);
    if(got_ns == 0u) [[unlikely]] { ::fast_io::fast_terminate(); }

    wasi_void_ptr_wasm64_t const time_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x3010u)};

    auto const ret_t1 =
        ::uwvm2::imported::wasi::wasip1::func::clock_time_get_wasm64(env, clockid_wasm64_t::clock_monotonic, static_cast<timestamp_wasm64_t>(0u), time_ptr);
    if(ret_t1 != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const t1 =
        ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<::std::underlying_type_t<timestamp_wasm64_t>>(memory, time_ptr);

    auto const ret_t2 = ::uwvm2::imported::wasi::wasip1::func::clock_time_get_wasm64(env,
                                                                                     clockid_wasm64_t::clock_monotonic,
                                                                                     static_cast<timestamp_wasm64_t>(1'000'000u),
                                                                                     time_ptr);
    if(ret_t2 != errno_wasm64_t::esuccess) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const t2 =
        ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<::std::underlying_type_t<timestamp_wasm64_t>>(memory, time_ptr);
    if(t2 < t1) [[unlikely]] { ::fast_io::fast_terminate(); }
}

int main()
{
#if __cpp_lib_atomic_wait >= 201907L
    constexpr unsigned thread_count{8u};
    constexpr ::std::size_t base_stride{0x10000u};  // one wasm page

    native_memory_t memory{};
    // allocate enough pages so each thread has an independent region
    memory.init_by_page_count(thread_count + 4u);

    // prepare argv/env for scenarios
    u8string a0{u8"hello"};
    u8string a1{u8"世界"};
    vector<u8string_view> args;
    args.push_back(u8string_view{a0.data(), a0.size()});
    args.push_back(u8string_view{a1.data(), a1.size()});

    u8string e0{u8"K=1"};
    u8string e1{u8"V=中文"};
    vector<u8string_view> envs;
    envs.push_back(u8string_view{e0.data(), e0.size()});
    envs.push_back(u8string_view{e1.data(), e1.size()});

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = args,
                                            .envs = envs,
                                            .fd_storage = {.fd_limit = 128uz},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false,
                                            .disable_utf8_check = false};

    // prepare fd table: dirfd at 3, per-thread regular file fds at [4 .. 4+thread_count)
    env.fd_storage.opens.resize(static_cast<::std::size_t>(4u + thread_count + 4u));

    set_dirfd_root(env, 3uz, static_cast<rights_t>(-1), static_cast<rights_t>(-1));

    // prepare per-thread regular files
    for(unsigned tid = 0; tid < thread_count; ++tid)
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(static_cast<::std::size_t>(4u + tid)).fd_p;
        fde.rights_base = static_cast<rights_wasm64_t>(-1);
        fde.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
# if defined(_WIN32) && !defined(__CYGWIN__)
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file =
            ::fast_io::native_file{u8"wasi_hybrid_fd.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
# else
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd =
            ::fast_io::native_file{u8"wasi_hybrid_fd.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
# endif
    }

    // precreate a small source file for path_open scenario
    try
    {
        ::fast_io::native_file f{u8"wasi_hybrid_src.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
        char const data[] = "ABCDEF";
        ::fast_io::operations::write_all_bytes(f, reinterpret_cast<::std::byte const*>(data), reinterpret_cast<::std::byte const*>(data) + (sizeof(data) - 1u));
    }
    catch(::fast_io::error const&)
    {
    }

    std::atomic<bool> failure{false};

    ::std::vector<fast_io::native_thread> threads;
    threads.reserve(thread_count);

    for(unsigned tid = 0; tid < thread_count; ++tid)
    {
        threads.emplace_back(
            [&memory, &env, &failure, tid]()
            {
                constexpr unsigned iterations{512u};
                for(unsigned i = 0; i < iterations && !failure.load(::std::memory_order_relaxed); ++i)
                {
                    auto const base_off = static_cast<::std::size_t>((tid + 1u) * base_stride);
                    auto const scenario = (tid + i) % 7u;

                    // per-thread fd for fd_* scenarios
                    auto const fd = static_cast<wasi_posix_fd_wasm64_t>(4 + tid);

                    try
                    {
                        switch(scenario)
                        {
                            case 0u:
                            {
                                wasi_scenario_args(memory, env, base_off);
                                break;
                            }
                            case 1u:
                            {
                                wasi_scenario_environ(memory, env, base_off);
                                break;
                            }
                            case 2u:
                            {
                                wasi_scenario_fd_io(memory, env, fd, base_off);
                                break;
                            }
                            case 3u:
                            {
                                wasi_scenario_fd_stat(memory, env, fd, base_off);
                                break;
                            }
                            case 4u:
                            {
                                wasi_scenario_dir(memory, env, static_cast<wasi_posix_fd_wasm64_t>(3), tid, base_off);
                                break;
                            }
                            case 5u:
                            {
                                wasi_scenario_open_read_close(memory, env, static_cast<wasi_posix_fd_wasm64_t>(3), base_off);
                                break;
                            }
                            case 6u:
                            {
                                wasi_scenario_clock(memory, env, base_off);
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
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_mt: failure detected in worker thread");
        ::fast_io::fast_terminate();
    }
#endif
}

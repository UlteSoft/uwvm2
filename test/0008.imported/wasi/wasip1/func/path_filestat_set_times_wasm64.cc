/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT-5
 * @version     2.0.0
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/path_filestat_set_times_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/path_filestat_get_wasm64.h>

using ::uwvm2::imported::wasi::wasip1::abi::fstflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;

inline static void write_path64(native_memory_t& memory, wasi_void_ptr_wasm64_t p, char8_t const* s, ::std::size_t n)
{
    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64(memory,
                                                                        p,
                                                                        reinterpret_cast<::std::byte const*>(s),
                                                                        reinterpret_cast<::std::byte const*>(s) + n);
}

int main()
{
    native_memory_t memory{};
    memory.init_by_page_count(4uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    // Prepare FD table
    env.fd_storage.opens.resize(24uz);

    // Helpers
    auto set_dirfd = [&](::std::size_t idx, rights_wasm64_t base_rights)
    {
        auto& fd = *env.fd_storage.opens.index_unchecked(idx).fd_p;
        fd.rights_base = base_rights;
        fd.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
        auto& ds = fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
        ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
        entry.ptr->dir_stack.storage.file = ::fast_io::dir_file{u8"."};
        ds.dir_stack.push_back(::std::move(entry));
    };

    auto set_filefd = [&](::std::size_t idx, rights_wasm64_t base_rights, ::uwvm2::utils::container::u8cstring_view name)
    {
        auto& fd = *env.fd_storage.opens.index_unchecked(idx).fd_p;
        fd.rights_base = base_rights;
        fd.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
#if defined(_WIN32) && !defined(__CYGWIN__)
        fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file =
            ::fast_io::native_file{name, ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
#else
        fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd =
            ::fast_io::native_file{name, ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
#endif
    };

    auto call_set_times64 = [&](wasi_posix_fd_wasm64_t fd,
                                lookupflags_wasm64_t flags,
                                wasi_void_ptr_wasm64_t p,
                                wasi_size_wasm64_t n,
                                timestamp_wasm64_t atim,
                                timestamp_wasm64_t mtim,
                                fstflags_wasm64_t ff)
    { return ::uwvm2::imported::wasi::wasip1::func::path_filestat_set_times_wasm64(env, fd, flags, p, n, atim, mtim, ff); };

    auto write_string = [&](wasi_void_ptr_wasm64_t p, char8_t const* s) { write_path64(memory, p, s, ::std::char_traits<char8_t>::length(s)); };

    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P0{1024u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P1{4096u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P2{8192u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P3{12288u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P4{16384u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P5{20480u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P6{24576u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P7{28672u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P8{32768u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P9{36864u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P10{40960u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P11{45056u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P12{49152u};
    [[maybe_unused]] constexpr wasi_void_ptr_wasm64_t P13{53248u};

    // ===== Case 0: negative fd -> ebadf =====
    {
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(-1),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          static_cast<wasi_void_ptr_wasm64_t>(0u),
                                          static_cast<wasi_size_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<fstflags_wasm64_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln("pfst64 Case 0 expected ebadf ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 1: rights missing -> enotcapable =====
    {
        set_dirfd(3uz, static_cast<rights_wasm64_t>(0));
        write_string(P0, u8"pfst64_no_rights");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(3),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P0,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_no_rights") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<fstflags_wasm64_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln("pfst64 Case 1 expected enotcapable ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 2: fd is file -> enotdir =====
    {
        set_filefd(4uz, static_cast<rights_wasm64_t>(-1), u8"pfst64_file.tmp");
        write_string(P1, u8"x");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(4),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P1,
                                          static_cast<wasi_size_wasm64_t>(1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<fstflags_wasm64_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir)
        {
            ::fast_io::io::perrln("pfst64 Case 2 expected enotdir ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    set_dirfd(5uz, static_cast<rights_wasm64_t>(-1));

    // ===== Case 3: absolute path -> enotcapable =====
    {
        write_string(P2, u8"/abs64");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P2,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"/abs64") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<fstflags_wasm64_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln("pfst64 Case 3 expected enotcapable ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 4: empty path -> einval =====
    {
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P3,
                                          static_cast<wasi_size_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<fstflags_wasm64_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("pfst64 Case 4 expected einval ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 5: '.' -> esuccess (or enosys on Win9x) =====
    {
        write_string(P4, u8".");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P4,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8".") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          fstflags_wasm64_t::filestat_set_atim_now);
        if(!(ret == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
             || ret == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enosys
#endif
#if defined(__MSDOS__) || defined(__DJGPP__)
             || ret == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enosys
#endif
             ))
        {
            ::fast_io::io::perrln("pfst64 Case 5 expected esuccess (or enosys) ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 6: '..' final -> enotcapable =====
    {
        write_string(P5, u8"..");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P5,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"..") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          fstflags_wasm64_t::filestat_set_mtim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln("pfst64 Case 6 expected enotcapable ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Prepare a/b
    try
    {
        ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pfst64_a");
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pfst64_a/b");
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_file f{u8"pfst64_mid_file", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 7: 'a/b/..' -> esuccess (or enosys) =====
    {
        write_string(P6, u8"pfst64_a/b/..");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P6,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_a/b/..") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          fstflags_wasm64_t::filestat_set_atim_now);
        if(!(ret == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
             || ret == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enosys
#endif
#if defined(__MSDOS__) || defined(__DJGPP__)
             || ret == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enosys
#endif
             ))
        {
            ::fast_io::io::perrln("pfst64 Case 7 expected esuccess (or enosys) ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 8: 'a/.' -> esuccess (or enosys) =====
    {
        write_string(P7, u8"pfst64_a/.");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P7,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_a/.") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          fstflags_wasm64_t::filestat_set_mtim_now);
        if(!(ret == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
             || ret == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enosys
#endif
#if defined(__MSDOS__) || defined(__DJGPP__)
             || ret == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enosys
#endif
             ))
        {
            ::fast_io::io::perrln("pfst64 Case 8 expected esuccess (or enosys) ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 9: non-existent final -> enoent =====
    {
        write_string(P8, u8"pfst64_no_such");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P8,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_no_such") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          fstflags_wasm64_t::filestat_set_atim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enoent
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
           && ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval
#endif
#if defined(__MSDOS__) || defined(__DJGPP__)
           && ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval
#endif
        )
        {
            ::fast_io::io::perrln("pfst64 Case 9 expected enoent ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 10: intermediate is file -> enotdir =====
    {
        write_string(P9, u8"pfst64_mid_file/x");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P9,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_mid_file/x") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          fstflags_wasm64_t::filestat_set_mtim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir)
        {
            ::fast_io::io::perrln("pfst64 Case 10 expected enotdir ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x0600) || defined(__MSDOS__) || defined(__DJGPP__))
    // Symlink-capable
    try
    {
        ::fast_io::native_file f{u8"pfst64_tA", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_symlinkat(u8"pfst64_tA", ::fast_io::at_fdcwd(), u8"pfst64_lA");
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 11: final symlink nofollow -> esuccess =====
    {
        write_string(P10, u8"pfst64_lA");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P10,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_lA") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          fstflags_wasm64_t::filestat_set_mtim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("pfst64 Case 11 expected esuccess ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 12: final symlink follow -> esuccess =====
    {
        write_string(P11, u8"pfst64_lA");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          lookupflags_wasm64_t::lookup_symlink_follow,
                                          P11,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_lA") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          fstflags_wasm64_t::filestat_set_atim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("pfst64 Case 12 expected esuccess ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    try
    {
        ::fast_io::native_file f{u8"pfst64_tC", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_symlinkat(u8"pfst64_tC", ::fast_io::at_fdcwd(), u8"pfst64_lC2");
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_symlinkat(u8"pfst64_lC2", ::fast_io::at_fdcwd(), u8"pfst64_lC1");
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 13: chain symlink follow -> esuccess =====
    {
        write_string(P12, u8"pfst64_lC1");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          lookupflags_wasm64_t::lookup_symlink_follow,
                                          P12,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_lC1") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          fstflags_wasm64_t::filestat_set_mtim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("pfst64 Case 13 expected esuccess ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    try
    {
        ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pfst64_realD");
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_file f{u8"pfst64_realD/f", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_symlinkat(u8"pfst64_realD", ::fast_io::at_fdcwd(), u8"pfst64_linkD");
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 14: intermediate symlink to dir -> esuccess =====
    {
        write_string(P13, u8"pfst64_linkD/f");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P13,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_linkD/f") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          fstflags_wasm64_t::filestat_set_atim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("pfst64 Case 14 expected esuccess ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Self-loop link
    try
    {
        ::fast_io::native_symlinkat(u8"pfst64_loop", ::fast_io::at_fdcwd(), u8"pfst64_loop");
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 15a: final self-loop follow -> eloop =====
    {
        write_string(P10, u8"pfst64_loop");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          lookupflags_wasm64_t::lookup_symlink_follow,
                                          P10,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_loop") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          fstflags_wasm64_t::filestat_set_mtim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eloop)
        {
            ::fast_io::io::perrln("pfst64 Case 15a expected eloop ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 15b: final self-loop nofollow -> esuccess =====
    {
        write_string(P11, u8"pfst64_loop");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P11,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_loop") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          fstflags_wasm64_t::filestat_set_atim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("pfst64 Case 15b expected esuccess ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Intermediate loop
    try
    {
        ::fast_io::native_symlinkat(u8"pfst64_loop2", ::fast_io::at_fdcwd(), u8"pfst64_loop2");
    }
    catch(::fast_io::error)
    {
    }
    {
        write_string(P12, u8"pfst64_loop2/x");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P12,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_loop2/x") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          fstflags_wasm64_t::filestat_set_mtim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eloop)
        {
            ::fast_io::io::perrln("pfst64 Case 16 expected eloop ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Abs symlink
    try
    {
        ::fast_io::native_symlinkat(u8"/etc", ::fast_io::at_fdcwd(), u8"pfst64_abs");
    }
    catch(::fast_io::error)
    {
    }
    {
        write_string(P13, u8"pfst64_abs");
        auto const r1 = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                         lookupflags_wasm64_t::lookup_symlink_follow,
                                         P13,
                                         static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_abs") - 1u),
                                         static_cast<timestamp_wasm64_t>(0u),
                                         static_cast<timestamp_wasm64_t>(0u),
                                         fstflags_wasm64_t::filestat_set_atim_now);
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln("pfst64 Case 17a expected enotcapable ", static_cast<unsigned>(r1));
            ::fast_io::fast_terminate();
        }
        auto const r2 = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                         static_cast<lookupflags_wasm64_t>(0),
                                         P13,
                                         static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_abs") - 1u),
                                         static_cast<timestamp_wasm64_t>(0u),
                                         static_cast<timestamp_wasm64_t>(0u),
                                         fstflags_wasm64_t::filestat_set_mtim_now);
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("pfst64 Case 17b expected esuccess ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }
    }
#endif

    // ===== Case 18: embedded NUL -> eilseq =====
    {
        constexpr wasi_void_ptr_wasm64_t Pn{60000u};
        char const raw[] = {'a', 'a', '\0', 'b', 'b'};
        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64(memory,
                                                                            Pn,
                                                                            reinterpret_cast<::std::byte const*>(raw),
                                                                            reinterpret_cast<::std::byte const*>(raw) + sizeof(raw));
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          Pn,
                                          static_cast<wasi_size_wasm64_t>(sizeof(raw)),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<fstflags_wasm64_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eilseq)
        {
            ::fast_io::io::perrln("pfst64 Case 18 expected eilseq ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

#if (defined(_WIN32) || defined(__CYGWIN__)) || (defined(__MSDOS__) || defined(__DJGPP__))
    // ===== Case 19: Windows/DOS invalid characters -> einval =====
    {
        write_string(P0, u8"pfst64_inv_bs_a\\b");
        auto const ret = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                          static_cast<lookupflags_wasm64_t>(0),
                                          P0,
                                          static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_inv_bs_a\\b") - 1u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<timestamp_wasm64_t>(0u),
                                          static_cast<fstflags_wasm64_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("pfst64 inv bs expected einval");
            ::fast_io::fast_terminate();
        }

        write_string(P1, u8"\\");
        auto const r1 = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                         static_cast<lookupflags_wasm64_t>(0),
                                         P1,
                                         static_cast<wasi_size_wasm64_t>(sizeof(u8"\\") - 1u),
                                         static_cast<timestamp_wasm64_t>(0u),
                                         static_cast<timestamp_wasm64_t>(0u),
                                         static_cast<fstflags_wasm64_t>(0));
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("pfst64 sole bs expected einval");
            ::fast_io::fast_terminate();
        }
    }
#endif

    // ===== Case 20: both *_now and *_tim set -> einval =====
    {
        write_string(P2, u8"pfst64_a");
        auto const ret =
            call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                             static_cast<lookupflags_wasm64_t>(0),
                             P2,
                             static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_a") - 1u),
                             static_cast<timestamp_wasm64_t>(1234567890ull),
                             static_cast<timestamp_wasm64_t>(0u),
                             static_cast<fstflags_wasm64_t>(static_cast<::std::underlying_type_t<fstflags_wasm64_t>>(fstflags_wasm64_t::filestat_set_atim_now) |
                                                            static_cast<::std::underlying_type_t<fstflags_wasm64_t>>(fstflags_wasm64_t::filestat_set_atim)));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("pfst64 Case 20 expected einval ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 21: only mtim set; only atim set -> esuccess (or enosys) =====
    {
        write_string(P3, u8"pfst64_a");
        auto const r1 = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                         static_cast<lookupflags_wasm64_t>(0),
                                         P3,
                                         static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_a") - 1u),
                                         static_cast<timestamp_wasm64_t>(0u),
                                         static_cast<timestamp_wasm64_t>(9876543210ull),
                                         fstflags_wasm64_t::filestat_set_mtim);
        if(!(r1 == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
             || r1 == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces
             || r1 == ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval
#endif
#if defined(__MSDOS__) || defined(__DJGPP__)
             || r1 == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval
#endif
             ))
        {
            ::fast_io::io::perrln("pfst64 Case 21a expected esuccess (or eacces) ", static_cast<unsigned>(r1));
            ::fast_io::fast_terminate();
        }
#if !(defined(__MSDOS__) || defined(__DJGPP__))
        auto const r2 = call_set_times64(static_cast<wasi_posix_fd_wasm64_t>(5),
                                         static_cast<lookupflags_wasm64_t>(0),
                                         P3,
                                         static_cast<wasi_size_wasm64_t>(sizeof(u8"pfst64_a") - 1u),
                                         static_cast<timestamp_wasm64_t>(1111111111ull),
                                         static_cast<timestamp_wasm64_t>(0u),
                                         fstflags_wasm64_t::filestat_set_atim);
        if(!(r2 == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess
# if defined(_WIN32) && defined(_WIN32_WINDOWS)
             || r2 == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces
             || r2 == ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval
# endif
             ))
        {
            ::fast_io::io::perrln("pfst64 Case 21b expected esuccess (or eacces) ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }
#endif
    }

    // Cleanup best-effort
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_mid_file", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_tA", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_lA", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_tC", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_lC1", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_lC2", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_linkD", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_realD/f", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_realD", ::fast_io::native_at_flags::removedir);
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_loop", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_loop2", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_abs", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_a/b", ::fast_io::native_at_flags::removedir);
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_a", ::fast_io::native_at_flags::removedir);
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst64_file.tmp", {});
    }
    catch(::fast_io::error)
    {
    }

    return 0;
}


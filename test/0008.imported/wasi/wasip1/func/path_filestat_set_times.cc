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

#include <uwvm2/imported/wasi/wasip1/func/path_filestat_set_times.h>
#include <uwvm2/imported/wasi/wasip1/func/path_filestat_get.h>

using ::uwvm2::imported::wasi::wasip1::abi::fstflags_t;
using ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
using ::uwvm2::imported::wasi::wasip1::abi::timestamp_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;

inline static void write_path32(native_memory_t& memory, wasi_void_ptr_t p, char8_t const* s, ::std::size_t n)
{
    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
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

    // Common helper: set a dir fd with CWD '.' at index idx
    auto set_dirfd = [&](::std::size_t idx, rights_t base_rights)
    {
        auto& fd = *env.fd_storage.opens.index_unchecked(idx).fd_p;
        fd.rights_base = base_rights;
        fd.rights_inherit = static_cast<rights_t>(-1);
        fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
        auto& ds = fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
        ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
        entry.ptr->dir_stack.file = ::fast_io::dir_file{u8"."};
        ds.dir_stack.push_back(::std::move(entry));
    };

    // Common helper: set a file fd at index idx
    auto set_filefd = [&](::std::size_t idx, rights_t base_rights, ::uwvm2::utils::container::u8cstring_view name)
    {
        auto& fd = *env.fd_storage.opens.index_unchecked(idx).fd_p;
        fd.rights_base = base_rights;
        fd.rights_inherit = static_cast<rights_t>(-1);
        fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
#if defined(_WIN32) && !defined(__CYGWIN__)
        fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file =
            ::fast_io::native_file{name, ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
#else
        fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd =
            ::fast_io::native_file{name, ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
#endif
    };

    // Small helpers
    auto call_set_times = [&](wasi_posix_fd_t fd, lookupflags_t flags, wasi_void_ptr_t p, wasi_size_t n, timestamp_t atim, timestamp_t mtim, fstflags_t ff)
    { return ::uwvm2::imported::wasi::wasip1::func::path_filestat_set_times(env, fd, flags, p, n, atim, mtim, ff); };

    auto write_string = [&](wasi_void_ptr_t p, char8_t const* s) { write_path32(memory, p, s, ::std::char_traits<char8_t>::length(s)); };

    [[maybe_unused]] constexpr wasi_void_ptr_t P0{1024u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P1{4096u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P2{8192u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P3{12288u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P4{16384u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P5{20480u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P6{24576u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P7{28672u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P8{32768u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P9{36864u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P10{40960u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P11{45056u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P12{49152u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P13{53248u};

    // ===== Case 0: negative fd -> ebadf =====
    {
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(-1),
                                        static_cast<lookupflags_t>(0),
                                        static_cast<wasi_void_ptr_t>(0u),
                                        static_cast<wasi_size_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<fstflags_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf)
        {
            ::fast_io::io::perrln("pfst32 Case 0 expected ebadf ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 1: rights missing -> enotcapable =====
    {
        set_dirfd(3uz, static_cast<rights_t>(0));
        write_string(P0, u8"pfst32_no_rights");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(3),
                                        static_cast<lookupflags_t>(0),
                                        P0,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_no_rights") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<fstflags_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable)
        {
            ::fast_io::io::perrln("pfst32 Case 1 expected enotcapable ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 2: fd is file -> enotdir =====
    {
        set_filefd(4uz, static_cast<rights_t>(-1), u8"pfst32_file.tmp");
        write_string(P1, u8"x");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(4),
                                        static_cast<lookupflags_t>(0),
                                        P1,
                                        static_cast<wasi_size_t>(1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<fstflags_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir)
        {
            ::fast_io::io::perrln("pfst32 Case 2 expected enotdir ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Prepare a directory fd with full rights at index 5 =====
    set_dirfd(5uz, static_cast<rights_t>(-1));

    // ===== Case 3: absolute path -> eperm =====
    {
        write_string(P2, u8"/abs32");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P2,
                                        static_cast<wasi_size_t>(sizeof(u8"/abs32") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<fstflags_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm)
        {
            ::fast_io::io::perrln("pfst32 Case 3 expected eperm ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 4: empty path -> einval =====
    {
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P3,
                                        static_cast<wasi_size_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<fstflags_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("pfst32 Case 4 expected einval ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 5: '.' -> esuccess (set current dir time) =====
    {
        write_string(P4, u8".");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P4,
                                        static_cast<wasi_size_t>(sizeof(u8".") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_atim_now);
        if(!(ret == ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
             || ret == ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys
#endif
#if defined(__MSDOS__) || defined(__DJGPP__)
             || ret == ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys
#endif
             ))
        {
            ::fast_io::io::perrln("pfst32 Case 5 expected esuccess (or enosys on Win9x) ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 6: '..' final -> eperm =====
    {
        write_string(P5, u8"..");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P5,
                                        static_cast<wasi_size_t>(sizeof(u8"..") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_mtim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm)
        {
            ::fast_io::io::perrln("pfst32 Case 6 expected eperm ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Prepare directories/files used by following tests =====
    // a/b path
    try
    {
        ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pfst32_a");
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pfst32_a/b");
    }
    catch(::fast_io::error)
    {
    }

    // mid file
    try
    {
        ::fast_io::native_file f{u8"pfst32_mid_file", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 7: 'a/b/..' -> esuccess (set 'a') =====
    {
        write_string(P6, u8"pfst32_a/b/..");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P6,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_a/b/..") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_atim_now);
        if(!(ret == ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
             || ret == ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys
#endif
#if defined(__MSDOS__) || defined(__DJGPP__)
             || ret == ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys
#endif
             ))
        {
            ::fast_io::io::perrln("pfst32 Case 7 expected esuccess (or enosys on Win9x) ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 8: 'a/.' -> esuccess =====
    {
        write_string(P7, u8"pfst32_a/.");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P7,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_a/.") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_mtim_now);
        if(!(ret == ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
             || ret == ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys
#endif
#if defined(__MSDOS__) || defined(__DJGPP__)
             || ret == ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys
#endif
             ))
        {
            ::fast_io::io::perrln("pfst32 Case 8 expected esuccess (or enosys on Win9x) ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 9: non-existent final -> enoent =====
    {
        write_string(P8, u8"pfst32_no_such");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P8,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_no_such") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_atim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
           && ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval
#endif
#if defined(__MSDOS__) || defined(__DJGPP__)
           && ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval
#endif
        )
        {
            ::fast_io::io::perrln("pfst32 Case 9 expected enoent ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 10: intermediate is file -> enotdir =====
    {
        write_string(P9, u8"pfst32_mid_file/x");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P9,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_mid_file/x") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_mtim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir)
        {
            ::fast_io::io::perrln("pfst32 Case 10 expected enotdir ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x0600) || defined(__MSDOS__) || defined(__DJGPP__))
    // ===== Symlink-capable platforms =====
    // Prepare: file pfst32_tA and symlink pfst32_lA -> pfst32_tA
    try
    {
        ::fast_io::native_file f{u8"pfst32_tA", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_symlinkat(u8"pfst32_tA", ::fast_io::at_fdcwd(), u8"pfst32_lA");
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 11: final symlink nofollow -> esuccess =====
    {
        write_string(P10, u8"pfst32_lA");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P10,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_lA") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_mtim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("pfst32 Case 11 expected esuccess ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 12: final symlink follow -> esuccess =====
    {
        write_string(P11, u8"pfst32_lA");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        lookupflags_t::lookup_symlink_follow,
                                        P11,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_lA") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_atim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("pfst32 Case 12 expected esuccess ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Prepare chain: lC1 -> lC2 -> tC
    try
    {
        ::fast_io::native_file f{u8"pfst32_tC", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_symlinkat(u8"pfst32_tC", ::fast_io::at_fdcwd(), u8"pfst32_lC2");
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_symlinkat(u8"pfst32_lC2", ::fast_io::at_fdcwd(), u8"pfst32_lC1");
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 13: chain symlink follow -> esuccess =====
    {
        write_string(P12, u8"pfst32_lC1");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        lookupflags_t::lookup_symlink_follow,
                                        P12,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_lC1") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_mtim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("pfst32 Case 13 expected esuccess ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Prepare: real directory and link to it
    try
    {
        ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pfst32_realD");
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_file f{u8"pfst32_realD/f", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_symlinkat(u8"pfst32_realD", ::fast_io::at_fdcwd(), u8"pfst32_linkD");
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 14: intermediate symlink to dir -> esuccess on file under it =====
    {
        write_string(P13, u8"pfst32_linkD/f");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P13,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_linkD/f") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_atim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("pfst32 Case 14 expected esuccess ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Self-loop link
    try
    {
        ::fast_io::native_symlinkat(u8"pfst32_loop", ::fast_io::at_fdcwd(), u8"pfst32_loop");
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 15a: final self-loop symlink follow -> eloop =====
    {
        write_string(P10, u8"pfst32_loop");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        lookupflags_t::lookup_symlink_follow,
                                        P10,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_loop") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_mtim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop)
        {
            ::fast_io::io::perrln("pfst32 Case 15a expected eloop ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 15b: final self-loop symlink nofollow -> esuccess (set link itself) =====
    {
        write_string(P11, u8"pfst32_loop");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P11,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_loop") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_atim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("pfst32 Case 15b expected esuccess ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Intermediate loop link
    try
    {
        ::fast_io::native_symlinkat(u8"pfst32_loop2", ::fast_io::at_fdcwd(), u8"pfst32_loop2");
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 16: intermediate loop symlink -> eloop =====
    {
        write_string(P12, u8"pfst32_loop2/x");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P12,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_loop2/x") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_mtim_now);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop)
        {
            ::fast_io::io::perrln("pfst32 Case 16 expected eloop ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 17: symlink to absolute path; follow -> eperm; nofollow -> esuccess =====
    try
    {
        ::fast_io::native_symlinkat(u8"/etc", ::fast_io::at_fdcwd(), u8"pfst32_abs");
    }
    catch(::fast_io::error)
    {
    }
    {
        write_string(P13, u8"pfst32_abs");
        auto const r1 = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                       lookupflags_t::lookup_symlink_follow,
                                       P13,
                                       static_cast<wasi_size_t>(sizeof(u8"pfst32_abs") - 1u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<timestamp_t>(0u),
                                       fstflags_t::filestat_set_atim_now);
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm)
        {
            ::fast_io::io::perrln("pfst32 Case 17a expected eperm ", static_cast<unsigned>(r1));
            ::fast_io::fast_terminate();
        }

        auto const r2 = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                       static_cast<lookupflags_t>(0),
                                       P13,
                                       static_cast<wasi_size_t>(sizeof(u8"pfst32_abs") - 1u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<timestamp_t>(0u),
                                       fstflags_t::filestat_set_mtim_now);
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("pfst32 Case 17b expected esuccess ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }
    }

#endif  // symlink-capable

    // ===== Case 18: embedded NUL in path -> eilseq =====
    {
        // write bytes "aa\0bb"
        constexpr wasi_void_ptr_t Pn{60000u};
        char const raw[] = {'a', 'a', '\0', 'b', 'b'};
        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
                                                                            Pn,
                                                                            reinterpret_cast<::std::byte const*>(raw),
                                                                            reinterpret_cast<::std::byte const*>(raw) + sizeof(raw));

        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        Pn,
                                        static_cast<wasi_size_t>(sizeof(raw)),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<fstflags_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eilseq)
        {
            ::fast_io::io::perrln("pfst32 Case 18 expected eilseq ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

#if (defined(_WIN32) || defined(__CYGWIN__)) || (defined(__MSDOS__) || defined(__DJGPP__))
    // ===== Case 19: Windows/DOS invalid characters -> einval =====
    {
        write_string(P0, u8"pfst32_inv_bs_a\\b");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P0,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_inv_bs_a\\b") - 1u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<fstflags_t>(0));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("pfst32 inv bs expected einval");
            ::fast_io::fast_terminate();
        }

        write_string(P1, u8"\\");
        auto const r1 = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                       static_cast<lookupflags_t>(0),
                                       P1,
                                       static_cast<wasi_size_t>(sizeof(u8"\\") - 1u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<fstflags_t>(0));
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("pfst32 sole bs expected einval");
            ::fast_io::fast_terminate();
        }

        write_string(P2, u8"pfst32_star_*");
        auto const r2 = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                       static_cast<lookupflags_t>(0),
                                       P2,
                                       static_cast<wasi_size_t>(sizeof(u8"pfst32_star_*") - 1u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<fstflags_t>(0));
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("pfst32 '*' expected einval");
            ::fast_io::fast_terminate();
        }

        write_string(P3, u8"pfst32_qmark_?");
        auto const r3 = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                       static_cast<lookupflags_t>(0),
                                       P3,
                                       static_cast<wasi_size_t>(sizeof(u8"pfst32_qmark_?") - 1u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<fstflags_t>(0));
        if(r3 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("pfst32 '?' expected einval");
            ::fast_io::fast_terminate();
        }

        write_string(P4, u8"pfst32_pipe_|");
        auto const r4 = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                       static_cast<lookupflags_t>(0),
                                       P4,
                                       static_cast<wasi_size_t>(sizeof(u8"pfst32_pipe_|") - 1u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<fstflags_t>(0));
        if(r4 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("pfst32 '|' expected einval");
            ::fast_io::fast_terminate();
        }

        write_string(P5, u8"pfst32_lt_<");
        auto const r5 = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                       static_cast<lookupflags_t>(0),
                                       P5,
                                       static_cast<wasi_size_t>(sizeof(u8"pfst32_lt_<") - 1u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<fstflags_t>(0));
        if(r5 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("pfst32 '<' expected einval");
            ::fast_io::fast_terminate();
        }

        write_string(P6, u8"pfst32_gt_>");
        auto const r6 = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                       static_cast<lookupflags_t>(0),
                                       P6,
                                       static_cast<wasi_size_t>(sizeof(u8"pfst32_gt_>") - 1u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<fstflags_t>(0));
        if(r6 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("pfst32 '>' expected einval");
            ::fast_io::fast_terminate();
        }

        write_string(P7, u8"pfst32_quote_\"");
        auto const r7 = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                       static_cast<lookupflags_t>(0),
                                       P7,
                                       static_cast<wasi_size_t>(sizeof(u8"pfst32_quote_\"") - 1u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<fstflags_t>(0));
        if(r7 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("pfst32 '" " expected einval");
            ::fast_io::fast_terminate();
        }

        write_string(P8, u8"pfst32_colon_a:b");
        auto const r8 = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                       static_cast<lookupflags_t>(0),
                                       P8,
                                       static_cast<wasi_size_t>(sizeof(u8"pfst32_colon_a:b") - 1u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<fstflags_t>(0));
        if(r8 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("pfst32 ':' expected einval");
            ::fast_io::fast_terminate();
        }

        write_string(P9, u8"C:pfst32_drive_rel");
        auto const r9 = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                       static_cast<lookupflags_t>(0),
                                       P9,
                                       static_cast<wasi_size_t>(sizeof(u8"C:pfst32_drive_rel") - 1u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<fstflags_t>(0));
        if(r9 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("pfst32 'C:' expected einval");
            ::fast_io::fast_terminate();
        }
    }
#endif

    // ===== Case 20: both *_now and *_tim set -> einval =====
    {
        write_string(P0, u8"pfst32_a");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P0,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_a") - 1u),
                                        static_cast<timestamp_t>(1234567890ull),
                                        static_cast<timestamp_t>(0u),
                                        static_cast<fstflags_t>(static_cast<::std::underlying_type_t<fstflags_t>>(fstflags_t::filestat_set_atim_now) |
                                                                static_cast<::std::underlying_type_t<fstflags_t>>(fstflags_t::filestat_set_atim)));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("pfst32 Case 20 expected einval ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 21: only mtim set -> esuccess; only atim set -> esuccess =====
    {
        write_string(P1, u8"pfst32_a");
        auto const r1 = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                       static_cast<lookupflags_t>(0),
                                       P1,
                                       static_cast<wasi_size_t>(sizeof(u8"pfst32_a") - 1u),
                                       static_cast<timestamp_t>(0u),
                                       static_cast<timestamp_t>(9876543210ull),
                                       fstflags_t::filestat_set_mtim);
        if(!(r1 == ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
             || r1 == ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces
             || r1 == ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval
#endif
#if defined(__MSDOS__) || defined(__DJGPP__)
             || r1 == ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval
#endif
             ))
        {
            ::fast_io::io::perrln("pfst32 Case 21a expected esuccess (or eacces) ", static_cast<unsigned>(r1));
            ::fast_io::fast_terminate();
        }
#if !(defined(__MSDOS__) || defined(__DJGPP__))

        auto const r2 = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                       static_cast<lookupflags_t>(0),
                                       P1,
                                       static_cast<wasi_size_t>(sizeof(u8"pfst32_a") - 1u),
                                       static_cast<timestamp_t>(1111111111ull),
                                       static_cast<timestamp_t>(0u),
                                       fstflags_t::filestat_set_atim);
        if(!(r2 == ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess
# if defined(_WIN32) && defined(_WIN32_WINDOWS)
             || r2 == ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces
             || r1 == ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval
# endif
             ))
        {
            ::fast_io::io::perrln("pfst32 Case 21b expected esuccess (or eacces) ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }
#endif
    }

#if defined(__linux__) && !(defined(__MSDOS__) || defined(__DJGPP__))
    // ===== Case 22: 32-bit time_t overflow -> eoverflow =====
# if (UINTPTR_MAX <= 0xFFFFFFFFu)
    {
        // seconds greater than time_t max (assume 32-bit time_t)
        unsigned long long const big_sec = 0xFFFFFFFFull + 10ull;
        unsigned long long const big_ns = big_sec * 1000000000ull;

        write_string(P2, u8"pfst32_a");
        auto const ret = call_set_times(static_cast<wasi_posix_fd_t>(5),
                                        static_cast<lookupflags_t>(0),
                                        P2,
                                        static_cast<wasi_size_t>(sizeof(u8"pfst32_a") - 1u),
                                        static_cast<timestamp_t>(big_ns),
                                        static_cast<timestamp_t>(0u),
                                        fstflags_t::filestat_set_atim);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow)
        {
            ::fast_io::io::perrln("pfst32 Case 22 expected eoverflow on 32-bit Linux ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
# endif
#endif

    // Cleanup best-effort
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_mid_file", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_tA", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_lA", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_tC", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_lC1", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_lC2", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_linkD", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_realD/f", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_realD", ::fast_io::native_at_flags::removedir);
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_loop", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_loop2", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_abs", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_a/b", ::fast_io::native_at_flags::removedir);
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_a", ::fast_io::native_at_flags::removedir);
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfst32_file.tmp", {});
    }
    catch(::fast_io::error)
    {
    }

    return 0;
}


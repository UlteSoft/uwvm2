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
#include <cstring>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/path_filestat_get.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_filestat_get.h>
#ifdef UWVM_DLLIMPORT
# error "UWVM_DLLIMPORT existed"
#endif

#ifdef UWVM_WASM_SUPPORT_WASM1
# error "UWVM_WASM_SUPPORT_WASM1 existed"
#endif

#ifdef UWVM_AES_RST_ALL
# error "UWVM_AES_RST_ALL existed"
#endif

#ifdef UWVM_COLOR_RST_ALL
# error "UWVM_COLOR_RST_ALL existed"
#endif

#ifdef UWVM_WIN32_TEXTATTR_RST_ALL
# error "UWVM_WIN32_TEXTATTR_RST_ALL existed"
#endif

#ifdef UWVM_IMPORT_WASI
# error "UWVM_IMPORT_WASI existed"
#endif

#ifdef UWVM_IMPORT_WASI_WASIP1
# error "UWVM_IMPORT_WASI_WASIP1 existed"
#endif
using ::uwvm2::imported::wasi::wasip1::abi::filetype_t;
using ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;

inline static filetype_t read_filetype(native_memory_t& memory, wasi_void_ptr_t stat_ptr)
{
    using u8_t = ::std::underlying_type_t<filetype_t>;
    auto const ft_u8 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<u8_t>(memory, stat_ptr + 16u);
    return static_cast<filetype_t>(ft_u8);
}

[[maybe_unused]] inline static ::uwvm2::imported::wasi::wasip1::abi::filesize_t read_size(native_memory_t& memory, wasi_void_ptr_t stat_ptr)
{
    using sz_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_t>;
    auto const sz_u = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<sz_t>(memory, stat_ptr + 32u);
    return static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(sz_u);
}

inline static void write_path(native_memory_t& memory, wasi_void_ptr_t p, char8_t const* s, ::std::size_t n)
{
    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
                                                                        p,
                                                                        reinterpret_cast<::std::byte const*>(s),
                                                                        reinterpret_cast<::std::byte const*>(s) + n);
}

int main()
{
    native_memory_t memory{};
    memory.init_by_page_count(2uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    // Prepare FD table
    env.fd_storage.opens.resize(16uz);

    // Common: setup a directory fd at index 3 with '.' and full rights
    auto& fd_root = *env.fd_storage.opens.index_unchecked(3uz).fd_p;
    fd_root.rights_base = static_cast<rights_t>(-1);
    fd_root.rights_inherit = static_cast<rights_t>(-1);
    fd_root.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
    {
        auto& ds = fd_root.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
        ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
        entry.ptr->dir_stack.storage.file = ::fast_io::dir_file{u8"."};
        ds.dir_stack.push_back(::std::move(entry));
    }

    // Shared output buffer for filestat
    constexpr wasi_void_ptr_t stat_ptr{1024u};

    // Case 0: negative fd -> ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(-1),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  static_cast<wasi_void_ptr_t>(0u),
                                                                                  static_cast<wasi_size_t>(0u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf)
        {
            ::fast_io::io::perrln("error: pfg32 Case 0 expected ebadf. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: rights missing -> enotcapable
    {
        auto& fd = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fd.rights_base = static_cast<rights_t>(0);
        fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
        {
            auto& ds = fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
            ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
            entry.ptr->dir_stack.storage.file = ::fast_io::dir_file{u8"."};
            ds.dir_stack.push_back(::std::move(entry));
        }

        constexpr wasi_void_ptr_t p{4096u};
        constexpr auto s = u8"pfg32_no_rights";
        write_path(memory, p, s, sizeof(u8"pfg32_no_rights") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(4),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_no_rights") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable)
        {
            ::fast_io::io::perrln("error: pfg32 Case 1 expected enotcapable. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: fd is file -> enotdir
    {
        auto& fd = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        fd.rights_base = static_cast<rights_t>(-1);
        fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        constexpr wasi_void_ptr_t p{8192u};
        constexpr auto s = u8"pfg32_notdir";
        write_path(memory, p, s, sizeof(u8"pfg32_notdir") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(5),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_notdir") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir)
        {
            ::fast_io::io::perrln("error: pfg32 Case 2 expected enotdir. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: absolute path -> enotcapable
    {
        constexpr wasi_void_ptr_t p{12288u};
        constexpr auto s = u8"/abs32_file";
        write_path(memory, p, s, sizeof(u8"/abs32_file") - 1u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"/abs32_file") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable)
        {
            ::fast_io::io::perrln("error: pfg32 Case 3 expected enotcapable. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: empty path -> einval
    {
        constexpr wasi_void_ptr_t p{16384u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(0u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: pfg32 Case 4 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 5: '.' -> esuccess (directory)
    {
        constexpr wasi_void_ptr_t p{18432u};
        constexpr auto s = u8".";
        write_path(memory, p, s, sizeof(u8".") - 1u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8".") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess || read_filetype(memory, stat_ptr) != filetype_t::filetype_directory)
        {
            ::fast_io::io::perrln("error: pfg32 Case 5 expected esuccess + directory. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Prepare regular file and directory
    try
    {
        ::fast_io::native_file f{u8"pfg32_f.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pfg32_d");
    }
    catch(::fast_io::error)
    {
    }

    // Case 6: file -> esuccess (regular)
    {
        constexpr wasi_void_ptr_t p{20480u};
        constexpr auto s = u8"pfg32_f.txt";
        write_path(memory, p, s, sizeof(u8"pfg32_f.txt") - 1u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_f.txt") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess || read_filetype(memory, stat_ptr) != filetype_t::filetype_regular_file)
        {
            ::fast_io::io::perrln("error: pfg32 Case 6 expected esuccess + regular_file. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 7: dir -> esuccess (directory)
    {
        constexpr wasi_void_ptr_t p{22528u};
        constexpr auto s = u8"pfg32_d";
        write_path(memory, p, s, sizeof(u8"pfg32_d") - 1u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_d") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess || read_filetype(memory, stat_ptr) != filetype_t::filetype_directory)
        {
            ::fast_io::io::perrln("error: pfg32 Case 7 expected esuccess + directory. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 8: not exist -> enoent
    {
        constexpr wasi_void_ptr_t p{24576u};
        constexpr auto s = u8"pfg32_no_such";
        write_path(memory, p, s, sizeof(u8"pfg32_no_such") - 1u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_no_such") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent)
        {
            ::fast_io::io::perrln("error: pfg32 Case 8 expected enoent. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 9: intermediate is file -> enotdir
    {
        try
        {
            ::fast_io::native_file f{u8"pfg32_mid_file", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_t p{26624u};
        constexpr auto s = u8"pfg32_mid_file/x";
        write_path(memory, p, s, sizeof(u8"pfg32_mid_file/x") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_mid_file/x") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir)
        {
            ::fast_io::io::perrln("error: pfg32 Case 9 expected enotdir. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_mid_file", {});
        }
        catch(::fast_io::error)
        {
        }
    }

    // ===== symlink-related tests (skip on platforms without symlink) =====
#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x600) || defined(__MSDOS__) || defined(__DJGPP__))

    // Case 10: final symlink nofollow -> symbolic_link and size equals link target string length
    {
        try
        {
            ::fast_io::native_file f{u8"pfg32_tA", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"pfg32_tA", ::fast_io::at_fdcwd(), u8"pfg32_lA");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_t p{28672u};
        constexpr auto s = u8"pfg32_lA";
        write_path(memory, p, s, sizeof(u8"pfg32_lA") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_lA") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess || read_filetype(memory, stat_ptr) != filetype_t::filetype_symbolic_link)
        {
            ::fast_io::io::perrln("error: pfg32 Case 10 expected esuccess + symbolic_link. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        // verify size equals link string length on POSIX platforms
# ifndef _WIN32
        {
            auto const sz = read_size(memory, stat_ptr);
            if(static_cast<::std::uint_least64_t>(sz) != static_cast<::std::uint_least64_t>(sizeof(u8"pfg32_tA") - 1u))
            {
                ::fast_io::io::perrln("error: pfg32 Case 10 symlink size mismatch. ", static_cast<unsigned>(ret));
                ::fast_io::fast_terminate();
            }
        }
# endif
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_lA", {});
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_tA", {});
        }
        catch(::fast_io::error)
        {
        }
    }

    // Case 11: final symlink follow -> regular_file
    {
        try
        {
            ::fast_io::native_file f{u8"pfg32_tB", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"pfg32_tB", ::fast_io::at_fdcwd(), u8"pfg32_lB");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_t p{30720u};
        constexpr auto s = u8"pfg32_lB";
        write_path(memory, p, s, sizeof(u8"pfg32_lB") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  lookupflags_t::lookup_symlink_follow,
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_lB") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess || read_filetype(memory, stat_ptr) != filetype_t::filetype_regular_file)
        {
            ::fast_io::io::perrln("error: pfg32 Case 11 expected esuccess + regular_file. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_lB", {});
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_tB", {});
        }
        catch(::fast_io::error)
        {
        }
    }

    // Case 12: chain symlink l1->l2->target, follow
    {
        try
        {
            ::fast_io::native_file f{u8"pfg32_tC", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"pfg32_tC", ::fast_io::at_fdcwd(), u8"pfg32_lC2");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"pfg32_lC2", ::fast_io::at_fdcwd(), u8"pfg32_lC1");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_t p{32768u};
        constexpr auto s = u8"pfg32_lC1";
        write_path(memory, p, s, sizeof(u8"pfg32_lC1") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  lookupflags_t::lookup_symlink_follow,
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_lC1") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess || read_filetype(memory, stat_ptr) != filetype_t::filetype_regular_file)
        {
            ::fast_io::io::perrln("error: pfg32 Case 12 expected esuccess + regular_file. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_lC1", {});
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_lC2", {});
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_tC", {});
        }
        catch(::fast_io::error)
        {
        }
    }

    // Case 13: intermediate symlink to dir
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pfg32_realD");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_file f{u8"pfg32_realD/f", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"pfg32_realD", ::fast_io::at_fdcwd(), u8"pfg32_linkD");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_t p{34816u};
        constexpr auto s = u8"pfg32_linkD/f";
        write_path(memory, p, s, sizeof(u8"pfg32_linkD/f") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_linkD/f") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess || read_filetype(memory, stat_ptr) != filetype_t::filetype_regular_file)
        {
            ::fast_io::io::perrln("error: pfg32 Case 13 expected esuccess + regular_file. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_linkD", {});
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_realD/f", {});
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_realD", ::fast_io::native_at_flags::removedir);
        }
        catch(::fast_io::error)
        {
        }
    }

    // Case 14: final self-loop symlink; follow -> eloop; nofollow -> symbolic_link
    {
        try
        {
            ::fast_io::native_symlinkat(u8"pfg32_loop", ::fast_io::at_fdcwd(), u8"pfg32_loop");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_t p1{36864u};
        constexpr auto s1 = u8"pfg32_loop";
        write_path(memory, p1, s1, sizeof(u8"pfg32_loop") - 1u);
        auto const r1 = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                 static_cast<wasi_posix_fd_t>(3),
                                                                                 lookupflags_t::lookup_symlink_follow,
                                                                                 p1,
                                                                                 static_cast<wasi_size_t>(sizeof(u8"pfg32_loop") - 1u),
                                                                                 stat_ptr);
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop)
        {
            ::fast_io::io::perrln("error: pfg32 Case 14a expected eloop. ", static_cast<unsigned>(r1));
            ::fast_io::fast_terminate();
        }

        constexpr wasi_void_ptr_t p2{38912u};
        write_path(memory, p2, s1, sizeof(u8"pfg32_loop") - 1u);
        auto const r2 = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                 static_cast<wasi_posix_fd_t>(3),
                                                                                 static_cast<lookupflags_t>(0),
                                                                                 p2,
                                                                                 static_cast<wasi_size_t>(sizeof(u8"pfg32_loop") - 1u),
                                                                                 stat_ptr);
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess || read_filetype(memory, stat_ptr) != filetype_t::filetype_symbolic_link)
        {
            ::fast_io::io::perrln("error: pfg32 Case 14b expected esuccess + symbolic_link. ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }

        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_loop", {});
        }
        catch(::fast_io::error)
        {
        }
    }

    // Case 15: intermediate loop symlink -> eloop
    {
        try
        {
            ::fast_io::native_symlinkat(u8"pfg32_loop2", ::fast_io::at_fdcwd(), u8"pfg32_loop2");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_t p{40960u};
        constexpr auto s = u8"pfg32_loop2/x";
        write_path(memory, p, s, sizeof(u8"pfg32_loop2/x") - 1u);
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                static_cast<wasi_posix_fd_t>(3),
                                                                                static_cast<lookupflags_t>(0),
                                                                                p,
                                                                                static_cast<wasi_size_t>(sizeof(u8"pfg32_loop2/x") - 1u),
                                                                                stat_ptr);
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop)
        {
            ::fast_io::io::perrln("error: pfg32 Case 15 expected eloop. ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_loop2", {});
        }
        catch(::fast_io::error)
        {
        }
    }

    // Case 16: symlink to absolute path; follow -> enotcapable; nofollow -> symbolic_link
    {
        try
        {
            ::fast_io::native_symlinkat(u8"/etc", ::fast_io::at_fdcwd(), u8"pfg32_abs");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_t p1{43008u};
        constexpr auto s1 = u8"pfg32_abs";
        write_path(memory, p1, s1, sizeof(u8"pfg32_abs") - 1u);
        auto const r1 = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                 static_cast<wasi_posix_fd_t>(3),
                                                                                 lookupflags_t::lookup_symlink_follow,
                                                                                 p1,
                                                                                 static_cast<wasi_size_t>(sizeof(u8"pfg32_abs") - 1u),
                                                                                 stat_ptr);
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable)
        {
            ::fast_io::io::perrln("error: pfg32 Case 16a expected enotcapable. ", static_cast<unsigned>(r1));
            ::fast_io::fast_terminate();
        }

        constexpr wasi_void_ptr_t p2{45056u};
        write_path(memory, p2, s1, sizeof(u8"pfg32_abs") - 1u);
        auto const r2 = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                 static_cast<wasi_posix_fd_t>(3),
                                                                                 static_cast<lookupflags_t>(0),
                                                                                 p2,
                                                                                 static_cast<wasi_size_t>(sizeof(u8"pfg32_abs") - 1u),
                                                                                 stat_ptr);
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess || read_filetype(memory, stat_ptr) != filetype_t::filetype_symbolic_link)
        {
            ::fast_io::io::perrln("error: pfg32 Case 16b expected esuccess + symbolic_link. ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }

        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_abs", {});
        }
        catch(::fast_io::error)
        {
        }
    }

#endif  // symlink-capable platforms

    // ===== Windows/DOS invalid-character tests (path syntax pre-checks) =====
#if (defined(_WIN32) || defined(__CYGWIN__)) || (defined(__MSDOS__) || defined(__DJGPP__))
    // backslash in a component -> einval
    {
        constexpr wasi_void_ptr_t p{55296u};
        constexpr auto s = u8"pfg32_inv_bs_a\\b";
        write_path(memory, p, s, sizeof(u8"pfg32_inv_bs_a\\b") - 1u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_inv_bs_a\\b") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: pfg32 inv bs expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
    // sole backslash -> einval
    {
        constexpr wasi_void_ptr_t p{56320u};
        constexpr auto s = u8"\\";
        write_path(memory, p, s, sizeof(u8"\\") - 1u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"\\") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: pfg32 sole bs expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
    // '*' -> einval
    {
        constexpr wasi_void_ptr_t p{57344u};
        constexpr auto s = u8"pfg32_star_*";
        write_path(memory, p, s, sizeof(u8"pfg32_star_*") - 1u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_star_*") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: pfg32 '*' expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
    // '?' -> einval
    {
        constexpr wasi_void_ptr_t p{59392u};
        constexpr auto s = u8"pfg32_qmark_?";
        write_path(memory, p, s, sizeof(u8"pfg32_qmark_?") - 1u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_qmark_?") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: pfg32 '?' expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
    // '|' -> einval
    {
        constexpr wasi_void_ptr_t p{61440u};
        constexpr auto s = u8"pfg32_pipe_|";
        write_path(memory, p, s, sizeof(u8"pfg32_pipe_|") - 1u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_pipe_|") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: pfg32 '|' expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
    // '<' and '>' -> einval
    {
        constexpr wasi_void_ptr_t p1{63488u};
        constexpr auto s1 = u8"pfg32_lt_<";
        write_path(memory, p1, s1, sizeof(u8"pfg32_lt_<") - 1u);
        auto const r1 = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                 static_cast<wasi_posix_fd_t>(3),
                                                                                 static_cast<lookupflags_t>(0),
                                                                                 p1,
                                                                                 static_cast<wasi_size_t>(sizeof(u8"pfg32_lt_<") - 1u),
                                                                                 stat_ptr);
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: pfg32 '<' expected einval. ", static_cast<unsigned>(r1));
            ::fast_io::fast_terminate();
        }

        constexpr wasi_void_ptr_t p2{65536u};
        constexpr auto s2 = u8"pfg32_gt_>";
        write_path(memory, p2, s2, sizeof(u8"pfg32_gt_>") - 1u);
        auto const r2 = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                 static_cast<wasi_posix_fd_t>(3),
                                                                                 static_cast<lookupflags_t>(0),
                                                                                 p2,
                                                                                 static_cast<wasi_size_t>(sizeof(u8"pfg32_gt_>") - 1u),
                                                                                 stat_ptr);
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: pfg32 '>' expected einval. ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }
    }
    // '"' -> einval
    {
        constexpr wasi_void_ptr_t p{67584u};
        constexpr auto s = u8"pfg32_quote_\"";
        write_path(memory, p, s, sizeof(u8"pfg32_quote_\"") - 1u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_quote_\"") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: pfg32 '" " expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
    // ':' -> einval
    {
        constexpr wasi_void_ptr_t p{69632u};
        constexpr auto s = u8"pfg32_colon_a:b";
        write_path(memory, p, s, sizeof(u8"pfg32_colon_a:b") - 1u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"pfg32_colon_a:b") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: pfg32 ':' expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
    // drive-relative prefix 'C:foo' -> einval
    {
        constexpr wasi_void_ptr_t p{71680u};
        constexpr auto s = u8"C:pfg32_drive_rel";
        write_path(memory, p, s, sizeof(u8"C:pfg32_drive_rel") - 1u);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                  static_cast<wasi_posix_fd_t>(3),
                                                                                  static_cast<lookupflags_t>(0),
                                                                                  p,
                                                                                  static_cast<wasi_size_t>(sizeof(u8"C:pfg32_drive_rel") - 1u),
                                                                                  stat_ptr);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: pfg32 'C:' expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
#endif

    // Case 17: '..' as final -> enotcapable (escape root rejected)
    {
        constexpr wasi_void_ptr_t p{47104u};
        constexpr auto s = u8"..";
        write_path(memory, p, s, sizeof(u8"..") - 1u);
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                static_cast<wasi_posix_fd_t>(3),
                                                                                static_cast<lookupflags_t>(0),
                                                                                p,
                                                                                static_cast<wasi_size_t>(sizeof(u8"..") - 1u),
                                                                                stat_ptr);
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable)
        {
            ::fast_io::io::perrln("error: pfg32 Case 17 expected enotcapable. ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
    }

    // Case 18: 'a/b/..' -> esuccess (directory)
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pfg32_a");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pfg32_a/b");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_t p{49152u};
        constexpr auto s = u8"pfg32_a/b/..";
        write_path(memory, p, s, sizeof(u8"pfg32_a/b/..") - 1u);
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                static_cast<wasi_posix_fd_t>(3),
                                                                                static_cast<lookupflags_t>(0),
                                                                                p,
                                                                                static_cast<wasi_size_t>(sizeof(u8"pfg32_a/b/..") - 1u),
                                                                                stat_ptr);
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess || read_filetype(memory, stat_ptr) != filetype_t::filetype_directory)
        {
            ::fast_io::io::perrln("error: pfg32 Case 18 expected esuccess + directory. ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }

        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_a/b", ::fast_io::native_at_flags::removedir);
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_a", ::fast_io::native_at_flags::removedir);
        }
        catch(::fast_io::error)
        {
        }
    }

    // Case 19: 'a/.' -> esuccess (directory)
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pfg32_a2");
        }
        catch(::fast_io::error)
        {
        }
        constexpr wasi_void_ptr_t p{51200u};
        constexpr auto s = u8"pfg32_a2/.";
        write_path(memory, p, s, sizeof(u8"pfg32_a2/.") - 1u);
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(env,
                                                                                static_cast<wasi_posix_fd_t>(3),
                                                                                static_cast<lookupflags_t>(0),
                                                                                p,
                                                                                static_cast<wasi_size_t>(sizeof(u8"pfg32_a2/.") - 1u),
                                                                                stat_ptr);
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess || read_filetype(memory, stat_ptr) != filetype_t::filetype_directory)
        {
            ::fast_io::io::perrln("error: pfg32 Case 19 expected esuccess + directory. ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_a2", ::fast_io::native_at_flags::removedir);
        }
        catch(::fast_io::error)
        {
        }
    }

    // Cleanup artifacts
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_f.txt", {});
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pfg32_d", ::fast_io::native_at_flags::removedir);
    }
    catch(::fast_io::error)
    {
    }

    return 0;
}


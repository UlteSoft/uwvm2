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

#include <uwvm2/imported/wasi/wasip1/func/path_link.h>

// NOLINTBEGIN

using ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;

inline static void write_bytes32(native_memory_t& memory, wasi_void_ptr_t p, void const* s, ::std::size_t n)
{
    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
                                                                        p,
                                                                        reinterpret_cast<::std::byte const*>(s),
                                                                        reinterpret_cast<::std::byte const*>(s) + n);
}

inline static void write_cu8str32(native_memory_t& memory, wasi_void_ptr_t p, char8_t const* s)
{ write_bytes32(memory, p, s, ::std::char_traits<char8_t>::length(s)); }

inline static void set_dirfd(wasip1_environment<native_memory_t>& env, ::std::size_t idx, rights_t base_rights)
{
    auto& fd = *env.fd_storage.opens.index_unchecked(idx).fd_p;
    fd.rights_base = base_rights;
    fd.rights_inherit = static_cast<rights_t>(-1);
    fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
    auto& ds = fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
    ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
    entry.ptr->dir_stack.file = ::fast_io::dir_file{u8"."};
    ds.dir_stack.push_back(::std::move(entry));
}

// helpers for cleanup per test case
inline static void try_unlink(char8_t const* name)
{
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), ::fast_io::mnp::os_c_str(name), {});
    }
    catch(::fast_io::error)
    {
    }
}

inline static void try_rmdir(char8_t const* name)
{
    try
    {
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), ::fast_io::mnp::os_c_str(name), ::fast_io::native_at_flags::removedir);
    }
    catch(::fast_io::error)
    {
    }
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
    env.fd_storage.opens.resize(16uz);

    // Pre-create a source file for positive cases
    {
        ::fast_io::native_file f{u8"uwvm_ut_pl32_src.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
        ::std::byte const data[3]{std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
        ::fast_io::operations::write_all_bytes(f, data, data + 3);
    }

    // Constants: memory offsets
    constexpr wasi_void_ptr_t P0{1024u};
    constexpr wasi_void_ptr_t P1{4096u};
    constexpr wasi_void_ptr_t P2{8192u};
    constexpr wasi_void_ptr_t P3{12288u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P4{16384u};
    [[maybe_unused]] constexpr wasi_void_ptr_t P5{20480u};

    // ===== Case 0a: negative old_fd -> ebadf =====
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(-1),
                                                                          static_cast<lookupflags_t>(0),
                                                                          static_cast<wasi_void_ptr_t>(0u),
                                                                          static_cast<wasi_size_t>(0u),
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<wasi_void_ptr_t>(0u),
                                                                          static_cast<wasi_size_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 0b: negative new_fd -> ebadf =====
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<lookupflags_t>(0),
                                                                          static_cast<wasi_void_ptr_t>(0u),
                                                                          static_cast<wasi_size_t>(0u),
                                                                          static_cast<wasi_posix_fd_t>(-1),
                                                                          static_cast<wasi_void_ptr_t>(0u),
                                                                          static_cast<wasi_size_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Common two dir fds at 3 (old) and 4 (new)
    set_dirfd(env, 3uz, static_cast<rights_t>(-1));
    set_dirfd(env, 4uz, static_cast<rights_t>(-1));

    // ===== Case 1a: missing right_path_link_source on old -> enotcapable =====
    {
        auto& oldfd = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        oldfd.rights_base = static_cast<rights_t>(0);
        oldfd.rights_inherit = static_cast<rights_t>(-1);
        oldfd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
        {
            auto& ds = oldfd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
            ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
            entry.ptr->dir_stack.file = ::fast_io::dir_file{u8"."};
            ds.dir_stack.push_back(::std::move(entry));
        }

        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"uwvm_ut_pl32_dst_a.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(5),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P0,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          P1,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_dst_a.txt") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"uwvm_ut_pl32_dst_a.txt");
    }

    // ===== Case 1b: missing right_path_link_target on new -> enotcapable =====
    {
        auto& newfd = *env.fd_storage.opens.index_unchecked(6uz).fd_p;
        newfd.rights_base = static_cast<rights_t>(0);
        newfd.rights_inherit = static_cast<rights_t>(-1);
        newfd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
        {
            auto& ds = newfd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
            ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
            entry.ptr->dir_stack.file = ::fast_io::dir_file{u8"."};
            ds.dir_stack.push_back(::std::move(entry));
        }

        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"uwvm_ut_pl32_dst_b.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P0,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                          static_cast<wasi_posix_fd_t>(6),
                                                                          P1,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_dst_b.txt") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"uwvm_ut_pl32_dst_b.txt");
    }

    // ===== Case 2a: old fd is file -> enotdir =====
    {
        auto& oldf = *env.fd_storage.opens.index_unchecked(7uz).fd_p;
        oldf.rights_base = static_cast<rights_t>(-1);
        oldf.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"uwvm_ut_pl32_dst_c.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(7),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P0,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          P1,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_dst_c.txt") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"uwvm_ut_pl32_dst_c.txt");
    }

    // ===== Case 2b: new fd is file -> enotdir =====
    {
        auto& newf = *env.fd_storage.opens.index_unchecked(8uz).fd_p;
        newf.rights_base = static_cast<rights_t>(-1);
        newf.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"uwvm_ut_pl32_dst_d.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P0,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                          static_cast<wasi_posix_fd_t>(8),
                                                                          P1,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_dst_d.txt") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"uwvm_ut_pl32_dst_d.txt");
    }

    // ===== Case 3a: empty old_path -> einval =====
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P2,
                                                                          static_cast<wasi_size_t>(0u),
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          P3,
                                                                          static_cast<wasi_size_t>(1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 3b: empty new_path -> einval =====
    {
        write_cu8str32(memory, P2, u8"uwvm_ut_pl32_src.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P2,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          P3,
                                                                          static_cast<wasi_size_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 4a: invalid UTF-8 (contains zero) -> eilseq =====
    {
        unsigned char const bad_bytes[3]{'A', 0x00u, 'B'};
        write_bytes32(memory, P0, bad_bytes, 3u);
        write_cu8str32(memory, P1, u8"uwvm_ut_pl32_dst_e.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P0,
                                                                          static_cast<wasi_size_t>(3u),
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          P1,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_dst_e.txt") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eilseq)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"uwvm_ut_pl32_dst_e.txt");
    }

    // ===== Case 4b: absolute path (old) -> eperm =====
    {
        write_cu8str32(memory, P0, u8"/abs_src");
        write_cu8str32(memory, P1, u8"dst.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P0,
                                                                          static_cast<wasi_size_t>(sizeof(u8"/abs_src") - 1u),
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          P1,
                                                                          static_cast<wasi_size_t>(sizeof(u8"dst.txt") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"dst.txt");
    }

    // ===== Case 5: new path final '.' -> eexist =====
    {
        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8".");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P0,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          P1,
                                                                          static_cast<wasi_size_t>(sizeof(u8".") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eexist)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 6: new path intermediate not exist -> enoent =====
    {
        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"not_exist_dir/child.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P0,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          P1,
                                                                          static_cast<wasi_size_t>(sizeof(u8"not_exist_dir/child.txt") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        // cleanup (best-effort)
        try_unlink(u8"not_exist_dir/child.txt");
        try_rmdir(u8"not_exist_dir");
    }

    // ===== Case 7: success: link file to new name in same dir =====
    {
        // ensure no stale file
        try
        {
            ::fast_io::native_unlinkat(at(::fast_io::dir_file{u8"."}), u8"uwvm_ut_pl32_dst_ok.txt");
        }
        catch(...)
        {
        }

        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"uwvm_ut_pl32_dst_ok.txt");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P0,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          P1,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_dst_ok.txt") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        // verify content
        ::fast_io::native_file f{u8"uwvm_ut_pl32_dst_ok.txt", ::fast_io::open_mode::in};
        ::std::byte buf[3]{};
        auto p = ::fast_io::operations::read_some_bytes(f, buf, buf + 3);
        if(p != buf + 3 || buf[0] != ::std::byte{'A'} || buf[1] != ::std::byte{'B'} || buf[2] != ::std::byte{'C'})
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"uwvm_ut_pl32_dst_ok.txt");
    }

    // ===== Symlink-related tests (platforms that support symlink/linkat semantics) =====
#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x600) || defined(__MSDOS__) || defined(__DJGPP__))

    // Prepare some fixtures
    try
    {
        ::fast_io::native_file f{u8"pl32_tA.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        ::std::byte const data[1]{std::byte{'X'}};
        ::fast_io::operations::write_all_bytes(f, data, data + 1);
    }
    catch(::fast_io::error)
    {
    }
    try
    {
        ::fast_io::native_symlinkat(u8"pl32_tA.txt", ::fast_io::at_fdcwd(), u8"pl32_lA");
    }
    catch(::fast_io::error)
    {
    }

    // ===== Case 8: old is symlink, nofollow -> link the symlink itself (new becomes symlink) =====
    {
        // cleanup dst
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pl32_hlA", {});
        }
        catch(::fast_io::error)
        {
        }

        write_cu8str32(memory, P0, u8"pl32_lA");
        write_cu8str32(memory, P1, u8"pl32_hlA");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P0,
                                                                          static_cast<wasi_size_t>(sizeof(u8"pl32_lA") - 1u),
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          P1,
                                                                          static_cast<wasi_size_t>(sizeof(u8"pl32_hlA") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        // verify new is also a symlink with same payload
        auto const link_payload = ::fast_io::native_readlinkat<char8_t>(::fast_io::at_fdcwd(), u8"pl32_hlA");
        if(link_payload != u8"pl32_tA.txt")
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_hlA");
        try_unlink(u8"pl32_lA");
        try_unlink(u8"pl32_tA.txt");
    }

    // ===== Case 9: old is symlink, follow -> link target regular file (new is a regular file hardlink) =====
    {
        // cleanup dst
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pl32_hlB", {});
        }
        catch(::fast_io::error)
        {
        }

        // create target and link
        try
        {
            ::fast_io::native_file f{u8"pl32_tB.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
            ::std::byte const data[1]{std::byte{'Y'}};
            ::fast_io::operations::write_all_bytes(f, data, data + 1);
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"pl32_tB.txt", ::fast_io::at_fdcwd(), u8"pl32_lB");
        }
        catch(::fast_io::error)
        {
        }

        write_cu8str32(memory, P0, u8"pl32_lB");
        write_cu8str32(memory, P1, u8"pl32_hlB");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t::lookup_symlink_follow,
                                                                          P0,
                                                                          static_cast<wasi_size_t>(sizeof(u8"pl32_lB") - 1u),
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          P1,
                                                                          static_cast<wasi_size_t>(sizeof(u8"pl32_hlB") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        // verify not a symlink
        bool is_symlink{};
        try
        {
            (void)::fast_io::native_readlinkat<char8_t>(::fast_io::at_fdcwd(), u8"pl32_hlB");
            is_symlink = true;
        }
        catch(::fast_io::error)
        {
        }
        if(is_symlink)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        // verify content equals target
        ::fast_io::native_file f{u8"pl32_hlB", ::fast_io::open_mode::in};
        ::std::byte b{};
        auto p2 = ::fast_io::operations::read_some_bytes(f, &b, &b + 1);
        if(p2 != &b + 1 || b != ::std::byte{'Y'})
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_hlB");
        try_unlink(u8"pl32_lB");
        try_unlink(u8"pl32_tB.txt");
    }

    // ===== Case 10: broken symlink; nofollow -> esuccess (link the symlink); follow -> enoent =====
    {
        try
        {
            ::fast_io::native_symlinkat(u8"no_such_target", ::fast_io::at_fdcwd(), u8"pl32_lBroken");
        }
        catch(::fast_io::error)
        {
        }

        // nofollow
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pl32_hlBroken_nf", {});
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"pl32_lBroken");
        write_cu8str32(memory, P1, u8"pl32_hlBroken_nf");
        auto const r1 = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                         static_cast<wasi_posix_fd_t>(3),
                                                                         static_cast<lookupflags_t>(0),
                                                                         P0,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_lBroken") - 1u),
                                                                         static_cast<wasi_posix_fd_t>(4),
                                                                         P1,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_hlBroken_nf") - 1u));
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r1));
            ::fast_io::fast_terminate();
        }
        auto const payload = ::fast_io::native_readlinkat<char8_t>(::fast_io::at_fdcwd(), u8"pl32_hlBroken_nf");
        if(payload != u8"no_such_target")
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r1));
            ::fast_io::fast_terminate();
        }

        // follow
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pl32_hlBroken_f", {});
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"pl32_lBroken");
        write_cu8str32(memory, P1, u8"pl32_hlBroken_f");
        auto const r2 = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                         static_cast<wasi_posix_fd_t>(3),
                                                                         ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t::lookup_symlink_follow,
                                                                         P0,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_lBroken") - 1u),
                                                                         static_cast<wasi_posix_fd_t>(4),
                                                                         P1,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_hlBroken_f") - 1u));
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_hlBroken_nf");
        try_unlink(u8"pl32_hlBroken_f");
        try_unlink(u8"pl32_lBroken");
    }

    // ===== Case 11: self-loop symlink; nofollow -> esuccess; follow -> eloop =====
    {
        try
        {
            ::fast_io::native_symlinkat(u8"pl32_loop", ::fast_io::at_fdcwd(), u8"pl32_loop");
        }
        catch(::fast_io::error)
        {
        }
        // nofollow
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pl32_hlLoop_nf", {});
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"pl32_loop");
        write_cu8str32(memory, P1, u8"pl32_hlLoop_nf");
        auto const r1 = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                         static_cast<wasi_posix_fd_t>(3),
                                                                         static_cast<lookupflags_t>(0),
                                                                         P0,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_loop") - 1u),
                                                                         static_cast<wasi_posix_fd_t>(4),
                                                                         P1,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_hlLoop_nf") - 1u));
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r1));
            ::fast_io::fast_terminate();
        }
        auto const payload = ::fast_io::native_readlinkat<char8_t>(::fast_io::at_fdcwd(), u8"pl32_hlLoop_nf");
        if(payload != u8"pl32_loop")
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r1));
            ::fast_io::fast_terminate();
        }

        // follow -> eloop
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pl32_hlLoop_f", {});
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"pl32_loop");
        write_cu8str32(memory, P1, u8"pl32_hlLoop_f");
        auto const r2 = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                         static_cast<wasi_posix_fd_t>(3),
                                                                         ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t::lookup_symlink_follow,
                                                                         P0,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_loop") - 1u),
                                                                         static_cast<wasi_posix_fd_t>(4),
                                                                         P1,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_hlLoop_f") - 1u));
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_hlLoop_nf");
        try_unlink(u8"pl32_hlLoop_f");
        try_unlink(u8"pl32_loop");
    }

    // ===== Case 12: old is directory -> eperm =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_dirD");
        }
        catch(::fast_io::error)
        {
        }
        // cleanup dst
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pl32_dirD_hl", {});
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"pl32_dirD");
        write_cu8str32(memory, P1, u8"pl32_dirD_hl");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_dirD") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_dirD_hl") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm
# if defined(_WIN32)
           && r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir
# endif
        )
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pl32_dirD", ::fast_io::native_at_flags::removedir);
        }
        catch(::fast_io::error)
        {
        }
        // ensure cleanup
        try_unlink(u8"pl32_dirD_hl");
        try_rmdir(u8"pl32_dirD");
    }

    // ===== Case 13: cross subdirectory link =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_A");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_B");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_file f{u8"pl32_A/x.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
            ::std::byte const d[1]{std::byte{'Z'}};
            ::fast_io::operations::write_all_bytes(f, d, d + 1);
        }
        catch(::fast_io::error)
        {
        }

        // link A/x.txt -> B/x_hl.txt
        write_cu8str32(memory, P0, u8"pl32_A/x.txt");
        write_cu8str32(memory, P1, u8"pl32_B/x_hl.txt");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_A/x.txt") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_B/x_hl.txt") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        ::fast_io::native_file f{u8"pl32_B/x_hl.txt", ::fast_io::open_mode::in};
        ::std::byte b{};
        auto p2 = ::fast_io::operations::read_some_bytes(f, &b, &b + 1);
        if(p2 != &b + 1 || b != ::std::byte{'Z'})
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_B/x_hl.txt");
        try_unlink(u8"pl32_A/x.txt");
        try_rmdir(u8"pl32_B");
        try_rmdir(u8"pl32_A");
    }

    // ===== Case 14: intermediate symlink dir in old path -> success =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_realC");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_file f{u8"pl32_realC/f.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
            ::std::byte const d[1]{std::byte{'Q'}};
            ::fast_io::operations::write_all_bytes(f, d, d + 1);
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"pl32_realC", ::fast_io::at_fdcwd(), u8"pl32_linkC");
        }
        catch(::fast_io::error)
        {
        }

        // link linkC/f.txt -> f_hlC.txt
        write_cu8str32(memory, P0, u8"pl32_linkC/f.txt");
        write_cu8str32(memory, P1, u8"pl32_f_hlC.txt");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_linkC/f.txt") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_f_hlC.txt") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_f_hlC.txt");
        try_unlink(u8"pl32_linkC");
        try_unlink(u8"pl32_realC/f.txt");
        try_rmdir(u8"pl32_realC");
    }

    // ===== Case 15: new parent is symlink dir -> success =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_realN");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"pl32_realN", ::fast_io::at_fdcwd(), u8"pl32_linkN");
        }
        catch(::fast_io::error)
        {
        }

        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"pl32_linkN/dst_in_symlink_dir.txt");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_linkN/dst_in_symlink_dir.txt") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_linkN/dst_in_symlink_dir.txt");
        try_unlink(u8"pl32_linkN");
        try_rmdir(u8"pl32_realN");
    }

    // ===== Case 16: new path already exists -> eexist =====
    {
        try
        {
            ::fast_io::native_file f{u8"pl32_exist.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::creat};
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"pl32_exist.txt");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_exist.txt") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eexist)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_exist.txt");
    }

    // ===== Case 17: old symlink to absolute path; follow -> eperm; nofollow -> esuccess and new is symlink =====
    {
        try
        {
            ::fast_io::native_symlinkat(u8"/etc", ::fast_io::at_fdcwd(), u8"pl32_absL");
        }
        catch(::fast_io::error)
        {
        }

        // follow
        write_cu8str32(memory, P0, u8"pl32_absL");
        write_cu8str32(memory, P1, u8"pl32_abs_follow_dst");
        auto const r1 = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                         static_cast<wasi_posix_fd_t>(3),
                                                                         ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t::lookup_symlink_follow,
                                                                         P0,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_absL") - 1u),
                                                                         static_cast<wasi_posix_fd_t>(4),
                                                                         P1,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_abs_follow_dst") - 1u));
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r1));
            ::fast_io::fast_terminate();
        }

        // nofollow
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pl32_abs_nof_dst", {});
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"pl32_absL");
        write_cu8str32(memory, P1, u8"pl32_abs_nof_dst");
        auto const r2 = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                         static_cast<wasi_posix_fd_t>(3),
                                                                         static_cast<lookupflags_t>(0),
                                                                         P0,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_absL") - 1u),
                                                                         static_cast<wasi_posix_fd_t>(4),
                                                                         P1,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_abs_nof_dst") - 1u));
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }
        auto const payload = ::fast_io::native_readlinkat<char8_t>(::fast_io::at_fdcwd(), u8"pl32_abs_nof_dst");
        if(payload != u8"/etc")
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_abs_nof_dst");
        try_unlink(u8"pl32_absL");
    }

    // ===== Case 18: symlink to directory; follow -> eperm; nofollow -> esuccess =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_realDirE");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"pl32_realDirE", ::fast_io::at_fdcwd(), u8"pl32_lDirE");
        }
        catch(::fast_io::error)
        {
        }

        // follow
        write_cu8str32(memory, P0, u8"pl32_lDirE");
        write_cu8str32(memory, P1, u8"pl32_dirE_dst");
        auto const r1 = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                         static_cast<wasi_posix_fd_t>(3),
                                                                         ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t::lookup_symlink_follow,
                                                                         P0,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_lDirE") - 1u),
                                                                         static_cast<wasi_posix_fd_t>(4),
                                                                         P1,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_dirE_dst") - 1u));
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm
# if defined(_WIN32)
           && r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir
# endif
        )
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r1));
            ::fast_io::fast_terminate();
        }

        // nofollow
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pl32_dirE_dst_nf", {});
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"pl32_lDirE");
        write_cu8str32(memory, P1, u8"pl32_dirE_dst_nf");
        auto const r2 = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                         static_cast<wasi_posix_fd_t>(3),
                                                                         static_cast<lookupflags_t>(0),
                                                                         P0,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_lDirE") - 1u),
                                                                         static_cast<wasi_posix_fd_t>(4),
                                                                         P1,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_dirE_dst_nf") - 1u));
# if defined(_WIN32)
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }
# else
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }
        auto const payload = ::fast_io::native_readlinkat<char8_t>(::fast_io::at_fdcwd(), u8"pl32_dirE_dst_nf");
        if(payload != u8"pl32_realDirE")
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r2));
            ::fast_io::fast_terminate();
        }

# endif
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pl32_realDirE", ::fast_io::native_at_flags::removedir);
        }
        catch(::fast_io::error)
        {
        }
        // cleanup
        try_unlink(u8"pl32_dirE_dst_nf");
        try_unlink(u8"pl32_lDirE");
        try_rmdir(u8"pl32_realDirE");
    }

    // ===== Case 19: normalized path with '..' and intermediate symlink =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_NA");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_NA/B");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_NA/B/C");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_file f{u8"pl32_NA/B/C/file.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
            ::std::byte const d[1]{std::byte{'M'}};
            ::fast_io::operations::write_all_bytes(f, d, d + 1);
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"B", ::fast_io::at_fdcwd(), u8"pl32_NA/lnB");
        }
        catch(::fast_io::error)
        {
        }

        // old path: pl32_NA/lnB/../B/C/file.txt (normalized to pl32_NA/B/C/file.txt)
        write_cu8str32(memory, P0, u8"pl32_NA/lnB/../B/C/file.txt");
        write_cu8str32(memory, P1, u8"pl32_N_destM");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t::lookup_symlink_follow,
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_NA/lnB/../B/C/file.txt") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_N_destM") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        ::fast_io::native_file f{u8"pl32_N_destM", ::fast_io::open_mode::in};
        ::std::byte b{};
        auto p2 = ::fast_io::operations::read_some_bytes(f, &b, &b + 1);
        if(p2 != &b + 1 || b != ::std::byte{'M'})
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_N_destM");
        try_unlink(u8"pl32_NA/lnB");
        try_unlink(u8"pl32_NA/B/C/file.txt");
        try_rmdir(u8"pl32_NA/B/C");
        try_rmdir(u8"pl32_NA/B");
        try_rmdir(u8"pl32_NA");
    }

    // ===== Case 20: new path contains zero byte (invalid UTF-8) -> eilseq =====
    {
        unsigned char const bad_bytes[3]{'A', 0x00u, 'B'};
        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_bytes32(memory, P1, bad_bytes, 3u);
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(3u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eilseq)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
    }

#endif  // symlink-capable platforms

    // ===== Windows/DOS invalid-character tests (path syntax pre-checks) =====
#if (defined(_WIN32) || defined(__CYGWIN__)) || (defined(__MSDOS__) || defined(__DJGPP__))
    // invalid in old path -> einval
    {
        write_cu8str32(memory, P0, u8"pl32_inv_bs_a\\b");
        write_cu8str32(memory, P1, u8"pl32_dst_inv1");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P0,
                                                                          static_cast<wasi_size_t>(sizeof(u8"pl32_inv_bs_a\\b") - 1u),
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          P1,
                                                                          static_cast<wasi_size_t>(sizeof(u8"pl32_dst_inv1") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
    // invalid in new path -> einval
    {
        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"pl32_star_*");
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                          static_cast<wasi_posix_fd_t>(3),
                                                                          static_cast<lookupflags_t>(0),
                                                                          P0,
                                                                          static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          P1,
                                                                          static_cast<wasi_size_t>(sizeof(u8"pl32_star_*") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
#endif

    // ===== Case 21: verify hardlink semantics (write via dest, read via src) =====
    {
        // Ensure dst exists as hard link to src (from Case 7), then append 'D' to dst
        try
        {
            ::fast_io::native_file f{u8"uwvm_ut_pl32_dst_ok.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::app};
            ::std::byte const d[1]{std::byte{'D'}};
            ::fast_io::operations::write_all_bytes(f, d, d + 1);
        }
        catch(::fast_io::error)
        {
        }
    }

    // ===== Case 22: create multiple hardlinks to the same file and read through all =====
    {
        // Prepare source
        try
        {
            ::fast_io::native_file f{u8"pl32_many_src.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
            ::std::byte const d[2]{std::byte{'K'}, std::byte{'1'}};
            ::fast_io::operations::write_all_bytes(f, d, d + 2);
        }
        catch(::fast_io::error)
        {
        }
        // Link to a, b, c
        auto link_one = [&](char8_t const* dst)
        {
            write_cu8str32(memory, P0, u8"pl32_many_src.txt");
            write_cu8str32(memory, P1, dst);
            auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                            static_cast<wasi_posix_fd_t>(3),
                                                                            static_cast<lookupflags_t>(0),
                                                                            P0,
                                                                            static_cast<wasi_size_t>(sizeof(u8"pl32_many_src.txt") - 1u),
                                                                            static_cast<wasi_posix_fd_t>(4),
                                                                            P1,
                                                                            static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(dst)));
            if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
            {
                ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
                ::fast_io::fast_terminate();
            }
        };
        link_one(u8"pl32_many_a");
        link_one(u8"pl32_many_b");
        link_one(u8"pl32_many_c");

        auto check = [&](char8_t const* name)
        {
            ::fast_io::native_file f{
                ::uwvm2::utils::container::u8cstring_view{::fast_io::containers::null_terminated, name, ::std::char_traits<char8_t>::length(name)},
                ::fast_io::open_mode::in
            };
            ::std::byte buf[2]{};
            auto p = ::fast_io::operations::read_some_bytes(f, buf, buf + 2);
            if(p != buf + 2 || buf[0] != ::std::byte{'K'} || buf[1] != ::std::byte{'1'})
            {
                ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(0u));
                ::fast_io::fast_terminate();
            }
        };
        check(u8"pl32_many_a");
        check(u8"pl32_many_b");
        check(u8"pl32_many_c");
        // cleanup
        try_unlink(u8"pl32_many_a");
        try_unlink(u8"pl32_many_b");
        try_unlink(u8"pl32_many_c");
        try_unlink(u8"pl32_many_src.txt");
    }

    // ===== Case 23: new path final '..' -> eperm =====
    {
        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"..");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"..") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 24: new path normalized with '..' inside parent (a/../dstN) -> esuccess at root =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_np");
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"pl32_np/../pl32_dstN.txt");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_np/../pl32_dstN.txt") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        ::fast_io::native_file f{u8"pl32_dstN.txt", ::fast_io::open_mode::in};
        ::std::byte b{};
        auto p = ::fast_io::operations::read_some_bytes(f, &b, &b + 1);
        if(p != &b + 1 || b != ::std::byte{'A'})
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"pl32_np", ::fast_io::native_at_flags::removedir);
        }
        catch(::fast_io::error)
        {
        }
        // cleanup
        try_unlink(u8"pl32_dstN.txt");
    }

    // ===== Case 25: old path with './' segments -> esuccess =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_dotseg");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_file f{u8"pl32_dotseg/fileQ", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
            ::std::byte const d[1]{std::byte{'q'}};
            ::fast_io::operations::write_all_bytes(f, d, d + 1);
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"pl32_dotseg/./fileQ");
        write_cu8str32(memory, P1, u8"pl32_dotseg/fileQ_hl");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_dotseg/./fileQ") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_dotseg/fileQ_hl") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_dotseg/fileQ_hl");
        try_unlink(u8"pl32_dotseg/fileQ");
        try_rmdir(u8"pl32_dotseg");
    }

    // ===== Case 26: new parent is an existing file -> enotdir =====
    {
        try
        {
            ::fast_io::native_file f{u8"pl32_mid_as_file", ::fast_io::open_mode::out | ::fast_io::open_mode::creat};
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"pl32_mid_as_file/dst.txt");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_mid_as_file/dst.txt") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_mid_as_file/dst.txt");
        try_unlink(u8"pl32_mid_as_file");
    }

    // ===== Case 27: old intermediate is file -> enotdir =====
    {
        try
        {
            ::fast_io::native_file f{u8"pl32_old_mid_file", ::fast_io::open_mode::out | ::fast_io::open_mode::creat};
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"pl32_old_mid_file/x");
        write_cu8str32(memory, P1, u8"pl32_old_mid_file_dst");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_old_mid_file/x") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_old_mid_file_dst") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_old_mid_file_dst");
        try_unlink(u8"pl32_old_mid_file");
    }

#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x600) || defined(__MSDOS__) || defined(__DJGPP__))
    // ===== Case 28: multi-level new parent symlink chain -> esuccess =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_realN1");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_realN1/realN2");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"pl32_realN1", ::fast_io::at_fdcwd(), u8"pl32_ln1");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"pl32_ln1/realN2", ::fast_io::at_fdcwd(), u8"pl32_ln2");
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"pl32_ln2/dst_multi_symlink.txt");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_ln2/dst_multi_symlink.txt") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_ln2/dst_multi_symlink.txt");
        try_unlink(u8"pl32_ln2");
        try_unlink(u8"pl32_ln1");
        try_rmdir(u8"pl32_realN1/realN2");
        try_rmdir(u8"pl32_realN1");
    }
#endif

    // ===== Case 29: new name equals old name (same dir) -> eexist =====
    {
        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"uwvm_ut_pl32_src.txt");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eexist)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 30: cross-subdir with '..' in new path middle =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_csd");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_csd/A");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_csd/C");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_file f{u8"pl32_csd/A/s.txt", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
            ::std::byte const d[1]{std::byte{'S'}};
            ::fast_io::operations::write_all_bytes(f, d, d + 1);
        }
        catch(::fast_io::error)
        {
        }

        write_cu8str32(memory, P0, u8"pl32_csd/A/s.txt");
        write_cu8str32(memory, P1, u8"pl32_csd/C/../dst_s_mid_dotdot.txt");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_csd/A/s.txt") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_csd/C/../dst_s_mid_dotdot.txt") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        ::fast_io::native_file f{u8"pl32_csd/dst_s_mid_dotdot.txt", ::fast_io::open_mode::in};
        ::std::byte b{};
        auto p = ::fast_io::operations::read_some_bytes(f, &b, &b + 1);
        if(p != &b + 1 || b != ::std::byte{'S'})
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_csd/dst_s_mid_dotdot.txt");
        try_unlink(u8"pl32_csd/A/s.txt");
        try_rmdir(u8"pl32_csd/C");
        try_rmdir(u8"pl32_csd/A");
        try_rmdir(u8"pl32_csd");
    }

#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x600) || defined(__MSDOS__) || defined(__DJGPP__))
    // ===== Case 31: broken symlink chain l1->l2->l3->noexist; nofollow -> esuccess linking l1 =====
    {
        try
        {
            ::fast_io::native_symlinkat(u8"l2_broken", ::fast_io::at_fdcwd(), u8"l1_broken");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"l3_broken", ::fast_io::at_fdcwd(), u8"l2_broken");
        }
        catch(::fast_io::error)
        {
        }
        // l3_broken does not exist
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"l1_broken_hl", {});
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"l1_broken");
        write_cu8str32(memory, P1, u8"l1_broken_hl");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"l1_broken") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(4),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"l1_broken_hl") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        auto const pl = ::fast_io::native_readlinkat<char8_t>(::fast_io::at_fdcwd(), u8"l1_broken_hl");
        if(pl != u8"l2_broken")
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"l1_broken_hl");
        try_unlink(u8"l2_broken");
        try_unlink(u8"l1_broken");
    }
#endif

    // ===== Case 32: link within same fd (old_fd == new_fd) =====
    {
        write_cu8str32(memory, P0, u8"uwvm_ut_pl32_src.txt");
        write_cu8str32(memory, P1, u8"pl32_samefd_dst.txt");
        auto const r = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        static_cast<lookupflags_t>(0),
                                                                        P0,
                                                                        static_cast<wasi_size_t>(sizeof(u8"uwvm_ut_pl32_src.txt") - 1u),
                                                                        static_cast<wasi_posix_fd_t>(3),
                                                                        P1,
                                                                        static_cast<wasi_size_t>(sizeof(u8"pl32_samefd_dst.txt") - 1u));
        if(r != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("[pl32] L:", static_cast<unsigned>(__LINE__), " ", static_cast<unsigned>(r));
            ::fast_io::fast_terminate();
        }
        // cleanup
        try_unlink(u8"pl32_samefd_dst.txt");
    }

    // ===== Case 33: ensure linking file inside nested dirs works repeatedly =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_nested");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_nested/a");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"pl32_nested/a/b");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_file f{u8"pl32_nested/a/b/srcN", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
            ::std::byte const d[2]{std::byte{'N'}, std::byte{'1'}};
            ::fast_io::operations::write_all_bytes(f, d, d + 2);
        }
        catch(::fast_io::error)
        {
        }
        write_cu8str32(memory, P0, u8"pl32_nested/a/b/srcN");
        write_cu8str32(memory, P1, u8"pl32_nested/dstN1");
        auto const r1 = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                         static_cast<wasi_posix_fd_t>(3),
                                                                         static_cast<lookupflags_t>(0),
                                                                         P0,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_nested/a/b/srcN") - 1u),
                                                                         static_cast<wasi_posix_fd_t>(4),
                                                                         P1,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_nested/dstN1") - 1u));
        if(r1 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) { ::fast_io::fast_terminate(); }
        write_cu8str32(memory, P1, u8"pl32_nested/dstN2");
        auto const r2 = ::uwvm2::imported::wasi::wasip1::func::path_link(env,
                                                                         static_cast<wasi_posix_fd_t>(3),
                                                                         static_cast<lookupflags_t>(0),
                                                                         P0,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_nested/a/b/srcN") - 1u),
                                                                         static_cast<wasi_posix_fd_t>(4),
                                                                         P1,
                                                                         static_cast<wasi_size_t>(sizeof(u8"pl32_nested/dstN2") - 1u));
        if(r2 != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) { ::fast_io::fast_terminate(); }

        try_unlink(u8"pl32_nested/dstN2");
        try_unlink(u8"pl32_nested/dstN1");
        try_unlink(u8"pl32_nested/a/b/srcN");
        try_rmdir(u8"pl32_nested/a/b");
        try_rmdir(u8"pl32_nested/a");
        try_rmdir(u8"pl32_nested");
    }

    try_unlink(u8"uwvm_ut_pl32_src.txt");
    try_unlink(u8"uwvm_ut_pl32_dst_ok.txt");
}

// NOLINTEND


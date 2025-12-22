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

#include <uwvm2/imported/wasi/wasip1/func/path_readlink.h>
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
// NOLINTBEGIN

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
{
    write_bytes32(memory, p, s, ::std::char_traits<char8_t>::length(s));
}

inline static void set_dirfd(wasip1_environment<native_memory_t>& env, ::std::size_t idx, rights_t base_rights)
{
    auto& fd = *env.fd_storage.opens.index_unchecked(idx).fd_p;
    fd.rights_base = base_rights;
    fd.rights_inherit = static_cast<rights_t>(-1);
    fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
    auto& ds = fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
    ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
    entry.ptr->dir_stack.storage.file = ::fast_io::dir_file{u8"."};
    ds.dir_stack.push_back(::std::move(entry));
}

inline static void set_filefd(wasip1_environment<native_memory_t>& env, ::std::size_t idx, rights_t base_rights)
{
    auto& fd = *env.fd_storage.opens.index_unchecked(idx).fd_p;
    fd.rights_base = base_rights;
    fd.rights_inherit = static_cast<rights_t>(-1);
    fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
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
    memory.init_by_page_count(6uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {.fd_limit = 64uz},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    // Prepare FD table
    env.fd_storage.opens.resize(32uz);

    // Constants: memory offsets
    constexpr wasi_void_ptr_t P_PATH0{1024u};
    constexpr wasi_void_ptr_t P_PATH1{4096u};
    constexpr wasi_void_ptr_t P_BUF{12288u};
    constexpr wasi_void_ptr_t P_USED{16384u};

    // ===== Case 0: negative fd -> ebadf =====
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(-1),
                                                                              static_cast<wasi_void_ptr_t>(0u),
                                                                              static_cast<wasi_size_t>(0u),
                                                                              static_cast<wasi_void_ptr_t>(0u),
                                                                              static_cast<wasi_size_t>(0u),
                                                                              static_cast<wasi_void_ptr_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf)
        {
            ::fast_io::io::perrln("error: prl32 Case 0 expected ebadf. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Setup a valid directory fd at index 3
    set_dirfd(env, 3uz, static_cast<rights_t>(-1));

    // ===== Case 1: rights missing -> enotcapable =====
    {
        set_dirfd(env, 4uz, static_cast<rights_t>(0));
        constexpr auto s = u8"any";
        write_cu8str32(memory, P_PATH0, s);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(4),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable)
        {
            ::fast_io::io::perrln("error: prl32 Case 1 expected enotcapable. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 2: fd is file -> enotdir =====
    {
        set_filefd(env, 5uz, static_cast<rights_t>(-1));
        constexpr auto s = u8"any";
        write_cu8str32(memory, P_PATH0, s);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(5),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir)
        {
            ::fast_io::io::perrln("error: prl32 Case 2 expected enotdir. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 3: empty path -> einval =====
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(0u),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: prl32 Case 3 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 4: absolute path -> enotcapable =====
    {
        constexpr auto s = u8"/abs";
        write_cu8str32(memory, P_PATH0, s);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable)
        {
            ::fast_io::io::perrln("error: prl32 Case 4 expected enotcapable. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 5: target is not a symlink -> einval =====
    {
        try
        {
            ::fast_io::native_file f{u8"uwvm_ut_prl32_nonsymlink", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
            ::std::byte const b{'x'};
            ::fast_io::operations::write_all_bytes(f, &b, &b + 1);
        }
        catch(::fast_io::error)
        {
        }

        constexpr auto s = u8"uwvm_ut_prl32_nonsymlink";
        write_cu8str32(memory, P_PATH0, s);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: prl32 Case 5 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        try_unlink(u8"uwvm_ut_prl32_nonsymlink");
    }

    // ===== Case 6: success read symlink, buffer sufficiently large =====
#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x600) || defined(__MSDOS__) || defined(__DJGPP__))
    {
        try_unlink(u8"uwvm_ut_prl32_link");
        try
        {
            ::fast_io::native_symlinkat(u8"uwvm_prl32_dst", ::fast_io::at_fdcwd(), u8"uwvm_ut_prl32_link");
        }
        catch(::fast_io::error)
        {
            // ignore if already exists
        }

        constexpr auto s = u8"uwvm_ut_prl32_link";
        write_cu8str32(memory, P_PATH0, s);

        // clear output region
        ::std::memset(memory.memory_begin + P_BUF, 0, 64);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("error: prl32 Case 6 expected esuccess. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        auto const used =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(memory, P_USED);
        constexpr char8_t expected[] = u8"uwvm_prl32_dst";
        if(used != sizeof(expected) - 1u)
        {
            ::fast_io::io::perrln("error: prl32 Case 6 buf_used mismatch. ", static_cast<unsigned long long>(used));
            ::fast_io::fast_terminate();
        }
        if(::std::memcmp(memory.memory_begin + P_BUF, expected, sizeof(expected) - 1u) != 0)
        {
            ::fast_io::io::perrln("error: prl32 Case 6 buffer content mismatch");
            ::fast_io::fast_terminate();
        }

        try_unlink(u8"uwvm_ut_prl32_link");
    }
#endif

    // ===== Case 7: enobufs when buffer too small =====
#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x600) || defined(__MSDOS__) || defined(__DJGPP__))
    {
        try_unlink(u8"uwvm_ut_prl32_link2");
        try
        {
            ::fast_io::native_symlinkat(u8"abcdef", ::fast_io::at_fdcwd(), u8"uwvm_ut_prl32_link2");
        }
        catch(::fast_io::error)
        {
        }

        constexpr auto s = u8"uwvm_ut_prl32_link2";
        write_cu8str32(memory, P_PATH0, s);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(4u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enobufs)
        {
            ::fast_io::io::perrln("error: prl32 Case 7 expected enobufs. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        try_unlink(u8"uwvm_ut_prl32_link2");
    }
#endif

    // ===== Case 8: invalid UTF-8 path -> eilseq =====
    {
        unsigned char const bad_utf8[2]{0xC3u, 0x28u};  // invalid sequence
        write_bytes32(memory, P_PATH1, bad_utf8, 2);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH1,
                                                                              static_cast<wasi_size_t>(2u),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eilseq)
        {
            ::fast_io::io::perrln("error: prl32 Case 8 expected eilseq. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // ===== Case 9: traversal escape via symlink in middle -> enotcapable =====
#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x600) || defined(__MSDOS__) || defined(__DJGPP__))
    {
        try_unlink(u8"uwvm_ut_prl32_upup");
        try
        {
            ::fast_io::native_symlinkat(u8"../..", ::fast_io::at_fdcwd(), u8"uwvm_ut_prl32_upup");
        }
        catch(::fast_io::error)
        {
        }

        constexpr auto s = u8"uwvm_ut_prl32_upup/x";
        write_cu8str32(memory, P_PATH0, s);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable)
        {
            ::fast_io::io::perrln("error: prl32 Case 9 expected enotcapable. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        try_unlink(u8"uwvm_ut_prl32_upup");
    }
#endif

    // ===== Case 9b: symlink loop in middle -> eloop =====
#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x600) || defined(__MSDOS__) || defined(__DJGPP__))
    {
        try_unlink(u8"uwvm_ut_prl32_loopmid");
        try
        {
            ::fast_io::native_symlinkat(u8"uwvm_ut_prl32_loopmid", ::fast_io::at_fdcwd(), u8"uwvm_ut_prl32_loopmid");
        }
        catch(::fast_io::error)
        {
        }

        constexpr auto s = u8"uwvm_ut_prl32_loopmid/x";
        write_cu8str32(memory, P_PATH0, s);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop)
        {
            ::fast_io::io::perrln("error: prl32 Case 9b expected eloop. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        try_unlink(u8"uwvm_ut_prl32_loopmid");
    }
#endif

    // ===== Case 10: last component '.' -> einval =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"prl32_dot_a");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"prl32_dot_a/b");
        }
        catch(::fast_io::error)
        {
        }

        constexpr auto s = u8"prl32_dot_a/b/.";
        write_cu8str32(memory, P_PATH0, s);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: prl32 Case 10 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        try_rmdir(u8"prl32_dot_a/b");
        try_rmdir(u8"prl32_dot_a");
    }

    // ===== Case 11: last component '..' after pushing -> einval =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"prl32_lastprev_a");
        }
        catch(::fast_io::error)
        {
        }

        constexpr auto s = u8"prl32_lastprev_a/..";
        write_cu8str32(memory, P_PATH0, s);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval)
        {
            ::fast_io::io::perrln("error: prl32 Case 11 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        try_rmdir(u8"prl32_lastprev_a");
    }

    // ===== Case 12: leading "../x" -> enotcapable =====
    {
        constexpr auto s = u8"../x";
        write_cu8str32(memory, P_PATH0, s);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable)
        {
            ::fast_io::io::perrln("error: prl32 Case 12 expected enotcapable. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x600) || defined(__MSDOS__) || defined(__DJGPP__))
    // ===== Case 13: middle symlink to dir, final is symlink in that dir -> esuccess + content =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"prl32_symdir_a");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"prl32_symdir_a", ::fast_io::at_fdcwd(), u8"prl32_link_to_a");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"TARGET_A", ::fast_io::at_fdcwd(), u8"prl32_symdir_a/inner_symlink");
        }
        catch(::fast_io::error)
        {
        }

        constexpr auto s = u8"prl32_link_to_a/inner_symlink";
        write_cu8str32(memory, P_PATH0, s);
        ::std::memset(memory.memory_begin + P_BUF, 0, 64);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("error: prl32 Case 13 expected esuccess. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        auto const used =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(memory, P_USED);
        constexpr char8_t expected[] = u8"TARGET_A";
        if(used != sizeof(expected) - 1u || ::std::memcmp(memory.memory_begin + P_BUF, expected, sizeof(expected) - 1u) != 0)
        {
            ::fast_io::io::perrln("error: prl32 Case 13 buffer mismatch");
            ::fast_io::fast_terminate();
        }

        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"prl32_symdir_a/inner_symlink", {});
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"prl32_link_to_a", {});
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"prl32_symdir_a", ::fast_io::native_at_flags::removedir);
        }
        catch(::fast_io::error)
        {
        }
    }

    // ===== Case 14: middle symlink to a file (not dir) -> enotdir =====
    {
        try
        {
            ::fast_io::native_file f{u8"prl32_file_mid", ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
            (void)f;
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"prl32_file_mid", ::fast_io::at_fdcwd(), u8"prl32_link_filemid");
        }
        catch(::fast_io::error)
        {
        }

        constexpr auto s = u8"prl32_link_filemid/x";
        write_cu8str32(memory, P_PATH0, s);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir)
        {
            ::fast_io::io::perrln("error: prl32 Case 14 expected enotdir. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        try_unlink(u8"prl32_link_filemid");
        try_unlink(u8"prl32_file_mid");
    }

    // ===== Case 15: middle symlink to absolute path -> enotcapable =====
    {
        try_unlink(u8"prl32_abs_mid");
        try
        {
            ::fast_io::native_symlinkat(u8"/etc", ::fast_io::at_fdcwd(), u8"prl32_abs_mid");
        }
        catch(::fast_io::error)
        {
        }
        constexpr auto s = u8"prl32_abs_mid/x";
        write_cu8str32(memory, P_PATH0, s);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable)
        {
            ::fast_io::io::perrln("error: prl32 Case 15 expected enotcapable. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        try_unlink(u8"prl32_abs_mid");
    }

    // ===== Case 16: middle two-node symlink loop A<->B -> eloop =====
    {
        try_unlink(u8"prl32_loopA");
        try_unlink(u8"prl32_loopB");
        try
        {
            ::fast_io::native_symlinkat(u8"prl32_loopB", ::fast_io::at_fdcwd(), u8"prl32_loopA");
            ::fast_io::native_symlinkat(u8"prl32_loopA", ::fast_io::at_fdcwd(), u8"prl32_loopB");
        }
        catch(::fast_io::error)
        {
        }
        constexpr auto s = u8"prl32_loopA/x";
        write_cu8str32(memory, P_PATH0, s);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop)
        {
            ::fast_io::io::perrln("error: prl32 Case 16 expected eloop. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        try_unlink(u8"prl32_loopA");
        try_unlink(u8"prl32_loopB");
    }

    // ===== Case 17: chain length > max_symlink_depth -> eloop =====
    {
        // Build a chain prl32_c01 -> prl32_c02 -> ... -> prl32_c45
        char8_t namebuf[32]{};
        for(int i = 45; i >= 2; --i)
        {
            // prl32_cXX
            ::std::snprintf(reinterpret_cast<char*>(namebuf), sizeof(namebuf), "prl32_c%02d", i - 1);
            ::uwvm2::utils::container::u8string link_from{reinterpret_cast<char8_t*>(namebuf),
                                                          reinterpret_cast<char8_t*>(namebuf) + ::fast_io::cstr_nlen(namebuf, 32)};
            ::std::snprintf(reinterpret_cast<char*>(namebuf), sizeof(namebuf), "prl32_c%02d", i);
            ::uwvm2::utils::container::u8string link_to{reinterpret_cast<char8_t*>(namebuf),
                                                        reinterpret_cast<char8_t*>(namebuf) + ::fast_io::cstr_nlen(namebuf, 32)};
            try
            {
                ::fast_io::native_symlinkat(link_to, ::fast_io::at_fdcwd(), link_from);
            }
            catch(::fast_io::error)
            {
            }
        }
        constexpr auto s = u8"prl32_c01/x";
        write_cu8str32(memory, P_PATH0, s);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop)
        {
            ::fast_io::io::perrln("error: prl32 Case 17 expected eloop. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        // cleanup
        for(int i = 1; i <= 44; ++i)
        {
            ::std::snprintf(reinterpret_cast<char*>(namebuf), sizeof(namebuf), "prl32_c%02d", i);
            try_unlink(reinterpret_cast<char8_t const*>(namebuf));
        }
        try_unlink(u8"prl32_c45");
    }

    // ===== Case 18: final symlink points to absolute path -> esuccess and return absolute content =====
    {
        try_unlink(u8"prl32_abs_final");
        try
        {
            ::fast_io::native_symlinkat(u8"/etc", ::fast_io::at_fdcwd(), u8"prl32_abs_final");
        }
        catch(::fast_io::error)
        {
        }

        constexpr auto s = u8"prl32_abs_final";
        write_cu8str32(memory, P_PATH0, s);
        ::std::memset(memory.memory_begin + P_BUF, 0, 64);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("error: prl32 Case 18 expected esuccess. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        constexpr char8_t expected[] = u8"/etc";
        auto const used =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(memory, P_USED);
        if(used != sizeof(expected) - 1u || ::std::memcmp(memory.memory_begin + P_BUF, expected, sizeof(expected) - 1u) != 0)
        {
            ::fast_io::io::perrln("error: prl32 Case 18 buffer mismatch");
            ::fast_io::fast_terminate();
        }
        try_unlink(u8"prl32_abs_final");
    }

    // ===== Case 19: final symlink length exactly equals buf_len -> esuccess =====
    {
        try_unlink(u8"prl32_exact_final");
        try
        {
            ::fast_io::native_symlinkat(u8"ABCD", ::fast_io::at_fdcwd(), u8"prl32_exact_final");
        }
        catch(::fast_io::error)
        {
        }
        constexpr auto s = u8"prl32_exact_final";
        write_cu8str32(memory, P_PATH0, s);
        ::std::memset(memory.memory_begin + P_BUF, 0, 64);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(4u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("error: prl32 Case 19 expected esuccess. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        constexpr char8_t expected[] = u8"ABCD";
        auto const used =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(memory, P_USED);
        if(used != sizeof(expected) - 1u || ::std::memcmp(memory.memory_begin + P_BUF, expected, sizeof(expected) - 1u) != 0)
        {
            ::fast_io::io::perrln("error: prl32 Case 19 buffer mismatch");
            ::fast_io::fast_terminate();
        }
        try_unlink(u8"prl32_exact_final");
    }

    // ===== Case 20: 0-sized buffer for non-empty final symlink -> enobufs =====
    {
        try_unlink(u8"prl32_smallbuf_final");
        try
        {
            ::fast_io::native_symlinkat(u8"Z", ::fast_io::at_fdcwd(), u8"prl32_smallbuf_final");
        }
        catch(::fast_io::error)
        {
        }
        constexpr auto s = u8"prl32_smallbuf_final";
        write_cu8str32(memory, P_PATH0, s);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(0u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::enobufs)
        {
            ::fast_io::io::perrln("error: prl32 Case 20 expected enobufs. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        try_unlink(u8"prl32_smallbuf_final");
    }

    // ===== Case 21: middle symlink to '.' should be no-op and continue =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"prl32_dotd");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8".", ::fast_io::at_fdcwd(), u8"prl32_dotd/dot");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"DOTTARGET", ::fast_io::at_fdcwd(), u8"prl32_dotd/inner");
        }
        catch(::fast_io::error)
        {
        }
        constexpr auto s = u8"prl32_dotd/dot/inner";
        write_cu8str32(memory, P_PATH0, s);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("error: prl32 Case 21 expected esuccess. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        constexpr char8_t expected[] = u8"DOTTARGET";
        auto const used =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(memory, P_USED);
        if(used != sizeof(expected) - 1u || ::std::memcmp(memory.memory_begin + P_BUF, expected, sizeof(expected) - 1u) != 0)
        {
            ::fast_io::io::perrln("error: prl32 Case 21 buffer mismatch");
            ::fast_io::fast_terminate();
        }
        try_unlink(u8"prl32_dotd/inner");
        try_unlink(u8"prl32_dotd/dot");
        try_rmdir(u8"prl32_dotd");
    }

    // ===== Case 22: middle symlink to '..' should pop and continue =====
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"prl32_pop_a");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"prl32_pop_a/b");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"..", ::fast_io::at_fdcwd(), u8"prl32_pop_a/b/up");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"POPTGT", ::fast_io::at_fdcwd(), u8"prl32_pop_a/inner");
        }
        catch(::fast_io::error)
        {
        }
        constexpr auto s = u8"prl32_pop_a/b/up/inner";
        write_cu8str32(memory, P_PATH0, s);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_readlink(env,
                                                                              static_cast<wasi_posix_fd_t>(3),
                                                                              P_PATH0,
                                                                              static_cast<wasi_size_t>(::std::char_traits<char8_t>::length(s)),
                                                                              P_BUF,
                                                                              static_cast<wasi_size_t>(64u),
                                                                              P_USED);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess)
        {
            ::fast_io::io::perrln("error: prl32 Case 22 expected esuccess. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
        constexpr char8_t expected[] = u8"POPTGT";
        auto const used =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(memory, P_USED);
        if(used != sizeof(expected) - 1u || ::std::memcmp(memory.memory_begin + P_BUF, expected, sizeof(expected) - 1u) != 0)
        {
            ::fast_io::io::perrln("error: prl32 Case 22 buffer mismatch");
            ::fast_io::fast_terminate();
        }
        try_unlink(u8"prl32_pop_a/inner");
        try_unlink(u8"prl32_pop_a/b/up");
        try_rmdir(u8"prl32_pop_a/b");
        try_rmdir(u8"prl32_pop_a");
    }
#endif

    return 0;
}

// NOLINTEND

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

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/path_remove_directory_wasm64.h>

using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;

inline static void write_path(native_memory_t& memory, wasi_void_ptr_wasm64_t p, char8_t const* s, ::std::size_t n)
{
    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64(memory,
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
    env.fd_storage.opens.resize(10uz);

    // Common: setup a directory fd at index 3 with '.' and full rights
    auto& fd_root = *env.fd_storage.opens.index_unchecked(3uz).fd_p;
    fd_root.rights_base = static_cast<rights_wasm64_t>(-1);
    fd_root.rights_inherit = static_cast<rights_wasm64_t>(-1);
    fd_root.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
    {
        auto& ds = fd_root.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
        ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
        entry.ptr->dir_stack.file = ::fast_io::dir_file{u8"."};
        ds.dir_stack.push_back(::std::move(entry));
    }

    // Case 0: negative fd -> ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env,
                                                                                             static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                             static_cast<wasi_void_ptr_wasm64_t>(0u),
                                                                                             static_cast<wasi_size_wasm64_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln("error: prd64 Case 0 expected ebadf. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: rights missing -> enotcapable
    {
        auto& fd = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fd.rights_base = static_cast<rights_wasm64_t>(0);
        fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
        {
            auto& ds = fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
            ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
            entry.ptr->dir_stack.file = ::fast_io::dir_file{u8"."};
            ds.dir_stack.push_back(::std::move(entry));
        }

        constexpr wasi_void_ptr_wasm64_t p{4096u};
        constexpr auto s = u8"prd64_no_rights";
        write_path(memory, p, s, sizeof(u8"prd64_no_rights") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env,
                                                                                             static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                             p,
                                                                                             static_cast<wasi_size_wasm64_t>(sizeof(u8"prd64_no_rights") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln("error: prd64 Case 1 expected enotcapable. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: fd is file -> enotdir
    {
        auto& fd = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        fd.rights_base = static_cast<rights_wasm64_t>(-1);
        fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        constexpr wasi_void_ptr_wasm64_t p{8192u};
        constexpr auto s = u8"prd64_notdir";
        write_path(memory, p, s, sizeof(u8"prd64_notdir") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env,
                                                                                             static_cast<wasi_posix_fd_wasm64_t>(5),
                                                                                             p,
                                                                                             static_cast<wasi_size_wasm64_t>(sizeof(u8"prd64_notdir") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir)
        {
            ::fast_io::io::perrln("error: prd64 Case 2 expected enotdir. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: absolute path -> eperm
    {
        constexpr wasi_void_ptr_wasm64_t p{12288u};
        constexpr auto s = u8"/abs64_dir";
        write_path(memory, p, s, sizeof(u8"/abs64_dir") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env,
                                                                                             static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                             p,
                                                                                             static_cast<wasi_size_wasm64_t>(sizeof(u8"/abs64_dir") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eperm)
        {
            ::fast_io::io::perrln("error: prd64 Case 3 expected eperm. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: path "." -> einval (cannot remove current directory entry)
    {
        constexpr wasi_void_ptr_wasm64_t p{16384u};
        constexpr auto s = u8".";
        write_path(memory, p, s, sizeof(u8".") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env,
                                                                                             static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                             p,
                                                                                             static_cast<wasi_size_wasm64_t>(sizeof(u8".") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("error: prd64 Case 4 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 5: empty path -> einval
    {
        constexpr wasi_void_ptr_wasm64_t p{18432u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env,
                                                                                             static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                             p,
                                                                                             static_cast<wasi_size_wasm64_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("error: prd64 Case 5 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 6: success remove empty directory
    {
        // prepare directory
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"prd64_ok");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_wasm64_t p{20480u};
        constexpr auto s = u8"prd64_ok";
        write_path(memory, p, s, sizeof(u8"prd64_ok") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env,
                                                                                             static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                             p,
                                                                                             static_cast<wasi_size_wasm64_t>(sizeof(u8"prd64_ok") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("error: prd64 Case 6 expected esuccess. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        // verify removed
        try
        {
            ::fast_io::dir_file df{u8"prd64_ok"};
            ::fast_io::io::perrln("error: prd64 Case 6 directory still exists after remove.");
            ::fast_io::fast_terminate();
        }
        catch(::fast_io::error)
        {
        }
    }

    // Case 7: non-empty directory -> enotempty
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"prd64_nonempty");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_file f{u8"prd64_nonempty/f", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_wasm64_t p{22528u};
        constexpr auto s = u8"prd64_nonempty";
        write_path(memory, p, s, sizeof(u8"prd64_nonempty") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env,
                                                                                             static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                             p,
                                                                                             static_cast<wasi_size_wasm64_t>(sizeof(u8"prd64_nonempty") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotempty)
        {
            ::fast_io::io::perrln("error: prd64 Case 7 expected enotempty. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        // cleanup
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"prd64_nonempty/f", {});
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"prd64_nonempty", ::fast_io::native_at_flags::removedir);
        }
        catch(::fast_io::error)
        {
        }
    }

    // Case 8: not exist -> enoent
    {
        // ensure not exist (ignore failures)
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"prd64_no_such", ::fast_io::native_at_flags::removedir);
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_wasm64_t p{24576u};
        constexpr auto s = u8"prd64_no_such";
        write_path(memory, p, s, sizeof(u8"prd64_no_such") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env,
                                                                                             static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                             p,
                                                                                             static_cast<wasi_size_wasm64_t>(sizeof(u8"prd64_no_such") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enoent)
        {
            ::fast_io::io::perrln("error: prd64 Case 8 expected enoent. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x600) || defined(__MSDOS__) || defined(__DJGPP__))
    // Case 9: symlink intermediate directory -> esuccess
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"prd64_sym_a");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"prd64_sym_a/c");
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"prd64_sym_a", ::fast_io::at_fdcwd(), u8"prd64_link_to_a");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_wasm64_t p{26624u};
        constexpr auto s = u8"prd64_link_to_a/c";
        write_path(memory, p, s, sizeof(u8"prd64_link_to_a/c") - 1u);

        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env,
                                                                                static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                p,
                                                                                static_cast<wasi_size_wasm64_t>(sizeof(u8"prd64_link_to_a/c") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("error: prd64 Case 9 expected esuccess. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        // verify target directory removed
        try
        {
            ::fast_io::dir_file df{u8"prd64_sym_a/c"};
            ::fast_io::io::perrln("error: prd64 Case 9 expected target directory removed.");
            ::fast_io::fast_terminate();
        }
        catch(::fast_io::error)
        {
        }

        // cleanup
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"prd64_link_to_a", {});
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"prd64_sym_a", ::fast_io::native_at_flags::removedir);
        }
        catch(::fast_io::error)
        {
        }
    }

    // Case 10: symlink loop -> eloop
    {
        try
        {
            ::fast_io::native_symlinkat(u8"prd64_loop", ::fast_io::at_fdcwd(), u8"prd64_loop");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_wasm64_t p{28672u};
        constexpr auto s = u8"prd64_loop/x";
        write_path(memory, p, s, sizeof(u8"prd64_loop/x") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env,
                                                                                             static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                             p,
                                                                                             static_cast<wasi_size_wasm64_t>(sizeof(u8"prd64_loop/x") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eloop)
        {
            ::fast_io::io::perrln("error: prd64 Case 10 expected eloop. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"prd64_loop", {});
        }
        catch(::fast_io::error)
        {
        }
    }
#endif

#if (defined(_WIN32) || defined(__CYGWIN__)) || (defined(__MSDOS__) || defined(__DJGPP__))
    // Case 11: backslash in a component -> einval
    {
        constexpr wasi_void_ptr_wasm64_t p{30720u};
        constexpr auto s = u8"prd64_inv_bs_a\\b";
        write_path(memory, p, s, sizeof(u8"prd64_inv_bs_a\\b") - 1u);

        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env,
                                                                                static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                p,
                                                                                static_cast<wasi_size_wasm64_t>(sizeof(u8"prd64_inv_bs_a\\b") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("error: prd64 Case 11 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 12: '*' in name -> einval
    {
        constexpr wasi_void_ptr_wasm64_t p{32768u};
        constexpr auto s = u8"prd64_star_*";
        write_path(memory, p, s, sizeof(u8"prd64_star_*") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(env,
                                                                                             static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                             p,
                                                                                             static_cast<wasi_size_wasm64_t>(sizeof(u8"prd64_star_*") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("error: prd64 Case 12 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
#endif
}

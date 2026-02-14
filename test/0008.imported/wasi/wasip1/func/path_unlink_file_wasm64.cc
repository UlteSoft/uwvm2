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

#include <uwvm2/imported/wasi/wasip1/func/path_unlink_file_wasm64.h>
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

#include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>

#if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_WASM64)
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
        entry.ptr->dir_stack.storage.file = ::fast_io::dir_file{u8"."};
        ds.dir_stack.push_back(::std::move(entry));
    }

    // Case 0: negative fd -> ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env,
                                                                                        static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(0u),
                                                                                        static_cast<wasi_size_wasm64_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln("error: puf64 Case 0 expected ebadf. ", static_cast<unsigned>(ret));
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
            entry.ptr->dir_stack.storage.file = ::fast_io::dir_file{u8"."};
            ds.dir_stack.push_back(::std::move(entry));
        }

        constexpr wasi_void_ptr_wasm64_t p{4096u};
        constexpr auto s = u8"puf64_no_rights";
        write_path(memory, p, s, sizeof(u8"puf64_no_rights") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env,
                                                                                        static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                        p,
                                                                                        static_cast<wasi_size_wasm64_t>(sizeof(u8"puf64_no_rights") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln("error: puf64 Case 1 expected enotcapable. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: fd is file -> enotdir
    {
        auto& fd = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        fd.rights_base = static_cast<rights_wasm64_t>(-1);
        fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        constexpr wasi_void_ptr_wasm64_t p{8192u};
        constexpr auto s = u8"puf64_notdir";
        write_path(memory, p, s, sizeof(u8"puf64_notdir") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env,
                                                                                        static_cast<wasi_posix_fd_wasm64_t>(5),
                                                                                        p,
                                                                                        static_cast<wasi_size_wasm64_t>(sizeof(u8"puf64_notdir") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir)
        {
            ::fast_io::io::perrln("error: puf64 Case 2 expected enotdir. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: absolute path -> enotcapable
    {
        constexpr wasi_void_ptr_wasm64_t p{12288u};
        constexpr auto s = u8"/abs64_file";
        write_path(memory, p, s, sizeof(u8"/abs64_file") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env,
                                                                                        static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                        p,
                                                                                        static_cast<wasi_size_wasm64_t>(sizeof(u8"/abs64_file") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln("error: puf64 Case 3 expected enotcapable. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: path "." -> eisdir (cannot unlink directory entry)
    {
        constexpr wasi_void_ptr_wasm64_t p{16384u};
        constexpr auto s = u8".";
        write_path(memory, p, s, sizeof(u8".") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env,
                                                                                        static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                        p,
                                                                                        static_cast<wasi_size_wasm64_t>(sizeof(u8".") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eisdir)
        {
            ::fast_io::io::perrln("error: puf64 Case 4 expected eisdir. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 5: empty path -> einval
    {
        constexpr wasi_void_ptr_wasm64_t p{18432u};
        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(3), p, static_cast<wasi_size_wasm64_t>(0u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("error: puf64 Case 5 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 6: success unlink regular file
    {
        // prepare file
        try
        {
            ::fast_io::native_file f{u8"puf64_ok", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_wasm64_t p{20480u};
        constexpr auto s = u8"puf64_ok";
        write_path(memory, p, s, sizeof(u8"puf64_ok") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env,
                                                                                        static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                        p,
                                                                                        static_cast<wasi_size_wasm64_t>(sizeof(u8"puf64_ok") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("error: puf64 Case 6 expected esuccess. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        // verify removed
        try
        {
            ::fast_io::native_file f{u8"puf64_ok", ::fast_io::open_mode::in};
            ::fast_io::io::perrln("error: puf64 Case 6 file still exists after unlink.");
            ::fast_io::fast_terminate();
        }
        catch(::fast_io::error)
        {
        }
    }

    // Case 7: target is directory -> eisdir
    {
        try
        {
            ::fast_io::native_mkdirat(::fast_io::at_fdcwd(), u8"puf64_dir", {});
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_wasm64_t p{22528u};
        constexpr auto s = u8"puf64_dir";
        write_path(memory, p, s, sizeof(u8"puf64_dir") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env,
                                                                                        static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                        p,
                                                                                        static_cast<wasi_size_wasm64_t>(sizeof(u8"puf64_dir") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eisdir && ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eperm &&
           ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces && ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enoent)
        {
            ::fast_io::io::perrln("error: puf64 Case 7 expected eisdir or eperm. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        // cleanup
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"puf64_dir", ::fast_io::native_at_flags::removedir);
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
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"puf64_no_such", {});
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_wasm64_t p{24576u};
        constexpr auto s = u8"puf64_no_such";
        write_path(memory, p, s, sizeof(u8"puf64_no_such") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env,
                                                                                        static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                        p,
                                                                                        static_cast<wasi_size_wasm64_t>(sizeof(u8"puf64_no_such") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enoent
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
           && ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces
#endif
        )
        {
            ::fast_io::io::perrln("error: puf64 Case 8 expected enoent. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

#if !(defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x600) || defined(__MSDOS__) || defined(__DJGPP__))
    // Case 9: symlink to file -> esuccess
    {
        try
        {
            ::fast_io::native_file f{u8"puf64_sym_target", ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        }
        catch(::fast_io::error)
        {
        }
        try
        {
            ::fast_io::native_symlinkat(u8"puf64_sym_target", ::fast_io::at_fdcwd(), u8"puf64_link_to_file");
        }
        catch(::fast_io::error)
        {
        }

        constexpr wasi_void_ptr_wasm64_t p{26624u};
        constexpr auto s = u8"puf64_link_to_file";
        write_path(memory, p, s, sizeof(u8"puf64_link_to_file") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env,
                                                                                        static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                        p,
                                                                                        static_cast<wasi_size_wasm64_t>(sizeof(u8"puf64_link_to_file") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln("error: puf64 Case 9 expected esuccess. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }

        // verify target still exists (unlink_file should remove link, not target)
        try
        {
            ::fast_io::native_file f{u8"puf64_sym_target", ::fast_io::open_mode::in};
        }
        catch(::fast_io::error)
        {
            ::fast_io::io::perrln("error: puf64 Case 9 target removed unexpectedly.");
            ::fast_io::fast_terminate();
        }

        // cleanup
        try
        {
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(), u8"puf64_sym_target", {});
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
        constexpr auto s = u8"puf64_inv_bs_a\\b";
        write_path(memory, p, s, sizeof(u8"puf64_inv_bs_a\\b") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env,
                                                                                        static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                        p,
                                                                                        static_cast<wasi_size_wasm64_t>(sizeof(u8"puf64_inv_bs_a\\b") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("error: puf64 Case 11 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }

    // Case 12: '*' in name -> einval
    {
        constexpr wasi_void_ptr_wasm64_t p{32768u};
        constexpr auto s = u8"puf64_star_*";
        write_path(memory, p, s, sizeof(u8"puf64_star_*") - 1u);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(env,
                                                                                        static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                        p,
                                                                                        static_cast<wasi_size_wasm64_t>(sizeof(u8"puf64_star_*") - 1u));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln("error: puf64 Case 12 expected einval. ", static_cast<unsigned>(ret));
            ::fast_io::fast_terminate();
        }
    }
#endif
}

#else

int main() {}

#endif

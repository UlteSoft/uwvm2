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

#include <cstddef>
#include <cstdint>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_filestat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/posix.h>
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
#if defined(_WIN32) && !defined(__CYGWIN__)

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::filetype_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
    using ::uwvm2::object::memory::linear::native_memory_t;

    native_memory_t memory{};
    memory.init_by_page_count(1uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    env.fd_storage.opens.resize(6uz);

    // Case 0: negative fd → ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get_wasm64(env,
                                                                                       static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                       static_cast<wasi_void_ptr_wasm64_t>(1024u));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_get_wasm64_win32: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: success for regular file; check filetype and nonnegative size
    {
        auto& fd4 = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fd4.rights_base = static_cast<rights_wasm64_t>(-1);
        fd4.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fd4.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fd4.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
# if defined(_WIN32) && !defined(__CYGWIN__)
            .file
# endif
            = ::fast_io::native_file{u8"test_fd_filestat_get_wasm64_win32_regular.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
    }
    {
        constexpr wasi_void_ptr_wasm64_t stat_ptr{2048u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(4), stat_ptr);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_get_wasm64_win32: expected esuccess for regular file");
            ::fast_io::fast_terminate();
        }

        auto const ft =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<std::underlying_type_t<filetype_wasm64_t>>(memory, stat_ptr + 16u);
        if(ft != static_cast<std::underlying_type_t<filetype_wasm64_t>>(filetype_wasm64_t::filetype_regular_file))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_get_wasm64_win32: filetype should be regular");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: ebadf after fd_close
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(2uz).fd_p;
        fde.rights_base = static_cast<rights_wasm64_t>(-1);
        fde.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
# if defined(_WIN32) && !defined(__CYGWIN__)
            .file
# endif
            = ::fast_io::native_file{u8"test_fd_filestat_get_wasm64_win32_close.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};

        auto const closed = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2));
        if(closed != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_get_wasm64_win32: close expected esuccess");
            ::fast_io::fast_terminate();
        }

        constexpr wasi_void_ptr_wasm64_t stat_ptr{6144u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2), stat_ptr);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_get_wasm64_win32: expected ebadf after fd_close");
            ::fast_io::fast_terminate();
        }
    }
}

#else

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::filetype_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
    using ::uwvm2::object::memory::linear::native_memory_t;

    native_memory_t memory{};
    memory.init_by_page_count(1uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    env.fd_storage.opens.resize(6uz);

    // Case 0: negative fd → ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get_wasm64(env,
                                                                                       static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                       static_cast<wasi_void_ptr_wasm64_t>(1024u));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_get_wasm64: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Prepare a regular file at fd=4
    {
        auto& fd4 = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fd4.rights_base = static_cast<rights_wasm64_t>(-1);
        fd4.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fd4.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fd4.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
# if defined(_WIN32) && !defined(__CYGWIN__)
            .file
# endif
            = ::fast_io::native_file{u8"test_fd_filestat_get_wasm64_regular.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
    }

    // Case 1: success for regular file; check filetype and size
    {
        constexpr wasi_void_ptr_wasm64_t stat_ptr{2048u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(4), stat_ptr);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_get_wasm64: expected esuccess for regular file");
            ::fast_io::fast_terminate();
        }

        auto const ft =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<std::underlying_type_t<filetype_wasm64_t>>(memory, stat_ptr + 16u);
        if(ft != static_cast<std::underlying_type_t<filetype_wasm64_t>>(filetype_wasm64_t::filetype_regular_file))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_get_wasm64: filetype should be regular");
            ::fast_io::fast_terminate();
        }

        auto const sz = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<
            std::underlying_type_t<decltype(::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t{})>>(memory, stat_ptr + 32u);
        (void)sz;
    }

    // Case 3: ebadf after fd_close
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(2uz).fd_p;
        fde.rights_base = static_cast<rights_wasm64_t>(-1);
        fde.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
# if defined(_WIN32) && !defined(__CYGWIN__)
            .file
# endif
            = ::fast_io::native_file{u8"test_fd_filestat_get_wasm64_close.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};

        auto const closed = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2));
        if(closed != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_get_wasm64: close expected esuccess");
            ::fast_io::fast_terminate();
        }

        constexpr wasi_void_ptr_wasm64_t stat_ptr{6144u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2), stat_ptr);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_get_wasm64: expected ebadf after fd_close");
            ::fast_io::fast_terminate();
        }
    }
}

#endif

#else

int main() {}

#endif

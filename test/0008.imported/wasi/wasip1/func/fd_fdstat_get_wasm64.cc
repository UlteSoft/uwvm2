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

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/posix.h>
#include <uwvm2/imported/wasi/wasip1/fd_manager/fd.h>
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

    // Ensure several fd entries exist
    env.fd_storage.opens.resize(6uz);

    // Case 0: negative fd → ebadf (wasm64)
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get_wasm64(env,
                                                                                     static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                     static_cast<wasi_void_ptr_wasm64_t>(1024u));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get_win32: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: success for regular file; check filetype only (Windows does not use fcntl flags here)
    {
        auto& fde4 = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fde4.rights_base = static_cast<rights_wasm64_t>(-1);
        fde4.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fde4.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde4.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
# if defined(_WIN32) && !defined(__CYGWIN__)
            .file
# endif
            = ::fast_io::native_file{u8"test_fd_fdstat_get_win32_regular.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
    }
    {
        constexpr wasi_void_ptr_wasm64_t stat_ptr{2048u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(4), stat_ptr);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get_win32: expected esuccess for regular file");
            ::fast_io::fast_terminate();
        }

        auto const ft =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<std::underlying_type_t<filetype_wasm64_t>>(memory, stat_ptr);
        if(ft != static_cast<std::underlying_type_t<filetype_wasm64_t>>(filetype_wasm64_t::filetype_regular_file))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get_win32: filetype should be regular");
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
            = ::fast_io::native_file{u8"test_fd_fdstat_get_win32_close.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};

        auto const closed = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2));
        if(closed != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get_win32: close expected esuccess");
            ::fast_io::fast_terminate();
        }

        constexpr wasi_void_ptr_wasm64_t stat_ptr{6144u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2), stat_ptr);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get_win32: expected ebadf after fd_close");
            ::fast_io::fast_terminate();
        }
    }
}

#else
int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t;
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
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get_wasm64(env,
                                                                                     static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                     static_cast<wasi_void_ptr_wasm64_t>(1024u));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get_wasm64: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Prepare a regular file at fd=4 with O_APPEND to test flags and type
    env.fd_storage.opens.index_unchecked(4uz).fd_p->rights_base = static_cast<rights_wasm64_t>(-1);
    env.fd_storage.opens.index_unchecked(4uz).fd_p->rights_inherit = static_cast<rights_wasm64_t>(-1);
    {
        auto& fde4 = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fde4.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde4.wasi_fd.ptr->wasi_fd_storage.storage.file_fd =
            ::fast_io::native_file{u8"test_fd_fdstat_get_wasm64_regular.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
    }
    {
        int const fd_native = env.fd_storage.opens.index_unchecked(4uz).fd_p->wasi_fd.ptr->wasi_fd_storage.storage.file_fd.native_handle();
        int const curr = ::uwvm2::imported::wasi::wasip1::func::posix::fcntl(fd_native, F_GETFL);
        if(curr == -1)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get_wasm64: F_GETFL failed");
            ::fast_io::fast_terminate();
        }
        if(::uwvm2::imported::wasi::wasip1::func::posix::fcntl(fd_native, F_SETFL, curr | O_APPEND) == -1)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get_wasm64: F_SETFL O_APPEND failed");
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: success for regular file; check filetype and fdflag_append
    {
        constexpr wasi_void_ptr_wasm64_t stat_ptr{2048u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(4), stat_ptr);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get_wasm64: expected esuccess for regular file");
            ::fast_io::fast_terminate();
        }

        auto const ft =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<std::underlying_type_t<filetype_wasm64_t>>(memory, stat_ptr);
        auto const flags = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<std::underlying_type_t<fdflags_wasm64_t>>(
            memory,
            static_cast<wasi_void_ptr_wasm64_t>(stat_ptr + 2u));

        if(ft != static_cast<std::underlying_type_t<filetype_wasm64_t>>(filetype_wasm64_t::filetype_regular_file))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get_wasm64: filetype should be regular");
            ::fast_io::fast_terminate();
        }

        if((flags & static_cast<std::underlying_type_t<fdflags_wasm64_t>>(fdflags_wasm64_t::fdflag_append)) == 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get_wasm64: fdflag_append should be set");
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
            = ::fast_io::native_file{u8"test_fd_fdstat_get_wasm64_close.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};

        auto const closed = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2));
        if(closed != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get_wasm64: close expected esuccess");
            ::fast_io::fast_terminate();
        }

        constexpr wasi_void_ptr_wasm64_t stat_ptr{6144u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2), stat_ptr);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_get_wasm64: expected ebadf after fd_close");
            ::fast_io::fast_terminate();
        }
    }
}

#endif

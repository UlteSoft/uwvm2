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

#include <uwvm2/imported/wasi/wasip1/func/fd_sync_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close_wasm64.h>

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
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

    // Prepare fd table: ensure indices [0..4] exist with valid entries
    env.fd_storage.opens.resize(5uz);

    // Case 1: enotcapable when rights do not include right_fd_sync
    {
        auto& fd1 = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fd1.rights_base = static_cast<rights_t>(0);
        fd1.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fd1.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_sync64.tmp", ::fast_io::open_mode::out};

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_sync_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(4));
        if(ret != errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_sync_wasm64: expected enotcapable when rights missing");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: esuccess when rights ok and fd valid
    {
        auto& fd3 = *env.fd_storage.opens.index_unchecked(3uz).fd_p;
        fd3.rights_base = static_cast<rights_t>(-1);
        fd3.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fd3.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_sync64_ok.tmp", ::fast_io::open_mode::out};

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_sync_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(3));
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_sync_wasm64: expected esuccess with valid rights and handle");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: ebadf for negative fd
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_sync_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(-1));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_sync_wasm64: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: ebadf after fd has been closed
    {
        auto& fd2 = *env.fd_storage.opens.index_unchecked(2uz).fd_p;
        fd2.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fd2.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_close_then_sync64.tmp", ::fast_io::open_mode::out};
        fd2.rights_base = static_cast<rights_t>(-1);

        auto const closed = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2));
        if(closed != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_sync_wasm64: close before sync expected esuccess");
            ::fast_io::fast_terminate();
        }

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_sync_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_sync_wasm64: expected ebadf after fd_close");
            ::fast_io::fast_terminate();
        }
    }
}


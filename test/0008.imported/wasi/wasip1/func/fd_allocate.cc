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

#include <uwvm2/imported/wasi/wasip1/func/fd_allocate.h>

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_t;
    using ::uwvm2::imported::wasi::wasip1::abi::filesize_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
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

    // Case 1: esuccess when len == 0 and rights ok
    {
        // ensure target fd has a valid native handle (not required for len==0 path, but keeps behavior uniform)
        env.fd_storage.opens.index_unchecked(3uz).fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        env.fd_storage.opens.index_unchecked(3uz)
            .fd_p->wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_allocate.log", ::fast_io::open_mode::out};
        env.fd_storage.opens.index_unchecked(3uz).fd_p->rights_base = static_cast<rights_t>(-1);
        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::fd_allocate(env, static_cast<wasi_posix_fd_t>(3), static_cast<filesize_t>(0), static_cast<filesize_t>(0));
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_allocate: expected esuccess when len==0");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: enotcapable when rights do not include right_fd_allocate
    {
        env.fd_storage.opens.index_unchecked(4uz).fd_p->rights_base = static_cast<rights_t>(0);
        env.fd_storage.opens.index_unchecked(4uz).fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        // ensure the target fd has a valid native handle
        env.fd_storage.opens.index_unchecked(4uz)
            .fd_p->wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_allocate.log", ::fast_io::open_mode::out};

        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::fd_allocate(env, static_cast<wasi_posix_fd_t>(4), static_cast<filesize_t>(0), static_cast<filesize_t>(0));
        if(ret != errno_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_allocate: expected enotcapable when rights missing");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: ebadf for negative fd
    {
        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::fd_allocate(env, static_cast<wasi_posix_fd_t>(-1), static_cast<filesize_t>(0), static_cast<filesize_t>(0));
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_allocate: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }
}


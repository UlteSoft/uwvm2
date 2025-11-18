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

#include <uwvm2/imported/wasi/wasip1/func/fd_filestat_set_size.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close.h>

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

    // Prepare fd table
    env.fd_storage.opens.resize(6uz);

    // Case 0: negative fd → ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_size(env, static_cast<wasi_posix_fd_t>(-1), static_cast<filesize_t>(0));
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_size: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Prepare a regular file at fd=4 (new fd model)
    {
        auto& fd4 = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fd4.rights_base = static_cast<rights_t>(-1);
        fd4.rights_inherit = static_cast<rights_t>(-1);
        fd4.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fd4.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_filestat_set_size_regular.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
    }

    // Case 1: success to extend file to 4096
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_size(env, static_cast<wasi_posix_fd_t>(4), static_cast<filesize_t>(4096u));
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_size: expected esuccess for regular file");
            ::fast_io::fast_terminate();
        }

        // verify physical size using fast_io::status
        auto const& nf = env.fd_storage.opens.index_unchecked(4uz)
                             .fd_p->wasi_fd.ptr->wasi_fd_storage.storage
                             .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                             .file
#endif
            ;
        auto const st = status(nf);
        if(st.size != 4096u)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_size: file size should be 4096");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: enotcapable when rights do not include right_fd_filestat_set_size
    {
        auto& fd5 = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        fd5.rights_base = static_cast<rights_t>(0);
        fd5.rights_inherit = static_cast<rights_t>(0);
        fd5.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fd5.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_filestat_set_size_rights.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_size(env, static_cast<wasi_posix_fd_t>(5), static_cast<filesize_t>(1024u));
        if(ret != errno_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_size: expected enotcapable when rights missing");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: ebadf after fd_close
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(2uz).fd_p;
        fde.rights_base = static_cast<rights_t>(-1);
        fde.rights_inherit = static_cast<rights_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_filestat_set_size_close.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};

        auto const closed = ::uwvm2::imported::wasi::wasip1::func::fd_close(env, static_cast<wasi_posix_fd_t>(2));
        if(closed != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_size: close expected esuccess");
            ::fast_io::fast_terminate();
        }

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_size(env, static_cast<wasi_posix_fd_t>(2), static_cast<filesize_t>(2048u));
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_size: expected ebadf after fd_close");
            ::fast_io::fast_terminate();
        }
    }
}


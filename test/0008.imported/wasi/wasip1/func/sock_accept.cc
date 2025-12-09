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
#include <cstdint>

#include <fast_io.h>

#if (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32)

#include <uwvm2/imported/wasi/wasip1/func/sock_accept.h>

#if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_t;
    using ::uwvm2::imported::wasi::wasip1::abi::fdflags_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t;
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

    // Prepare fd table for all test cases
    env.fd_storage.opens.resize(4uz);

    constexpr wasi_void_ptr_t FD_PTR{1024u};
    constexpr wasi_void_ptr_t ADDR_PTR{2048u};

    // Case 0: negative fd -> ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_accept(env,
                                                                            static_cast<wasi_posix_fd_t>(-1),
                                                                            static_cast<fdflags_t>(0),
                                                                            FD_PTR,
                                                                            ADDR_PTR);
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: closed descriptor (close_pos != SIZE_MAX) -> ebadf
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(1uz).fd_p;
        fde.close_pos = 0u;

        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::sock_accept(env, static_cast<wasi_posix_fd_t>(1), static_cast<fdflags_t>(0), FD_PTR, ADDR_PTR);
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept: expected ebadf for closed descriptor");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: missing right_sock_accept -> enotcapable
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(2uz).fd_p;
        fde.close_pos = static_cast<std::size_t>(-1);
        fde.rights_base = static_cast<rights_t>(0);
        fde.rights_inherit = static_cast<rights_t>(0);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::sock_accept(env, static_cast<wasi_posix_fd_t>(2), static_cast<fdflags_t>(0), FD_PTR, ADDR_PTR);
        if(ret != errno_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept: expected enotcapable when right_sock_accept missing");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: fd_flags contains unsupported bits -> einval
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(3uz).fd_p;
        fde.close_pos = static_cast<std::size_t>(-1);
        fde.rights_base = ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_sock_accept;
        fde.rights_inherit = ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_sock_accept;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        auto const invalid_flags = fdflags_t::fdflag_append;

        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::sock_accept(env, static_cast<wasi_posix_fd_t>(3), invalid_flags, FD_PTR, ADDR_PTR);
        if(ret != errno_t::einval)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept: expected einval for unsupported fdflags");
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: descriptor type is dir with proper rights -> enotsock
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(0uz).fd_p;
        fde.close_pos = static_cast<std::size_t>(-1);
        fde.rights_base = ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_sock_accept;
        fde.rights_inherit = ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_sock_accept;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);

        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::sock_accept(env, static_cast<wasi_posix_fd_t>(0), static_cast<fdflags_t>(0), FD_PTR, ADDR_PTR);
        if(ret != errno_t::enotsock)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept: expected enotsock for directory descriptor");
            ::fast_io::fast_terminate();
        }
    }

    return 0;
}

#else

int main() {}

#endif

#else

int main() {}

#endif

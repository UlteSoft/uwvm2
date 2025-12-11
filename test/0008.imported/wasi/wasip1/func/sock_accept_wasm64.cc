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

#if (!defined(__NEWLIB__) || defined(__CYGWIN__))

# include <uwvm2/imported/wasi/wasip1/func/sock_accept_wasm64.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
# if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t;
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
                                            .fd_storage = {.fd_limit = 64uz},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    // Prepare fd table for all test cases
    env.fd_storage.opens.resize(4uz);

    constexpr wasi_void_ptr_wasm64_t FD_PTR{1024u};
    constexpr wasi_void_ptr_wasm64_t ADDR_PTR{2048u};

    // Case 0: negative fd -> ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_accept_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                   static_cast<fdflags_wasm64_t>(0),
                                                                                   FD_PTR,
                                                                                   ADDR_PTR);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: closed descriptor (close_pos != SIZE_MAX) -> ebadf
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(1uz).fd_p;
        fde.close_pos = 0u;

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_accept_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(1),
                                                                                   static_cast<fdflags_wasm64_t>(0),
                                                                                   FD_PTR,
                                                                                   ADDR_PTR);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: expected ebadf for closed descriptor");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: missing right_sock_accept -> enotcapable
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(2uz).fd_p;
        fde.close_pos = static_cast<std::size_t>(-1);
        fde.rights_base = static_cast<rights_wasm64_t>(0);
        fde.rights_inherit = static_cast<rights_wasm64_t>(0);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_accept_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(2),
                                                                                   static_cast<fdflags_wasm64_t>(0),
                                                                                   FD_PTR,
                                                                                   ADDR_PTR);
        if(ret != errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: expected enotcapable when right_sock_accept missing");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: fd_flags contains unsupported bits -> einval
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(3uz).fd_p;
        fde.close_pos = static_cast<std::size_t>(-1);
        fde.rights_base = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_sock_accept;
        fde.rights_inherit = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_sock_accept;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        auto const invalid_flags = fdflags_wasm64_t::fdflag_append;

        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::sock_accept_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(3), invalid_flags, FD_PTR, ADDR_PTR);
        if(ret != errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: expected einval for unsupported fdflags");
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: descriptor type is dir with proper rights -> enotsock
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(0uz).fd_p;
        fde.close_pos = static_cast<std::size_t>(-1);
        fde.rights_base = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_sock_accept;
        fde.rights_inherit = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_sock_accept;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_accept_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(0),
                                                                                   static_cast<fdflags_wasm64_t>(0),
                                                                                   FD_PTR,
                                                                                   ADDR_PTR);
        if(ret != errno_wasm64_t::enotsock)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: expected enotsock for directory descriptor");
            ::fast_io::fast_terminate();
        }
    }

# if !defined(_WIN32)
    // Case 5: real TCP accept on loopback
    {
        // Create listening socket on 127.0.0.1:ephemeral
        int listen_fd{::socket(AF_INET, SOCK_STREAM, 0)};
        if(listen_fd < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: failed to create listening socket");
            ::fast_io::fast_terminate();
        }

        int optval{1};
        ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        ::sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;

        if(::bind(listen_fd, reinterpret_cast<::sockaddr*>(::std::addressof(addr)), static_cast<socklen_t>(sizeof(addr))) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: bind failed");
            ::fast_io::fast_terminate();
        }

        socklen_t addrlen{static_cast<socklen_t>(sizeof(addr))};
        if(::getsockname(listen_fd, reinterpret_cast<::sockaddr*>(::std::addressof(addr)), ::std::addressof(addrlen)) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: getsockname failed");
            ::fast_io::fast_terminate();
        }

        if(::listen(listen_fd, 1) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: listen failed");
            ::fast_io::fast_terminate();
        }

        // Put listening socket into WASI fd table (fd 0)
        auto& fde_listen = *env.fd_storage.opens.index_unchecked(0uz).fd_p;
        fde_listen.close_pos = static_cast<std::size_t>(-1);
        fde_listen.rights_base = static_cast<rights_wasm64_t>(-1);
        fde_listen.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fde_listen.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde_listen.wasi_fd.ptr->wasi_fd_storage.storage.file_fd = ::fast_io::native_file{listen_fd};

        // Create a client socket and connect
        int client_fd{::socket(AF_INET, SOCK_STREAM, 0)};
        if(client_fd < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: failed to create client socket");
            ::fast_io::fast_terminate();
        }

        if(::connect(client_fd, reinterpret_cast<::sockaddr*>(::std::addressof(addr)), static_cast<socklen_t>(sizeof(addr))) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: connect failed");
            ::fast_io::fast_terminate();
        }

        // Accept via WASI sock_accept_wasm64
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_accept_wasm64(env,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(0),
                                                                                   static_cast<fdflags_wasm64_t>(0),
                                                                                   FD_PTR,
                                                                                   ADDR_PTR);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: expected esuccess for real TCP accept");
            ::fast_io::fast_terminate();
        }

        auto const new_fd =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_posix_fd_wasm64_t>(memory, FD_PTR);
        if(new_fd < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: new fd should be non-negative");
            ::fast_io::fast_terminate();
        }

        auto const new_fd_index{static_cast<std::size_t>(new_fd)};
        if(new_fd_index >= env.fd_storage.opens.size() ||
           env.fd_storage.opens.index_unchecked(new_fd_index).fd_p == nullptr)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_accept_wasm64: new fd not recorded in fd table");
            ::fast_io::fast_terminate();
        }

        ::close(client_fd);
    }
# endif

    return 0;
}

# else

int main() {}

# endif

#else

int main() {}

#endif

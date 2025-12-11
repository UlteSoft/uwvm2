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
#include <cstring>

#include <fast_io.h>

// keep tests minimal; avoid platform-specific headers

#if (!defined(__NEWLIB__) || defined(__CYGWIN__))

# include <uwvm2/imported/wasi/wasip1/func/sock_recv_wasm64.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
# if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::riflags_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::roflags_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
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

    // Prepare FD table for all test cases
    env.fd_storage.opens.resize(4uz);

    constexpr wasi_void_ptr_wasm64_t IOVS_PTR{1024u};
    constexpr wasi_void_ptr_wasm64_t NREAD_PTR{2048u};
    constexpr wasi_void_ptr_wasm64_t ROFLAGS_PTR{4096u};

    // Case 0: negative fd -> ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_recv_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                 IOVS_PTR,
                                                                                 static_cast<wasi_size_wasm64_t>(0u),
                                                                                 static_cast<riflags_wasm64_t>(0),
                                                                                 NREAD_PTR,
                                                                                 ROFLAGS_PTR);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

# if !defined(_WIN32)
    // Case 4: real TCP recv on loopback
    {
        native_memory_t memory2{};
        memory2.init_by_page_count(1uz);

        wasip1_environment<native_memory_t> env2{.wasip1_memory = ::std::addressof(memory2),
                                                 .argv = {},
                                                 .envs = {},
                                                 .fd_storage = {.fd_limit = 64uz},
                                                 .mount_dir_roots = {},
                                                 .trace_wasip1_call = false};

        env2.fd_storage.opens.resize(2uz);

        // Create listening socket on 127.0.0.1:ephemeral
        int listen_fd{::socket(AF_INET, SOCK_STREAM, 0)};
        if(listen_fd < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: failed to create listening socket");
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
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: bind failed");
            ::fast_io::fast_terminate();
        }

        socklen_t addrlen{static_cast<socklen_t>(sizeof(addr))};
        if(::getsockname(listen_fd, reinterpret_cast<::sockaddr*>(::std::addressof(addr)), ::std::addressof(addrlen)) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: getsockname failed");
            ::fast_io::fast_terminate();
        }

        if(::listen(listen_fd, 1) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: listen failed");
            ::fast_io::fast_terminate();
        }

        // Create a client socket and connect
        int client_fd{::socket(AF_INET, SOCK_STREAM, 0)};
        if(client_fd < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: failed to create client socket");
            ::fast_io::fast_terminate();
        }

        if(::connect(client_fd, reinterpret_cast<::sockaddr*>(::std::addressof(addr)), static_cast<socklen_t>(sizeof(addr))) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: connect failed");
            ::fast_io::fast_terminate();
        }

        int accepted_fd{::accept(listen_fd, nullptr, nullptr)};
        if(accepted_fd < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: accept failed");
            ::fast_io::fast_terminate();
        }

        ::close(listen_fd);

        // Put accepted socket into WASI fd table (fd 1)
        auto& fde = *env2.fd_storage.opens.index_unchecked(1uz).fd_p;
        fde.close_pos = static_cast<std::size_t>(-1);
        fde.rights_base = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_read;
        fde.rights_inherit = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_read;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd = ::fast_io::native_file{accepted_fd};

        // Prepare payload
        constexpr char const payload[] = "hello_wasi_sock_recv";
        constexpr std::size_t payload_size_host{sizeof(payload) - 1uz};
        constexpr wasi_size_wasm64_t payload_size_wasi{static_cast<wasi_size_wasm64_t>(payload_size_host)};

        constexpr wasi_void_ptr_wasm64_t BUF_PTR{8192u};
        constexpr wasi_void_ptr_wasm64_t IOV_PTR{12288u};
        constexpr wasi_void_ptr_wasm64_t NREAD2_PTR{14336u};
        constexpr wasi_void_ptr_wasm64_t ROFLAGS2_PTR{16384u};

        // Set up one iovec: { ptr = BUF_PTR, len = payload_size }
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory2, IOV_PTR, BUF_PTR);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(
            memory2,
            static_cast<wasi_void_ptr_wasm64_t>(IOV_PTR + 8u),
            payload_size_wasi);

        // Send data from client
        auto const send_res = ::send(client_fd, payload, static_cast<int>(payload_size_host), 0);
        if(send_res != static_cast<int>(payload_size_host))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: send failed");
            ::fast_io::fast_terminate();
        }

        // Recv via WASI sock_recv_wasm64
        auto const ret2 = ::uwvm2::imported::wasi::wasip1::func::sock_recv_wasm64(env2,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(1),
                                                                                   IOV_PTR,
                                                                                   static_cast<wasi_size_wasm64_t>(1u),
                                                                                   static_cast<riflags_wasm64_t>(0),
                                                                                   NREAD2_PTR,
                                                                                   ROFLAGS2_PTR);
        if(ret2 != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: expected esuccess for real TCP recv");
            ::fast_io::fast_terminate();
        }

        auto const nread2 =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory2, NREAD2_PTR);
        if(nread2 != payload_size_wasi)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: nread mismatch for real TCP recv");
            ::fast_io::fast_terminate();
        }

        using roflags_underlying_t2 = ::std::underlying_type_t<roflags_t>;
        auto const roflags_val2 =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<roflags_underlying_t2>(memory2, ROFLAGS2_PTR);
        if((roflags_val2 &
            static_cast<roflags_underlying_t2>(::uwvm2::imported::wasi::wasip1::abi::roflags_t::sock_recv_data_truncated)) != 0u)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: unexpected truncation flag for real TCP recv");
            ::fast_io::fast_terminate();
        }

        if(::std::memcmp(memory2.memory_begin + BUF_PTR, payload, payload_size_host) != 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: payload mismatch for real TCP recv");
            ::fast_io::fast_terminate();
        }

        ::close(client_fd);
    }
# endif

    // Case 1: closed descriptor (close_pos != SIZE_MAX) -> ebadf
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(1uz).fd_p;
        fde.close_pos = 0u;

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_recv_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(1),
                                                                                 IOVS_PTR,
                                                                                 static_cast<wasi_size_wasm64_t>(0u),
                                                                                 static_cast<riflags_wasm64_t>(0),
                                                                                 NREAD_PTR,
                                                                                 ROFLAGS_PTR);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: expected ebadf for closed descriptor");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: missing right_fd_read -> enotcapable
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(2uz).fd_p;
        fde.close_pos = static_cast<std::size_t>(-1);
        fde.rights_base = static_cast<rights_wasm64_t>(0);
        fde.rights_inherit = static_cast<rights_wasm64_t>(0);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_recv_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(2),
                                                                                 IOVS_PTR,
                                                                                 static_cast<wasi_size_wasm64_t>(0u),
                                                                                 static_cast<riflags_wasm64_t>(0),
                                                                                 NREAD_PTR,
                                                                                 ROFLAGS_PTR);
        if(ret != errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: expected enotcapable when right_fd_read missing");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: zero iovs_len -> esuccess, nread=0, roflags=0
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(0uz).fd_p;
        fde.close_pos = static_cast<std::size_t>(-1);
        fde.rights_base = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_read;
        fde.rights_inherit = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_read;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_recv_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(0),
                                                                                 IOVS_PTR,
                                                                                 static_cast<wasi_size_wasm64_t>(0u),
                                                                                 static_cast<riflags_wasm64_t>(0),
                                                                                 NREAD_PTR,
                                                                                 ROFLAGS_PTR);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: zero-iov should return esuccess");
            ::fast_io::fast_terminate();
        }

        auto const n0 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, NREAD_PTR);
        if(n0 != static_cast<wasi_size_wasm64_t>(0u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: zero-iov nread should be 0");
            ::fast_io::fast_terminate();
        }

        using roflags_underlying_t = ::std::underlying_type_t<roflags_t>;
        auto const roflags_val = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<roflags_underlying_t>(memory, ROFLAGS_PTR);
        if(roflags_val != static_cast<roflags_underlying_t>(0u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv_wasm64: zero-iov roflags should be 0");
            ::fast_io::fast_terminate();
        }
    }

    return 0;
}

# else

int main() {}

# endif

#else

int main() {}

#endif

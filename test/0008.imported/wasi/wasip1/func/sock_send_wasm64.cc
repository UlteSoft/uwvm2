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

// keep tests minimal; avoid redundant platform-specific headers

#if (!defined(__NEWLIB__) || defined(__CYGWIN__))

# include <uwvm2/imported/wasi/wasip1/func/sock_send_wasm64.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
# if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::siflags_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
    using ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e;
    using ::uwvm2::object::memory::linear::native_memory_t;

    native_memory_t memory{};
    memory.init_by_page_count(1uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    // Prepare FD table for local tests
    env.fd_storage.opens.resize(4uz);

    constexpr wasi_void_ptr_wasm64_t IOVS_PTR{1024u};
    constexpr wasi_void_ptr_wasm64_t NSENT_PTR{2048u};

    // Case 0: negative fd -> ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_send_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                 IOVS_PTR,
                                                                                 static_cast<wasi_size_wasm64_t>(0u),
                                                                                 static_cast<siflags_wasm64_t>(0),
                                                                                 NSENT_PTR);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: closed descriptor (close_pos != SIZE_MAX) -> ebadf
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(1uz).fd_p;
        fde.close_pos = 0u;

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_send_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(1),
                                                                                 IOVS_PTR,
                                                                                 static_cast<wasi_size_wasm64_t>(0u),
                                                                                 static_cast<siflags_wasm64_t>(0),
                                                                                 NSENT_PTR);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: expected ebadf for closed descriptor");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: missing right_fd_write -> enotcapable
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(2uz).fd_p;
        fde.close_pos = static_cast<::std::size_t>(-1);
        fde.rights_base = static_cast<rights_wasm64_t>(0);
        fde.rights_inherit = static_cast<rights_wasm64_t>(0);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(wasi_fd_type_e::file);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_send_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(2),
                                                                                 IOVS_PTR,
                                                                                 static_cast<wasi_size_wasm64_t>(0u),
                                                                                 static_cast<siflags_wasm64_t>(0),
                                                                                 NSENT_PTR);
        if(ret != errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: expected enotcapable when right_fd_write missing");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: non-zero si_flags -> einval
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(3uz).fd_p;
        fde.close_pos = static_cast<::std::size_t>(-1);
        fde.rights_base = static_cast<rights_wasm64_t>(-1);
        fde.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(wasi_fd_type_e::file);

        using siflags_underlying_t = ::std::underlying_type_t<siflags_wasm64_t>;
        auto const invalid_flags = static_cast<siflags_wasm64_t>(static_cast<siflags_underlying_t>(1u));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_send_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 IOVS_PTR,
                                                                                 static_cast<wasi_size_wasm64_t>(0u),
                                                                                 invalid_flags,
                                                                                 NSENT_PTR);
        if(ret != errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: expected einval for non-zero si_flags");
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: zero iovs_len -> esuccess, nsent=0
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(0uz).fd_p;
        fde.close_pos = static_cast<::std::size_t>(-1);
        fde.rights_base = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_write;
        fde.rights_inherit = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_write;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(wasi_fd_type_e::file);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_send_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(0),
                                                                                 IOVS_PTR,
                                                                                 static_cast<wasi_size_wasm64_t>(0u),
                                                                                 static_cast<siflags_wasm64_t>(0),
                                                                                 NSENT_PTR);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: zero-iov should return esuccess");
            ::fast_io::fast_terminate();
        }

        auto const n0 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, NSENT_PTR);
        if(n0 != static_cast<wasi_size_wasm64_t>(0u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: zero-iov nsent should be 0");
            ::fast_io::fast_terminate();
        }
    }

    // Case 5: descriptor type is dir with proper rights -> enotsock
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(0uz).fd_p;
        fde.close_pos = static_cast<::std::size_t>(-1);
        fde.rights_base = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_write;
        fde.rights_inherit = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_write;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(wasi_fd_type_e::dir);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_send_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(0),
                                                                                 IOVS_PTR,
                                                                                 static_cast<wasi_size_wasm64_t>(0u),
                                                                                 static_cast<siflags_wasm64_t>(0),
                                                                                 NSENT_PTR);
        if(ret != errno_wasm64_t::enotsock)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: expected enotsock for directory descriptor");
            ::fast_io::fast_terminate();
        }
    }

# if !defined(_WIN32)
    // Case 6: real TCP send on loopback
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
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: failed to create listening socket");
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
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: bind failed");
            ::fast_io::fast_terminate();
        }

        socklen_t addrlen{static_cast<socklen_t>(sizeof(addr))};
        if(::getsockname(listen_fd, reinterpret_cast<::sockaddr*>(::std::addressof(addr)), ::std::addressof(addrlen)) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: getsockname failed");
            ::fast_io::fast_terminate();
        }

        if(::listen(listen_fd, 1) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: listen failed");
            ::fast_io::fast_terminate();
        }

        // Create a client socket and connect (client end will be managed by WASI env)
        int client_fd{::socket(AF_INET, SOCK_STREAM, 0)};
        if(client_fd < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: failed to create client socket");
            ::fast_io::fast_terminate();
        }

        if(::connect(client_fd, reinterpret_cast<::sockaddr*>(::std::addressof(addr)), static_cast<socklen_t>(sizeof(addr))) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: connect failed");
            ::fast_io::fast_terminate();
        }

        int accepted_fd{::accept(listen_fd, nullptr, nullptr)};
        if(accepted_fd < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: accept failed");
            ::fast_io::fast_terminate();
        }

        ::close(listen_fd);

        // Put client socket into WASI fd table (fd 1) for sending
        auto& fde = *env2.fd_storage.opens.index_unchecked(1uz).fd_p;
        fde.close_pos = static_cast<::std::size_t>(-1);
        fde.rights_base = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_write;
        fde.rights_inherit = ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_write;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd = ::fast_io::native_file{client_fd};

        // Prepare payload
        constexpr char const payload[] = "hello_wasi_sock_send";
        constexpr ::std::size_t payload_size_host{sizeof(payload) - 1uz};
        constexpr wasi_size_wasm64_t payload_size_wasm64{static_cast<wasi_size_wasm64_t>(payload_size_host)};

        constexpr wasi_void_ptr_wasm64_t BUF_PTR{8192u};
        constexpr wasi_void_ptr_wasm64_t IOV_PTR{12288u};
        constexpr wasi_void_ptr_wasm64_t NSENT2_PTR{14336u};

        // Write payload into WASM memory
        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64(
            memory2,
            BUF_PTR,
            reinterpret_cast<::std::byte const*>(payload),
            reinterpret_cast<::std::byte const*>(payload) + payload_size_host);

        // Set up one ciovec: { buf = BUF_PTR, buf_len = payload_size }
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory2, IOV_PTR, BUF_PTR);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(
            memory2,
            static_cast<wasi_void_ptr_wasm64_t>(IOV_PTR + 8u),
            payload_size_wasm64);

        // Send via WASI sock_send_wasm64
        auto const ret2 = ::uwvm2::imported::wasi::wasip1::func::sock_send_wasm64(env2,
                                                                                   static_cast<wasi_posix_fd_wasm64_t>(1),
                                                                                   IOV_PTR,
                                                                                   static_cast<wasi_size_wasm64_t>(1u),
                                                                                   static_cast<siflags_wasm64_t>(0),
                                                                                   NSENT2_PTR);
        if(ret2 != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: expected esuccess for real TCP send");
            ::fast_io::fast_terminate();
        }

        auto const nsent2 =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory2, NSENT2_PTR);
        if(nsent2 != payload_size_wasm64)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: nsent mismatch for real TCP send");
            ::fast_io::fast_terminate();
        }

        // Receive on accepted side and check payload
        char recv_buf[64]{};
        auto const recv_res = ::recv(accepted_fd, recv_buf, static_cast<int>(sizeof(recv_buf)), 0);
        if(recv_res != static_cast<int>(payload_size_host))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: recv length mismatch on peer");
            ::fast_io::fast_terminate();
        }

        if(::std::memcmp(recv_buf, payload, payload_size_host) != 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_send_wasm64: payload mismatch on peer");
            ::fast_io::fast_terminate();
        }

        ::close(accepted_fd);
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


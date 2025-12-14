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

#if defined(_WIN32) && !defined(__CYGWIN__) && !(defined(__clang__) && defined(__MINGW64__))
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# define UWVM_HAS_NATIVE_WIN32_WS2 1
# include <winsock2.h>
# include <ws2tcpip.h>
#endif

#include <fast_io.h>

// keep tests minimal; avoid redundant platform-specific headers

#if (!defined(__NEWLIB__) || defined(__CYGWIN__))

# include <uwvm2/imported/wasi/wasip1/func/sock_shutdown.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
# if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
    using ::uwvm2::imported::wasi::wasip1::abi::sdflags_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
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

    env.fd_storage.opens.resize(4uz);

    // Case 0: negative fd -> ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_shutdown(env, static_cast<wasi_posix_fd_t>(-1), sdflags_t::shut_rd);
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: closed descriptor (close_pos != SIZE_MAX) -> ebadf
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(1uz).fd_p;
        fde.close_pos = 0u;

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_shutdown(env, static_cast<wasi_posix_fd_t>(1), sdflags_t::shut_rd);
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: expected ebadf for closed descriptor");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: missing rights -> enotcapable
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(2uz).fd_p;
        fde.close_pos = static_cast<::std::size_t>(-1);
        fde.rights_base = static_cast<rights_t>(0);
        fde.rights_inherit = static_cast<rights_t>(0);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_shutdown(env, static_cast<wasi_posix_fd_t>(2), sdflags_t::shut_rd);
        if(ret != errno_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: expected enotcapable for missing right_sock_shutdown");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: invalid how (0) -> einval
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(0uz).fd_p;
        fde.close_pos = static_cast<::std::size_t>(-1);
        fde.rights_base = rights_t::right_sock_shutdown;
        fde.rights_inherit = rights_t::right_sock_shutdown;

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_shutdown(env, static_cast<wasi_posix_fd_t>(0), static_cast<sdflags_t>(0));
        if(ret != errno_t::einval)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: expected einval for zero sdflags");
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: invalid how (unknown bits) -> einval
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(0uz).fd_p;
        fde.close_pos = static_cast<::std::size_t>(-1);
        fde.rights_base = rights_t::right_sock_shutdown;
        fde.rights_inherit = rights_t::right_sock_shutdown;

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_shutdown(env, static_cast<wasi_posix_fd_t>(0), static_cast<sdflags_t>(0x04));
        if(ret != errno_t::einval)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: expected einval for invalid sdflags bits");
            ::fast_io::fast_terminate();
        }
    }

    // Case 5: descriptor type is dir -> enotsock
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(0uz).fd_p;
        fde.close_pos = static_cast<::std::size_t>(-1);
        fde.rights_base = rights_t::right_sock_shutdown;
        fde.rights_inherit = rights_t::right_sock_shutdown;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(wasi_fd_type_e::dir);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_shutdown(env, static_cast<wasi_posix_fd_t>(0), sdflags_t::shut_rd);
        if(ret != errno_t::enotsock)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: expected enotsock for directory descriptor");
            ::fast_io::fast_terminate();
        }
    }

#  if !defined(_WIN32)
    // Case 6: real TCP shutdown on loopback (POSIX)
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

        int listen_fd{::socket(AF_INET, SOCK_STREAM, 0)};
        if(listen_fd < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: failed to create listening socket");
            ::fast_io::fast_terminate();
        }

        int optval{1};
        ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        ::sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;

        if(::bind(listen_fd, reinterpret_cast<::sockaddr*>(::std::addressof(addr)), static_cast<socklen_t>(sizeof(addr))) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: bind failed");
            ::close(listen_fd);
            ::fast_io::fast_terminate();
        }

        ::socklen_t addrlen{static_cast<socklen_t>(sizeof(addr))};
        if(::getsockname(listen_fd, reinterpret_cast<::sockaddr*>(::std::addressof(addr)), ::std::addressof(addrlen)) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: getsockname failed");
            ::close(listen_fd);
            ::fast_io::fast_terminate();
        }

        if(::listen(listen_fd, 1) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: listen failed");
            ::close(listen_fd);
            ::fast_io::fast_terminate();
        }

        int client_fd{::socket(AF_INET, SOCK_STREAM, 0)};
        if(client_fd < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: failed to create client socket");
            ::close(listen_fd);
            ::fast_io::fast_terminate();
        }

        if(::connect(client_fd, reinterpret_cast<::sockaddr*>(::std::addressof(addr)), addrlen) < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: connect failed");
            ::close(client_fd);
            ::close(listen_fd);
            ::fast_io::fast_terminate();
        }

        int accepted_fd{::accept(listen_fd, nullptr, nullptr)};
        if(accepted_fd < 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: accept failed");
            ::close(client_fd);
            ::close(listen_fd);
            ::fast_io::fast_terminate();
        }

        ::close(listen_fd);

        auto& fde = *env2.fd_storage.opens.index_unchecked(1uz).fd_p;
        fde.close_pos = static_cast<::std::size_t>(-1);
        fde.rights_base = rights_t::right_sock_shutdown;
        fde.rights_inherit = rights_t::right_sock_shutdown;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd = ::fast_io::native_file{accepted_fd};

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_shutdown(env2, static_cast<wasi_posix_fd_t>(1), sdflags_t::shut_wr);
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown: expected esuccess for real TCP shutdown");
            ::close(client_fd);
            ::fast_io::fast_terminate();
        }

        ::close(client_fd);
    }
#  elif defined(UWVM_HAS_NATIVE_WIN32_WS2)
    // Case 6: real TCP shutdown on loopback (WinSock2 / Win32 socket type)
    {
        WSADATA wsa_data{};
        if(::WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown(win32): WSAStartup failed");
            ::fast_io::fast_terminate();
        }

        native_memory_t memory2{};
        memory2.init_by_page_count(1uz);

        wasip1_environment<native_memory_t> env2{.wasip1_memory = ::std::addressof(memory2),
                                                 .argv = {},
                                                 .envs = {},
                                                 .fd_storage = {.fd_limit = 64uz},
                                                 .mount_dir_roots = {},
                                                 .trace_wasip1_call = false};

        env2.fd_storage.opens.resize(2uz);

        SOCKET listen_socket{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
        if(listen_socket == INVALID_SOCKET)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown(win32): failed to create listening socket");
            ::WSACleanup();
            ::fast_io::fast_terminate();
        }

        ::sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;

        if(::bind(listen_socket, reinterpret_cast<::sockaddr*>(::std::addressof(addr)), static_cast<int>(sizeof(addr))) == SOCKET_ERROR)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown(win32): bind failed");
            ::closesocket(listen_socket);
            ::WSACleanup();
            ::fast_io::fast_terminate();
        }

        int addrlen{static_cast<int>(sizeof(addr))};
        if(::getsockname(listen_socket, reinterpret_cast<::sockaddr*>(::std::addressof(addr)), ::std::addressof(addrlen)) == SOCKET_ERROR)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown(win32): getsockname failed");
            ::closesocket(listen_socket);
            ::WSACleanup();
            ::fast_io::fast_terminate();
        }

        if(::listen(listen_socket, 1) == SOCKET_ERROR)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown(win32): listen failed");
            ::closesocket(listen_socket);
            ::WSACleanup();
            ::fast_io::fast_terminate();
        }

        SOCKET client_socket{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
        if(client_socket == INVALID_SOCKET)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown(win32): failed to create client socket");
            ::closesocket(listen_socket);
            ::WSACleanup();
            ::fast_io::fast_terminate();
        }

        if(::connect(client_socket, reinterpret_cast<::sockaddr*>(::std::addressof(addr)), addrlen) == SOCKET_ERROR)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown(win32): connect failed");
            ::closesocket(client_socket);
            ::closesocket(listen_socket);
            ::WSACleanup();
            ::fast_io::fast_terminate();
        }

        SOCKET accepted_socket{::accept(listen_socket, nullptr, nullptr)};
        if(accepted_socket == INVALID_SOCKET)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown(win32): accept failed");
            ::closesocket(client_socket);
            ::closesocket(listen_socket);
            ::WSACleanup();
            ::fast_io::fast_terminate();
        }

        ::closesocket(listen_socket);

        auto& fde = *env2.fd_storage.opens.index_unchecked(1uz).fd_p;
        fde.close_pos = static_cast<::std::size_t>(-1);
        fde.rights_base = rights_t::right_sock_shutdown;
        fde.rights_inherit = rights_t::right_sock_shutdown;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(wasi_fd_type_e::socket);
        fde.wasi_fd.ptr->wasi_fd_storage.storage.socket_fd = ::fast_io::win32_socket_file{static_cast<::std::size_t>(accepted_socket)};

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_shutdown(env2, static_cast<wasi_posix_fd_t>(1), sdflags_t::shut_wr);
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_shutdown(win32): expected esuccess for real TCP shutdown");
            ::closesocket(client_socket);
            ::closesocket(accepted_socket);
            ::WSACleanup();
            ::fast_io::fast_terminate();
        }

        ::closesocket(client_socket);
        ::closesocket(accepted_socket);
        ::WSACleanup();
    }
#  endif

    return 0;
}

# else

int main() {}

# endif

#else

int main() {}

#endif



/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
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

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <climits>
# include <cstring>
# include <limits>
# include <concepts>
# include <bit>
# include <memory>
# include <type_traits>
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
// platform
#if __has_include(<errno.h>)
# include <errno.h>
#endif
# if __has_include(<sys/fcntl.h>)
#  include <sys/fcntl.h>
# endif
# if __has_include(<sys/socket.h>)
#  include <sys/socket.h>
# endif
# if __has_include(<netinet/in.h>)
#  include <netinet/in.h>
# endif
// import
# include <fast_io.h>
# include <fast_io_device.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/utils/mutex/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/object/memory/linear/impl.h>
# include <uwvm2/imported/wasi/wasip1/abi/impl.h>
# include <uwvm2/imported/wasi/wasip1/fd_manager/impl.h>
# include <uwvm2/imported/wasi/wasip1/memory/impl.h>
# include <uwvm2/imported/wasi/wasip1/environment/impl.h>
# include "base.h"
# include "posix.h"
#endif

#ifndef UWVM_CPP_EXCEPTIONS
# warning "Without enabling C++ exceptions, using this WASI function may cause termination."
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t sock_shutdown_base(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t sock_fd,
        ::uwvm2::imported::wasi::wasip1::abi::sdflags_t how) noexcept
    {
        // The negative value fd is invalid, and this check prevents subsequent undefined behavior.
        if(sock_fd < 0) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }

        auto& wasm_fd_storage{env.fd_storage};

        // The pointer to `wasm_fd` is fixed and remains unchanged even when the vector within `fd_manager` is resized.
        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t* curr_wasi_fd_t_p;  // no initialize

        // Subsequent operations involving the file descriptor require locking. curr_fd_release_guard release when return.
        ::uwvm2::utils::mutex::mutex_merely_release_guard_t curr_fd_release_guard{};

        {
            // Simply acquiring data using a shared_lock
            ::uwvm2::utils::mutex::rw_fair_shared_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

            // Negative states have been excluded, so the conversion result will only be positive numbers.
            using unsigned_fd_t = ::std::make_unsigned_t<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>;
            auto const unsigned_fd{static_cast<unsigned_fd_t>(sock_fd)};

            // On platforms where `size_t` is smaller than the `fd` type, this check must be added.
            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            if constexpr(::std::numeric_limits<unsigned_fd_t>::max() > size_t_max)
            {
                if(unsigned_fd > size_t_max) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }
            }

            auto const fd_opens_pos{static_cast<::std::size_t>(unsigned_fd)};

            // The minimum value in rename_map is greater than opensize.
            if(wasm_fd_storage.opens.size() <= fd_opens_pos)
            {
                // Possibly within the tree being renumbered
                if(auto const renumber_map_iter{wasm_fd_storage.renumber_map.find(sock_fd)}; renumber_map_iter != wasm_fd_storage.renumber_map.end())
                {
                    curr_wasi_fd_t_p = renumber_map_iter->second.fd_p;
                }
                else [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf;
                }
            }
            else
            {
                // The addition here is safe.
                curr_wasi_fd_t_p = wasm_fd_storage.opens.index_unchecked(fd_opens_pos).fd_p;
            }

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(curr_wasi_fd_t_p == nullptr) [[unlikely]]
            {
                // Security issues inherent to virtual machines
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }
# endif

            // Lock fd before releasing the fds lock to prevent concurrent close.
            curr_fd_release_guard.device_p = ::std::addressof(curr_wasi_fd_t_p->fd_mutex);
            curr_fd_release_guard.lock();
        }

        // curr_fd_uniptr is not null.
        auto& curr_fd{*curr_wasi_fd_t_p};

        // If obtained from the renumber map, it will always be the correct value. If obtained from the open vec, it requires checking whether it is closed.
        // Therefore, a unified check is implemented.
        if(curr_fd.close_pos != SIZE_MAX) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }

        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_sock_shutdown) !=
           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_sock_shutdown) [[unlikely]]
        {
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable;
        }

        using sdflags_underlying_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::sdflags_t>;

        constexpr sdflags_underlying_t shut_rd_u{static_cast<sdflags_underlying_t>(::uwvm2::imported::wasi::wasip1::abi::sdflags_t::shut_rd)};
        constexpr sdflags_underlying_t shut_wr_u{static_cast<sdflags_underlying_t>(::uwvm2::imported::wasi::wasip1::abi::sdflags_t::shut_wr)};
        constexpr sdflags_underlying_t valid_mask_u{static_cast<sdflags_underlying_t>(shut_rd_u | shut_wr_u)};

        auto const how_u{static_cast<sdflags_underlying_t>(how)};

        // sdflags is a bitset. Only {shut_rd, shut_wr} are valid, and at least one bit must be set.
        if(((how_u & valid_mask_u) == 0) || ((how_u & ~valid_mask_u) != 0)) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval; }

        // If ptr is null, it indicates an attempt to open a closed file. However, the preceding check for close pos already prevents such closed files from
        // being processed, making this a virtual machine implementation error.
        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
        {
// This will be checked at runtime.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
        }

        int native_how{};

# if defined(_WIN32) && !defined(__CYGWIN__)
        // Winsock: 0=SD_RECEIVE, 1=SD_SEND, 2=SD_BOTH
        if((how_u & valid_mask_u) == valid_mask_u) { native_how = 2; }
        else if((how_u & shut_rd_u) == shut_rd_u) { native_how = 0; }
        else
        {
            native_how = 1;
        }
# else
        // POSIX: SHUT_RD, SHUT_WR, SHUT_RDWR
        if((how_u & valid_mask_u) == valid_mask_u) { native_how = SHUT_RDWR; }
        else if((how_u & shut_rd_u) == shut_rd_u) { native_how = SHUT_RD; }
        else
        {
            native_how = SHUT_WR;
        }
# endif

        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
        {
            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsock;
            }
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file_observer:
            {
# if defined(_WIN32) && !defined(__CYGWIN__)
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsock;
# else
                ::fast_io::native_io_observer curr_fd_native_observer{};

                bool const is_file_observer{curr_fd.wasi_fd.ptr->wasi_fd_storage.type ==
                                            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file_observer};
                if(is_file_observer)
                {
                    auto& file_observer{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_observer};
                    curr_fd_native_observer = file_observer;
                }
                else
                {
                    auto& file_fd{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd};
                    curr_fd_native_observer = file_fd;
                }

                auto const& curr_fd_native_file{curr_fd_native_observer};

                int const native_fd{curr_fd_native_file.native_handle()};

                auto const shutdown_ret{::uwvm2::imported::wasi::wasip1::func::posix::shutdown(native_fd, native_how)};
                if(shutdown_ret == -1) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(
                        ::fast_io::error{::fast_io::posix_domain_value, static_cast<::fast_io::error::value_type>(static_cast<unsigned>(errno))});
                }

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
# endif
            }
# if defined(_WIN32) && !defined(__CYGWIN__)
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket_observer:
            {
                ::fast_io::win32_socket_io_observer curr_fd_native_observer{};

                bool const is_socket_observer{curr_fd.wasi_fd.ptr->wasi_fd_storage.type ==
                                              ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket_observer};
                if(is_socket_observer)
                {
                    auto& socket_observer{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.socket_observer};
                    curr_fd_native_observer = socket_observer;
                }
                else
                {
                    auto& socket_fd{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.socket_fd};
                    curr_fd_native_observer = socket_fd;
                }

                auto const& curr_fd_native_file{curr_fd_native_observer};

                auto const socket_handle{curr_fd_native_file.native_handle()};

                auto const wsa_ret{::fast_io::win32::shutdown(socket_handle, native_how)};

                if(wsa_ret != 0) [[unlikely]]
                {
                    auto const wsa_err{static_cast<::std::uint_least32_t>(::fast_io::win32::WSAGetLastError())};

                    switch(wsa_err)
                    {
                        // Winsock (WSA*) error codes mapping (aligned with other sock_* implementations)
                        case 10004uz /*WSAEINTR*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                        // align with ebadf policy
                        case 10009uz /*WSAEBADF*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10013uz /*WSAEACCES*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                        case 10014uz /*WSAEFAULT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efault;
                        case 10022uz /*WSAEINVAL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        case 10024uz /*WSAEMFILE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::emfile;
                        case 10035uz /*WSAEWOULDBLOCK*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eagain;
                        case 10036uz /*WSAEINPROGRESS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einprogress;
                        case 10037uz /*WSAEALREADY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ealready;
                        case 10038uz /*WSAENOTSOCK*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsock;
                        case 10039uz /*WSAEDESTADDRREQ*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::edestaddrreq;
                        case 10040uz /*WSAEMSGSIZE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::emsgsize;
                        case 10041uz /*WSAEPROTOTYPE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eprototype;
                        case 10042uz /*WSAENOPROTOOPT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoprotoopt;
                        case 10043uz /*WSAEPROTONOSUPPORT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eprotonosupport;
                        case 10044uz /*WSAESOCKTNOSUPPORT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                        case 10045uz /*WSAEOPNOTSUPP*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                        case 10046uz /*WSAEPFNOSUPPORT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eafnosupport;
                        case 10047uz /*WSAEAFNOSUPPORT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eafnosupport;
                        case 10048uz /*WSAEADDRINUSE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eaddrinuse;
                        case 10049uz /*WSAEADDRNOTAVAIL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eaddrnotavail;
                        case 10050uz /*WSAENETDOWN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enetdown;
                        case 10051uz /*WSAENETUNREACH*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enetunreach;
                        case 10052uz /*WSAENETRESET*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enetreset;
                        case 10053uz /*WSAECONNABORTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::econnaborted;
                        case 10054uz /*WSAECONNRESET*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::econnreset;
                        case 10055uz /*WSAENOBUFS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enobufs;
                        case 10056uz /*WSAEISCONN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisconn;
                        case 10057uz /*WSAENOTCONN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotconn;
                        case 10058uz /*WSAESHUTDOWN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::epipe;
                        case 10059uz /*WSAETOOMANYREFS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10060uz /*WSAETIMEDOUT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::etimedout;
                        case 10061uz /*WSAECONNREFUSED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::econnrefused;
                        case 10062uz /*WSAELOOP*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop;
                        case 10063uz /*WSAENAMETOOLONG*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enametoolong;
                        case 10064uz /*WSAEHOSTDOWN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ehostunreach;
                        case 10065uz /*WSAEHOSTUNREACH*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ehostunreach;
                        case 10066uz /*WSAENOTEMPTY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotempty;
                        case 10067uz /*WSAEPROCLIM*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eagain;
                        case 10068uz /*WSAEUSERS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eagain;
                        case 10069uz /*WSAEDQUOT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::edquot;
                        case 10070uz /*WSAESTALE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::estale;
                        case 10071uz /*WSAEREMOTE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10091uz /*WSASYSNOTREADY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enetdown;
                        case 10092uz /*WSAVERNOTSUPPORTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                        case 10093uz /*WSANOTINITIALISED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                        case 10101uz /*WSAEDISCON*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::econnreset;
                        case 10102uz /*WSAENOMORE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10103uz /*WSAECANCELLED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ecanceled;
                        case 10104uz /*WSAEINVALIDPROCTABLE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10105uz /*WSAEINVALIDPROVIDER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10106uz /*WSAEPROVIDERFAILEDINIT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10107uz /*WSASYSCALLFAILURE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10108uz /*WSASERVICE_NOT_FOUND*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent;
                        case 10109uz /*WSATYPE_NOT_FOUND*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent;
                        case 10110uz /*WSA_E_NO_MORE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10111uz /*WSA_E_CANCELLED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ecanceled;
                        case 10112uz /*WSAEREFUSED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::econnrefused;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }
# endif
            [[unlikely]] default:
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
        }
    }

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t sock_shutdown(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t sock_fd,
        ::uwvm2::imported::wasi::wasip1::abi::sdflags_t how) noexcept
    {
        auto const trace_wasip1_call{env.trace_wasip1_call};

        if(trace_wasip1_call) [[unlikely]]
        {
# ifdef UWVM
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"wasip1: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"sock_shutdown",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                sock_fd,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(how)>>>(how),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: sock_shutdown(",
                                sock_fd,
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(how)>>>(how),
                                u8") (wasi-trace)\n");
# endif
        }

        return sock_shutdown_base(env, sock_fd, how);
    }
}

#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

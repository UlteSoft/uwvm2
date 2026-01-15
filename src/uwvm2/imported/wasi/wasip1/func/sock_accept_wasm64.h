

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
# include <utility>
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
// platform
# if __has_include(<errno.h>)
#  include <errno.h>
# endif
# if __has_include(<fcntl.h>)
#  include <fcntl.h>
# elif __has_include(<sys/fcntl.h>)
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
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/object/memory/linear/impl.h>
# include <uwvm2/imported/wasi/wasip1/abi/impl.h>
# include <uwvm2/imported/wasi/wasip1/fd_manager/impl.h>
# include <uwvm2/imported/wasi/wasip1/memory/impl.h>
# include <uwvm2/imported/wasi/wasip1/environment/impl.h>
# include "base.h"
# include "posix.h"
# include "sock_accept.h"
#endif

#ifndef UWVM_CPP_EXCEPTIONS
# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wcpp"
# elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcpp"
# endif
# warning "Without enabling C++ exceptions, using this WASI function may cause termination."
# if defined(__clang__)
#  pragma clang diagnostic pop
# elif defined(__GNUC__)
#  pragma GCC diagnostic pop
# endif
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{
    inline ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t sock_accept_wasm64(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t sock_fd,
        ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t fd_flags,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ro_fd_ptrsz,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ro_addr_ptrsz) noexcept
    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(env.wasip1_memory == nullptr) [[unlikely]]
        {
            // Security issues inherent to virtual machines
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
# endif

        auto& memory{*env.wasip1_memory};

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
                                u8"sock_accept_wasm64",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                sock_fd,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(fd_flags)>>>(fd_flags),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(ro_fd_ptrsz),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(ro_addr_ptrsz),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: sock_accept_wasm64(",
                                sock_fd,
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(fd_flags)>>>(fd_flags),
                                u8", ",
                                ::fast_io::mnp::addrvw(ro_fd_ptrsz),
                                u8", ",
                                ::fast_io::mnp::addrvw(ro_addr_ptrsz),
                                u8") (wasi-trace)\n");
# endif
        }

        // The negative value fd is invalid, and this check prevents subsequent undefined behavior.
        if(sock_fd < 0) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf; }

        auto& wasm_fd_storage{env.fd_storage};

        // The pointer to `wasm_fd` is fixed and remains unchanged even when the vector within `fd_manager` is resized.
        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t* curr_wasi_fd_t_p;  // no initialize

        // Subsequent operations involving the file descriptor require locking. curr_fd_release_guard release when return.
        ::uwvm2::utils::mutex::mutex_merely_release_guard_t curr_fd_release_guard{};

        {
            // Prevent operations to obtain the size or perform resizing at this time.
            // Only a lock is required when acquiring the unique pointer for the file descriptor. The lock can be released once the acquisition is complete.
            // Since the file descriptor's location is fixed and accessed via the unique pointer,

            // Simply acquiring data using a shared_lock
            ::uwvm2::utils::mutex::rw_fair_shared_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

            // Negative states have been excluded, so the conversion result will only be positive numbers.
            using unsigned_fd_t = ::std::make_unsigned_t<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t>;
            auto const unsigned_fd{static_cast<unsigned_fd_t>(sock_fd)};

            // On platforms where `size_t` is smaller than the `fd` type, this check must be added.
            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            if constexpr(::std::numeric_limits<unsigned_fd_t>::max() > size_t_max)
            {
                if(unsigned_fd > size_t_max) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf; }
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
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf;
                }
            }
            else
            {
                // The addition here is safe.
                curr_wasi_fd_t_p = wasm_fd_storage.opens.index_unchecked(fd_opens_pos).fd_p;
            }

            // curr_wasi_fd_t_p never nullptr
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(curr_wasi_fd_t_p == nullptr) [[unlikely]]
            {
                // Security issues inherent to virtual machines
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }
# endif

            // Other threads will definitely lock fds_rwlock when performing close operations (since they need to access the fd vector). If the current thread
            // is performing fdatasync, no other thread can be executing any close operations simultaneously, eliminating any destruction issues. Therefore,
            // acquiring the lock at this point is safe. However, the problem arises when, immediately after acquiring the lock and before releasing the manager
            // lock and beginning fd operations, another thread executes a deletion that removes this fd. Subsequent operations by the current thread would then
            // encounter issues. Thus, locking must occur before releasing fds_rwlock.
            curr_fd_release_guard.device_p = ::std::addressof(curr_wasi_fd_t_p->fd_mutex);
            curr_fd_release_guard.lock();

            // After unlocking fds_lock, members within `wasm_fd_storage_t` can no longer be accessed or modified.
        }

        // curr_fd_uniptr is not null.
        auto& curr_fd{*curr_wasi_fd_t_p};

        // If obtained from the renumber map, it will always be the correct value. If obtained from the open vec, it requires checking whether it is closed.
        // Therefore, a unified check is implemented.
        if(curr_fd.close_pos != SIZE_MAX) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf; }

        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_sock_accept) !=
           ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_sock_accept) [[unlikely]]
        {
            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable;
        }

        // Only fdflag_nonblock is supported for sock_accept_wasm64. Any other bits are treated as invalid arguments.
        if((fd_flags & ~::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_nonblock) != ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t{})
            [[unlikely]]
        {
            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval;
        }

        [[maybe_unused]] bool const is_nonblock{(fd_flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_nonblock) ==
                                                ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_nonblock};

        // If ptr is null, it indicates an attempt to open a closed file. However, the preceding check for close pos already prevents such closed files from
        // being processed, making this a virtual machine implementation error.
        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
        {
// This will be checked at runtime.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
        }

        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
        {
            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
            }
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file_observer:
            {
# if defined(_WIN32) && !defined(__CYGWIN__)
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotsock;
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

                bool parent_is_nonblock{};

                {
                    auto const listen_flags{::uwvm2::imported::wasi::wasip1::func::posix::fcntl(curr_fd_native_file.native_handle(), F_GETFL, 0)};

                    if(listen_flags == -1) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(
                            ::fast_io::error{::fast_io::posix_domain_value, static_cast<::fast_io::error::value_type>(static_cast<unsigned>(errno))});
                    }

#  if defined(O_NONBLOCK)
                    parent_is_nonblock = (listen_flags & O_NONBLOCK) != 0;
#  endif
                }

                bool const is_nonblock_effective{is_nonblock || parent_is_nonblock};

#  if !(defined(O_NONBLOCK) && O_NONBLOCK != 0)
                if(is_nonblock) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotsup; }
#  endif

                struct ::sockaddr_storage addr{};
                ::fast_io::native_socklen_t addrlen{static_cast<::fast_io::native_socklen_t>(sizeof(addr))};

                ::fast_io::posix_file new_socket_file{};

#  ifdef UWVM_CPP_EXCEPTIONS
                try
#  endif
                {
                    new_socket_file = ::fast_io::posix_file{::fast_io::posix_accept(curr_fd_native_file, ::std::addressof(addr), ::std::addressof(addrlen))};
                }
#  ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error e)
                {
                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                }
#  endif

                // set nonblock flag

#  if defined(O_NONBLOCK) && O_NONBLOCK != 0
                auto flags{::uwvm2::imported::wasi::wasip1::func::posix::fcntl(new_socket_file.native_handle(), F_GETFL, 0)};

                if(flags == -1) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(
                        ::fast_io::error{::fast_io::posix_domain_value, static_cast<::fast_io::error::value_type>(static_cast<unsigned>(errno))});
                }

                if(is_nonblock_effective) { flags |= O_NONBLOCK; }
                else
                {
                    flags &= ~O_NONBLOCK;
                }

                if(::uwvm2::imported::wasi::wasip1::func::posix::fcntl(new_socket_file.native_handle(), F_SETFL, flags) == -1) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(
                        ::fast_io::error{::fast_io::posix_domain_value, static_cast<::fast_io::error::value_type>(static_cast<unsigned>(errno))});
                }
#  endif

                using fd_t = ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;

                fd_t new_fd{};

                // storage, This is new file descriptor; no need to lock it.
                ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t new_wasi_fd{};

                new_wasi_fd.fd_p->rights_base = curr_fd.rights_base & curr_fd.rights_inherit;
                new_wasi_fd.fd_p->rights_inherit = new_wasi_fd.fd_p->rights_base;
                new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
                new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.file_fd = ::std::move(new_socket_file);

                // Release locks that are no longer needed to avoid excessive lock granularity.
                curr_fd_release_guard.unlock();
                curr_fd_release_guard.device_p = nullptr;

                {
                    // Manipulating fd_manager requires a unique_lock.
                    ::uwvm2::utils::mutex::rw_fair_unique_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

                    // check limit
                    using fd_t = ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
                    if constexpr(::std::numeric_limits<::std::size_t>::max() > ::std::numeric_limits<fd_t>::max())
                    {
                        if(env.fd_storage.fd_limit > ::std::numeric_limits<fd_t>::max()) [[unlikely]]
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        }
                    }

                    auto const open_size{wasm_fd_storage.opens.size()};
                    auto const close_size{wasm_fd_storage.closes.size()};
                    auto const renumber_size{wasm_fd_storage.renumber_map.size()};

                    // There is no need to worry about overflow here, as all files in wasip1 are created using path_open and are incrementally assigned.
                    auto const all_size{open_size - close_size + renumber_size};
                    if(all_size >= env.fd_storage.fd_limit) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::emfile; }

                    // create new
                    if(!wasm_fd_storage.closes.empty())
                    {
                        auto const new_fd_pos{wasm_fd_storage.closes.back_unchecked()};
                        wasm_fd_storage.closes.pop_back_unchecked();

                        // Do not use `index_unchecked` here. Use a checked index `operator[]` to ensure safety.
                        auto& new_wasi_fd_p{wasm_fd_storage.opens[new_fd_pos]};

                        new_wasi_fd_p = ::std::move(new_wasi_fd);

                        new_fd = static_cast<fd_t>(new_fd_pos);
                    }
                    else
                    {

                        if constexpr(::std::numeric_limits<::std::size_t>::max() > ::std::numeric_limits<fd_t>::max())
                        {
                            if(wasm_fd_storage.opens.size() >= ::std::numeric_limits<fd_t>::max()) [[unlikely]]
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::emfile;
                            }
                        }

                        auto const& new_wasm_fd{wasm_fd_storage.opens.emplace_back(::std::move(new_wasi_fd))};

                        new_fd = static_cast<fd_t>(static_cast<::std::size_t>(::std::addressof(new_wasm_fd) - wasm_fd_storage.opens.cbegin()));

                        // To normalize it, start traversing from renumber. If it's an open size, move it to open vec.
                        auto open_size{wasm_fd_storage.opens.size()};

                        // Use while loop to avoid iterator invalidation during erase
                        for(;;)
                        {
                            auto it2{wasm_fd_storage.renumber_map.find(static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t>(open_size))};
                            if(it2 == wasm_fd_storage.renumber_map.end()) { break; }

                            wasm_fd_storage.opens.push_back(it2->second.release());
                            wasm_fd_storage.renumber_map.erase(it2);
                            ++open_size;
                        }
                    }
                }

                {
                    [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64_unlocked(memory, ro_fd_ptrsz, new_fd);

                    // Encode the peer address into the __wasi_addr_port_t layout used by WASIX:
                    //
                    //   tag: Addressfamily (1 = Inet4, 2 = Inet6)
                    //   _padding: always 0
                    //   u.octs: [port_be[0], port_be[1], ip bytes...]
                    //
                    // The buffer pointed to by ro_addr_ptrsz must hold size_of_wasi_addr_port_t bytes.
                    ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm64_unlocked(
                        memory,
                        ro_addr_ptrsz,
                        ::uwvm2::imported::wasi::wasip1::func::size_of_wasi_addr_port_t);

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8 raw[::uwvm2::imported::wasi::wasip1::func::size_of_wasi_addr_port_t]{};

                    auto* const raw_ptr{raw};

                    if(addrlen > 0u)
                    {
                        switch(addr.ss_family)
                        {
                            case AF_INET:
                            {
                                if(addrlen >= static_cast<::fast_io::native_socklen_t>(sizeof(::sockaddr_in)))
                                {
                                    using sockaddr_in_const_may_alias UWVM_GNU_MAY_ALIAS = ::sockaddr_in const*;

                                    auto const sin{reinterpret_cast<sockaddr_in_const_may_alias>(::std::addressof(addr))};

                                    // tag: Inet4
                                    raw_ptr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(1u);
                                    raw_ptr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(0u);

                                    // ::std::uint_least8_t = unsigned char, default may alias
                                    auto const port_bytes{reinterpret_cast<::std::uint_least8_t const*>(::std::addressof(sin->sin_port))};
                                    raw_ptr[2] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(port_bytes[0u]);
                                    raw_ptr[3] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(port_bytes[1u]);

                                    auto const ip_bytes{reinterpret_cast<::std::uint_least8_t const*>(::std::addressof(sin->sin_addr))};
                                    raw_ptr[4] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(ip_bytes[0u]);
                                    raw_ptr[5] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(ip_bytes[1u]);
                                    raw_ptr[6] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(ip_bytes[2u]);
                                    raw_ptr[7] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(ip_bytes[3u]);
                                }
                                break;
                            }
                            case AF_INET6:
                            {
                                if(addrlen >= static_cast<::fast_io::native_socklen_t>(sizeof(::sockaddr_in6)))
                                {
                                    using sockaddr_in6_const_may_alias UWVM_GNU_MAY_ALIAS = ::sockaddr_in6 const*;
                                    auto const sin6{reinterpret_cast<sockaddr_in6_const_may_alias>(::std::addressof(addr))};

                                    // tag: Inet6
                                    raw_ptr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(2u);
                                    raw_ptr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(0u);

                                    // ::std::uint_least8_t = unsigned char, default may alias
                                    auto const port_bytes{reinterpret_cast<::std::uint_least8_t const*>(::std::addressof(sin6->sin6_port))};
                                    raw_ptr[2] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(port_bytes[0u]);
                                    raw_ptr[3] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(port_bytes[1u]);

                                    auto const ip_bytes{reinterpret_cast<::std::uint_least8_t const*>(::std::addressof(sin6->sin6_addr))};
                                    for(unsigned i{}; i != 16u; ++i)
                                    {
                                        raw_ptr[4u + i] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(ip_bytes[i]);
                                    }
                                }
                                break;
                            }
                            [[unlikely]] default:
                            {
                                // Unknown family: leave all zeros (tag = Addressfamily::Unspec).
                                break;
                            }
                        }
                    }

                    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64_unchecked_unlocked(
                        memory,
                        ro_addr_ptrsz,
                        reinterpret_cast<::std::byte const*>(raw_ptr),
                        reinterpret_cast<::std::byte const*>(raw_ptr) + ::uwvm2::imported::wasi::wasip1::func::size_of_wasi_addr_port_t);
                }

                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess;
# endif
            }
            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotsock;
            }
# if defined(_WIN32) && !defined(__CYGWIN__)
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket_observer:
            {
                ::fast_io::win32_socket_io_observer curr_socket_observer{};

                bool const is_socket_observer{curr_fd.wasi_fd.ptr->wasi_fd_storage.type ==
                                              ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket_observer};

                if(is_socket_observer)
                {
                    auto& curr_socket_observer_ref{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.socket_observer};
                    curr_socket_observer = curr_socket_observer_ref;
                }
                else
                {
                    auto& curr_socket_observer_ref{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.socket_fd};
                    curr_socket_observer = curr_socket_observer_ref;
                }

                auto const& curr_fd_native_file{curr_socket_observer};

                ::fast_io::win32::sockaddr_storage addr{};
                ::fast_io::native_socklen_t addrlen{static_cast<::fast_io::native_socklen_t>(sizeof(addr))};

                ::fast_io::win32_socket_file new_socket_file{};

#  ifdef UWVM_CPP_EXCEPTIONS
                try
#  endif
                {
                    new_socket_file =
                        ::fast_io::win32_socket_file{::fast_io::posix_accept(curr_fd_native_file, ::std::addressof(addr), ::std::addressof(addrlen))};
                }
#  ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error e)
                {
#   if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(e.domain != ::fast_io::win32_domain_value) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#   endif
                    switch(e.code)
                    {
                        // Winsock (WSA*) error codes mapping (subset relevant to accept)
                        case 10004 /*WSAEINTR*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eintr;
                        case 10009 /*WSAEBADF*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        case 10013 /*WSAEACCES*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces;
                        case 10014 /*WSAEFAULT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::efault;
                        case 10022 /*WSAEINVAL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval;
                        case 10024 /*WSAEMFILE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::emfile;
                        case 10035 /*WSAEWOULDBLOCK*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eagain;
                        case 10036 /*WSAEINPROGRESS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einprogress;
                        case 10037 /*WSAEALREADY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ealready;
                        case 10038 /*WSAENOTSOCK*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotsock;
                        case 10050 /*WSAENETDOWN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enetdown;
                        case 10051 /*WSAENETUNREACH*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enetunreach;
                        case 10052 /*WSAENETRESET*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enetreset;
                        case 10053 /*WSAECONNABORTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::econnaborted;
                        case 10054 /*WSAECONNRESET*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::econnreset;
                        case 10055 /*WSAENOBUFS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enobufs;
                        case 10056 /*WSAEISCONN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eisconn;
                        case 10057 /*WSAENOTCONN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotconn;
                        case 10058 /*WSAESHUTDOWN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotconn;
                        case 10060 /*WSAETIMEDOUT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::etimedout;
                        case 10061 /*WSAECONNREFUSED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::econnrefused;
                        case 10064 /*WSAEHOSTDOWN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ehostunreach;
                        case 10065 /*WSAEHOSTUNREACH*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ehostunreach;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                    }
                }
#  endif

                // set nonblock flag

                ::std::uint_least32_t mode{static_cast<::std::uint_least32_t>(is_nonblock)};

                if(::fast_io::win32::ioctlsocket(new_socket_file.native_handle(), 0x8004'667El /*FIONBIO*/, ::std::addressof(mode)) == -1) [[unlikely]]
                {
                    switch(::fast_io::win32::WSAGetLastError())
                    {
                        case 10022 /*WSAEINVAL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval;
                        case 10004 /*WSAEINTR*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eintr;
                        case 10038 /*WSAENOTSOCK*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf;
                        case 10013 /*WSAEACCES*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces;
                        case 10050 /*WSAENETDOWN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        case 10055 /*WSAENOBUFS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enobufs;
                        case 10014 /*WSAEFAULT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::efault;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                    }
                }

                using fd_t = ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;

                fd_t new_fd{};

                // storage, This is new file descriptor; no need to lock it.
                ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t new_wasi_fd{};

                new_wasi_fd.fd_p->rights_base = curr_fd.rights_base & curr_fd.rights_inherit;
                new_wasi_fd.fd_p->rights_inherit = new_wasi_fd.fd_p->rights_base;
                new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket);
                new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.socket_fd = ::std::move(new_socket_file);

                // Release locks that are no longer needed to avoid excessive lock granularity.
                curr_fd_release_guard.unlock();
                curr_fd_release_guard.device_p = nullptr;

                {
                    // Manipulating fd_manager requires a unique_lock.
                    ::uwvm2::utils::mutex::rw_fair_unique_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

                    // check limit
                    using fd_t = ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
                    if constexpr(::std::numeric_limits<::std::size_t>::max() > ::std::numeric_limits<fd_t>::max())
                    {
                        if(env.fd_storage.fd_limit > ::std::numeric_limits<fd_t>::max()) [[unlikely]]
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        }
                    }

                    auto const open_size{wasm_fd_storage.opens.size()};
                    auto const close_size{wasm_fd_storage.closes.size()};
                    auto const renumber_size{wasm_fd_storage.renumber_map.size()};

                    // There is no need to worry about overflow here, as all files in wasip1 are created using path_open and are incrementally assigned.
                    auto const all_size{open_size - close_size + renumber_size};
                    if(all_size >= env.fd_storage.fd_limit) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::emfile; }

                    // create new
                    if(!wasm_fd_storage.closes.empty())
                    {
                        auto const new_fd_pos{wasm_fd_storage.closes.back_unchecked()};
                        wasm_fd_storage.closes.pop_back_unchecked();

                        // Do not use `index_unchecked` here. Use a checked index `operator[]` to ensure safety.
                        auto& new_wasi_fd_p{wasm_fd_storage.opens[new_fd_pos]};

                        new_wasi_fd_p = ::std::move(new_wasi_fd);

                        new_fd = static_cast<fd_t>(new_fd_pos);
                    }
                    else
                    {

                        if constexpr(::std::numeric_limits<::std::size_t>::max() > ::std::numeric_limits<fd_t>::max())
                        {
                            if(wasm_fd_storage.opens.size() >= ::std::numeric_limits<fd_t>::max()) [[unlikely]]
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::emfile;
                            }
                        }

                        auto const& new_wasm_fd{wasm_fd_storage.opens.emplace_back(::std::move(new_wasi_fd))};

                        new_fd = static_cast<fd_t>(static_cast<::std::size_t>(::std::addressof(new_wasm_fd) - wasm_fd_storage.opens.cbegin()));

                        // To normalize it, start traversing from renumber. If it's an open size, move it to open vec.
                        auto open_size{wasm_fd_storage.opens.size()};

                        // Use while loop to avoid iterator invalidation during erase
                        for(;;)
                        {
                            auto it2{wasm_fd_storage.renumber_map.find(static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t>(open_size))};
                            if(it2 == wasm_fd_storage.renumber_map.end()) { break; }

                            wasm_fd_storage.opens.push_back(it2->second.release());
                            wasm_fd_storage.renumber_map.erase(it2);
                            ++open_size;
                        }
                    }
                }

                {
                    [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                    ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64_unlocked(memory, ro_fd_ptrsz, new_fd);

                    // Encode the peer address into the __wasi_addr_port_t layout used by WASIX:
                    //
                    //   tag: Addressfamily (1 = Inet4, 2 = Inet6)
                    //   _padding: always 0
                    //   u.octs: [port_be[0], port_be[1], ip bytes...]
                    //
                    // The buffer pointed to by ro_addr_ptrsz must hold size_of_wasi_addr_port_t bytes.
                    ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm64_unlocked(
                        memory,
                        ro_addr_ptrsz,
                        ::uwvm2::imported::wasi::wasip1::func::size_of_wasi_addr_port_t);

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8 raw[::uwvm2::imported::wasi::wasip1::func::size_of_wasi_addr_port_t]{};

                    auto* const raw_ptr{raw};

                    if(addrlen > 0)
                    {
                        switch(addr.ss_family)
                        {
                            case 2 /*AF_INET*/:
                            {
                                if(addrlen >= static_cast<::fast_io::native_socklen_t>(sizeof(::fast_io::win32::sockaddr_in)))
                                {
                                    using sockaddr_in_const_may_alias UWVM_GNU_MAY_ALIAS = ::fast_io::win32::sockaddr_in const*;
                                    auto const sin{reinterpret_cast<sockaddr_in_const_may_alias>(::std::addressof(addr))};

                                    // tag: Inet4
                                    raw_ptr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(1u);
                                    raw_ptr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(0u);

                                    // ::std::uint_least8_t = unsigned char, default may alias
                                    auto const port_bytes{reinterpret_cast<::std::uint_least8_t const*>(::std::addressof(sin->sin_port))};
                                    raw_ptr[2] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(port_bytes[0u]);
                                    raw_ptr[3] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(port_bytes[1u]);

                                    auto const ip_bytes{reinterpret_cast<::std::uint_least8_t const*>(::std::addressof(sin->sin_addr))};
                                    raw_ptr[4] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(ip_bytes[0u]);
                                    raw_ptr[5] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(ip_bytes[1u]);
                                    raw_ptr[6] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(ip_bytes[2u]);
                                    raw_ptr[7] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(ip_bytes[3u]);
                                }
                                break;
                            }
                            case 23 /*AF_INET6*/:
                            {
                                if(addrlen >= static_cast<::fast_io::native_socklen_t>(sizeof(::fast_io::win32::sockaddr_in6)))
                                {
                                    using sockaddr_in6_const_may_alias UWVM_GNU_MAY_ALIAS = ::fast_io::win32::sockaddr_in6 const*;
                                    auto const sin6{reinterpret_cast<sockaddr_in6_const_may_alias>(::std::addressof(addr))};

                                    // tag: Inet6
                                    raw_ptr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(2u);
                                    raw_ptr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(0u);

                                    // ::std::uint_least8_t = unsigned char, default may alias
                                    auto const port_bytes{reinterpret_cast<::std::uint_least8_t const*>(::std::addressof(sin6->sin6_port))};
                                    raw_ptr[2] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(port_bytes[0u]);
                                    raw_ptr[3] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(port_bytes[1u]);

                                    auto const ip_bytes{reinterpret_cast<::std::uint_least8_t const*>(::std::addressof(sin6->sin6_addr))};
                                    for(::std::size_t i{}; i != 16u; ++i)
                                    {
                                        raw_ptr[4u + i] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>(ip_bytes[i]);
                                    }
                                }
                                break;
                            }
                            [[unlikely]] default:
                            {
                                // Unknown family: leave all zeros (tag = Addressfamily::Unspec).
                                break;
                            }
                        }
                    }

                    ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64_unchecked_unlocked(
                        memory,
                        ro_addr_ptrsz,
                        reinterpret_cast<::std::byte const*>(raw_ptr),
                        reinterpret_cast<::std::byte const*>(raw_ptr) + ::uwvm2::imported::wasi::wasip1::func::size_of_wasi_addr_port_t);
                }

                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess;
            }
# endif
            [[unlikely]] default:
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
            }
        }
    }
}

#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

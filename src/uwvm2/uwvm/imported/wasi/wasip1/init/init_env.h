/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-27
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
# include <limits>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/imported/wasi/wasip1/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::imported::wasi::wasip1::storage
{
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)

    /// @note This can only be used when initialization occurs before WASM execution, so no locks are added here.
    template <::uwvm2::imported::wasi::wasip1::environment::wasip1_memory memory_type>
    inline constexpr bool init_wasip1_environment(::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<memory_type> & env) noexcept
    {
        auto const print_init_error{[](::uwvm2::utils::container::u8string_view msg) constexpr noexcept
                                    {
                                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                            u8"uwvm: ",
                                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                                            u8"[error] ",
                                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                            u8"Initialization error in the wasip1 environment: ",
                                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                            msg,
                                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                                            u8"\n\n");
                                    }};

        /// @brief   Clear fd storage
        env.fd_storage.opens.clear();
        env.fd_storage.closes.clear();
        env.fd_storage.renumber_map.clear();

        ::std::size_t const fd_limit{env.fd_storage.fd_limit};

        using fd_t = ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
        using fd_map_t = ::uwvm2::utils::container::map<fd_t, ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t>;
        fd_map_t fd_map{};

        auto const try_emplace_fd{
            [&fd_map, &print_init_error](fd_t fd, ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t&& p) constexpr noexcept -> bool
            {
                [[maybe_unused]] auto [it, inserted]{fd_map.emplace(fd, ::std::move(p))};
                if(!inserted) [[unlikely]]
                {
                    print_init_error(u8"duplicate preopened fd");
                    return false;
                }
                return true;
            }};

        auto const init_stdio{[&print_init_error](::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t& new_fd_fd,
                                                  ::fast_io::native_io_observer obs,
                                                  bool writable) constexpr noexcept -> bool
                              {
                                  using rights_t = ::uwvm2::imported::wasi::wasip1::abi::rights_t;

                                  new_fd_fd.rights_base = writable ? rights_t::right_fd_write : rights_t::right_fd_read;
                                  new_fd_fd.rights_inherit = new_fd_fd.rights_base;

#  if !defined(__AVR__) && !((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) && !(defined(__MSDOS__) || defined(__DJGPP__)) &&             \
      !(defined(__NEWLIB__) && !defined(__CYGWIN__)) && !defined(_PICOLIBC__) && !defined(__wasm__)
                                  // can dup
                                  new_fd_fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
#   ifdef UWVM_CPP_EXCEPTIONS
                                  try
#   endif
                                  {
#   if defined(_WIN32) && !defined(__CYGWIN__)
                                      new_fd_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file = ::fast_io::native_file{::fast_io::io_dup, obs};
#   else
                                      new_fd_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd = ::fast_io::native_file{::fast_io::io_dup, obs};
#   endif
                                  }
#   ifdef UWVM_CPP_EXCEPTIONS
                                  catch(::fast_io::error)
                                  {
                                      print_init_error(u8"dup stdio failed");
                                      return false;
                                  }
#   endif
#  else
                                  // cannot dup, use observer
                                  new_fd_fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file_observer);
                                  new_fd_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_observer = obs;
#  endif

                                  return true;
                              }};

        // stdio (fd0, fd1, fd2)
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t fd0{};
            if(!init_stdio(*fd0.fd_p, ::fast_io::in(), false)) [[unlikely]] { return false; }
            if(!try_emplace_fd(static_cast<fd_t>(0), ::std::move(fd0))) [[unlikely]] { return false; }
        }
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t fd1{};
            if(!init_stdio(*fd1.fd_p, ::fast_io::out(), true)) [[unlikely]] { return false; }
            if(!try_emplace_fd(static_cast<fd_t>(1), ::std::move(fd1))) [[unlikely]] { return false; }
        }
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t fd2{};
            if(!init_stdio(*fd2.fd_p, ::fast_io::err(), true)) [[unlikely]] { return false; }
            if(!try_emplace_fd(static_cast<fd_t>(2), ::std::move(fd2))) [[unlikely]] { return false; }
        }

        // preopened sockets
        // Note: This function does not modify the host's SIGPIPE handling; socket ops handle it (e.g. MSG_NOSIGNAL in sock_send).
        for(auto const& ps: env.preopen_sockets)
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t new_sock_fd{};

            new_sock_fd.fd_p->rights_base = static_cast<::uwvm2::imported::wasi::wasip1::abi::rights_t>(-1);
            new_sock_fd.fd_p->rights_inherit = new_sock_fd.fd_p->rights_base;

#  ifdef UWVM_CPP_EXCEPTIONS
            try
#  endif
            {
                ::fast_io::native_socket_file sock{ps.sock_family, ps.sock_type, ::fast_io::open_mode{}, ps.sock_protocol};

                if(ps.sock_family == ::uwvm2::imported::wasi::wasip1::environment::sock_family_t::local) [[unlikely]]
                {
                    print_init_error(u8"local(unix) socket preopen not implemented in init");
                    return false;
                }

                if(ps.sock_family == ::uwvm2::imported::wasi::wasip1::environment::sock_family_t::inet && !ps.ip.address.isv4) [[unlikely]]
                {
                    print_init_error(u8"socket family mismatch (inet but not ipv4)");
                    return false;
                }
                if(ps.sock_family == ::uwvm2::imported::wasi::wasip1::environment::sock_family_t::inet6 && ps.ip.address.isv4) [[unlikely]]
                {
                    print_init_error(u8"socket family mismatch (inet6 but ipv4)");
                    return false;
                }

                if(ps.ip.address.isv4)
                {
                    ::fast_io::posix_sockaddr_in in{.sin_family = ::fast_io::to_posix_sock_family(::fast_io::sock_family::inet),
                                                    .sin_port = ::fast_io::big_endian(ps.ip.port),
                                                    .sin_addr = ps.ip.address.address.v4};

                    if(ps.handle_type == ::uwvm2::imported::wasi::wasip1::environment::handle_type_e::connect)
                    {
                        ::fast_io::posix_connect(sock, ::std::addressof(in), sizeof(in));
                    }
                    else
                    {
                        ::fast_io::posix_bind(sock, ::std::addressof(in), sizeof(in));
                        if(ps.handle_type == ::uwvm2::imported::wasi::wasip1::environment::handle_type_e::listen) { ::fast_io::posix_listen(sock, 128); }
                    }
                }
                else
                {
                    ::fast_io::posix_sockaddr_in6 in6{.sin6_family = ::fast_io::to_posix_sock_family(::fast_io::sock_family::inet6),
                                                      .sin6_port = ::fast_io::big_endian(ps.ip.port),
                                                      .sin6_flowinfo = 0,
                                                      .sin6_addr = ps.ip.address.address.v6,
                                                      .sin6_scoped_id = 0};

                    if(ps.handle_type == ::uwvm2::imported::wasi::wasip1::environment::handle_type_e::connect)
                    {
                        ::fast_io::posix_connect(sock, ::std::addressof(in6), sizeof(in6));
                    }
                    else
                    {
                        ::fast_io::posix_bind(sock, ::std::addressof(in6), sizeof(in6));
                        if(ps.handle_type == ::uwvm2::imported::wasi::wasip1::environment::handle_type_e::listen) { ::fast_io::posix_listen(sock, 128); }
                    }
                }

#  if defined(_WIN32) && !defined(__CYGWIN__)
                new_sock_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket);
                new_sock_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.socket_fd = ::std::move(sock);
#  else
                new_sock_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
                new_sock_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.file_fd = ::std::move(sock);
#  endif
            }
#  ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                print_init_error(u8"preopen socket init failed");
                return false;
            }
#  endif

            if(!try_emplace_fd(ps.fd, ::std::move(new_sock_fd))) [[unlikely]] { return false; }
        }

        // preopened directories: assign from fd3, skipping occupied fds
        fd_t next_dir_fd{static_cast<fd_t>(3)};
        for(auto const& mr: env.mount_dir_roots)
        {
            while(fd_map.find(next_dir_fd) != fd_map.end()) [[unlikely]]
            {
                if(next_dir_fd == static_cast<fd_t>(::std::numeric_limits<fd_t>::max())) [[unlikely]]
                {
                    print_init_error(u8"fd exhausted");
                    return false;
                }
                ++next_dir_fd;
            }

            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t new_dir_fd{};
            new_dir_fd.fd_p->rights_base = static_cast<::uwvm2::imported::wasi::wasip1::abi::rights_t>(-1);
            new_dir_fd.fd_p->rights_inherit = new_dir_fd.fd_p->rights_base;

            new_dir_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);

            // preload stack (exactly 1 element)
            auto& dir_stack_vec{new_dir_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.dir_stack.dir_stack};
            auto& new_entry_ref{dir_stack_vec.emplace_back()};
            auto& new_entry{new_entry_ref.ptr->dir_stack};

            new_entry.name = mr.preload_dir;

#  if !defined(__AVR__) && !((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) && !(defined(__MSDOS__) || defined(__DJGPP__)) &&             \
      !(defined(__NEWLIB__) && !defined(__CYGWIN__)) && !defined(_PICOLIBC__) && !defined(__wasm__)
            // can dup
            new_entry.is_observer = false;
#   ifdef UWVM_CPP_EXCEPTIONS
            try
#   endif
            {
                new_entry.storage.file = ::fast_io::dir_file{::fast_io::io_dup, mr.entry};
            }
#   ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                print_init_error(u8"dup preopen dir failed");
                return false;
            }
#   endif
#  else
            // cannot dup, use observer
            new_entry.is_observer = true;
            new_entry.storage.observer = ::fast_io::dir_io_observer{mr.entry};
#  endif

            if(!try_emplace_fd(next_dir_fd, ::std::move(new_dir_fd))) [[unlikely]] { return false; }
            ++next_dir_fd;
        }

        // fd limit check
        if(fd_map.size() > fd_limit) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Initialization error in the wasip1 environment: fd limit exceeded (",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                fd_limit,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8")\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            return false;
        }

        // materialize into opens + renumber_map (no holes in opens)
        fd_t fd_cursor{};
        for(;; ++fd_cursor)
        {
            auto it{fd_map.find(fd_cursor)};
            if(it == fd_map.end()) { break; }
            env.fd_storage.opens.emplace_back(::std::move(it->second));
            fd_map.erase(it);
        }

        for(auto& [fd, uni]: fd_map) { env.fd_storage.renumber_map.emplace(fd, ::std::move(uni)); }

        return true;
    }

# endif
#endif
}  // namespace uwvm2::uwvm::imported::wasi::wasip1::storage

#ifndef UWVM_MODULE
// macro
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

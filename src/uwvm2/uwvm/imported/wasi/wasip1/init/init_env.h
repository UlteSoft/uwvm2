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
# include <cstring>
# include <cerrno>
# include <limits>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
// system
# if defined(UWVM_SUPPORT_UNIX_PATH_SOCKET)
#  if __has_include(<sys/un.h>)
#   include <sys/un.h>
#  endif
#  if __has_include(<unistd.h>)
#   include <unistd.h>
#  endif
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

#  if defined(UWVM_SUPPORT_UNIX_PATH_SOCKET) && __has_include(<sys/un.h>) && defined(AF_LOCAL) && __has_include(<unistd.h>)
    namespace posix
    {
        extern int unlink(char const* path) noexcept
#   if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("unlink")
#   else
            __asm__("_unlink")
#   endif
                ;
    }  // namespace posix
#  endif

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

        auto const print_init_error_with_fast_io_error{
            [](::uwvm2::utils::container::u8string_view msg, ::fast_io::error e) constexpr noexcept
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
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8" (",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    e,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8")\n\n",
                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }};

        // ====== Stronger failure semantics (internal) ======
        // Do not mutate `env.fd_storage` until we know initialization succeeds.
        // Note: `--wasip1-set-fd-limit 0` is translated to `fd_t::max()` by the cmdline callback; here `0` means "unset".
        auto const fd_limit_before{env.fd_storage.fd_limit};
        ::std::size_t fd_limit{fd_limit_before};
        if(fd_limit == 0uz) [[unlikely]] { fd_limit = 1024uz; }

        using fd_t = ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
        using fd_map_t = ::uwvm2::utils::container::map<fd_t, ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t>;
        fd_map_t fd_map{};

        // env.fd_storage.opens always contains at least stdio (fd0..fd2).
        if(fd_limit < 3uz) [[unlikely]]
        {
            print_init_error(u8"fd_limit too small (need >= 3 for stdio)");
            return false;
        }

        if constexpr(::std::numeric_limits<::std::size_t>::max() > static_cast<::std::size_t>(::std::numeric_limits<fd_t>::max()))
        {
            if(fd_limit > static_cast<::std::size_t>(::std::numeric_limits<fd_t>::max())) [[unlikely]]
            {
                print_init_error(u8"fd_limit exceeds fd_t maximum");
                return false;
            }
        }

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

        auto const init_stdio{[&print_init_error_with_fast_io_error](::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t& new_fd_fd,
                                                                     ::fast_io::native_io_observer obs) constexpr noexcept -> bool
                              {
                                  using rights_t = ::uwvm2::imported::wasi::wasip1::abi::rights_t;

                                  // Keep stdio permissive by default: OS-level capabilities are enforced by the host anyway.
                                  new_fd_fd.rights_base = static_cast<rights_t>(-1);
                                  new_fd_fd.rights_inherit = static_cast<rights_t>(-1);

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
                                  catch(::fast_io::error e)
                                  {
                                      print_init_error_with_fast_io_error(u8"dup stdio failed", e);
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
            if(!init_stdio(*fd0.fd_p, ::fast_io::in())) [[unlikely]] { return false; }
            if(!try_emplace_fd(static_cast<fd_t>(0), ::std::move(fd0))) [[unlikely]] { return false; }
        }
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t fd1{};
            if(!init_stdio(*fd1.fd_p, ::fast_io::out())) [[unlikely]] { return false; }
            if(!try_emplace_fd(static_cast<fd_t>(1), ::std::move(fd1))) [[unlikely]] { return false; }
        }
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t fd2{};
            if(!init_stdio(*fd2.fd_p, ::fast_io::err())) [[unlikely]] { return false; }
            if(!try_emplace_fd(static_cast<fd_t>(2), ::std::move(fd2))) [[unlikely]] { return false; }
        }

        // preopened sockets
        // Note: This function does not modify the host's SIGPIPE handling; socket ops handle it (e.g. MSG_NOSIGNAL in sock_send).
        for(auto const& ps: env.preopen_sockets)
        {
            if(ps.fd < static_cast<fd_t>(0)) [[unlikely]]
            {
                print_init_error(u8"preopened socket fd is negative");
                return false;
            }

            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t new_sock_fd{};

            new_sock_fd.fd_p->rights_base = static_cast<::uwvm2::imported::wasi::wasip1::abi::rights_t>(-1);
            new_sock_fd.fd_p->rights_inherit = static_cast<::uwvm2::imported::wasi::wasip1::abi::rights_t>(-1);

#  ifdef UWVM_CPP_EXCEPTIONS
            try
#  endif
            {
                ::fast_io::native_socket_file sock{ps.sock_family, ps.sock_type, ::fast_io::open_mode{}, ps.sock_protocol};

                if(ps.sock_family == ::uwvm2::imported::wasi::wasip1::environment::sock_family_t::local)
                {
#  if defined(UWVM_SUPPORT_UNIX_PATH_SOCKET) && __has_include(<sys/un.h>) && defined(AF_LOCAL)
                    auto const& local_path_u8{ps.local_unix_path};
                    if(local_path_u8.empty()) [[unlikely]]
                    {
                        print_init_error(u8"unix socket path is empty");
                        return false;
                    }

                    struct ::sockaddr_un un{};
                    un.sun_family = static_cast<decltype(un.sun_family)>(AF_LOCAL);

                    bool abstract_namespace{};
                    ::std::size_t copy_len{local_path_u8.size()};

#   if defined(__linux__)
                    if(local_path_u8.front() == u8'@') [[unlikely]]
                    {
                        abstract_namespace = true;
                        copy_len = local_path_u8.size() - 1uz;
                    }
#   endif

                    if(abstract_namespace)
                    {
                        if(copy_len >= sizeof(un.sun_path)) [[unlikely]]
                        {
                            print_init_error(u8"unix socket path too long");
                            return false;
                        }

                        un.sun_path[0u] = '\0';
                        if(copy_len != 0uz) { ::std::memcpy(un.sun_path + 1, reinterpret_cast<char const*>(local_path_u8.data() + 1u), copy_len); }
                    }
                    else
                    {
                        if(copy_len >= sizeof(un.sun_path)) [[unlikely]]
                        {
                            print_init_error(u8"unix socket path too long");
                            return false;
                        }

                        // `unlink()` below treats `un.sun_path` as a C-string; reject embedded NULs to avoid truncation surprises.
                        if(::std::memchr(local_path_u8.data(), 0, local_path_u8.size()) != nullptr) [[unlikely]]
                        {
                            print_init_error(u8"unix socket path contains NUL");
                            return false;
                        }

                        ::std::memcpy(un.sun_path, reinterpret_cast<char const*>(local_path_u8.data()), copy_len);
                        un.sun_path[copy_len] = '\0';
                    }

                    auto const base_len{static_cast<::fast_io::posix_socklen_t>(__builtin_offsetof(::sockaddr_un, sun_path))};
                    // For filesystem namespace: include trailing NUL (we set `un.sun_path[copy_len] = '\0'`).
                    // For abstract namespace: `copy_len` excludes the leading '@', and the "+1" accounts for the leading NUL at `un.sun_path[0]`.
                    auto const addr_len{static_cast<::fast_io::posix_socklen_t>(base_len + copy_len + 1u)};

                    if constexpr(requires(::sockaddr_un tmp) { tmp.sun_len; })
                    {
                        using sun_len_t = decltype(un.sun_len);
                        if(addr_len > ::std::numeric_limits<sun_len_t>::max()) [[unlikely]]
                        {
                            print_init_error(u8"unix socket sockaddr too large");
                            return false;
                        }
                        un.sun_len = static_cast<sun_len_t>(addr_len);
                    }

                    if(ps.handle_type != ::uwvm2::imported::wasi::wasip1::environment::handle_type_e::connect && !abstract_namespace)
                    {
#   if __has_include(<unistd.h>)
                        errno = 0;
                        auto const unlink_ret{posix::unlink(un.sun_path)};
                        if(unlink_ret == -1 && errno != ENOENT) [[unlikely]]
                        {
                            print_init_error(u8"unlink existing unix socket file failed");
                            return false;
                        }
#   endif
                    }

                    if(ps.handle_type == ::uwvm2::imported::wasi::wasip1::environment::handle_type_e::connect)
                    {
                        ::fast_io::posix_connect(sock, ::std::addressof(un), addr_len);
                    }
                    else
                    {
                        ::fast_io::posix_bind(sock, ::std::addressof(un), addr_len);
                        if(ps.handle_type == ::uwvm2::imported::wasi::wasip1::environment::handle_type_e::listen) { ::fast_io::posix_listen(sock, 128); }
                    }
#  else
                    print_init_error(u8"local(unix) socket preopen unsupported on this platform");
                    return false;
#  endif
                }
                else
                {
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
            catch(::fast_io::error e)
            {
                print_init_error_with_fast_io_error(u8"preopen socket init failed", e);
                return false;
            }
#  endif

            if(!try_emplace_fd(ps.fd, ::std::move(new_sock_fd))) [[unlikely]] { return false; }
        }

        // preopened directories: assign from fd3, skipping occupied fds (including explicit socket fds)
        // Since they are all continuous (even if sockets are interspersed, they can still be scanned simultaneously by preopen)
        ::std::size_t next_dir_fd_u{3uz};
        constexpr ::std::size_t fd_t_max_u{static_cast<::std::size_t>(::std::numeric_limits<fd_t>::max())};
        for(auto const& mr: env.mount_dir_roots)
        {
            fd_t next_dir_fd{};
            for(;;)
            {
                if(next_dir_fd_u > fd_t_max_u) [[unlikely]]
                {
                    print_init_error(u8"fd exhausted");
                    return false;
                }

                next_dir_fd = static_cast<fd_t>(next_dir_fd_u);
                if(fd_map.find(next_dir_fd) == fd_map.end()) { break; }
                ++next_dir_fd_u;
            }

            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t new_dir_fd{};
            new_dir_fd.fd_p->rights_base = static_cast<::uwvm2::imported::wasi::wasip1::abi::rights_t>(-1);
            new_dir_fd.fd_p->rights_inherit = static_cast<::uwvm2::imported::wasi::wasip1::abi::rights_t>(-1);

            new_dir_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);

            // preload stack (exactly 1 element)
            auto& dir_stack_vec{new_dir_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.dir_stack.dir_stack};
            auto& new_entry_ref{dir_stack_vec.emplace_back()};
            auto& new_entry{new_entry_ref.ptr->dir_stack};

#  if !defined(__AVR__) && !((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) && !(defined(__MSDOS__) || defined(__DJGPP__)) &&             \
      !(defined(__NEWLIB__) && !defined(__CYGWIN__)) && !defined(_PICOLIBC__) && !defined(__wasm__)
            // can dup

            // default: new_entry.is_observer = false;
            new_entry.name = mr.preload_dir;

#   ifdef UWVM_CPP_EXCEPTIONS
            try
#   endif
            {
                ::fast_io::native_file tmp{::fast_io::io_dup, mr.entry};
                new_entry.storage.file = ::fast_io::dir_file{tmp.release()};
            }
#   ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error e)
            {
                print_init_error_with_fast_io_error(u8"dup preopen dir failed", e);
                return false;
            }
#   endif
#  else
            // cannot dup, use observer
            // dir_stack_entry_t default-constructs the `file` union member; switch to observer safely.
            new_entry = ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_t{true};
            new_entry.name = mr.preload_dir;
            new_entry.storage.observer = ::fast_io::dir_io_observer{mr.entry};
#  endif

            if(!try_emplace_fd(next_dir_fd, ::std::move(new_dir_fd))) [[unlikely]] { return false; }
            ++next_dir_fd_u;
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
        ::uwvm2::utils::container::vector<::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t> opens_new{};
        ::uwvm2::utils::container::map<fd_t, ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t> renumber_map_new{};

        fd_t fd_cursor{};
        for(;;)
        {
            auto it{fd_map.find(fd_cursor)};
            if(it == fd_map.end()) { break; }
            opens_new.emplace_back(::std::move(it->second));
            fd_map.erase(it);

            // Avoid signed overflow on fd_t (wasi_posix_fd_t is wasm_i32).
            if(fd_cursor == ::std::numeric_limits<fd_t>::max()) { break; }
            ++fd_cursor;
        }

        for(auto& [fd, uni]: fd_map) { renumber_map_new.emplace(fd, ::std::move(uni)); }

        // commit (swap destroys old state on success; env remains unchanged on failure)
        env.fd_storage.opens.swap(opens_new);
        env.fd_storage.renumber_map.swap(renumber_map_new);
        env.fd_storage.closes.clear();
        if(fd_limit_before == 0uz) [[unlikely]] { env.fd_storage.fd_limit = fd_limit; }

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

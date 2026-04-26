/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-04-25
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
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/storage/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include "wasip1_mount_dir.h"
# if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
#  include "wasip1_socket_tcp_connect.h"
#  include "wasip1_socket_tcp_listen.h"
#  include "wasip1_socket_udp_bind.h"
#  include "wasip1_socket_udp_connect.h"
# endif
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)
    namespace wasip1_module_details
    {
        using target_kind = ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_target_kind_t;
        using override_state_t = ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_override_t;
        using mount_dir_root_t = ::uwvm2::imported::wasi::wasip1::environment::mount_dir_root_t;
#  if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
        using preopen_socket_t = ::uwvm2::imported::wasi::wasip1::environment::preopen_socket_t;
#  endif

        [[nodiscard]] inline constexpr bool target_needs_module_name(target_kind kind) noexcept
        {
            return kind != target_kind::main_wasm;
        }

        [[nodiscard]] inline constexpr bool parse_target_kind(::uwvm2::utils::container::u8string_view text, target_kind& out) noexcept
        {
            if(text == u8"main")
            {
                out = target_kind::main_wasm;
                return true;
            }
            if(text == u8"preload-wasm")
            {
                out = target_kind::preload_wasm;
                return true;
            }
            if(text == u8"dl")
            {
                out = target_kind::preloaded_dl;
                return true;
            }
            if(text == u8"weak")
            {
                out = target_kind::weak_symbol;
                return true;
            }
            return false;
        }

        [[nodiscard]] inline constexpr bool target_is_native_preload(target_kind kind) noexcept
        {
            return kind == target_kind::preloaded_dl || kind == target_kind::weak_symbol;
        }

        [[nodiscard]] inline bool validate_wasm_utf8_name(::uwvm2::utils::container::u8string_view text) noexcept
        {
            auto const [module_name_pos, module_name_err]{
                ::uwvm2::uwvm::wasm::feature::handle_text_format(::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_text_format_wapper,
                                                                 text.cbegin(),
                                                                 text.cend())};
            static_cast<void>(module_name_pos);
            return module_name_err == ::uwvm2::utils::utf::utf_error_code::success;
        }

        inline ::uwvm2::utils::cmdline::parameter_return_type
            print_usage_error(::uwvm2::utils::cmdline::parameter const& parameter,
                              ::uwvm2::utils::container::u8string_view msg) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                msg,
                                u8" Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(parameter),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        [[nodiscard]] inline ::uwvm2::utils::cmdline::parameter_return_type apply_mount_dir_to_override(override_state_t& target,
                                                                                                         ::uwvm2::utils::container::u8cstring_view wasi_dir,
                                                                                                         ::uwvm2::utils::container::u8cstring_view system_dir) noexcept
        {
            auto const saved_global_disable_utf8_check{::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.disable_utf8_check};
            auto saved_global_mount_dir_roots{::std::move(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.mount_dir_roots)};
            auto saved_target_mount_dir_roots{::std::move(target.mount_dir_roots)};

            auto const global_mount_dir_count{saved_global_mount_dir_roots.size()};

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.mount_dir_roots = ::std::move(saved_global_mount_dir_roots);
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.mount_dir_roots.reserve(
                global_mount_dir_count + saved_target_mount_dir_roots.size());
            for(auto& mount_root: saved_target_mount_dir_roots)
            {
                ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.mount_dir_roots.emplace_back(::std::move(mount_root));
            }

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.disable_utf8_check =
                target.disable_utf8_check_is_set ? target.disable_utf8_check : saved_global_disable_utf8_check;

            ::uwvm2::utils::cmdline::parameter_parsing_results fake_results[]{
                {.str = u8"--wasip1-mount-dir", .para = ::std::addressof(::uwvm2::uwvm::cmdline::params::wasip1_mount_dir),
                 .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::parameter},
                {.str = wasi_dir, .para = nullptr, .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg},
                {.str = system_dir, .para = nullptr, .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg}};

            auto const ret{
                ::uwvm2::uwvm::cmdline::params::details::wasip1_mount_dir_callback(fake_results, fake_results, fake_results + (sizeof(fake_results) / sizeof(fake_results[0])))};

            ::uwvm2::utils::container::vector<mount_dir_root_t> restored_global_mount_dir_roots{};
            restored_global_mount_dir_roots.reserve(global_mount_dir_count);

            ::uwvm2::utils::container::vector<mount_dir_root_t> restored_target_mount_dir_roots{};
            restored_target_mount_dir_roots.reserve(
                ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.mount_dir_roots.size() - global_mount_dir_count);

            ::std::size_t mount_index{};
            for(auto& mount_root: ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.mount_dir_roots)
            {
                if(mount_index < global_mount_dir_count) { restored_global_mount_dir_roots.emplace_back(::std::move(mount_root)); }
                else
                {
                    restored_target_mount_dir_roots.emplace_back(::std::move(mount_root));
                }
                ++mount_index;
            }

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.mount_dir_roots = ::std::move(restored_global_mount_dir_roots);
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.disable_utf8_check = saved_global_disable_utf8_check;
            target.mount_dir_roots = ::std::move(restored_target_mount_dir_roots);

            return ret;
        }

#  if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
        template <typename Parameter, typename Callback>
        [[nodiscard]] inline ::uwvm2::utils::cmdline::parameter_return_type apply_socket_callback_to_override(
            override_state_t& target,
            ::uwvm2::utils::container::u8cstring_view parameter_name,
            Parameter const& parameter,
            Callback callback,
            ::uwvm2::utils::container::u8cstring_view arg1,
            ::uwvm2::utils::container::u8cstring_view arg2) noexcept
        {
            auto saved_global_preopen_sockets{::std::move(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets)};
            auto saved_target_preopen_sockets{::std::move(target.preopen_sockets)};

            auto const global_socket_count{saved_global_preopen_sockets.size()};

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets = ::std::move(saved_global_preopen_sockets);
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets.reserve(
                global_socket_count + saved_target_preopen_sockets.size());
            for(auto& preopen_socket: saved_target_preopen_sockets)
            {
                ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets.emplace_back(::std::move(preopen_socket));
            }

            ::uwvm2::utils::cmdline::parameter_parsing_results fake_results[]{
                {.str = parameter_name, .para = ::std::addressof(parameter), .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::parameter},
                {.str = arg1, .para = nullptr, .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg},
                {.str = arg2, .para = nullptr, .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg}};

            auto const ret{callback(fake_results, fake_results, fake_results + (sizeof(fake_results) / sizeof(fake_results[0])))};

            ::uwvm2::utils::container::vector<preopen_socket_t> restored_global_preopen_sockets{};
            restored_global_preopen_sockets.reserve(global_socket_count);

            ::uwvm2::utils::container::vector<preopen_socket_t> restored_target_preopen_sockets{};
            restored_target_preopen_sockets.reserve(
                ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets.size() - global_socket_count);

            ::std::size_t socket_index{};
            for(auto& preopen_socket: ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets)
            {
                if(socket_index < global_socket_count) { restored_global_preopen_sockets.emplace_back(::std::move(preopen_socket)); }
                else
                {
                    restored_target_preopen_sockets.emplace_back(::std::move(preopen_socket));
                }
                ++socket_index;
            }

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets = ::std::move(restored_global_preopen_sockets);
            target.preopen_sockets = ::std::move(restored_target_preopen_sockets);

            return ret;
        }

        template <typename Parameter, typename Callback>
        [[nodiscard]] inline ::uwvm2::utils::cmdline::parameter_return_type apply_socket_callback_to_override(
            override_state_t& target,
            ::uwvm2::utils::container::u8cstring_view parameter_name,
            Parameter const& parameter,
            Callback callback,
            ::uwvm2::utils::container::u8cstring_view arg1,
            ::uwvm2::utils::container::u8cstring_view arg2,
            ::uwvm2::utils::container::u8cstring_view arg3) noexcept
        {
            auto saved_global_preopen_sockets{::std::move(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets)};
            auto saved_target_preopen_sockets{::std::move(target.preopen_sockets)};

            auto const global_socket_count{saved_global_preopen_sockets.size()};

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets = ::std::move(saved_global_preopen_sockets);
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets.reserve(
                global_socket_count + saved_target_preopen_sockets.size());
            for(auto& preopen_socket: saved_target_preopen_sockets)
            {
                ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets.emplace_back(::std::move(preopen_socket));
            }

            ::uwvm2::utils::cmdline::parameter_parsing_results fake_results[]{
                {.str = parameter_name, .para = ::std::addressof(parameter), .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::parameter},
                {.str = arg1, .para = nullptr, .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg},
                {.str = arg2, .para = nullptr, .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg},
                {.str = arg3, .para = nullptr, .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg}};

            auto const ret{callback(fake_results, fake_results, fake_results + (sizeof(fake_results) / sizeof(fake_results[0])))};

            ::uwvm2::utils::container::vector<preopen_socket_t> restored_global_preopen_sockets{};
            restored_global_preopen_sockets.reserve(global_socket_count);

            ::uwvm2::utils::container::vector<preopen_socket_t> restored_target_preopen_sockets{};
            restored_target_preopen_sockets.reserve(
                ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets.size() - global_socket_count);

            ::std::size_t socket_index{};
            for(auto& preopen_socket: ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets)
            {
                if(socket_index < global_socket_count) { restored_global_preopen_sockets.emplace_back(::std::move(preopen_socket)); }
                else
                {
                    restored_target_preopen_sockets.emplace_back(::std::move(preopen_socket));
                }
                ++socket_index;
            }

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets = ::std::move(restored_global_preopen_sockets);
            target.preopen_sockets = ::std::move(restored_target_preopen_sockets);

            return ret;
        }
#  endif

        [[nodiscard]] inline ::uwvm2::utils::cmdline::parameter_return_type parse_fd_limit(
            ::uwvm2::utils::cmdline::parameter const& parameter,
            ::uwvm2::utils::container::u8string_view text,
            override_state_t& target) noexcept
        {
            ::std::size_t limit{};
            auto const [next, err]{::fast_io::parse_by_scan(text.cbegin(), text.cend(), limit)};
            if(err != ::fast_io::parse_code::ok || next != text.cend()) [[unlikely]]
            {
                return print_usage_error(parameter, u8"Invalid fd limit.");
            }

            using fd_t = ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
            if constexpr(::std::numeric_limits<::std::size_t>::max() > ::std::numeric_limits<fd_t>::max())
            {
                if(limit > ::std::numeric_limits<fd_t>::max()) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Illegal fd limit.");
                }
            }

            if(limit == 0uz) { limit = ::std::numeric_limits<fd_t>::max(); }
            target.fd_limit = limit;
            target.fd_limit_is_set = true;
            return ::uwvm2::utils::cmdline::parameter_return_type::def;
        }

        enum class enable_disable_mode_t : unsigned
        {
            target_specific = 0u,
            both
        };

        inline constexpr void apply_enable_disable_action(override_state_t& target,
                                                          enable_disable_mode_t mode,
                                                          target_kind target_kind_value,
                                                          bool enabled) noexcept
        {
            if(mode == enable_disable_mode_t::target_specific)
            {
                if(target_is_native_preload(target_kind_value))
                {
                    target.expose_host_api = enabled;
                    target.expose_host_api_is_set = true;
                }
                else
                {
                    target.enabled = enabled;
                    target.enabled_is_set = true;
                }
                return;
            }

            target.enabled = enabled;
            target.enabled_is_set = true;
            target.expose_host_api = enabled;
            target.expose_host_api_is_set = true;
        }

        [[nodiscard]] inline ::uwvm2::utils::cmdline::parameter_return_type apply_module_action(
            ::uwvm2::utils::cmdline::parameter const& parameter,
            ::uwvm2::utils::cmdline::parameter_parsing_results* mark_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results* action_arg,
            ::uwvm2::utils::cmdline::parameter_parsing_results* para_end,
            override_state_t& target,
            enable_disable_mode_t enable_disable_mode,
            target_kind target_kind_value) noexcept
        {
            using parameter_return_type = ::uwvm2::utils::cmdline::parameter_return_type;
            using parameter_type = ::uwvm2::utils::cmdline::parameter_parsing_results_type;

            auto const action{::uwvm2::utils::container::u8string_view{action_arg->str}};

            auto const mark_consumed{
                [mark_begin](::uwvm2::utils::cmdline::parameter_parsing_results* last) constexpr noexcept
                {
                    for(auto curr{mark_begin}; curr <= last; ++curr) { curr->type = parameter_type::occupied_arg; }
                }};

            if(action == u8"enable")
            {
                apply_enable_disable_action(target, enable_disable_mode, target_kind_value, true);
                mark_consumed(action_arg);
                return parameter_return_type::def;
            }

            if(action == u8"disable")
            {
                apply_enable_disable_action(target, enable_disable_mode, target_kind_value, false);
                mark_consumed(action_arg);
                return parameter_return_type::def;
            }

            if(action == u8"expose-host-api")
            {
                target.expose_host_api = true;
                target.expose_host_api_is_set = true;
                mark_consumed(action_arg);
                return parameter_return_type::def;
            }

            if(action == u8"hide-host-api")
            {
                target.expose_host_api = false;
                target.expose_host_api_is_set = true;
                mark_consumed(action_arg);
                return parameter_return_type::def;
            }

            if(action == u8"noinherit-system-environment")
            {
                target.noinherit_system_environment = true;
                target.noinherit_system_environment_is_set = true;
                mark_consumed(action_arg);
                return parameter_return_type::def;
            }

            if(action == u8"inherit-system-environment")
            {
                target.noinherit_system_environment = false;
                target.noinherit_system_environment_is_set = true;
                mark_consumed(action_arg);
                return parameter_return_type::def;
            }

            if(action == u8"disable-utf8-check")
            {
                target.disable_utf8_check = true;
                target.disable_utf8_check_is_set = true;
                mark_consumed(action_arg);
                return parameter_return_type::def;
            }

            if(action == u8"enable-utf8-check")
            {
                target.disable_utf8_check = false;
                target.disable_utf8_check_is_set = true;
                mark_consumed(action_arg);
                return parameter_return_type::def;
            }

            auto extra1{action_arg + 1u};
            if(action == u8"set-argv0")
            {
                if(extra1 == para_end || extra1->type != parameter_type::arg) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Missing argv0.");
                }
                target.argv0_storage = ::uwvm2::utils::container::u8string{extra1->str};
                target.argv0_is_set = true;
                mark_consumed(extra1);
                return parameter_return_type::def;
            }

            if(action == u8"set-fd-limit")
            {
                if(extra1 == para_end || extra1->type != parameter_type::arg) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Missing fd limit.");
                }
                auto const ret{parse_fd_limit(parameter, ::uwvm2::utils::container::u8string_view{extra1->str}, target)};
                if(ret != parameter_return_type::def) [[unlikely]] { return ret; }
                mark_consumed(extra1);
                return parameter_return_type::def;
            }

            if(action == u8"delete-system-environment")
            {
                if(extra1 == para_end || extra1->type != parameter_type::arg) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Missing environment variable name.");
                }

                auto const env_name{::uwvm2::utils::container::u8string_view{extra1->str}};
                if(env_name.empty() || env_name.find_character(u8'=') != ::fast_io::containers::npos) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Invalid environment variable name.");
                }

                target.delete_system_environment.emplace_back(env_name);
                mark_consumed(extra1);
                return parameter_return_type::def;
            }

            if(action == u8"add-environment")
            {
                auto extra2{extra1 + 1u};
                if(extra1 == para_end || extra1->type != parameter_type::arg || extra2 == para_end || extra2->type != parameter_type::arg) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Missing add-environment arguments.");
                }

                auto const env_name{::uwvm2::utils::container::u8string_view{extra1->str}};
                if(env_name.empty() || env_name.find_character(u8'=') != ::fast_io::containers::npos) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Invalid environment variable name.");
                }

                target.add_environment.emplace_back(::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_add_environment_t{
                    .env = env_name,
                    .value = ::uwvm2::utils::container::u8string_view{extra2->str}});
                mark_consumed(extra2);
                return parameter_return_type::def;
            }

            if(action == u8"mount-dir")
            {
                auto extra2{extra1 + 1u};
                if(extra1 == para_end || extra1->type != parameter_type::arg || extra2 == para_end || extra2->type != parameter_type::arg) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Missing mount-dir arguments.");
                }

                auto const ret{apply_mount_dir_to_override(target, extra1->str, extra2->str)};
                if(ret != parameter_return_type::def) [[unlikely]] { return ret; }
                mark_consumed(extra2);
                return parameter_return_type::def;
            }

#  if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
            if(action == u8"socket-tcp-connect" || action == u8"socket-tcp-listen" || action == u8"socket-udp-bind" || action == u8"socket-udp-connect")
            {
                auto extra2{extra1 + 1u};
                if(extra1 == para_end || extra1->type != parameter_type::arg || extra2 == para_end || extra2->type != parameter_type::arg) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Missing socket action arguments.");
                }

                auto apply_socket_action{
                    [&](auto const parameter_name, auto const& socket_parameter, auto callback) noexcept -> parameter_return_type
                    {
                        if(extra2->str == u8"unix")
                        {
                            auto extra3{extra2 + 1u};
                            if(extra3 == para_end || extra3->type != parameter_type::arg) [[unlikely]]
                            {
                                return print_usage_error(parameter, u8"Missing unix socket path.");
                            }

                            auto const ret{
                                apply_socket_callback_to_override(target, parameter_name, socket_parameter, callback, extra1->str, extra2->str, extra3->str)};
                            if(ret != parameter_return_type::def) [[unlikely]] { return ret; }
                            mark_consumed(extra3);
                            return parameter_return_type::def;
                        }

                        auto const ret{
                            apply_socket_callback_to_override(target, parameter_name, socket_parameter, callback, extra1->str, extra2->str)};
                        if(ret != parameter_return_type::def) [[unlikely]] { return ret; }
                        mark_consumed(extra2);
                        return parameter_return_type::def;
                    }};

                if(action == u8"socket-tcp-connect")
                {
                    return apply_socket_action(::uwvm2::utils::container::u8cstring_view{u8"--wasip1-socket-tcp-connect"},
                                               ::uwvm2::uwvm::cmdline::params::wasip1_socket_tcp_connect,
                                               ::uwvm2::uwvm::cmdline::params::details::wasip1_socket_tcp_connect_callback);
                }
                if(action == u8"socket-tcp-listen")
                {
                    return apply_socket_action(::uwvm2::utils::container::u8cstring_view{u8"--wasip1-socket-tcp-listen"},
                                               ::uwvm2::uwvm::cmdline::params::wasip1_socket_tcp_listen,
                                               ::uwvm2::uwvm::cmdline::params::details::wasip1_socket_tcp_listen_callback);
                }
                if(action == u8"socket-udp-bind")
                {
                    return apply_socket_action(::uwvm2::utils::container::u8cstring_view{u8"--wasip1-socket-udp-bind"},
                                               ::uwvm2::uwvm::cmdline::params::wasip1_socket_udp_bind,
                                               ::uwvm2::uwvm::cmdline::params::details::wasip1_socket_udp_bind_callback);
                }
                return apply_socket_action(::uwvm2::utils::container::u8cstring_view{u8"--wasip1-socket-udp-connect"},
                                           ::uwvm2::uwvm::cmdline::params::wasip1_socket_udp_connect,
                                           ::uwvm2::uwvm::cmdline::params::details::wasip1_socket_udp_connect_callback);
            }
#  endif

            return print_usage_error(parameter, u8"Unknown module action.");
        }
    }  // namespace wasip1_module_details

#  if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#  else
    UWVM_GNU_COLD inline constexpr
#  endif
        ::uwvm2::utils::cmdline::parameter_return_type wasip1_module_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results*
                                                                                   para_begin,
                                                                               ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
                                                                               ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept
    {
        using parameter_type = ::uwvm2::utils::cmdline::parameter_parsing_results_type;
        using target_kind = wasip1_module_details::target_kind;

        auto target_arg{para_curr + 1u};
        if(target_arg == para_end || target_arg->type != parameter_type::arg) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module, u8"Missing module target.");
        }

        target_kind target_kind_value{};
        auto const target_text{::uwvm2::utils::container::u8string_view{target_arg->str}};
        if(!wasip1_module_details::parse_target_kind(target_text, target_kind_value)) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module, u8"Invalid module target.");
        }

        ::uwvm2::utils::container::u8string_view module_name{};
        auto action_arg{target_arg + 1u};
        if(wasip1_module_details::target_needs_module_name(target_kind_value))
        {
            if(action_arg == para_end || action_arg->type != parameter_type::arg) [[unlikely]]
            {
                return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module, u8"Missing module name.");
            }

            module_name = ::uwvm2::utils::container::u8string_view{action_arg->str};
            if(module_name.empty()) [[unlikely]]
            {
                return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module, u8"Module name cannot be empty.");
            }
            if(!wasip1_module_details::validate_wasm_utf8_name(module_name)) [[unlikely]]
            {
                return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module,
                                                                u8"Invalid module name. WebAssembly names must be valid UTF-8.");
            }
            ++action_arg;
        }

        if(action_arg == para_end || action_arg->type != parameter_type::arg) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module, u8"Missing module action.");
        }

        auto& target{
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::upsert_wasip1_module_override(target_kind_value, module_name)};
        return wasip1_module_details::apply_module_action(::uwvm2::uwvm::cmdline::params::wasip1_module,
                                                          target_arg,
                                                          action_arg,
                                                          para_end,
                                                          target,
                                                          wasip1_module_details::enable_disable_mode_t::target_specific,
                                                          target_kind_value);
    }

#  if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#  else
    UWVM_GNU_COLD inline constexpr
#  endif
        ::uwvm2::utils::cmdline::parameter_return_type wasip1_module_group_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
            ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept
    {
        using parameter_type = ::uwvm2::utils::cmdline::parameter_parsing_results_type;

        auto module_arg{para_curr + 1u};
        if(module_arg == para_end || module_arg->type != parameter_type::arg) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module_group, u8"Missing module name.");
        }

        auto const module_name{::uwvm2::utils::container::u8string_view{module_arg->str}};
        if(module_name.empty()) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module_group, u8"Module name cannot be empty.");
        }
        if(!wasip1_module_details::validate_wasm_utf8_name(module_name)) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module_group,
                                                            u8"Invalid module name. WebAssembly names must be valid UTF-8.");
        }

        auto group_arg{module_arg + 1u};
        if(group_arg == para_end || group_arg->type != parameter_type::arg) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module_group, u8"Missing group name.");
        }

        auto const group_name{::uwvm2::utils::container::u8string_view{group_arg->str}};
        if(group_name.empty()) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module_group, u8"Group name cannot be empty.");
        }
        if(!wasip1_module_details::validate_wasm_utf8_name(group_name)) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module_group,
                                                            u8"Invalid group name. WebAssembly names must be valid UTF-8.");
        }

        auto action_arg{group_arg + 1u};
        if(action_arg == para_end || action_arg->type != parameter_type::arg) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module_group, u8"Missing module action.");
        }

        auto const group_index{::uwvm2::uwvm::imported::wasi::wasip1::storage::upsert_named_wasip1_group_index(group_name)};
        if(!::uwvm2::uwvm::imported::wasi::wasip1::storage::try_bind_wasip1_module_to_named_group(module_name, group_index)) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module_group,
                                                            u8"One module can only be bound to one WASI Preview 1 group.");
        }

        auto* target{::uwvm2::uwvm::imported::wasi::wasip1::storage::find_wasip1_group_state(group_index)};
        if(target == nullptr) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module_group,
                                                            u8"Failed to resolve WASI Preview 1 group.");
        }

        return wasip1_module_details::apply_module_action(::uwvm2::uwvm::cmdline::params::wasip1_module_group,
                                                          module_arg,
                                                          action_arg,
                                                          para_end,
                                                          *target,
                                                          wasip1_module_details::enable_disable_mode_t::both,
                                                          wasip1_module_details::target_kind::main_wasm);
    }

# endif
#endif
}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
// macro
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

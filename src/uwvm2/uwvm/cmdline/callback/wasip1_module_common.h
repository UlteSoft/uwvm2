/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-04-27
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
# include <memory>
# include <utility>
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
# include "wasip1_global_mount_dir.h"
# include "wasip1_global_socket_tcp_connect.h"
# include "wasip1_global_socket_tcp_listen.h"
# include "wasip1_global_socket_udp_bind.h"
# include "wasip1_global_socket_udp_connect.h"
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
        using override_state_t = ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_override_t;
        using mount_dir_root_t = ::uwvm2::imported::wasi::wasip1::environment::mount_dir_root_t;
        using trace_output_target_t = ::uwvm2::imported::wasi::wasip1::environment::trace_wasip1_output_target_t;
#  if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
        using preopen_socket_t = ::uwvm2::imported::wasi::wasip1::environment::preopen_socket_t;
#  endif

        // All target-local WASI Preview 1 commands are reduced to this enum at
        // the public callback boundary. Keeping the dispatcher strongly typed is
        // intentional: the single and group callback files only validate their
        // leading selector (module name or group name), and every semantic rule
        // for a WASI target lives in the switch below. That avoids maintaining a
        // second string parser for each concrete command name.
        enum class target_action_t : unsigned
        {
            enable,
            disable,
            expose_host_api,
            hide_host_api,
            noinherit_system_environment,
            inherit_system_environment,
            disable_utf8_check,
            enable_utf8_check,
            trace,
            set_argv0,
            force_args,
            set_fd_limit,
            delete_system_environment,
            add_or_replace_environment,
            mount_dir,
#  if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
            socket_tcp_connect,
            socket_tcp_listen,
            socket_udp_bind,
            socket_udp_connect,
#  endif
        };

        // Parse the action word used by the legacy "module action sequence"
        // callback. The modern single/group parameter files pass
        // target_action_t directly, but this helper is kept for the compact
        // multi-action form so both frontends share exactly the same validator.
        [[nodiscard]] inline constexpr bool parse_target_action(::uwvm2::utils::container::u8string_view text, target_action_t& action) noexcept
        {
            if(text == u8"enable") { action = target_action_t::enable; }
            else if(text == u8"disable") { action = target_action_t::disable; }
            else if(text == u8"expose-host-api") { action = target_action_t::expose_host_api; }
            else if(text == u8"hide-host-api") { action = target_action_t::hide_host_api; }
            else if(text == u8"noinherit-system-environment") { action = target_action_t::noinherit_system_environment; }
            else if(text == u8"inherit-system-environment") { action = target_action_t::inherit_system_environment; }
            else if(text == u8"disable-utf8-check") { action = target_action_t::disable_utf8_check; }
            else if(text == u8"enable-utf8-check") { action = target_action_t::enable_utf8_check; }
            else if(text == u8"trace") { action = target_action_t::trace; }
            else if(text == u8"set-argv0") { action = target_action_t::set_argv0; }
            else if(text == u8"force-args") { action = target_action_t::force_args; }
            else if(text == u8"set-fd-limit") { action = target_action_t::set_fd_limit; }
            else if(text == u8"delete-system-environment") { action = target_action_t::delete_system_environment; }
            else if(text == u8"add-or-replace-environment") { action = target_action_t::add_or_replace_environment; }
            else if(text == u8"mount-dir") { action = target_action_t::mount_dir; }
#  if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
            else if(text == u8"socket-tcp-connect") { action = target_action_t::socket_tcp_connect; }
            else if(text == u8"socket-tcp-listen") { action = target_action_t::socket_tcp_listen; }
            else if(text == u8"socket-udp-bind") { action = target_action_t::socket_udp_bind; }
            else if(text == u8"socket-udp-connect") { action = target_action_t::socket_udp_connect; }
#  endif
            else { return false; }
            return true;
        }

        // WebAssembly module names are user-visible identifiers, but the loader
        // stores them in the same UTF-8 text representation used by wasm names.
        // Validate at command-line time so later group lookups can compare raw
        // byte strings without revalidating or diagnosing late.
        [[nodiscard]] inline constexpr bool validate_wasm_utf8_name(::uwvm2::utils::container::u8string_view text) noexcept
        {
            auto const [module_name_pos,
                        module_name_err]{::uwvm2::uwvm::wasm::feature::handle_text_format(::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_text_format_wapper,
                                                                                          text.cbegin(),
                                                                                          text.cend())};
            static_cast<void>(module_name_pos);
            return module_name_err == ::uwvm2::utils::utf::utf_error_code::success;
        }

        inline constexpr ::uwvm2::utils::cmdline::parameter_return_type print_usage_error(::uwvm2::utils::cmdline::parameter const& parameter,
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

        [[nodiscard]] inline constexpr bool
            is_argument_result(::uwvm2::utils::cmdline::parameter_parsing_results_type type) noexcept
        {
            return type == ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg ||
                   type == ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        }

        [[nodiscard]] inline constexpr bool parse_size_t(::uwvm2::utils::container::u8string_view text, ::std::size_t& value) noexcept
        {
            auto const [next, err]{::fast_io::parse_by_scan(text.cbegin(), text.cend(), value)};
            return err == ::fast_io::parse_code::ok && next == text.cend();
        }

        // force-args consumes a dynamic number of following tokens. The generic
        // command-line scanner normally treats an option-looking token such as
        // "--x" as a parameter, so force-args has a pretreatment hook that marks
        // the count and exactly <count> following tokens as occupied arguments.
        // This is also what makes `--wasip1-*-force-args 0` unambiguous: the
        // count is consumed even though no argv payload follows.
        inline constexpr void force_args_pretreatment(char8_t const* const*& argv_curr,
                                            char8_t const* const* argv_end,
                                            ::uwvm2::utils::container::vector<::uwvm2::utils::cmdline::parameter_parsing_results>& pr,
                                            ::std::size_t leading_args_before_count) noexcept
        {
            auto curr{argv_curr + 1u};

            for(::std::size_t i{}; i != leading_args_before_count; ++i)
            {
                if(curr == argv_end || *curr == nullptr) [[unlikely]]
                {
                    argv_curr = curr;
                    return;
                }

                auto const arg_str{::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(*curr)}};
                pr.emplace_back_unchecked(arg_str, nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg);
                ++curr;
            }

            if(curr == argv_end || *curr == nullptr) [[unlikely]]
            {
                argv_curr = curr;
                return;
            }

            auto const count_str{::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(*curr)}};
            pr.emplace_back_unchecked(count_str, nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg);
            ++curr;

            ::std::size_t arg_count{};
            if(!parse_size_t(count_str, arg_count))
            {
                argv_curr = curr;
                return;
            }

            for(::std::size_t i{}; i != arg_count; ++i)
            {
                if(curr == argv_end || *curr == nullptr) [[unlikely]]
                {
                    argv_curr = curr;
                    return;
                }

                auto const arg_str{::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(*curr)}};
                pr.emplace_back_unchecked(arg_str, nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg);
                ++curr;
            }

            argv_curr = curr;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::cmdline::parameter_return_type
            apply_mount_dir_to_override(override_state_t& target,
                                        ::uwvm2::utils::container::u8cstring_view wasi_dir,
                                        ::uwvm2::utils::container::u8cstring_view system_dir) noexcept
        {
            // Target-specific mounts must obey the same rules as global mounts:
            // guest path normalization, duplicate rejection, ancestor/child
            // overlap rejection, UTF-8 policy, host directory opening, and the
            // `--wasip1-disable-mount-path-normalization` /
            // `--wasip1-allow-overlapping-mount-paths` switches. Rather than
            // duplicating that security-sensitive logic here, temporarily build
            // a synthetic default environment containing the global mounts plus
            // the target's current mounts, then invoke the global mount callback.
            auto const saved_global_disable_utf8_check{::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.disable_utf8_check};
            auto saved_global_mount_dir_roots{::std::move(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.mount_dir_roots)};
            auto saved_target_mount_dir_roots{::std::move(target.mount_dir_roots)};

            auto const global_mount_dir_count{saved_global_mount_dir_roots.size()};

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.mount_dir_roots = ::std::move(saved_global_mount_dir_roots);
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.mount_dir_roots.reserve(global_mount_dir_count +
                                                                                                       saved_target_mount_dir_roots.size());
            for(auto& mount_root: saved_target_mount_dir_roots)
            {
                ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.mount_dir_roots.emplace_back(::std::move(mount_root));
            }

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.disable_utf8_check =
                target.disable_utf8_check_is_set ? target.disable_utf8_check : saved_global_disable_utf8_check;

            ::uwvm2::utils::cmdline::parameter_parsing_results fake_results[]{
                {.str = u8"--wasip1-global-mount-dir",
                 .para = ::std::addressof(::uwvm2::uwvm::cmdline::params::wasip1_global_mount_dir),
                 .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::parameter                                                                             },
                {.str = wasi_dir,                      .para = nullptr,                             .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg},
                {.str = system_dir,                    .para = nullptr,                             .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg}
            };

            auto const ret{
                ::uwvm2::uwvm::cmdline::params::details::wasip1_global_mount_dir_callback(fake_results,
                                                                                          fake_results,
                                                                                          fake_results + (sizeof(fake_results) / sizeof(fake_results[0])))};

            // Split the synthetic environment back into the real global portion
            // and the target-specific portion. A successful callback appends the
            // new mount after the preserved global entries, so the original
            // global count is the stable boundary.
            ::uwvm2::utils::container::vector<mount_dir_root_t> restored_global_mount_dir_roots{};
            restored_global_mount_dir_roots.reserve(global_mount_dir_count);

            ::uwvm2::utils::container::vector<mount_dir_root_t> restored_target_mount_dir_roots{};
            restored_target_mount_dir_roots.reserve(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.mount_dir_roots.size() -
                                                    global_mount_dir_count);

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
        [[nodiscard]] inline constexpr ::uwvm2::utils::cmdline::parameter_return_type
            apply_socket_callback_to_override(override_state_t& target,
                                              ::uwvm2::utils::container::u8cstring_view parameter_name,
                                              Parameter const& parameter,
                                              Callback callback,
                                              ::uwvm2::utils::container::u8cstring_view arg1,
                                              ::uwvm2::utils::container::u8cstring_view arg2) noexcept
        {
            // Socket parsing already exists in the global callback layer, and it
            // includes platform-specific address validation plus fd range checks.
            // For a target-local socket command, temporarily append the target's
            // sockets after the global sockets, invoke the global callback, then
            // split the vector at the original global boundary. The target still
            // owns only its own sockets; the temporary global prefix only makes
            // duplicate fd checks see the same effective preopen set that runtime
            // initialization will later materialize.
            auto saved_global_preopen_sockets{::std::move(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets)};
            auto saved_target_preopen_sockets{::std::move(target.preopen_sockets)};

            auto const global_socket_count{saved_global_preopen_sockets.size()};

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets = ::std::move(saved_global_preopen_sockets);
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets.reserve(global_socket_count +
                                                                                                       saved_target_preopen_sockets.size());
            for(auto& preopen_socket: saved_target_preopen_sockets)
            {
                ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets.emplace_back(::std::move(preopen_socket));
            }

            ::uwvm2::utils::cmdline::parameter_parsing_results fake_results[]{
                {.str = parameter_name, .para = ::std::addressof(parameter), .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::parameter},
                {.str = arg1,           .para = nullptr,                     .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg      },
                {.str = arg2,           .para = nullptr,                     .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg      }
            };

            auto const ret{callback(fake_results, fake_results, fake_results + (sizeof(fake_results) / sizeof(fake_results[0])))};

            ::uwvm2::utils::container::vector<preopen_socket_t> restored_global_preopen_sockets{};
            restored_global_preopen_sockets.reserve(global_socket_count);

            ::uwvm2::utils::container::vector<preopen_socket_t> restored_target_preopen_sockets{};
            restored_target_preopen_sockets.reserve(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets.size() -
                                                    global_socket_count);

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
        [[nodiscard]] inline constexpr ::uwvm2::utils::cmdline::parameter_return_type
            apply_socket_callback_to_override(override_state_t& target,
                                              ::uwvm2::utils::container::u8cstring_view parameter_name,
                                              Parameter const& parameter,
                                              Callback callback,
                                              ::uwvm2::utils::container::u8cstring_view arg1,
                                              ::uwvm2::utils::container::u8cstring_view arg2,
                                              ::uwvm2::utils::container::u8cstring_view arg3) noexcept
        {
            // Three-argument socket commands are currently the Unix-domain form:
            // <fd> unix <path>. Keep this overload structurally identical to the
            // two-argument path so target-local TCP/UDP and Unix socket handling
            // cannot diverge from the global parser.
            auto saved_global_preopen_sockets{::std::move(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets)};
            auto saved_target_preopen_sockets{::std::move(target.preopen_sockets)};

            auto const global_socket_count{saved_global_preopen_sockets.size()};

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets = ::std::move(saved_global_preopen_sockets);
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets.reserve(global_socket_count +
                                                                                                       saved_target_preopen_sockets.size());
            for(auto& preopen_socket: saved_target_preopen_sockets)
            {
                ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets.emplace_back(::std::move(preopen_socket));
            }

            ::uwvm2::utils::cmdline::parameter_parsing_results fake_results[]{
                {.str = parameter_name, .para = ::std::addressof(parameter), .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::parameter},
                {.str = arg1,           .para = nullptr,                     .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg      },
                {.str = arg2,           .para = nullptr,                     .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg      },
                {.str = arg3,           .para = nullptr,                     .type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg      }
            };

            auto const ret{callback(fake_results, fake_results, fake_results + (sizeof(fake_results) / sizeof(fake_results[0])))};

            ::uwvm2::utils::container::vector<preopen_socket_t> restored_global_preopen_sockets{};
            restored_global_preopen_sockets.reserve(global_socket_count);

            ::uwvm2::utils::container::vector<preopen_socket_t> restored_target_preopen_sockets{};
            restored_target_preopen_sockets.reserve(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.preopen_sockets.size() -
                                                    global_socket_count);

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

        [[nodiscard]] inline constexpr ::uwvm2::utils::cmdline::parameter_return_type parse_fd_limit(::uwvm2::utils::cmdline::parameter const& parameter,
                                                                                           ::uwvm2::utils::container::u8string_view text,
                                                                                           override_state_t& target) noexcept
        {
            ::std::size_t limit{};
            auto const [next, err]{::fast_io::parse_by_scan(text.cbegin(), text.cend(), limit)};
            if(err != ::fast_io::parse_code::ok || next != text.cend()) [[unlikely]] { return print_usage_error(parameter, u8"Invalid fd limit."); }

            using fd_t = ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
            if constexpr(::std::numeric_limits<::std::size_t>::max() > ::std::numeric_limits<fd_t>::max())
            {
                if(limit > ::std::numeric_limits<fd_t>::max()) [[unlikely]] { return print_usage_error(parameter, u8"Illegal fd limit."); }
            }

            // Match the global fd-limit command: zero is a user-facing spelling
            // for "maximum representable WASI fd", not a request to disable fd
            // allocation. Runtime initialization uses a literal zero only as the
            // internal "no explicit limit has been committed yet" sentinel.
            if(limit == 0uz) { limit = ::std::numeric_limits<fd_t>::max(); }
            if(target.fd_limit_is_set) [[unlikely]]
            {
                return print_usage_error(parameter,
                                         u8"Duplicate or conflicting module action. Cannot set fd limit more than once for the same WASI Preview 1 target.");
            }
            target.fd_limit = limit;
            target.fd_limit_is_set = true;
            return ::uwvm2::utils::cmdline::parameter_return_type::def;
        }

        [[nodiscard]] inline constexpr bool trace_configuration_matches(override_state_t const& target,
                                                              trace_output_target_t trace_target,
                                                              ::uwvm2::utils::container::u8string_view file_path) noexcept
        {
            // The public parameter is "once", but this predicate lets the
            // sequence parser diagnose identical repeats separately from repeats
            // that would silently redirect traces to another sink.
            if(!target.trace_wasip1_call_is_set) [[unlikely]] { return true; }
            if(target.trace_wasip1_call != (trace_target != trace_output_target_t::none)) [[unlikely]] { return false; }
            if(target.trace_wasip1_output_target != trace_target) [[unlikely]] { return false; }
            if(trace_target == trace_output_target_t::file) [[unlikely]] { return target.trace_wasip1_output_file_path_storage == file_path; }
            return true;
        }

        [[nodiscard]] inline constexpr bool has_deleted_environment(override_state_t const& target, ::uwvm2::utils::container::u8string_view env_name) noexcept
        {
            return target.delete_system_environment.find(env_name) != target.delete_system_environment.cend();
        }

#  if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
        [[nodiscard]] inline constexpr bool has_duplicate_preopen_socket_fd(override_state_t const& target) noexcept
        {
            for(auto curr{target.preopen_sockets.cbegin()}; curr != target.preopen_sockets.cend(); ++curr)
            {
                for(auto next{curr + 1}; next != target.preopen_sockets.cend(); ++next)
                {
                    if(curr->fd == next->fd) [[unlikely]] { return true; }
                }
            }
            return false;
        }
#  endif

        [[nodiscard]] inline constexpr ::uwvm2::utils::cmdline::parameter_return_type
            apply_module_action(::uwvm2::utils::cmdline::parameter const& parameter,
                                ::uwvm2::utils::cmdline::parameter_parsing_results* mark_begin,
                                ::uwvm2::utils::cmdline::parameter_parsing_results* mark_action_end,
                                target_action_t action,
                                ::uwvm2::utils::cmdline::parameter_parsing_results* extra1,
                                ::uwvm2::utils::cmdline::parameter_parsing_results* para_end,
                                override_state_t& target) noexcept
        {
            using parameter_return_type = ::uwvm2::utils::cmdline::parameter_return_type;
            using parameter_type = ::uwvm2::utils::cmdline::parameter_parsing_results_type;

            // Mark every token consumed by this action as occupied so a later
            // pass does not reinterpret an argv payload, file path, socket
            // address, or environment value as another parameter.
            auto const mark_consumed{[mark_begin](::uwvm2::utils::cmdline::parameter_parsing_results* last) constexpr noexcept
                                     {
                                         for(auto curr{mark_begin}; curr <= last; ++curr) { curr->type = parameter_type::occupied_arg; }
                                     }};

            // Several target options are mutually exclusive boolean overrides:
            // enable/disable, expose/hide, inherit/noinherit, strict/relaxed
            // UTF-8. Store both the value and an "is set" bit so "false" is not
            // confused with "not specified; inherit the global default".
            auto const set_bool_option{[&](bool& option, bool& option_is_set, bool value, auto const& conflict_message) noexcept -> parameter_return_type
                                       {
                                           if(option_is_set) [[unlikely]] { return print_usage_error(parameter, conflict_message); }
                                           option = value;
                                           option_is_set = true;
                                           return parameter_return_type::def;
                                       }};

            auto const apply_module_enable_disable_action{
                [&](bool enabled) noexcept -> parameter_return_type
                {
                    if(auto const ret{set_bool_option(
                           target.enabled,
                           target.enabled_is_set,
                           enabled,
                           u8"Duplicate or conflicting module action. Cannot combine or repeat enable/disable for the same WASI Preview 1 target.")};
                       ret != parameter_return_type::def) [[unlikely]]
                    {
                        return ret;
                    }
                    mark_consumed(mark_action_end);
                    return parameter_return_type::def;
                }};

            switch(action)
            {
            case target_action_t::enable:
                return apply_module_enable_disable_action(true);
            case target_action_t::disable:
                return apply_module_enable_disable_action(false);
            case target_action_t::expose_host_api:
            {
                if(auto const ret{set_bool_option(
                       target.expose_host_api,
                       target.expose_host_api_is_set,
                       true,
                       u8"Duplicate or conflicting module action. Cannot combine or repeat expose-host-api/hide-host-api for the same WASI Preview 1 target.")};
                   ret != parameter_return_type::def) [[unlikely]]
                {
                    return ret;
                }
                mark_consumed(mark_action_end);
                return parameter_return_type::def;
            }
            case target_action_t::hide_host_api:
            {
                if(auto const ret{set_bool_option(
                       target.expose_host_api,
                       target.expose_host_api_is_set,
                       false,
                       u8"Duplicate or conflicting module action. Cannot combine or repeat expose-host-api/hide-host-api for the same WASI Preview 1 target.")};
                   ret != parameter_return_type::def) [[unlikely]]
                {
                    return ret;
                }
                mark_consumed(mark_action_end);
                return parameter_return_type::def;
            }
            case target_action_t::noinherit_system_environment:
            {
                if(auto const ret{set_bool_option(
                       target.noinherit_system_environment,
                       target.noinherit_system_environment_is_set,
                       true,
                       u8"Duplicate or conflicting module action. Cannot combine or repeat noinherit-system-environment/inherit-system-environment for the same WASI Preview 1 target.")};
                   ret != parameter_return_type::def) [[unlikely]]
                {
                    return ret;
                }
                mark_consumed(mark_action_end);
                return parameter_return_type::def;
            }
            case target_action_t::inherit_system_environment:
            {
                if(auto const ret{set_bool_option(
                       target.noinherit_system_environment,
                       target.noinherit_system_environment_is_set,
                       false,
                       u8"Duplicate or conflicting module action. Cannot combine or repeat noinherit-system-environment/inherit-system-environment for the same WASI Preview 1 target.")};
                   ret != parameter_return_type::def) [[unlikely]]
                {
                    return ret;
                }
                mark_consumed(mark_action_end);
                return parameter_return_type::def;
            }
            case target_action_t::disable_utf8_check:
            {
                if(auto const ret{set_bool_option(
                       target.disable_utf8_check,
                       target.disable_utf8_check_is_set,
                       true,
                       u8"Duplicate or conflicting module action. Cannot combine or repeat disable-utf8-check/enable-utf8-check for the same WASI Preview 1 target.")};
                   ret != parameter_return_type::def) [[unlikely]]
                {
                    return ret;
                }
                mark_consumed(mark_action_end);
                return parameter_return_type::def;
            }
            case target_action_t::enable_utf8_check:
            {
                if(auto const ret{set_bool_option(
                       target.disable_utf8_check,
                       target.disable_utf8_check_is_set,
                       false,
                       u8"Duplicate or conflicting module action. Cannot combine or repeat disable-utf8-check/enable-utf8-check for the same WASI Preview 1 target.")};
                   ret != parameter_return_type::def) [[unlikely]]
                {
                    return ret;
                }
                mark_consumed(mark_action_end);
                return parameter_return_type::def;
            }
            case target_action_t::trace:
            {
                if(extra1 == para_end || extra1->type != parameter_type::arg) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Missing trace output target.");
                }

                auto const trace_target_text{::uwvm2::utils::container::u8string_view{extra1->str}};
                auto set_trace_target{
                    [&](trace_output_target_t trace_target, ::uwvm2::utils::cmdline::parameter_parsing_results* last) noexcept -> parameter_return_type
                    {
                        if(!trace_configuration_matches(target, trace_target, {})) [[unlikely]]
                        {
                            return print_usage_error(parameter,
                                                     u8"Conflicting module action. Cannot set different trace outputs for the same WASI Preview 1 target.");
                        }
                        if(target.trace_wasip1_call_is_set) [[unlikely]]
                        {
                            return print_usage_error(
                                parameter,
                                u8"Duplicate or conflicting module action. Cannot set trace output more than once for the same WASI Preview 1 target.");
                        }
                        target.trace_wasip1_call = trace_target != trace_output_target_t::none;
                        target.trace_wasip1_call_is_set = true;
                        target.trace_wasip1_output_target = trace_target;
                        if(trace_target != trace_output_target_t::file) { target.trace_wasip1_output_file_path_storage.clear(); }
                        mark_consumed(last);
                        return parameter_return_type::def;
                    }};

                if(trace_target_text == u8"none") { return set_trace_target(trace_output_target_t::none, extra1); }
                if(trace_target_text == u8"out") { return set_trace_target(trace_output_target_t::out, extra1); }
                if(trace_target_text == u8"err") { return set_trace_target(trace_output_target_t::err, extra1); }

#  if !defined(__AVR__) && !((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) && !(defined(__MSDOS__) || defined(__DJGPP__)) &&             \
      !(defined(__NEWLIB__) && !defined(__CYGWIN__)) && !defined(_PICOLIBC__) && !defined(__wasm__)
                auto set_trace_file{
                    [&](::uwvm2::utils::container::u8string_view file_path,
                        ::uwvm2::utils::cmdline::parameter_parsing_results* last) noexcept -> parameter_return_type
                    {
                        if(file_path.empty()) [[unlikely]] { return print_usage_error(parameter, u8"Missing trace output file path."); }
                        if(!trace_configuration_matches(target, trace_output_target_t::file, file_path)) [[unlikely]]
                        {
                            return print_usage_error(parameter,
                                                     u8"Conflicting module action. Cannot set different trace outputs for the same WASI Preview 1 target.");
                        }
                        if(target.trace_wasip1_call_is_set) [[unlikely]]
                        {
                            return print_usage_error(
                                parameter,
                                u8"Duplicate or conflicting module action. Cannot set trace output more than once for the same WASI Preview 1 target.");
                        }

                        ::fast_io::u8native_file test_output{};
                        if(!::uwvm2::uwvm::imported::wasi::wasip1::storage::reopen_wasip1_trace_output_file(test_output, file_path)) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                                u8"[error] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"Unable to open WASI trace output file \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                                file_path,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            return parameter_return_type::return_m1_imme;
                        }

                        target.trace_wasip1_call = true;
                        target.trace_wasip1_call_is_set = true;
                        target.trace_wasip1_output_target = trace_output_target_t::file;
                        target.trace_wasip1_output_file_path_storage = ::uwvm2::utils::container::u8string{file_path};
                        mark_consumed(last);
                        return parameter_return_type::def;
                    }};

                if(trace_target_text == u8"file")
                {
                    auto extra2{extra1 + 1u};
                    if(extra2 == para_end || extra2->type != parameter_type::arg) [[unlikely]]
                    {
                        return print_usage_error(parameter, u8"Missing trace output file path.");
                    }
                    return set_trace_file(::uwvm2::utils::container::u8string_view{extra2->str}, extra2);
                }
                return set_trace_file(trace_target_text, extra1);
#  else
                return print_usage_error(parameter, u8"Invalid trace output target.");
#  endif
            }
            case target_action_t::set_argv0:
            {
                if(extra1 == para_end || extra1->type != parameter_type::arg) [[unlikely]] { return print_usage_error(parameter, u8"Missing argv0."); }
                if(target.argv0_is_set) [[unlikely]]
                {
                    return print_usage_error(parameter,
                                             u8"Duplicate or conflicting module action. Cannot set argv0 more than once for the same WASI Preview 1 target.");
                }
                if(target.force_args_is_set) [[unlikely]]
                {
                    return print_usage_error(parameter,
                                             u8"Conflicting module action. Cannot combine force-args and set-argv0 for the same WASI Preview 1 target.");
                }
                target.argv0_storage = ::uwvm2::utils::container::u8string{extra1->str};
                target.argv0_is_set = true;
                mark_consumed(extra1);
                return parameter_return_type::def;
            }
            case target_action_t::force_args:
            {
                // Target force-args is a complete argv replacement for this
                // target. It deliberately does not inherit the `--run` argv tail
                // and cannot be combined with set-argv0, because set-argv0 is
                // defined as a patch over the default argv vector.
                if(extra1 == para_end || !is_argument_result(extra1->type)) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Missing force-args count.");
                }

                ::std::size_t arg_count{};
                if(!parse_size_t(::uwvm2::utils::container::u8string_view{extra1->str}, arg_count)) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Invalid force-args count (size_t).");
                }

                if(target.argv0_is_set) [[unlikely]]
                {
                    return print_usage_error(parameter,
                                             u8"Conflicting module action. Cannot combine force-args and set-argv0 for the same WASI Preview 1 target.");
                }
                if(target.force_args_is_set) [[unlikely]]
                {
                    return print_usage_error(parameter,
                                             u8"Duplicate module action. Cannot set force-args more than once for the same WASI Preview 1 target.");
                }

                ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> force_args{};
                force_args.reserve(arg_count);

                auto last{extra1};
                auto arg_curr{extra1 + 1u};
                for(::std::size_t i{}; i != arg_count; ++i, ++arg_curr)
                {
                    if(arg_curr == para_end || !is_argument_result(arg_curr->type)) [[unlikely]]
                    {
                        return print_usage_error(parameter, u8"Missing force-args value.");
                    }
                    force_args.emplace_back(::uwvm2::utils::container::u8string{arg_curr->str});
                    last = arg_curr;
                }

                target.force_argument_storage = ::std::move(force_args);
                target.force_args_is_set = true;
                mark_consumed(last);
                return parameter_return_type::def;
            }
            case target_action_t::set_fd_limit:
            {
                if(extra1 == para_end || extra1->type != parameter_type::arg) [[unlikely]] { return print_usage_error(parameter, u8"Missing fd limit."); }
                auto const ret{parse_fd_limit(parameter, ::uwvm2::utils::container::u8string_view{extra1->str}, target)};
                if(ret != parameter_return_type::def) [[unlikely]] { return ret; }
                mark_consumed(extra1);
                return parameter_return_type::def;
            }
            case target_action_t::delete_system_environment:
            {
                // Deleting the same name twice has no runtime side effect, but
                // it is rejected because it usually means a positional single or
                // group command was accidentally attached to the wrong target.
                if(extra1 == para_end || extra1->type != parameter_type::arg) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Missing environment variable name.");
                }

                auto const env_name{::uwvm2::utils::container::u8string_view{extra1->str}};
                if(env_name.empty() || env_name.find_character(u8'=') != ::fast_io::containers::npos) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Invalid environment variable name.");
                }
                if(has_deleted_environment(target, env_name)) [[unlikely]]
                {
                    return print_usage_error(
                        parameter,
                        u8"Duplicate module action. Cannot delete the same environment variable more than once in one WASI Preview 1 target.");
                }
                if(target.add_or_replace_environment.find(env_name) != target.add_or_replace_environment.cend()) [[unlikely]]
                {
                    return print_usage_error(
                        parameter,
                        u8"Conflicting module action. Cannot combine delete-system-environment and add-or-replace-environment for the same variable in one WASI Preview 1 target.");
                }

                target.delete_system_environment.emplace(env_name);
                mark_consumed(extra1);
                return parameter_return_type::def;
            }
            case target_action_t::add_or_replace_environment:
            {
                // Add-or-replace remains repeatable by design: the last value for
                // a key wins before materialization. A same-target delete for the
                // same name is still a conflict because command order should not
                // change whether a target imports or synthesizes that variable.
                auto extra2{extra1 + 1u};
                if(extra1 == para_end || extra1->type != parameter_type::arg || extra2 == para_end || extra2->type != parameter_type::arg) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Missing add-or-replace-environment arguments.");
                }

                auto const env_name{::uwvm2::utils::container::u8string_view{extra1->str}};
                if(env_name.empty() || env_name.find_character(u8'=') != ::fast_io::containers::npos) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Invalid environment variable name.");
                }
                if(has_deleted_environment(target, env_name)) [[unlikely]]
                {
                    return print_usage_error(
                        parameter,
                        u8"Conflicting module action. Cannot combine add-or-replace-environment and delete-system-environment for the same variable in one WASI Preview 1 target.");
                }

                auto const env_value{::uwvm2::utils::container::u8string_view{extra2->str}};
                if(auto env_iter{target.add_or_replace_environment.find(env_name)}; env_iter != target.add_or_replace_environment.end())
                {
                    env_iter->second = env_value;
                }
                else
                {
                    target.add_or_replace_environment.emplace(env_name, env_value);
                }
                mark_consumed(extra2);
                return parameter_return_type::def;
            }
            case target_action_t::mount_dir:
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
            case target_action_t::socket_tcp_connect:
            case target_action_t::socket_tcp_listen:
            case target_action_t::socket_udp_bind:
            case target_action_t::socket_udp_connect:
            {
                // Socket actions share the global TCP/UDP parsers, then perform
                // an additional target-local duplicate fd check. Runtime will
                // merge global and target sockets; duplicate fds inside one
                // target are rejected here so the failure is tied to the exact
                // command that introduced it.
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
                            if(has_duplicate_preopen_socket_fd(target)) [[unlikely]]
                            {
                                return print_usage_error(
                                    parameter,
                                    u8"Conflicting module action. Cannot assign the same WASI socket fd more than once in one WASI Preview 1 target.");
                            }
                            mark_consumed(extra3);
                            return parameter_return_type::def;
                        }

                        auto const ret{apply_socket_callback_to_override(target, parameter_name, socket_parameter, callback, extra1->str, extra2->str)};
                        if(ret != parameter_return_type::def) [[unlikely]] { return ret; }
                        if(has_duplicate_preopen_socket_fd(target)) [[unlikely]]
                        {
                            return print_usage_error(
                                parameter,
                                u8"Conflicting module action. Cannot assign the same WASI socket fd more than once in one WASI Preview 1 target.");
                        }
                        mark_consumed(extra2);
                        return parameter_return_type::def;
                    }};

                switch(action)
                {
                case target_action_t::socket_tcp_connect:
                    return apply_socket_action(::uwvm2::utils::container::u8cstring_view{u8"--wasip1-global-socket-tcp-connect"},
                                               ::uwvm2::uwvm::cmdline::params::wasip1_global_socket_tcp_connect,
                                               ::uwvm2::uwvm::cmdline::params::details::wasip1_global_socket_tcp_connect_callback);
                case target_action_t::socket_tcp_listen:
                    return apply_socket_action(::uwvm2::utils::container::u8cstring_view{u8"--wasip1-global-socket-tcp-listen"},
                                               ::uwvm2::uwvm::cmdline::params::wasip1_global_socket_tcp_listen,
                                               ::uwvm2::uwvm::cmdline::params::details::wasip1_global_socket_tcp_listen_callback);
                case target_action_t::socket_udp_bind:
                    return apply_socket_action(::uwvm2::utils::container::u8cstring_view{u8"--wasip1-global-socket-udp-bind"},
                                               ::uwvm2::uwvm::cmdline::params::wasip1_global_socket_udp_bind,
                                               ::uwvm2::uwvm::cmdline::params::details::wasip1_global_socket_udp_bind_callback);
                case target_action_t::socket_udp_connect:
                    return apply_socket_action(::uwvm2::utils::container::u8cstring_view{u8"--wasip1-global-socket-udp-connect"},
                                               ::uwvm2::uwvm::cmdline::params::wasip1_global_socket_udp_connect,
                                               ::uwvm2::uwvm::cmdline::params::details::wasip1_global_socket_udp_connect_callback);
                default:
                    break;
                }
                break;
            }
#  endif
            }

            return print_usage_error(parameter, u8"Unknown module action.");
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::cmdline::parameter_return_type
            apply_module_action_sequence(::uwvm2::utils::cmdline::parameter const& parameter,
                                         ::uwvm2::utils::cmdline::parameter_parsing_results* mark_begin,
                                         ::uwvm2::utils::cmdline::parameter_parsing_results* action_arg,
                                         ::uwvm2::utils::cmdline::parameter_parsing_results* para_end,
                                         override_state_t& target) noexcept
        {
            using parameter_return_type = ::uwvm2::utils::cmdline::parameter_return_type;
            using parameter_type = ::uwvm2::utils::cmdline::parameter_parsing_results_type;

            // Legacy sequence form:
            //   <target> action [payload...] action [payload...]
            // Each successful action marks its own payload as occupied; the loop
            // then advances over those occupied tokens until it finds the next
            // action word or reaches the end of the parsed command line.
            auto curr_action{action_arg};
            while(curr_action != para_end && curr_action->type == parameter_type::arg)
            {
                target_action_t action{};
                if(!parse_target_action(::uwvm2::utils::container::u8string_view{curr_action->str}, action)) [[unlikely]]
                {
                    return print_usage_error(parameter, u8"Unknown module action.");
                }

                auto const ret{apply_module_action(parameter,
                                                   mark_begin,
                                                   curr_action,
                                                   action,
                                                   curr_action + 1u,
                                                   para_end,
                                                   target)};
                if(ret != parameter_return_type::def) [[unlikely]] { return ret; }

                for(++curr_action; curr_action != para_end && curr_action->type == parameter_type::occupied_arg; ++curr_action) {}
            }

            return parameter_return_type::def;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::cmdline::parameter_return_type apply_target_action(::uwvm2::utils::cmdline::parameter const& parameter,
                                                                                                ::uwvm2::utils::cmdline::parameter_parsing_results* target_arg,
                                                                                                ::uwvm2::utils::cmdline::parameter_parsing_results* para_end,
                                                                                                override_state_t& target,
                                                                                                target_action_t action) noexcept
        { return apply_module_action(parameter, target_arg, target_arg, action, target_arg + 1u, para_end, target); }
    }  // namespace wasip1_module_details

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

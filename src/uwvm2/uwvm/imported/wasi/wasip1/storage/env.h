/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
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
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>  // wasi
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/imported/wasi/wasip1/impl.h>
# include <uwvm2/object/memory/linear/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::imported::wasi::wasip1::storage
{
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)
    using wasip1_env_type = ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t>;

    // Command-line environment edits are stored as views into the already-owned
    // command-line argument storage. They are consumed only during pre-execution
    // WASI environment initialization, so no extra string ownership is required
    // here. The final "KEY=VALUE" strings are materialized later in
    // wasip1_environment_storage or in a target state's environment_storage.
    //
    // Delete entries are represented as a set because duplicate deletion is a
    // command-line error and initialization only needs membership tests.
    // Add-or-replace entries are represented as a map because the observable
    // semantics are keyed by environment name: inserting the same name again
    // replaces the previous value before the environment is materialized.
    using wasip1_environment_name_set_t =
        ::uwvm2::utils::container::unordered_flat_set<::uwvm2::utils::container::u8string_view,
                                                      ::uwvm2::utils::container::pred::u8string_view_hash,
                                                      ::uwvm2::utils::container::pred::u8string_view_equal>;
    using wasip1_environment_map_t =
        ::uwvm2::utils::container::unordered_flat_map<::uwvm2::utils::container::u8string_view,
                                                      ::uwvm2::utils::container::u8string_view,
                                                      ::uwvm2::utils::container::pred::u8string_view_hash,
                                                      ::uwvm2::utils::container::pred::u8string_view_equal>;

    enum class wasip1_module_target_kind_t : unsigned
    {
        main_wasm = 0u,
        preload_wasm,
        preloaded_dl,
        weak_symbol
    };

    // One configured target state is used for both public target models:
    //
    // * a "single" target, keyed directly by one Wasm module name; and
    // * a named "group" target, keyed by group name and then referenced by one
    //   or more module names.
    //
    // The runtime only needs a resolved state for "this Wasm module", so both
    // forms intentionally share this storage. Each option stores a value plus an
    // explicit presence bit when the default value is meaningful and must be
    // distinguishable from "inherit the global setting".
    struct wasip1_group_state_t
    {
        // Import visibility override. If unset, the module follows the global
        // `local_preload_wasip1` setting; if set, this target alone can enable
        // or disable the built-in `wasi_snapshot_preview1` module.
        bool enabled{};
        bool enabled_is_set{};

        // Host API export visibility override. This controls whether the stable
        // local WASI Preview 1 host API is exported to preload-DL/weak-symbol
        // consumers for this target instead of using the global default.
        bool expose_host_api{};
        bool expose_host_api_is_set{};

        // Environment inheritance override. `false` is a real value, so the
        // presence bit decides whether to use the target value or inherit the
        // global noinherit/inherit decision.
        bool noinherit_system_environment{};
        bool noinherit_system_environment_is_set{};

        // Runtime UTF-8 validation override for this target. Command-line mount
        // parsing has already happened by the time this state is initialized, so
        // this only affects WASI calls that validate guest memory strings later.
        bool disable_utf8_check{};
        bool disable_utf8_check_is_set{};

        // Target fd limit override. The command-line callback translates user
        // value 0 to the maximum WASI fd so this field can be copied directly
        // into the concrete environment.
        bool fd_limit_is_set{};
        ::std::size_t fd_limit{};

        // Partial argv override. set-argv0 patches argv[0] over the default
        // `--run <wasm> ...` vector and is mutually exclusive with force-args.
        bool argv0_is_set{};
        ::uwvm2::utils::container::u8string argv0_storage{};

        // Complete argv override. A zero-length vector is valid, so the presence
        // bit is required to distinguish "force zero args" from "not set".
        bool force_args_is_set{};
        ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> force_argument_storage{};

        // Trace configuration is copied into the initialized environment. The
        // group kind/name fields are metadata for trace formatting only; they do
        // not affect import visibility or target lookup.
        bool trace_wasip1_call{};
        bool trace_wasip1_call_is_set{};
        ::uwvm2::imported::wasi::wasip1::environment::trace_wasip1_output_target_t trace_wasip1_output_target{};
        ::uwvm2::utils::container::u8string trace_wasip1_output_file_path_storage{};
        ::uwvm2::imported::wasi::wasip1::environment::trace_wasip1_group_kind_t trace_wasip1_group_kind{
            ::uwvm2::imported::wasi::wasip1::environment::trace_wasip1_group_kind_t::single};
        ::uwvm2::utils::container::u8string trace_wasip1_group_name_storage{};

        // Target-local environment edits. These mirror the global edit model but
        // are applied after the global command-line defaults when the target's
        // concrete WASI environment is initialized. A target map entry therefore
        // overrides a global add-or-replace entry with the same key, while a
        // target delete entry participates in the combined delete set.
        wasip1_environment_name_set_t delete_system_environment{};
        wasip1_environment_map_t add_or_replace_environment{};
        ::uwvm2::utils::container::vector<::uwvm2::imported::wasi::wasip1::environment::mount_dir_root_t> mount_dir_roots{};
#  if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
        ::uwvm2::utils::container::vector<::uwvm2::imported::wasi::wasip1::environment::preopen_socket_t> preopen_sockets{};
#  endif

        ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> environment_storage{};
        ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> argument_storage{};

        wasip1_env_type env{};

        [[nodiscard]] inline constexpr bool has_override() const noexcept
        {
            return this->enabled_is_set || this->expose_host_api_is_set || this->noinherit_system_environment_is_set || this->disable_utf8_check_is_set ||
                   this->fd_limit_is_set || this->argv0_is_set || this->force_args_is_set || this->trace_wasip1_call_is_set ||
                   !this->delete_system_environment.empty() || !this->add_or_replace_environment.empty() || !this->mount_dir_roots.empty()
#  if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
                   || !this->preopen_sockets.empty()
#  endif
                ;
        }
    };

    using wasip1_module_override_t = wasip1_group_state_t;
    using wasip1_group_index_t = ::std::size_t;
    inline constexpr wasip1_group_index_t invalid_wasip1_group_index{static_cast<wasip1_group_index_t>(-1)};
    using wasip1_group_state_storage_t = ::uwvm2::utils::container::deque<wasip1_group_state_t>;
    using wasip1_group_index_map_t = ::uwvm2::utils::container::unordered_flat_map<::uwvm2::utils::container::u8string,
                                                                                   wasip1_group_index_t,
                                                                                   ::uwvm2::utils::container::pred::u8string_view_hash,
                                                                                   ::uwvm2::utils::container::pred::u8string_view_equal>;

    /// @brief     Do not inherit host environment variables into WASI Preview 1 environment.
    inline bool wasip1_noinherit_system_environment{};  // [global]

    /// @brief     Delete inherited host environment variables by name.
    /// @details   This set is ignored when wasip1_noinherit_system_environment is set because no host entries are imported.
    ///            The set still remains valid command-line state so target override initialization can merge it with
    ///            target-local delete sets without introducing duplicate entries.
    inline wasip1_environment_name_set_t wasip1_delete_system_environment{};  // [global]

    /// @brief     Add or replace environment variables by name.
    /// @details   These entries are applied after host inheritance and after delete processing. This ordering is intentional:
    ///            an explicit add-or-replace command should be able to recreate a variable even if an inherited host variable
    ///            with the same name was deleted earlier. When target overrides are initialized, target entries replace global
    ///            entries with the same key before materialization.
    inline wasip1_environment_map_t wasip1_add_or_replace_environment{};  // [global]

    /// @brief     Store raw WASI Preview 1 mount paths instead of normalized guest paths.
    inline bool wasip1_disable_mount_path_normalization{};  // [global]

    /// @brief     Allow duplicate or overlapping WASI Preview 1 mount guest paths.
    inline bool wasip1_allow_overlapping_mount_paths{};  // [global]

    /// @brief     The storage of final WASI Preview 1 environment variables ("key=value").
    inline ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> wasip1_environment_storage{};  // [global]

    /// @brief     The storage of final WASI Preview 1 arguments.
    inline ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> wasip1_argument_storage{};  // [global]

    /// @brief     Whether the global-default WASI Preview 1 argv[0] was explicitly overridden.
    inline bool wasip1_argv0_is_set{};  // [global]

    /// @brief     Override for WASI Preview 1 argv[0].
    inline ::uwvm2::utils::container::u8string wasip1_argv0_storage{};  // [global]

    /// @brief     Whether the global-default WASI Preview 1 argv vector is explicitly replaced.
    inline bool wasip1_force_args_is_set{};  // [global]

    /// @brief     Complete global-default WASI Preview 1 argv vector supplied by `--wasip1-global-force-args`.
    inline ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> wasip1_force_argument_storage{};  // [global]

    /// @brief     Default WasiPreview1 environment
    inline wasip1_env_type default_wasip1_env{};  // [global]

    // All configured target states live in one deque so pointers/references to a
    // state remain stable while command-line parsing appends more groups.
    inline wasip1_group_state_storage_t configured_wasip1_groups{};    // [global]

    // group name -> state index for `--wasip1-group-create <group>`.
    inline wasip1_group_index_map_t configured_named_wasip1_groups{};  // [global]

    // module name -> state index for `--wasip1-single-create <module>`.
    inline wasip1_group_index_map_t configured_module_wasip1_anonymous_groups{};  // [global]

    // module name -> named group state index for `--wasip1-group-add-module`.
    inline wasip1_group_index_map_t configured_named_wasip1_module_groups{};      // [global]

#  if defined(UWVM_USE_THREAD_LOCAL)
    // Fast path: these are consulted on host API calls, so keep direct
    // thread_local storage and direct use sites when UWVM_USE_THREAD_LOCAL is on.
#   if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#    ifdef UWVM
    [[__gnu__::__tls_model__("local-exec")]]
#    else
    [[__gnu__::__tls_model__("local-dynamic")]]
#    endif
#   endif
    // Host API calls consult current_wasip1_env_ptr instead of the global
    // environment directly. The runtime sets this pointer around calls into a
    // Wasm module so one process can execute modules with different WASI
    // defaults, mount tables, sockets, argv/env values, and trace settings.
    inline thread_local wasip1_env_type* current_wasip1_env_ptr{::std::addressof(default_wasip1_env)};  // [global] [thread_local]

#   if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#    ifdef UWVM
    [[__gnu__::__tls_model__("local-exec")]]
#    else
    [[__gnu__::__tls_model__("local-dynamic")]]
#    endif
#   endif
    // The target metadata is separate from the environment pointer because
    // dependency checking and tracing sometimes need the module identity even
    // before a concrete environment pointer has been switched.
    inline thread_local bool current_wasip1_target_is_set{};  // [global] [thread_local]

#   if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#    ifdef UWVM
    [[__gnu__::__tls_model__("local-exec")]]
#    else
    [[__gnu__::__tls_model__("local-dynamic")]]
#    endif
#   endif
    inline thread_local wasip1_module_target_kind_t current_wasip1_target_kind{};  // [global] [thread_local]

#   if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#    ifdef UWVM
    [[__gnu__::__tls_model__("local-exec")]]
#    else
    [[__gnu__::__tls_model__("local-dynamic")]]
#    endif
#   endif
    inline thread_local ::uwvm2::utils::container::u8string_view current_wasip1_target_module_name{};  // [global] [thread_local]
#  else
    // Compatibility path for builds without C++ thread_local. The state is still
    // per native thread, but looked up through a container keyed by thread id.
    using os_thread_id_t =
#   if defined(__SINGLE_THREAD__)
        ::std::size_t;
#   else
        decltype(::fast_io::this_thread::get_id());
#   endif

    struct wasip1_thread_state
    {
        wasip1_env_type* current_env_ptr{::std::addressof(default_wasip1_env)};
        bool current_target_is_set{};
        wasip1_module_target_kind_t current_target_kind{};
        ::uwvm2::utils::container::u8string_view current_target_module_name{};
    };

    [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr os_thread_id_t current_thread_id() noexcept
    {
#   if defined(__SINGLE_THREAD__)
        return 0uz;
#   else
        return ::fast_io::this_thread::get_id();
#   endif
    }

    inline ::uwvm2::utils::container::concurrent_node_map<os_thread_id_t, wasip1_thread_state> current_wasip1_states{};  // [global]

    [[nodiscard]] inline constexpr wasip1_thread_state& current_wasip1_state() noexcept
    {
        auto const id{current_thread_id()};
        wasip1_thread_state* st{};

        current_wasip1_states.try_emplace_and_visit(
            id,
            [&](auto& kv) noexcept { st = ::std::addressof(kv.second); },
            [&](auto& kv) noexcept { st = ::std::addressof(kv.second); });

        if(st == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        return *st;
    }
#  endif

#  if !defined(UWVM_USE_THREAD_LOCAL)
    [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasip1_env_type*& current_wasip1_env_ptr_ref() noexcept
    { return current_wasip1_state().current_env_ptr; }

    [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool& current_wasip1_target_is_set_ref() noexcept
    { return current_wasip1_state().current_target_is_set; }

    [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasip1_module_target_kind_t& current_wasip1_target_kind_ref() noexcept
    { return current_wasip1_state().current_target_kind; }

    [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::uwvm2::utils::container::u8string_view& current_wasip1_target_module_name_ref() noexcept
    { return current_wasip1_state().current_target_module_name; }
#  endif

    [[nodiscard]] inline constexpr bool reopen_wasip1_trace_output_file(::fast_io::u8native_file & output_file,
                                                              ::uwvm2::utils::container::u8string_view path,
                                                              bool nt_path_warning = true) noexcept
    {
        static_cast<void>(nt_path_warning);

#  if !defined(__AVR__) && !((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) && !(defined(__MSDOS__) || defined(__DJGPP__)) &&             \
      !(defined(__NEWLIB__) && !defined(__CYGWIN__)) && !defined(_PICOLIBC__) && !defined(__wasm__)
#   if defined(_WIN32) && !defined(_WIN32_WINDOWS)
        if(path.starts_with(u8"::NT::"))
        {
            ::fast_io::u8cstring_view const nt_path{::fast_io::containers::null_terminated, path.subview(6uz)};

            if(nt_path_warning && ::uwvm2::uwvm::io::show_nt_path_warning)
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    u8"[warn]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Resolve to NT path: \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    nt_path,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\".",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8" (nt-path)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                if(::uwvm2::uwvm::io::nt_path_warning_fatal) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Convert warnings to fatal errors. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(nt-path)\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }

#    if defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
            try
#    endif
            {
                output_file = ::fast_io::u8native_file{::fast_io::io_kernel, nt_path, ::fast_io::open_mode::out | ::fast_io::open_mode::follow};
                return true;
            }
#    if defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
            catch(::fast_io::error)
            {
                return false;
            }
#    endif
        }
#   endif

#   if defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
        try
#   endif
        {
            ::fast_io::u8cstring_view const file_path{::fast_io::containers::null_terminated, path};
            output_file = ::fast_io::u8native_file{file_path, ::fast_io::open_mode::out | ::fast_io::open_mode::follow};
            return true;
        }
#   if defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
        catch(::fast_io::error)
        {
            return false;
        }
#   endif
#  else
        static_cast<void>(output_file);
        static_cast<void>(path);
        return false;
#  endif
    }

    [[nodiscard]] inline constexpr wasip1_env_type& current_wasip1_env() noexcept
    {
#  if defined(UWVM_USE_THREAD_LOCAL)
        if(current_wasip1_env_ptr == nullptr) [[unlikely]] { return default_wasip1_env; }
        return *current_wasip1_env_ptr;
#  else
        auto const current_env_ptr{current_wasip1_env_ptr_ref()};
        if(current_env_ptr == nullptr) [[unlikely]] { return default_wasip1_env; }
        return *current_env_ptr;
#  endif
    }

    struct scoped_current_wasip1_env_t
    {
        wasip1_env_type* saved{};

        // RAII switch used by runtime dispatch around calls into a target-bound
        // Wasm module. Restoring in the destructor keeps nested host calls and
        // early exits from leaking one module's WASI state into the next module.
        inline constexpr explicit scoped_current_wasip1_env_t(wasip1_env_type& env) noexcept :
#  if defined(UWVM_USE_THREAD_LOCAL)
            saved{current_wasip1_env_ptr}
        { current_wasip1_env_ptr = ::std::addressof(env); }
#  else
            saved{current_wasip1_env_ptr_ref()}
        { current_wasip1_env_ptr_ref() = ::std::addressof(env); }
#  endif

        scoped_current_wasip1_env_t(scoped_current_wasip1_env_t const&) = delete;
        scoped_current_wasip1_env_t& operator= (scoped_current_wasip1_env_t const&) = delete;

        inline constexpr ~scoped_current_wasip1_env_t()
        {
#  if defined(UWVM_USE_THREAD_LOCAL)
            if(this->saved == nullptr) [[unlikely]] { current_wasip1_env_ptr = ::std::addressof(default_wasip1_env); }
            else
            {
                current_wasip1_env_ptr = this->saved;
            }
#  else
            if(this->saved == nullptr) [[unlikely]] { current_wasip1_env_ptr_ref() = ::std::addressof(default_wasip1_env); }
            else
            {
                current_wasip1_env_ptr_ref() = this->saved;
            }
#  endif
        }
    };

    struct scoped_current_wasip1_target_t
    {
        bool saved_is_set{};
        wasip1_module_target_kind_t saved_kind{};
        ::uwvm2::utils::container::u8string_view saved_module_name{};

        // Record the active module identity for diagnostics and target lookup.
        // The string view refers to module-name storage owned by the loader, not
        // transient command-line argument memory.
        inline constexpr explicit scoped_current_wasip1_target_t(wasip1_module_target_kind_t target_kind, ::uwvm2::utils::container::u8string_view module_name) noexcept :
#  if defined(UWVM_USE_THREAD_LOCAL)
            saved_is_set{current_wasip1_target_is_set}, saved_kind{current_wasip1_target_kind}, saved_module_name{current_wasip1_target_module_name}
        {
            current_wasip1_target_is_set = true;
            current_wasip1_target_kind = target_kind;
            current_wasip1_target_module_name = module_name;
        }
#  else
            saved_is_set{current_wasip1_target_is_set_ref()},
            saved_kind{current_wasip1_target_kind_ref()},
            saved_module_name{current_wasip1_target_module_name_ref()}
        {
            current_wasip1_target_is_set_ref() = true;
            current_wasip1_target_kind_ref() = target_kind;
            current_wasip1_target_module_name_ref() = module_name;
        }
#  endif

        scoped_current_wasip1_target_t(scoped_current_wasip1_target_t const&) = delete;
        scoped_current_wasip1_target_t& operator= (scoped_current_wasip1_target_t const&) = delete;

        inline constexpr ~scoped_current_wasip1_target_t()
        {
#  if defined(UWVM_USE_THREAD_LOCAL)
            current_wasip1_target_is_set = this->saved_is_set;
            current_wasip1_target_kind = this->saved_kind;
            current_wasip1_target_module_name = this->saved_module_name;
#  else
            current_wasip1_target_is_set_ref() = this->saved_is_set;
            current_wasip1_target_kind_ref() = this->saved_kind;
            current_wasip1_target_module_name_ref() = this->saved_module_name;
#  endif
        }
    };

    [[nodiscard]] inline constexpr wasip1_group_state_t* find_wasip1_group_state(wasip1_group_index_t group_index) noexcept
    {
        if(group_index == invalid_wasip1_group_index || group_index >= configured_wasip1_groups.size()) [[unlikely]] { return nullptr; }
        return ::std::addressof(configured_wasip1_groups[group_index]);
    }

    [[nodiscard]] inline constexpr wasip1_group_state_t const* find_wasip1_group_state_const(wasip1_group_index_t group_index) noexcept
    { return find_wasip1_group_state(group_index); }

    [[nodiscard]] inline constexpr wasip1_group_index_t create_wasip1_group() noexcept
    {
        configured_wasip1_groups.emplace_back();
        return configured_wasip1_groups.size() - 1uz;
    }

    template <typename Map>
    [[nodiscard]] inline constexpr wasip1_group_index_t find_wasip1_group_index_in_map(Map const& map, ::uwvm2::utils::container::u8string_view name) noexcept
    {
        if(auto const it{map.find(name)}; it != map.end()) [[likely]] { return it->second; }
        return invalid_wasip1_group_index;
    }

    [[nodiscard]] inline constexpr wasip1_group_index_t find_named_wasip1_group_index(::uwvm2::utils::container::u8string_view group_name) noexcept
    { return find_wasip1_group_index_in_map(configured_named_wasip1_groups, group_name); }

    [[nodiscard]] inline constexpr wasip1_group_state_t* find_named_wasip1_group(::uwvm2::utils::container::u8string_view group_name) noexcept
    { return find_wasip1_group_state(find_named_wasip1_group_index(group_name)); }

    [[nodiscard]] inline constexpr wasip1_group_index_t try_create_named_wasip1_group_index(::uwvm2::utils::container::u8string_view group_name) noexcept
    {
        // Reserve the group name before creating the backing state so duplicate
        // group creation fails without allocating a second, unreachable state.
        auto const [it, inserted]{configured_named_wasip1_groups.try_emplace(::uwvm2::utils::container::u8string{group_name}, invalid_wasip1_group_index)};
        if(!inserted) [[unlikely]] { return invalid_wasip1_group_index; }

        it->second = create_wasip1_group();
        if(auto const state{find_wasip1_group_state(it->second)}; state != nullptr) [[likely]]
        {
            state->trace_wasip1_group_kind = ::uwvm2::imported::wasi::wasip1::environment::trace_wasip1_group_kind_t::custom_group;
            state->trace_wasip1_group_name_storage = ::uwvm2::utils::container::u8string{group_name};
        }
        return it->second;
    }

    [[nodiscard]] inline constexpr bool try_add_wasip1_module_to_named_group(::uwvm2::utils::container::u8string_view module_name,
                                                                   wasip1_group_index_t group_index) noexcept
    {
        // A module may not be configured as both an anonymous single target and
        // a named group member. Rejecting the overlap during parsing keeps the
        // runtime target lookup one-to-one.
        if(find_wasip1_group_index_in_map(configured_module_wasip1_anonymous_groups, module_name) != invalid_wasip1_group_index) [[unlikely]] { return false; }

        auto const [it, inserted]{configured_named_wasip1_module_groups.try_emplace(::uwvm2::utils::container::u8string{module_name}, group_index)};
        if(inserted) [[likely]] { return true; }
        static_cast<void>(it);
        return false;
    }

    [[nodiscard]] inline constexpr wasip1_group_index_t find_named_wasip1_module_group_index(::uwvm2::utils::container::u8string_view module_name) noexcept
    { return find_wasip1_group_index_in_map(configured_named_wasip1_module_groups, module_name); }

    [[nodiscard]] inline constexpr wasip1_group_index_t find_targetless_wasip1_anonymous_module_group_index(::uwvm2::utils::container::u8string_view module_name) noexcept
    { return find_wasip1_group_index_in_map(configured_module_wasip1_anonymous_groups, module_name); }

    [[nodiscard]] inline constexpr wasip1_group_state_t* find_targetless_wasip1_module_override(::uwvm2::utils::container::u8string_view module_name) noexcept
    { return find_wasip1_group_state(find_targetless_wasip1_anonymous_module_group_index(module_name)); }

    [[nodiscard]] inline constexpr wasip1_group_index_t find_wasip1_anonymous_module_group_index([[maybe_unused]] wasip1_module_target_kind_t target_kind,
                                                                                       ::uwvm2::utils::container::u8string_view module_name) noexcept
    { return find_targetless_wasip1_anonymous_module_group_index(module_name); }

    [[nodiscard]] inline constexpr wasip1_group_index_t find_wasip1_module_group_index(wasip1_module_target_kind_t target_kind,
                                                                             ::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        // Normal command-line validation prevents a module from having both an
        // anonymous and named group binding. If corrupted state reaches here,
        // terminate rather than arbitrarily choosing one security policy.
        auto const anonymous_group_index{find_wasip1_anonymous_module_group_index(target_kind, module_name)};
        auto const named_group_index{find_named_wasip1_module_group_index(module_name)};
        if(anonymous_group_index != invalid_wasip1_group_index && named_group_index != invalid_wasip1_group_index && anonymous_group_index != named_group_index)
            [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }
        if(anonymous_group_index != invalid_wasip1_group_index) [[unlikely]] { return anonymous_group_index; }
        return named_group_index;
    }

    [[nodiscard]] inline constexpr wasip1_group_state_t* find_wasip1_module_override(wasip1_module_target_kind_t target_kind,
                                                                           ::uwvm2::utils::container::u8string_view module_name) noexcept
    { return find_wasip1_group_state(find_wasip1_module_group_index(target_kind, module_name)); }

    [[nodiscard]] inline constexpr wasip1_module_override_t const* find_wasip1_module_override_const(wasip1_module_target_kind_t target_kind,
                                                                                           ::uwvm2::utils::container::u8string_view module_name) noexcept
    { return find_wasip1_module_override(target_kind, module_name); }

    [[nodiscard]] inline constexpr wasip1_group_state_t* try_create_targetless_wasip1_module_override(::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        // A single target is anonymous storage keyed by module name. It cannot
        // be created after the module has already been bound to a named group.
        if(find_named_wasip1_module_group_index(module_name) != invalid_wasip1_group_index) [[unlikely]] { return nullptr; }

        auto const [it, inserted]{
            configured_module_wasip1_anonymous_groups.try_emplace(::uwvm2::utils::container::u8string{module_name}, invalid_wasip1_group_index)};
        if(!inserted) [[unlikely]] { return nullptr; }

        it->second = create_wasip1_group();
        auto const state{find_wasip1_group_state(it->second)};
        if(state == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        state->trace_wasip1_group_kind = ::uwvm2::imported::wasi::wasip1::environment::trace_wasip1_group_kind_t::single;
        state->trace_wasip1_group_name_storage.clear();
        return state;
    }

    [[nodiscard]] inline constexpr bool has_any_configured_wasip1_module_override() noexcept
    {
        for(auto const& state: configured_wasip1_groups)
        {
            if(state.has_override()) [[unlikely]] { return true; }
        }
        return false;
    }

    [[nodiscard]] inline constexpr bool has_any_enabled_wasip1_wasm_module_override() noexcept
    {
        for(auto const& state: configured_wasip1_groups)
        {
            if(state.enabled_is_set && state.enabled) [[unlikely]] { return true; }
        }
        return false;
    }
# endif
#endif
}  // namespace uwvm2::uwvm::imported::wasi::wasip1::storage

#ifndef UWVM_MODULE
// macro
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>  // wasi
# include <uwvm2/utils/macro/pop_macros.h>
#endif

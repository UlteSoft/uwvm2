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
# include <memory>
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
# include <uwvm2/imported/wasi/wasip1/impl.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/storage/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::imported::wasi::wasip1::local_imported
{
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)

    namespace details
    {
        using feature_list = ::uwvm2::uwvm::wasm::type::binfmt_ver1_feature_list_t;
        using wasm_value_type = ::uwvm2::uwvm::wasm::type::feature_list_final_value_type_t<feature_list>;

        template <typename T>
        inline consteval wasm_value_type map_to_wasm_value_type() noexcept
        {
            using type = ::std::remove_cvref_t<T>;
            using base_type_holder = ::std::conditional_t<::std::is_enum_v<type>, ::std::underlying_type<type>, ::std::type_identity<type>>;
            using base_type = typename base_type_holder::type;

            static_assert(::std::is_integral_v<base_type>, "WASI local_imported only supports integral/enum scalars");

            if constexpr(sizeof(base_type) <= sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32)) { return wasm_value_type::i32; }
            else if constexpr(sizeof(base_type) <= sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64)) { return wasm_value_type::i64; }
            else
            {
                static_assert(sizeof(base_type) <= sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64),
                              "unsupported scalar size for WASI local_imported");
                return wasm_value_type::i64;
            }
        }

        template <typename Ret>
        struct wasip1_result_tuple_nonvoid
        {
            using type = ::uwvm2::uwvm::wasm::type::import_function_result_tuple_t<feature_list, map_to_wasm_value_type<Ret>()>;
        };

        template <typename Ret>
        struct wasip1_result_tuple
        {
            using clean_ret = ::std::remove_cvref_t<Ret>;
            using holder = ::std::
                conditional_t<::std::is_void_v<clean_ret>, ::std::type_identity<::uwvm2::utils::container::tuple<>>, wasip1_result_tuple_nonvoid<clean_ret>>;
            using type = typename holder::type;
        };

        template <typename Ret>
        using wasip1_result_tuple_t = typename wasip1_result_tuple<Ret>::type;

        template <typename>
        struct wasip1_fn_traits;

        template <typename Ret, typename Env, typename... Args>
        struct wasip1_fn_traits<Ret (*)(Env&, Args...)>
        {
            using ret_type = Ret;
            using env_type = Env;
            using arg_tuple = ::std::tuple<Args...>;
        };

        template <typename T, typename U>
        inline constexpr ::std::remove_cvref_t<T> cast_wasm_scalar(U v) noexcept
        {
            using out_type = ::std::remove_cvref_t<T>;
            if constexpr(::std::is_enum_v<out_type>)
            {
                using underlying = ::std::underlying_type_t<out_type>;
                return static_cast<out_type>(static_cast<underlying>(v));
            }
            else
            {
                return static_cast<out_type>(v);
            }
        }

        template <auto Fn>
        struct wasip1_local_imported_function_base;

        template <typename Ret, typename Env, typename... Args, Ret (*Fn)(Env&, Args...)>
        struct wasip1_local_imported_function_base<Fn>
        {
            static_assert(::std::is_same_v<Env, ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_env_type>,
                          "WASI local_imported wrapper expects uwvm wasip1 env type");

            using value_type = wasm_value_type;
            using res_tuple = wasip1_result_tuple_t<Ret>;
            using para_tuple = ::uwvm2::uwvm::wasm::type::import_function_parameter_tuple_t<feature_list, map_to_wasm_value_type<Args>()...>;
            using local_imported_function_type = ::uwvm2::uwvm::wasm::type::local_imported_function_type_t<res_tuple, para_tuple>;

            inline static constexpr void call(local_imported_function_type& func_type) noexcept
            {
                auto& env{::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env};

                if constexpr(::std::is_void_v<Ret>)
                {
                    [&]<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
                    { Fn(env, cast_wasm_scalar<Args>(::uwvm2::utils::container::get<I>(func_type.params))...); }(::std::make_index_sequence<sizeof...(Args)>{});
                }
                else
                {
                    auto const retv{[&]<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
                                    {
                                        return Fn(env, cast_wasm_scalar<Args>(::uwvm2::utils::container::get<I>(func_type.params))...);
                                    }(::std::make_index_sequence<sizeof...(Args)>{})};

                    using res0_type = ::std::remove_cvref_t<decltype(::uwvm2::utils::container::get<0>(func_type.res))>;
                    ::uwvm2::utils::container::get<0>(func_type.res) = static_cast<res0_type>(retv);
                }
            }
        };

        template <auto Fn, auto& Name>
        struct wasip1_local_imported_function final : wasip1_local_imported_function_base<Fn>
        {
            inline static constexpr ::uwvm2::utils::container::u8string_view function_name{Name};
        };

        inline constexpr char8_t name_args_get[] = u8"args_get";
        using args_get = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::args_get), name_args_get>;

        inline constexpr char8_t name_args_sizes_get[] = u8"args_sizes_get";
        using args_sizes_get = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::args_sizes_get), name_args_sizes_get>;

        inline constexpr char8_t name_clock_res_get[] = u8"clock_res_get";
        using clock_res_get = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::clock_res_get), name_clock_res_get>;

        inline constexpr char8_t name_clock_time_get[] = u8"clock_time_get";
        using clock_time_get = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::clock_time_get), name_clock_time_get>;

        inline constexpr char8_t name_environ_get[] = u8"environ_get";
        using environ_get = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::environ_get), name_environ_get>;

        inline constexpr char8_t name_environ_sizes_get[] = u8"environ_sizes_get";
        using environ_sizes_get =
            wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::environ_sizes_get), name_environ_sizes_get>;

        inline constexpr char8_t name_fd_advise[] = u8"fd_advise";
        using fd_advise = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_advise), name_fd_advise>;

        inline constexpr char8_t name_fd_allocate[] = u8"fd_allocate";
        using fd_allocate = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_allocate), name_fd_allocate>;

        inline constexpr char8_t name_fd_close[] = u8"fd_close";
        using fd_close = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_close), name_fd_close>;

        inline constexpr char8_t name_fd_datasync[] = u8"fd_datasync";
        using fd_datasync = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_datasync), name_fd_datasync>;

        inline constexpr char8_t name_fd_fdstat_get[] = u8"fd_fdstat_get";
        using fd_fdstat_get = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get), name_fd_fdstat_get>;

        inline constexpr char8_t name_fd_fdstat_set_flags[] = u8"fd_fdstat_set_flags";
        using fd_fdstat_set_flags =
            wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_flags), name_fd_fdstat_set_flags>;

        inline constexpr char8_t name_fd_fdstat_set_rights[] = u8"fd_fdstat_set_rights";
        using fd_fdstat_set_rights =
            wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_rights), name_fd_fdstat_set_rights>;

        inline constexpr char8_t name_fd_filestat_get[] = u8"fd_filestat_get";
        using fd_filestat_get = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_filestat_get), name_fd_filestat_get>;

        inline constexpr char8_t name_fd_filestat_set_size[] = u8"fd_filestat_set_size";
        using fd_filestat_set_size =
            wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_size), name_fd_filestat_set_size>;

        inline constexpr char8_t name_fd_filestat_set_times[] = u8"fd_filestat_set_times";
        using fd_filestat_set_times =
            wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_times), name_fd_filestat_set_times>;

        inline constexpr char8_t name_fd_pread[] = u8"fd_pread";
        using fd_pread = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_pread), name_fd_pread>;

        inline constexpr char8_t name_fd_prestat_dir_name[] = u8"fd_prestat_dir_name";
        using fd_prestat_dir_name =
            wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_prestat_dir_name), name_fd_prestat_dir_name>;

        inline constexpr char8_t name_fd_prestat_get[] = u8"fd_prestat_get";
        using fd_prestat_get = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_prestat_get), name_fd_prestat_get>;

        inline constexpr char8_t name_fd_pwrite[] = u8"fd_pwrite";
        using fd_pwrite = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_pwrite), name_fd_pwrite>;

        inline constexpr char8_t name_fd_read[] = u8"fd_read";
        using fd_read = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_read), name_fd_read>;

        inline constexpr char8_t name_fd_readdir[] = u8"fd_readdir";
        using fd_readdir = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_readdir), name_fd_readdir>;

        inline constexpr char8_t name_fd_renumber[] = u8"fd_renumber";
        using fd_renumber = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_renumber), name_fd_renumber>;

        inline constexpr char8_t name_fd_seek[] = u8"fd_seek";
        using fd_seek = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_seek), name_fd_seek>;

        inline constexpr char8_t name_fd_sync[] = u8"fd_sync";
        using fd_sync = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_sync), name_fd_sync>;

        inline constexpr char8_t name_fd_tell[] = u8"fd_tell";
        using fd_tell = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_tell), name_fd_tell>;

        inline constexpr char8_t name_fd_write[] = u8"fd_write";
        using fd_write = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::fd_write), name_fd_write>;

        inline constexpr char8_t name_path_create_directory[] = u8"path_create_directory";
        using path_create_directory =
            wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::path_create_directory), name_path_create_directory>;

        inline constexpr char8_t name_path_filestat_get[] = u8"path_filestat_get";
        using path_filestat_get =
            wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::path_filestat_get), name_path_filestat_get>;

        inline constexpr char8_t name_path_filestat_set_times[] = u8"path_filestat_set_times";
        using path_filestat_set_times =
            wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::path_filestat_set_times), name_path_filestat_set_times>;

        inline constexpr char8_t name_path_link[] = u8"path_link";
        using path_link = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::path_link), name_path_link>;

        inline constexpr char8_t name_path_open[] = u8"path_open";
        using path_open = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::path_open), name_path_open>;

        inline constexpr char8_t name_path_readlink[] = u8"path_readlink";
        using path_readlink = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::path_readlink), name_path_readlink>;

        inline constexpr char8_t name_path_remove_directory[] = u8"path_remove_directory";
        using path_remove_directory =
            wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::path_remove_directory), name_path_remove_directory>;

        inline constexpr char8_t name_path_rename[] = u8"path_rename";
        using path_rename = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::path_rename), name_path_rename>;

        inline constexpr char8_t name_path_symlink[] = u8"path_symlink";
        using path_symlink = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::path_symlink), name_path_symlink>;

        inline constexpr char8_t name_path_unlink_file[] = u8"path_unlink_file";
        using path_unlink_file =
            wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::path_unlink_file), name_path_unlink_file>;

        inline constexpr char8_t name_poll_oneoff[] = u8"poll_oneoff";
        using poll_oneoff = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::poll_oneoff), name_poll_oneoff>;

        inline constexpr char8_t name_proc_exit[] = u8"proc_exit";
        using proc_exit = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::proc_exit), name_proc_exit>;

        inline constexpr char8_t name_proc_raise[] = u8"proc_raise";
        using proc_raise = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::proc_raise), name_proc_raise>;

        inline constexpr char8_t name_random_get[] = u8"random_get";
        using random_get = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::random_get), name_random_get>;

        inline constexpr char8_t name_sched_yield[] = u8"sched_yield";
        using sched_yield = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::sched_yield), name_sched_yield>;

        inline constexpr char8_t name_sock_accept[] = u8"sock_accept";
        using sock_accept = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::sock_accept), name_sock_accept>;

        inline constexpr char8_t name_sock_recv[] = u8"sock_recv";
        using sock_recv = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::sock_recv), name_sock_recv>;

        inline constexpr char8_t name_sock_send[] = u8"sock_send";
        using sock_send = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::sock_send), name_sock_send>;

        inline constexpr char8_t name_sock_shutdown[] = u8"sock_shutdown";
        using sock_shutdown = wasip1_local_imported_function<::std::addressof(::uwvm2::imported::wasi::wasip1::func::sock_shutdown), name_sock_shutdown>;
    }  // namespace details

    struct wasip1_local_imported_module_t
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view module_name{u8"wasi_snapshot_preview1"};

        using local_function_tuple = ::uwvm2::utils::container::tuple<details::args_get,
                                                                      details::args_sizes_get,
                                                                      details::clock_res_get,
                                                                      details::clock_time_get,
                                                                      details::environ_get,
                                                                      details::environ_sizes_get,
                                                                      details::fd_advise,
                                                                      details::fd_allocate,
                                                                      details::fd_close,
                                                                      details::fd_datasync,
                                                                      details::fd_fdstat_get,
                                                                      details::fd_fdstat_set_flags,
                                                                      details::fd_fdstat_set_rights,
                                                                      details::fd_filestat_get,
                                                                      details::fd_filestat_set_size,
                                                                      details::fd_filestat_set_times,
                                                                      details::fd_pread,
                                                                      details::fd_prestat_dir_name,
                                                                      details::fd_prestat_get,
                                                                      details::fd_pwrite,
                                                                      details::fd_read,
                                                                      details::fd_readdir,
                                                                      details::fd_renumber,
                                                                      details::fd_seek,
                                                                      details::fd_sync,
                                                                      details::fd_tell,
                                                                      details::fd_write,
                                                                      details::path_create_directory,
                                                                      details::path_filestat_get,
                                                                      details::path_filestat_set_times,
                                                                      details::path_link,
                                                                      details::path_open,
                                                                      details::path_readlink,
                                                                      details::path_remove_directory,
                                                                      details::path_rename,
                                                                      details::path_symlink,
                                                                      details::path_unlink_file,
                                                                      details::poll_oneoff,
                                                                      details::proc_exit,
                                                                      details::proc_raise,
                                                                      details::random_get,
                                                                      details::sched_yield,
                                                                      details::sock_accept,
                                                                      details::sock_recv,
                                                                      details::sock_send,
                                                                      details::sock_shutdown>;
    };

    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_module<wasip1_local_imported_module_t>);
    static_assert(::uwvm2::uwvm::wasm::type::has_local_function_tuple<wasip1_local_imported_module_t>);
    /// @note This local_imported module only provides the WASI Preview1 importable function list.
    ///       The runtime environment (`default_wasip1_env`, memory binding, argv/envp, fd table, etc.) is initialized by the loader
    ///       (`init_wasip1_environment` inside `load_local_modules`) before any WASI function is invoked, so it is intentionally not modeled
    ///       as a "local_imported function"/concept requirement here.

    inline constexpr wasip1_local_imported_module_t wasip1_local_imported_module{};

# endif
#endif
}  // namespace uwvm2::uwvm::imported::wasi::wasip1::local_imported

#ifndef UWVM_MODULE
// macro
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>  // wasi
# include <uwvm2/utils/macro/pop_macros.h>
#endif

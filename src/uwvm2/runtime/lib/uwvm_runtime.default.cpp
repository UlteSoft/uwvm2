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

// std
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
// macro
#include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>

// platform
#if !UWVM_HAS_BUILTIN(__builtin_alloca) && (defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__) && !defined(__CYGWIN__))
# include <malloc.h>
#elif !UWVM_HAS_BUILTIN(__builtin_alloca)
# include <alloca.h>
#endif

#ifndef UWVM_MODULE
// import
# include <fast_io.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/optable/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/runtime/lib/uwvm_runtime.h>
# if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
#  include <uwvm2/uwvm/imported/wasi/wasip1/storage/env.h>
# endif
#endif

namespace uwvm2::runtime::uwvm_int
{
    namespace
    {
        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        using runtime_module_storage_t = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
        using runtime_imported_func_storage_t = ::uwvm2::uwvm::runtime::storage::imported_function_storage_t;
        using runtime_local_func_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t;
        using runtime_table_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t;
        using runtime_table_elem_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t;

        using capi_function_t = ::uwvm2::uwvm::wasm::type::capi_function_t;
        using local_imported_t = ::uwvm2::uwvm::wasm::type::local_imported_t;

        using compiled_module_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t;
        using compiled_local_func_t = ::uwvm2::runtime::compiler::uwvm_int::optable::local_func_storage_t;

        constexpr ::std::size_t local_slot_size{sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::wasm_stack_top_i32_i64_f32_f64_u)};
        static_assert(local_slot_size == 8uz);

        struct compiled_defined_func_info
        {
            ::std::size_t module_id{};
            ::std::size_t function_index{};
            runtime_local_func_storage_t const* runtime_func{};
            compiled_local_func_t const* compiled_func{};
            ::std::size_t param_bytes{};
            ::std::size_t result_bytes{};
        };

        struct compiled_module_record
        {
            ::uwvm2::utils::container::u8string_view module_name{};
            runtime_module_storage_t const* runtime_module{};
            compiled_module_t compiled{};
        };

        inline ::uwvm2::utils::container::vector<compiled_module_record> g_modules{};
        inline ::uwvm2::utils::container::unordered_flat_map<::uwvm2::utils::container::u8string_view, ::std::size_t> g_module_name_to_id{};
        // Full-compile: keep the hot local-call path O(1) by indexing local funcs with vectors (not hash maps).
        inline ::uwvm2::utils::container::vector<::uwvm2::utils::container::vector<compiled_defined_func_info>> g_defined_func_cache{};

        struct defined_func_ptr_range
        {
            ::std::uintptr_t begin{};
            ::std::uintptr_t end{};
            ::std::size_t module_id{};
        };

        // For indirect calls / import-alias resolution that only has `local_defined_function_storage_t*`,
        // map pointer-address to {module_id, local_index} via a sorted range table.
        inline ::uwvm2::utils::container::vector<defined_func_ptr_range> g_defined_func_ptr_ranges{};
        inline bool g_bridges_initialized{};
        inline bool g_compiled_all{};

        struct call_stack_frame
        {
            ::std::size_t module_id{};
            ::std::size_t function_index{};
        };

        inline ::uwvm2::utils::container::vector<call_stack_frame> g_call_stack{};

        [[nodiscard]] inline compiled_defined_func_info const* find_defined_func_info(runtime_local_func_storage_t const* f) noexcept
        {
            if(f == nullptr) [[unlikely]] { return nullptr; }
            if(g_defined_func_ptr_ranges.empty()) [[unlikely]] { return nullptr; }

            ::std::uintptr_t const addr{reinterpret_cast<::std::uintptr_t>(f)};
            auto const it{::std::upper_bound(g_defined_func_ptr_ranges.begin(),
                                             g_defined_func_ptr_ranges.end(),
                                             addr,
                                             [](auto const a, defined_func_ptr_range const& r) constexpr noexcept { return a < r.begin; })};
            if(it == g_defined_func_ptr_ranges.begin()) [[unlikely]] { return nullptr; }

            auto const& r{*(it - 1)};
            if(addr < r.begin || addr >= r.end) [[unlikely]] { return nullptr; }

            constexpr ::std::size_t elem_size{sizeof(runtime_local_func_storage_t)};
            static_assert(elem_size != 0uz);

            ::std::uintptr_t const off_bytes{addr - r.begin};
            if((off_bytes % elem_size) != 0u) [[unlikely]] { return nullptr; }
            auto const local_idx{static_cast<::std::size_t>(off_bytes / elem_size)};

            if(r.module_id >= g_defined_func_cache.size()) [[unlikely]] { return nullptr; }
            auto const& mod_cache{g_defined_func_cache.index_unchecked(r.module_id)};
            if(local_idx >= mod_cache.size()) [[unlikely]] { return nullptr; }

            auto const* const info{::std::addressof(mod_cache.index_unchecked(local_idx))};
            if(info->runtime_func != f) [[unlikely]] { return nullptr; }
            return info;
        }

        struct call_stack_guard
        {
            bool active{};

            inline constexpr explicit call_stack_guard(::std::size_t module_id, ::std::size_t function_index) noexcept
            {
                g_call_stack.push_back(call_stack_frame{module_id, function_index});
                active = true;
            }

            call_stack_guard(call_stack_guard const&) = delete;
            call_stack_guard& operator= (call_stack_guard const&) = delete;

            inline constexpr ~call_stack_guard()
            {
                if(active && !g_call_stack.empty()) { g_call_stack.pop_back(); }
            }
        };

        enum class trap_kind : unsigned
        {
            // opfunc
            unreachable,
            invalid_conversion_to_integer,
            integer_divide_by_zero,
            integer_overflow,
            // call_indirect (wasm1.0 MVP)
            call_indirect_table_out_of_bounds,
            call_indirect_null_element,
            call_indirect_type_mismatch,
            // uncatched int error (wasm 3.0, exception)
            uncatched_int_tag

        };

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view trap_kind_name(trap_kind k) noexcept
        {
            switch(k)
            {
                case trap_kind::unreachable:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"catch unreachable"};
                }
                case trap_kind::invalid_conversion_to_integer:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"invalid conversion to integer"};
                }
                case trap_kind::integer_divide_by_zero:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"integer divide by zero"};
                }
                case trap_kind::integer_overflow:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"integer overflow"};
                }
                case trap_kind::call_indirect_table_out_of_bounds:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"call_indirect: table index out of bounds"};
                }
                case trap_kind::call_indirect_null_element:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"call_indirect: uninitialized element"};
                }
                case trap_kind::call_indirect_type_mismatch:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"call_indirect: signature mismatch"};
                }
                case trap_kind::uncatched_int_tag:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"tag: uncatched wasm exception"};
                }
                [[unlikely]] default:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"unknown trap"};
                }
            }
        }

        [[nodiscard]] inline ::uwvm2::utils::container::u8string_view resolve_module_display_name(::uwvm2::utils::container::u8string_view module_name) noexcept
        {
            auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
            if(it == ::uwvm2::uwvm::wasm::storage::all_module.end()) { return module_name; }

            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            auto const& am{it->second};
            if(am.type != module_type_t::exec_wasm && am.type != module_type_t::preloaded_wasm) { return module_name; }

            auto const wf{am.module_storage_ptr.wf};
            if(wf == nullptr) { return module_name; }

            auto const n{wf->wasm_custom_name.module_name};
            if(n.empty()) { return module_name; }
            return n;
        }

        [[nodiscard]] inline ::uwvm2::utils::container::u8string_view resolve_func_display_name(::uwvm2::utils::container::u8string_view module_name,
                                                                                                ::std::size_t function_index) noexcept
        {
            auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
            if(it == ::uwvm2::uwvm::wasm::storage::all_module.end()) { return {}; }

            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            auto const& am{it->second};
            if(am.type != module_type_t::exec_wasm && am.type != module_type_t::preloaded_wasm) { return {}; }

            auto const wf{am.module_storage_ptr.wf};
            if(wf == nullptr) { return {}; }

            using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
            constexpr auto wasm_u32_max{static_cast<::std::size_t>(::std::numeric_limits<wasm_u32>::max())};
            if(function_index > wasm_u32_max) { return {}; }

            auto const key{static_cast<wasm_u32>(function_index)};
            auto const fn_it{wf->wasm_custom_name.function_name.find(key)};
            if(fn_it == wf->wasm_custom_name.function_name.end()) { return {}; }
            return fn_it->second;
        }

        inline constexpr void dump_call_stack_for_trap() noexcept
        {
            // No copies will be made here.
            auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
            // Add raii locks while unlocking operations
            ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
            // No copies will be made here.
            auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Call stack:\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            auto const n{g_call_stack.size()};
            for(::std::size_t i{}; i != n; ++i)
            {
                auto const& fr{g_call_stack.index_unchecked(n - 1uz - i)};
                if(fr.module_id >= g_modules.size()) { continue; }

                auto const& mod_rec{g_modules.index_unchecked(fr.module_id)};
                auto const mod_name{resolve_module_display_name(mod_rec.module_name)};
                auto const fn_name{resolve_func_display_name(mod_rec.module_name, fr.function_index)};

                ::fast_io::io::perr(u8log_output_ul,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"#",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    i,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8" module=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    mod_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8" func_idx=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    fr.function_index);

                if(!fn_name.empty())
                {
                    ::fast_io::io::perr(u8log_output_ul,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8" func_name=\"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        fn_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\"");
                }

                ::fast_io::io::perrln(u8log_output_ul, ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }

            ::fast_io::io::perrln(u8log_output_ul);
        }

        [[noreturn]] inline constexpr void trap_fatal(trap_kind k) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Runtime crash (",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                trap_kind_name(k),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8")\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            dump_call_stack_for_trap();

            ::fast_io::fast_terminate();
        }

#ifdef UWVM_CPP_EXCEPTIONS
        [[noreturn]] inline void print_and_terminate_compile_validation_error(::uwvm2::utils::container::u8string_view module_name,
                                                                              ::uwvm2::validation::error::code_validation_error_impl const& v_err) noexcept
        {
            // Try to print detailed validator diagnostics (same format as `uwvm/runtime/validator/validate.h`).
            auto const fallback_and_terminate{
                [&]() noexcept
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                        u8"[error] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Validation error during compilation (module=\"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        module_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\").\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }};

            auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
            if(it == ::uwvm2::uwvm::wasm::storage::all_module.end()) [[unlikely]] { fallback_and_terminate(); }

            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            auto const& am{it->second};
            if(am.type != module_type_t::exec_wasm && am.type != module_type_t::preloaded_wasm) [[unlikely]] { fallback_and_terminate(); }

            auto const wf{am.module_storage_ptr.wf};
            if(wf == nullptr || wf->binfmt_ver != 1u) [[unlikely]] { fallback_and_terminate(); }

            auto const file_name{wf->file_name};
            auto const& module_storage{wf->wasm_module_storage.wasm_binfmt_ver1_storage};

            auto const module_begin{module_storage.module_span.module_begin};
            auto const module_end{module_storage.module_span.module_end};
            if(module_begin == nullptr || module_end == nullptr) [[unlikely]] { fallback_and_terminate(); }

            ::uwvm2::uwvm::utils::memory::print_memory const memory_printer{module_begin, v_err.err_curr, module_end};

            ::uwvm2::validation::error::error_output_t errout{};
            errout.module_begin = module_begin;
            errout.err = v_err;
            errout.flag.enable_ansi = static_cast<::std::uint_least8_t>(::uwvm2::uwvm::utils::ansies::put_color);
# if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
            errout.flag.win32_use_text_attr = static_cast<::std::uint_least8_t>(!::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b);
# endif

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                // 1
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Validation error in WebAssembly Code (module=\"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                module_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\", file=\"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                file_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\").\n",
                                // 2
                                errout,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\n"
                                // 3
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Validator Memory Indication: ",
                                memory_printer,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                u8"\n\n");

            ::fast_io::fast_terminate();
        }
#endif

        enum class valtype_kind : unsigned
        {
            wasm_enum,
            raw_u8
        };

        struct valtype_vec_view
        {
            valtype_kind kind{};
            void const* data{};
            ::std::size_t size{};

            inline constexpr ::std::uint_least8_t at(::std::size_t i) const noexcept
            {
                if(i >= size) [[unlikely]] { return 0u; }

                if(kind == valtype_kind::raw_u8)
                {
                    auto const p{static_cast<::std::uint_least8_t const*>(data)};
                    if(p == nullptr) [[unlikely]] { return 0u; }
                    return p[i];
                }
                else
                {
                    using wasm_value_type_const_ptr_t_may_alias UWVM_GNU_MAY_ALIAS = wasm_value_type const*;

                    auto const p{static_cast<wasm_value_type_const_ptr_t_may_alias>(data)};
                    if(p == nullptr) [[unlikely]] { return 0u; }
                    return static_cast<::std::uint_least8_t>(p[i]);
                }
            }
        };

        struct func_sig_view
        {
            valtype_vec_view params{};
            valtype_vec_view results{};
        };

        // Precomputed import dispatch table for O(1) imported calls.
        // This is built once before execution (after uwvm runtime initialization + compilation).
        struct cached_import_target
        {
            enum class kind : unsigned
            {
                defined,
                local_imported,
                dl,
                weak_symbol
            };

            kind k{};
            call_stack_frame frame{};
            func_sig_view sig{};
            ::std::size_t param_bytes{};
            ::std::size_t result_bytes{};

            union
            {
                struct
                {
                    runtime_local_func_storage_t const* runtime_func{};
                    compiled_local_func_t const* compiled_func{};
                } defined;

                runtime_imported_func_storage_t::local_imported_target_t local_imported;

                capi_function_t const* capi_ptr;
            } u{};
        };

        inline ::uwvm2::utils::container::vector<::uwvm2::utils::container::vector<cached_import_target>> g_import_call_cache{};

        [[nodiscard]] inline constexpr ::std::size_t valtype_size(::std::uint_least8_t code) noexcept
        {
            switch(static_cast<wasm_value_type>(code))
            {
                case wasm_value_type::i32:
                {
                    return 4uz;
                }
                case wasm_value_type::i64:
                {
                    return 8uz;
                }
                case wasm_value_type::f32:
                {
                    return 4uz;
                }
                case wasm_value_type::f64:
                {
                    return 8uz;
                }
                default:
                {
                    if(code == static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)) { return 16uz; }
                    return 0uz;
                }
            }
        }

        [[nodiscard]] inline constexpr bool func_sig_equal(func_sig_view const& a, func_sig_view const& b) noexcept
        {
            if(a.params.size != b.params.size || a.results.size != b.results.size) { return false; }
            for(::std::size_t i{}; i != a.params.size; ++i)
            {
                if(a.params.at(i) != b.params.at(i)) { return false; }
            }
            for(::std::size_t i{}; i != a.results.size; ++i)
            {
                if(a.results.at(i) != b.results.at(i)) { return false; }
            }
            return true;
        }

        [[nodiscard]] inline constexpr ::std::size_t total_abi_bytes(valtype_vec_view const& v) noexcept
        {
            ::std::size_t total{};
            for(::std::size_t i{}; i != v.size; ++i)
            {
                auto const sz{valtype_size(v.at(i))};
                if(sz == 0uz) [[unlikely]] { return 0uz; }
                total += sz;
            }
            return total;
        }

        [[nodiscard]] inline constexpr func_sig_view func_sig_from_defined(runtime_local_func_storage_t const* f) noexcept
        {
            auto const ft{f->function_type_ptr};
            return {
                {valtype_kind::wasm_enum, ft->parameter.begin, static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin)},
                {valtype_kind::wasm_enum, ft->result.begin,    static_cast<::std::size_t>(ft->result.end - ft->result.begin)      }
            };
        }

        [[nodiscard]] inline func_sig_view func_sig_from_local_imported(local_imported_t const* m, ::std::size_t idx) noexcept
        {
            auto const info{m->get_function_information_from_index(idx)};
            if(!info.successed) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const& ft{info.function_type};
            return {
                {valtype_kind::wasm_enum, ft.parameter.begin, static_cast<::std::size_t>(ft.parameter.end - ft.parameter.begin)},
                {valtype_kind::wasm_enum, ft.result.begin,    static_cast<::std::size_t>(ft.result.end - ft.result.begin)      }
            };
        }

        [[nodiscard]] inline constexpr func_sig_view func_sig_from_capi(capi_function_t const* f) noexcept
        {
            return {
                {valtype_kind::raw_u8, f->para_type_vec_begin, f->para_type_vec_size},
                {valtype_kind::raw_u8, f->res_type_vec_begin,  f->res_type_vec_size }
            };
        }

        struct resolved_func
        {
            enum class kind : unsigned
            {
                defined,
                local_imported,
                dl,
                weak_symbol
            };

            kind k{};

            union
            {
                runtime_local_func_storage_t const* defined_ptr;
                runtime_imported_func_storage_t::local_imported_target_t local_imported;
                capi_function_t const* capi_ptr;
            } u{};
        };

        // Import resolution is performed by uwvm runtime initializer.
        // This runtime only consumes the initialized link_kind/target fields and never performs on-demand linking.
        [[nodiscard]] inline runtime_imported_func_storage_t const* resolve_import_leaf_assuming_initialized(runtime_imported_func_storage_t const* f) noexcept
        {
            using link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
            auto curr{f};
            for(::std::size_t steps{};; ++steps)
            {
                // The initializer guarantees import-alias chains are finite and acyclic. This guard is for internal bugs.
                if(steps > 8192uz) [[unlikely]] { return nullptr; }
                if(curr == nullptr) [[unlikely]] { return nullptr; }

                switch(curr->link_kind)
                {
                    case link_kind::imported:
                    {
                        curr = curr->target.imported_ptr;
                        continue;
                    }
                    case link_kind::defined: [[fallthrough]];
                    case link_kind::local_imported:
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                    case link_kind::dl:
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                    case link_kind::weak_symbol:
#endif
                    {
                        return curr;
                    }
                    case link_kind::unresolved:
                    {
                        return nullptr;
                    }
                    default:
                    {
                        return nullptr;
                    }
                }
            }
        }

        [[nodiscard]] inline resolved_func resolve_func_from_import_assuming_initialized(runtime_imported_func_storage_t const* f) noexcept
        {
            using link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
            auto const leaf{resolve_import_leaf_assuming_initialized(f)};
            if(leaf == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            switch(leaf->link_kind)
            {
                case link_kind::defined:
                {
                    resolved_func r{};
                    r.k = resolved_func::kind::defined;
                    r.u.defined_ptr = leaf->target.defined_ptr;
                    return r;
                }
                case link_kind::local_imported:
                {
                    resolved_func r{};
                    r.k = resolved_func::kind::local_imported;
                    r.u.local_imported = leaf->target.local_imported;
                    return r;
                }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                case link_kind::dl:
                {
                    resolved_func r{};
                    r.k = resolved_func::kind::dl;
                    r.u.capi_ptr = leaf->target.dl_ptr;
                    return r;
                }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                case link_kind::weak_symbol:
                {
                    resolved_func r{};
                    r.k = resolved_func::kind::weak_symbol;
                    r.u.capi_ptr = leaf->target.weak_symbol_ptr;
                    return r;
                }
#endif
                case link_kind::imported: [[fallthrough]];
                default:
                {
                    [[unlikely]] ::fast_io::fast_terminate();
                }
            }
        }

        using opfunc_byref_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<::std::byte const*, ::std::byte*, ::std::byte*>;

        using byte_allocator = ::fast_io::native_thread_local_allocator;

        struct heap_buf_guard
        {
            void* ptr{};

            inline constexpr heap_buf_guard() noexcept = default;

            heap_buf_guard(heap_buf_guard const&) = delete;
            heap_buf_guard& operator= (heap_buf_guard const&) = delete;

            inline constexpr ~heap_buf_guard()
            {
                if(ptr) { byte_allocator::deallocate(ptr); }
            }
        };

#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        inline ::uwvm2::object::memory::linear::native_memory_t const* resolve_memory0_ptr(runtime_module_storage_t const& rt) noexcept
        {
            using imported_memory_storage_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t;
            using memory_link_kind = imported_memory_storage_t::imported_memory_link_kind;

            auto const import_n{rt.imported_memory_vec_storage.size()};
            if(import_n == 0uz)
            {
                if(rt.local_defined_memory_vec_storage.empty()) { return nullptr; }
                return ::std::addressof(rt.local_defined_memory_vec_storage.index_unchecked(0uz).memory);
            }

            constexpr ::std::size_t kMaxChain{4096uz};
            auto const* curr{::std::addressof(rt.imported_memory_vec_storage.index_unchecked(0uz))};
            for(::std::size_t steps{}; steps != kMaxChain; ++steps)
            {
                if(curr == nullptr) { return nullptr; }

                switch(curr->link_kind)
                {
                    case memory_link_kind::imported:
                    {
                        curr = curr->target.imported_ptr;
                        break;
                    }
                    case memory_link_kind::defined:
                    {
                        auto const* const def{curr->target.defined_ptr};
                        if(def == nullptr) { return nullptr; }
                        return ::std::addressof(def->memory);
                    }
                    case memory_link_kind::local_imported: [[fallthrough]];
                    case memory_link_kind::unresolved: [[fallthrough]];
                    default:
                    {
                        return nullptr;
                    }
                }
            }
            return nullptr;
        }

        inline void bind_default_wasip1_memory(runtime_module_storage_t const& rt) noexcept
        {
            // Best-effort binding: WASI functions will trap/return errors if a caller without memory[0] invokes them.
            // Always overwrite the pointer to avoid using a stale memory from a previous run.
            auto const* const mem0{resolve_memory0_ptr(rt)};
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.wasip1_memory =
                const_cast<::uwvm2::object::memory::linear::native_memory_t*>(mem0);
        }
#endif

        // IMPORTANT:
        // Do NOT wrap `alloca` in a helper function that returns the pointer: `alloca` is stack-frame bound,
        // so allocating in a callee and returning the pointer is a dangling pointer unless the compiler inlines it.
        // Use this macro so the `alloca` happens in the caller's stack frame.
#if UWVM_HAS_BUILTIN(__builtin_alloca)
# define UWVM_ALLOCA_BYTES(uwvm_n) __builtin_alloca(uwvm_n)
#elif defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__) && !defined(__CYGWIN__)
# define UWVM_ALLOCA_BYTES(uwvm_n) _alloca(uwvm_n)
#else
# define UWVM_ALLOCA_BYTES(uwvm_n) alloca(uwvm_n)
#endif

#define UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(dst_ptr, bytes_expr, guard_obj)                                                                          \
    {                                                                                                                                                          \
        ::std::size_t uwvm_n{bytes_expr};                                                                                                                      \
        if(uwvm_n == 0uz) [[unlikely]] { uwvm_n = 1uz; }                                                                                                       \
        if(uwvm_n < 1024uz)                                                                                                                                    \
        {                                                                                                                                                      \
            void* const uwvm_p{UWVM_ALLOCA_BYTES(uwvm_n)};                                                                                                     \
            ::std::memset(uwvm_p, 0, uwvm_n);                                                                                                                  \
            dst_ptr = static_cast<::std::byte*>(uwvm_p);                                                                                                       \
        }                                                                                                                                                      \
        else                                                                                                                                                   \
        {                                                                                                                                                      \
            void* const uwvm_p{byte_allocator::allocate_zero(uwvm_n)};                                                                                         \
            guard_obj.ptr = uwvm_p;                                                                                                                            \
            dst_ptr = static_cast<::std::byte*>(uwvm_p);                                                                                                       \
        }                                                                                                                                                      \
    }

        template <::std::size_t I, ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
        using uwvmint_interp_arg_t = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::interpreter_tuple_arg_t<I, CompileOption>;

        template <::std::size_t I, ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
        UWVM_ALWAYS_INLINE inline constexpr uwvmint_interp_arg_t<I, CompileOption>
            uwvmint_init_interp_arg(::std::byte const* ip, ::std::byte* stack_top, ::std::byte* local_base) noexcept
        {
            if constexpr(I == 0uz) { return ip; }
            else if constexpr(I == 1uz) { return stack_top; }
            else if constexpr(I == 2uz) { return local_base; }
            else
            {
                return uwvmint_interp_arg_t<I, CompileOption>{};
            }
        }

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption, ::std::size_t... Is>
        UWVM_ALWAYS_INLINE inline void execute_compiled_defined_tailcall_impl(::std::index_sequence<Is...>,
                                                                              ::std::byte const* ip,
                                                                              ::std::byte* stack_top,
                                                                              ::std::byte* local_base) noexcept
        {
            using opfunc_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<uwvmint_interp_arg_t<Is, CompileOption>...>;
            opfunc_t fn;  // no init
            ::std::memcpy(::std::addressof(fn), ip, sizeof(fn));
            fn(uwvmint_init_interp_arg<Is, CompileOption>(ip, stack_top, local_base)...);
        }

        inline consteval ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t get_curr_target_tranopt() noexcept
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t res{};

#if !(defined(__pdp11) || (defined(__MSDOS__) || defined(__DJGPP__)) || (defined(__wasm__) && !defined(__wasm_tail_call__)))
            res.is_tail_call = true;
#endif

#if defined(__ARM_PCS_AAPCS64) || defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || defined(__arm64ec__) || defined(_M_ARM64EC)
            // aarch64: AAPCS64 (x0-x7 integer args, v0-v7 fp/simd args)
            // 3 fixed args: (ip, operand_stack_top, local_base) => occupy x0-x2
            // Use remaining integer args (x3-x7) for i32/i64 stack-top caching, and fp/simd args (v0-v7) for f32/f64/v128.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 16uz;
#elif defined(__arm__) || defined(_M_ARM)
            // ARM32: AAPCS/EABI (r0-r3 integer args; hard-float variants may also use VFP regs).
            // After the 3 fixed interpreter args, there is at most 1 remaining core argument register (r3).
            // A full scalar+fp stack-top cache would largely spill to memory on most ABIs, negating the benefit.
            // Leave stack-top caching disabled here (SIZE_MAX/SIZE_MAX).
#elif ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                                     \
    (!defined(_WIN32) || (defined(__GNUC__) || defined(__clang__)))
            // x86_64: sysv abi
            // x86_64: sysv abi in ms abi
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 6uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 6uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 14uz;
#elif defined(_WIN32) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                  \
    !(defined(__GNUC__) || defined(__clang__))
            // x86_64: Windows x64 (MS ABI) (rcx/rdx/r8/r9, xmm0-xmm3)
            // This ABI provides only 4 register argument slots total. After the 3 fixed interpreter args, only 1 slot remains (r9/xmm3).
            // Empirically, enabling a 1-slot scalar4-merged stack-top cache tends to regress overall performance
            // (register pressure + spills), so keep stack-top caching disabled by default here.
#if 0
            /// @deprecated MS ABI "1-slot" stack-top cache experiment.
            ///             Often regresses performance due to spills/register shuffling. Kept for reference.
            ///             Keep v128 caching off by default.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 4uz;
#endif
#elif defined(__i386__) || defined(_M_IX86)
            // i386: (usually) only 2 register argument slots under fastcall (ecx/edx), and we already need 3 fixed args.
            // Leave stack-top caching disabled here (SIZE_MAX/SIZE_MAX).
#elif defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || defined(_ARCH_PPC64)
            // powerpc64: SysV ELF (r3-r10 integer args, VSX for fp/simd)
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 16uz;
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
            // powerpc32: AIX/SysV/EABI variants differ in i64/f64 passing (often reg-pairs) and hard-float rules.
            // Keep stack-top caching disabled by default for correctness across ABIs.
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
# if defined(__riscv_float_abi_soft) || defined(__riscv_float_abi_single)
            // riscv64: soft-float / single-float (f64 may not be passed in fp regs). Use a scalar4-merged ring in the integer register file.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
# else
            // riscv64: psABI (a0-a7 integer args, fa0-fa7 fp args). Keep v128 caching off by default:
            // `wasm_v128` argument passing is not consistently vector-reg based across toolchains/ABIs.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 16uz;
# endif
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 32)
            // riscv32: i64/f64 are passed in register pairs and the effective register slots are tight.
            // Leave stack-top caching disabled here (SIZE_MAX/SIZE_MAX).
#elif defined(__loongarch__) && defined(__loongarch64)
# if defined(__loongarch_soft_float) || defined(__loongarch_single_float)
            // loongarch64: soft-float / single-float (f64 may not be passed in fp regs). Use a scalar4-merged ring.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
# else
            // loongarch64: LP64D (a0-a7 integer args, fa0-fa7 fp args). Keep v128 caching off by default:
            // `wasm_v128` argument passing may be lowered to GPR pairs/stack depending on ABI + vector extension.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 16uz;
# endif
#elif defined(__loongarch__)
            // loongarch32: i64/f64 passing uses pairs / stack depending on ABI; keep caching disabled by default.
#elif defined(__mips__) || defined(__MIPS__) || defined(_MIPS_ARCH)
            // MIPS ABIs are slot-based: fp args are only register-passed while they remain within the ABI's argument slots.
            // We conservatively target the 8-slot N32/N64 ABIs; O32 has only 4 slots and cannot satisfy Wasm1's minimum ring sizes without heavy spilling.
# if defined(__mips_n32) || defined(__mips_n64)
#  if defined(__mips_soft_float)
            // N32/N64 soft-float: use a scalar4-merged ring in the integer slots (fits in the 8 arg slots).
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#  else
            // N32/N64 hard-float: keep total args within 8 slots so fp values still use FPRs.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 6uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 6uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#  endif
# endif
#elif defined(__s390x__)
            // s390x: Linux ABI (r2-r6 integer args, f0/f2/... fp args). Keep v128 caching off by default:
            // 16-byte vectors can be passed indirectly by pointer.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 6uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 6uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#elif defined(__s390__) || defined(__SYSC_ZARCH__)
            // s390 (31-bit) / z/Architecture (non-s390x toolchains): i64/f64 passing is ABI-sensitive (often reg pairs).
            // Leave stack-top caching disabled by default.
#elif defined(__sparc__)
            // SPARC: multiple ABIs (v8/v9, 32/64) with different fp arg rules. Leave caching disabled by default.
#elif defined(__IA64__) || defined(_M_IA64) || defined(__ia64__) || defined(__itanium__)
            // IA-64: Itanium ABI is rare today; keep caching disabled by default to avoid ABI mismatches.
#elif defined(__alpha__)
            // Alpha: uncommon; leave caching disabled by default.
#elif defined(__m68k__) || defined(__mc68000__)
            // m68k: uncommon; leave caching disabled by default.
#elif defined(__HPPA__)
            // HPPA: uncommon; leave caching disabled by default.
#elif defined(__e2k__)
            // E2K: uncommon; leave caching disabled by default.
#elif defined(__XTENSA__)
            // Xtensa: embedded; leave caching disabled by default.
#elif defined(__BFIN__)
            // Blackfin: embedded; leave caching disabled by default.
#elif defined(__convex__)
            // Convex: historical; leave caching disabled by default.
#elif defined(__370__) || defined(__THW_370__)
            // System/370: historical; leave caching disabled by default.
#elif defined(__pdp10) || defined(__pdp7) || defined(__pdp11)
            // PDP family: historical; leave caching disabled by default.
#elif defined(__THW_RS6000) || defined(_IBMR2) || defined(_POWER) || defined(_ARCH_PWR) || defined(_ARCH_PWR2)
            // RS/6000: historical; leave caching disabled by default.
#elif defined(__CUDA_ARCH__)
            // PTX (CUDA device code): stack-top caching is not applicable here.
#elif defined(__sh__)
            // SuperH: embedded; leave caching disabled by default.
#elif defined(__AVR__)
            // AVR: embedded; leave caching disabled by default.
#elif defined(__wasm__)
            // UWVM itself may be built as wasm32-wasi; stack-top caching via native ABI registers is not applicable here.
#endif

            return res;
        }

        inline void execute_compiled_defined([[maybe_unused]] runtime_local_func_storage_t const* runtime_func,
                                             compiled_local_func_t const* compiled_func,
                                             ::std::size_t param_bytes,
                                             ::std::size_t result_bytes,
                                             ::std::byte** caller_stack_top_ptr) noexcept
        {
            auto caller_stack_top{*caller_stack_top_ptr};
            auto const caller_args_begin{caller_stack_top - param_bytes};
            // Pop params from the caller stack first (so nested calls can't see them).
            *caller_stack_top_ptr = caller_args_begin;

            // Allocate locals as a packed byte buffer (i32/f32=4, i64/f64=8, plus the internal temp local).
            heap_buf_guard locals_guard{};
            ::std::byte* local_base{};
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(local_base, compiled_func->local_bytes_max, locals_guard);

            if(param_bytes > compiled_func->local_bytes_max) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#else
                ::fast_io::fast_terminate();
#endif
            }

            if(param_bytes != 0uz) { ::std::memcpy(local_base, caller_args_begin, param_bytes); }

            // Allocate operand stack with the exact max byte size computed by the compiler (byte-packed: i32/f32=4, i64/f64=8).
            heap_buf_guard operand_guard{};
            auto const stack_cap_raw{compiled_func->operand_stack_byte_max};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(stack_cap_raw == 0uz && compiled_func->operand_stack_max != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
#endif
            if(stack_cap_raw < result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

            ::std::byte* operand_base{};
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(operand_base, stack_cap_raw, operand_guard);

            ::std::byte const* ip{compiled_func->op.operands.data()};
            ::std::byte* stack_top{operand_base};

            constexpr auto curr_target_tranopt{get_curr_target_tranopt()};

            if constexpr(curr_target_tranopt.is_tail_call)
            {
                constexpr ::std::size_t tuple_size{
                    ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::interpreter_tuple_size<curr_target_tranopt>()};
                execute_compiled_defined_tailcall_impl<curr_target_tranopt>(::std::make_index_sequence<tuple_size>{}, ip, stack_top, local_base);
            }
            else
            {
                while(ip != nullptr) [[likely]]
                {
                    opfunc_byref_t fn;  // no init
                    ::std::memcpy(::std::addressof(fn), ip, sizeof(fn));
                    fn(ip, stack_top, local_base);
                }

                auto const actual_result_bytes{static_cast<::std::size_t>(stack_top - operand_base)};
                if(actual_result_bytes != result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
            }

            // Append results back to caller stack.
            ::std::memcpy(*caller_stack_top_ptr, operand_base, result_bytes);
            *caller_stack_top_ptr += result_bytes;
        }

        inline void invoke_local_imported(runtime_imported_func_storage_t::local_imported_target_t const& tgt,
                                          ::std::size_t para_bytes,
                                          ::std::size_t res_bytes,
                                          ::std::byte** caller_stack_top_ptr) noexcept
        {
            local_imported_t const* m{tgt.module_ptr};
            if(m == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const caller_stack_top{*caller_stack_top_ptr};
            auto const caller_args_begin{caller_stack_top - para_bytes};
            *caller_stack_top_ptr = caller_args_begin;

            heap_buf_guard res_guard{};
            heap_buf_guard par_guard{};
            ::std::byte* resbuf{};
            ::std::byte* parbuf{};
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(resbuf, res_bytes, res_guard);
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(parbuf, para_bytes, par_guard);
            if(para_bytes != 0uz) { ::std::memcpy(parbuf, caller_args_begin, para_bytes); }

            m->call_func_index(tgt.index, resbuf, parbuf);

            if(res_bytes != 0uz) { ::std::memcpy(*caller_stack_top_ptr, resbuf, res_bytes); }
            *caller_stack_top_ptr += res_bytes;
        }

        inline void invoke_capi(capi_function_t const* f, ::std::size_t para_bytes, ::std::size_t res_bytes, ::std::byte** caller_stack_top_ptr) noexcept
        {
            if(f == nullptr || f->func_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const caller_stack_top{*caller_stack_top_ptr};
            auto const caller_args_begin{caller_stack_top - para_bytes};
            *caller_stack_top_ptr = caller_args_begin;

            heap_buf_guard res_guard{};
            heap_buf_guard par_guard{};
            ::std::byte* resbuf{};
            ::std::byte* parbuf{};
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(resbuf, res_bytes, res_guard);
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(parbuf, para_bytes, par_guard);
            if(para_bytes != 0uz) { ::std::memcpy(parbuf, caller_args_begin, para_bytes); }

            f->func_ptr(resbuf, parbuf);

            if(res_bytes != 0uz) { ::std::memcpy(*caller_stack_top_ptr, resbuf, res_bytes); }
            *caller_stack_top_ptr += res_bytes;
        }

        inline void invoke_resolved(resolved_func const& rf, ::std::byte** caller_stack_top_ptr) noexcept
        {
            switch(rf.k)
            {
                case resolved_func::kind::defined:
                {
                    auto const* const info{find_defined_func_info(rf.u.defined_ptr)};
                    if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    execute_compiled_defined(info->runtime_func, info->compiled_func, info->param_bytes, info->result_bytes, caller_stack_top_ptr);
                    return;
                }
                case resolved_func::kind::local_imported:
                {
                    local_imported_t const* m{rf.u.local_imported.module_ptr};
                    auto const sig{func_sig_from_local_imported(m, rf.u.local_imported.index)};
                    if(sig.params.data == nullptr && sig.params.size != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }

                    auto const para_bytes{total_abi_bytes(sig.params)};
                    auto const res_bytes{total_abi_bytes(sig.results)};
                    if((para_bytes == 0uz && sig.params.size != 0uz) || (res_bytes == 0uz && sig.results.size != 0uz)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                    invoke_local_imported(rf.u.local_imported, para_bytes, res_bytes, caller_stack_top_ptr);
                    return;
                }
                case resolved_func::kind::dl:
                case resolved_func::kind::weak_symbol:
                {
                    capi_function_t const* f{rf.u.capi_ptr};
                    auto const sig{func_sig_from_capi(f)};

                    auto const para_bytes{total_abi_bytes(sig.params)};
                    auto const res_bytes{total_abi_bytes(sig.results)};
                    if((para_bytes == 0uz && sig.params.size != 0uz) || (res_bytes == 0uz && sig.results.size != 0uz)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                    invoke_capi(f, para_bytes, res_bytes, caller_stack_top_ptr);
                    return;
                }
                [[unlikely]] default:
                {
                    ::fast_io::fast_terminate();
                }
            }
        }

        [[nodiscard]] inline constexpr runtime_table_storage_t const* resolve_table(runtime_module_storage_t const& module, ::std::size_t table_index) noexcept
        {
            auto const import_n{module.imported_table_vec_storage.size()};
            if(table_index < import_n)
            {
                auto t{::std::addressof(module.imported_table_vec_storage.index_unchecked(table_index))};
                for(;;)
                {
                    if(t == nullptr) [[unlikely]] { return nullptr; }
                    using lk = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                    switch(t->link_kind)
                    {
                        case lk::defined:
                        {
                            return t->target.defined_ptr;
                        }
                        case lk::imported:
                        {
                            t = t->target.imported_ptr;
                            continue;
                        }
                        case lk::unresolved: [[fallthrough]];
                        default:
                        {
                            return nullptr;
                        }
                    }
                }
            }

            auto const local_index{table_index - import_n};
            if(local_index >= module.local_defined_table_vec_storage.size()) [[unlikely]] { return nullptr; }
            return ::std::addressof(module.local_defined_table_vec_storage.index_unchecked(local_index));
        }

        [[nodiscard]] inline constexpr func_sig_view
            expected_sig_from_type_index(runtime_module_storage_t const& module, ::std::size_t type_index, bool& ok) noexcept
        {
            ok = false;
            auto const begin{module.type_section_storage.type_section_begin};
            auto const end{module.type_section_storage.type_section_end};
            if(begin == nullptr || end == nullptr) [[unlikely]] { return {}; }
            auto const total{static_cast<::std::size_t>(end - begin)};
            if(type_index >= total) [[unlikely]] { return {}; }

            auto const ft{begin + type_index};
            ok = true;
            return {
                {valtype_kind::wasm_enum, ft->parameter.begin, static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin)},
                {valtype_kind::wasm_enum, ft->result.begin,    static_cast<::std::size_t>(ft->result.end - ft->result.begin)      }
            };
        }

        inline void ensure_bridges_initialized() noexcept;
        inline void compile_all_modules_if_needed() noexcept;

        // ==========
        // Bridges
        // ==========

        inline void unreachable_trap() noexcept { trap_fatal(trap_kind::unreachable); }

        inline void trap_invalid_conversion_to_integer() noexcept { trap_fatal(trap_kind::invalid_conversion_to_integer); }

        inline void trap_integer_divide_by_zero() noexcept { trap_fatal(trap_kind::integer_divide_by_zero); }

        inline void trap_integer_overflow() noexcept { trap_fatal(trap_kind::integer_overflow); }

        inline void call_bridge(::std::size_t wasm_module_id, ::std::size_t func_index, ::std::byte** stack_top_ptr) UWVM_THROWS
        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(!g_compiled_all) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

            if(wasm_module_id >= g_modules.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& module_rec{g_modules.index_unchecked(wasm_module_id)};
            auto const& module{*module_rec.runtime_module};

            auto const import_n{module.imported_function_vec_storage.size()};
            auto const local_n{module.local_defined_function_vec_storage.size()};
            if(func_index >= import_n + local_n) [[unlikely]] { ::fast_io::fast_terminate(); }

            if(func_index < import_n)
            {
                if(wasm_module_id >= g_import_call_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const& cache{g_import_call_cache.index_unchecked(wasm_module_id)};
                if(func_index >= cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const& tgt{cache.index_unchecked(func_index)};
                call_stack_guard g{tgt.frame.module_id, tgt.frame.function_index};

                switch(tgt.k)
                {
                    case cached_import_target::kind::defined:
                    {
                        execute_compiled_defined(tgt.u.defined.runtime_func, tgt.u.defined.compiled_func, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                        return;
                    }
                    case cached_import_target::kind::local_imported:
                    {
                        invoke_local_imported(tgt.u.local_imported, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                        return;
                    }
                    case cached_import_target::kind::dl:
                    {
                        invoke_capi(tgt.u.capi_ptr, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                        return;
                    }
                    case cached_import_target::kind::weak_symbol:
                    {
                        invoke_capi(tgt.u.capi_ptr, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                        return;
                    }
                    [[unlikely]] default:
                    {
                        ::fast_io::fast_terminate();
                    }
                }
            }

            auto const local_index{func_index - import_n};
            auto const lf{::std::addressof(module.local_defined_function_vec_storage.index_unchecked(local_index))};
            call_stack_guard g{wasm_module_id, func_index};
            if(wasm_module_id >= g_defined_func_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& mod_cache{g_defined_func_cache.index_unchecked(wasm_module_id)};
            if(local_index >= mod_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& info{mod_cache.index_unchecked(local_index)};
            if(info.runtime_func != lf) [[unlikely]] { ::fast_io::fast_terminate(); }
            execute_compiled_defined(info.runtime_func, info.compiled_func, info.param_bytes, info.result_bytes, stack_top_ptr);
        }

        inline void
            call_indirect_bridge(::std::size_t wasm_module_id, ::std::size_t type_index, ::std::size_t table_index, ::std::byte** stack_top_ptr) UWVM_THROWS
        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(!g_compiled_all) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

            if(wasm_module_id >= g_modules.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& module_rec{g_modules.index_unchecked(wasm_module_id)};
            auto const& module{*module_rec.runtime_module};

            // Pop selector index (i32).
            wasm_i32 selector_i32{};
            *stack_top_ptr -= sizeof(selector_i32);
            ::std::memcpy(::std::addressof(selector_i32), *stack_top_ptr, sizeof(selector_i32));
            auto const selector_u32{::std::bit_cast<::std::uint_least32_t>(selector_i32)};

            auto const table{resolve_table(module, table_index)};
            if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(selector_u32 >= table->elems.size()) [[unlikely]] { trap_fatal(trap_kind::call_indirect_table_out_of_bounds); }

            auto const& elem{table->elems.index_unchecked(static_cast<::std::size_t>(selector_u32))};
            resolved_func rf{};
            func_sig_view actual_sig{};
            cached_import_target const* cached_tgt{};

            switch(elem.type)
            {
                case ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_defined:
                {
                    if(elem.storage.defined_ptr == nullptr) [[unlikely]] { trap_fatal(trap_kind::call_indirect_null_element); }
                    rf.k = resolved_func::kind::defined;
                    rf.u.defined_ptr = elem.storage.defined_ptr;
                    actual_sig = func_sig_from_defined(elem.storage.defined_ptr);
                    break;
                }
                case ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_imported:
                {
                    if(elem.storage.imported_ptr == nullptr) [[unlikely]] { trap_fatal(trap_kind::call_indirect_null_element); }

                    // Fast path: table element points to this module's import slot.
                    auto const imp_ptr{elem.storage.imported_ptr};
                    auto const base{module.imported_function_vec_storage.data()};
                    auto const imp_n{module.imported_function_vec_storage.size()};
                    if(base != nullptr && imp_ptr >= base && imp_ptr < base + imp_n)
                    {
                        auto const idx{static_cast<::std::size_t>(imp_ptr - base)};
                        if(wasm_module_id >= g_import_call_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                        auto const& cache{g_import_call_cache.index_unchecked(wasm_module_id)};
                        if(idx >= cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                        cached_tgt = ::std::addressof(cache.index_unchecked(idx));
                        auto const& tgt{*cached_tgt};

                        switch(tgt.k)
                        {
                            case cached_import_target::kind::defined:
                            {
                                rf.k = resolved_func::kind::defined;
                                rf.u.defined_ptr = tgt.u.defined.runtime_func;
                                actual_sig = tgt.sig;
                                break;
                            }
                            case cached_import_target::kind::local_imported:
                            {
                                rf.k = resolved_func::kind::local_imported;
                                rf.u.local_imported = tgt.u.local_imported;
                                actual_sig = tgt.sig;
                                break;
                            }
                            case cached_import_target::kind::dl:
                            case cached_import_target::kind::weak_symbol:
                            {
                                rf.k = (tgt.k == cached_import_target::kind::dl) ? resolved_func::kind::dl : resolved_func::kind::weak_symbol;
                                rf.u.capi_ptr = tgt.u.capi_ptr;
                                actual_sig = tgt.sig;
                                break;
                            }
                            [[unlikely]] default:
                            {
                                ::fast_io::fast_terminate();
                            }
                        }

                        break;
                    }

                    // Fallback: resolve the import-alias chain (already initialized by uwvm runtime initializer).
                    rf = resolve_func_from_import_assuming_initialized(imp_ptr);
                    switch(rf.k)
                    {
                        case resolved_func::kind::defined: actual_sig = func_sig_from_defined(rf.u.defined_ptr); break;
                        case resolved_func::kind::local_imported:
                        {
                            local_imported_t const* m{rf.u.local_imported.module_ptr};
                            actual_sig = func_sig_from_local_imported(m, rf.u.local_imported.index);
                            break;
                        }
                        case resolved_func::kind::dl:
                        case resolved_func::kind::weak_symbol:
                            actual_sig = func_sig_from_capi(rf.u.capi_ptr);
                            break;
                        [[unlikely]] default:
                            ::fast_io::fast_terminate();
                    }
                    break;
                }
                [[unlikely]] default:
                {
                    // Note: UWVM currently targets wasm1.0 MVP, where tables are effectively used for funcref-based indirect calls.
                    // This default branch is intentionally left as a hard failure to reserve room for future table element kinds
                    // (e.g., reference-types / typed function references and table.set-driven polymorphic entries).
                    // Until that extension is implemented, we can only validate/guard and must not guess semantics.
                    ::fast_io::fast_terminate();
                }
            }

            bool expected_ok{};
            auto const expected_sig{expected_sig_from_type_index(module, type_index, expected_ok)};
            if(!expected_ok) [[unlikely]] { ::fast_io::fast_terminate(); }

            if(!func_sig_equal(expected_sig, actual_sig)) [[unlikely]] { trap_fatal(trap_kind::call_indirect_type_mismatch); }

            if(cached_tgt != nullptr)
            {
                auto const& tgt{*cached_tgt};
                call_stack_guard g{tgt.frame.module_id, tgt.frame.function_index};
                switch(tgt.k)
                {
                    case cached_import_target::kind::defined:
                    {
                        execute_compiled_defined(tgt.u.defined.runtime_func, tgt.u.defined.compiled_func, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                        return;
                    }
                    case cached_import_target::kind::local_imported:
                    {
                        invoke_local_imported(tgt.u.local_imported, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                        return;
                    }
                    case cached_import_target::kind::dl:
                    case cached_import_target::kind::weak_symbol:
                    {
                        invoke_capi(tgt.u.capi_ptr, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                        return;
                    }
                    [[unlikely]] default:
                    {
                        ::fast_io::fast_terminate();
                    }
                }
            }

            if(rf.k == resolved_func::kind::defined)
            {
                auto const* const info{find_defined_func_info(rf.u.defined_ptr)};
                if(info != nullptr)
                {
                    call_stack_guard g{info->module_id, info->function_index};
                    execute_compiled_defined(info->runtime_func, info->compiled_func, info->param_bytes, info->result_bytes, stack_top_ptr);
                    return;
                }
            }

            // Best-effort: record the original table element function index when it is an imported slot.
            if(elem.type == ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_imported)
            {
                auto const imp_ptr{elem.storage.imported_ptr};
                auto const base{module.imported_function_vec_storage.data()};
                if(base == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const imp_n{module.imported_function_vec_storage.size()};
                if(imp_ptr < base || imp_ptr >= base + imp_n) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const func_idx{static_cast<::std::size_t>(imp_ptr - base)};
                call_stack_guard g{wasm_module_id, func_idx};
                invoke_resolved(rf, stack_top_ptr);
                return;
            }

            // Fallback for direct local-defined table element.
            if(elem.type == ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_defined)
            {
                auto const def_ptr{elem.storage.defined_ptr};
                auto const base{module.local_defined_function_vec_storage.data()};
                if(base == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const local_n{module.local_defined_function_vec_storage.size()};
                if(def_ptr < base || def_ptr >= base + local_n) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const local_idx{static_cast<::std::size_t>(def_ptr - base)};
                auto const func_idx{module.imported_function_vec_storage.size() + local_idx};
                call_stack_guard g{wasm_module_id, func_idx};
                invoke_resolved(rf, stack_top_ptr);
                return;
            }

            ::fast_io::fast_terminate();
        }

        inline void ensure_bridges_initialized() noexcept
        {
            if(g_bridges_initialized) { return; }
            g_bridges_initialized = true;

            ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func = unreachable_trap;
            ::uwvm2::runtime::compiler::uwvm_int::optable::trap_invalid_conversion_to_integer_func = trap_invalid_conversion_to_integer;
            ::uwvm2::runtime::compiler::uwvm_int::optable::trap_integer_divide_by_zero_func = trap_integer_divide_by_zero;
            ::uwvm2::runtime::compiler::uwvm_int::optable::trap_integer_overflow_func = trap_integer_overflow;

            ::uwvm2::runtime::compiler::uwvm_int::optable::call_func = call_bridge;
            ::uwvm2::runtime::compiler::uwvm_int::optable::call_indirect_func = call_indirect_bridge;
        }

        inline void compile_all_modules_if_needed() noexcept
        {
            ensure_bridges_initialized();
            if(g_compiled_all) { return; }
            g_compiled_all = true;

            ::fast_io::unix_timestamp start_time{};
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
#ifdef UWVM_CPP_EXCEPTIONS
                try
#endif
                {
                    start_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                }
#ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    // do nothing
                }
#endif
            }

            // Assign module ids.
            g_modules.clear();
            g_module_name_to_id.clear();
            g_defined_func_cache.clear();
            g_defined_func_ptr_ranges.clear();
            g_import_call_cache.clear();

            auto const& rt_map{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage};
            g_modules.reserve(rt_map.size());
            g_module_name_to_id.reserve(rt_map.size());

            ::std::size_t id{};
            for(auto const& kv: rt_map)
            {
                g_module_name_to_id.emplace(kv.first, id);
                compiled_module_record rec{};
                rec.module_name = kv.first;
                rec.runtime_module = ::std::addressof(kv.second);
                g_modules.push_back(::std::move(rec));
                ++id;
            }

            g_defined_func_cache.resize(g_modules.size());

            constexpr ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t kTranslateOpt{get_curr_target_tranopt()};

            // Compile modules and build function map.
            for(auto& rec: g_modules)
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option opt{};
                auto const it{g_module_name_to_id.find(rec.module_name)};
                if(it == g_module_name_to_id.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
                opt.curr_wasm_id = it->second;

                ::uwvm2::validation::error::code_validation_error_impl err{};

#ifdef UWVM_CPP_EXCEPTIONS
                try
#endif
                {
                    rec.compiled =
                        ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_all_from_uwvm_single_func<kTranslateOpt>(*rec.runtime_module,
                                                                                                                                      opt,
                                                                                                                                      err);
                }
#ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    print_and_terminate_compile_validation_error(rec.module_name, err);
                }
#endif

                auto const local_n{rec.runtime_module->local_defined_function_vec_storage.size()};
                if(local_n != rec.compiled.local_funcs.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto& mod_cache{g_defined_func_cache.index_unchecked(opt.curr_wasm_id)};
                mod_cache.clear();
                mod_cache.resize(local_n);

                if(local_n != 0uz)
                {
                    auto const base_ptr{rec.runtime_module->local_defined_function_vec_storage.data()};
                    if(base_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    ::std::uintptr_t const begin{reinterpret_cast<::std::uintptr_t>(base_ptr)};
                    constexpr ::std::size_t elem_size{sizeof(runtime_local_func_storage_t)};
                    static_assert(elem_size != 0uz);
                    if(local_n > (::std::numeric_limits<::std::uintptr_t>::max() / elem_size)) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uintptr_t const bytes{static_cast<::std::uintptr_t>(local_n * elem_size)};
                    if(begin > ::std::numeric_limits<::std::uintptr_t>::max() - bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

                    g_defined_func_ptr_ranges.push_back(defined_func_ptr_range{begin, begin + bytes, opt.curr_wasm_id});
                }

                for(::std::size_t i{}; i != local_n; ++i)
                {
                    auto const runtime_func{::std::addressof(rec.runtime_module->local_defined_function_vec_storage.index_unchecked(i))};
                    auto const compiled_func{::std::addressof(rec.compiled.local_funcs.index_unchecked(i))};

                    auto const sig{func_sig_from_defined(runtime_func)};
                    auto const param_bytes{total_abi_bytes(sig.params)};
                    auto const result_bytes{total_abi_bytes(sig.results)};
                    if((param_bytes == 0uz && sig.params.size != 0uz) || (result_bytes == 0uz && sig.results.size != 0uz)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                    mod_cache.index_unchecked(i) = compiled_defined_func_info{opt.curr_wasm_id,
                                                                              rec.runtime_module->imported_function_vec_storage.size() + i,
                                                                              runtime_func,
                                                                              compiled_func,
                                                                              param_bytes,
                                                                              result_bytes};
                }
            }

            if(!g_defined_func_ptr_ranges.empty())
            {
                ::std::sort(g_defined_func_ptr_ranges.begin(),
                            g_defined_func_ptr_ranges.end(),
                            [](defined_func_ptr_range const& a, defined_func_ptr_range const& b) constexpr noexcept { return a.begin < b.begin; });
            }

            // Build an O(1) dispatch table for imported calls, flattening any import-alias chains ahead of time.
            g_import_call_cache.resize(g_modules.size());
            for(::std::size_t mid{}; mid != g_modules.size(); ++mid)
            {
                auto const& rec{g_modules.index_unchecked(mid)};
                auto const rt{rec.runtime_module};
                if(rt == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const import_n{rt->imported_function_vec_storage.size()};
                auto& cache{g_import_call_cache.index_unchecked(mid)};
                cache.clear();
                cache.resize(import_n);

                for(::std::size_t i{}; i != import_n; ++i)
                {
                    auto const imp{::std::addressof(rt->imported_function_vec_storage.index_unchecked(i))};
                    auto const rf{resolve_func_from_import_assuming_initialized(imp)};

                    cached_import_target tgt{};
                    // Default to the import slot frame; for resolved wasm functions we overwrite with the final module/function index.
                    tgt.frame.module_id = mid;
                    tgt.frame.function_index = i;

                    switch(rf.k)
                    {
                        case resolved_func::kind::defined:
                        {
                            auto const* const info{find_defined_func_info(rf.u.defined_ptr)};
                            if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                            tgt.k = cached_import_target::kind::defined;
                            tgt.frame.module_id = info->module_id;
                            tgt.frame.function_index = info->function_index;
                            tgt.sig = func_sig_from_defined(info->runtime_func);
                            tgt.param_bytes = info->param_bytes;
                            tgt.result_bytes = info->result_bytes;
                            tgt.u.defined.runtime_func = info->runtime_func;
                            tgt.u.defined.compiled_func = info->compiled_func;
                            break;
                        }
                        case resolved_func::kind::local_imported:
                        {
                            tgt.k = cached_import_target::kind::local_imported;
                            tgt.u.local_imported = rf.u.local_imported;
                            tgt.sig = func_sig_from_local_imported(tgt.u.local_imported.module_ptr, tgt.u.local_imported.index);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        case resolved_func::kind::dl:
                        {
                            tgt.k = cached_import_target::kind::dl;
                            tgt.u.capi_ptr = rf.u.capi_ptr;
                            tgt.sig = func_sig_from_capi(tgt.u.capi_ptr);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        case resolved_func::kind::weak_symbol:
                        {
                            tgt.k = cached_import_target::kind::weak_symbol;
                            tgt.u.capi_ptr = rf.u.capi_ptr;
                            tgt.sig = func_sig_from_capi(tgt.u.capi_ptr);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        [[unlikely]] default:
                        {
                            ::fast_io::fast_terminate();
                        }
                    }

                    cache.index_unchecked(i) = tgt;
                }
            }

            // finished
            ::fast_io::unix_timestamp end_time{};
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
#ifdef UWVM_CPP_EXCEPTIONS
                try
#endif
                {
                    end_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                }
#ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    // do nothing
                }
#endif

                // verbose
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"UWVM Interperter full translation done. (time=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                    end_time - start_time,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"s). ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(verbose)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }
        }

    }  // namespace

    void full_compile_and_run_main_module(::uwvm2::utils::container::u8string_view main_module_name, full_compile_run_config cfg) noexcept
    {
        compile_all_modules_if_needed();

        auto const it{g_module_name_to_id.find(main_module_name)};
        if(it == g_module_name_to_id.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const main_id{it->second};

        auto const main_module{g_modules.index_unchecked(main_id).runtime_module};
        if(main_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        // Bind WASI Preview1 env to the main module's memory[0] before any guest-to-host call is made.
        bind_default_wasip1_memory(*main_module);
#endif

        auto const import_n{main_module->imported_function_vec_storage.size()};
        auto const total_n{import_n + main_module->local_defined_function_vec_storage.size()};
        if(cfg.entry_function_index >= total_n) [[unlikely]] { ::fast_io::fast_terminate(); }

        // Allocate the exact host-call stack space required by the entry function signature.
        // Layout: [params...] then call; the callee pops params and pushes results.
        ::std::size_t param_bytes{};
        ::std::size_t result_bytes{};
        if(cfg.entry_function_index < import_n)
        {
            if(main_id >= g_import_call_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& cache{g_import_call_cache.index_unchecked(main_id)};
            if(cfg.entry_function_index >= cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& tgt{cache.index_unchecked(cfg.entry_function_index)};

            // For VM entry, only allow imported functions that ultimately resolve to a wasm-defined function.
            if(tgt.k != cached_import_target::kind::defined) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Entry function is imported but resolves to a non-wasm implementation (module=\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    main_module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\").\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            // We don't pass host arguments; require `() -> ()`.
            if(tgt.sig.params.size != 0uz || tgt.sig.results.size != 0uz) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Entry function signature is not () -> () (module=\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    main_module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\").\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            param_bytes = tgt.param_bytes;
            result_bytes = tgt.result_bytes;
        }
        else
        {
            auto const local_index{cfg.entry_function_index - import_n};
            if(main_id >= g_defined_func_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& mod_cache{g_defined_func_cache.index_unchecked(main_id)};
            if(local_index >= mod_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& entry_info{mod_cache.index_unchecked(local_index)};
            auto const* const expected_rt{::std::addressof(main_module->local_defined_function_vec_storage.index_unchecked(local_index))};
            if(entry_info.runtime_func != expected_rt) [[unlikely]] { ::fast_io::fast_terminate(); }

            // We don't pass host arguments; require `() -> ()`.
            if(entry_info.param_bytes != 0uz || entry_info.result_bytes != 0uz) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Entry function signature is not () -> () (module=\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    main_module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\").\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            param_bytes = entry_info.param_bytes;
            result_bytes = entry_info.result_bytes;
        }

        if(param_bytes > (::std::numeric_limits<::std::size_t>::max() - result_bytes)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const stack_bytes{param_bytes + result_bytes};

        heap_buf_guard host_stack_guard{};
        ::std::byte* host_stack_base{};
        UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(host_stack_base, stack_bytes, host_stack_guard);
        ::std::byte* stack_top_ptr{host_stack_base + param_bytes};

#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            call_bridge(main_id, cfg.entry_function_index, ::std::addressof(stack_top_ptr));
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
            trap_fatal(trap_kind::uncatched_int_tag);
        }
#endif
    }
}  // namespace uwvm2::runtime::uwvm_int

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

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
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
// macro
#include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
#include <uwvm2/utils/macro/push_macros.h>

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
        inline ::uwvm2::utils::container::unordered_flat_map<runtime_local_func_storage_t const*, compiled_defined_func_info> g_defined_func_map{};
        inline bool g_bridges_initialized{};
        inline bool g_compiled_all{};

        struct call_stack_frame
        {
            ::std::size_t module_id{};
            ::std::size_t function_index{};
        };

        inline ::uwvm2::utils::container::vector<call_stack_frame> g_call_stack{};

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
                                    u8" #",
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

                ::fast_io::io::perr(u8log_output_ul, u8"\n\n", ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }
        }

        [[noreturn]] inline constexpr void trap_fatal(trap_kind k) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8" runtime crash (",
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

            auto const* const wf{am.module_storage_ptr.wf};
            if(wf == nullptr || wf->binfmt_ver != 1u) [[unlikely]] { fallback_and_terminate(); }

            auto const file_name{wf->file_name};
            auto const& module_storage{wf->wasm_module_storage.wasm_binfmt_ver1_storage};

            auto const* const module_begin{module_storage.module_span.module_begin};
            auto const* const module_end{module_storage.module_span.module_end};
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

        [[nodiscard]] inline constexpr ::std::size_t operand_stack_capacity_bytes(::std::size_t operand_stack_max_values) noexcept
        {
            if(operand_stack_max_values > (::std::numeric_limits<::std::size_t>::max() / local_slot_size)) [[unlikely]] { return 0uz; }
            return operand_stack_max_values * local_slot_size;
        }

        using opfunc_byref_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<::std::byte const*, ::std::byte*, ::std::byte*>;

        using byte_allocator = ::fast_io::native_thread_local_allocator;

        struct heap_buf_guard
        {
            void* ptr{};

            inline constexpr ~heap_buf_guard()
            {
                if(ptr) { byte_allocator::deallocate(ptr); }
            }
        };

        [[nodiscard]] inline ::std::byte* alloc_zeroed_bytes_nonnull(::std::size_t bytes, heap_buf_guard& guard) noexcept
        {
            if(bytes == 0uz) { bytes = 1uz; }
            if(bytes < 1024uz) [[likely]]
            {
#if UWVM_HAS_BUILTIN(__builtin_alloca)
                void* const p{__builtin_alloca(bytes)};
#elif defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__) && !defined(__CYGWIN__)
                void* const p{_alloca(bytes)};
#else
                void* const p{alloca(bytes)};
#endif
                ::std::memset(p, 0, bytes);
                return static_cast<::std::byte*>(p);
            }
            void* const p{byte_allocator::allocate_zero(bytes)};
            guard.ptr = p;
            return static_cast<::std::byte*>(p);
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
            ::std::byte* local_base{alloc_zeroed_bytes_nonnull(compiled_func->local_bytes_max, locals_guard)};

            if(param_bytes > compiled_func->local_bytes_max) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(param_bytes != 0uz) { ::std::memcpy(local_base, caller_args_begin, param_bytes); }

            // Allocate operand stack with the exact max byte size computed by the compiler (byte-packed: i32/f32=4, i64/f64=8).
            heap_buf_guard operand_guard{};
            auto const stack_cap_raw{compiled_func->operand_stack_byte_max != 0uz ? compiled_func->operand_stack_byte_max
                                                                                  : operand_stack_capacity_bytes(compiled_func->operand_stack_max)};
            if(stack_cap_raw == 0uz && compiled_func->operand_stack_max != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
            ::std::byte* const operand_base{alloc_zeroed_bytes_nonnull(stack_cap_raw, operand_guard)};

            ::std::byte const* ip{compiled_func->op.operands.data()};
            ::std::byte* stack_top{operand_base};

            while(ip != nullptr)
            {
                opfunc_byref_t fn;  // no init
                ::std::memcpy(::std::addressof(fn), ip, sizeof(fn));
                fn(ip, stack_top, local_base);
            }

            auto const actual_result_bytes{static_cast<::std::size_t>(stack_top - operand_base)};
            if(actual_result_bytes != result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

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
            ::std::byte* const resbuf{alloc_zeroed_bytes_nonnull(res_bytes, res_guard)};
            ::std::byte* const parbuf{alloc_zeroed_bytes_nonnull(para_bytes, par_guard)};
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
            ::std::byte* const resbuf{alloc_zeroed_bytes_nonnull(res_bytes, res_guard)};
            ::std::byte* const parbuf{alloc_zeroed_bytes_nonnull(para_bytes, par_guard)};
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
                    auto it{g_defined_func_map.find(rf.u.defined_ptr)};
                    if(it == g_defined_func_map.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
                    execute_compiled_defined(it->second.runtime_func,
                                             it->second.compiled_func,
                                             it->second.param_bytes,
                                             it->second.result_bytes,
                                             caller_stack_top_ptr);
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
            auto const it{g_defined_func_map.find(lf)};
            if(it == g_defined_func_map.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
            execute_compiled_defined(it->second.runtime_func, it->second.compiled_func, it->second.param_bytes, it->second.result_bytes, stack_top_ptr);
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
                auto const it{g_defined_func_map.find(rf.u.defined_ptr)};
                if(it != g_defined_func_map.end())
                {
                    call_stack_guard g{it->second.module_id, it->second.function_index};
                    invoke_resolved(rf, stack_top_ptr);
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

        inline consteval ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t get_curr_target_tranopt() noexcept
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t res{};

#if !(defined(__pdp11) || (defined(__MSDOS__) || defined(__DJGPP__)) || (defined(__wasm__) && !defined(__wasm_tail_call__)))
            res.is_tail_call = true;
#endif

#if defined(__ARM_PCS_AAPCS64)
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 16uz;
#else
#endif

            return res;
        }

        inline void compile_all_modules_if_needed() noexcept
        {
            ensure_bridges_initialized();
            if(g_compiled_all) { return; }
            g_compiled_all = true;

            // Assign module ids.
            g_modules.clear();
            g_module_name_to_id.clear();
            g_defined_func_map.clear();
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
                    g_defined_func_map.emplace(runtime_func,
                                               compiled_defined_func_info{opt.curr_wasm_id,
                                                                          rec.runtime_module->imported_function_vec_storage.size() + i,
                                                                          runtime_func,
                                                                          compiled_func,
                                                                          param_bytes,
                                                                          result_bytes});
                }
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
                            auto const it{g_defined_func_map.find(rf.u.defined_ptr)};
                            if(it == g_defined_func_map.end()) [[unlikely]] { ::fast_io::fast_terminate(); }

                            tgt.k = cached_import_target::kind::defined;
                            tgt.frame.module_id = it->second.module_id;
                            tgt.frame.function_index = it->second.function_index;
                            tgt.sig = func_sig_from_defined(it->second.runtime_func);
                            tgt.param_bytes = it->second.param_bytes;
                            tgt.result_bytes = it->second.result_bytes;
                            tgt.u.defined.runtime_func = it->second.runtime_func;
                            tgt.u.defined.compiled_func = it->second.compiled_func;
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

        auto const import_n{main_module->imported_function_vec_storage.size()};
        if(cfg.entry_function_index < import_n) [[unlikely]]
        {
            // Entry function must not be imported.
            ::fast_io::fast_terminate();
        }

        auto const total_n{import_n + main_module->local_defined_function_vec_storage.size()};
        if(cfg.entry_function_index >= total_n) [[unlikely]] { ::fast_io::fast_terminate(); }

        ::std::byte stack_buf_storage[8uz * 8uz]{};  // enough for common start/_start
        ::std::byte* stack_top{stack_buf_storage};
        ::std::byte* stack_top_ptr{stack_top};

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
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

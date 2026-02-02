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
            runtime_local_func_storage_t const* runtime_func{};
            compiled_local_func_t const* compiled_func{};
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

            [[nodiscard]] inline constexpr ::std::uint_least8_t at(::std::size_t i) const noexcept
            {
                if(i >= size) [[unlikely]] { return 0u; }

                if(kind == valtype_kind::raw_u8)
                {
                    auto const* p{static_cast<::std::uint_least8_t const*>(data)};
                    return p[i];
                }
                else
                {
                    auto const* p{static_cast<wasm_value_type const*>(data)};
                    return static_cast<::std::uint_least8_t>(p[i]);
                }
            }
        };

        struct func_sig_view
        {
            valtype_vec_view params{};
            valtype_vec_view results{};
        };

        [[nodiscard]] inline constexpr ::std::size_t valtype_size(::std::uint_least8_t code) noexcept
        {
            switch(static_cast<wasm_value_type>(code))
            {
                case wasm_value_type::i32: return 4uz;
                case wasm_value_type::i64: return 8uz;
                case wasm_value_type::f32: return 4uz;
                case wasm_value_type::f64: return 8uz;
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
            auto const* ft{f->function_type_ptr};
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

        [[nodiscard]] inline bool try_link_imported_func_using_uwvm(runtime_imported_func_storage_t* imp) noexcept
        {
            if(imp == nullptr) [[unlikely]] { return false; }

            using link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
            if(imp->link_kind != link_kind::unresolved) { return true; }

            auto const* import_ptr{imp->import_type_ptr};
            if(import_ptr == nullptr) [[unlikely]] { return false; }

            // Resolve export record from uwvm global export table.
            auto const mod_it{::uwvm2::uwvm::wasm::storage::all_module_export.find(import_ptr->module_name)};
            if(mod_it == ::uwvm2::uwvm::wasm::storage::all_module_export.end()) [[unlikely]] { return false; }
            auto const name_it{mod_it->second.find(import_ptr->extern_name)};
            if(name_it == mod_it->second.end()) [[unlikely]] { return false; }

            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;
            using local_imported_export_type_t = ::uwvm2::uwvm::wasm::type::local_imported_export_type_t;

            auto const& export_record{name_it->second};
            switch(export_record.type)
            {
                case module_type_t::exec_wasm: [[fallthrough]];
                case module_type_t::preloaded_wasm:
                {
                    if(export_record.storage.wasm_file_export_storage_ptr.binfmt_ver != 1u) [[unlikely]] { return false; }
                    auto const* export_ptr{export_record.storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                    if(export_ptr == nullptr || export_ptr->type != external_types::func) [[unlikely]] { return false; }

                    auto const rt_it{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.find(import_ptr->module_name)};
                    if(rt_it == ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.end()) [[unlikely]] { return false; }

                    auto* const exported_rt{::std::addressof(rt_it->second)};

                    // This conversion is reasonable, as no index exceeding size_t will occur during the parsing phase.
                    auto const exported_idx{static_cast<::std::size_t>(export_ptr->storage.func_idx)};
                    auto const imported_count{exported_rt->imported_function_vec_storage.size()};

                    if(exported_idx < imported_count)
                    {
                        imp->target.imported_ptr = ::std::addressof(exported_rt->imported_function_vec_storage.index_unchecked(exported_idx));
                        imp->link_kind = link_kind::imported;
                        imp->is_opposite_side_imported = true;
                    }
                    else
                    {
                        auto const local_idx{exported_idx - imported_count};
                        if(local_idx >= exported_rt->local_defined_function_vec_storage.size()) [[unlikely]] { return false; }

                        imp->target.defined_ptr = ::std::addressof(exported_rt->local_defined_function_vec_storage.index_unchecked(local_idx));
                        imp->link_kind = link_kind::defined;
                        imp->is_opposite_side_imported = false;
                    }

                    return true;
                }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                case module_type_t::preloaded_dl:
                {
                    auto const* dl_ptr{export_record.storage.wasm_dl_export_storage_ptr.storage};
                    if(dl_ptr == nullptr) [[unlikely]] { return false; }
                    imp->target.dl_ptr = dl_ptr;
                    imp->link_kind = link_kind::dl;
                    imp->is_opposite_side_imported = false;
                    return true;
                }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                case module_type_t::weak_symbol:
                {
                    auto const* weak_ptr{export_record.storage.wasm_weak_symbol_export_storage_ptr.storage};
                    if(weak_ptr == nullptr) [[unlikely]] { return false; }
                    imp->target.weak_symbol_ptr = weak_ptr;
                    imp->link_kind = link_kind::weak_symbol;
                    imp->is_opposite_side_imported = false;
                    return true;
                }
#endif
                case module_type_t::local_import:
                {
                    auto const& li_exp{export_record.storage.local_imported_export_storage_ptr};
                    if(li_exp.type != local_imported_export_type_t::func || li_exp.storage == nullptr) [[unlikely]] { return false; }

                    imp->target.local_imported.module_ptr = li_exp.storage;
                    imp->target.local_imported.index = li_exp.index;
                    imp->link_kind = link_kind::local_imported;
                    imp->is_opposite_side_imported = false;
                    return true;
                }
                [[unlikely]] default:
                {
                    return false;
                }
            }
        }

        [[nodiscard]] inline runtime_imported_func_storage_t const* resolve_import_chain(runtime_imported_func_storage_t const* f) noexcept
        {
            auto const* curr{f};
            for(::std::size_t steps{};; ++steps)
            {
                if(steps > 8192uz) [[unlikely]] { return nullptr; }
                if(curr == nullptr) [[unlikely]] { return nullptr; }

                using link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                if(curr->link_kind == link_kind::unresolved)
                {
                    // Link on-demand using uwvm internal storages populated by the main runtime.
                    auto* const mut{const_cast<runtime_imported_func_storage_t*>(curr)};
                    if(!try_link_imported_func_using_uwvm(mut)) [[unlikely]] { return nullptr; }
                    continue;
                }

                if(curr->link_kind != link_kind::imported) { return curr; }
                curr = curr->target.imported_ptr;
            }
        }

        [[nodiscard]] inline resolved_func resolve_func_from_import(runtime_imported_func_storage_t const* f) noexcept
        {
            using link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
            auto const* curr{f};
            for(::std::size_t steps{};; ++steps)
            {
                if(steps > 8192uz) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const* leaf{resolve_import_chain(curr)};
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
                    case link_kind::imported:
                    {
                        curr = leaf->target.imported_ptr;
                        continue;
                    }
                    case link_kind::unresolved:
                    default:
                    {
                        ::fast_io::fast_terminate();
                    }
                }
            }
        }

        [[nodiscard]] inline constexpr ::std::size_t operand_stack_capacity_bytes(::std::size_t operand_stack_max_values) noexcept
        {
            if(operand_stack_max_values > (::std::numeric_limits<::std::size_t>::max() / local_slot_size)) [[unlikely]] { return 0uz; }
            return operand_stack_max_values * local_slot_size;
        }

        using opfunc_byref_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<::std::byte const*, ::std::byte*, ::std::byte*>;

        inline void execute_compiled_defined(runtime_local_func_storage_t const* runtime_func,
                                             compiled_local_func_t const* compiled_func,
                                             ::std::byte** caller_stack_top_ptr) noexcept
        {
            auto const sig{func_sig_from_defined(runtime_func)};
            auto const param_bytes{total_abi_bytes(sig.params)};
            auto const result_bytes{total_abi_bytes(sig.results)};
            if((param_bytes == 0uz && sig.params.size != 0uz) || (result_bytes == 0uz && sig.results.size != 0uz)) [[unlikely]] { ::fast_io::fast_terminate(); }

            // v128 is not supported in the scalar local-slot layout.
            for(::std::size_t i{}; i != sig.params.size; ++i)
            {
                if(sig.params.at(i) == static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
            }
            for(::std::size_t i{}; i != sig.results.size; ++i)
            {
                if(sig.results.at(i) == static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
            }

            auto* caller_stack_top{*caller_stack_top_ptr};
            auto* const caller_args_begin{caller_stack_top - param_bytes};
            // Pop params from the caller stack first (so nested calls can't see them).
            *caller_stack_top_ptr = caller_args_begin;

            // Allocate and initialize locals.
            ::uwvm2::utils::container::vector<::std::byte> locals{};
            locals.resize(compiled_func->local_count * local_slot_size);
            ::std::memset(locals.data(), 0, locals.size());

            // Copy params into locals[0..param_count).
            ::std::byte const* argp{caller_args_begin};
            for(::std::size_t i{}; i != sig.params.size; ++i)
            {
                auto const sz{valtype_size(sig.params.at(i))};
                ::std::memcpy(locals.data() + (i * local_slot_size), argp, sz);
                argp += sz;
            }

            // Allocate operand stack (max values * 8 bytes is safe for scalar wasm1).
            auto const stack_cap{operand_stack_capacity_bytes(compiled_func->operand_stack_max)};
            if(stack_cap == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }

            ::uwvm2::utils::container::vector<::std::byte> operand_stack{};
            operand_stack.resize(stack_cap);

            ::std::byte const* ip{compiled_func->op.operands.data()};
            ::std::byte* stack_top{operand_stack.data()};
            ::std::byte* local_base{locals.data()};

            while(ip != nullptr)
            {
                opfunc_byref_t fn;  // no init
                ::std::memcpy(::std::addressof(fn), ip, sizeof(fn));
                fn(ip, stack_top, local_base);
            }

            auto const actual_result_bytes{static_cast<::std::size_t>(stack_top - operand_stack.data())};
            if(actual_result_bytes != result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

            // Append results back to caller stack.
            ::std::memcpy(*caller_stack_top_ptr, operand_stack.data(), result_bytes);
            *caller_stack_top_ptr += result_bytes;
        }

        inline void invoke_resolved(resolved_func const& rf, ::std::byte** caller_stack_top_ptr) noexcept
        {
            switch(rf.k)
            {
                case resolved_func::kind::defined:
                {
                    auto it{g_defined_func_map.find(rf.u.defined_ptr)};
                    if(it == g_defined_func_map.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
                    execute_compiled_defined(it->second.runtime_func, it->second.compiled_func, caller_stack_top_ptr);
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

                    auto* const caller_stack_top{*caller_stack_top_ptr};
                    auto* const caller_args_begin{caller_stack_top - para_bytes};
                    *caller_stack_top_ptr = caller_args_begin;

                    ::uwvm2::utils::container::vector<::std::byte> resbuf{};
                    ::uwvm2::utils::container::vector<::std::byte> parbuf{};
                    resbuf.resize(res_bytes);
                    parbuf.resize(para_bytes);
                    ::std::memcpy(parbuf.data(), caller_args_begin, para_bytes);

                    m->call_func_index(rf.u.local_imported.index, resbuf.data(), parbuf.data());

                    ::std::memcpy(*caller_stack_top_ptr, resbuf.data(), res_bytes);
                    *caller_stack_top_ptr += res_bytes;
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

                    auto* const caller_stack_top{*caller_stack_top_ptr};
                    auto* const caller_args_begin{caller_stack_top - para_bytes};
                    *caller_stack_top_ptr = caller_args_begin;

                    ::uwvm2::utils::container::vector<::std::byte> resbuf{};
                    ::uwvm2::utils::container::vector<::std::byte> parbuf{};
                    resbuf.resize(res_bytes);
                    parbuf.resize(para_bytes);
                    ::std::memcpy(parbuf.data(), caller_args_begin, para_bytes);

                    f->func_ptr(resbuf.data(), parbuf.data());

                    ::std::memcpy(*caller_stack_top_ptr, resbuf.data(), res_bytes);
                    *caller_stack_top_ptr += res_bytes;
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
                auto const* t{::std::addressof(module.imported_table_vec_storage.index_unchecked(table_index))};
                for(;;)
                {
                    if(t == nullptr) [[unlikely]] { return nullptr; }
                    using lk = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                    switch(t->link_kind)
                    {
                        case lk::defined: return t->target.defined_ptr;
                        case lk::imported: t = t->target.imported_ptr; continue;
                        case lk::unresolved: [[fallthrough]];
                        default: return nullptr;
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
            auto const* begin{module.type_section_storage.type_section_begin};
            auto const* end{module.type_section_storage.type_section_end};
            if(begin == nullptr || end == nullptr) [[unlikely]] { return {}; }
            auto const total{static_cast<::std::size_t>(end - begin)};
            if(type_index >= total) [[unlikely]] { return {}; }

            auto const* ft{begin + type_index};
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

        inline void unreachable_trap() noexcept { ::fast_io::fast_terminate(); }

        inline void trap_invalid_conversion_to_integer() noexcept { ::fast_io::fast_terminate(); }

        inline void trap_integer_divide_by_zero() noexcept { ::fast_io::fast_terminate(); }

        inline void trap_integer_overflow() noexcept { ::fast_io::fast_terminate(); }

        inline void call_bridge(::std::size_t wasm_module_id, ::std::size_t func_index, ::std::byte** stack_top_ptr) noexcept
        {
            compile_all_modules_if_needed();

            if(wasm_module_id >= g_modules.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& module_rec{g_modules.index_unchecked(wasm_module_id)};
            auto const& module{*module_rec.runtime_module};

            auto const import_n{module.imported_function_vec_storage.size()};
            auto const local_n{module.local_defined_function_vec_storage.size()};
            if(func_index >= import_n + local_n) [[unlikely]] { ::fast_io::fast_terminate(); }

            if(func_index < import_n)
            {
                auto const* imp{::std::addressof(module.imported_function_vec_storage.index_unchecked(func_index))};
                auto const rf{resolve_func_from_import(imp)};
                invoke_resolved(rf, stack_top_ptr);
                return;
            }

            auto const local_index{func_index - import_n};
            auto const* lf{::std::addressof(module.local_defined_function_vec_storage.index_unchecked(local_index))};
            resolved_func rf{};
            rf.k = resolved_func::kind::defined;
            rf.u.defined_ptr = lf;
            invoke_resolved(rf, stack_top_ptr);
        }

        inline void
            call_indirect_bridge(::std::size_t wasm_module_id, ::std::size_t type_index, ::std::size_t table_index, ::std::byte** stack_top_ptr) noexcept
        {
            compile_all_modules_if_needed();

            if(wasm_module_id >= g_modules.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& module_rec{g_modules.index_unchecked(wasm_module_id)};
            auto const& module{*module_rec.runtime_module};

            // Pop selector index (i32).
            wasm_i32 selector_i32{};
            *stack_top_ptr -= sizeof(selector_i32);
            ::std::memcpy(::std::addressof(selector_i32), *stack_top_ptr, sizeof(selector_i32));
            auto const selector_u32{::std::bit_cast<::std::uint_least32_t>(selector_i32)};

            auto const* table{resolve_table(module, table_index)};
            if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(selector_u32 >= table->elems.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const& elem{table->elems.index_unchecked(static_cast<::std::size_t>(selector_u32))};
            resolved_func rf{};
            func_sig_view actual_sig{};

            switch(elem.type)
            {
                case ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_defined:
                {
                    if(elem.storage.defined_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    rf.k = resolved_func::kind::defined;
                    rf.u.defined_ptr = elem.storage.defined_ptr;
                    actual_sig = func_sig_from_defined(elem.storage.defined_ptr);
                    break;
                }
                case ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_imported:
                {
                    if(elem.storage.imported_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    rf = resolve_func_from_import(elem.storage.imported_ptr);
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
                    ::fast_io::fast_terminate();
                }
            }

            bool expected_ok{};
            auto const expected_sig{expected_sig_from_type_index(module, type_index, expected_ok)};
            if(!expected_ok) [[unlikely]] { ::fast_io::fast_terminate(); }

            if(!func_sig_equal(expected_sig, actual_sig)) [[unlikely]] { ::fast_io::fast_terminate(); }

            invoke_resolved(rf, stack_top_ptr);
        }

        inline void ensure_bridges_initialized() noexcept
        {
            if(g_bridges_initialized) { return; }
            g_bridges_initialized = true;

            ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func = &unreachable_trap;
            ::uwvm2::runtime::compiler::uwvm_int::optable::trap_invalid_conversion_to_integer_func = &trap_invalid_conversion_to_integer;
            ::uwvm2::runtime::compiler::uwvm_int::optable::trap_integer_divide_by_zero_func = &trap_integer_divide_by_zero;
            ::uwvm2::runtime::compiler::uwvm_int::optable::trap_integer_overflow_func = &trap_integer_overflow;

            ::uwvm2::runtime::compiler::uwvm_int::optable::call_func = &call_bridge;
            ::uwvm2::runtime::compiler::uwvm_int::optable::call_indirect_func = &call_indirect_bridge;
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

            constexpr ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t kTranslateOpt{.is_tail_call = false};

            // Compile modules and build function map.
            for(auto& rec: g_modules)
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option opt{};
                auto const it{g_module_name_to_id.find(rec.module_name)};
                if(it == g_module_name_to_id.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
                opt.curr_wasm_id = it->second;

                ::uwvm2::validation::error::code_validation_error_impl err{};
                rec.compiled =
                    ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_all_from_uwvm_single_func<kTranslateOpt>(*rec.runtime_module,
                                                                                                                                  opt,
                                                                                                                                  err);

                auto const local_n{rec.runtime_module->local_defined_function_vec_storage.size()};
                if(local_n != rec.compiled.local_funcs.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

                for(::std::size_t i{}; i != local_n; ++i)
                {
                    auto const* runtime_func{::std::addressof(rec.runtime_module->local_defined_function_vec_storage.index_unchecked(i))};
                    auto const* compiled_func{::std::addressof(rec.compiled.local_funcs.index_unchecked(i))};
                    g_defined_func_map.emplace(runtime_func, compiled_defined_func_info{opt.curr_wasm_id, runtime_func, compiled_func});
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

        auto const* const main_module{g_modules.index_unchecked(main_id).runtime_module};
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

        call_bridge(main_id, cfg.entry_function_index, ::std::addressof(stack_top_ptr));
    }
}  // namespace uwvm2::runtime::uwvm_int

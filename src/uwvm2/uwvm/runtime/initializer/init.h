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
# include <limits>
# include <memory>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/impl.h>
# include <uwvm2/object/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/wasm/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::initializer
{
    namespace details
    {
        template <typename... Args>
        inline constexpr void verbose_info(Args&&... args) noexcept
        {
            if(!::uwvm2::uwvm::io::show_verbose) [[likely]] { return; }

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                ::std::forward<Args>(args)...,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                u8"[",
                                local(::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::realtime)),
                                u8"] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(verbose)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
        }

        inline ::uwvm2::utils::container::u8string_view current_initializing_module_name{};

        template <typename... Args>
        inline constexpr void verbose_module_info(Args&&... args) noexcept
        {
            if(current_initializing_module_name.empty())
            {
                verbose_info(::std::forward<Args>(args)...);
                return;
            }

            verbose_info(u8"initializer: Module \"",
                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                         current_initializing_module_name,
                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                         u8"\": ",
                         ::std::forward<Args>(args)...);
        }

        inline constexpr ::std::size_t importdesc_func_index{0uz};
        inline constexpr ::std::size_t importdesc_table_index{1uz};
        inline constexpr ::std::size_t importdesc_memory_index{2uz};
        inline constexpr ::std::size_t importdesc_global_index{3uz};
        inline constexpr ::std::size_t importdesc_tag_index{4uz};

        // this is an adl function
        inline constexpr ::uwvm2::object::global::global_type
            to_object_global_type(::uwvm2::parser::wasm::standard::wasm1::type::value_type t /* [adl] */) noexcept
        {
            /// @note The parser stage already validated the module version/value type, so no version/feature checks are needed here.
            switch(t)
            {
                case ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32:
                {
                    return ::uwvm2::object::global::global_type::wasm_i32;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64:
                {
                    return ::uwvm2::object::global::global_type::wasm_i64;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32:
                {
                    return ::uwvm2::object::global::global_type::wasm_f32;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64:
                {
                    return ::uwvm2::object::global::global_type::wasm_f64;
                }
                [[unlikely]] default:
                {
                    // This is the adl function, whose output matches the parser's results. If errors occur, they are caused by bugs within the function itself
                    // (not due to forgotten implementation).
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }
        }

        // wasm1 const expr allows: i32/i64/f32/f64.const and global.get (only immutable imported globals).
        // Note: uwvm2 uses `std::uint_least64_t` for runtime offsets/addresses, so wasm1 `i32` offsets need widening conversion.
        // For wasm1 table/data offsets, the expression must evaluate to an i32, so we best-effort decode:
        // - i32.const
        // - global.get (only after import-linking, see `try_eval_wasm1_const_expr_offset_after_linking`)
        inline constexpr void
            try_eval_wasm1_const_expr_offset(::uwvm2::parser::wasm::standard::wasm1::const_expr::wasm1_const_expr_storage_t const& expr /* [adl] */,
                                             ::std::uint_least64_t& out) noexcept
        {
            if(expr.opcodes.size() != 1uz) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"initializer: wasm1.0 const expr must contain exactly one opcode; got ",
                                    expr.opcodes.size(),
                                    u8".\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            // size checked before, not empty
            auto const& op{expr.opcodes.front_unchecked()};
            if(op.opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const)
            {
                out = static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(op.storage.i32));
                return;
            }
            else if(op.opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get)
            {
                // wasm1.0 allows `global.get` (imported immutable globals only), but evaluation requires import-linking.
                // Keep a placeholder here; `finalize_wasm1_offsets_after_linking()` will evaluate the real value.
                out = 0u;
                return;
            }

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"initializer: Constant expression offset retrieval in wasm1.0 encountered an invalid instruction: ",
                                ::fast_io::mnp::hex0x<true>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(op.opcode)),
                                u8".\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            // terminate
            ::fast_io::fast_terminate();
        }

        inline void ensure_wasm1_local_defined_global_initialized(::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t& g) noexcept;

        inline void try_resolve_wasm1_imported_global_value(::uwvm2::uwvm::runtime::storage::imported_global_storage_t const* imported_global_ptr,
                                                            ::uwvm2::object::global::wasm_global_storage_t const*& out) noexcept
        {
            using imported_global_ptr_t = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t const*;

            ::uwvm2::utils::container::unordered_flat_set<imported_global_ptr_t> visited{};

            auto curr{imported_global_ptr};
            for(;;)
            {
                if(curr == nullptr) [[unlikely]]
                {
                    // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }

                // Detect reference cycles in imported globals.
                if(!visited.emplace(curr).second) [[unlikely]]
                {
                    if(curr->import_type_ptr == nullptr) [[unlikely]]
                    {
                        // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"initializer: Global \"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        curr->import_type_ptr->module_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8".",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        curr->import_type_ptr->extern_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\" encountered a circular dependency during initialization.\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    // terminate
                    ::fast_io::fast_terminate();
                }

                if(curr->is_opposite_side_imported)
                {
                    auto const* next{curr->target.imported_ptr};
                    if(next == nullptr) [[unlikely]]
                    {
                        if(curr->import_type_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"initializer: Unresolved imported global \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            curr->import_type_ptr->module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8".",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            curr->import_type_ptr->extern_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\".\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    curr = next;
                    continue;
                }

                using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;

                if(curr->link_kind == global_link_kind::local_imported)
                {
                    // Resolve leaf to a local-imported global (host global).
                    auto const idx{curr->target.local_imported.index};
                    auto* const li{curr->target.local_imported.module_ptr};
                    if(li == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    thread_local ::uwvm2::object::global::wasm_global_storage_t local_imported_global{};
                    local_imported_global.kind = to_object_global_type(li->global_value_type_from_index(idx));
                    local_imported_global.is_mutable = li->global_is_mutable_from_index(idx);
                    li->global_get_from_index(idx, reinterpret_cast<::std::byte*>(::std::addressof(local_imported_global.storage)));

                    out = ::std::addressof(local_imported_global);
                    return;
                }

                if(curr->link_kind != global_link_kind::defined) [[unlikely]]
                {
                    if(curr->import_type_ptr == nullptr) [[unlikely]]
                    {
                        // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"initializer: Unresolved imported global \"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        curr->import_type_ptr->module_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8".",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        curr->import_type_ptr->extern_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\".\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }

                auto* const def{curr->target.defined_ptr};
                if(def == nullptr) [[unlikely]]
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }

                ensure_wasm1_local_defined_global_initialized(*def);

                out = ::std::addressof(def->global);
                return;
            }

            // unreachable
            ::std::unreachable();
        }

        inline void try_resolve_wasm1_imported_global_i32_value(::uwvm2::uwvm::runtime::storage::imported_global_storage_t const* imported_global_ptr,
                                                                ::std::uint_least64_t& out) noexcept
        {
            using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;

            if(imported_global_ptr == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            if(imported_global_ptr->import_type_ptr == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            if(imported_global_ptr->import_type_ptr->imports.type != external_types::global) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            // wasm1.0: offsets can only read imported *immutable* globals via `global.get`.
            if(imported_global_ptr->import_type_ptr->imports.storage.global.is_mutable) [[unlikely]]
            {
                ::fast_io::io::perr(
                    ::uwvm2::uwvm::io::u8log_output,
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                    u8"uwvm: ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                    u8"[fatal] ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8"initializer: In wasm1.0, constant expressions may only use `global.get` on imported immutable globals; got mutable global \"",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                    imported_global_ptr->import_type_ptr->module_name,
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8".",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                    imported_global_ptr->import_type_ptr->extern_name,
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8"\".\n\n",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            ::uwvm2::object::global::wasm_global_storage_t const* resolved_global{};
            try_resolve_wasm1_imported_global_value(imported_global_ptr, resolved_global);

            if(resolved_global == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            if(resolved_global->kind != ::uwvm2::object::global::global_type::wasm_i32) [[unlikely]]
            {
                ::fast_io::io::perr(
                    ::uwvm2::uwvm::io::u8log_output,
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                    u8"uwvm: ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                    u8"[fatal] ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8"initializer: In wasm1.0, constant expressions retrieve offsets from imported globals, where the global type is not i32: ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                    ::uwvm2::object::global::get_global_type_name(resolved_global->kind),
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8".\n\n",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            out = static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(resolved_global->storage.i32));
            return;
        }

        inline bool maybe_resolve_wasm1_imported_table_defined(::uwvm2::uwvm::runtime::storage::imported_table_storage_t const* imported_table_ptr,
                                                               ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t*& out) noexcept
        {
            using imported_table_ptr_t = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t const*;

            ::uwvm2::utils::container::unordered_flat_set<imported_table_ptr_t> visited{};

            auto const* curr{imported_table_ptr};
            for(;;)
            {
                if(curr == nullptr) [[unlikely]]
                {
                    // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }

                // Detect reference cycles in imported tables.
                if(!visited.emplace(curr).second) [[unlikely]]
                {
                    if(curr->import_type_ptr == nullptr) [[unlikely]]
                    {
                        // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"initializer: Table \"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        curr->import_type_ptr->module_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8".",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        curr->import_type_ptr->extern_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\" encountered a circular dependency during import resolution.\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }

                if(curr->is_opposite_side_imported)
                {
                    auto const* next{curr->target.imported_ptr};
                    if(next == nullptr) [[unlikely]] { return false; }
                    curr = next;
                    continue;
                }

                using table_link_kind = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                if(curr->link_kind != table_link_kind::defined) [[unlikely]] { return false; }

                auto* def{curr->target.defined_ptr};
                if(def == nullptr) [[unlikely]] { return false; }

                out = def;
                return true;
            }
        }

        inline bool maybe_resolve_wasm1_imported_memory_defined(::uwvm2::uwvm::runtime::storage::imported_memory_storage_t const* imported_memory_ptr,
                                                                ::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t*& out) noexcept
        {
            using imported_memory_ptr_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t const*;

            ::uwvm2::utils::container::unordered_flat_set<imported_memory_ptr_t> visited{};

            auto const* curr{imported_memory_ptr};
            for(;;)
            {
                if(curr == nullptr) [[unlikely]]
                {
                    // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }

                // Detect reference cycles in imported memories.
                if(!visited.emplace(curr).second) [[unlikely]]
                {
                    if(curr->import_type_ptr == nullptr) [[unlikely]]
                    {
                        // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"initializer: Memory \"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        curr->import_type_ptr->module_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8".",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        curr->import_type_ptr->extern_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\" encountered a circular dependency during import resolution.\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }

                if(curr->is_opposite_side_imported)
                {
                    auto const* next{curr->target.imported_ptr};
                    if(next == nullptr) [[unlikely]] { return false; }
                    curr = next;
                    continue;
                }

                using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                if(curr->link_kind != memory_link_kind::defined) [[unlikely]] { return false; }

                auto* def{curr->target.defined_ptr};
                if(def == nullptr) [[unlikely]] { return false; }

                out = def;
                return true;
            }
        }

        struct wasm1_resolved_imported_memory_t
        {
            ::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t* defined_ptr{};
            ::uwvm2::uwvm::wasm::type::local_imported_t* local_imported_ptr{};
            ::std::size_t local_imported_index{};
        };

        inline bool maybe_resolve_wasm1_imported_memory(::uwvm2::uwvm::runtime::storage::imported_memory_storage_t const* imported_memory_ptr,
                                                        wasm1_resolved_imported_memory_t& out) noexcept
        {
            using imported_memory_ptr_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t const*;

            ::uwvm2::utils::container::unordered_flat_set<imported_memory_ptr_t> visited{};

            auto const* curr{imported_memory_ptr};
            for(;;)
            {
                if(curr == nullptr) [[unlikely]]
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }

                if(!visited.emplace(curr).second) [[unlikely]]
                {
                    if(curr->import_type_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"initializer: Memory \"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        curr->import_type_ptr->module_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8".",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        curr->import_type_ptr->extern_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\" encountered a circular dependency during import resolution.\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }

                if(curr->is_opposite_side_imported)
                {
                    auto const* next{curr->target.imported_ptr};
                    if(next == nullptr) [[unlikely]] { return false; }
                    curr = next;
                    continue;
                }

                using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                if(curr->link_kind == memory_link_kind::defined)
                {
                    out.defined_ptr = curr->target.defined_ptr;
                    out.local_imported_ptr = nullptr;
                    out.local_imported_index = 0uz;
                    return true;
                }

                if(curr->link_kind == memory_link_kind::local_imported)
                {
                    out.defined_ptr = nullptr;
                    out.local_imported_ptr = curr->target.local_imported.module_ptr;
                    out.local_imported_index = curr->target.local_imported.index;
                    return true;
                }

                return false;
            }
        }

        inline constexpr bool wasm1_limits_match(::uwvm2::parser::wasm::standard::wasm1::type::limits_type const& expected,
                                                 ::uwvm2::parser::wasm::standard::wasm1::type::limits_type const& actual) noexcept
        {
            if(actual.min < expected.min) { return false; }

            if(expected.present_max)
            {
                if(!actual.present_max) { return false; }
                if(actual.max > expected.max) { return false; }
            }

            return true;
        }

        template <typename ValueType>
        inline constexpr ::std::size_t safe_ptr_range_size(ValueType const* begin, ValueType const* end) noexcept
        {
            if(begin == end) { return 0uz; }
            if(begin == nullptr || end == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            auto const diff{end - begin};
            if(diff < 0) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }
            return static_cast<::std::size_t>(diff);
        }

        inline constexpr bool wasm1_function_type_equal(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* expected,
                                                        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* actual) noexcept
        {
            if(expected == actual) { return true; }
            if(expected == nullptr || actual == nullptr) { return false; }

            auto const expected_para_len{safe_ptr_range_size(expected->parameter.begin, expected->parameter.end)};
            auto const actual_para_len{safe_ptr_range_size(actual->parameter.begin, actual->parameter.end)};
            if(expected_para_len != actual_para_len) { return false; }

            for(::std::size_t i{}; i != expected_para_len; ++i)
            {
                if(expected->parameter.begin[i] != actual->parameter.begin[i]) { return false; }
            }

            auto const expected_res_len{safe_ptr_range_size(expected->result.begin, expected->result.end)};
            auto const actual_res_len{safe_ptr_range_size(actual->result.begin, actual->result.end)};
            if(expected_res_len != actual_res_len) { return false; }

            for(::std::size_t i{}; i != expected_res_len; ++i)
            {
                if(expected->result.begin[i] != actual->result.begin[i]) { return false; }
            }

            return true;
        }

        inline constexpr bool wasm1_function_type_equal_to_capi(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* expected,
                                                                ::uwvm2::uwvm::wasm::type::capi_function_t const* actual) noexcept
        {
            if(expected == nullptr || actual == nullptr) { return false; }

            auto const expected_para_len{safe_ptr_range_size(expected->parameter.begin, expected->parameter.end)};
            if(expected_para_len != actual->para_type_vec_size) { return false; }
            if(actual->para_type_vec_size != 0uz && actual->para_type_vec_begin == nullptr) [[unlikely]] { return false; }

            for(::std::size_t i{}; i != expected_para_len; ++i)
            {
                if(static_cast<::std::uint_least8_t>(expected->parameter.begin[i]) != actual->para_type_vec_begin[i]) { return false; }
            }

            auto const expected_res_len{safe_ptr_range_size(expected->result.begin, expected->result.end)};
            if(expected_res_len != actual->res_type_vec_size) { return false; }
            if(actual->res_type_vec_size != 0uz && actual->res_type_vec_begin == nullptr) [[unlikely]] { return false; }

            for(::std::size_t i{}; i != expected_res_len; ++i)
            {
                if(static_cast<::std::uint_least8_t>(expected->result.begin[i]) != actual->res_type_vec_begin[i]) { return false; }
            }

            return true;
        }

        inline void validate_wasm_file_module_import_types_after_linking() noexcept
        {
            using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;

            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage)
            {
                verbose_info(u8"initializer: Validate import types for module \"",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             curr_module_name,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8"\". ");

                ::std::size_t func_checked{};
                ::std::size_t table_checked{};
                ::std::size_t table_skipped_unresolved{};
                ::std::size_t memory_checked{};
                ::std::size_t memory_skipped_unresolved{};
                ::std::size_t global_checked{};

                // func imports
                for(auto const& imp: curr_rt.imported_function_vec_storage)
                {
                    auto const* import_ptr{imp.import_type_ptr};
                    if(import_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                    if(imp.link_kind == func_link_kind::unresolved) { continue; }
                    ++func_checked;

                    if(import_ptr->imports.type != external_types::func) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    auto const* expected_type{import_ptr->imports.storage.function};
                    if(expected_type == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    if(imp.link_kind == func_link_kind::imported)
                    {
                        auto const* imported_target{imp.target.imported_ptr};
                        if(imported_target == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const* target_import_ptr{imported_target->import_type_ptr};
                        if(target_import_ptr == nullptr || target_import_ptr->imports.type != external_types::func ||
                           target_import_ptr->imports.storage.function == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const* actual_type{target_import_ptr->imports.storage.function};
                        if(!wasm1_function_type_equal(expected_type, actual_type)) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\", imported function \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch.\n    expected: ",
                                                ::uwvm2::parser::wasm::standard::wasm1::features::section_details(*expected_type),
                                                u8"\n    got: ",
                                                ::uwvm2::parser::wasm::standard::wasm1::features::section_details(*actual_type),
                                                u8"\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }
                    }
                    else if(imp.link_kind == func_link_kind::defined)
                    {
                        auto const* def{imp.target.defined_ptr};
                        if(def == nullptr || def->function_type_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const* actual_type{def->function_type_ptr};
                        if(!wasm1_function_type_equal(expected_type, actual_type)) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\", imported function \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch.\n    expected: ",
                                                ::uwvm2::parser::wasm::standard::wasm1::features::section_details(*expected_type),
                                                u8"\n    got: ",
                                                ::uwvm2::parser::wasm::standard::wasm1::features::section_details(*actual_type),
                                                u8"\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }
                    }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                    else if(imp.link_kind == func_link_kind::dl)
                    {
                        auto const* dl_ptr{imp.target.dl_ptr};
                        if(dl_ptr == nullptr) [[unlikely]]
                        {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                            ::fast_io::fast_terminate();
                        }

                        if(!wasm1_function_type_equal_to_capi(expected_type, dl_ptr)) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\", imported function \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch.\n    expected: ",
                                                ::uwvm2::parser::wasm::standard::wasm1::features::section_details(*expected_type),
                                                u8"\n    got: (dl) para_types=",
                                                dl_ptr->para_type_vec_size,
                                                u8", res_types=",
                                                dl_ptr->res_type_vec_size,
                                                u8"\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }
                    }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                    else if(imp.link_kind == func_link_kind::weak_symbol)
                    {
                        auto const* weak_ptr{imp.target.weak_symbol_ptr};
                        if(weak_ptr == nullptr) [[unlikely]]
                        {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                            ::fast_io::fast_terminate();
                        }

                        if(!wasm1_function_type_equal_to_capi(expected_type, weak_ptr)) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\", imported function \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch.\n    expected: ",
                                                ::uwvm2::parser::wasm::standard::wasm1::features::section_details(*expected_type),
                                                u8"\n    got: (weak_symbol) para_types=",
                                                weak_ptr->para_type_vec_size,
                                                u8", res_types=",
                                                weak_ptr->res_type_vec_size,
                                                u8"\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }
                    }
#endif
                    else if(imp.link_kind == func_link_kind::local_imported)
                    {
                        auto const& li{imp.target.local_imported};
                        if(li.module_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const info{li.module_ptr->get_function_information_from_index(li.index)};
                        if(!info.successed) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const* actual_type{::std::addressof(info.function_type)};
                        if(!wasm1_function_type_equal(expected_type, actual_type)) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\", imported function \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch.\n    expected: ",
                                                ::uwvm2::parser::wasm::standard::wasm1::features::section_details(*expected_type),
                                                u8"\n    got: ",
                                                ::uwvm2::parser::wasm::standard::wasm1::features::section_details(*actual_type),
                                                u8"\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }
                    }
                    else [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }

                // table imports
                for(auto const& imp: curr_rt.imported_table_vec_storage)
                {
                    auto const* import_ptr{imp.import_type_ptr};
                    if(import_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    using table_link_kind = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                    if(imp.link_kind == table_link_kind::unresolved) { continue; }

                    if(import_ptr->imports.type != external_types::table) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t* resolved_table{};
                    if(!maybe_resolve_wasm1_imported_table_defined(::std::addressof(imp), resolved_table))
                    {
                        ++table_skipped_unresolved;
                        continue;
                    }
                    ++table_checked;

                    if(resolved_table == nullptr || resolved_table->table_type_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    auto const& expected_table{import_ptr->imports.storage.table};
                    auto const& actual_table{*resolved_table->table_type_ptr};
                    if(!wasm1_limits_match(expected_table.limits, actual_table.limits)) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In module \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            curr_module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\", imported table \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            import_ptr->module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8".",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            import_ptr->extern_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\" has a type mismatch.\n    expected: ",
                                            ::uwvm2::parser::wasm::standard::wasm1::type::section_details(expected_table),
                                            u8"\n    got: ",
                                            ::uwvm2::parser::wasm::standard::wasm1::type::section_details(actual_table),
                                            u8"\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }
                }

                // memory imports
                for(auto const& imp: curr_rt.imported_memory_vec_storage)
                {
                    auto const* import_ptr{imp.import_type_ptr};
                    if(import_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                    if(imp.link_kind == memory_link_kind::unresolved) { continue; }

                    if(import_ptr->imports.type != external_types::memory) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    wasm1_resolved_imported_memory_t resolved_memory{};
                    if(!maybe_resolve_wasm1_imported_memory(::std::addressof(imp), resolved_memory))
                    {
                        ++memory_skipped_unresolved;
                        continue;
                    }
                    ++memory_checked;

                    auto const& expected_memory{import_ptr->imports.storage.memory};

                    if(resolved_memory.defined_ptr != nullptr)
                    {
                        if(resolved_memory.defined_ptr->memory_type_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const& actual_memory{*resolved_memory.defined_ptr->memory_type_ptr};
                        if(!wasm1_limits_match(expected_memory.limits, actual_memory.limits)) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\", imported memory \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch.\n    expected: ",
                                                ::uwvm2::parser::wasm::standard::wasm1::type::section_details(expected_memory),
                                                u8"\n    got: ",
                                                ::uwvm2::parser::wasm::standard::wasm1::type::section_details(actual_memory),
                                                u8"\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }
                    }
                    else
                    {
                        if(resolved_memory.local_imported_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const page_size_bytes{resolved_memory.local_imported_ptr->memory_page_size_from_index(resolved_memory.local_imported_index)};
                        if(page_size_bytes != 65536u) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\", imported memory \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\" has an unsupported host page size (page_size_bytes=",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                page_size_bytes,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8").\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }

                        auto const page_count_u64{resolved_memory.local_imported_ptr->memory_size_from_index(resolved_memory.local_imported_index)};
                        constexpr auto max_u32{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
                        if(page_count_u64 > max_u32) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        ::uwvm2::parser::wasm::standard::wasm1::type::memory_type actual_memory{};
                        actual_memory.limits.min = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(page_count_u64);
                        actual_memory.limits.present_max = true;
                        actual_memory.limits.max = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(page_count_u64);

                        if(!wasm1_limits_match(expected_memory.limits, actual_memory.limits)) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\", imported memory \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch.\n    expected: ",
                                                ::uwvm2::parser::wasm::standard::wasm1::type::section_details(expected_memory),
                                                u8"\n    got: ",
                                                ::uwvm2::parser::wasm::standard::wasm1::type::section_details(actual_memory),
                                                u8"\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }
                    }
                }

                // global imports
                for(auto const& imp: curr_rt.imported_global_vec_storage)
                {
                    auto const* import_ptr{imp.import_type_ptr};
                    if(import_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;
                    if(imp.link_kind == global_link_kind::unresolved) { continue; }
                    ++global_checked;

                    if(import_ptr->imports.type != external_types::global) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    auto const& expected_global{import_ptr->imports.storage.global};
                    ::uwvm2::parser::wasm::standard::wasm1::type::global_type const* actual_global_ptr{};
                    ::uwvm2::parser::wasm::standard::wasm1::type::global_type actual_global_local_imported{};

                    if(imp.link_kind == global_link_kind::imported)
                    {
                        auto const* imported_target{imp.target.imported_ptr};
                        if(imported_target == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const* target_import_ptr{imported_target->import_type_ptr};
                        if(target_import_ptr == nullptr || target_import_ptr->imports.type != external_types::global) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }
                        actual_global_ptr = ::std::addressof(target_import_ptr->imports.storage.global);
                    }
                    else if(imp.link_kind == global_link_kind::defined)
                    {
                        auto const* def{imp.target.defined_ptr};
                        if(def == nullptr || def->global_type_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }
                        actual_global_ptr = def->global_type_ptr;
                    }
                    else
                    {
                        if(imp.link_kind != global_link_kind::local_imported) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const& li{imp.target.local_imported};
                        if(li.module_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const vt_u8{static_cast<::std::uint_least8_t>(li.module_ptr->global_value_type_from_index(li.index))};

                        switch(vt_u8)
                        {
                            case static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32):
                            case static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64):
                            case static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32):
                            case static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64):
                            {
                                actual_global_local_imported.type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(vt_u8);
                                break;
                            }
                            [[unlikely]] default:
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported global \"",
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                    u8"\" has an unsupported host global type.\n\n",
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            }
                        }

                        actual_global_local_imported.is_mutable = li.module_ptr->global_is_mutable_from_index(li.index);
                        actual_global_ptr = ::std::addressof(actual_global_local_imported);
                    }

                    if(actual_global_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    auto const& actual_global{*actual_global_ptr};
                    if(expected_global.type != actual_global.type || expected_global.is_mutable != actual_global.is_mutable) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In module \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            curr_module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\", imported global \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            import_ptr->module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8".",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            import_ptr->extern_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\" has a type mismatch.\n    expected: ",
                                            ::uwvm2::parser::wasm::standard::wasm1::type::section_details(expected_global),
                                            u8"\n    got: ",
                                            ::uwvm2::parser::wasm::standard::wasm1::type::section_details(actual_global),
                                            u8"\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }
                }

                verbose_info(u8"initializer: Import type validation summary for module \"",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             curr_module_name,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8"\": checked(f/t/m/g)=",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             func_checked,
                             u8"/",
                             table_checked,
                             u8"/",
                             memory_checked,
                             u8"/",
                             global_checked,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8", unresolved_skipped(t/m)=",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             table_skipped_unresolved,
                             u8"/",
                             memory_skipped_unresolved,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8". ");
            }
        }

        inline void try_eval_wasm1_const_expr_offset_after_linking(::uwvm2::parser::wasm::standard::wasm1::const_expr::wasm1_const_expr_storage_t const& expr,
                                                                   ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_rt,
                                                                   ::std::uint_least64_t& out) noexcept
        {
            if(expr.opcodes.size() != 1uz)
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"initializer: wasm1.0 const expr must contain exactly one opcode; got ",
                                    expr.opcodes.size(),
                                    u8".\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            auto const& op{expr.opcodes.front_unchecked()};

            if(op.opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const)
            {
                out = static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(op.storage.i32));
                return;
            }
            else if(op.opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get)
            {
                auto const idx{static_cast<::std::size_t>(op.storage.imported_global_idx)};
                auto const imported_global_count{curr_rt.imported_global_vec_storage.size()};
                if(idx >= imported_global_count) [[unlikely]]
                {
                    ::fast_io::io::perr(
                        ::uwvm2::uwvm::io::u8log_output,
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                        u8"uwvm: ",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                        u8"[fatal] ",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                        u8"initializer: In wasm1.0, constant expressions retrieve offsets from imported globals, where the index is out of bounds: ",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                        idx,
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                        u8" >= ",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                        imported_global_count,
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                        u8".\n\n",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }

                try_resolve_wasm1_imported_global_i32_value(::std::addressof(curr_rt.imported_global_vec_storage.index_unchecked(idx)), out);

                return;
            }

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"initializer: Constant expression offset retrieval in wasm1.0 encountered an invalid instruction: ",
                                ::fast_io::mnp::hex0x<true>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(op.opcode)),
                                u8".\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            // terminate
            ::fast_io::fast_terminate();
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline void initialize_from_binfmt_ver1_module_storage(
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& out) noexcept
        {
            using type_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>;
            using import_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>;
            using table_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>;
            using memory_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>;
            using global_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>;
            using element_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>;
            using code_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>;
            using data_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::data_section_storage_t<Fs...>;

            auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t>(module_storage.sections)};
            auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t>(module_storage.sections)};
            auto const& funcsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>(module_storage.sections)};
            auto const& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<table_section_storage_t>(module_storage.sections)};
            auto const& memorysec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<memory_section_storage_t>(module_storage.sections)};
            auto const& globalsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<global_section_storage_t>(module_storage.sections)};
            auto const& elemsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<element_section_storage_t>(module_storage.sections)};
            auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<code_section_storage_t>(module_storage.sections)};
            auto const& datasec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<data_section_storage_t>(module_storage.sections)};

            verbose_module_info(u8"Init: imported descriptors. ");
            // imported
            {
                auto const& imported_funcs{importsec.importdesc.index_unchecked(importdesc_func_index)};
                out.imported_function_vec_storage.reserve(imported_funcs.size());
                for(auto const import_ptr: imported_funcs)
                {
                    ::uwvm2::uwvm::runtime::storage::imported_function_storage_t rec{};
                    rec.import_type_ptr = import_ptr;
                    out.imported_function_vec_storage.push_back_unchecked(::std::move(rec));
                }
            }
            {
                auto const& imported_tables{importsec.importdesc.index_unchecked(importdesc_table_index)};
                out.imported_table_vec_storage.reserve(imported_tables.size());
                for(auto const import_ptr: imported_tables)
                {
                    ::uwvm2::uwvm::runtime::storage::imported_table_storage_t rec{};
                    rec.import_type_ptr = import_ptr;
                    out.imported_table_vec_storage.push_back_unchecked(::std::move(rec));
                }
            }
            {
                auto const& imported_memories{importsec.importdesc.index_unchecked(importdesc_memory_index)};
                out.imported_memory_vec_storage.reserve(imported_memories.size());
                for(auto const import_ptr: imported_memories)
                {
                    ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t rec{};
                    rec.import_type_ptr = import_ptr;
                    out.imported_memory_vec_storage.push_back_unchecked(::std::move(rec));
                }
            }
            {
                auto const& imported_globals{importsec.importdesc.index_unchecked(importdesc_global_index)};
                out.imported_global_vec_storage.reserve(imported_globals.size());
                for(auto const import_ptr: imported_globals)
                {
                    ::uwvm2::uwvm::runtime::storage::imported_global_storage_t rec{};
                    rec.import_type_ptr = import_ptr;
                    out.imported_global_vec_storage.push_back_unchecked(::std::move(rec));
                }
            }

            verbose_module_info(u8"Init: local functions and code. ");
            // local defined function + code
            {
                auto const defined_func_count{funcsec.funcs.size()};
                if(defined_func_count != codesec.codes.size()) [[unlikely]]
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }

                out.local_defined_function_vec_storage.reserve(defined_func_count);
                out.local_defined_code_vec_storage.reserve(defined_func_count);

                for(::std::size_t i{}; i != defined_func_count; ++i)
                {
                    auto const type_idx{static_cast<::std::size_t>(funcsec.funcs.index_unchecked(i))};
                    if(type_idx >= typesec.types.size()) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    ::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t f{};
                    f.function_type_ptr = ::std::addressof(typesec.types.index_unchecked(type_idx));
                    f.wasm_code_ptr = ::std::addressof(codesec.codes.index_unchecked(i));
                    out.local_defined_function_vec_storage.push_back_unchecked(f);

                    ::uwvm2::uwvm::runtime::storage::local_defined_code_storage_t c{};
                    c.code_type_ptr = ::std::addressof(codesec.codes.index_unchecked(i));
                    c.func_ptr = ::std::addressof(out.local_defined_function_vec_storage.back());
                    out.local_defined_code_vec_storage.push_back_unchecked(c);
                }
            }

            verbose_module_info(u8"Init: local tables. ");
            // local defined table
            {
                out.local_defined_table_vec_storage.reserve(tablesec.tables.size());
                for(auto const& table_type: tablesec.tables)
                {
                    ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t rec{};
                    rec.table_type_ptr = ::std::addressof(table_type);
                    rec.elems.resize(static_cast<::std::size_t>(table_type.limits.min));
                    out.local_defined_table_vec_storage.push_back_unchecked(::std::move(rec));
                }
            }

            verbose_module_info(u8"Init: local memories. ");
            // local defined memory
            {
                out.local_defined_memory_vec_storage.reserve(memorysec.memories.size());
                for(auto const& memory_type: memorysec.memories)
                {
                    out.local_defined_memory_vec_storage.emplace_back();
                    auto& rec{out.local_defined_memory_vec_storage.back()};
                    rec.memory_type_ptr = ::std::addressof(memory_type);
                    rec.memory.init_by_page_count(static_cast<::std::size_t>(memory_type.limits.min));
                }
            }

            verbose_module_info(u8"Init: local globals. ");
            // local defined global
            {
                out.local_defined_global_vec_storage.reserve(globalsec.local_globals.size());
                for(auto const& local_global: globalsec.local_globals)
                {
                    ::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t rec{};
                    rec.global_type_ptr = ::std::addressof(local_global.global);
                    rec.local_global_type_ptr = ::std::addressof(local_global);
                    rec.global.kind = details::to_object_global_type(local_global.global.type);
                    rec.global.is_mutable = local_global.global.is_mutable;

                    if(local_global.expr.opcodes.size() != 1uz) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"initializer: wasm1.0 global initializer const expr must contain exactly one opcode; got ",
                                            local_global.expr.opcodes.size(),
                                            u8".\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    auto const& op{local_global.expr.opcodes.front_unchecked()};
                    switch(op.opcode)
                    {
                        case ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const:
                        {
                            rec.global.storage.i32 = op.storage.i32;
                            rec.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                            break;
                        }
                        case ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i64_const:
                        {
                            rec.global.storage.i64 = op.storage.i64;
                            rec.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                            break;
                        }
                        case ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f32_const:
                        {
                            rec.global.storage.f32 = op.storage.f32;
                            rec.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                            break;
                        }
                        case ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f64_const:
                        {
                            rec.global.storage.f64 = op.storage.f64;
                            rec.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                            break;
                        }
                        case ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get:
                        {
                            // Requires import-linking; evaluated in `finalize_wasm1_globals_after_linking()`.
                            rec.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::uninitialized;
                            break;
                        }
                        [[unlikely]] default:
                        {
                            ::fast_io::io::perr(
                                ::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"initializer: wasm1.0 global initializer const expr encountered an invalid instruction: ",
                                ::fast_io::mnp::hex0x<true>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(op.opcode)),
                                u8".\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }
                    }

                    out.local_defined_global_vec_storage.push_back_unchecked(::std::move(rec));
                }
            }

            verbose_module_info(u8"Init: element segments. ");
            // element (wasm1: active segments)
            {
                out.local_defined_element_vec_storage.reserve(elemsec.elems.size());
                for(auto const& elem: elemsec.elems)
                {
                    ::uwvm2::uwvm::runtime::storage::local_defined_element_storage_t rec{};
                    rec.element_type_ptr = ::std::addressof(elem);
                    rec.element.table_idx = elem.storage.table_idx.table_idx;
                    auto const funcidx_size{elem.storage.table_idx.vec_funcidx.size()};
                    if(funcidx_size == 0uz)
                    {
                        rec.element.funcidx_begin = nullptr;
                        rec.element.funcidx_end = nullptr;
                    }
                    else
                    {
                        rec.element.funcidx_begin = elem.storage.table_idx.vec_funcidx.data();
                        rec.element.funcidx_end = rec.element.funcidx_begin + funcidx_size;
                    }
                    rec.element.kind = ::uwvm2::uwvm::runtime::storage::wasm_element_segment_kind::active;
                    rec.element.dropped = false;
                    try_eval_wasm1_const_expr_offset(elem.storage.table_idx.expr, rec.element.offset);
                    out.local_defined_element_vec_storage.push_back_unchecked(::std::move(rec));
                }
            }

            verbose_module_info(u8"Init: data segments. ");
            // data (wasm1: active segments)
            {
                out.local_defined_data_vec_storage.reserve(datasec.datas.size());
                for(auto const& data: datasec.datas)
                {
                    ::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t rec{};
                    rec.data_type_ptr = ::std::addressof(data);
                    rec.data.kind = ::uwvm2::uwvm::runtime::storage::wasm_data_segment_kind::active;
                    rec.data.dropped = false;
                    rec.data.memory_idx = data.storage.memory_idx.memory_idx;
                    rec.data.byte_begin = reinterpret_cast<::std::byte const*>(data.storage.memory_idx.byte.begin);
                    rec.data.byte_end = reinterpret_cast<::std::byte const*>(data.storage.memory_idx.byte.end);
                    try_eval_wasm1_const_expr_offset(data.storage.memory_idx.expr, rec.data.offset);
                    out.local_defined_data_vec_storage.push_back_unchecked(::std::move(rec));
                }
            }
        }

        inline void ensure_wasm1_local_defined_global_initialized(::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t& g) noexcept
        {
            using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;

            switch(g.init_state)
            {
                case ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized:
                {
                    return;
                }
                case ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initializing:
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"initializer: Global initialization encountered a circular dependency.\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
                case ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::uninitialized:
                {
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }

            if(g.owner_module_rt_ptr == nullptr || g.local_global_type_ptr == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            g.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initializing;

            auto const& expr{g.local_global_type_ptr->expr};
            if(expr.opcodes.size() != 1uz) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"initializer: wasm1.0 global initializer const expr must contain exactly one opcode; got ",
                                    expr.opcodes.size(),
                                    u8".\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            auto const& op{expr.opcodes.front_unchecked()};
            switch(op.opcode)
            {
                case ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const:
                {
                    g.global.storage.i32 = op.storage.i32;
                    break;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i64_const:
                {
                    g.global.storage.i64 = op.storage.i64;
                    break;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f32_const:
                {
                    g.global.storage.f32 = op.storage.f32;
                    break;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f64_const:
                {
                    g.global.storage.f64 = op.storage.f64;
                    break;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get:
                {
                    auto const idx{static_cast<::std::size_t>(op.storage.imported_global_idx)};
                    auto const imported_count{g.owner_module_rt_ptr->imported_global_vec_storage.size()};
                    if(idx >= imported_count) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In wasm1.0, global initializer refers to an imported global index that is out of bounds: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            idx,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8" >= ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            imported_count,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8".\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    auto const imported_global_ptr{::std::addressof(g.owner_module_rt_ptr->imported_global_vec_storage.index_unchecked(idx))};
                    if(imported_global_ptr->import_type_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    if(imported_global_ptr->import_type_ptr->imports.type != external_types::global) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    // wasm1.0: global initializers may only use `global.get` on imported immutable globals.
                    if(imported_global_ptr->import_type_ptr->imports.storage.global.is_mutable) [[unlikely]]
                    {
                        ::fast_io::io::perr(
                            ::uwvm2::uwvm::io::u8log_output,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"initializer: In wasm1.0, global initializers may only use `global.get` on imported immutable globals; got mutable global \"",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                            imported_global_ptr->import_type_ptr->module_name,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8".",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                            imported_global_ptr->import_type_ptr->extern_name,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"\".\n\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    ::uwvm2::object::global::wasm_global_storage_t const* resolved_global{};
                    try_resolve_wasm1_imported_global_value(imported_global_ptr, resolved_global);

                    if(resolved_global == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    if(resolved_global->kind != g.global.kind) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In wasm1.0, global initializer type mismatch: expected ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            ::uwvm2::object::global::get_global_type_name(g.global.kind),
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8", got ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            ::uwvm2::object::global::get_global_type_name(resolved_global->kind),
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8".\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    switch(g.global.kind)
                    {
                        case ::uwvm2::object::global::global_type::wasm_i32:
                        {
                            g.global.storage.i32 = resolved_global->storage.i32;
                            break;
                        }
                        case ::uwvm2::object::global::global_type::wasm_i64:
                        {
                            g.global.storage.i64 = resolved_global->storage.i64;
                            break;
                        }
                        case ::uwvm2::object::global::global_type::wasm_f32:
                        {
                            g.global.storage.f32 = resolved_global->storage.f32;
                            break;
                        }
                        case ::uwvm2::object::global::global_type::wasm_f64:
                        {
                            g.global.storage.f64 = resolved_global->storage.f64;
                            break;
                        }
                        [[unlikely]] default:
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }
                    }

                    break;
                }
                [[unlikely]] default:
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"initializer: wasm1.0 global initializer const expr encountered an invalid instruction: ",
                                        ::fast_io::mnp::hex0x<true>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(op.opcode)),
                                        u8".\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }

            g.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
        }

        inline void finalize_wasm1_globals_after_linking() noexcept
        {
            // First: attach owner pointers for on-demand evaluation across modules.
            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage)
            {
                for(auto& g: curr_rt.local_defined_global_vec_storage) { g.owner_module_rt_ptr = ::std::addressof(curr_rt); }
            }

            // Second: evaluate all wasm1 global initializers (including those that use `global.get`).
            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage)
            {
                verbose_info(u8"initializer: Finalize globals for module \"",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             curr_module_name,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8"\". ");
                ::std::size_t globals_total{curr_rt.local_defined_global_vec_storage.size()};
                ::std::size_t globals_need_eval{};
                for(auto const& g: curr_rt.local_defined_global_vec_storage)
                {
                    if(g.init_state != ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized) { ++globals_need_eval; }
                }
                for(auto& g: curr_rt.local_defined_global_vec_storage) { ensure_wasm1_local_defined_global_initialized(g); }
                verbose_info(u8"initializer: Finalize globals summary for module \"",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             curr_module_name,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8"\": total=",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             globals_total,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8", evaluated=",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             globals_need_eval,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8". ");
            }
        }

        inline void finalize_wasm1_offsets_after_linking() noexcept
        {
            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage)
            {
                verbose_info(u8"initializer: Finalize offsets for module \"",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             curr_module_name,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8"\". ");

                for(auto& elem: curr_rt.local_defined_element_vec_storage)
                {
                    if(elem.element_type_ptr == nullptr) [[unlikely]] { continue; }
                    auto const& expr{elem.element_type_ptr->storage.table_idx.expr};
                    try_eval_wasm1_const_expr_offset_after_linking(expr, curr_rt, elem.element.offset);
                }

                for(auto& data: curr_rt.local_defined_data_vec_storage)
                {
                    if(data.data_type_ptr == nullptr) [[unlikely]] { continue; }
                    auto const& expr{data.data_type_ptr->storage.memory_idx.expr};
                    try_eval_wasm1_const_expr_offset_after_linking(expr, curr_rt, data.data.offset);
                }

                verbose_info(u8"initializer: Finalize offsets summary for module \"",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             curr_module_name,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8"\": segments(elem/data)=",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             curr_rt.local_defined_element_vec_storage.size(),
                             u8"/",
                             curr_rt.local_defined_data_vec_storage.size(),
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8". ");
            }
        }

        inline constexpr ::std::size_t safe_u32_to_size_t(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 v) noexcept
        {
            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
            if constexpr(size_t_max < wasm_u32_max)
            {
                if(v > size_t_max) [[unlikely]]
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }
            return static_cast<::std::size_t>(v);
        }

        inline constexpr ::std::size_t safe_u64_to_size_t(::std::uint_least64_t v) noexcept
        {
            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            if(v > size_t_max) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }
            return static_cast<::std::size_t>(v);
        }

        template <typename NativeMemory>
        inline constexpr ::std::size_t get_native_memory_length_bytes(NativeMemory const& memory) noexcept
        {
            // All supported backends expose `get_page_size()` and `custom_page_size_log2`.
            return memory.get_page_size() << memory.custom_page_size_log2;
        }

        inline void apply_wasm1_active_element_and_data_segments_after_linking() noexcept
        {
            using table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;

            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage)
            {
                verbose_info(u8"initializer: Apply active elem/data segments for module \"",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             curr_module_name,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8"\". ");

                ::std::size_t elem_active_applied{};
                ::std::size_t data_active_applied{};

                // element (wasm1: active segments)
                for(auto const& elem_seg: curr_rt.local_defined_element_vec_storage)
                {
                    auto const& elem{elem_seg.element};
                    if(elem.kind != ::uwvm2::uwvm::runtime::storage::wasm_element_segment_kind::active || elem.dropped) { continue; }
                    ++elem_active_applied;

                    auto const table_idx{safe_u32_to_size_t(elem.table_idx)};
                    auto const imported_table_count{curr_rt.imported_table_vec_storage.size()};

                    ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t* target_table{};
                    if(table_idx < imported_table_count)
                    {
                        auto const* imported_table_ptr{::std::addressof(curr_rt.imported_table_vec_storage.index_unchecked(table_idx))};
                        if(!maybe_resolve_wasm1_imported_table_defined(imported_table_ptr, target_table) || target_table == nullptr) [[unlikely]]
                        {
                            if(imported_table_ptr == nullptr || imported_table_ptr->import_type_ptr == nullptr) [[unlikely]]
                            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                ::fast_io::fast_terminate();
                            }

                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\", element segment requires an unresolved imported table \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                imported_table_ptr->import_type_ptr->module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                imported_table_ptr->import_type_ptr->extern_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }
                    }
                    else
                    {
                        auto const local_idx{table_idx - imported_table_count};
                        if(local_idx >= curr_rt.local_defined_table_vec_storage.size()) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }
                        target_table = ::std::addressof(curr_rt.local_defined_table_vec_storage.index_unchecked(local_idx));
                    }

                    if(target_table == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    auto const offset{safe_u64_to_size_t(elem.offset)};

                    // funcidx payload length
                    auto const* funcidx_begin{elem.funcidx_begin};
                    auto const* funcidx_end{elem.funcidx_end};
                    if((funcidx_begin == nullptr) != (funcidx_end == nullptr)) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                    auto const func_count{safe_ptr_range_size(funcidx_begin, funcidx_end)};

                    auto const table_size{target_table->elems.size()};
                    if(offset > table_size || func_count > (table_size - offset)) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In module \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            curr_module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\", element segment initialization would write past table bounds (offset=",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            offset,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8", count=",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            func_count,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8", table_size=",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            table_size,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8").\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    auto const imported_func_count{curr_rt.imported_function_vec_storage.size()};
                    auto const local_func_count{curr_rt.local_defined_function_vec_storage.size()};
                    auto const all_func_count{imported_func_count + local_func_count};

                    for(::std::size_t i{}; i != func_count; ++i)
                    {
                        auto& slot{target_table->elems.index_unchecked(offset + i)};
                        auto const func_idx{safe_u32_to_size_t(funcidx_begin[i])};
                        if(func_idx >= all_func_count) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        if(func_idx < imported_func_count)
                        {
                            slot.storage.imported_ptr = ::std::addressof(curr_rt.imported_function_vec_storage.index_unchecked(func_idx));
                            slot.type = table_elem_type::func_ref_imported;
                        }
                        else
                        {
                            auto const local_idx{func_idx - imported_func_count};
                            slot.storage.defined_ptr = ::std::addressof(curr_rt.local_defined_function_vec_storage.index_unchecked(local_idx));
                            slot.type = table_elem_type::func_ref_defined;
                        }
                    }
                }

                // data (wasm1: active segments)
                for(auto const& data_seg: curr_rt.local_defined_data_vec_storage)
                {
                    auto const& data{data_seg.data};
                    if(data.kind != ::uwvm2::uwvm::runtime::storage::wasm_data_segment_kind::active || data.dropped) { continue; }
                    ++data_active_applied;

                    auto const mem_idx{safe_u32_to_size_t(data.memory_idx)};
                    auto const imported_mem_count{curr_rt.imported_memory_vec_storage.size()};

                    ::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t* target_memory{};
                    if(mem_idx < imported_mem_count)
                    {
                        auto const* imported_memory_ptr{::std::addressof(curr_rt.imported_memory_vec_storage.index_unchecked(mem_idx))};
                        if(!maybe_resolve_wasm1_imported_memory_defined(imported_memory_ptr, target_memory) || target_memory == nullptr) [[unlikely]]
                        {
                            if(imported_memory_ptr == nullptr || imported_memory_ptr->import_type_ptr == nullptr) [[unlikely]]
                            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                ::fast_io::fast_terminate();
                            }

                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\", data segment requires an unresolved imported memory \"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                imported_memory_ptr->import_type_ptr->module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                imported_memory_ptr->import_type_ptr->extern_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }
                    }
                    else
                    {
                        auto const local_idx{mem_idx - imported_mem_count};
                        if(local_idx >= curr_rt.local_defined_memory_vec_storage.size()) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }
                        target_memory = ::std::addressof(curr_rt.local_defined_memory_vec_storage.index_unchecked(local_idx));
                    }

                    if(target_memory == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    auto const offset{safe_u64_to_size_t(data.offset)};

                    auto const* byte_begin{data.byte_begin};
                    auto const* byte_end{data.byte_end};
                    if((byte_begin == nullptr) != (byte_end == nullptr)) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    auto const byte_count{safe_ptr_range_size(byte_begin, byte_end)};

                    auto const mem_length{get_native_memory_length_bytes(target_memory->memory)};
                    if(offset > mem_length || byte_count > (mem_length - offset)) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In module \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            curr_module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\", data segment initialization would write past memory bounds (offset=",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            offset,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8", size=",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            byte_count,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8", memory_size=",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            mem_length,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8").\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    if(byte_count != 0uz)
                    {
                        if(target_memory->memory.memory_begin == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        ::fast_io::freestanding::my_memcpy(target_memory->memory.memory_begin + offset, byte_begin, byte_count);
                    }
                }

                verbose_info(u8"initializer: Apply segments summary for module \"",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             curr_module_name,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8"\": applied(elem/data)=",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             elem_active_applied,
                             u8"/",
                             data_active_applied,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8". ");
            }
        }

        inline void initialize_from_wasm_file(::uwvm2::uwvm::wasm::type::wasm_file_t const& wf,
                                              ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& out) noexcept
        {
            switch(wf.binfmt_ver)
            {
                case 1u:
                {
                    initialize_from_binfmt_ver1_module_storage(wf.wasm_module_storage.wasm_binfmt_ver1_storage, out);
                    break;
                }

                    /// @todo support other version
                    static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");

                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }
        }

        inline void resolve_imports_for_wasm_file_modules() noexcept
        {
            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;
            using local_imported_export_type_t = ::uwvm2::uwvm::wasm::type::local_imported_export_type_t;

            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage)
            {
                verbose_info(u8"initializer: Resolve imports for module \"",
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                             curr_module_name,
                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                             u8"\". ");

                auto const resolve_exported_module_runtime{
                    [](auto const& import_ptr) constexpr noexcept -> ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t*
                    {
                        if(import_ptr == nullptr) [[unlikely]] { return nullptr; }
                        auto const it{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.find(import_ptr->module_name)};
                        if(it == ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.end()) [[unlikely]] { return nullptr; }
                        return ::std::addressof(it->second);
                    }};

                auto const resolve_export_record{[](auto const& import_ptr) constexpr noexcept -> ::uwvm2::uwvm::wasm::type::all_module_export_t const*
                                                 {
                                                     if(import_ptr == nullptr) [[unlikely]] { return nullptr; }

                                                     auto const mod_it{::uwvm2::uwvm::wasm::storage::all_module_export.find(import_ptr->module_name)};
                                                     if(mod_it == ::uwvm2::uwvm::wasm::storage::all_module_export.end()) [[unlikely]] { return nullptr; }
                                                     auto const name_it{mod_it->second.find(import_ptr->extern_name)};
                                                     if(name_it == mod_it->second.end()) [[unlikely]] { return nullptr; }
                                                     return ::std::addressof(name_it->second);
                                                 }};

                for(auto& imp: curr_rt.imported_function_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    auto const export_record{resolve_export_record(import_ptr)};
                    if(export_record == nullptr) [[unlikely]] { continue; }

                    switch(export_record->type)
                    {
                        case module_type_t::exec_wasm: [[fallthrough]];
                        case module_type_t::preloaded_wasm:
                        {
                            if(export_record->storage.wasm_file_export_storage_ptr.binfmt_ver != 1u) [[unlikely]] { continue; }

                            auto const export_ptr{export_record->storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                            if(export_ptr == nullptr || export_ptr->type != external_types::func) [[unlikely]] { continue; }

                            auto const exported_rt{resolve_exported_module_runtime(import_ptr)};
                            if(exported_rt == nullptr) [[unlikely]] { continue; }

                            auto const exported_idx{static_cast<::std::size_t>(export_ptr->storage.func_idx)};
                            auto const imported_count{exported_rt->imported_function_vec_storage.size()};
                            using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                            if(exported_idx < imported_count)
                            {
                                imp.target.imported_ptr = ::std::addressof(exported_rt->imported_function_vec_storage.index_unchecked(exported_idx));
                                imp.link_kind = func_link_kind::imported;
                                imp.is_opposite_side_imported = true;
                            }
                            else
                            {
                                auto const local_idx{exported_idx - imported_count};
                                if(local_idx >= exported_rt->local_defined_function_vec_storage.size()) [[unlikely]] { continue; }
                                imp.target.defined_ptr = ::std::addressof(exported_rt->local_defined_function_vec_storage.index_unchecked(local_idx));
                                imp.link_kind = func_link_kind::defined;
                                imp.is_opposite_side_imported = false;
                            }
                            break;
                        }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                        case module_type_t::preloaded_dl:
                        {
                            auto const* dl_ptr{export_record->storage.wasm_dl_export_storage_ptr.storage};
                            if(dl_ptr == nullptr) [[unlikely]] { continue; }
                            using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                            imp.target.dl_ptr = dl_ptr;
                            imp.link_kind = func_link_kind::dl;
                            imp.is_opposite_side_imported = false;
                            break;
                        }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                        case module_type_t::weak_symbol:
                        {
                            auto const* weak_ptr{export_record->storage.wasm_weak_symbol_export_storage_ptr.storage};
                            if(weak_ptr == nullptr) [[unlikely]] { continue; }
                            using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                            imp.target.weak_symbol_ptr = weak_ptr;
                            imp.link_kind = func_link_kind::weak_symbol;
                            imp.is_opposite_side_imported = false;
                            break;
                        }
#endif
                        case module_type_t::local_import:
                        {
                            auto const& li_exp{export_record->storage.local_imported_export_storage_ptr};
                            if(li_exp.type != local_imported_export_type_t::func || li_exp.storage == nullptr) [[unlikely]] { continue; }
                            using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                            imp.target.local_imported.module_ptr = li_exp.storage;
                            imp.target.local_imported.index = li_exp.index;
                            imp.link_kind = func_link_kind::local_imported;
                            imp.is_opposite_side_imported = false;
                            break;
                        }
                        [[unlikely]] default:
                        {
                            break;
                        }
                    }
                }

                {
                    auto const total{curr_rt.imported_function_vec_storage.size()};
                    ::std::size_t linked_imported{};
                    ::std::size_t linked_defined{};
                    ::std::size_t linked_local_imported{};
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                    ::std::size_t linked_dl{};
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                    ::std::size_t linked_weak_symbol{};
#endif
                    for(auto const& imp: curr_rt.imported_function_vec_storage)
                    {
                        using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                        linked_imported += static_cast<::std::size_t>(imp.link_kind == func_link_kind::imported);
                        linked_defined += static_cast<::std::size_t>(imp.link_kind == func_link_kind::defined);
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                        linked_dl += static_cast<::std::size_t>(imp.link_kind == func_link_kind::dl);
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                        linked_weak_symbol += static_cast<::std::size_t>(imp.link_kind == func_link_kind::weak_symbol);
#endif
                        linked_local_imported += static_cast<::std::size_t>(imp.link_kind == func_link_kind::local_imported);
                    }
                    auto const unresolved{total - linked_imported - linked_defined
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                          - linked_dl
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                          - linked_weak_symbol
#endif
                                          - linked_local_imported};
                    verbose_info(u8"initializer: Resolve imports summary (func) for module \"",
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                 u8"\": total=",
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                 total,
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                 u8", linked(imported/defined"
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                 u8"/dl"
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                 u8"/weak_symbol"
#endif
                                 u8"/local_imported/unresolved)=",
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                 linked_imported,
                                 u8"/",
                                 linked_defined,
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                 u8"/",
                                 linked_dl,
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                 u8"/",
                                 linked_weak_symbol,
#endif
                                 u8"/",
                                 linked_local_imported,
                                 u8"/",
                                 unresolved,
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                 u8". ");
                }

                for(auto& imp: curr_rt.imported_table_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    auto const export_record{resolve_export_record(import_ptr)};
                    if(export_record == nullptr) [[unlikely]] { continue; }
                    if(export_record->type != module_type_t::exec_wasm && export_record->type != module_type_t::preloaded_wasm) { continue; }
                    if(export_record->storage.wasm_file_export_storage_ptr.binfmt_ver != 1u) [[unlikely]] { continue; }

                    auto const export_ptr{export_record->storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                    if(export_ptr == nullptr || export_ptr->type != external_types::table) [[unlikely]] { continue; }

                    auto const exported_rt{resolve_exported_module_runtime(import_ptr)};
                    if(exported_rt == nullptr) [[unlikely]] { continue; }

                    auto const exported_idx{static_cast<::std::size_t>(export_ptr->storage.table_idx)};
                    auto const imported_count{exported_rt->imported_table_vec_storage.size()};
                    using table_link_kind = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                    if(exported_idx < imported_count)
                    {
                        imp.target.imported_ptr = ::std::addressof(exported_rt->imported_table_vec_storage.index_unchecked(exported_idx));
                        imp.link_kind = table_link_kind::imported;
                        imp.is_opposite_side_imported = true;
                    }
                    else
                    {
                        auto const local_idx{exported_idx - imported_count};
                        if(local_idx >= exported_rt->local_defined_table_vec_storage.size()) [[unlikely]] { continue; }
                        imp.target.defined_ptr = ::std::addressof(exported_rt->local_defined_table_vec_storage.index_unchecked(local_idx));
                        imp.link_kind = table_link_kind::defined;
                        imp.is_opposite_side_imported = false;
                    }
                }

                {
                    auto const total{curr_rt.imported_table_vec_storage.size()};
                    ::std::size_t linked_imported{};
                    ::std::size_t linked_defined{};
                    ::std::size_t linked_local_imported{};
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                    ::std::size_t linked_dl{};
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                    ::std::size_t linked_weak_symbol{};
#endif
                    for(auto const& imp: curr_rt.imported_table_vec_storage)
                    {
                        using table_link_kind = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                        linked_imported += static_cast<::std::size_t>(imp.link_kind == table_link_kind::imported);
                        linked_defined += static_cast<::std::size_t>(imp.link_kind == table_link_kind::defined);
                    }
                    auto const unresolved{total - linked_imported - linked_defined};
                    verbose_info(u8"initializer: Resolve imports summary (table) for module \"",
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                 u8"\": total=",
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                 total,
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                 u8", linked(imported/defined"
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                 u8"/dl"
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                 u8"/weak_symbol"
#endif
                                 u8"/local_imported/unresolved)=",
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                 linked_imported,
                                 u8"/",
                                 linked_defined,
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                 u8"/",
                                 linked_dl,
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                 u8"/",
                                 linked_weak_symbol,
#endif
                                 u8"/",
                                 linked_local_imported,
                                 u8"/",
                                 unresolved,
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                 u8". ");
                }

                for(auto& imp: curr_rt.imported_memory_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    auto const export_record{resolve_export_record(import_ptr)};
                    if(export_record == nullptr) [[unlikely]] { continue; }

                    switch(export_record->type)
                    {
                        case module_type_t::exec_wasm:
                        case module_type_t::preloaded_wasm:
                        {
                            if(export_record->storage.wasm_file_export_storage_ptr.binfmt_ver != 1u) [[unlikely]] { continue; }

                            auto const export_ptr{export_record->storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                            if(export_ptr == nullptr || export_ptr->type != external_types::memory) [[unlikely]] { continue; }

                            auto const exported_rt{resolve_exported_module_runtime(import_ptr)};
                            if(exported_rt == nullptr) [[unlikely]] { continue; }

                            auto const exported_idx{static_cast<::std::size_t>(export_ptr->storage.memory_idx)};
                            auto const imported_count{exported_rt->imported_memory_vec_storage.size()};
                            using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                            if(exported_idx < imported_count)
                            {
                                imp.target.imported_ptr = ::std::addressof(exported_rt->imported_memory_vec_storage.index_unchecked(exported_idx));
                                imp.link_kind = memory_link_kind::imported;
                                imp.is_opposite_side_imported = true;
                            }
                            else
                            {
                                auto const local_idx{exported_idx - imported_count};
                                if(local_idx >= exported_rt->local_defined_memory_vec_storage.size()) [[unlikely]] { continue; }
                                imp.target.defined_ptr = ::std::addressof(exported_rt->local_defined_memory_vec_storage.index_unchecked(local_idx));
                                imp.link_kind = memory_link_kind::defined;
                                imp.is_opposite_side_imported = false;
                            }
                            break;
                        }
                        case module_type_t::local_import:
                        {
                            auto const& li_exp{export_record->storage.local_imported_export_storage_ptr};
                            if(li_exp.type != local_imported_export_type_t::memory || li_exp.storage == nullptr) [[unlikely]] { continue; }
                            using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                            imp.target.local_imported.module_ptr = li_exp.storage;
                            imp.target.local_imported.index = li_exp.index;
                            imp.link_kind = memory_link_kind::local_imported;
                            imp.is_opposite_side_imported = false;
                            break;
                        }
                        [[unlikely]] default:
                        {
                            break;
                        }
                    }
                }

                {
                    auto const total{curr_rt.imported_memory_vec_storage.size()};
                    ::std::size_t linked_imported{};
                    ::std::size_t linked_defined{};
                    ::std::size_t linked_local_imported{};
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                    ::std::size_t linked_dl{};
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                    ::std::size_t linked_weak_symbol{};
#endif
                    for(auto const& imp: curr_rt.imported_memory_vec_storage)
                    {
                        using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                        linked_imported += static_cast<::std::size_t>(imp.link_kind == memory_link_kind::imported);
                        linked_defined += static_cast<::std::size_t>(imp.link_kind == memory_link_kind::defined);
                        linked_local_imported += static_cast<::std::size_t>(imp.link_kind == memory_link_kind::local_imported);
                    }
                    auto const unresolved{total - linked_imported - linked_defined - linked_local_imported};
                    verbose_info(u8"initializer: Resolve imports summary (memory) for module \"",
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                 u8"\": total=",
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                 total,
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                 u8", linked(imported/defined"
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                 u8"/dl"
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                 u8"/weak_symbol"
#endif
                                 u8"/local_imported/unresolved)=",
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                 linked_imported,
                                 u8"/",
                                 linked_defined,
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                 u8"/",
                                 linked_dl,
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                 u8"/",
                                 linked_weak_symbol,
#endif
                                 u8"/",
                                 linked_local_imported,
                                 u8"/",
                                 unresolved,
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                 u8". ");
                }

                for(auto& imp: curr_rt.imported_global_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    auto const export_record{resolve_export_record(import_ptr)};
                    if(export_record == nullptr) [[unlikely]] { continue; }
                    switch(export_record->type)
                    {
                        case module_type_t::exec_wasm:
                        case module_type_t::preloaded_wasm:
                        {
                            if(export_record->storage.wasm_file_export_storage_ptr.binfmt_ver != 1u) [[unlikely]] { continue; }

                            auto const export_ptr{export_record->storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                            if(export_ptr == nullptr || export_ptr->type != external_types::global) [[unlikely]] { continue; }

                            auto const exported_rt{resolve_exported_module_runtime(import_ptr)};
                            if(exported_rt == nullptr) [[unlikely]] { continue; }

                            auto const exported_idx{static_cast<::std::size_t>(export_ptr->storage.global_idx)};
                            auto const imported_count{exported_rt->imported_global_vec_storage.size()};
                            using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;
                            if(exported_idx < imported_count)
                            {
                                imp.target.imported_ptr = ::std::addressof(exported_rt->imported_global_vec_storage.index_unchecked(exported_idx));
                                imp.link_kind = global_link_kind::imported;
                                imp.is_opposite_side_imported = true;
                            }
                            else
                            {
                                auto const local_idx{exported_idx - imported_count};
                                if(local_idx >= exported_rt->local_defined_global_vec_storage.size()) [[unlikely]] { continue; }
                                imp.target.defined_ptr = ::std::addressof(exported_rt->local_defined_global_vec_storage.index_unchecked(local_idx));
                                imp.link_kind = global_link_kind::defined;
                                imp.is_opposite_side_imported = false;
                            }
                            break;
                        }
                        case module_type_t::local_import:
                        {
                            auto const& li_exp{export_record->storage.local_imported_export_storage_ptr};
                            if(li_exp.type != local_imported_export_type_t::global || li_exp.storage == nullptr) [[unlikely]] { continue; }
                            using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;
                            imp.target.local_imported.module_ptr = li_exp.storage;
                            imp.target.local_imported.index = li_exp.index;
                            imp.link_kind = global_link_kind::local_imported;
                            imp.is_opposite_side_imported = false;
                            break;
                        }
                        [[unlikely]] default:
                        {
                            break;
                        }
                    }
                }

                {
                    auto const total{curr_rt.imported_global_vec_storage.size()};
                    ::std::size_t linked_imported{};
                    ::std::size_t linked_defined{};
                    ::std::size_t linked_local_imported{};
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                    ::std::size_t linked_dl{};
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                    ::std::size_t linked_weak_symbol{};
#endif
                    for(auto const& imp: curr_rt.imported_global_vec_storage)
                    {
                        using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;
                        linked_imported += static_cast<::std::size_t>(imp.link_kind == global_link_kind::imported);
                        linked_defined += static_cast<::std::size_t>(imp.link_kind == global_link_kind::defined);
                        linked_local_imported += static_cast<::std::size_t>(imp.link_kind == global_link_kind::local_imported);
                    }
                    auto const unresolved{total - linked_imported - linked_defined - linked_local_imported
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                          - linked_dl
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                          - linked_weak_symbol
#endif
                    };
                    verbose_info(u8"initializer: Resolve imports summary (global) for module \"",
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                 u8"\": total=",
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                 total,
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                 u8", linked(imported/defined"
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                 u8"/dl"
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                 u8"/weak_symbol"
#endif
                                 u8"/local_imported/unresolved)=",
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                 linked_imported,
                                 u8"/",
                                 linked_defined,
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                 u8"/",
                                 linked_dl,
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                 u8"/",
                                 linked_weak_symbol,
#endif
                                 u8"/",
                                 linked_local_imported,
                                 u8"/",
                                 unresolved,
                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                 u8". ");
                }
            }
        }

        inline void error_on_unresolved_imports_after_linking() noexcept
        {
            bool any_unresolved{};

            for([[maybe_unused]] auto const& [curr_module_name, curr_rt]: ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage)
            {
                ::std::size_t unresolved_func{};
                ::std::size_t unresolved_table{};
                ::std::size_t unresolved_memory{};
                ::std::size_t unresolved_global{};

                {
                    using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                    for(auto const& imp: curr_rt.imported_function_vec_storage)
                    {
                        unresolved_func += static_cast<::std::size_t>(imp.link_kind == func_link_kind::unresolved);
                    }
                }
                {
                    using table_link_kind = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                    for(auto const& imp: curr_rt.imported_table_vec_storage)
                    {
                        unresolved_table += static_cast<::std::size_t>(imp.link_kind == table_link_kind::unresolved);
                    }
                }
                {
                    using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                    for(auto const& imp: curr_rt.imported_memory_vec_storage)
                    {
                        unresolved_memory += static_cast<::std::size_t>(imp.link_kind == memory_link_kind::unresolved);
                    }
                }
                {
                    using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;
                    for(auto const& imp: curr_rt.imported_global_vec_storage)
                    {
                        unresolved_global += static_cast<::std::size_t>(imp.link_kind == global_link_kind::unresolved);
                    }
                }

                if(unresolved_func == 0uz && unresolved_table == 0uz && unresolved_memory == 0uz && unresolved_global == 0uz) { continue; }
                any_unresolved = true;

                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"initializer: Unresolved imports in module \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    curr_module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\": unresolved(f/t/m/g)=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    unresolved_func,
                                    u8"/",
                                    unresolved_table,
                                    u8"/",
                                    unresolved_memory,
                                    u8"/",
                                    unresolved_global,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                auto const print_import{
                    [&curr_module_name](::uwvm2::utils::container::u8string_view kind,
                                        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t const* import_ptr) noexcept
                    {
                        if(import_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In module \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            curr_module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\", unresolved ",
                                            kind,
                                            u8" import: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            import_ptr->module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8".",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            import_ptr->extern_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                            u8"\n");
                    }};

                {
                    using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                    for(auto const& imp: curr_rt.imported_function_vec_storage)
                    {
                        if(imp.link_kind != func_link_kind::unresolved) { continue; }
                        print_import(u8"function", imp.import_type_ptr);
                    }
                }
                {
                    using table_link_kind = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                    for(auto const& imp: curr_rt.imported_table_vec_storage)
                    {
                        if(imp.link_kind != table_link_kind::unresolved) { continue; }
                        print_import(u8"table", imp.import_type_ptr);
                    }
                }
                {
                    using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                    for(auto const& imp: curr_rt.imported_memory_vec_storage)
                    {
                        if(imp.link_kind != memory_link_kind::unresolved) { continue; }
                        print_import(u8"memory", imp.import_type_ptr);
                    }
                }
                {
                    using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;
                    for(auto const& imp: curr_rt.imported_global_vec_storage)
                    {
                        if(imp.link_kind != global_link_kind::unresolved) { continue; }
                        print_import(u8"global", imp.import_type_ptr);
                    }
                }

                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output, u8"\n");
            }

            if(any_unresolved) { ::fast_io::fast_terminate(); }
        }
    }  // namespace details

    inline void initialize_runtime() noexcept
    {
        details::verbose_info(u8"Initialize the runtime environment for the WASM module. ");

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

        using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
        constexpr auto module_type_to_string{[](module_type_t t) constexpr noexcept -> ::uwvm2::utils::container::u8string_view
                                             {
                                                 switch(t)
                                                 {
                                                     case module_type_t::exec_wasm:
                                                     {
                                                         return u8"exec_wasm";
                                                     }
                                                     case module_type_t::preloaded_wasm:
                                                     {
                                                         return u8"preloaded_wasm";
                                                     }
                                                     case module_type_t::local_import:
                                                     {
                                                         return u8"local_import";
                                                     }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                                     case module_type_t::preloaded_dl:
                                                     {
                                                         return u8"preloaded_dl";
                                                     }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                                     case module_type_t::weak_symbol:
                                                     {
                                                         return u8"weak_symbol";
                                                     }
#endif
                                                     [[unlikely]] default:
                                                     {
                                                         return u8"unknown";
                                                     }
                                                 }
                                             }};

        details::verbose_info(u8"initializer: Clear runtime storage. ");
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.clear();
        auto const all_module_size{::uwvm2::uwvm::wasm::storage::all_module.size()};
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.reserve(all_module_size);
        details::verbose_info(u8"initializer: Reserve runtime storage (modules=",
                              ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                              all_module_size,
                              ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                              u8"). ");

        for(auto const& [module_name, mod]: ::uwvm2::uwvm::wasm::storage::all_module)
        {
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t rt{};
            details::verbose_info(u8"initializer: Build runtime record for module \"",
                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                  module_name,
                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                  u8"\" (type=",
                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                  module_type_to_string(mod.type),
                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                  u8"). ");

            switch(mod.type)
            {
                case ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm: [[fallthrough]];
                case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm:
                {
                    if(mod.module_storage_ptr.wf == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    details::current_initializing_module_name = module_name;
                    details::initialize_from_wasm_file(*mod.module_storage_ptr.wf, rt);
                    details::current_initializing_module_name = {};
                    details::verbose_info(u8"initializer: Module \"",
                                          ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                          module_name,
                                          ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                          u8"\": Init: imported(f/t/m/g)=",
                                          ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                          rt.imported_function_vec_storage.size(),
                                          u8"/",
                                          rt.imported_table_vec_storage.size(),
                                          u8"/",
                                          rt.imported_memory_vec_storage.size(),
                                          u8"/",
                                          rt.imported_global_vec_storage.size(),
                                          ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                          u8", local(f/t/m/g)=",
                                          ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                          rt.local_defined_function_vec_storage.size(),
                                          u8"/",
                                          rt.local_defined_table_vec_storage.size(),
                                          u8"/",
                                          rt.local_defined_memory_vec_storage.size(),
                                          u8"/",
                                          rt.local_defined_global_vec_storage.size(),
                                          ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                          u8", segments(elem/data)=",
                                          ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                          rt.local_defined_element_vec_storage.size(),
                                          u8"/",
                                          rt.local_defined_data_vec_storage.size(),
                                          ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                          u8". ");

                    // no necessary to check, When constructing the all_module, duplicate names have already been excluded.
                    ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.try_emplace(module_name, ::std::move(rt));

                    break;
                }
                default:
                {
                    // Other module types are not yet representable by `wasm_module_storage_t`.
                    details::verbose_info(u8"initializer: Skip module \"",
                                          ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                          module_name,
                                          ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                          u8"\" (type not supported by runtime storage yet). ");
                    break;
                }
            }
        }

        // Best-effort linking between wasm file modules.
        details::verbose_info(u8"initializer: Resolve imports (best-effort). ");
        details::resolve_imports_for_wasm_file_modules();
        details::error_on_unresolved_imports_after_linking();
        details::verbose_info(u8"initializer: Validate linked import types. ");
        details::validate_wasm_file_module_import_types_after_linking();
        details::verbose_info(u8"initializer: Finalize wasm1 globals. ");
        details::finalize_wasm1_globals_after_linking();
        details::verbose_info(u8"initializer: Finalize wasm1 offsets. ");
        details::finalize_wasm1_offsets_after_linking();
        details::verbose_info(u8"initializer: Apply wasm1 active elem/data segments. ");
        details::apply_wasm1_active_element_and_data_segments_after_linking();

        // finalize time
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
        }

        details::verbose_info(u8"initializer: Runtime initialization done. (time=",
                              ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                              end_time - start_time,
                              ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                              u8"s). ");
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

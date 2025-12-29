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
                    if(curr->imported_ptr == nullptr) [[unlikely]]
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

                    curr = curr->imported_ptr;
                    continue;
                }

                auto const def{curr->defined_ptr};
                if(def == nullptr) [[unlikely]]
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

                auto& def_mut{*const_cast<::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t*>(def)};
                ensure_wasm1_local_defined_global_initialized(def_mut);

                out = ::std::addressof(def_mut.global);
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
                for(auto& g: curr_rt.local_defined_global_vec_storage) { ensure_wasm1_local_defined_global_initialized(g); }
            }
        }

        inline void finalize_wasm1_offsets_after_linking() noexcept
        {
            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage)
            {

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

            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage)
            {
                auto const resolve_exported_module_runtime{
                    [](auto const& import_ptr) constexpr noexcept -> ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const*
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
                    if(export_record->type != module_type_t::exec_wasm && export_record->type != module_type_t::preloaded_wasm) { continue; }
                    if(export_record->storage.wasm_file_export_storage_ptr.binfmt_ver != 1u) [[unlikely]] { continue; }

                    auto const export_ptr{export_record->storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                    if(export_ptr == nullptr || export_ptr->type != external_types::func) [[unlikely]] { continue; }

                    auto const exported_rt{resolve_exported_module_runtime(import_ptr)};
                    if(exported_rt == nullptr) [[unlikely]] { continue; }

                    auto const exported_idx{static_cast<::std::size_t>(export_ptr->storage.func_idx)};
                    auto const imported_count{exported_rt->imported_function_vec_storage.size()};
                    if(exported_idx < imported_count)
                    {
                        imp.imported_ptr = ::std::addressof(exported_rt->imported_function_vec_storage.index_unchecked(exported_idx));
                        imp.defined_ptr = nullptr;
                        imp.is_opposite_side_imported = true;
                    }
                    else
                    {
                        auto const local_idx{exported_idx - imported_count};
                        if(local_idx >= exported_rt->local_defined_function_vec_storage.size()) [[unlikely]] { continue; }
                        imp.imported_ptr = nullptr;
                        imp.defined_ptr = ::std::addressof(exported_rt->local_defined_function_vec_storage.index_unchecked(local_idx));
                        imp.is_opposite_side_imported = false;
                    }
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
                    if(exported_idx < imported_count)
                    {
                        imp.imported_ptr = ::std::addressof(exported_rt->imported_table_vec_storage.index_unchecked(exported_idx));
                        imp.defined_ptr = nullptr;
                        imp.is_opposite_side_imported = true;
                    }
                    else
                    {
                        auto const local_idx{exported_idx - imported_count};
                        if(local_idx >= exported_rt->local_defined_table_vec_storage.size()) [[unlikely]] { continue; }
                        imp.imported_ptr = nullptr;
                        imp.defined_ptr = ::std::addressof(exported_rt->local_defined_table_vec_storage.index_unchecked(local_idx));
                        imp.is_opposite_side_imported = false;
                    }
                }

                for(auto& imp: curr_rt.imported_memory_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    auto const export_record{resolve_export_record(import_ptr)};
                    if(export_record == nullptr) [[unlikely]] { continue; }
                    if(export_record->type != module_type_t::exec_wasm && export_record->type != module_type_t::preloaded_wasm) { continue; }
                    if(export_record->storage.wasm_file_export_storage_ptr.binfmt_ver != 1u) [[unlikely]] { continue; }

                    auto const export_ptr{export_record->storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                    if(export_ptr == nullptr || export_ptr->type != external_types::memory) [[unlikely]] { continue; }

                    auto const exported_rt{resolve_exported_module_runtime(import_ptr)};
                    if(exported_rt == nullptr) [[unlikely]] { continue; }

                    auto const exported_idx{static_cast<::std::size_t>(export_ptr->storage.memory_idx)};
                    auto const imported_count{exported_rt->imported_memory_vec_storage.size()};
                    if(exported_idx < imported_count)
                    {
                        imp.imported_ptr = ::std::addressof(exported_rt->imported_memory_vec_storage.index_unchecked(exported_idx));
                        imp.defined_ptr = nullptr;
                        imp.is_opposite_side_imported = true;
                    }
                    else
                    {
                        auto const local_idx{exported_idx - imported_count};
                        if(local_idx >= exported_rt->local_defined_memory_vec_storage.size()) [[unlikely]] { continue; }
                        imp.imported_ptr = nullptr;
                        imp.defined_ptr = ::std::addressof(exported_rt->local_defined_memory_vec_storage.index_unchecked(local_idx));
                        imp.is_opposite_side_imported = false;
                    }
                }

                for(auto& imp: curr_rt.imported_global_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    auto const export_record{resolve_export_record(import_ptr)};
                    if(export_record == nullptr) [[unlikely]] { continue; }
                    if(export_record->type != module_type_t::exec_wasm && export_record->type != module_type_t::preloaded_wasm) { continue; }
                    if(export_record->storage.wasm_file_export_storage_ptr.binfmt_ver != 1u) [[unlikely]] { continue; }

                    auto const export_ptr{export_record->storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                    if(export_ptr == nullptr || export_ptr->type != external_types::global) [[unlikely]] { continue; }

                    auto const exported_rt{resolve_exported_module_runtime(import_ptr)};
                    if(exported_rt == nullptr) [[unlikely]] { continue; }

                    auto const exported_idx{static_cast<::std::size_t>(export_ptr->storage.global_idx)};
                    auto const imported_count{exported_rt->imported_global_vec_storage.size()};
                    if(exported_idx < imported_count)
                    {
                        imp.imported_ptr = ::std::addressof(exported_rt->imported_global_vec_storage.index_unchecked(exported_idx));
                        imp.defined_ptr = nullptr;
                        imp.is_opposite_side_imported = true;
                    }
                    else
                    {
                        auto const local_idx{exported_idx - imported_count};
                        if(local_idx >= exported_rt->local_defined_global_vec_storage.size()) [[unlikely]] { continue; }
                        imp.imported_ptr = nullptr;
                        imp.defined_ptr = ::std::addressof(exported_rt->local_defined_global_vec_storage.index_unchecked(local_idx));
                        imp.is_opposite_side_imported = false;
                    }
                }
            }
        }
    }  // namespace details

    inline void initialize_runtime() noexcept
    {
        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Initialize the runtime environment for the WASM module. ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                u8"[",
                                local(::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::realtime)),
                                u8"] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(verbose)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
        }

        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.clear();
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.reserve(::uwvm2::uwvm::wasm::storage::all_module.size());

        for(auto const& [module_name, mod]: ::uwvm2::uwvm::wasm::storage::all_module)
        {
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t rt{};

            switch(mod.type)
            {
                case ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm:
                case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm:
                {
                    if(mod.module_storage_ptr.wf == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    details::initialize_from_wasm_file(*mod.module_storage_ptr.wf, rt);
                    break;
                }
                default:
                {
                    // Other module types are not yet representable by `wasm_module_storage_t`.
                    break;
                }
            }

            ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.try_emplace(module_name, ::std::move(rt));
            // no necessary to check, When constructing the all_module, duplicate names have already been excluded.
        }

        // Best-effort linking between wasm file modules.
        details::resolve_imports_for_wasm_file_modules();
        details::finalize_wasm1_globals_after_linking();
        details::finalize_wasm1_offsets_after_linking();
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

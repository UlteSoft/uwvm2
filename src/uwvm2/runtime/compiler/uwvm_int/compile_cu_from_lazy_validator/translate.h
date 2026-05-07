/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
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
# include <atomic>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <exception>
# include <limits>
# include <memory>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/thread/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/validation/error/impl.h>
# include <uwvm2/validation/standard/wasm1/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/optable/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator
{
    using wasm1_code = ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic;
    using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
    using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
    using code_validation_error_code = ::uwvm2::validation::error::code_validation_error_code;

    using runtime_module_storage_t = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
    using parser_module_storage_t = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t;
    using full_function_symbol_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t;
    using local_func_storage_t = ::uwvm2::runtime::compiler::uwvm_int::optable::local_func_storage_t;

    enum class lazy_validation_mode : unsigned
    {
        validate_on_lazy_compile,
        assume_full_code_verified
    };

    enum class lazy_execution_unit_kind : unsigned
    {
        function,
        block,
        loop,
        if_
    };

    enum class lazy_compile_unit_kind : unsigned
    {
        function,
        execution_unit,
        code_size_group
    };

    enum class lazy_materialization_scope : unsigned
    {
        whole_function,
        execution_unit_range
    };

    enum class lazy_execution_unit_split_policy_t : unsigned
    {
        function_only,
        structured_control
    };

    enum class lazy_compile_unit_split_policy_t : unsigned
    {
        function,
        execution_unit,
        code_size
    };

    struct lazy_split_config
    {
        lazy_execution_unit_split_policy_t eu_policy{lazy_execution_unit_split_policy_t::structured_control};
        lazy_compile_unit_split_policy_t cu_policy{lazy_compile_unit_split_policy_t::code_size};
        ::std::size_t cu_code_size{4096uz};
    };

    struct lazy_execution_unit_storage_t
    {
        ::std::size_t function_index{};
        ::std::size_t local_function_index{};
        ::std::size_t parent_eu_index{SIZE_MAX};
        ::std::size_t depth{};
        ::std::byte const* code_begin{};
        ::std::byte const* code_end{};
        ::std::size_t code_offset{};
        ::std::size_t code_size{};
        lazy_execution_unit_kind kind{lazy_execution_unit_kind::function};
    };

    struct lazy_compile_unit_storage_t
    {
        ::uwvm2::utils::thread::lazy_compile_unit_state state{};
        ::std::size_t function_index{};
        ::std::size_t local_function_index{};
        ::std::size_t begin_eu_index{};
        ::std::size_t end_eu_index{};
        ::std::byte const* code_begin{};
        ::std::byte const* code_end{};
        ::std::size_t code_offset{};
        ::std::size_t code_size{};
        lazy_compile_unit_kind kind{lazy_compile_unit_kind::function};
        lazy_materialization_scope materialization_scope{lazy_materialization_scope::whole_function};
    };

    struct lazy_function_storage_t
    {
        ::uwvm2::utils::thread::lazy_compile_unit_state materialization_state{};
        ::std::size_t function_index{};
        ::std::size_t local_function_index{};
        ::std::size_t first_eu_index{};
        ::std::size_t eu_count{};
        ::std::size_t first_cu_index{};
        ::std::size_t cu_count{};
        ::std::size_t primary_cu_index{SIZE_MAX};
    };

    struct lazy_module_storage_t
    {
        full_function_symbol_t compiled{};
        ::uwvm2::utils::container::vector<lazy_function_storage_t> functions{};
        ::uwvm2::utils::container::vector<lazy_execution_unit_storage_t> execution_units{};
        ::uwvm2::utils::container::vector<lazy_compile_unit_storage_t> compile_units{};
    };

    struct lazy_compile_options
    {
        ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option compile_options{};
        parser_module_storage_t const* validator_module_storage{};
        lazy_validation_mode validation_mode{lazy_validation_mode::validate_on_lazy_compile};
    };

    struct lazy_compile_request_context
    {
        runtime_module_storage_t const* curr_module{};
        lazy_module_storage_t* lazy_storage{};
        lazy_compile_options options{};
        ::std::size_t compile_unit_index{};
        ::uwvm2::validation::error::code_validation_error_impl* err{};
    };

    namespace details
    {
        struct lazy_split_control_frame
        {
            ::std::size_t eu_index{};
            lazy_execution_unit_kind kind{lazy_execution_unit_kind::function};
        };

        [[nodiscard]] inline constexpr ::std::size_t active_parent_eu_index(lazy_split_control_frame const& frame) noexcept
        {
            return frame.eu_index;
        }

        [[nodiscard]] inline constexpr ::std::size_t byte_offset(::std::byte const* base, ::std::byte const* curr) noexcept
        { return static_cast<::std::size_t>(curr - base); }

        inline void fail_lazy_split(::std::byte const* op_begin,
                                    code_validation_error_code ec,
                                    ::uwvm2::validation::error::code_validation_error_impl& err,
                                    ::fast_io::parse_code pc = ::fast_io::parse_code::invalid) UWVM_THROWS
        {
            err.err_curr = op_begin;
            err.err_code = ec;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(pc);
        }

        template <typename T>
        inline T read_leb128_immediate(::std::byte const*& code_curr,
                                       ::std::byte const* code_end,
                                       ::std::byte const* op_begin,
                                       code_validation_error_code ec,
                                       ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            T value;  // No initialization necessary

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const [next, parse_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                  ::fast_io::mnp::leb128_get(value))};
            if(parse_err != ::fast_io::parse_code::ok) [[unlikely]] { fail_lazy_split(op_begin, ec, err, parse_err); }

            code_curr = reinterpret_cast<::std::byte const*>(next);
            return value;
        }

        inline void skip_fixed_immediate(::std::byte const*& code_curr,
                                         ::std::byte const* code_end,
                                         ::std::byte const* op_begin,
                                         ::std::size_t bytes,
                                         ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            auto const remaining{static_cast<::std::size_t>(code_end - code_curr)};
            if(bytes > remaining) [[unlikely]]
            {
                fail_lazy_split(op_begin, code_validation_error_code::invalid_const_immediate, err, ::fast_io::parse_code::end_of_file);
            }
            code_curr += bytes;
        }

        inline ::std::size_t append_execution_unit(lazy_module_storage_t& storage,
                                                   ::std::size_t function_index,
                                                   ::std::size_t local_function_index,
                                                   ::std::size_t parent_eu_index,
                                                   ::std::size_t depth,
                                                   ::std::byte const* function_code_begin,
                                                   ::std::byte const* code_begin,
                                                   ::std::byte const* code_end,
                                                   lazy_execution_unit_kind kind)
        {
            auto const eu_index{storage.execution_units.size()};
            auto const size{code_end == nullptr ? 0uz : static_cast<::std::size_t>(code_end - code_begin)};
            storage.execution_units.push_back({.function_index = function_index,
                                               .local_function_index = local_function_index,
                                               .parent_eu_index = parent_eu_index,
                                               .depth = depth,
                                               .code_begin = code_begin,
                                               .code_end = code_end,
                                               .code_offset = byte_offset(function_code_begin, code_begin),
                                               .code_size = size,
                                               .kind = kind});
            return eu_index;
        }

        inline void set_execution_unit_end(lazy_module_storage_t& storage, ::std::size_t eu_index, ::std::byte const* code_end) noexcept
        {
            auto& eu{storage.execution_units.index_unchecked(eu_index)};
            eu.code_end = code_end;
            eu.code_size = static_cast<::std::size_t>(code_end - eu.code_begin);
        }

        inline ::std::size_t append_compile_unit_from_eu_range(lazy_module_storage_t& storage,
                                                               lazy_function_storage_t const& fn,
                                                               ::std::size_t begin_eu_index,
                                                               ::std::size_t end_eu_index,
                                                               lazy_compile_unit_kind kind,
                                                               lazy_materialization_scope scope)
        {
            auto const cu_index{storage.compile_units.size()};
            auto const& first_eu{storage.execution_units.index_unchecked(begin_eu_index)};
            auto const code_begin{first_eu.code_begin};
            auto code_end{first_eu.code_end};
            for(::std::size_t i{begin_eu_index + 1uz}; i != end_eu_index; ++i)
            {
                auto const curr_end{storage.execution_units.index_unchecked(i).code_end};
                code_end = curr_end > code_end ? curr_end : code_end;
            }
            storage.compile_units.push_back({.function_index = fn.function_index,
                                             .local_function_index = fn.local_function_index,
                                             .begin_eu_index = begin_eu_index,
                                             .end_eu_index = end_eu_index,
                                             .code_begin = code_begin,
                                             .code_end = code_end,
                                             .code_offset = first_eu.code_offset,
                                             .code_size = static_cast<::std::size_t>(code_end - code_begin),
                                             .kind = kind,
                                             .materialization_scope = scope});
            return cu_index;
        }

        inline void append_function_compile_units(lazy_module_storage_t& storage, lazy_function_storage_t& fn, lazy_split_config cfg)
        {
            fn.first_cu_index = storage.compile_units.size();

            if(cfg.cu_policy == lazy_compile_unit_split_policy_t::function || fn.eu_count <= 1uz)
            {
                fn.primary_cu_index = append_compile_unit_from_eu_range(storage,
                                                                        fn,
                                                                        fn.first_eu_index,
                                                                        fn.first_eu_index + fn.eu_count,
                                                                        lazy_compile_unit_kind::function,
                                                                        lazy_materialization_scope::whole_function);
                fn.cu_count = storage.compile_units.size() - fn.first_cu_index;
                return;
            }

            auto const is_candidate{[&](lazy_execution_unit_storage_t const& eu) constexpr noexcept
                                    {
                                        if(eu.kind == lazy_execution_unit_kind::function) { return false; }
                                        return true;
                                    }};

            if(cfg.cu_policy == lazy_compile_unit_split_policy_t::execution_unit)
            {
                for(::std::size_t i{fn.first_eu_index}; i != fn.first_eu_index + fn.eu_count; ++i)
                {
                    if(!is_candidate(storage.execution_units.index_unchecked(i))) { continue; }
                    auto const cu{append_compile_unit_from_eu_range(storage,
                                                                    fn,
                                                                    i,
                                                                    i + 1uz,
                                                                    lazy_compile_unit_kind::execution_unit,
                                                                    lazy_materialization_scope::whole_function)};
                    if(fn.primary_cu_index == SIZE_MAX) { fn.primary_cu_index = cu; }
                }
            }
            else
            {
                auto const target_size{cfg.cu_code_size == 0uz ? 1uz : cfg.cu_code_size};
                ::std::size_t group_begin{SIZE_MAX};
                ::std::size_t group_end{SIZE_MAX};
                ::std::size_t group_size{};

                auto const flush_group{[&]()
                                       {
                                           if(group_begin == SIZE_MAX) { return; }
                                           auto const cu{append_compile_unit_from_eu_range(storage,
                                                                                           fn,
                                                                                           group_begin,
                                                                                           group_end,
                                                                                           lazy_compile_unit_kind::code_size_group,
                                                                                           lazy_materialization_scope::whole_function)};
                                           if(fn.primary_cu_index == SIZE_MAX) { fn.primary_cu_index = cu; }
                                           group_begin = SIZE_MAX;
                                           group_end = SIZE_MAX;
                                           group_size = 0uz;
                                       }};

                for(::std::size_t i{fn.first_eu_index}; i != fn.first_eu_index + fn.eu_count; ++i)
                {
                    auto const& eu{storage.execution_units.index_unchecked(i)};
                    if(!is_candidate(eu)) { continue; }

                    if(group_begin == SIZE_MAX)
                    {
                        group_begin = i;
                        group_end = i + 1uz;
                        group_size = eu.code_size;
                    }
                    else if(group_size >= target_size)
                    {
                        flush_group();
                        group_begin = i;
                        group_end = i + 1uz;
                        group_size = eu.code_size;
                    }
                    else
                    {
                        group_end = i + 1uz;
                        if(eu.code_size > (::std::numeric_limits<::std::size_t>::max() - group_size)) [[unlikely]]
                        {
                            group_size = ::std::numeric_limits<::std::size_t>::max();
                        }
                        else
                        {
                            group_size += eu.code_size;
                        }
                    }
                }

                flush_group();
            }

            if(fn.primary_cu_index == SIZE_MAX)
            {
                fn.primary_cu_index = append_compile_unit_from_eu_range(storage,
                                                                        fn,
                                                                        fn.first_eu_index,
                                                                        fn.first_eu_index + fn.eu_count,
                                                                        lazy_compile_unit_kind::function,
                                                                        lazy_materialization_scope::whole_function);
            }

            fn.cu_count = storage.compile_units.size() - fn.first_cu_index;
        }

        inline void skip_wasm1_non_structural_immediates(::std::byte const*& code_curr,
                                                         ::std::byte const* code_end,
                                                         ::std::byte const* op_begin,
                                                         wasm1_code curr_opbase,
                                                         ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            switch(curr_opbase)
            {
                case wasm1_code::br:
                case wasm1_code::br_if:
                {
                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_label_index, err);
                    return;
                }
                case wasm1_code::br_table:
                {
                    auto const target_count{
                        read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_label_index, err)};

                    auto const remaining_bytes{static_cast<::std::size_t>(code_end - code_curr)};
                    auto const target_count_uz{static_cast<::std::size_t>(target_count)};
                    auto const target_count_exceeds_size_t{static_cast<wasm_u32>(target_count_uz) != target_count};
                    auto const target_count_plus_default_overflows{!target_count_exceeds_size_t &&
                                                                   target_count_uz == ::std::numeric_limits<::std::size_t>::max()};
                    if(target_count_exceeds_size_t || target_count_plus_default_overflows || target_count_uz + 1uz > remaining_bytes) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.br_table_target_count_exceeds_remaining_bytes.target_count = target_count;
                        err.err_selectable.br_table_target_count_exceeds_remaining_bytes.remaining_bytes = remaining_bytes;
                        err.err_selectable.br_table_target_count_exceeds_remaining_bytes.max_target_count =
                            static_cast<wasm_u32>(remaining_bytes == 0uz ? 0uz : remaining_bytes - 1uz);
                        err.err_code = code_validation_error_code::br_table_target_count_exceeds_remaining_bytes;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    for(::std::size_t i{}; i != target_count_uz + 1uz; ++i)
                    {
                        (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_label_index, err);
                    }
                    return;
                }
                case wasm1_code::call:
                {
                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_function_index_encoding, err);
                    return;
                }
                case wasm1_code::call_indirect:
                {
                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_type_index, err);
                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_table_index, err);
                    return;
                }
                case wasm1_code::local_get:
                case wasm1_code::local_set:
                case wasm1_code::local_tee:
                {
                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_local_index, err);
                    return;
                }
                case wasm1_code::global_get:
                case wasm1_code::global_set:
                {
                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_global_index, err);
                    return;
                }
                case wasm1_code::i32_load:
                case wasm1_code::i64_load:
                case wasm1_code::f32_load:
                case wasm1_code::f64_load:
                case wasm1_code::i32_load8_s:
                case wasm1_code::i32_load8_u:
                case wasm1_code::i32_load16_s:
                case wasm1_code::i32_load16_u:
                case wasm1_code::i64_load8_s:
                case wasm1_code::i64_load8_u:
                case wasm1_code::i64_load16_s:
                case wasm1_code::i64_load16_u:
                case wasm1_code::i64_load32_s:
                case wasm1_code::i64_load32_u:
                case wasm1_code::i32_store:
                case wasm1_code::i64_store:
                case wasm1_code::f32_store:
                case wasm1_code::f64_store:
                case wasm1_code::i32_store8:
                case wasm1_code::i32_store16:
                case wasm1_code::i64_store8:
                case wasm1_code::i64_store16:
                case wasm1_code::i64_store32:
                {
                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_memarg_align, err);
                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_memarg_offset, err);
                    return;
                }
                case wasm1_code::memory_size:
                case wasm1_code::memory_grow:
                {
                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_memory_index, err);
                    return;
                }
                case wasm1_code::i32_const:
                {
                    (void)read_leb128_immediate<wasm_i32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_const_immediate, err);
                    return;
                }
                case wasm1_code::i64_const:
                {
                    (void)read_leb128_immediate<wasm_i64>(code_curr, code_end, op_begin, code_validation_error_code::invalid_const_immediate, err);
                    return;
                }
                case wasm1_code::f32_const:
                {
                    skip_fixed_immediate(code_curr, code_end, op_begin, sizeof(wasm_f32), err);
                    return;
                }
                case wasm1_code::f64_const:
                {
                    skip_fixed_immediate(code_curr, code_end, op_begin, sizeof(wasm_f64), err);
                    return;
                }
                default:
                {
                    auto const op_byte{static_cast<unsigned>(curr_opbase)};
                    if((op_byte >= static_cast<unsigned>(wasm1_code::i32_eqz) && op_byte <= static_cast<unsigned>(wasm1_code::f64_reinterpret_i64)) ||
                       curr_opbase == wasm1_code::unreachable || curr_opbase == wasm1_code::nop || curr_opbase == wasm1_code::drop ||
                       curr_opbase == wasm1_code::select || curr_opbase == wasm1_code::return_)
                    {
                        return;
                    }

                    err.err_curr = op_begin;
                    err.err_selectable.u8 = static_cast<::std::uint_least8_t>(op_byte);
                    err.err_code = code_validation_error_code::illegal_opbase;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }
        }

        inline void build_lazy_function_execution_units(runtime_module_storage_t const& curr_module,
                                                        lazy_module_storage_t& storage,
                                                        ::std::size_t local_function_index,
                                                        lazy_split_config cfg,
                                                        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            auto const import_func_count{curr_module.imported_function_vec_storage.size()};
            auto const function_index{import_func_count + local_function_index};
            auto const& curr_local_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_index)};
            auto const& curr_code{*curr_local_func.wasm_code_ptr};
            auto const code_begin{reinterpret_cast<::std::byte const*>(curr_code.body.expr_begin)};
            auto const code_end{reinterpret_cast<::std::byte const*>(curr_code.body.code_end)};

            auto& fn{storage.functions.index_unchecked(local_function_index)};
            fn.function_index = function_index;
            fn.local_function_index = local_function_index;
            fn.first_eu_index = storage.execution_units.size();

            auto const function_eu_index{append_execution_unit(storage,
                                                               function_index,
                                                               local_function_index,
                                                               SIZE_MAX,
                                                               0uz,
                                                               code_begin,
                                                               code_begin,
                                                               nullptr,
                                                               lazy_execution_unit_kind::function)};

            if(cfg.eu_policy == lazy_execution_unit_split_policy_t::function_only)
            {
                set_execution_unit_end(storage, function_eu_index, code_end);
                fn.eu_count = storage.execution_units.size() - fn.first_eu_index;
                append_function_compile_units(storage, fn, cfg);
                return;
            }

            ::uwvm2::utils::container::vector<lazy_split_control_frame> control_stack{};
            control_stack.reserve(32uz);
            control_stack.push_back({.eu_index = function_eu_index, .kind = lazy_execution_unit_kind::function});

            auto code_curr{code_begin};

            for(;;)
            {
                if(code_curr == code_end) [[unlikely]]
                {
                    // [... ] | (end)
                    // [safe] | unsafe (could be the section_end)
                    //          ^^ code_curr

                    // Validation completes when the end is reached, so this condition can never be met. If it were met, it would indicate a missing end.
                    fail_lazy_split(code_curr, code_validation_error_code::missing_end, err);
                }

                // opbase ...
                // [safe] unsafe (could be the section_end)
                // ^^ code_curr

                auto const op_begin{code_curr};

                wasm1_code curr_opbase;  // No initialization necessary
                ::std::memcpy(::std::addressof(curr_opbase), code_curr, sizeof(wasm1_code));
                ++code_curr;

                switch(curr_opbase)
                {
                    case wasm1_code::block:
                    case wasm1_code::loop:
                    case wasm1_code::if_:
                    {
                        // block blocktype ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        if(code_curr == code_end) [[unlikely]]
                        {
                            fail_lazy_split(op_begin, code_validation_error_code::missing_block_type, err, ::fast_io::parse_code::end_of_file);
                        }

                        // block blocktype ...
                        // [     safe    ] unsafe (could be the section_end)
                        //       ^^ code_curr

                        ++code_curr;

                        // block blocktype ...
                        // [     safe    ] unsafe (could be the section_end)
                        //                 ^^ code_curr

                        auto const parent_eu_index{active_parent_eu_index(control_stack.back_unchecked())};
                        auto const depth{control_stack.size()};
                        auto const kind{curr_opbase == wasm1_code::block  ? lazy_execution_unit_kind::block
                                        : curr_opbase == wasm1_code::loop ? lazy_execution_unit_kind::loop
                                                                          : lazy_execution_unit_kind::if_};
                        auto const eu_index{
                            append_execution_unit(storage, function_index, local_function_index, parent_eu_index, depth, code_begin, op_begin, nullptr, kind)};
                        control_stack.push_back({.eu_index = eu_index, .kind = kind});
                        break;
                    }
                    case wasm1_code::else_:
                    {
                        // else   ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        if(control_stack.empty() || control_stack.back_unchecked().kind != lazy_execution_unit_kind::if_) [[unlikely]]
                        {
                            fail_lazy_split(op_begin, code_validation_error_code::illegal_else, err);
                        }
                        break;
                    }
                    case wasm1_code::end:
                    {
                        // end    ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        if(control_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(curr_opbase);
                            err.err_code = code_validation_error_code::illegal_opbase;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const frame{control_stack.back_unchecked()};
                        set_execution_unit_end(storage, frame.eu_index, code_curr);
                        control_stack.pop_back_unchecked();

                        if(frame.kind == lazy_execution_unit_kind::function)
                        {
                            if(code_curr != code_end) [[unlikely]] { fail_lazy_split(op_begin, code_validation_error_code::trailing_code_after_end, err); }

                            fn.eu_count = storage.execution_units.size() - fn.first_eu_index;
                            append_function_compile_units(storage, fn, cfg);
                            return;
                        }
                        break;
                    }
                    case wasm1_code::return_:
                    case wasm1_code::unreachable:
                    {
                        break;
                    }
                    default:
                    {
                        skip_wasm1_non_structural_immediates(code_curr, code_end, op_begin, curr_opbase, err);
                        break;
                    }
                }
            }
        }

        [[nodiscard]] inline auto
            match_trivial_call_inline_body(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_code_t const* code_ptr) noexcept
        {
            return ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::match_trivial_call_inline_body(code_ptr);
        }

        inline void fill_lazy_local_defined_call_info(runtime_module_storage_t const& curr_module,
                                                      ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option const& options,
                                                      full_function_symbol_t& storage) noexcept
        {
            auto const import_func_count{curr_module.imported_function_vec_storage.size()};
            auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};

            storage.local_funcs.clear();
            storage.local_funcs.resize(local_func_count);
            storage.local_defined_call_info.clear();
            storage.local_defined_call_info.resize(local_func_count);

            for(::std::size_t i{}; i != local_func_count; ++i)
            {
                auto& info{storage.local_defined_call_info.index_unchecked(i)};
                info.module_id = options.curr_wasm_id;
                info.function_index = import_func_count + i;
            }

#include "../compile_all_from_uwvm/translate/single_func_call_info.h"
        }

        inline constexpr void aggregate_lazy_local_function_storage(full_function_symbol_t& storage) noexcept
        {
            storage.local_count = 0uz;
            storage.local_bytes_max = 0uz;
            storage.local_bytes_zeroinit_end = 0uz;
            storage.operand_stack_max = 0uz;
            storage.operand_stack_byte_max = 0uz;

            for(auto const& local_func: storage.local_funcs)
            {
                storage.local_count = local_func.local_count > storage.local_count ? local_func.local_count : storage.local_count;
                storage.local_bytes_max = local_func.local_bytes_max > storage.local_bytes_max ? local_func.local_bytes_max : storage.local_bytes_max;
                storage.local_bytes_zeroinit_end = local_func.local_bytes_zeroinit_end > storage.local_bytes_zeroinit_end ? local_func.local_bytes_zeroinit_end
                                                                                                                          : storage.local_bytes_zeroinit_end;
                storage.operand_stack_max = local_func.operand_stack_max > storage.operand_stack_max ? local_func.operand_stack_max : storage.operand_stack_max;
                storage.operand_stack_byte_max =
                    local_func.operand_stack_byte_max > storage.operand_stack_byte_max ? local_func.operand_stack_byte_max : storage.operand_stack_byte_max;
            }
        }

        inline void mark_function_compile_units_state(lazy_module_storage_t& storage,
                                                      lazy_function_storage_t& fn,
                                                      ::uwvm2::utils::thread::lazy_compile_state state) noexcept
        {
            fn.materialization_state.state.store(state, ::std::memory_order_release);
            for(::std::size_t i{fn.first_cu_index}; i != fn.first_cu_index + fn.cu_count; ++i)
            {
                storage.compile_units.index_unchecked(i).state.state.store(state, ::std::memory_order_release);
            }
        }

        inline void validate_function_if_needed(runtime_module_storage_t const& curr_module,
                                                lazy_compile_options const& options,
                                                ::std::size_t local_function_index,
                                                ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            if(options.validation_mode == lazy_validation_mode::validate_on_lazy_compile)
            {
                // Lazy validation must use the standard wasm1 validator directly. This keeps the lazy path aligned with
                // src/uwvm2/validation/standard/wasm1/validator.h, including its memory-safety boundary comments and parse checks.
                if(options.validator_module_storage == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const import_func_count{curr_module.imported_function_vec_storage.size()};
                auto const function_index{import_func_count + local_function_index};
                auto const& curr_local_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_index)};
                auto const& curr_code{*curr_local_func.wasm_code_ptr};
                auto const code_begin{reinterpret_cast<::std::byte const*>(curr_code.body.expr_begin)};
                auto const code_end{reinterpret_cast<::std::byte const*>(curr_code.body.code_end)};

                ::uwvm2::validation::standard::wasm1::validate_code(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version{},
                                                                    *options.validator_module_storage,
                                                                    function_index,
                                                                    code_begin,
                                                                    code_end,
                                                                    err);
            }
        }

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
        inline void compile_lazy_local_function(runtime_module_storage_t const& curr_module,
                                                lazy_module_storage_t& storage,
                                                lazy_compile_options& options,
                                                ::std::size_t local_function_index,
                                                ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            validate_function_if_needed(curr_module, options, local_function_index, err);

            ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::compile_all_from_uwvm_local_func<CompileOption>(curr_module,
                                                                                                                                  options.compile_options,
                                                                                                                                  storage.compiled,
                                                                                                                                  local_function_index,
                                                                                                                                  err);
        }

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
        inline void lazy_compile_request_entry(void* user_data) noexcept
        {
            auto const ctx{static_cast<lazy_compile_request_context*>(user_data)};
            if(ctx == nullptr || ctx->curr_module == nullptr || ctx->lazy_storage == nullptr) [[unlikely]] { return; }

            auto& storage{*ctx->lazy_storage};
            if(ctx->compile_unit_index >= storage.compile_units.size()) [[unlikely]] { return; }

            auto& cu{storage.compile_units.index_unchecked(ctx->compile_unit_index)};
            if(cu.local_function_index >= storage.functions.size()) [[unlikely]] { return; }
            auto& fn{storage.functions.index_unchecked(cu.local_function_index)};

            ::uwvm2::validation::error::code_validation_error_impl local_err{};
            auto& err{ctx->err == nullptr ? local_err : *ctx->err};

# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                compile_lazy_local_function<CompileOption>(*ctx->curr_module, storage, ctx->options, cu.local_function_index, err);
                mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::compiled);
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(...)
            {
                mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::failed);
            }
# endif
        }
    }  // namespace details

    inline lazy_module_storage_t initialize_lazy_module_storage(runtime_module_storage_t const& curr_module,
                                                                ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option const& options,
                                                                ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                lazy_split_config split_config = {}) UWVM_THROWS
    {
        lazy_module_storage_t storage{};

        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        details::fill_lazy_local_defined_call_info(curr_module, options, storage.compiled);

        storage.functions.clear();
        storage.functions.resize(local_func_count);
        storage.execution_units.clear();
        storage.compile_units.clear();
        storage.execution_units.reserve(local_func_count);
        storage.compile_units.reserve(local_func_count);

        for(::std::size_t local_function_index{}; local_function_index != local_func_count; ++local_function_index)
        {
            details::build_lazy_function_execution_units(curr_module, storage, local_function_index, split_config, err);
        }
        return storage;
    }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    inline void compile_cu_from_lazy_validator(runtime_module_storage_t const& curr_module,
                                               lazy_module_storage_t& storage,
                                               lazy_compile_options& options,
                                               ::std::size_t compile_unit_index,
                                               ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        if(compile_unit_index >= storage.compile_units.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto& cu{storage.compile_units.index_unchecked(compile_unit_index)};
        if(cu.local_function_index >= storage.functions.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto& fn{storage.functions.index_unchecked(cu.local_function_index)};

        for(;;)
        {
            auto const st{fn.materialization_state.state.load(::std::memory_order_acquire)};
            if(st == ::uwvm2::utils::thread::lazy_compile_state::compiled) { return; }
            if(st == ::uwvm2::utils::thread::lazy_compile_state::failed) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(st == ::uwvm2::utils::thread::lazy_compile_state::uncompiled)
            {
                auto expected{::uwvm2::utils::thread::lazy_compile_state::uncompiled};
                if(fn.materialization_state.state.compare_exchange_strong(expected,
                                                                          ::uwvm2::utils::thread::lazy_compile_state::compiling,
                                                                          ::std::memory_order_acq_rel,
                                                                          ::std::memory_order_acquire))
                {
                    break;
                }
                continue;
            }

            ::uwvm2::utils::thread::lazy_compile_thread_yield();
        }

        details::compile_lazy_local_function<CompileOption>(curr_module, storage, options, cu.local_function_index, err);
        details::mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::compiled);
    }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    [[nodiscard]] inline ::uwvm2::utils::thread::lazy_compile_request make_lazy_compile_request(lazy_compile_request_context & ctx,
                                                                                                unsigned priority = 0u) noexcept
    {
        if(ctx.lazy_storage == nullptr || ctx.compile_unit_index >= ctx.lazy_storage->compile_units.size()) [[unlikely]] { return {}; }

        auto& cu{ctx.lazy_storage->compile_units.index_unchecked(ctx.compile_unit_index)};
        if(cu.local_function_index >= ctx.lazy_storage->functions.size()) [[unlikely]] { return {}; }
        auto& fn{ctx.lazy_storage->functions.index_unchecked(cu.local_function_index)};

        auto* unit{cu.materialization_scope == lazy_materialization_scope::whole_function ? ::std::addressof(fn.materialization_state)
                                                                                          : ::std::addressof(cu.state)};
        return {.unit = unit, .compile = details::lazy_compile_request_entry<CompileOption>, .user_data = ::std::addressof(ctx), .priority = priority};
    }
}
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

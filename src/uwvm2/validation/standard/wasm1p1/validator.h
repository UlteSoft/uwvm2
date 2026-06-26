/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     antecedent dependency: WebAssembly Release 1.0 (2019-07-20)
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-07-07
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
# include <algorithm>
# include <climits>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
# include <limits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/intrinsics/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/utils/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/validation/error/impl.h>
# include <uwvm2/validation/concepts/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm1p1
{
    using wasm1_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::mvp_op_basic;
    using wasm1p1_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_basic;
    using wasm1p1_numeric_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_numeric;
    using wasm1p1_simd_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_simd;
    using wasm_byte = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte;
    using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;

    inline constexpr wasm_byte opcode_byte(wasm1p1_code opcode) noexcept { return static_cast<wasm_byte>(opcode); }
    inline constexpr wasm_u32 opcode_u32(wasm1p1_code opcode) noexcept { return static_cast<wasm_u32>(opcode_byte(opcode)); }

    struct wasm1p1_code_version
    {
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using operand_stack_value_type = ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct operand_stack_storage_t
    {
        operand_stack_value_type<Fs...> type{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using operand_stack_type = ::uwvm2::utils::container::vector<operand_stack_storage_t<Fs...>>;

    template <typename T>
    struct fast_io_native_typed_global_allocator_guard
    {
        using allocator = ::fast_io::native_typed_global_allocator<T>;
        T* ptr{};

        inline constexpr fast_io_native_typed_global_allocator_guard() noexcept = default;

        inline constexpr fast_io_native_typed_global_allocator_guard(T* o_ptr) noexcept : ptr{o_ptr} {}

        inline constexpr fast_io_native_typed_global_allocator_guard(fast_io_native_typed_global_allocator_guard const&) = delete;
        inline constexpr fast_io_native_typed_global_allocator_guard& operator= (fast_io_native_typed_global_allocator_guard const&) = delete;

        inline constexpr fast_io_native_typed_global_allocator_guard(fast_io_native_typed_global_allocator_guard&& other) noexcept :
            ptr{::std::exchange(other.ptr, nullptr)}
        {
        }

        inline constexpr fast_io_native_typed_global_allocator_guard& operator= (fast_io_native_typed_global_allocator_guard&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            // The deallocator performs internal null pointer checks.
            allocator::deallocate(this->ptr);
            this->ptr = ::std::exchange(other.ptr, nullptr);

            return *this;
        }

        inline constexpr ~fast_io_native_typed_global_allocator_guard()
        {
            // The deallocator performs internal null pointer checks.
            allocator::deallocate(this->ptr);
        }
    };

    template <typename T>
    struct fast_io_native_typed_thread_local_allocator_guard
    {
        using allocator = ::fast_io::native_typed_thread_local_allocator<T>;
        T* ptr{};

        inline constexpr fast_io_native_typed_thread_local_allocator_guard() noexcept = default;

        inline constexpr fast_io_native_typed_thread_local_allocator_guard(T* o_ptr) noexcept : ptr{o_ptr} {}

        inline constexpr fast_io_native_typed_thread_local_allocator_guard(fast_io_native_typed_thread_local_allocator_guard const&) = delete;
        inline constexpr fast_io_native_typed_thread_local_allocator_guard& operator= (fast_io_native_typed_thread_local_allocator_guard const&) = delete;

        inline constexpr fast_io_native_typed_thread_local_allocator_guard(fast_io_native_typed_thread_local_allocator_guard&& other) noexcept :
            ptr{::std::exchange(other.ptr, nullptr)}
        {
        }

        inline constexpr fast_io_native_typed_thread_local_allocator_guard& operator= (fast_io_native_typed_thread_local_allocator_guard&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            // The deallocator performs internal null pointer checks.
            allocator::deallocate(this->ptr);
            this->ptr = ::std::exchange(other.ptr, nullptr);

            return *this;
        }

        inline constexpr ~fast_io_native_typed_thread_local_allocator_guard()
        {
            // The deallocator performs internal null pointer checks.
            allocator::deallocate(this->ptr);
        }
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using block_result_type = ::uwvm2::parser::wasm::standard::wasm1::features::final_result_type<Fs...>;

    enum class block_type : unsigned
    {
        function,
        block,
        loop,
        if_,
        else_
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct block_t
    {
        block_result_type<Fs...> label{};
        block_result_type<Fs...> start{};
        block_result_type<Fs...> result{};
        ::std::size_t operand_stack_base{};
        block_type type{};
        bool polymorphic_base{};
        bool then_polymorphic_end{};  // only meaningful for if/else frames
    };

    namespace details
    {
        inline constexpr auto ref_func_opcode{
            static_cast<wasm1_code>(static_cast<wasm_byte>(wasm1p1_code::ref_func))};

        [[noreturn]] inline constexpr void fail_feature_required(::std::byte const* const op_begin,
                                                                 ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                 ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const value,
                                                                 ::uwvm2::parser::wasm::base::wasm1p1_feature_kind const feature,
                                                                 ::uwvm2::parser::wasm::base::wasm1p1_error_subject const subject) UWVM_THROWS
        {
            err.err_curr = op_begin;
            err.err_selectable.wasm1p1_feature_required.value = value;
            err.err_selectable.wasm1p1_feature_required.feature = feature;
            err.err_selectable.wasm1p1_feature_required.subject = subject;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        [[noreturn]] inline constexpr void fail_invalid_immediate(::std::byte const* const op_begin,
                                                                  ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                  ::uwvm2::utils::container::u8string_view op_name,
                                                                  ::fast_io::parse_code pc = ::fast_io::parse_code::invalid) UWVM_THROWS
        {
            err.err_curr = op_begin;
            err.err_selectable.invalid_const_immediate.op_code_name = op_name;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_const_immediate;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(pc);
        }

        template <typename T>
        inline constexpr T read_leb128(::std::byte const*& code_curr,
                                       ::std::byte const* const code_end,
                                       ::std::byte const* const op_begin,
                                       ::uwvm2::validation::error::code_validation_error_impl& err,
                                       ::uwvm2::utils::container::u8string_view op_name) UWVM_THROWS
        {
            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            T value{};  // No initialization necessary.
            auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                             reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                             ::fast_io::mnp::leb128_get(value))};
            if(perr != ::fast_io::parse_code::ok) [[unlikely]] { fail_invalid_immediate(op_begin, err, op_name, perr); }

            // op_name ...
            // [safe ] unsafe (could be the section_end)
            //        ^^ code_curr

            // parse_by_scan succeeded, so [code_curr, next) is safe.
            code_curr = reinterpret_cast<::std::byte const*>(next);

            // op_name ...
            // [safe       ] unsafe (could be the section_end)
            //              ^^ code_curr

            return value;
        }

        inline constexpr void skip_bytes(::std::byte const*& code_curr,
                                         ::std::byte const* const code_end,
                                         ::std::byte const* const op_begin,
                                         ::std::size_t const bytes,
                                         ::uwvm2::validation::error::code_validation_error_impl& err,
                                         ::uwvm2::utils::container::u8string_view op_name) UWVM_THROWS
        {
            if(static_cast<::std::size_t>(code_end - code_curr) < bytes) [[unlikely]] { fail_invalid_immediate(op_begin, err, op_name); }

            // op_name payload ...
            // [safe ] unsafe (could be the section_end)
            //        ^^ code_curr

            // The remaining-byte check proves [code_curr, code_curr + bytes) is safe.
            code_curr += bytes;

            // op_name payload ...
            // [safe          ] unsafe (could be the section_end)
            //                 ^^ code_curr
        }

        inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte read_u8(
            ::std::byte const*& code_curr,
            ::std::byte const* const code_end,
            ::std::byte const* const op_begin,
            ::uwvm2::validation::error::code_validation_error_impl& err,
            ::uwvm2::utils::container::u8string_view op_name) UWVM_THROWS
        {
            auto const imm_pos{code_curr};
            skip_bytes(code_curr, code_end, op_begin, 1uz, err, op_name);

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte value{};  // No initialization necessary.
            ::std::memcpy(::std::addressof(value), imm_pos, sizeof(value));
#if CHAR_BIT > 8
            value = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(static_cast<::std::uint_least8_t>(value) & 0xFFu);
#endif
            return value;
        }

        inline constexpr void append_unique_ref(::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>& refs,
                                                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const func_idx)
        {
            if(::std::find(refs.begin(), refs.end(), func_idx) == refs.end()) { refs.push_back(func_idx); }
        }

        template <typename ConstExpr>
        inline constexpr void collect_const_expr_refs(
            ConstExpr const& expr,
            ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>& refs)
        {
            for(auto const& op: expr.opcodes)
            {
                if(op.opcode == ref_func_opcode) { append_unique_ref(refs, op.storage.ref_func_idx); }
            }
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void collect_declared_refs(
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
            ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>& refs)
        {
            auto const& exportsec{
                ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::export_section_storage_t<Fs...>>(module_storage.sections)};
            for(auto const& exp: exportsec.exports)
            {
                if(exp.exports.type == ::uwvm2::parser::wasm::standard::wasm1::type::external_types::func) { append_unique_ref(refs, exp.exports.storage.func_idx); }
            }

            auto const& globalsec{
                ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections)};
            for(auto const& global: globalsec.local_globals) { collect_const_expr_refs(global.expr, refs); }

            auto const& elemsec{
                ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>>(module_storage.sections)};
            for(auto const& elem: elemsec.elems)
            {
                auto const& segment{elem.storage.segment};
                for(auto const func_idx: segment.vec_funcidx) { append_unique_ref(refs, func_idx); }
                for(auto const& expr: segment.vec_expr) { collect_const_expr_refs(expr, refs); }
            }
        }
    }  // namespace details

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code(::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version,
                                        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
                                        ::std::size_t const function_index,
                                        ::std::byte const* code_begin,
                                        ::std::byte const* code_end,
                                        ::uwvm2::validation::error::code_validation_error_impl& err,
                                        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        static_assert((::std::same_as<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1, Fs> || ...),
                      "wasm1p1 validation requires the wasm1p1 parser feature");

        // check
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 0uz);
        auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};
        if(function_index < import_func_count) [[unlikely]]
        {
            err.err_curr = code_begin;
            err.err_selectable.not_local_function.function_index = function_index;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::not_local_function;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        auto const local_func_idx{function_index - import_func_count};

        auto const& funcsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>(
                module_storage.sections)};
        auto const local_func_count{funcsec.funcs.size()};
        if(local_func_idx >= local_func_count) [[unlikely]]
        {
            err.err_curr = code_begin;
            err.err_selectable.invalid_function_index.function_index = function_index;
            // this add will never overflow, because it has been validated in parsing.
            err.err_selectable.invalid_function_index.all_function_size = import_func_count + local_func_count;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_function_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(module_storage.sections)};

        auto const& curr_func_type{typesec.types.index_unchecked(funcsec.funcs.index_unchecked(local_func_idx))};
        auto const func_parameter_begin{curr_func_type.parameter.begin};
        auto const func_parameter_end{curr_func_type.parameter.end};
        auto const func_parameter_count_uz{static_cast<::std::size_t>(func_parameter_end - func_parameter_begin)};
        auto const func_parameter_count_u32{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_parameter_count_uz)};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(func_parameter_count_u32 != func_parameter_count_uz) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(module_storage.sections)};

        auto const& curr_code{codesec.codes.index_unchecked(local_func_idx)};
        auto const& curr_code_locals{curr_code.locals};

        // all local count = parameter + local defined local count
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_local_count{func_parameter_count_u32};
        for(auto const& local_part: curr_code_locals)
        {
            // all_local_count never overflow and never exceed the max of size_t
            all_local_count += local_part.count;
        }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if constexpr(::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max() > ::std::numeric_limits<::std::size_t>::max())
        {
            if(all_local_count > ::std::numeric_limits<::std::size_t>::max()) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
        }
#endif

        auto const& globalsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 3uz);
        auto const& imported_globals{importsec.importdesc.index_unchecked(3u)};
        auto const imported_global_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_globals.size())};
        auto const local_global_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(globalsec.local_globals.size())};
        // all_global_count never overflow and never exceed the max of u32 (validated by parser limits)
        auto const all_global_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_global_count + local_global_count)};

        // table
        auto const& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 1uz);
        auto const& imported_tables{importsec.importdesc.index_unchecked(1u)};
        auto const imported_table_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_tables.size())};
        auto const local_table_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(tablesec.tables.size())};
        // all_table_count never overflow and never exceed the max of u32 (validated by parser limits)
        auto const all_table_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_table_count + local_table_count)};

        auto const& elemsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>>(module_storage.sections)};

        // memory
        auto const& memsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 2uz);
        auto const& imported_memories{importsec.importdesc.index_unchecked(2u)};
        auto const imported_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_memories.size())};
        auto const local_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(memsec.memories.size())};
        // all_memory_count never overflow and never exceed the max of u32 (validated by parser limits)
        auto const all_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_memory_count + local_memory_count)};

        auto const& datacountsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>>(module_storage.sections)};

        auto const& wasm1p1_para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> declared_refs{};
        details::collect_declared_refs(module_storage, declared_refs);

        // control-flow stack
        using curr_block_type = block_t<Fs...>;
        ::uwvm2::utils::container::vector<curr_block_type> control_flow_stack{};

        // operand stack
        using curr_operand_stack_value_type = operand_stack_value_type<Fs...>;
        using curr_operand_stack_type = operand_stack_type<Fs...>;
        curr_operand_stack_type operand_stack{};
        bool is_polymorphic{};

        // block type
        using value_type_enum = curr_operand_stack_value_type;
        static constexpr value_type_enum i32_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32)};
        static constexpr value_type_enum i64_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64)};
        static constexpr value_type_enum f32_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32)};
        static constexpr value_type_enum f64_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64)};
        static constexpr value_type_enum v128_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)};
        static constexpr value_type_enum funcref_result_arr[1u]{static_cast<value_type_enum>(
            ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref)};
        static constexpr value_type_enum externref_result_arr[1u]{static_cast<value_type_enum>(
            ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::externref)};
        static constexpr value_type_enum i32_v128_operands[2u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32),
                                                               static_cast<value_type_enum>(
                                                                   ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)};
        static constexpr value_type_enum v128_i32_operands[2u]{static_cast<value_type_enum>(
                                                                   ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                                                               static_cast<value_type_enum>(
                                                                   ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32)};
        static constexpr value_type_enum v128_i64_operands[2u]{static_cast<value_type_enum>(
                                                                   ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                                                               static_cast<value_type_enum>(
                                                                   ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64)};
        static constexpr value_type_enum v128_f32_operands[2u]{static_cast<value_type_enum>(
                                                                   ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                                                               static_cast<value_type_enum>(
                                                                   ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32)};
        static constexpr value_type_enum v128_f64_operands[2u]{static_cast<value_type_enum>(
                                                                   ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                                                               static_cast<value_type_enum>(
                                                                   ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64)};
        static constexpr value_type_enum v128_v128_operands[2u]{static_cast<value_type_enum>(
                                                                    ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                                                                static_cast<value_type_enum>(
                                                                    ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)};

        // function block (label/result type is the function result)
        control_flow_stack.push_back({.label = curr_func_type.result,
                                      .start = {},
                                      .result = curr_func_type.result,
                                      .operand_stack_base = 0uz,
                                      .type = block_type::function,
                                      .polymorphic_base = false,
                                      .then_polymorphic_end = false});

        // start parse the code
        auto code_curr{code_begin};

        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        using code_validation_error_code = ::uwvm2::validation::error::code_validation_error_code;
        auto const to_wasm1_value_type{[](curr_operand_stack_value_type type) constexpr noexcept -> wasm_value_type
                                       { return static_cast<wasm_value_type>(type); }};

        struct concrete_operand_t
        {
            bool from_stack{};
            curr_operand_stack_value_type type{};
        };

        auto const curr_frame_operand_stack_base{[&]() constexpr noexcept -> ::std::size_t
                                                 {
                                                     if(control_flow_stack.empty()) { return 0uz; }
                                                     return control_flow_stack.back_unchecked().operand_stack_base;
                                                 }};

        auto const concrete_operand_count{[&]() constexpr noexcept -> ::std::size_t
                                          {
                                              auto const base{curr_frame_operand_stack_base()};
                                              auto const stack_size{operand_stack.size()};
                                              return stack_size >= base ? (stack_size - base) : 0uz;
                                          }};

        auto const report_operand_stack_underflow{
            [&](::std::byte const* op_begin, ::uwvm2::utils::container::u8string_view op_name, ::std::size_t required_count) constexpr UWVM_THROWS
            {
                err.err_curr = op_begin;
                err.err_selectable.operand_stack_underflow.op_code_name = op_name;
                err.err_selectable.operand_stack_underflow.stack_size_actual = concrete_operand_count();
                err.err_selectable.operand_stack_underflow.stack_size_required = required_count;
                err.err_code = code_validation_error_code::operand_stack_underflow;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }};

        auto const try_pop_concrete_operand{[&]() constexpr noexcept -> concrete_operand_t
                                            {
                                                if(concrete_operand_count() == 0uz) { return {}; }
                                                auto const operand{operand_stack.back_unchecked()};
                                                operand_stack.pop_back_unchecked();
                                                return {.from_stack = true, .type = operand.type};
                                            }};

        auto const try_peek_concrete_operand{[&]() constexpr noexcept -> concrete_operand_t
                                             {
                                                 if(concrete_operand_count() == 0uz) { return {}; }
                                                 return {.from_stack = true, .type = operand_stack.back_unchecked().type};
                                             }};

        auto const pop_available_concrete_operands{[&](::std::size_t count) constexpr noexcept
                                                   {
                                                       while(count-- != 0uz && concrete_operand_count() != 0uz) { operand_stack.pop_back_unchecked(); }
                                                   }};

        auto const is_reference_value_type{[](curr_operand_stack_value_type type) constexpr noexcept -> bool
                                           {
                                               auto const vt{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>(type)};
                                               using value_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type;
                                               return vt == value_type::funcref || vt == value_type::externref;
                                           }};

        auto const is_wasm1_numeric_value_type{[](curr_operand_stack_value_type type) constexpr noexcept -> bool
                                               {
                                                   return type == curr_operand_stack_value_type::i32 || type == curr_operand_stack_value_type::i64 ||
                                                          type == curr_operand_stack_value_type::f32 || type == curr_operand_stack_value_type::f64;
                                               }};

        auto const push_value_types{[&](block_result_type<Fs...> types) constexpr
                                    {
                                        for(auto curr{types.begin}; curr != types.end; ++curr) { operand_stack.push_back({*curr}); }
                                    }};

        auto const pop_expected_operands{
            [&](::std::byte const* op_begin, ::uwvm2::utils::container::u8string_view op_name, block_result_type<Fs...> expected) constexpr UWVM_THROWS
            {
                auto const expected_count{static_cast<::std::size_t>(expected.end - expected.begin)};
                if(!is_polymorphic && concrete_operand_count() < expected_count) [[unlikely]]
                {
                    report_operand_stack_underflow(op_begin, op_name, expected_count);
                }

                auto const stack_size{operand_stack.size()};
                auto const available_count{concrete_operand_count()};
                auto const concrete_to_check{available_count < expected_count ? available_count : expected_count};

                for(::std::size_t i{}; i != concrete_to_check; ++i)
                {
                    auto const expected_type{expected.begin[expected_count - 1uz - i]};
                    auto const actual_type{operand_stack.index_unchecked(stack_size - 1uz - i).type};
                    if(actual_type != expected_type) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.br_value_type_mismatch.op_code_name = op_name;
                        err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                        err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_type);
                        err.err_code = code_validation_error_code::br_value_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                }

                pop_available_concrete_operands(expected_count);
            }};

        auto const ensure_wasm1p1_value_type_enabled{
            [&](::std::byte const* op_begin,
                curr_operand_stack_value_type type,
                ::uwvm2::parser::wasm::base::wasm1p1_error_subject subject) constexpr UWVM_THROWS
            {
                auto const vt{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>(type)};
                if(!::uwvm2::parser::wasm::standard::wasm1p1::type::is_valid_value_type(vt)) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.wasm1p1_invalid_reference_type.value =
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(type);
                    err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                if(!::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(vt, fs_para)) [[unlikely]]
                {
                    auto const feature{vt == ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128
                                           ? ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd
                                           : ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types};
                    details::fail_feature_required(op_begin,
                                                   err,
                                                   static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(type),
                                                   feature,
                                                   subject);
                }
            }};

        struct block_signature_t
        {
            block_result_type<Fs...> start{};
            block_result_type<Fs...> result{};
        };

        auto const parse_block_type{
            [&](::std::byte const* op_begin,
                [[maybe_unused]] ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS -> block_signature_t
            {
                if(code_curr == code_end) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_code = code_validation_error_code::missing_block_type;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
                }

                // op_name blocktype ...
                // [safe ] unsafe (could be the section_end)
                //        ^^ code_curr

                auto const blocktype_begin{code_curr};
                auto const blocktype{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>(
                    code_curr, code_end, op_begin, err, u8"blocktype")};

                // op_name blocktype ...
                // [safe ] unsafe (could be the section_end)
                //        ^^ blocktype_begin

                // read_leb128 moved code_curr only after proving the whole blocktype immediate safe.

                switch(blocktype)
                {
                    case -64:
                    {
                        return {};
                    }
                    case -1:
                    {
                        return {.result = {i32_result_arr, i32_result_arr + 1u}};
                    }
                    case -2:
                    {
                        return {.result = {i64_result_arr, i64_result_arr + 1u}};
                    }
                    case -3:
                    {
                        return {.result = {f32_result_arr, f32_result_arr + 1u}};
                    }
                    case -4:
                    {
                        return {.result = {f64_result_arr, f64_result_arr + 1u}};
                    }
                    case -5:
                    {
                        ensure_wasm1p1_value_type_enabled(
                            op_begin,
                            static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                        return {.result = {v128_result_arr, v128_result_arr + 1u}};
                    }
                    case -16:
                    {
                        ensure_wasm1p1_value_type_enabled(
                            op_begin,
                            static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref),
                            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                        return {.result = {funcref_result_arr, funcref_result_arr + 1u}};
                    }
                    case -17:
                    {
                        ensure_wasm1p1_value_type_enabled(
                            op_begin,
                            static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::externref),
                            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                        return {.result = {externref_result_arr, externref_result_arr + 1u}};
                    }
                    default:
                    {
                        break;
                    }
                }

                if(blocktype >= 0)
                {
                    if(!wasm1p1_para.enable_multi_value) [[unlikely]]
                    {
                        details::fail_feature_required(op_begin,
                                                       err,
                                                       static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(blocktype),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::multi_value,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                    }

                    auto const all_type_count_uz{typesec.types.size()};
                    if(static_cast<::std::uint_least64_t>(blocktype) > ::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max() ||
                       static_cast<::std::size_t>(blocktype) >= all_type_count_uz) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_type_index.type_index =
                            blocktype > 0 ? static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(blocktype) : 0u;
                        err.err_selectable.illegal_type_index.all_type_count =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_type_count_uz);
                        err.err_code = code_validation_error_code::illegal_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const& block_func_type{typesec.types.index_unchecked(static_cast<::std::size_t>(blocktype))};
                    return {.start = block_func_type.parameter, .result = block_func_type.result};
                }

                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte first_blocktype_byte{};
                ::std::memcpy(::std::addressof(first_blocktype_byte), blocktype_begin, sizeof(first_blocktype_byte));
#if CHAR_BIT > 8
                first_blocktype_byte = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                    static_cast<::std::uint_least8_t>(first_blocktype_byte) & 0xFFu);
#endif
                err.err_curr = op_begin;
                err.err_selectable.u8 = first_blocktype_byte;
                err.err_code = code_validation_error_code::illegal_block_type;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }};

        auto const enter_control_frame{
            [&](::std::byte const* op_begin,
                ::uwvm2::utils::container::u8string_view op_name,
                block_type type,
                block_signature_t signature) constexpr UWVM_THROWS
            {
                pop_expected_operands(op_begin, op_name, signature.start);

                auto const base{operand_stack.size()};
                auto const label{type == block_type::loop ? signature.start : signature.result};
                control_flow_stack.push_back({.label = label,
                                              .start = signature.start,
                                              .result = signature.result,
                                              .operand_stack_base = base,
                                              .type = type,
                                              .polymorphic_base = is_polymorphic,
                                              .then_polymorphic_end = false});
                push_value_types(signature.start);

                // Stack-polymorphism is scoped to the current control frame only.
                is_polymorphic = false;
            }};

        auto const validate_numeric_unary{[&](::uwvm2::utils::container::u8string_view op_name,
                                              curr_operand_stack_value_type expected_operand_type,
                                              curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
                                          {
                                              // op_name ...
                                              // [safe] unsafe (could be the section_end)
                                              // ^^ code_curr

                                              auto const op_begin{code_curr};

                                              // op_name ...
                                              // [safe] unsafe (could be the section_end)
                                              // ^^ op_begin

                                              ++code_curr;

                                              // op_name ...
                                              // [safe]  unsafe (could be the section_end)
                                              //         ^^ code_curr

                                              if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]]
                                              {
                                                  report_operand_stack_underflow(op_begin, op_name, 1uz);
                                              }

                                              auto const operand{try_pop_concrete_operand()};
                                              if(operand.from_stack && operand.type != expected_operand_type) [[unlikely]]
                                              {
                                                  err.err_curr = op_begin;
                                                  err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                                                  err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(expected_operand_type);
                                                  err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(operand.type);
                                                  err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                                  ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                              }

                                              operand_stack.push_back({result_type});
                                          }};

        auto const validate_numeric_binary{[&](::uwvm2::utils::container::u8string_view op_name,
                                               curr_operand_stack_value_type expected_operand_type,
                                               curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
                                           {
                                               // op_name ...
                                               // [safe] unsafe (could be the section_end)
                                               // ^^ code_curr

                                               auto const op_begin{code_curr};

                                               // op_name ...
                                               // [safe] unsafe (could be the section_end)
                                               // ^^ op_begin

                                               ++code_curr;

                                               // op_name ...
                                               // [safe ] unsafe (could be the section_end)
                                               //         ^^ code_curr

                                               if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]]
                                               {
                                                   report_operand_stack_underflow(op_begin, op_name, 2uz);
                                               }

                                               // rhs
                                               auto const rhs{try_pop_concrete_operand()};
                                               if(rhs.from_stack && rhs.type != expected_operand_type) [[unlikely]]
                                               {
                                                   err.err_curr = op_begin;
                                                   err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                                                   err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(expected_operand_type);
                                                   err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(rhs.type);
                                                   err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                                   ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                               }

                                               // lhs
                                               auto const lhs{try_pop_concrete_operand()};
                                               if(lhs.from_stack && lhs.type != expected_operand_type) [[unlikely]]
                                               {
                                                   err.err_curr = op_begin;
                                                   err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                                                   err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(expected_operand_type);
                                                   err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(lhs.type);
                                                   err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                                   ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                               }

                                               operand_stack.push_back({result_type});
                                           }};

        auto const validate_mem_load{[&](::uwvm2::utils::container::u8string_view op_name,
                                         ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const max_align,
                                         curr_operand_stack_value_type const result_type) constexpr UWVM_THROWS
                                     {
                                         auto const op_begin{code_curr};
                                         ++code_curr;

                                         ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                                         ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                                         using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                         auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                                     reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                     ::fast_io::mnp::leb128_get(align))};
                                         if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                                         {
                                             err.err_curr = op_begin;
                                             err.err_code = code_validation_error_code::invalid_memarg_align;
                                             ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                                         }

                                         code_curr = reinterpret_cast<::std::byte const*>(align_next);

                                         auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                       ::fast_io::mnp::leb128_get(offset))};
                                         if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                                         {
                                             err.err_curr = op_begin;
                                             err.err_code = code_validation_error_code::invalid_memarg_offset;
                                             ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                                         }

                                         code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                                         if(all_memory_count == 0u) [[unlikely]]
                                         {
                                             err.err_curr = op_begin;
                                             err.err_selectable.no_memory.op_code_name = op_name;
                                             err.err_selectable.no_memory.align = align;
                                             err.err_selectable.no_memory.offset = offset;
                                             err.err_code = code_validation_error_code::no_memory;
                                             ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                         }

                                         if(align > max_align) [[unlikely]]
                                         {
                                             err.err_curr = op_begin;
                                             err.err_selectable.illegal_memarg_alignment.op_code_name = op_name;
                                             err.err_selectable.illegal_memarg_alignment.align = align;
                                             err.err_selectable.illegal_memarg_alignment.max_align = max_align;
                                             err.err_code = code_validation_error_code::illegal_memarg_alignment;
                                             ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                         }

                                         if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]]
                                         {
                                             report_operand_stack_underflow(op_begin, op_name, 1uz);
                                         }

                                         auto const addr{try_pop_concrete_operand()};
                                         if(addr.from_stack && addr.type != curr_operand_stack_value_type::i32) [[unlikely]]
                                         {
                                             err.err_curr = op_begin;
                                             err.err_selectable.memarg_address_type_not_i32.op_code_name = op_name;
                                             err.err_selectable.memarg_address_type_not_i32.addr_type = to_wasm1_value_type(addr.type);
                                             err.err_code = code_validation_error_code::memarg_address_type_not_i32;
                                             ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                         }

                                         operand_stack.push_back({result_type});
                                     }};

        auto const validate_mem_store{[&](::uwvm2::utils::container::u8string_view op_name,
                                          ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const max_align,
                                          curr_operand_stack_value_type const expected_value_type) constexpr UWVM_THROWS
                                      {
                                          auto const op_begin{code_curr};
                                          ++code_curr;

                                          ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                                          ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                                          using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                          auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                                      reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                      ::fast_io::mnp::leb128_get(align))};
                                          if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                                          {
                                              err.err_curr = op_begin;
                                              err.err_code = code_validation_error_code::invalid_memarg_align;
                                              ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                                          }

                                          code_curr = reinterpret_cast<::std::byte const*>(align_next);

                                          auto const [offset_next,
                                                      offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                           reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                           ::fast_io::mnp::leb128_get(offset))};
                                          if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                                          {
                                              err.err_curr = op_begin;
                                              err.err_code = code_validation_error_code::invalid_memarg_offset;
                                              ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                                          }

                                          code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                                          if(all_memory_count == 0u) [[unlikely]]
                                          {
                                              err.err_curr = op_begin;
                                              err.err_selectable.no_memory.op_code_name = op_name;
                                              err.err_selectable.no_memory.align = align;
                                              err.err_selectable.no_memory.offset = offset;
                                              err.err_code = code_validation_error_code::no_memory;
                                              ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                          }

                                          if(align > max_align) [[unlikely]]
                                          {
                                              err.err_curr = op_begin;
                                              err.err_selectable.illegal_memarg_alignment.op_code_name = op_name;
                                              err.err_selectable.illegal_memarg_alignment.align = align;
                                              err.err_selectable.illegal_memarg_alignment.max_align = max_align;
                                              err.err_code = code_validation_error_code::illegal_memarg_alignment;
                                              ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                          }

                                          if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]]
                                          {
                                              report_operand_stack_underflow(op_begin, op_name, 2uz);
                                          }

                                          auto const value{try_pop_concrete_operand()};
                                          auto const addr{try_pop_concrete_operand()};

                                          if(addr.from_stack && addr.type != curr_operand_stack_value_type::i32) [[unlikely]]
                                          {
                                              err.err_curr = op_begin;
                                              err.err_selectable.memarg_address_type_not_i32.op_code_name = op_name;
                                              err.err_selectable.memarg_address_type_not_i32.addr_type = to_wasm1_value_type(addr.type);
                                              err.err_code = code_validation_error_code::memarg_address_type_not_i32;
                                              ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                          }

                                          if(value.from_stack && value.type != expected_value_type) [[unlikely]]
                                          {
                                              err.err_curr = op_begin;
                                              err.err_selectable.store_value_type_mismatch.op_code_name = op_name;
                                              err.err_selectable.store_value_type_mismatch.expected_type = to_wasm1_value_type(expected_value_type);
                                              err.err_selectable.store_value_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                                              err.err_code = code_validation_error_code::store_value_type_mismatch;
                                              ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                          }
                                      }};

        auto const check_data_index{
            [&](::std::byte const* op_begin, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 data_index) constexpr UWVM_THROWS
            {
                if(!datacountsec.present || data_index >= datacountsec.count) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.illegal_data_index.data_index = data_index;
                    err.err_selectable.illegal_data_index.all_data_count = datacountsec.present ? datacountsec.count : 0u;
                    err.err_code = code_validation_error_code::illegal_data_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }};

        auto const check_element_index{
            [&](::std::byte const* op_begin, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 element_index) constexpr UWVM_THROWS
            {
                auto const all_element_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(elemsec.elems.size())};
                if(element_index >= all_element_count) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.illegal_element_index.element_index = element_index;
                    err.err_selectable.illegal_element_index.all_element_count = all_element_count;
                    err.err_code = code_validation_error_code::illegal_element_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }};

        auto const check_table_index{
            [&](::std::byte const* op_begin, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_index) constexpr UWVM_THROWS
            {
                if(table_index >= all_table_count) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.illegal_table_index.table_index = table_index;
                    err.err_selectable.illegal_table_index.all_table_count = all_table_count;
                    err.err_code = code_validation_error_code::illegal_table_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }};

        auto const check_memory_index_zero{
            [&](::std::byte const* op_begin,
                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memory_index,
                ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
            {
                if(all_memory_count == 0u) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.no_memory.op_code_name = op_name;
                    err.err_selectable.no_memory.align = 0u;
                    err.err_selectable.no_memory.offset = 0u;
                    err.err_code = code_validation_error_code::no_memory;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                if(memory_index != 0u) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.illegal_memory_index.memory_index = memory_index;
                    err.err_selectable.illegal_memory_index.all_memory_count = all_memory_count;
                    err.err_code = code_validation_error_code::illegal_memory_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }};

        auto const get_table_value_type{
            [&](::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_index) constexpr noexcept -> curr_operand_stack_value_type
            {
                if(table_index < imported_table_count)
                {
                    auto const imported_table_ptr{imported_tables.index_unchecked(table_index)};
                    return static_cast<curr_operand_stack_value_type>(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(imported_table_ptr->imports.storage.table.reftype));
                }

                auto const local_table_index{table_index - imported_table_count};
                return static_cast<curr_operand_stack_value_type>(
                    ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(tablesec.tables.index_unchecked(local_table_index).reftype));
            }};

        auto const check_ref_func_index{
            [&](::std::byte const* op_begin, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_index) constexpr UWVM_THROWS
            {
                auto const all_function_size{import_func_count + local_func_count};
                if(static_cast<::std::size_t>(func_index) >= all_function_size) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.invalid_function_index.function_index = func_index;
                    err.err_selectable.invalid_function_index.all_function_size = all_function_size;
                    err.err_code = code_validation_error_code::invalid_function_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                if(::std::find(declared_refs.begin(), declared_refs.end(), func_index) == declared_refs.end()) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.wasm1p1_undeclared_ref_func.function_index = func_index;
                    err.err_code = code_validation_error_code::wasm1p1_undeclared_ref_func;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }};

        auto const validate_i32_operands{
            [&](::std::byte const* op_begin, ::uwvm2::utils::container::u8string_view op_name, ::std::size_t count) constexpr UWVM_THROWS
            {
                if(!is_polymorphic && concrete_operand_count() < count) [[unlikely]] { report_operand_stack_underflow(op_begin, op_name, count); }

                auto const concrete_to_check{concrete_operand_count() < count ? concrete_operand_count() : count};
                for(::std::size_t i{}; i != concrete_to_check; ++i)
                {
                    auto const actual_type{operand_stack.index_unchecked(operand_stack.size() - 1uz - i).type};
                    if(actual_type != curr_operand_stack_value_type::i32) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                        err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i32);
                        err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(actual_type);
                        err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                }

                pop_available_concrete_operands(count);
            }};

        auto const validate_numeric_unary_stack_effect{
            [&](::std::byte const* op_begin,
                ::uwvm2::utils::container::u8string_view op_name,
                curr_operand_stack_value_type expected_operand_type,
                curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
            {
                if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, op_name, 1uz); }

                auto const operand{try_pop_concrete_operand()};
                if(operand.from_stack && operand.type != expected_operand_type) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(expected_operand_type);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(operand.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                operand_stack.push_back({result_type});
            }};

        // [before_section ... ] | opbase opextent
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ code_curr

        // a WebAssembly function with type '() -> ()' (often written as returning “nil”) can have no meaningful code, but it still must have a valid
        // instruction sequence—at minimum an end.

        for(;;)
        {
            if(code_curr == code_end) [[unlikely]]
            {
                // [... ] | (end)
                // [safe] | unsafe (could be the section_end)
                //          ^^ code_curr

                // Validation completes when the end is reached, so this condition can never be met. If it were met, it would indicate a missing end.
                err.err_curr = code_curr;
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::missing_end;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // opbase ...
            // [safe] unsafe (could be the section_end)
            // ^^ code_curr

            // switch the code
            wasm_byte curr_opbase;  // no initialize necessary
            ::std::memcpy(::std::addressof(curr_opbase), code_curr, sizeof(wasm_byte));

            switch(curr_opbase)
            {
                case static_cast<wasm_byte>(wasm1_code::unreachable):
                {
                    // `unreachable` makes the operand stack "polymorphic" (per Wasm validation rules):
                    // after an unreachable point, the following instructions are type-checked under the
                    // assumption that any required operands can be popped (and any results pushed),
                    // because this code path will not execute at runtime; this suppresses false
                    // operand-stack underflow/type errors until the control-flow merges/ends.

                    // unreachable ...
                    // [   safe  ] unsafe (could be the section_end)
                    // ^^ code_curr

                    ++code_curr;

                    // unreachable ...
                    // [   safe  ] unsafe (could be the section_end)
                    //             ^^ code_curr

                    // In Wasm validation, `unreachable` resets the operand stack height to the current label's base,
                    // and then makes the stack polymorphic for subsequent type-checking.
                    if(!control_flow_stack.empty())
                    {
                        auto const base{control_flow_stack.back_unchecked().operand_stack_base};
                        while(operand_stack.size() > base) { operand_stack.pop_back_unchecked(); }
                    }

                    is_polymorphic = true;

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::nop):
                {
                    // nop    ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    ++code_curr;

                    // nop    ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::block):
                {
                    // block  blocktype ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // block  blocktype ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // block  blocktype ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    auto const signature{parse_block_type(op_begin, u8"block")};
                    enter_control_frame(op_begin, u8"block", block_type::block, signature);

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::loop):
                {
                    // loop   blocktype ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // loop   blocktype ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // loop   blocktype ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    auto const signature{parse_block_type(op_begin, u8"loop")};
                    enter_control_frame(op_begin, u8"loop", block_type::loop, signature);

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::if_):
                {
                    // if     blocktype ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // if     blocktype ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // if     blocktype ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    auto const signature{parse_block_type(op_begin, u8"if")};

                    // Stack effect before entering the then branch: (params..., i32 cond) -> (params...).
                    auto const if_param_count{static_cast<::std::size_t>(signature.start.end - signature.start.begin)};
                    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
                    auto const if_required_overflows{if_param_count == max_operand_stack_requirement};
                    auto const if_required_stack_size{if_required_overflows ? max_operand_stack_requirement : (if_param_count + 1uz)};
                    if(!is_polymorphic && (if_required_overflows || concrete_operand_count() < if_required_stack_size)) [[unlikely]]
                    {
                        report_operand_stack_underflow(op_begin, u8"if", if_required_stack_size);
                    }

                    auto const cond{try_pop_concrete_operand()};
                    if(cond.from_stack && cond.type != curr_operand_stack_value_type::i32) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.if_cond_type_not_i32.cond_type = to_wasm1_value_type(cond.type);
                        err.err_code = code_validation_error_code::if_cond_type_not_i32;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    enter_control_frame(op_begin, u8"if", block_type::if_, signature);

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::else_):
                {
                    // else   ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // else   ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // else   ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    if(control_flow_stack.empty() || control_flow_stack.back_unchecked().type != block_type::if_) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_else;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto& if_frame{control_flow_stack.back_unchecked()};

                    // Validate the then-branch result before switching to else.
                    // Match `end`: polymorphic mode only relaxes underflow, but still rejects extra values
                    // and still checks types when enough concrete values are present.
                    auto const expected_count{static_cast<::std::size_t>(if_frame.result.end - if_frame.result.begin)};
                    auto const base{if_frame.operand_stack_base};
                    auto const stack_size{operand_stack.size()};
                    auto const actual_count{stack_size >= base ? stack_size - base : 0uz};

                    if(!is_polymorphic ? (actual_count != expected_count) : (actual_count > expected_count))
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.if_then_result_mismatch.expected_count = expected_count;
                        err.err_selectable.if_then_result_mismatch.actual_count = actual_count;

                        if(expected_count == 1uz)
                        {
                            err.err_selectable.if_then_result_mismatch.expected_type =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(*if_frame.result.begin);
                        }
                        else
                        {
                            err.err_selectable.if_then_result_mismatch.expected_type = {};
                        }

                        if(actual_count == 1uz && stack_size != 0uz)
                        {
                            err.err_selectable.if_then_result_mismatch.actual_type =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(operand_stack.back_unchecked().type);
                        }
                        else
                        {
                            err.err_selectable.if_then_result_mismatch.actual_type = {};
                        }

                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::if_then_result_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(expected_count != 0uz)
                    {
                        auto const concrete_to_check{actual_count < expected_count ? actual_count : expected_count};
                        for(::std::size_t i{}; i != concrete_to_check; ++i)
                        {
                            auto const expected_type{if_frame.result.begin[expected_count - 1uz - i]};
                            auto const actual_type{operand_stack.index_unchecked(stack_size - 1uz - i).type};
                            if(actual_type != expected_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.if_then_result_mismatch.expected_count = expected_count;
                                err.err_selectable.if_then_result_mismatch.actual_count = actual_count;
                                err.err_selectable.if_then_result_mismatch.expected_type =
                                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                                err.err_selectable.if_then_result_mismatch.actual_type =
                                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                                err.err_code = ::uwvm2::validation::error::code_validation_error_code::if_then_result_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                    }

                    // Record then-branch reachability to merge with else at `end`.
                    if_frame.then_polymorphic_end = is_polymorphic;

                    // Start else branch with the operand stack at if-entry height.
                    while(operand_stack.size() > if_frame.operand_stack_base) { operand_stack.pop_back_unchecked(); }
                    push_value_types(if_frame.start);
                    // As in the spec's push_ctrl(else, ...), the else-frame itself starts reachable.
                    is_polymorphic = false;

                    // Mark that else has been consumed.
                    if_frame.type = block_type::else_;

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::end):
                {
                    // end    ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // end    ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // end    ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    // `end` closes the innermost control frame (block/loop/if/function) and checks that the current
                    // operand stack matches the declared block result type.

                    if(control_flow_stack.empty()) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.u8 = static_cast<::std::uint_least8_t>(curr_opbase);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_opbase;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const frame{control_flow_stack.back_unchecked()};
                    bool const is_function_frame{frame.type == block_type::function};

                    ::uwvm2::utils::container::u8string_view block_kind;  // no initialization necessary
                    switch(frame.type)
                    {
                        case block_type::function:
                        {
                            block_kind = u8"function";
                            break;
                        }
                        case block_type::block:
                        {
                            block_kind = u8"block";
                            break;
                        }
                        case block_type::loop:
                        {
                            block_kind = u8"loop";
                            break;
                        }
                        case block_type::if_:
                        {
                            block_kind = u8"if";
                            break;
                        }
                        case block_type::else_:
                        {
                            block_kind = u8"if-else";
                            break;
                        }
                        [[unlikely]] default:
                        {
                            block_kind = u8"block";
                            break;
                        }
                    }

                    auto const expected_count{static_cast<::std::size_t>(frame.result.end - frame.result.begin)};

                    // Special rule: an `if` with a non-empty result type must have an `else` branch, otherwise the
                    // false branch would not produce the required values.
                    if(frame.type == block_type::if_ && expected_count != 0uz) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.if_missing_else.expected_count = expected_count;
                        err.err_selectable.if_missing_else.expected_type =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(*frame.result.begin);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::if_missing_else;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const base{frame.operand_stack_base};
                    auto const stack_size{operand_stack.size()};
                    auto const actual_count{stack_size >= base ? stack_size - base : 0uz};

                    // Stack end rule:
                    // - In reachable code, the stack at `end` must match the block result types exactly.
                    // - In polymorphic (unreachable) code, stack underflow is permitted, but extra values are not.
                    if(!is_polymorphic ? (actual_count != expected_count) : (actual_count > expected_count))
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.end_result_mismatch.block_kind = block_kind;
                        err.err_selectable.end_result_mismatch.expected_count = expected_count;
                        err.err_selectable.end_result_mismatch.actual_count = actual_count;

                        if(expected_count == 1uz)
                        {
                            err.err_selectable.end_result_mismatch.expected_type =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(*frame.result.begin);
                        }
                        else
                        {
                            err.err_selectable.end_result_mismatch.expected_type = {};
                        }

                        if(actual_count == 1uz && stack_size != 0uz)
                        {
                            err.err_selectable.end_result_mismatch.actual_type =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(operand_stack.back_unchecked().type);
                        }
                        else
                        {
                            err.err_selectable.end_result_mismatch.actual_type = {};
                        }

                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::end_result_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // If the stack has enough values to satisfy the expected results, check their types even in
                    // polymorphic (unreachable) mode; only the underflow aspect is suppressed.
                    if(expected_count != 0uz)
                    {
                        auto const concrete_to_check{actual_count < expected_count ? actual_count : expected_count};
                        for(::std::size_t i{}; i != concrete_to_check; ++i)
                        {
                            auto const expected_type{frame.result.begin[expected_count - 1uz - i]};
                            auto const actual_type{operand_stack.index_unchecked(stack_size - 1uz - i).type};
                            if(actual_type != expected_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.end_result_mismatch.block_kind = block_kind;
                                err.err_selectable.end_result_mismatch.expected_count = expected_count;
                                err.err_selectable.end_result_mismatch.actual_count = actual_count;
                                err.err_selectable.end_result_mismatch.expected_type =
                                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                                err.err_selectable.end_result_mismatch.actual_type =
                                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                                err.err_code = ::uwvm2::validation::error::code_validation_error_code::end_result_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                    }

                    // Leave the frame: discard any intermediate values and push the declared results for outer typing.
                    while(operand_stack.size() > base) { operand_stack.pop_back_unchecked(); }
                    for(::std::size_t i{}; i != expected_count; ++i) { operand_stack.push_back({frame.result.begin[i]}); }

                    // Restore / merge the polymorphic state.
                    if(frame.type == block_type::else_)
                    {
                        // For if-else, continuation is unreachable only when both branches are unreachable.
                        is_polymorphic = frame.polymorphic_base || (frame.then_polymorphic_end && is_polymorphic);
                    }
                    else
                    {
                        is_polymorphic = frame.polymorphic_base;
                    }

                    // Pop the control frame.
                    control_flow_stack.pop_back_unchecked();

                    // The function body is a single expression terminated by `end`. When the function frame is closed,
                    // validation of this function is complete and `end` must be the last opcode in the body.
                    if(is_function_frame)
                    {
                        if(code_curr != code_end) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::trailing_code_after_end;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        return;
                    }

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::br):
                {
                    // br     label_index ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // br     label_index ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // br     label_index ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 label_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [label_next, label_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(label_index))};
                    if(label_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(label_err);
                    }

                    // br     label_index ...
                    // [     safe       ] unsafe (could be the section_end)
                    //        ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(label_next);

                    // br     label_index ...
                    // [     safe       ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    auto const all_label_count_uz{control_flow_stack.size()};
                    auto const label_index_uz{static_cast<::std::size_t>(label_index)};
                    if(label_index_uz >= all_label_count_uz) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_label_index.label_index = label_index;
                        err.err_selectable.illegal_label_index.all_label_count =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_label_count_uz);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_label_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const& target_frame{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - label_index_uz)};

                    // Label arity = label_types count. In MVP, we only support empty or single-value blocktypes.
                    // IMPORTANT: for `loop`, label types are the loop *parameters* (the types at the beginning of the loop),
                    // not the loop result types. MVP has no block parameters, so a loop label always has arity 0.
                    auto const target_arity{static_cast<::std::size_t>(target_frame.label.end - target_frame.label.begin)};

                    if(!is_polymorphic && concrete_operand_count() < target_arity) [[unlikely]]
                    {
                        report_operand_stack_underflow(op_begin, u8"br", target_arity);
                    }

                    // type-check the branch arguments that are concrete above the current frame base
                    if(target_arity != 0uz)
                    {
                        auto const concrete_to_check{concrete_operand_count() < target_arity ? concrete_operand_count() : target_arity};
                        for(::std::size_t i{}; i != concrete_to_check; ++i)
                        {
                            auto const expected_type{target_frame.label.begin[target_arity - 1uz - i]};
                            auto const actual_type{operand_stack.index_unchecked(operand_stack.size() - 1uz - i).type};
                            if(actual_type != expected_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"br";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_type);
                                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                    }

                    // Consume branch arguments (if present) and make stack polymorphic (unreachable).
                    pop_available_concrete_operands(target_arity);
                    // Avoid leaking concrete stack values into the polymorphic region (prevents false type errors after an unconditional branch).
                    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
                    while(operand_stack.size() > curr_frame_base) { operand_stack.pop_back_unchecked(); }
                    is_polymorphic = true;

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::br_if):
                {
                    // br_if  label_index ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // br_if  label_index ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // br_if  label_index ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 label_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [label_next, label_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(label_index))};
                    if(label_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(label_err);
                    }

                    // br_if  label_index ...
                    // [      safe      ] unsafe (could be the section_end)
                    //        ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(label_next);

                    // br_if  label_index ...
                    // [      safe      ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    auto const all_label_count_uz{control_flow_stack.size()};
                    auto const label_index_uz{static_cast<::std::size_t>(label_index)};
                    if(label_index_uz >= all_label_count_uz) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_label_index.label_index = label_index;
                        err.err_selectable.illegal_label_index.all_label_count =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_label_count_uz);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_label_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const& target_frame{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - label_index_uz)};

                    // Label arity = label_types count (MVP: 0 or 1).
                    // IMPORTANT: for `loop`, label types are parameters (MVP: none), not result types.
                    auto const target_arity{static_cast<::std::size_t>(target_frame.label.end - target_frame.label.begin)};

                    // Need (labelargs..., i32 cond)
                    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
                    auto const target_arity_plus_cond_overflows{target_arity == max_operand_stack_requirement};
                    auto const required_stack_size{target_arity_plus_cond_overflows ? max_operand_stack_requirement : (target_arity + 1uz)};

                    if(!is_polymorphic && (target_arity_plus_cond_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
                    {
                        report_operand_stack_underflow(op_begin, u8"br_if", required_stack_size);
                    }

                    // cond (must be i32 if present)
                    auto const cond{try_pop_concrete_operand()};
                    if(cond.from_stack && cond.type != curr_operand_stack_value_type::i32) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.br_cond_type_not_i32.op_code_name = u8"br_if";
                        err.err_selectable.br_cond_type_not_i32.cond_type = to_wasm1_value_type(cond.type);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_cond_type_not_i32;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // type-check label arguments if present (they remain on stack for the fallthrough path)
                    if(target_arity != 0uz)
                    {
                        auto const concrete_to_check{concrete_operand_count() < target_arity ? concrete_operand_count() : target_arity};
                        for(::std::size_t i{}; i != concrete_to_check; ++i)
                        {
                            auto const expected_type{target_frame.label.begin[target_arity - 1uz - i]};
                            auto const actual_type{operand_stack.index_unchecked(operand_stack.size() - 1uz - i).type};
                            if(actual_type != expected_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"br_if";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_type);
                                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        if(is_polymorphic && concrete_to_check != target_arity)
                        {
                            // In polymorphic mode, `br_if` still re-establishes the fallthrough stack as if
                            // label arguments had been popped and pushed back.
                            pop_available_concrete_operands(concrete_to_check);
                            push_value_types(target_frame.label);
                        }
                    }

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::br_table):
                {
                    // br_table  target_count ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // br_table  target_count ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // br_table  target_count ...
                    // [ safe ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 target_count;  // No initialization necessary
                    auto const [cnt_next, cnt_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(target_count))};
                    if(cnt_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(cnt_err);
                    }

                    // br_table  target_count ...
                    // [       safe         ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(cnt_next);

                    // br_table  target_count ...
                    // [       safe         ] unsafe (could be the section_end)
                    //                       ^^ code_curr

                    // Security hardening: each `br_table` label index (including the default label)
                    // is encoded as an unsigned LEB128 value and therefore occupies at least one
                    // byte. Reject impossible `target_count` values before further validation so
                    // malformed inputs cannot inflate validation/translation work into a resource
                    // exhaustion path.
                    auto const remaining_bytes{static_cast<::std::size_t>(code_end - code_curr)};
                    constexpr auto max_br_table_label_count{::std::numeric_limits<::std::size_t>::max()};
                    bool target_count_exceeds_size_t{};
                    ::std::size_t target_count_uz{};
                    if constexpr(::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max() > max_br_table_label_count)
                    {
                        if(target_count > max_br_table_label_count) [[unlikely]] { target_count_exceeds_size_t = true; }
                        else
                        {
                            target_count_uz = static_cast<::std::size_t>(target_count);
                        }
                    }
                    else
                    {
                        target_count_uz = static_cast<::std::size_t>(target_count);
                    }

                    auto const target_count_plus_default_overflows{!target_count_exceeds_size_t && target_count_uz == max_br_table_label_count};
                    if(target_count_exceeds_size_t || target_count_plus_default_overflows || remaining_bytes == 0uz || target_count_uz >= remaining_bytes)
                        [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.br_table_target_count_exceeds_remaining_bytes.target_count = target_count;
                        err.err_selectable.br_table_target_count_exceeds_remaining_bytes.remaining_bytes = remaining_bytes;
                        err.err_selectable.br_table_target_count_exceeds_remaining_bytes.max_target_count =
                            (remaining_bytes == 0uz ? 0uz : remaining_bytes - 1uz);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_table_target_count_exceeds_remaining_bytes;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const all_label_count_uz{control_flow_stack.size()};
                    auto const validate_label{[&](::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 li) constexpr UWVM_THROWS
                                              {
                                                  if(static_cast<::std::size_t>(li) >= all_label_count_uz) [[unlikely]]
                                                  {
                                                      err.err_curr = op_begin;
                                                      err.err_selectable.illegal_label_index.label_index = li;
                                                      err.err_selectable.illegal_label_index.all_label_count =
                                                          static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_label_count_uz);
                                                      err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_label_index;
                                                      ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                                  }
                                              }};

                    struct get_sig_result_t
                    {
                        block_result_type<Fs...> types{};
                    };

                    auto const get_sig{[&](::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 li) constexpr noexcept
                                       {
                                           auto const& frame{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - static_cast<::std::size_t>(li))};
                                           return get_sig_result_t{frame.label};
                                       }};

                    bool have_expected_sig{};
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 expected_label{};
                    block_result_type<Fs...> expected_label_types{};

                    auto const check_br_table_sig{
                        [&](::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 li, block_result_type<Fs...> actual_types) constexpr UWVM_THROWS
                        {
                            if(!have_expected_sig)
                            {
                                have_expected_sig = true;
                                expected_label = li;
                                expected_label_types = actual_types;
                                return;
                            }

                            auto const expected_arity{static_cast<::std::size_t>(expected_label_types.end - expected_label_types.begin)};
                            auto const actual_arity{static_cast<::std::size_t>(actual_types.end - actual_types.begin)};
                            bool mismatch{expected_arity != actual_arity};
                            curr_operand_stack_value_type expected_type{};
                            curr_operand_stack_value_type actual_type{};

                            auto const comparable_count{expected_arity < actual_arity ? expected_arity : actual_arity};
                            for(::std::size_t i{}; i != comparable_count; ++i)
                            {
                                if(expected_label_types.begin[i] != actual_types.begin[i])
                                {
                                    mismatch = true;
                                    expected_type = expected_label_types.begin[i];
                                    actual_type = actual_types.begin[i];
                                    break;
                                }
                            }

                            if(mismatch) [[unlikely]]
                            {
                                if(comparable_count == 0uz || expected_type == curr_operand_stack_value_type{})
                                {
                                    if(expected_arity != 0uz) { expected_type = expected_label_types.begin[0]; }
                                    if(actual_arity != 0uz) { actual_type = actual_types.begin[0]; }
                                }

                                err.err_curr = op_begin;
                                err.err_selectable.br_table_target_type_mismatch.expected_label_index = expected_label;
                                err.err_selectable.br_table_target_type_mismatch.mismatched_label_index = li;
                                err.err_selectable.br_table_target_type_mismatch.expected_arity =
                                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(expected_arity);
                                err.err_selectable.br_table_target_type_mismatch.actual_arity =
                                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(actual_arity);
                                err.err_selectable.br_table_target_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                                err.err_selectable.br_table_target_type_mismatch.actual_type = to_wasm1_value_type(actual_type);
                                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_table_target_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }};

                    for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 i{}; i != target_count; ++i)
                    {
                        // ...    | curr_target ...
                        // [safe] | unsafe (could be the section_end)
                        //          ^^ code_curr

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 li;  // No initialization necessary
                        auto const [li_next, li_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                              ::fast_io::mnp::leb128_get(li))};
                        if(li_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(li_err);
                        }

                        // ...   | curr_target ...
                        // [safe | safe      ] unsafe (could be the section_end)
                        //         ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(li_next);

                        // ...   | curr_target ...
                        // [safe | safe      ] unsafe (could be the section_end)
                        //                     ^^ code_curr

                        validate_label(li);

                        check_br_table_sig(li, get_sig(li).types);
                    }

                    // ... last_target | default_label ...
                    // [   safe      ]   unsafe (could be the section_end)
                    //                   ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 default_label;  // No initialization necessary
                    auto const [def_next, def_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(default_label))};
                    if(def_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(def_err);
                    }

                    // ... last_target | default_label ...
                    // [         safe  |      safe   ] unsafe (could be the section_end)
                    //                   ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(def_next);

                    // ... last_target | default_label ...
                    // [         safe  |      safe   ] unsafe (could be the section_end)
                    //                                 ^^ code_curr

                    validate_label(default_label);

                    check_br_table_sig(default_label, get_sig(default_label).types);

                    // Stack effect: (labelargs..., i32 index) -> unreachable
                    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
                    auto const expected_arity{static_cast<::std::size_t>(expected_label_types.end - expected_label_types.begin)};
                    auto const expected_arity_plus_index_overflows{expected_arity == max_operand_stack_requirement};
                    auto const required_stack_size{expected_arity_plus_index_overflows ? max_operand_stack_requirement : (expected_arity + 1uz)};

                    if(!is_polymorphic && (expected_arity_plus_index_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
                    {
                        report_operand_stack_underflow(op_begin, u8"br_table", required_stack_size);
                    }

                    auto const idx{try_pop_concrete_operand()};
                    if(idx.from_stack && idx.type != curr_operand_stack_value_type::i32) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.br_cond_type_not_i32.op_code_name = u8"br_table";
                        err.err_selectable.br_cond_type_not_i32.cond_type = to_wasm1_value_type(idx.type);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_cond_type_not_i32;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(expected_arity != 0uz)
                    {
                        auto const concrete_to_check{concrete_operand_count() < expected_arity ? concrete_operand_count() : expected_arity};
                        for(::std::size_t i{}; i != concrete_to_check; ++i)
                        {
                            auto const actual_type{operand_stack.index_unchecked(operand_stack.size() - 1uz - i).type};
                            auto const curr_expected_type{expected_label_types.begin[expected_arity - 1uz - i]};
                            if(actual_type != curr_expected_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"br_table";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(curr_expected_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_type);
                                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                    }

                    // Consume label args if present and make stack polymorphic.
                    pop_available_concrete_operands(expected_arity);
                    // Avoid leaking concrete stack values into the polymorphic region (prevents false type errors after br_table).
                    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
                    while(operand_stack.size() > curr_frame_base) { operand_stack.pop_back_unchecked(); }
                    is_polymorphic = true;

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::return_):
                {
                    // return ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // return ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // return ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    // `return` exits the function immediately. It is equivalent to an unconditional branch to the
                    // implicit outer function label (the bottom frame in control_flow_stack).
                    auto const& func_frame{control_flow_stack.index_unchecked(0u)};

                    ::std::size_t const return_arity{static_cast<::std::size_t>(func_frame.result.end - func_frame.result.begin)};

                    if(!is_polymorphic && concrete_operand_count() < return_arity) [[unlikely]]
                    {
                        report_operand_stack_underflow(op_begin, u8"return", return_arity);
                    }

                    auto const operator_stack_size{operand_stack.size()};
                    auto const available_return_values{concrete_operand_count()};

                    // Type-check the return values if present. For multi-value, values are validated from the top of the stack.
                    if(return_arity != 0uz)
                    {
                        auto const concrete_to_check{available_return_values < return_arity ? available_return_values : return_arity};
                        for(::std::size_t i{}; i != concrete_to_check; ++i)
                        {
                            auto const expected_type{func_frame.result.begin[return_arity - 1uz - i]};
                            auto const actual_type{operand_stack.index_unchecked(operator_stack_size - 1uz - i).type};
                            if(actual_type != expected_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"return";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_type);
                                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                    }

                    // Consume return values (if present) and make stack polymorphic (unreachable).
                    pop_available_concrete_operands(return_arity);

                    // Avoid leaking concrete stack values into the polymorphic region (prevents false type errors after return).
                    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
                    while(operand_stack.size() > curr_frame_base) { operand_stack.pop_back_unchecked(); }
                    is_polymorphic = true;

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::call):
                {
                    // call     func_index ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // call     func_index ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // call     func_index ...
                    // [ safe ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [func_next, func_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                              ::fast_io::mnp::leb128_get(func_index))};
                    if(func_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_function_index_encoding;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(func_err);
                    }

                    // call func_index ...
                    // [      safe   ] unsafe (could be the section_end)
                    //      ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(func_next);

                    // call func_index ...
                    // [      safe   ] unsafe (could be the section_end)
                    //                ^^ code_curr

                    // Validate function index range (imports + locals)
                    auto const all_function_size{import_func_count + local_func_count};
                    if(static_cast<::std::size_t>(func_index) >= all_function_size) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.invalid_function_index.function_index = static_cast<::std::size_t>(func_index);
                        err.err_selectable.invalid_function_index.all_function_size = all_function_size;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_function_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Resolve callee type
                    ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...> const* callee_type_ptr{};
                    if(static_cast<::std::size_t>(func_index) < import_func_count)
                    {
                        auto const& imported_funcs{importsec.importdesc.index_unchecked(0u)};
                        auto const imported_func_ptr{imported_funcs.index_unchecked(static_cast<::std::size_t>(func_index))};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(imported_func_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                        callee_type_ptr = imported_func_ptr->imports.storage.function;
                    }
                    else
                    {
                        auto const local_idx{static_cast<::std::size_t>(func_index) - import_func_count};
                        callee_type_ptr = typesec.types.cbegin() + funcsec.funcs.index_unchecked(local_idx);
                    }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(callee_type_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

                    auto const& callee_type{*callee_type_ptr};

                    auto const param_count{static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
                    auto const result_count{static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};

                    if(!is_polymorphic && concrete_operand_count() < param_count) [[unlikely]]
                    {
                        report_operand_stack_underflow(op_begin, u8"call", param_count);
                    }

                    auto const stack_size{operand_stack.size()};
                    auto const available_param_count{concrete_operand_count()};

                    // Type-check any concrete arguments above the current frame base; missing deeper operands are treated as unknown in polymorphic mode.
                    if(param_count != 0uz)
                    {
                        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
                        for(::std::size_t i{}; i != concrete_to_check; ++i)
                        {
                            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
                            auto const actual_type{operand_stack.index_unchecked(stack_size - 1uz - i).type};
                            if(actual_type != expected_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"call";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_type);
                                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                    }

                    // Consume parameters if present.
                    pop_available_concrete_operands(param_count);

                    // Push results.
                    if(result_count != 0uz)
                    {
                        for(::std::size_t i{}; i != result_count; ++i) { operand_stack.push_back({callee_type.result.begin[i]}); }
                    }

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::call_indirect):
                {
                    // call_indirect  type_index table_index ...
                    // [ safe      ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // call_indirect  type_index table_index ...
                    // [ safe      ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // call_indirect type_index table_index ...
                    // [    safe   ] unsafe (could be the section_end)
                    //               ^^ code_curr

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 type_index;  // No initialization necessary
                    auto const [type_next, type_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                              ::fast_io::mnp::leb128_get(type_index))};
                    if(type_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(type_err);
                    }

                    // call_indirect type_index table_index ...
                    // [          safe        ] unsafe (could be the section_end)
                    //               ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(type_next);

                    // call_indirect type_index table_index ...
                    // [          safe        ] unsafe (could be the section_end)
                    //                          ^^ code_curr

                    auto const all_type_count_uz{typesec.types.size()};
                    if(static_cast<::std::size_t>(type_index) >= all_type_count_uz) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_type_index.type_index = type_index;
                        err.err_selectable.illegal_type_index.all_type_count =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_type_count_uz);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_index;  // No initialization necessary
                    auto const [table_next, table_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(table_index))};
                    if(table_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_table_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(table_err);
                    }

                    // call_indirect type_index table_index ...
                    // [                safe              ] unsafe (could be the section_end)
                    //                          ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(table_next);

                    // call_indirect type_index table_index ...
                    // [                safe              ] unsafe (could be the section_end)
                    //                                      ^^ code_curr

                    if(table_index >= all_table_count) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_table_index.table_index = table_index;
                        err.err_selectable.illegal_table_index.all_table_count = all_table_count;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_table_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(get_table_value_type(table_index) !=
                       static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref)) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.br_value_type_mismatch.op_code_name = u8"call_indirect";
                        err.err_selectable.br_value_type_mismatch.expected_type =
                            to_wasm1_value_type(static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref));
                        err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(get_table_value_type(table_index));
                        err.err_code = code_validation_error_code::br_value_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Resolve the function signature by type index.
                    auto const& callee_type{typesec.types.index_unchecked(static_cast<::std::size_t>(type_index))};
                    auto const param_count{static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
                    auto const result_count{static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};

                    // Stack effect: (args..., i32 func_index) -> (results...)
                    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
                    auto const param_count_plus_table_index_overflows{param_count == max_operand_stack_requirement};
                    auto const required_stack_size{param_count_plus_table_index_overflows ? max_operand_stack_requirement : (param_count + 1uz)};

                    if(!is_polymorphic && (param_count_plus_table_index_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
                    {
                        report_operand_stack_underflow(op_begin, u8"call_indirect", required_stack_size);
                    }

                    // function index operand (must be i32 if present)
                    auto const idx{try_pop_concrete_operand()};
                    if(idx.from_stack && idx.type != curr_operand_stack_value_type::i32) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.br_cond_type_not_i32.op_code_name = u8"call_indirect";
                        err.err_selectable.br_cond_type_not_i32.cond_type = to_wasm1_value_type(idx.type);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_cond_type_not_i32;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const stack_size{operand_stack.size()};
                    auto const available_param_count{concrete_operand_count()};
                    if(param_count != 0uz)
                    {
                        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
                        for(::std::size_t i{}; i != concrete_to_check; ++i)
                        {
                            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
                            auto const actual_type{operand_stack.index_unchecked(stack_size - 1uz - i).type};
                            if(actual_type != expected_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"call_indirect";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_type);
                                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                    }

                    pop_available_concrete_operands(param_count);

                    if(result_count != 0uz)
                    {
                        for(::std::size_t i{}; i != result_count; ++i) { operand_stack.push_back({callee_type.result.begin[i]}); }
                    }

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::drop):
                {
                    // drop   ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // drop   ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ op_begin

                    ++code_curr;

                    // drop   ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    if(concrete_operand_count() == 0uz) [[unlikely]]
                    {
                        // Polymorphic stack: underflow is allowed, so drop becomes a no-op on the concrete stack.
                        if(!is_polymorphic) { report_operand_stack_underflow(op_begin, u8"drop", 1uz); }
                    }
                    else
                    {
                        operand_stack.pop_back_unchecked();
                    }

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::select):
                {
                    // select ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // select ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // select ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    // Stack effect: (v1 v2 i32) -> (v) where v is v1/v2 and v1,v2 must have the same type.
                    // In polymorphic mode, operand-stack underflow is allowed, but concrete operands (if present) are still type-checked.

                    if(!is_polymorphic && concrete_operand_count() < 3uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"select", 3uz); }

                    // cond (must be i32 if it exists on the concrete stack)
                    bool cond_from_stack{};
                    curr_operand_stack_value_type cond_type{};
                    if(auto const cond{try_pop_concrete_operand()}; cond.from_stack)
                    {
                        cond_from_stack = true;
                        cond_type = cond.type;
                    }

                    if(cond_from_stack && cond_type != curr_operand_stack_value_type::i32) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.select_cond_type_not_i32.cond_type = to_wasm1_value_type(cond_type);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::select_cond_type_not_i32;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // v2
                    bool v2_from_stack{};
                    curr_operand_stack_value_type v2_type{};
                    if(auto const v2{try_pop_concrete_operand()}; v2.from_stack)
                    {
                        v2_from_stack = true;
                        v2_type = v2.type;
                    }

                    // v1 (kept as result when present, matching existing implementation)
                    bool v1_from_stack{};
                    curr_operand_stack_value_type v1_type{};
                    if(auto const v1{try_peek_concrete_operand()}; v1.from_stack)
                    {
                        v1_from_stack = true;
                        v1_type = v1.type;
                    }

                    if(v1_from_stack && v2_from_stack && v1_type != v2_type) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_value_type(v1_type);
                        err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_value_type(v2_type);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::select_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const select_value_type{v1_from_stack ? v1_type : v2_type};
                    if((v1_from_stack || v2_from_stack) && !is_wasm1_numeric_value_type(select_value_type)) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_value_type(select_value_type);
                        err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_value_type(select_value_type);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::select_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // If v1 is not present on the concrete stack but v2 is, we must still produce one result of v2's type.
                    if(!v1_from_stack && v2_from_stack) { operand_stack.push_back({v2_type}); }

                    break;
                }
                case opcode_byte(wasm1p1_code::select_t):
                {
                    // select_t result_types ...
                    // [  safe ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // select_t result_types ...
                    // [  safe ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // select_t result_types ...
                    // [  safe ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    if(!wasm1p1_para.enable_multi_value) [[unlikely]]
                    {
                        details::fail_feature_required(op_begin,
                                                       err,
                                                       opcode_u32(wasm1p1_code::select_t),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::multi_value,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                    }

                    auto const result_type_count{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                        code_curr, code_end, op_begin, err, u8"select.result_types")};
                    if(result_type_count != 1u) [[unlikely]] { details::fail_invalid_immediate(op_begin, err, u8"select.result_types"); }

                    auto const result_type_byte{details::read_u8(code_curr, code_end, op_begin, err, u8"select.result_type")};
                    auto const result_type{static_cast<curr_operand_stack_value_type>(result_type_byte)};
                    ensure_wasm1p1_value_type_enabled(op_begin, result_type, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);

                    if(!is_polymorphic && concrete_operand_count() < 3uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"select", 3uz); }

                    auto const cond{try_pop_concrete_operand()};
                    if(cond.from_stack && cond.type != curr_operand_stack_value_type::i32) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.select_cond_type_not_i32.cond_type = to_wasm1_value_type(cond.type);
                        err.err_code = code_validation_error_code::select_cond_type_not_i32;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const v2{try_pop_concrete_operand()};
                    if(v2.from_stack && v2.type != result_type) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_value_type(result_type);
                        err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_value_type(v2.type);
                        err.err_code = code_validation_error_code::select_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const v1{try_pop_concrete_operand()};
                    if(v1.from_stack && v1.type != result_type) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_value_type(result_type);
                        err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_value_type(v1.type);
                        err.err_code = code_validation_error_code::select_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    operand_stack.push_back({result_type});

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::local_get):
                {
                    // local.get ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // local.get ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // local.get local_index ...
                    // [ safe  ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
                    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                            ::fast_io::mnp::leb128_get(local_index))};

                    if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_local_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
                    }

                    // local.get local_index ...
                    // [     safe          ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

                    // local.get local_index ...
                    // [     safe          ] unsafe (could be the section_end)
                    //                       ^^ code_curr

                    // check the local_index is valid
                    if(local_index >= all_local_count) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_local_index.local_index = local_index;
                        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    curr_operand_stack_value_type curr_local_type{};

                    if(local_index < func_parameter_count_u32)
                    {
                        // function parameter
                        curr_local_type = func_parameter_begin[local_index];
                    }
                    else
                    {
                        // function defined local variable
                        auto tem_local_index{local_index - func_parameter_count_u32};

                        bool found_local{};
                        for(auto const& local_part: curr_code_locals)
                        {
                            if(tem_local_index < local_part.count)
                            {
                                curr_local_type = local_part.type;
                                found_local = true;
                                break;
                            }

                            tem_local_index -= local_part.count;
                        }

                        if(!found_local) [[unlikely]]
                        {
                            // Inconsistency between `all_local_count` and the locals vector; treat as invalid code.
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_local_index.local_index = local_index;
                            err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    // local.get always pushes one value of the local's type (even in polymorphic mode)
                    operand_stack.push_back({curr_local_type});

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::local_set):
                {
                    // local.set ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // local.set ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // local.set local_index ...
                    // [ safe  ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
                    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                            ::fast_io::mnp::leb128_get(local_index))};

                    if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_local_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
                    }

                    // local.set local_index ...
                    // [     safe          ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

                    // local.set local_index ...
                    // [     safe          ] unsafe (could be the section_end)
                    //                       ^^ code_curr

                    // check the local_index is valid
                    if(local_index >= all_local_count) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_local_index.local_index = local_index;
                        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    curr_operand_stack_value_type curr_local_type{};

                    if(local_index < func_parameter_count_u32)
                    {
                        // function parameter
                        curr_local_type = func_parameter_begin[local_index];
                    }
                    else
                    {
                        // function defined local variable
                        auto tem_local_index{local_index - func_parameter_count_u32};

                        bool found_local{};
                        for(auto const& local_part: curr_code_locals)
                        {
                            if(tem_local_index < local_part.count)
                            {
                                curr_local_type = local_part.type;
                                found_local = true;
                                break;
                            }

                            tem_local_index -= local_part.count;
                        }

                        if(!found_local) [[unlikely]]
                        {
                            // Inconsistency between `all_local_count` and the locals vector; treat as invalid code.
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_local_index.local_index = local_index;
                            err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"local.set", 1uz); }

                    if(auto const value{try_pop_concrete_operand()}; value.from_stack && value.type != curr_local_type) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.local_variable_type_mismatch.local_index = local_index;
                        err.err_selectable.local_variable_type_mismatch.expected_type = to_wasm1_value_type(curr_local_type);
                        err.err_selectable.local_variable_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::local_set_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::local_tee):
                {
                    // local.tee ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // local.tee ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // local.tee local_index ...
                    // [ safe  ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
                    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                            ::fast_io::mnp::leb128_get(local_index))};

                    if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_local_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
                    }

                    // local.tee local_index ...
                    // [     safe          ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

                    // local.tee local_index ...
                    // [     safe          ] unsafe (could be the section_end)
                    //                       ^^ code_curr

                    // check the local_index is valid
                    if(local_index >= all_local_count) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_local_index.local_index = local_index;
                        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    curr_operand_stack_value_type curr_local_type{};

                    if(local_index < func_parameter_count_u32)
                    {
                        // function parameter
                        curr_local_type = func_parameter_begin[local_index];
                    }
                    else
                    {
                        // function defined local variable
                        auto tem_local_index{local_index - func_parameter_count_u32};

                        bool found_local{};
                        for(auto const& local_part: curr_code_locals)
                        {
                            if(tem_local_index < local_part.count)
                            {
                                curr_local_type = local_part.type;
                                found_local = true;
                                break;
                            }

                            tem_local_index -= local_part.count;
                        }

                        if(!found_local) [[unlikely]]
                        {
                            // Inconsistency between `all_local_count` and the locals vector; treat as invalid code.
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_local_index.local_index = local_index;
                            err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    if(concrete_operand_count() == 0uz) [[unlikely]]
                    {
                        // Polymorphic stack: underflow is allowed.
                        if(!is_polymorphic) { report_operand_stack_underflow(op_begin, u8"local.tee", 1uz); }
                        else
                        {
                            // In polymorphic mode, `local.tee` still produces a value of the local's type.
                            // pop t (dismiss), push t (here)
                            operand_stack.push_back({curr_local_type});
                        }
                    }
                    else
                    {
                        auto const value{try_peek_concrete_operand()};
                        if(value.type != curr_local_type) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.local_variable_type_mismatch.local_index = local_index;
                            err.err_selectable.local_variable_type_mismatch.expected_type = to_wasm1_value_type(curr_local_type);
                            err.err_selectable.local_variable_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::local_tee_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::global_get):
                {
                    // global.get ...
                    // [  safe  ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // global.get ...
                    // [ safe   ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // global.get global_index ...
                    // [ safe   ] unsafe (could be the section_end)
                    //            ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 global_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
                    auto const [global_index_next, global_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                              ::fast_io::mnp::leb128_get(global_index))};

                    if(global_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_global_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(global_index_err);
                    }

                    // global.get global_index ...
                    // [     safe            ] unsafe (could be the section_end)
                    //            ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(global_index_next);

                    // global.get global_index ...
                    // [      safe           ] unsafe (could be the section_end)
                    //                         ^^ code_curr

                    // check the global_index is valid
                    if(global_index >= all_global_count) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_global_index.global_index = global_index;
                        err.err_selectable.illegal_global_index.all_global_count = all_global_count;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_global_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    curr_operand_stack_value_type curr_global_type{};
                    if(global_index < imported_global_count)
                    {
                        auto const imported_global_ptr{imported_globals.index_unchecked(global_index)};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                        curr_global_type = imported_global_ptr->imports.storage.global.type;
                    }
                    else
                    {
                        auto const local_global_index{global_index - imported_global_count};
                        curr_global_type = globalsec.local_globals.index_unchecked(local_global_index).global.type;
                    }

                    // global.get always pushes one value of the global's type (even in polymorphic mode)
                    operand_stack.push_back({curr_global_type});

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::global_set):
                {
                    // global.set ...
                    // [  safe  ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // global.set ...
                    // [ safe   ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // global.set global_index ...
                    // [ safe   ] unsafe (could be the section_end)
                    //            ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 global_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
                    auto const [global_index_next, global_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                              ::fast_io::mnp::leb128_get(global_index))};

                    if(global_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_global_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(global_index_err);
                    }

                    // global.set global_index ...
                    // [     safe            ] unsafe (could be the section_end)
                    //            ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(global_index_next);

                    // global.set global_index ...
                    // [      safe           ] unsafe (could be the section_end)
                    //                         ^^ code_curr

                    // Validate global_index range (imports + local globals)
                    if(global_index >= all_global_count) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_global_index.global_index = global_index;
                        err.err_selectable.illegal_global_index.all_global_count = all_global_count;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_global_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Resolve the global's value type and mutability for global.set
                    curr_operand_stack_value_type curr_global_type{};

                    bool curr_global_mutable{};
                    if(global_index < imported_global_count)
                    {
                        auto const imported_global_ptr{imported_globals.index_unchecked(global_index)};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                        auto const& imported_global{imported_global_ptr->imports.storage.global};
                        curr_global_type = imported_global.type;
                        curr_global_mutable = imported_global.is_mutable;
                    }
                    else
                    {
                        auto const local_global_index{global_index - imported_global_count};
                        auto const& local_global{globalsec.local_globals.index_unchecked(local_global_index).global};
                        curr_global_type = local_global.type;
                        curr_global_mutable = local_global.is_mutable;
                    }

                    // global.set requires the target global to be mutable (immutable globals cannot be written)
                    if(!curr_global_mutable) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.immutable_global_set.global_index = global_index;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::immutable_global_set;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (value) -> () where value must match global's value type
                    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"global.set", 1uz); }

                    if(auto const value{try_pop_concrete_operand()}; value.from_stack && value.type != curr_global_type) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.global_variable_type_mismatch.global_index = global_index;
                        err.err_selectable.global_variable_type_mismatch.expected_type = to_wasm1_value_type(curr_global_type);
                        err.err_selectable.global_variable_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::global_set_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_load):
                {
                    validate_mem_load(u8"i32.load", 2u, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_load):
                {
                    validate_mem_load(u8"i64.load", 3u, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_load):
                {
                    validate_mem_load(u8"f32.load", 2u, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_load):
                {
                    validate_mem_load(u8"f64.load", 3u, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_load8_s):
                {
                    validate_mem_load(u8"i32.load8_s", 0u, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_load8_u):
                {
                    validate_mem_load(u8"i32.load8_u", 0u, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_load16_s):
                {
                    validate_mem_load(u8"i32.load16_s", 1u, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_load16_u):
                {
                    validate_mem_load(u8"i32.load16_u", 1u, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_load8_s):
                {
                    validate_mem_load(u8"i64.load8_s", 0u, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_load8_u):
                {
                    validate_mem_load(u8"i64.load8_u", 0u, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_load16_s):
                {
                    validate_mem_load(u8"i64.load16_s", 1u, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_load16_u):
                {
                    validate_mem_load(u8"i64.load16_u", 1u, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_load32_s):
                {
                    validate_mem_load(u8"i64.load32_s", 2u, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_load32_u):
                {
                    validate_mem_load(u8"i64.load32_u", 2u, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_store):
                {
                    validate_mem_store(u8"i32.store", 2u, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_store):
                {
                    validate_mem_store(u8"i64.store", 3u, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_store):
                {
                    validate_mem_store(u8"f32.store", 2u, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_store):
                {
                    validate_mem_store(u8"f64.store", 3u, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_store8):
                {
                    validate_mem_store(u8"i32.store8", 0u, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_store16):
                {
                    validate_mem_store(u8"i32.store16", 1u, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_store8):
                {
                    validate_mem_store(u8"i64.store8", 0u, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_store16):
                {
                    validate_mem_store(u8"i64.store16", 1u, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_store32):
                {
                    validate_mem_store(u8"i64.store32", 2u, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::memory_size):
                {
                    // memory.size memidx ...
                    // [ safe    ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // memory.size memidx ...
                    // [ safe    ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // memory.size memidx ...
                    // [ safe    ] unsafe (could be the section_end)
                    //             ^^ code_curr

                    // In the Wasm MVP binary format, `memory.size` carries a reserved memory index
                    // immediate that must decode to memory index 0. This immediate is still encoded
                    // as an unsigned LEB128 integer, not as a raw fixed byte.
                    //
                    // That distinction matters for validation strictness: the validator must reject
                    // malformed LEB128, but it must not incorrectly require the canonical single-byte
                    // encoding `0x00`. Per the W3C binary integer rules, trailing-zero forms that are
                    // still well-formed LEB128 within the width bounds remain valid encodings of zero.
                    // Therefore the correct MVP check here is:
                    //   1. decode the immediate as `u32` LEB128,
                    //   2. reject malformed encodings as `invalid_memory_index`,
                    //   3. reject decoded non-zero values as `illegal_memory_index`.
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memidx;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [mem_next, mem_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(memidx))};
                    if(mem_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_memory_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(mem_err);
                    }

                    // memory.size memidx ...
                    // [ safe           ] unsafe (could be the section_end)
                    //             ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(mem_next);

                    // memory.size memidx ...
                    // [ safe           ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    // MVP only defines memory 0 here. The binary encoding may use any well-formed
                    // LEB128 representation whose decoded value is zero; the semantic constraint is
                    // on the decoded memidx, not on the byte sequence being exactly `0x00`.
                    if(memidx != 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memory_index.memory_index = memidx;
                        err.err_selectable.illegal_memory_index.all_memory_count = all_memory_count;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_memory_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"memory.size";
                        err.err_selectable.no_memory.align = 0u;
                        err.err_selectable.no_memory.offset = 0u;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: () -> (i32)
                    operand_stack.push_back({curr_operand_stack_value_type::i32});

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::memory_grow):
                {
                    // memory.grow memidx ...
                    // [ safe    ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // memory.grow memidx ...
                    // [ safe    ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // memory.grow memidx ...
                    // [ safe    ] unsafe (could be the section_end)
                    //             ^^ code_curr

                    // `memory.grow` follows the same MVP rule as `memory.size`: the immediate is a
                    // reserved memidx encoded as unsigned LEB128 and it must decode to zero.
                    //
                    // We intentionally validate the decoded value instead of hard-coding a literal
                    // single-byte check, because well-formed non-canonical zero encodings are still
                    // accepted by the Wasm binary integer grammar.
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memidx;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [mem_next, mem_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(memidx))};
                    if(mem_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_memory_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(mem_err);
                    }

                    // memory.grow memidx ...
                    // [        safe    ] unsafe (could be the section_end)
                    //             ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(mem_next);

                    // memory.grow memidx ...
                    // [        safe    ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    // As above, this is a decoded-value check, not a raw-byte check.
                    if(memidx != 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memory_index.memory_index = memidx;
                        err.err_selectable.illegal_memory_index.all_memory_count = all_memory_count;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_memory_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"memory.grow";
                        err.err_selectable.no_memory.align = 0u;
                        err.err_selectable.no_memory.offset = 0u;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 delta_pages) -> (i32 previous_pages_or_minus1)
                    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"memory.grow", 1uz); }

                    if(auto const delta{try_pop_concrete_operand()}; delta.from_stack && delta.type != curr_operand_stack_value_type::i32) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.memory_grow_delta_type_not_i32.delta_type = to_wasm1_value_type(delta.type);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::memory_grow_delta_type_not_i32;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    operand_stack.push_back({curr_operand_stack_value_type::i32});

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_const):
                {
                    // i32.const i32 ...
                    // [ safe  ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i32.const i32 ...
                    // [ safe  ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i32.const i32 ...
                    // [ safe  ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 imm;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(imm))};
                    if(imm_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.invalid_const_immediate.op_code_name = u8"i32.const";
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_const_immediate;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(imm_err);
                    }

                    // i32.const i32 ...
                    // [    safe   ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(imm_next);

                    // i32.const i32 ...
                    // [    safe   ] unsafe (could be the section_end)
                    //               ^^ code_curr

                    // Stack effect: () -> (i32)
                    operand_stack.push_back({curr_operand_stack_value_type::i32});

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_const):
                {
                    // i64.const i64 ...
                    // [ safe  ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i64.const i64 ...
                    // [ safe  ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i64.const i64 ...
                    // [ safe  ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 imm;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(imm))};
                    if(imm_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.invalid_const_immediate.op_code_name = u8"i64.const";
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_const_immediate;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(imm_err);
                    }

                    // i64.const i64 ...
                    // [     safe  ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(imm_next);

                    // i64.const i64 ...
                    // [     safe  ] unsafe (could be the section_end)
                    //               ^^ code_curr

                    // Stack effect: () -> (i64)
                    operand_stack.push_back({curr_operand_stack_value_type::i64});

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_const):
                {
                    // f32.const f32 ...
                    // [ safe  ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // f32.const f32 ...
                    // [ safe  ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // f32.const f32 ...
                    // [ safe  ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    if(static_cast<::std::size_t>(code_end - code_curr) < sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32)) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.invalid_const_immediate.op_code_name = u8"f32.const";
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_const_immediate;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
                    }

                    // f32.const f32 ...
                    // [ safe      ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    code_curr += sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32);

                    // f32.const f32 ...
                    // [ safe      ] unsafe (could be the section_end)
                    //               ^^ code_curr

                    // Stack effect: () -> (f32)
                    operand_stack.push_back({curr_operand_stack_value_type::f32});

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_const):
                {
                    // f64.const f64 ...
                    // [ safe  ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // f64.const f64 ...
                    // [ safe  ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // f64.const f64 ...
                    // [ safe  ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    if(static_cast<::std::size_t>(code_end - code_curr) < sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64)) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.invalid_const_immediate.op_code_name = u8"f64.const";
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_const_immediate;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
                    }

                    // f64.const f64 ...
                    // [     safe  ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    code_curr += sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64);

                    // f64.const f64 ...
                    // [     safe  ] unsafe (could be the section_end)
                    //               ^^ code_curr

                    // Stack effect: () -> (f64)
                    operand_stack.push_back({curr_operand_stack_value_type::f64});

                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_eqz):
                {
                    validate_numeric_unary(u8"i32.eqz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_eq):
                {
                    validate_numeric_binary(u8"i32.eq", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_ne):
                {
                    validate_numeric_binary(u8"i32.ne", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_lt_s):
                {
                    validate_numeric_binary(u8"i32.lt_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_lt_u):
                {
                    validate_numeric_binary(u8"i32.lt_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_gt_s):
                {
                    validate_numeric_binary(u8"i32.gt_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_gt_u):
                {
                    validate_numeric_binary(u8"i32.gt_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_le_s):
                {
                    validate_numeric_binary(u8"i32.le_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_le_u):
                {
                    validate_numeric_binary(u8"i32.le_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_ge_s):
                {
                    validate_numeric_binary(u8"i32.ge_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_ge_u):
                {
                    validate_numeric_binary(u8"i32.ge_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_eqz):
                {
                    validate_numeric_unary(u8"i64.eqz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_eq):
                {
                    validate_numeric_binary(u8"i64.eq", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_ne):
                {
                    validate_numeric_binary(u8"i64.ne", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_lt_s):
                {
                    validate_numeric_binary(u8"i64.lt_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_lt_u):
                {
                    validate_numeric_binary(u8"i64.lt_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_gt_s):
                {
                    validate_numeric_binary(u8"i64.gt_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_gt_u):
                {
                    validate_numeric_binary(u8"i64.gt_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_le_s):
                {
                    validate_numeric_binary(u8"i64.le_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_le_u):
                {
                    validate_numeric_binary(u8"i64.le_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_ge_s):
                {
                    validate_numeric_binary(u8"i64.ge_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_ge_u):
                {
                    validate_numeric_binary(u8"i64.ge_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_eq):
                {
                    validate_numeric_binary(u8"f32.eq", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_ne):
                {
                    validate_numeric_binary(u8"f32.ne", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_lt):
                {
                    validate_numeric_binary(u8"f32.lt", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_gt):
                {
                    validate_numeric_binary(u8"f32.gt", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_le):
                {
                    validate_numeric_binary(u8"f32.le", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_ge):
                {
                    validate_numeric_binary(u8"f32.ge", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_eq):
                {
                    validate_numeric_binary(u8"f64.eq", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_ne):
                {
                    validate_numeric_binary(u8"f64.ne", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_lt):
                {
                    validate_numeric_binary(u8"f64.lt", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_gt):
                {
                    validate_numeric_binary(u8"f64.gt", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_le):
                {
                    validate_numeric_binary(u8"f64.le", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_ge):
                {
                    validate_numeric_binary(u8"f64.ge", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_clz):
                {
                    validate_numeric_unary(u8"i32.clz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_ctz):
                {
                    validate_numeric_unary(u8"i32.ctz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_popcnt):
                {
                    validate_numeric_unary(u8"i32.popcnt", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_add):
                {
                    validate_numeric_binary(u8"i32.add", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_sub):
                {
                    validate_numeric_binary(u8"i32.sub", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_mul):
                {
                    validate_numeric_binary(u8"i32.mul", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_div_s):
                {
                    validate_numeric_binary(u8"i32.div_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_div_u):
                {
                    validate_numeric_binary(u8"i32.div_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_rem_s):
                {
                    validate_numeric_binary(u8"i32.rem_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_rem_u):
                {
                    validate_numeric_binary(u8"i32.rem_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_and):
                {
                    validate_numeric_binary(u8"i32.and", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_or):
                {
                    validate_numeric_binary(u8"i32.or", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_xor):
                {
                    validate_numeric_binary(u8"i32.xor", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_shl):
                {
                    validate_numeric_binary(u8"i32.shl", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_shr_s):
                {
                    validate_numeric_binary(u8"i32.shr_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_shr_u):
                {
                    validate_numeric_binary(u8"i32.shr_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_rotl):
                {
                    validate_numeric_binary(u8"i32.rotl", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_rotr):
                {
                    validate_numeric_binary(u8"i32.rotr", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_clz):
                {
                    validate_numeric_unary(u8"i64.clz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_ctz):
                {
                    validate_numeric_unary(u8"i64.ctz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_popcnt):
                {
                    validate_numeric_unary(u8"i64.popcnt", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_add):
                {
                    validate_numeric_binary(u8"i64.add", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_sub):
                {
                    validate_numeric_binary(u8"i64.sub", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_mul):
                {
                    validate_numeric_binary(u8"i64.mul", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_div_s):
                {
                    validate_numeric_binary(u8"i64.div_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_div_u):
                {
                    validate_numeric_binary(u8"i64.div_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_rem_s):
                {
                    validate_numeric_binary(u8"i64.rem_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_rem_u):
                {
                    validate_numeric_binary(u8"i64.rem_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_and):
                {
                    validate_numeric_binary(u8"i64.and", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_or):
                {
                    validate_numeric_binary(u8"i64.or", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_xor):
                {
                    validate_numeric_binary(u8"i64.xor", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_shl):
                {
                    validate_numeric_binary(u8"i64.shl", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_shr_s):
                {
                    validate_numeric_binary(u8"i64.shr_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_shr_u):
                {
                    validate_numeric_binary(u8"i64.shr_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_rotl):
                {
                    validate_numeric_binary(u8"i64.rotl", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_rotr):
                {
                    validate_numeric_binary(u8"i64.rotr", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_abs):
                {
                    validate_numeric_unary(u8"f32.abs", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_neg):
                {
                    validate_numeric_unary(u8"f32.neg", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_ceil):
                {
                    validate_numeric_unary(u8"f32.ceil", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_floor):
                {
                    validate_numeric_unary(u8"f32.floor", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_trunc):
                {
                    validate_numeric_unary(u8"f32.trunc", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_nearest):
                {
                    validate_numeric_unary(u8"f32.nearest", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_sqrt):
                {
                    validate_numeric_unary(u8"f32.sqrt", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_add):
                {
                    validate_numeric_binary(u8"f32.add", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_sub):
                {
                    validate_numeric_binary(u8"f32.sub", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_mul):
                {
                    validate_numeric_binary(u8"f32.mul", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_div):
                {
                    validate_numeric_binary(u8"f32.div", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_min):
                {
                    validate_numeric_binary(u8"f32.min", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_max):
                {
                    validate_numeric_binary(u8"f32.max", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_copysign):
                {
                    validate_numeric_binary(u8"f32.copysign", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_abs):
                {
                    validate_numeric_unary(u8"f64.abs", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_neg):
                {
                    validate_numeric_unary(u8"f64.neg", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_ceil):
                {
                    validate_numeric_unary(u8"f64.ceil", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_floor):
                {
                    validate_numeric_unary(u8"f64.floor", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_trunc):
                {
                    validate_numeric_unary(u8"f64.trunc", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_nearest):
                {
                    validate_numeric_unary(u8"f64.nearest", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_sqrt):
                {
                    validate_numeric_unary(u8"f64.sqrt", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_add):
                {
                    validate_numeric_binary(u8"f64.add", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_sub):
                {
                    validate_numeric_binary(u8"f64.sub", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_mul):
                {
                    validate_numeric_binary(u8"f64.mul", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_div):
                {
                    validate_numeric_binary(u8"f64.div", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_min):
                {
                    validate_numeric_binary(u8"f64.min", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_max):
                {
                    validate_numeric_binary(u8"f64.max", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_copysign):
                {
                    validate_numeric_binary(u8"f64.copysign", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_wrap_i64):
                {
                    validate_numeric_unary(u8"i32.wrap_i64", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_trunc_f32_s):
                {
                    validate_numeric_unary(u8"i32.trunc_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_trunc_f32_u):
                {
                    validate_numeric_unary(u8"i32.trunc_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_trunc_f64_s):
                {
                    validate_numeric_unary(u8"i32.trunc_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_trunc_f64_u):
                {
                    validate_numeric_unary(u8"i32.trunc_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_extend_i32_s):
                {
                    validate_numeric_unary(u8"i64.extend_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_extend_i32_u):
                {
                    validate_numeric_unary(u8"i64.extend_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_trunc_f32_s):
                {
                    validate_numeric_unary(u8"i64.trunc_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_trunc_f32_u):
                {
                    validate_numeric_unary(u8"i64.trunc_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_trunc_f64_s):
                {
                    validate_numeric_unary(u8"i64.trunc_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_trunc_f64_u):
                {
                    validate_numeric_unary(u8"i64.trunc_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_convert_i32_s):
                {
                    validate_numeric_unary(u8"f32.convert_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_convert_i32_u):
                {
                    validate_numeric_unary(u8"f32.convert_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_convert_i64_s):
                {
                    validate_numeric_unary(u8"f32.convert_i64_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_convert_i64_u):
                {
                    validate_numeric_unary(u8"f32.convert_i64_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_demote_f64):
                {
                    validate_numeric_unary(u8"f32.demote_f64", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_convert_i32_s):
                {
                    validate_numeric_unary(u8"f64.convert_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_convert_i32_u):
                {
                    validate_numeric_unary(u8"f64.convert_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_convert_i64_s):
                {
                    validate_numeric_unary(u8"f64.convert_i64_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_convert_i64_u):
                {
                    validate_numeric_unary(u8"f64.convert_i64_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_promote_f32):
                {
                    validate_numeric_unary(u8"f64.promote_f32", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_reinterpret_f32):
                {
                    validate_numeric_unary(u8"i32.reinterpret_f32", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_reinterpret_f64):
                {
                    validate_numeric_unary(u8"i64.reinterpret_f64", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f32_reinterpret_i32):
                {
                    validate_numeric_unary(u8"f32.reinterpret_i32", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::f64_reinterpret_i64):
                {
                    validate_numeric_unary(u8"f64.reinterpret_i64", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);
                    break;
                }
                case opcode_byte(wasm1p1_code::i32_extend8_s):
                {
                    if(!wasm1p1_para.enable_sign_extension) [[unlikely]]
                    {
                        details::fail_feature_required(code_curr,
                                                       err,
                                                       opcode_u32(wasm1p1_code::i32_extend8_s),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                    }
                    validate_numeric_unary(u8"i32.extend8_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case opcode_byte(wasm1p1_code::i32_extend16_s):
                {
                    if(!wasm1p1_para.enable_sign_extension) [[unlikely]]
                    {
                        details::fail_feature_required(code_curr,
                                                       err,
                                                       opcode_u32(wasm1p1_code::i32_extend16_s),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                    }
                    validate_numeric_unary(u8"i32.extend16_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                    break;
                }
                case opcode_byte(wasm1p1_code::i64_extend8_s):
                {
                    if(!wasm1p1_para.enable_sign_extension) [[unlikely]]
                    {
                        details::fail_feature_required(code_curr,
                                                       err,
                                                       opcode_u32(wasm1p1_code::i64_extend8_s),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                    }
                    validate_numeric_unary(u8"i64.extend8_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case opcode_byte(wasm1p1_code::i64_extend16_s):
                {
                    if(!wasm1p1_para.enable_sign_extension) [[unlikely]]
                    {
                        details::fail_feature_required(code_curr,
                                                       err,
                                                       opcode_u32(wasm1p1_code::i64_extend16_s),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                    }
                    validate_numeric_unary(u8"i64.extend16_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case opcode_byte(wasm1p1_code::i64_extend32_s):
                {
                    if(!wasm1p1_para.enable_sign_extension) [[unlikely]]
                    {
                        details::fail_feature_required(code_curr,
                                                       err,
                                                       opcode_u32(wasm1p1_code::i64_extend32_s),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                    }
                    validate_numeric_unary(u8"i64.extend32_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                    break;
                }
                case opcode_byte(wasm1p1_code::ref_null):
                {
                    // ref.null reftype ...
                    // [  safe ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};
                    ++code_curr;

                    // ref.null reftype ...
                    // [  safe ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    if(!wasm1p1_para.enable_reference_types) [[unlikely]]
                    {
                        details::fail_feature_required(op_begin,
                                                       err,
                                                       opcode_u32(wasm1p1_code::ref_null),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null);
                    }

                    auto const rt_byte{details::read_u8(code_curr, code_end, op_begin, err, u8"ref.null")};
                    auto const rt{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type>(rt_byte)};
                    using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
                    if(rt != reference_type::funcref && rt != reference_type::externref) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.wasm1p1_invalid_reference_type.value = rt_byte;
                        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const vt{static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(rt))};
                    ensure_wasm1p1_value_type_enabled(op_begin, vt, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type);
                    operand_stack.push_back({vt});
                    break;
                }
                case opcode_byte(wasm1p1_code::ref_is_null):
                {
                    // ref.is_null ...
                    // [  safe   ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};
                    ++code_curr;

                    // ref.is_null ...
                    // [  safe   ] unsafe (could be the section_end)
                    //            ^^ code_curr

                    if(!wasm1p1_para.enable_reference_types) [[unlikely]]
                    {
                        details::fail_feature_required(op_begin,
                                                       err,
                                                       opcode_u32(wasm1p1_code::ref_is_null),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type);
                    }

                    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"ref.is_null", 1uz); }

                    auto const ref_value{try_pop_concrete_operand()};
                    if(ref_value.from_stack && !is_reference_value_type(ref_value.type)) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.wasm1p1_invalid_reference_type.value =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(ref_value.type);
                        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    operand_stack.push_back({curr_operand_stack_value_type::i32});
                    break;
                }
                case opcode_byte(wasm1p1_code::ref_func):
                {
                    // ref.func funcidx ...
                    // [  safe ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};
                    ++code_curr;

                    // ref.func funcidx ...
                    // [  safe ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    if(!wasm1p1_para.enable_reference_types) [[unlikely]]
                    {
                        details::fail_feature_required(op_begin,
                                                       err,
                                                       opcode_u32(wasm1p1_code::ref_func),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func);
                    }

                    auto const func_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                        code_curr, code_end, op_begin, err, u8"ref.func")};
                    check_ref_func_index(op_begin, func_index);
                    operand_stack.push_back(
                        {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref)});
                    break;
                }
                case opcode_byte(wasm1p1_code::numeric_prefix):
                {
                    // numeric_prefix subopcode ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};
                    ++code_curr;

                    // numeric_prefix subopcode ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    auto const subopcode{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                        code_curr, code_end, op_begin, err, u8"numeric_prefix")};
                    auto const numeric_code{static_cast<wasm1p1_numeric_code>(subopcode)};

                    switch(numeric_code)
                    {
                        case wasm1p1_numeric_code::i32_trunc_sat_f32_s:
                        {
                            if(!wasm1p1_para.enable_nontrapping_float_to_int) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin, u8"i32.trunc_sat_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                            break;
                        }
                        case wasm1p1_numeric_code::i32_trunc_sat_f32_u:
                        {
                            if(!wasm1p1_para.enable_nontrapping_float_to_int) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin, u8"i32.trunc_sat_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                            break;
                        }
                        case wasm1p1_numeric_code::i32_trunc_sat_f64_s:
                        {
                            if(!wasm1p1_para.enable_nontrapping_float_to_int) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin, u8"i32.trunc_sat_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                            break;
                        }
                        case wasm1p1_numeric_code::i32_trunc_sat_f64_u:
                        {
                            if(!wasm1p1_para.enable_nontrapping_float_to_int) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin, u8"i32.trunc_sat_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                            break;
                        }
                        case wasm1p1_numeric_code::i64_trunc_sat_f32_s:
                        {
                            if(!wasm1p1_para.enable_nontrapping_float_to_int) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin, u8"i64.trunc_sat_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);
                            break;
                        }
                        case wasm1p1_numeric_code::i64_trunc_sat_f32_u:
                        {
                            if(!wasm1p1_para.enable_nontrapping_float_to_int) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin, u8"i64.trunc_sat_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);
                            break;
                        }
                        case wasm1p1_numeric_code::i64_trunc_sat_f64_s:
                        {
                            if(!wasm1p1_para.enable_nontrapping_float_to_int) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin, u8"i64.trunc_sat_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
                            break;
                        }
                        case wasm1p1_numeric_code::i64_trunc_sat_f64_u:
                        {
                            if(!wasm1p1_para.enable_nontrapping_float_to_int) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin, u8"i64.trunc_sat_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
                            break;
                        }
                        case wasm1p1_numeric_code::memory_init:
                        {
                            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment);
                            }
                            auto const data_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                                code_curr, code_end, op_begin, err, u8"memory.init.dataidx")};
                            check_data_index(op_begin, data_index);
                            auto const memory_index{details::read_u8(code_curr, code_end, op_begin, err, u8"memory.init.memidx")};
                            check_memory_index_zero(op_begin, memory_index, u8"memory.init");
                            validate_i32_operands(op_begin, u8"memory.init", 3uz);
                            break;
                        }
                        case wasm1p1_numeric_code::data_drop:
                        {
                            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment);
                            }
                            auto const data_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                                code_curr, code_end, op_begin, err, u8"data.drop")};
                            check_data_index(op_begin, data_index);
                            break;
                        }
                        case wasm1p1_numeric_code::memory_copy:
                        {
                            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            auto const dst_memory_index{details::read_u8(code_curr, code_end, op_begin, err, u8"memory.copy.dst")};
                            check_memory_index_zero(op_begin, dst_memory_index, u8"memory.copy");
                            auto const src_memory_index{details::read_u8(code_curr, code_end, op_begin, err, u8"memory.copy.src")};
                            check_memory_index_zero(op_begin, src_memory_index, u8"memory.copy");
                            validate_i32_operands(op_begin, u8"memory.copy", 3uz);
                            break;
                        }
                        case wasm1p1_numeric_code::memory_fill:
                        {
                            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            auto const memory_index{details::read_u8(code_curr, code_end, op_begin, err, u8"memory.fill")};
                            check_memory_index_zero(op_begin, memory_index, u8"memory.fill");
                            validate_i32_operands(op_begin, u8"memory.fill", 3uz);
                            break;
                        }
                        case wasm1p1_numeric_code::table_init:
                        {
                            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment);
                            }
                            auto const element_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                                code_curr, code_end, op_begin, err, u8"table.init.elemidx")};
                            check_element_index(op_begin, element_index);
                            auto const table_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                                code_curr, code_end, op_begin, err, u8"table.init.tableidx")};
                            check_table_index(op_begin, table_index);

                            auto const element_value_type{static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(
                                    elemsec.elems.index_unchecked(element_index).storage.segment.reftype))};
                            auto const table_value_type{get_table_value_type(table_index)};
                            if(element_value_type != table_value_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.init";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_value_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(element_value_type);
                                err.err_code = code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            validate_i32_operands(op_begin, u8"table.init", 3uz);
                            break;
                        }
                        case wasm1p1_numeric_code::elem_drop:
                        {
                            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment);
                            }
                            auto const element_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                                code_curr, code_end, op_begin, err, u8"elem.drop")};
                            check_element_index(op_begin, element_index);
                            break;
                        }
                        case wasm1p1_numeric_code::table_copy:
                        {
                            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            auto const dst_table_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                                code_curr, code_end, op_begin, err, u8"table.copy.dst")};
                            check_table_index(op_begin, dst_table_index);
                            auto const src_table_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                                code_curr, code_end, op_begin, err, u8"table.copy.src")};
                            check_table_index(op_begin, src_table_index);

                            auto const dst_type{get_table_value_type(dst_table_index)};
                            auto const src_type{get_table_value_type(src_table_index)};
                            if(dst_type != src_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.copy";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(dst_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(src_type);
                                err.err_code = code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            validate_i32_operands(op_begin, u8"table.copy", 3uz);
                            break;
                        }
                        case wasm1p1_numeric_code::table_grow:
                        {
                            if(!wasm1p1_para.enable_reference_types) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            auto const table_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                                code_curr, code_end, op_begin, err, u8"table.grow")};
                            check_table_index(op_begin, table_index);
                            auto const table_type{get_table_value_type(table_index)};

                            if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"table.grow", 2uz); }

                            auto const delta{try_pop_concrete_operand()};
                            if(delta.from_stack && delta.type != curr_operand_stack_value_type::i32) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.grow";
                                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i32);
                                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(delta.type);
                                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            auto const value{try_pop_concrete_operand()};
                            if(value.from_stack && value.type != table_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.grow";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                                err.err_code = code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            operand_stack.push_back({curr_operand_stack_value_type::i32});
                            break;
                        }
                        case wasm1p1_numeric_code::table_size:
                        {
                            if(!wasm1p1_para.enable_reference_types) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            auto const table_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                                code_curr, code_end, op_begin, err, u8"table.size")};
                            check_table_index(op_begin, table_index);
                            operand_stack.push_back({curr_operand_stack_value_type::i32});
                            break;
                        }
                        case wasm1p1_numeric_code::table_fill:
                        {
                            if(!wasm1p1_para.enable_reference_types) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            auto const table_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                                code_curr, code_end, op_begin, err, u8"table.fill")};
                            check_table_index(op_begin, table_index);
                            auto const table_type{get_table_value_type(table_index)};

                            if(!is_polymorphic && concrete_operand_count() < 3uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"table.fill", 3uz); }

                            auto const len{try_pop_concrete_operand()};
                            if(len.from_stack && len.type != curr_operand_stack_value_type::i32) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.fill";
                                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i32);
                                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(len.type);
                                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            auto const value{try_pop_concrete_operand()};
                            if(value.from_stack && value.type != table_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.fill";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                                err.err_code = code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            auto const index{try_pop_concrete_operand()};
                            if(index.from_stack && index.type != curr_operand_stack_value_type::i32) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.fill";
                                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i32);
                                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(index.type);
                                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            break;
                        }
                        [[unlikely]] default:
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(subopcode);
                            err.err_code = code_validation_error_code::illegal_opbase;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    break;
                }
                case opcode_byte(wasm1p1_code::simd_prefix):
                {
                    // simd_prefix subopcode ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};
                    ++code_curr;

                    // simd_prefix subopcode ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    if(!wasm1p1_para.enable_simd) [[unlikely]]
                    {
                        details::fail_feature_required(op_begin,
                                                       err,
                                                       opcode_u32(wasm1p1_code::simd_prefix),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const);
                    }

                    auto const simd_subopcode{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                        code_curr, code_end, op_begin, err, u8"simd")};
                    auto const simd_code{static_cast<wasm1p1_simd_code>(simd_subopcode)};

                    auto const validate_simd_memarg{
                        [&](::uwvm2::utils::container::u8string_view op_name,
                            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 max_align) constexpr UWVM_THROWS
                        {
                            auto const align{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                                code_curr, code_end, op_begin, err, u8"simd.memarg.align")};
                            auto const offset{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                                code_curr, code_end, op_begin, err, u8"simd.memarg.offset")};

                            if(all_memory_count == 0u) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.no_memory.op_code_name = op_name;
                                err.err_selectable.no_memory.align = align;
                                err.err_selectable.no_memory.offset = offset;
                                err.err_code = code_validation_error_code::no_memory;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            if(align > max_align) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.illegal_memarg_alignment.op_code_name = op_name;
                                err.err_selectable.illegal_memarg_alignment.align = align;
                                err.err_selectable.illegal_memarg_alignment.max_align = max_align;
                                err.err_code = code_validation_error_code::illegal_memarg_alignment;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }};

                    auto const check_lane_index{
                        [&](::uwvm2::utils::container::u8string_view op_name,
                            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte lane,
                            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte lane_count) constexpr UWVM_THROWS
                        {
                            if(lane >= lane_count) [[unlikely]] { details::fail_invalid_immediate(op_begin, err, op_name); }
                        }};

                    auto const simd_v128_type{
                        static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)};
                    curr_operand_stack_value_type v128_v128_v128_operands[3]{simd_v128_type, simd_v128_type, simd_v128_type};

                    auto const push_simd_v128{[&]() constexpr { operand_stack.push_back({simd_v128_type}); }};

                    auto const validate_simd_unary_v128{
                        [&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                        {
                            pop_expected_operands(op_begin, op_name, {v128_result_arr, v128_result_arr + 1u});
                            push_simd_v128();
                        }};

                    auto const validate_simd_binary_v128{
                        [&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                        {
                            pop_expected_operands(op_begin, op_name, {v128_v128_operands, v128_v128_operands + 2u});
                            push_simd_v128();
                        }};

                    auto const validate_simd_ternary_v128{
                        [&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                        {
                            pop_expected_operands(op_begin, op_name, {v128_v128_v128_operands, v128_v128_v128_operands + 3u});
                            push_simd_v128();
                        }};

                    auto const validate_simd_test_v128{
                        [&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                        {
                            pop_expected_operands(op_begin, op_name, {v128_result_arr, v128_result_arr + 1u});
                            operand_stack.push_back({curr_operand_stack_value_type::i32});
                        }};

                    auto const validate_simd_shift_v128{
                        [&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                        {
                            pop_expected_operands(op_begin, op_name, {v128_i32_operands, v128_i32_operands + 2u});
                            push_simd_v128();
                        }};

                    auto const validate_simd_splat{
                        [&](::uwvm2::utils::container::u8string_view op_name, curr_operand_stack_value_type scalar_type) constexpr UWVM_THROWS
                        { validate_numeric_unary_stack_effect(op_begin, op_name, scalar_type, simd_v128_type); }};

                    switch(simd_code)
                    {
                        case wasm1p1_simd_code::v128_load:
                        {
                            validate_simd_memarg(u8"v128.load", 4u);
                            validate_i32_operands(op_begin, u8"v128.load", 1uz);
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load8x8_s: [[fallthrough]];
                        case wasm1p1_simd_code::v128_load8x8_u:
                        {
                            validate_simd_memarg(u8"v128.load8x8", 3u);
                            validate_i32_operands(op_begin, u8"v128.load8x8", 1uz);
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load16x4_s: [[fallthrough]];
                        case wasm1p1_simd_code::v128_load16x4_u:
                        {
                            validate_simd_memarg(u8"v128.load16x4", 3u);
                            validate_i32_operands(op_begin, u8"v128.load16x4", 1uz);
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load32x2_s: [[fallthrough]];
                        case wasm1p1_simd_code::v128_load32x2_u:
                        {
                            validate_simd_memarg(u8"v128.load32x2", 3u);
                            validate_i32_operands(op_begin, u8"v128.load32x2", 1uz);
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load8_splat:
                        {
                            validate_simd_memarg(u8"v128.load8_splat", 0u);
                            validate_i32_operands(op_begin, u8"v128.load8_splat", 1uz);
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load16_splat:
                        {
                            validate_simd_memarg(u8"v128.load16_splat", 1u);
                            validate_i32_operands(op_begin, u8"v128.load16_splat", 1uz);
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load32_splat:
                        {
                            validate_simd_memarg(u8"v128.load32_splat", 2u);
                            validate_i32_operands(op_begin, u8"v128.load32_splat", 1uz);
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load64_splat:
                        {
                            validate_simd_memarg(u8"v128.load64_splat", 3u);
                            validate_i32_operands(op_begin, u8"v128.load64_splat", 1uz);
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_store:
                        {
                            validate_simd_memarg(u8"v128.store", 4u);
                            pop_expected_operands(op_begin, u8"v128.store", {i32_v128_operands, i32_v128_operands + 2u});
                            break;
                        }
                        case wasm1p1_simd_code::v128_const:
                        {
                            details::skip_bytes(code_curr, code_end, op_begin, 16uz, err, u8"v128.const");
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::i8x16_shuffle:
                        {
                            details::skip_bytes(code_curr, code_end, op_begin, 16uz, err, u8"i8x16.shuffle");
                            pop_expected_operands(op_begin, u8"i8x16.shuffle", {v128_v128_operands, v128_v128_operands + 2u});
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::i8x16_swizzle:
                        {
                            validate_simd_binary_v128(u8"i8x16.swizzle");
                            break;
                        }
                        case wasm1p1_simd_code::i8x16_splat:
                        {
                            validate_simd_splat(u8"i8x16.splat", curr_operand_stack_value_type::i32);
                            break;
                        }
                        case wasm1p1_simd_code::i16x8_splat:
                        {
                            validate_simd_splat(u8"i16x8.splat", curr_operand_stack_value_type::i32);
                            break;
                        }
                        case wasm1p1_simd_code::i32x4_splat:
                        {
                            validate_simd_splat(u8"i32x4.splat", curr_operand_stack_value_type::i32);
                            break;
                        }
                        case wasm1p1_simd_code::i64x2_splat:
                        {
                            validate_simd_splat(u8"i64x2.splat", curr_operand_stack_value_type::i64);
                            break;
                        }
                        case wasm1p1_simd_code::f32x4_splat:
                        {
                            validate_simd_splat(u8"f32x4.splat", curr_operand_stack_value_type::f32);
                            break;
                        }
                        case wasm1p1_simd_code::f64x2_splat:
                        {
                            validate_simd_splat(u8"f64x2.splat", curr_operand_stack_value_type::f64);
                            break;
                        }
                        case wasm1p1_simd_code::i8x16_extract_lane_s: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_extract_lane_u:
                        {
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"i8x16.extract_lane")};
                            check_lane_index(u8"i8x16.extract_lane", lane, 16u);
                            pop_expected_operands(op_begin, u8"i8x16.extract_lane", {v128_result_arr, v128_result_arr + 1u});
                            operand_stack.push_back({curr_operand_stack_value_type::i32});
                            break;
                        }
                        case wasm1p1_simd_code::i8x16_replace_lane:
                        {
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"i8x16.replace_lane")};
                            check_lane_index(u8"i8x16.replace_lane", lane, 16u);
                            pop_expected_operands(op_begin, u8"i8x16.replace_lane", {v128_i32_operands, v128_i32_operands + 2u});
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::i16x8_extract_lane_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_extract_lane_u:
                        {
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"i16x8.extract_lane")};
                            check_lane_index(u8"i16x8.extract_lane", lane, 8u);
                            pop_expected_operands(op_begin, u8"i16x8.extract_lane", {v128_result_arr, v128_result_arr + 1u});
                            operand_stack.push_back({curr_operand_stack_value_type::i32});
                            break;
                        }
                        case wasm1p1_simd_code::i16x8_replace_lane:
                        {
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"i16x8.replace_lane")};
                            check_lane_index(u8"i16x8.replace_lane", lane, 8u);
                            pop_expected_operands(op_begin, u8"i16x8.replace_lane", {v128_i32_operands, v128_i32_operands + 2u});
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::i32x4_extract_lane:
                        {
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"i32x4.extract_lane")};
                            check_lane_index(u8"i32x4.extract_lane", lane, 4u);
                            pop_expected_operands(op_begin, u8"i32x4.extract_lane", {v128_result_arr, v128_result_arr + 1u});
                            operand_stack.push_back({curr_operand_stack_value_type::i32});
                            break;
                        }
                        case wasm1p1_simd_code::i32x4_replace_lane:
                        {
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"i32x4.replace_lane")};
                            check_lane_index(u8"i32x4.replace_lane", lane, 4u);
                            pop_expected_operands(op_begin, u8"i32x4.replace_lane", {v128_i32_operands, v128_i32_operands + 2u});
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::i64x2_extract_lane:
                        {
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"i64x2.extract_lane")};
                            check_lane_index(u8"i64x2.extract_lane", lane, 2u);
                            pop_expected_operands(op_begin, u8"i64x2.extract_lane", {v128_result_arr, v128_result_arr + 1u});
                            operand_stack.push_back({curr_operand_stack_value_type::i64});
                            break;
                        }
                        case wasm1p1_simd_code::i64x2_replace_lane:
                        {
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"i64x2.replace_lane")};
                            check_lane_index(u8"i64x2.replace_lane", lane, 2u);
                            pop_expected_operands(op_begin, u8"i64x2.replace_lane", {v128_i64_operands, v128_i64_operands + 2u});
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::f32x4_extract_lane:
                        {
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"f32x4.extract_lane")};
                            check_lane_index(u8"f32x4.extract_lane", lane, 4u);
                            pop_expected_operands(op_begin, u8"f32x4.extract_lane", {v128_result_arr, v128_result_arr + 1u});
                            operand_stack.push_back({curr_operand_stack_value_type::f32});
                            break;
                        }
                        case wasm1p1_simd_code::f32x4_replace_lane:
                        {
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"f32x4.replace_lane")};
                            check_lane_index(u8"f32x4.replace_lane", lane, 4u);
                            pop_expected_operands(op_begin, u8"f32x4.replace_lane", {v128_f32_operands, v128_f32_operands + 2u});
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::f64x2_extract_lane:
                        {
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"f64x2.extract_lane")};
                            check_lane_index(u8"f64x2.extract_lane", lane, 2u);
                            pop_expected_operands(op_begin, u8"f64x2.extract_lane", {v128_result_arr, v128_result_arr + 1u});
                            operand_stack.push_back({curr_operand_stack_value_type::f64});
                            break;
                        }
                        case wasm1p1_simd_code::f64x2_replace_lane:
                        {
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"f64x2.replace_lane")};
                            check_lane_index(u8"f64x2.replace_lane", lane, 2u);
                            pop_expected_operands(op_begin, u8"f64x2.replace_lane", {v128_f64_operands, v128_f64_operands + 2u});
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load8_lane: [[fallthrough]];
                        case wasm1p1_simd_code::v128_load16_lane: [[fallthrough]];
                        case wasm1p1_simd_code::v128_load32_lane: [[fallthrough]];
                        case wasm1p1_simd_code::v128_load64_lane:
                        {
                            auto const max_align{simd_code == wasm1p1_simd_code::v128_load8_lane    ? 0u
                                                 : simd_code == wasm1p1_simd_code::v128_load16_lane ? 1u
                                                 : simd_code == wasm1p1_simd_code::v128_load32_lane ? 2u
                                                                                                    : 3u};
                            validate_simd_memarg(u8"v128.load_lane", max_align);
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"v128.load_lane")};
                            auto const lane_count{simd_code == wasm1p1_simd_code::v128_load8_lane    ? 16u
                                                  : simd_code == wasm1p1_simd_code::v128_load16_lane ? 8u
                                                  : simd_code == wasm1p1_simd_code::v128_load32_lane ? 4u
                                                                                                     : 2u};
                            check_lane_index(u8"v128.load_lane", lane, static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(lane_count));
                            pop_expected_operands(op_begin, u8"v128.load_lane", {i32_v128_operands, i32_v128_operands + 2u});
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_store8_lane: [[fallthrough]];
                        case wasm1p1_simd_code::v128_store16_lane: [[fallthrough]];
                        case wasm1p1_simd_code::v128_store32_lane: [[fallthrough]];
                        case wasm1p1_simd_code::v128_store64_lane:
                        {
                            auto const max_align{simd_code == wasm1p1_simd_code::v128_store8_lane    ? 0u
                                                 : simd_code == wasm1p1_simd_code::v128_store16_lane ? 1u
                                                 : simd_code == wasm1p1_simd_code::v128_store32_lane ? 2u
                                                                                                     : 3u};
                            validate_simd_memarg(u8"v128.store_lane", max_align);
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"v128.store_lane")};
                            auto const lane_count{simd_code == wasm1p1_simd_code::v128_store8_lane    ? 16u
                                                  : simd_code == wasm1p1_simd_code::v128_store16_lane ? 8u
                                                  : simd_code == wasm1p1_simd_code::v128_store32_lane ? 4u
                                                                                                      : 2u};
                            check_lane_index(u8"v128.store_lane", lane, static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(lane_count));
                            pop_expected_operands(op_begin, u8"v128.store_lane", {i32_v128_operands, i32_v128_operands + 2u});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load32_zero:
                        {
                            validate_simd_memarg(u8"v128.load32_zero", 2u);
                            validate_i32_operands(op_begin, u8"v128.load32_zero", 1uz);
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load64_zero:
                        {
                            validate_simd_memarg(u8"v128.load64_zero", 3u);
                            validate_i32_operands(op_begin, u8"v128.load64_zero", 1uz);
                            operand_stack.push_back({static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::f32x4_demote_f64x2_zero:
                        {
                            validate_simd_unary_v128(u8"f32x4.demote_f64x2_zero");
                            break;
                        }
                        case wasm1p1_simd_code::f64x2_promote_low_f32x4:
                        {
                            validate_simd_unary_v128(u8"f64x2.promote_low_f32x4");
                            break;
                        }
                        case wasm1p1_simd_code::i8x16_eq: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_ne: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_lt_s: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_lt_u: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_gt_s: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_gt_u: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_le_s: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_le_u: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_ge_s: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_ge_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_eq: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_ne: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_lt_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_lt_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_gt_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_gt_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_le_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_le_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_ge_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_ge_u: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_eq: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_ne: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_lt_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_lt_u: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_gt_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_gt_u: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_le_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_le_u: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_ge_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_ge_u: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_eq: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_ne: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_lt: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_gt: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_le: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_ge: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_eq: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_ne: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_lt: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_gt: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_le: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_ge: [[fallthrough]];
                        case wasm1p1_simd_code::v128_and: [[fallthrough]];
                        case wasm1p1_simd_code::v128_andnot: [[fallthrough]];
                        case wasm1p1_simd_code::v128_or: [[fallthrough]];
                        case wasm1p1_simd_code::v128_xor:
                        {
                            validate_simd_binary_v128(u8"simd.binary");
                            break;
                        }
                        case wasm1p1_simd_code::v128_not:
                        {
                            validate_simd_unary_v128(u8"v128.not");
                            break;
                        }
                        case wasm1p1_simd_code::v128_bitselect:
                        {
                            validate_simd_ternary_v128(u8"v128.bitselect");
                            break;
                        }
                        case wasm1p1_simd_code::v128_any_true:
                        {
                            validate_simd_test_v128(u8"v128.any_true");
                            break;
                        }
                        case wasm1p1_simd_code::i8x16_abs: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_neg: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_popcnt: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_ceil: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_floor: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_trunc: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_nearest: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_extadd_pairwise_i8x16_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_extadd_pairwise_i8x16_u: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_extadd_pairwise_i16x8_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_extadd_pairwise_i16x8_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_abs: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_neg: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_extend_low_i8x16_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_extend_high_i8x16_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_extend_low_i8x16_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_extend_high_i8x16_u: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_nearest: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_abs: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_neg: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_extend_low_i16x8_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_extend_high_i16x8_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_extend_low_i16x8_u: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_extend_high_i16x8_u: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_abs: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_neg: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_extend_low_i32x4_s: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_extend_high_i32x4_s: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_extend_low_i32x4_u: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_extend_high_i32x4_u: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_abs: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_neg: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_sqrt: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_abs: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_neg: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_sqrt: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_trunc_sat_f32x4_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_trunc_sat_f32x4_u: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_convert_i32x4_s: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_convert_i32x4_u: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_trunc_sat_f64x2_s_zero: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_trunc_sat_f64x2_u_zero: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_convert_low_i32x4_s: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_convert_low_i32x4_u:
                        {
                            validate_simd_unary_v128(u8"simd.unary");
                            break;
                        }
                        case wasm1p1_simd_code::i8x16_all_true: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_bitmask: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_all_true: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_bitmask: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_all_true: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_bitmask: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_all_true: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_bitmask:
                        {
                            validate_simd_test_v128(u8"simd.test");
                            break;
                        }
                        case wasm1p1_simd_code::i8x16_narrow_i16x8_s: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_narrow_i16x8_u: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_add: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_add_sat_s: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_add_sat_u: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_sub: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_sub_sat_s: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_sub_sat_u: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_min_s: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_min_u: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_max_s: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_max_u: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_avgr_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_q15mulr_sat_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_narrow_i32x4_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_narrow_i32x4_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_add: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_add_sat_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_add_sat_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_sub: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_sub_sat_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_sub_sat_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_mul: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_min_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_min_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_max_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_max_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_avgr_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_extmul_low_i8x16_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_extmul_high_i8x16_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_extmul_low_i8x16_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_extmul_high_i8x16_u: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_add: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_sub: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_mul: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_min_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_min_u: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_max_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_max_u: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_dot_i16x8_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_extmul_low_i16x8_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_extmul_high_i16x8_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_extmul_low_i16x8_u: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_extmul_high_i16x8_u: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_add: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_sub: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_mul: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_eq: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_ne: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_lt_s: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_gt_s: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_le_s: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_ge_s: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_extmul_low_i32x4_s: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_extmul_high_i32x4_s: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_extmul_low_i32x4_u: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_extmul_high_i32x4_u: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_add: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_sub: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_mul: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_div: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_min: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_max: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_pmin: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_pmax: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_add: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_sub: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_mul: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_div: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_min: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_max: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_pmin: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_pmax:
                        {
                            validate_simd_binary_v128(u8"simd.binary");
                            break;
                        }
                        case wasm1p1_simd_code::i8x16_shl: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_shr_s: [[fallthrough]];
                        case wasm1p1_simd_code::i8x16_shr_u: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_shl: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_shr_s: [[fallthrough]];
                        case wasm1p1_simd_code::i16x8_shr_u: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_shl: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_shr_s: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_shr_u: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_shl: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_shr_s: [[fallthrough]];
                        case wasm1p1_simd_code::i64x2_shr_u:
                        {
                            validate_simd_shift_v128(u8"simd.shift");
                            break;
                        }
                        [[unlikely]] default:
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(simd_subopcode);
                            err.err_code = code_validation_error_code::illegal_opbase;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    break;
                }
                [[unlikely]] default:
                {
                    err.err_curr = code_curr;
                    err.err_selectable.u8 = static_cast<::std::uint_least8_t>(curr_opbase);
                    err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_opbase;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    break;
                }
            }
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code(::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version code_version,
                                        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
                                        ::std::size_t const function_index,
                                        ::std::byte const* code_begin,
                                        ::std::byte const* code_end,
                                        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> fs_para{};
        if constexpr((::std::same_as<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1, Fs> || ...))
        {
            auto& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
            para.enable_multi_value = true;
            para.enable_reference_types = true;
            para.enable_bulk_memory = true;
            para.enable_sign_extension = true;
            para.enable_nontrapping_float_to_int = true;
            para.enable_simd = true;
        }

        ::uwvm2::validation::standard::wasm1p1::validate_code(code_version, module_storage, function_index, code_begin, code_end, err, fs_para);
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif

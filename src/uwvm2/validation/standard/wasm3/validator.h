/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Core 3.0
 * @details     Derived from the complete Wasm 2 validator; Core 3 rules are implemented here.
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
# include <span>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/validation/standard/wasm3/relaxed_simd.h>
# include "retained_plan.h"
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
# include <uwvm2/validation/standard/wasm1/impl.h>
# include <uwvm2/validation/standard/wasm1p1/impl.h>
# include <uwvm2/validation/standard/wasm2/impl.h>
# include "memory_validation.h"
# include "scalar_memory_semantics.h"
# include "memory_page_semantics.h"
# include "table_validation.h"
# include "threads.h"
# include "atomic_semantics.h"
# include "bulk_memory_semantics.h"
# include "tail_call.h"
# include "reference_policy.h"
# include "local_declarations.h"
# include "declaration_policy.h"
# include "exception_policy.h"
# include "exception_validation.h"
# include "gc_immediate.h"
# include "gc_validation.h"
# include "reference_constant_event.h"
# include "reference_unary_event.h"
# include "i32_numeric_event.h"
# include "i64_numeric_event.h"
# include "integer_width_event.h"
# include "integer_compare_event.h"
# include "table_access_event.h"
# include "typed_select_event.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    using wasm1_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::mvp_op_basic;
    using wasm1p1_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_basic;
    using wasm1p1_numeric_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_numeric;
    using wasm1p1_simd_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_simd;
    using wasm_byte = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte;
    using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
    using wasm2_feature_kind = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind;

    inline constexpr wasm_byte opcode_byte(wasm1p1_code opcode) noexcept { return static_cast<wasm_byte>(opcode); }
    inline constexpr wasm_u32 opcode_u32(wasm1p1_code opcode) noexcept { return static_cast<wasm_u32>(opcode_byte(opcode)); }

    struct wasm3_code_version {};

    inline constexpr void require_gc_recursive_type_policy(
        bool enabled, bool requires_gc, ::std::byte const* code_begin,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        // Keep the exported compatibility entry point; the module and body
        // frontends share one exact metadata-only GC type requirement rule.
        require_gc_type_declaration_policy(enabled, requires_gc, code_begin, err);
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using operand_stack_value_type = ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct operand_stack_storage_t
    {
        operand_stack_value_type<Fs...> type{};
        bool is_unknown{};
        bool is_reference_bottom{};
        // Compile-time type witness for ref.func/ref.null <typeidx>. SIZE_MAX means
        // an erased supertype such as funcref; it must not satisfy call_ref x.
        ::std::size_t exact_function_type_index{(::std::numeric_limits<::std::size_t>::max)()};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type core_type{};
        bool has_core_type{};
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
            // [other owns allocation] -> [this owns allocation]; other becomes null.
            // [safe                 ] no byte pointer is advanced or dereferenced.
        }

        inline constexpr fast_io_native_typed_global_allocator_guard& operator= (fast_io_native_typed_global_allocator_guard&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            // The deallocator performs internal null pointer checks.
            allocator::deallocate(this->ptr);
            // [other owns allocation] -> [this owns allocation]; other becomes null.
            // [safe                 ] old ownership was released before this pointer update.
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
            // [other owns allocation] -> [this owns allocation]; other becomes null.
            // [safe                 ] no byte pointer is advanced or dereferenced.
        }

        inline constexpr fast_io_native_typed_thread_local_allocator_guard& operator= (fast_io_native_typed_thread_local_allocator_guard&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            // The deallocator performs internal null pointer checks.
            allocator::deallocate(this->ptr);
            // [other owns allocation] -> [this owns allocation]; other becomes null.
            // [safe                 ] old ownership was released before this pointer update.
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
        ::std::size_t signature_type_index{(::std::numeric_limits<::std::size_t>::max)()};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type singleton_result_core_type{};
        bool has_singleton_result_core_type{};
        ::std::size_t operand_stack_base{};
        block_type type{};
        bool polymorphic_base{};
        ::std::size_t local_init_checkpoint{};
    };

    namespace details
    {
        inline constexpr auto ref_func_opcode{static_cast<wasm1_code>(static_cast<wasm_byte>(wasm1p1_code::ref_func))};

        [[noreturn]] inline constexpr void fail_feature_required(::std::byte const* const op_begin,
                                                                 ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                 ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const value,
                                                                 ::uwvm2::parser::wasm::base::wasm1p1_feature_kind const feature,
                                                                 ::uwvm2::parser::wasm::base::wasm1p1_error_subject const subject) UWVM_THROWS
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_selectable.wasm1p1_feature_required.value = value;
            err.err_selectable.wasm1p1_feature_required.feature = feature;
            err.err_selectable.wasm1p1_feature_required.subject = subject;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        [[noreturn]] inline constexpr void fail_wasm2_feature_required(::std::byte const* const op_begin,
                                                                       ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                       ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const value,
                                                                       ::uwvm2::parser::wasm::base::wasm2_feature_kind const feature,
                                                                       ::uwvm2::parser::wasm::base::wasm2_error_subject const subject) UWVM_THROWS
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_selectable.wasm2_feature_required.value = value;
            err.err_selectable.wasm2_feature_required.feature = feature;
            err.err_selectable.wasm2_feature_required.subject = subject;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm2_feature_required;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        [[noreturn]] inline constexpr void fail_invalid_immediate(::std::byte const* const op_begin,
                                                                  ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                  ::uwvm2::utils::container::u8string_view op_name,
                                                                  ::fast_io::parse_code pc = ::fast_io::parse_code::invalid) UWVM_THROWS
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
            // A zero-length/empty body may have equal null endpoints. Reject the nonempty
            // immediate before subtracting those pointers; all callers request 1 or 16 bytes.
            if(code_curr == code_end) [[unlikely]] { fail_invalid_immediate(op_begin, err, op_name); }
            // [consumed opcode] payload ... code_end
            // [safe          ] unsafe (could be code_end)
            //                  ^^ code_curr: caller supplies endpoints of one function-body allocation.
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

        inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte read_u8(::std::byte const*& code_curr,
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
        inline constexpr void collect_const_expr_refs(ConstExpr const& expr,
                                                      ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>& refs)
        {
            for(auto const& op: expr.opcodes)
            {
                if(op.opcode == ref_func_opcode) { append_unique_ref(refs, op.storage.ref_func_idx); }
            }
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void
            collect_declared_refs(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
                                  ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>& refs)
        {
            auto const& exportsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::export_section_storage_t<Fs...>>(module_storage.sections)};
            for(auto const& exp: exportsec.exports)
            {
                if(exp.exports.type == ::uwvm2::parser::wasm::standard::wasm1::type::external_types::func)
                {
                    append_unique_ref(refs, exp.exports.storage.func_idx);
                }
            }

            auto const& globalsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections)};
            for(auto const& global: globalsec.local_globals) { collect_const_expr_refs(global.expr, refs); }

            auto const& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage.sections)};
            for(auto const& expr: tablesec.initializers) { collect_const_expr_refs(expr, refs); }

            auto const& elemsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>>(module_storage.sections)};
            for(auto const& elem: elemsec.elems)
            {
                auto const& segment{elem.storage.segment};
                for(auto const func_idx: segment.vec_funcidx) { append_unique_ref(refs, func_idx); }
                for(auto const& expr: segment.vec_expr) { collect_const_expr_refs(expr, refs); }
            }
        }
    }  // namespace details

    template <typename ValidatedOperationSink, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code_to_sink(wasm3_code_version,
                                        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
                                        ::std::size_t const function_index,
                                        ::std::byte const* code_begin,
                                        ::std::byte const* code_end,
                                        ::uwvm2::validation::error::code_validation_error_impl& err,
                                        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
                                        ValidatedOperationSink& validated_operations) UWVM_THROWS
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
            // [function body bytes, possibly empty] | code_end
            // [readable only if nonempty           ] | one-past is not dereferenced
            // ^^ code_begin -> err.err_curr: diagnostic copy; begin may equal end.
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
            // [function body bytes, possibly empty] | code_end
            // [readable only if nonempty           ] | one-past is not dereferenced
            // ^^ code_begin -> err.err_curr: diagnostic copy; begin may equal end.
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

        auto const& datacountsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>>(module_storage.sections)};

        auto const& wasm1p1_para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        require_gc_recursive_type_policy(!wasm1p1_para.disable_gc, typesec.requires_gc, code_begin, err);
        require_function_declaration_policy(curr_func_type, curr_code_locals,
            wasm1p1_para.disable_function_references && signatures_require_function_references(typesec), wasm1p1_para, code_begin, err,
            tablesec.requires_function_references, globalsec.requires_function_references, elemsec.requires_function_references,
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1p1::features::tag_section_storage_t<Fs...>>(module_storage.sections).present ||
            !::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(module_storage.sections).importdesc.index_unchecked(4uz).empty(),
            ::std::addressof(typesec.core3_context), typesec.types.size(),
            tablesec.requires_gc, globalsec.requires_gc, elemsec.requires_gc,
            {.types = typesec.requires_exceptions, .tables = tablesec.requires_exceptions,
             .globals = globalsec.requires_exceptions, .elements = elemsec.requires_exceptions,
             .locals = codesec.locals_require_exceptions},
            {.simd = typesec.requires_simd, .reference_types = typesec.requires_reference_types,
             .multi_value = typesec.requires_multi_value},
            {.tables_reference_types = tablesec.requires_reference_types, .globals_reference_types = globalsec.requires_reference_types,
             .elements_reference_types = elemsec.requires_reference_types, .globals_simd = globalsec.requires_simd,
             .table_reference_value = tablesec.reference_types_diagnostic_value,
             .global_reference_value = globalsec.reference_types_diagnostic_value,
             .element_reference_value = elemsec.reference_types_diagnostic_value},
            {.memory64 = memsec.requires_memory64, .table64 = tablesec.requires_table64,
             .shared = memsec.requires_threads, .multi_memory = all_memory_count > 1u},
            {.table_initializer = tablesec.requires_table_initializer,
             .extended_const = globalsec.constant_expressions_require_extended_const,
             .extended_const_value = globalsec.extended_const_diagnostic_value,
             .extended_const_subject = globalsec.extended_const_diagnostic_subject,
             .opcodes = globalsec.constant_expression_opcode_requirements});
        /// @brief Central policy query for every WebAssembly 2.0 instruction family.
        auto const feature_enabled{[&](wasm2_feature_kind const feature) constexpr noexcept
                                   { return ::uwvm2::parser::wasm::standard::wasm2::features::feature_enabled(wasm1p1_para, feature); }};

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
        // Lazily flattened once per validated function, only if it contains an aggregate instruction.
        // These pointers borrow parser-owned subtype objects and never outlive this synchronous validation.
        [[maybe_unused]] bool gc_environment_ready{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm3::type::sub_type const*> gc_definitions{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm3::type::core_value_type> gc_element_types{};
        auto const rich_signatures_available{!typesec.owned_signatures.empty() &&
            typesec.owned_signatures.size() == typesec.types.size()};
        auto const* curr_owned_signature{rich_signatures_available ?
            ::std::addressof(typesec.owned_signatures.index_unchecked(
                static_cast<::std::size_t>(funcsec.funcs.index_unchecked(local_func_idx)))) : nullptr};
        auto const type_index_from_pointer{[&](auto const* declaration) noexcept -> ::std::size_t
        {
            if(!rich_signatures_available) { return typesec.types.size(); }
            for(::std::size_t i{}; i != typesec.types.size(); ++i)
            {
                // [typesec.types.begin, typesec.types.end) is one parser-owned allocation.
                // [safe                                      ] i < size proves this read.
                //                     ^^ index_unchecked(i) borrows a live type; no pointer advances.
                if(::std::addressof(typesec.types.index_unchecked(i)) == declaration) { return i; }
            }
            return typesec.types.size();
        }};
        auto const local_core_type{[&](wasm_u32 index) noexcept
        {
            if(index < func_parameter_count_u32)
            {
                return curr_owned_signature != nullptr ? curr_owned_signature->parameters.index_unchecked(index) :
                    ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(func_parameter_begin[index]);
            }
            auto remaining{index - func_parameter_count_u32};
            for(auto const& run: curr_code_locals)
            {
                if(remaining < run.count)
                { return run.has_core_type ? run.core_type :
                    ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(run.type); }
                remaining -= run.count;
            }
            // Every caller has proved index < all_local_count. Reaching here means parser metadata differs.
            ::fast_io::fast_terminate();
        }};
        ::uwvm2::validation::standard::wasm3::core3_local_initialization initialized_locals{};
        auto const local_initially_initialized{[&](wasm_u32 index) noexcept
        { return index < func_parameter_count_u32 ||
            ::uwvm2::validation::standard::wasm3::core3_value_is_defaultable(local_core_type(index)); }};

        // block type
        using value_type_enum = curr_operand_stack_value_type;
        static constexpr value_type_enum i32_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32)};
        static constexpr value_type_enum i64_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64)};
        static constexpr value_type_enum f32_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32)};
        static constexpr value_type_enum f64_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64)};
        static constexpr value_type_enum v128_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)};
        static constexpr value_type_enum funcref_result_arr[1u]{
            static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref)};
        static constexpr value_type_enum externref_result_arr[1u]{
            static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::externref)};
        static constexpr value_type_enum exnref_result_arr[1u]{static_cast<value_type_enum>(0x69u)};
        static constexpr value_type_enum v128_i32_operands[2u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                                                               static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32)};
        static constexpr value_type_enum v128_i64_operands[2u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                                                               static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64)};
        static constexpr value_type_enum v128_f32_operands[2u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                                                               static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32)};
        static constexpr value_type_enum v128_f64_operands[2u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                                                               static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64)};
        static constexpr value_type_enum v128_v128_operands[2u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                                                                static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)};

        // function block (label/result type is the function result)
        control_flow_stack.push_back({.label = curr_func_type.result,
                                      .start = {},
                                      .result = curr_func_type.result,
                                      .signature_type_index = static_cast<::std::size_t>(funcsec.funcs.index_unchecked(local_func_idx)),
                                      .operand_stack_base = 0uz,
                                      .type = block_type::function,
                                      .polymorphic_base = false,
                                      .local_init_checkpoint = initialized_locals.checkpoint()});

        if constexpr(ValidatedOperationSink::retains_operations)
        {
            // [code_begin, code_end) is this parser-owned expression allocation.
            // [safe               ] immutable borrow only, no second byte traversal.
            // ^^ begin/end -> integer extent; parsed body membership proves subtraction.
            auto const result_count{curr_func_type.result.begin == curr_func_type.result.end ? 0uz :
                static_cast<::std::size_t>(curr_func_type.result.end - curr_func_type.result.begin)};
            validated_operations.begin_function(function_index, func_parameter_count_uz, all_local_count,
                result_count, static_cast<::std::size_t>(code_end - code_begin), local_core_type,
                curr_code_locals, [](auto const& run) noexcept
                { return run.has_core_type ? run.core_type : core3_legacy_carrier_type(run.type); },
                [&](::std::size_t index) noexcept
                {
                    // The builder loops index < result_count; both alternatives borrow
                    // this actual signature. Rich recursive IDs remain source-relative.
                    return curr_owned_signature != nullptr ? curr_owned_signature->results.index_unchecked(index) :
                        core3_legacy_carrier_type(curr_func_type.result.begin[index]);
                });
        }

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
            bool is_unknown{};
            bool is_reference_bottom{};
            ::std::size_t exact_function_type_index{(::std::numeric_limits<::std::size_t>::max)()};
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type core_type{};
            bool has_core_type{};
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
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                                                return {.from_stack = true, .type = operand.type, .is_unknown = operand.is_unknown,
                                                        .is_reference_bottom = operand.is_reference_bottom,
                                                        .exact_function_type_index = operand.exact_function_type_index,
                                                        .core_type = operand.core_type, .has_core_type = operand.has_core_type};
                                                }};

        auto const try_peek_concrete_operand{[&]() constexpr noexcept -> concrete_operand_t
                                                 {
                                                     if(concrete_operand_count() == 0uz) { return {}; }
                                                     auto const operand{operand_stack.back_unchecked()};
                                                     return {.from_stack = true, .type = operand.type, .is_unknown = operand.is_unknown,
                                                             .is_reference_bottom = operand.is_reference_bottom,
                                                             .exact_function_type_index = operand.exact_function_type_index,
                                                             .core_type = operand.core_type, .has_core_type = operand.has_core_type};
                                                 }};

        auto const pop_available_concrete_operands{[&](::std::size_t count) constexpr noexcept
                                                       {
                                                           while(count-- != 0uz && concrete_operand_count() != 0uz) { operand_stack.pop_back_unchecked(); }
                                                       }};

        auto const operand_type_matches{[](concrete_operand_t operand, curr_operand_stack_value_type expected_type) constexpr noexcept
                                        { return !operand.from_stack || ::uwvm2::validation::standard::wasm3::reference_carrier_matches(operand, expected_type); }};

        auto const stack_entry_type_matches{[](operand_stack_storage_t<Fs...> operand, curr_operand_stack_value_type expected_type) constexpr noexcept
                                           { return ::uwvm2::validation::standard::wasm3::reference_carrier_matches(operand, expected_type); }};

        struct block_core_type_t
        {
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type type{};
            bool has_type{};
        };
        auto const block_core_type_at{[&](block_result_type<Fs...> types, ::std::size_t signature_type_index,
                                        bool result, bool has_singleton_result_core_type,
                                        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type singleton_result_core_type,
                                        ::std::size_t index) constexpr noexcept -> block_core_type_t
        {
            auto const count{types.begin == types.end ? 0uz : static_cast<::std::size_t>(types.end - types.begin)};
            if(index >= count) { ::fast_io::fast_terminate(); }
            if(rich_signatures_available && signature_type_index < typesec.owned_signatures.size())
            {
                auto const& signature{typesec.owned_signatures.index_unchecked(signature_type_index)};
                auto const& tuple{result ? signature.results : signature.parameters};
                if(tuple.size() != count) { ::fast_io::fast_terminate(); }
                // [tuple.begin, tuple.end) is parser-owned for the life of validation.
                // [safe                 ] index < count == tuple.size() proves this read.
                //         ^^ index_unchecked(index) borrows one type; no cursor moves.
                return {tuple.index_unchecked(index), true};
            }
            if(result && has_singleton_result_core_type && count == 1uz)
            { return {singleton_result_core_type, true}; }
            return {};
        }};
        auto const block_value_matches{[&](operand_stack_storage_t<Fs...> actual, block_result_type<Fs...> types,
                                         ::std::size_t signature_type_index, bool result,
                                         bool has_singleton_result_core_type,
                                         ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type singleton_result_core_type,
                                         ::std::size_t index) noexcept
        {
            auto const expected{block_core_type_at(types, signature_type_index, result,
                has_singleton_result_core_type, singleton_result_core_type, index)};
            if(expected.has_type && !actual.is_unknown)
            {
                return ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                    ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual),
                    expected.type, typesec);
            }
            // [types.begin, types.end) is the retained flat signature range.
            // [safe                 ] block_core_type_at proved index < its count.
            //         ^^ begin[index] reads a carrier; no pointer advances.
            return stack_entry_type_matches(actual, types.begin[index]);
        }};

        auto const is_reference_value_type{[](curr_operand_stack_value_type type) constexpr noexcept -> bool
                                           {
                                               auto const vt{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>(type)};
                                               using value_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type;
                                               return vt == value_type::funcref || vt == value_type::externref ||
                                                   static_cast<unsigned>(type) == 0x69u;
                                           }};

        auto const is_untyped_select_value_type{[&](curr_operand_stack_value_type type) constexpr noexcept -> bool
                                                {
                                                    if(type == curr_operand_stack_value_type::i32 || type == curr_operand_stack_value_type::i64 ||
                                                       type == curr_operand_stack_value_type::f32 || type == curr_operand_stack_value_type::f64)
                                                    {
                                                        return true;
                                                    }

                                                    auto const vt{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>(type)};
                                                    return vt == ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128 && feature_enabled(wasm2_feature_kind::simd);
                                                }};

        auto const push_value_types{[&](block_result_type<Fs...> types,
                                       ::std::size_t signature_type_index = (::std::numeric_limits<::std::size_t>::max)(),
                                       bool result = false,
                                       bool has_singleton_result_core_type = false,
                                       ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type singleton_result_core_type = {}) constexpr
                                        {
                                            auto const count{types.begin == types.end ? 0uz : static_cast<::std::size_t>(types.end - types.begin)};
                                            for(::std::size_t i{}; i != count; ++i)
                                            {
                                                auto const rich{block_core_type_at(types, signature_type_index, result,
                                                    has_singleton_result_core_type, singleton_result_core_type, i)};
                                                // [types.begin, types.end) is the parser-owned flat signature.
                                                // [safe                 ] i < count proves begin[i] readable.
                                                //         ^^ begin[i] is borrowed; the loop changes only an index.
                                                operand_stack.push_back({.type = types.begin[i], .core_type = rich.type,
                                                    .has_core_type = rich.has_type});
                                            }
                                        }};

        auto const push_unknown_operand{[&]() constexpr { operand_stack.push_back({.is_unknown = true}); }};

        auto const pop_expected_operands{
            [&](::std::byte const* op_begin, ::uwvm2::utils::container::u8string_view op_name, block_result_type<Fs...> expected,
                ::std::size_t signature_type_index = (::std::numeric_limits<::std::size_t>::max)(),
                bool has_singleton_result_core_type = false,
                ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type singleton_result_core_type = {}) constexpr UWVM_THROWS
            {
                auto const expected_count{expected.begin == expected.end ? 0uz : static_cast<::std::size_t>(expected.end - expected.begin)};
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
                        auto const actual_operand{operand_stack.index_unchecked(stack_size - 1uz - i)};
                        if(!block_value_matches(actual_operand, expected, signature_type_index, false,
                            has_singleton_result_core_type, singleton_result_core_type, expected_count - 1uz - i)) [[unlikely]]
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_selectable.br_value_type_mismatch.op_code_name = op_name;
                            err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                            err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
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
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(type);
                    err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                if(!::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(vt, fs_para)) [[unlikely]]
                {
                    auto const feature{vt == ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128
                                           ? ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd
                                           : ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types};
                    details::fail_feature_required(op_begin, err, static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(type), feature, subject);
                }
            }};

        struct block_signature_t
        {
            block_result_type<Fs...> start{};
            block_result_type<Fs...> result{};
            ::std::size_t signature_type_index{(::std::numeric_limits<::std::size_t>::max)()};
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type singleton_result_core_type{};
            bool has_singleton_result_core_type{};
        };

        auto const parse_block_type{
            [&](::std::byte const* op_begin, [[maybe_unused]] ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS -> block_signature_t
            {
                if(code_curr == code_end) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_code = code_validation_error_code::missing_block_type;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
                }

                // op_name blocktype ...
                // [safe ] unsafe (could be the section_end)
                //         ^^ code_curr

                // [control opcode][valtype or s33 index ... code_end)
                // [safe          ] code_curr != code_end above proves the prefix readable.
                if(::uwvm2::validation::standard::wasm3::is_core3_extended_block_reference_prefix(
                    ::std::to_integer<unsigned>(*code_curr)))
                {
                    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type exact_type{};
                    auto const carrier{::uwvm2::validation::standard::wasm3::read_core3_value_carrier(
                        code_curr, code_end, !wasm1p1_para.disable_function_references, op_begin, err,
                        typesec.types.size(), ::std::addressof(exact_type), typesec.core3_context,
                        !wasm1p1_para.disable_gc, !wasm1p1_para.disable_exceptions)};
                    // [control opcode][checked valtype] next ... code_end
                    // [safe                          ] unsafe (could be code_end)
                    //                                  ^^ code_curr: bounded decoder committed the complete value,
                    //                                     after proving every Core 3 reference valtype byte readable.
                    auto const value{static_cast<curr_operand_stack_value_type>(carrier)};
                    // Core 3 scanner already checked GC/exception/function-reference
                    // gates against the exact heap. Rechecking the projected 0x70
                    // carrier would misclassify anyref/i31ref as funcref.
                    // Both arrays own one live carrier for this entire validation. Their +1 endpoints are one-past.
                    if(value == curr_operand_stack_value_type::funcref)
                    { return {.result = {funcref_result_arr, funcref_result_arr + 1u},
                        .singleton_result_core_type = exact_type, .has_singleton_result_core_type = true}; }
                    if(carrier == 0x69u)
                    { return {.result = {exnref_result_arr, exnref_result_arr + 1u},
                        .singleton_result_core_type = exact_type, .has_singleton_result_core_type = true}; }
                    return {.result = {externref_result_arr, externref_result_arr + 1u},
                        .singleton_result_core_type = exact_type, .has_singleton_result_core_type = true};
                }

                auto const blocktype_begin{code_curr};
                auto const blocktype{
                    details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>(code_curr, code_end, op_begin, err, u8"blocktype")};
                auto const blocktype_encoded_size{static_cast<::std::size_t>(code_curr - blocktype_begin)};

                // op_name blocktype ...
                // [      safe     ] unsafe (could be the section_end)
                //                   ^^ code_curr

                // op_name blocktype ...
                // [safe ] unsafe (could be the section_end)
                //         ^^ blocktype_begin

                // read_leb128 moved code_curr only after proving the whole blocktype immediate safe. A wasm1.1 blocktype is
                // encoded as s33, so the binary encoding may occupy at most 5 bytes.
                if(blocktype_encoded_size > 5uz || (blocktype < 0 && blocktype_encoded_size != 1uz)) [[unlikely]]
                {
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte first_blocktype_byte{};
                    ::std::memcpy(::std::addressof(first_blocktype_byte), blocktype_begin, sizeof(first_blocktype_byte));
#if CHAR_BIT > 8
                    first_blocktype_byte =
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(static_cast<::std::uint_least8_t>(first_blocktype_byte) & 0xFFu);
#endif
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.u8 = first_blocktype_byte;
                    err.err_code = code_validation_error_code::illegal_block_type;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                switch(blocktype)
                {
                    case -64:
                    {
                        return {};
                    }
                    case -1:
                    {
                        return {
                            .result = {i32_result_arr, i32_result_arr + 1u}
                        };
                    }
                    case -2:
                    {
                        return {
                            .result = {i64_result_arr, i64_result_arr + 1u}
                        };
                    }
                    case -3:
                    {
                        return {
                            .result = {f32_result_arr, f32_result_arr + 1u}
                        };
                    }
                    case -4:
                    {
                        return {
                            .result = {f64_result_arr, f64_result_arr + 1u}
                        };
                    }
                    case -5:
                    {
                        ensure_wasm1p1_value_type_enabled(
                            op_begin,
                            static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                        return {
                            .result = {v128_result_arr, v128_result_arr + 1u}
                        };
                    }
                    case -16:
                    {
                        ensure_wasm1p1_value_type_enabled(
                            op_begin,
                            static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref),
                            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                        // A legacy reference carrier also carries projected GC values. Preserve its exact heap.
                        // [singleton carrier] unsafe (one-past)
                        // ^^ result.begin     ^^ result.end: +1 stays within the live static singleton extent.
                        return {.result = {funcref_result_arr, funcref_result_arr + 1u},
                                .singleton_result_core_type = {
                                    .kind = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::reference,
                                    .heap = {-16}, .nullable = true, .source_prefix = 0x70u},
                                .has_singleton_result_core_type = true};
                    }
                    case -17:
                    {
                        ensure_wasm1p1_value_type_enabled(
                            op_begin,
                            static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::externref),
                            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                        // A legacy reference carrier also carries projected GC values. Preserve its exact heap.
                        // [singleton carrier] unsafe (one-past)
                        // ^^ result.begin     ^^ result.end: +1 stays within the live static singleton extent.
                        return {.result = {externref_result_arr, externref_result_arr + 1u},
                                .singleton_result_core_type = {
                                    .kind = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::reference,
                                    .heap = {-17}, .nullable = true, .source_prefix = 0x6fu},
                                .has_singleton_result_core_type = true};
                    }
                    default:
                    {
                        break;
                    }
                }

                if(blocktype >= 0)
                {
                    if(!feature_enabled(wasm2_feature_kind::multi_value)) [[unlikely]]
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_type_index.type_index =
                            blocktype > 0 ? static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(blocktype) : 0u;
                        err.err_selectable.illegal_type_index.all_type_count =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_type_count_uz);
                        err.err_code = code_validation_error_code::illegal_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    require_core3_function_type_index_policy(::std::addressof(typesec.core3_context),
                        static_cast<::std::size_t>(blocktype), op_begin, u8"blocktype", err);
                    auto const& block_func_type{typesec.types.index_unchecked(static_cast<::std::size_t>(blocktype))};
                    return {.start = block_func_type.parameter, .result = block_func_type.result,
                        .signature_type_index = static_cast<::std::size_t>(blocktype)};
                }

                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte first_blocktype_byte{};
                ::std::memcpy(::std::addressof(first_blocktype_byte), blocktype_begin, sizeof(first_blocktype_byte));
#if CHAR_BIT > 8
                first_blocktype_byte =
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(static_cast<::std::uint_least8_t>(first_blocktype_byte) & 0xFFu);
#endif
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.u8 = first_blocktype_byte;
                err.err_code = code_validation_error_code::illegal_block_type;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }};

        // Tag indices are imported-first, exactly like functions. Keep signature lookup separate from
        // runtime tag identity: validation compares payload types, never module-local numeric identities.
        struct exception_tag_signature_t
        {
            block_result_type<Fs...> parameters{};
            ::std::size_t type_index{(::std::numeric_limits<::std::size_t>::max)()};
        };
        auto const exception_tag_parameters{[&](wasm_u32 index, ::std::byte const* op_begin) constexpr UWVM_THROWS -> exception_tag_signature_t
        {
            auto const& tag_imports{importsec.importdesc.index_unchecked(4uz)};
            auto const& tags{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1p1::features::tag_section_storage_t<Fs...>>(module_storage.sections).type_indices};
            // Compare before subtracting the import prefix; both vectors remain owned by module_storage.
            bool const imported{index < tag_imports.size()};
            if(!imported && static_cast<::std::size_t>(index) - tag_imports.size() >= tags.size()) [[unlikely]]
            {
                // [throw/try_table] ... code_end; dispatch proved op_begin readable, diagnostic only.
                // [safe          ]
                // ^^ err_curr
                err.err_curr = op_begin; err.err_selectable.u8 = ::std::to_integer<unsigned>(*op_begin);
                err.err_code = code_validation_error_code::illegal_opbase;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            auto const type_index{imported ? tag_imports.index_unchecked(index)->imports.storage.tag_type_index :
                tags.index_unchecked(static_cast<::std::size_t>(index) - tag_imports.size())};
            if(type_index >= typesec.types.size()) [[unlikely]]
            {
                // [throw/try_table] ... code_end
                // [safe          ] err_curr borrows the checked opcode; no cursor movement.
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin; err.err_code = code_validation_error_code::illegal_type_index;
                err.err_selectable.illegal_type_index = {type_index, static_cast<wasm_u32>(typesec.types.size())};
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            // The parser checked a function signature with empty results for every local/imported tag.
            return {typesec.types.index_unchecked(type_index).parameter, static_cast<::std::size_t>(type_index)};
        }};
        struct exception_core_matching
        {
            ::std::remove_reference_t<decltype(typesec)> const* section{};
            inline bool matches(::uwvm2::parser::wasm::standard::wasm3::type::core_value_type source,
                ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type target) const noexcept
            {
                return ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(source, target, *section);
            }
        };

        auto const enter_control_frame{[&](::std::byte const* op_begin,
                                           ::uwvm2::utils::container::u8string_view op_name,
                                           block_type type,
                                           block_signature_t signature) constexpr UWVM_THROWS
                                       {
                                           pop_expected_operands(op_begin, op_name, signature.start,
                                               signature.signature_type_index);

                                           auto const base{operand_stack.size()};
                                           auto const label{type == block_type::loop ? signature.start : signature.result};
                                           control_flow_stack.push_back({.label = label,
                                                                         .start = signature.start,
                                                                         .result = signature.result,
                                                                         .signature_type_index = signature.signature_type_index,
                                                                         .singleton_result_core_type = signature.singleton_result_core_type,
                                                                         .has_singleton_result_core_type = signature.has_singleton_result_core_type,
                                                                         .operand_stack_base = base,
                                                                         .type = type,
                                                                         .polymorphic_base = is_polymorphic,
                                                                         .local_init_checkpoint = initialized_locals.checkpoint()});
                                           push_value_types(signature.start, signature.signature_type_index);

                                           // Stack-polymorphism is scoped to the current control frame only.
                                           is_polymorphic = false;
                                       }};

        // One first-read i32 numeric transition shared by every Core 3 facade.
        // Dispatch bounded/read the sole opcode byte; this adapter never rereads it.
        auto const validate_i32_numeric{
            [&]<unsigned Opcode>(::uwvm2::utils::container::u8string_view op_name)
                constexpr UWVM_THROWS -> ::uwvm2::validation::standard::wasm3::validated_i32_numeric_event
            {
                namespace v = ::uwvm2::validation::standard::wasm3;
                // [code_begin ... checked opcode ...] | code_end
                // [safe same expression allocation ] | unsafe (one-past)
                //                 ^^ op_begin copies the dispatcher-bounded code_curr.
                auto const op_begin{code_curr};
                auto const source_offset{static_cast<::std::size_t>(op_begin - code_begin)};
                // The dispatch proof code_curr<code_end permits this ONE byte advance.
                ++code_curr;
                // [decoded checked opcode] remaining ... | code_end
                // [safe                  ] unsafe (could be code_end)
                //                          ^^ code_curr; no immediate exists/read follows.
                decltype(try_pop_concrete_operand()) actual{};
                auto const consume{[&]() constexpr noexcept -> v::core3_operand
                {
                    // The common kernel proves one concrete value above the current frame
                    // BEFORE this owned copy/pop; a polymorphic synthetic pop never calls it.
                    actual = try_pop_concrete_operand();
                    return {v::core3_operand_effective_type(actual), actual.is_unknown};
                }};
                auto const push{[&](auto) constexpr UWVM_THROWS { operand_stack.push_back({curr_operand_stack_value_type::i32}); }};
                v::validated_i32_numeric_event event{};
                auto const result{v::transition_i32_numeric_event<Opcode>(event, is_polymorphic,
                    concrete_operand_count, consume, push, source_offset, control_flow_stack.size())};
                if(result.error == v::typed_stack_error::stack_underflow) [[unlikely]]
                { report_operand_stack_underflow(op_begin, op_name, Opcode <= 0x69u ? 1uz : 2uz); }
                if(result.error != v::typed_stack_error::ok) [[unlikely]]
                {
                    // [checked opcode] remaining ... | code_end
                    // [safe] ^^ err_curr copies op_begin; no pointer advance/dereference.
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i32);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(actual.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return event;
            }};

        auto const validate_i64_numeric{
            [&]<unsigned Opcode>(::uwvm2::utils::container::u8string_view op_name)
                constexpr UWVM_THROWS -> ::uwvm2::validation::standard::wasm3::validated_i64_numeric_event
            {
                namespace v = ::uwvm2::validation::standard::wasm3;
                // [code_begin ... checked opcode ...] | code_end
                // [safe same expression allocation ] | unsafe (one-past)
                //                 ^^ op_begin copies the dispatcher-bounded code_curr.
                auto const op_begin{code_curr};
                auto const source_offset{static_cast<::std::size_t>(op_begin - code_begin)};
                // The dispatch proof code_curr<code_end permits this ONE byte advance.
                ++code_curr;
                // [decoded checked opcode] remaining ... | code_end
                // [safe                  ] unsafe (could be code_end)
                //                          ^^ code_curr; no immediate exists/read follows.
                decltype(try_pop_concrete_operand()) actual{};
                auto const consume{[&]() constexpr noexcept -> v::core3_operand
                {
                    // The common kernel proves one concrete value above the current frame
                    // BEFORE this owned copy/pop; a polymorphic synthetic pop never calls it.
                    actual = try_pop_concrete_operand();
                    return {v::core3_operand_effective_type(actual), actual.is_unknown};
                }};
                auto const push{[&](auto) constexpr UWVM_THROWS { operand_stack.push_back({curr_operand_stack_value_type::i64}); }};
                v::validated_i64_numeric_event event{};
                auto const result{v::transition_i64_numeric_event<Opcode>(event, is_polymorphic,
                    concrete_operand_count, consume, push, source_offset, control_flow_stack.size())};
                if(result.error == v::typed_stack_error::stack_underflow) [[unlikely]]
                { report_operand_stack_underflow(op_begin, op_name, Opcode <= 0x7bu ? 1uz : 2uz); }
                if(result.error != v::typed_stack_error::ok) [[unlikely]]
                {
                    // [checked opcode] remaining ... | code_end
                    // [safe] ^^ err_curr copies op_begin; no pointer advance/dereference.
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i64);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(actual.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return event;
            }};

        auto const validate_integer_width{
            [&]<unsigned Opcode>(::uwvm2::utils::container::u8string_view op_name)
                constexpr UWVM_THROWS -> ::uwvm2::validation::standard::wasm3::validated_integer_width_event
            {
                namespace v = ::uwvm2::validation::standard::wasm3;
                constexpr auto expected_type{Opcode == 0xa7u ? curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
                constexpr auto result_type{Opcode == 0xa7u ? curr_operand_stack_value_type::i32 : curr_operand_stack_value_type::i64};
                // [code_begin ... checked opcode ...] | code_end
                // [safe same expression allocation ] | unsafe (one-past)
                //                 ^^ op_begin copies the dispatcher-bounded code_curr.
                auto const op_begin{code_curr};
                auto const source_offset{static_cast<::std::size_t>(op_begin - code_begin)};
                // The dispatch proof code_curr<code_end permits this ONE byte advance.
                ++code_curr;
                // [decoded checked opcode] remaining ... | code_end
                // [safe                  ] unsafe (could be code_end)
                //                          ^^ code_curr; no immediate exists/read follows.
                decltype(try_pop_concrete_operand()) actual{};
                auto const consume{[&]() constexpr noexcept -> v::core3_operand
                {
                    // The common kernel proves one concrete value above the current frame
                    // BEFORE this owned copy/pop; a polymorphic synthetic pop never calls it.
                    actual = try_pop_concrete_operand();
                    return {v::core3_operand_effective_type(actual), actual.is_unknown};
                }};
                auto const push{[&](auto) constexpr UWVM_THROWS { operand_stack.push_back({result_type}); }};
                v::validated_integer_width_event event{};
                auto const result{v::transition_integer_width_event<Opcode>(event, is_polymorphic,
                    concrete_operand_count, consume, push, source_offset, control_flow_stack.size())};
                if(result.error == v::typed_stack_error::stack_underflow) [[unlikely]]
                { report_operand_stack_underflow(op_begin, op_name, 1uz); }
                if(result.error != v::typed_stack_error::ok) [[unlikely]]
                {
                    // [checked opcode] remaining ... | code_end
                    // [safe] ^^ err_curr copies op_begin; no pointer advance/dereference.
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(actual.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return event;
            }};

        auto const validate_integer_compare{
            [&]<unsigned Opcode>(::uwvm2::utils::container::u8string_view op_name)
                constexpr UWVM_THROWS -> ::uwvm2::validation::standard::wasm3::validated_integer_compare_event
            {
                namespace v = ::uwvm2::validation::standard::wasm3;
                constexpr auto expected_type{Opcode <= 0x4fu ? curr_operand_stack_value_type::i32 : curr_operand_stack_value_type::i64};
                constexpr auto result_type{curr_operand_stack_value_type::i32};
                // [code_begin ... checked opcode ...] | code_end
                // [safe same expression allocation ] | unsafe (one-past)
                //                 ^^ op_begin copies the dispatcher-bounded code_curr.
                auto const op_begin{code_curr};
                auto const source_offset{static_cast<::std::size_t>(op_begin - code_begin)};
                // The dispatch proof code_curr<code_end permits this ONE byte advance.
                ++code_curr;
                // [decoded checked opcode] remaining ... | code_end
                // [safe                  ] unsafe (could be code_end)
                //                          ^^ code_curr; no immediate exists/read follows.
                decltype(try_pop_concrete_operand()) actual{};
                auto const consume{[&]() constexpr noexcept -> v::core3_operand
                {
                    // The common kernel proves one concrete value above the current frame
                    // BEFORE this owned copy/pop; a polymorphic synthetic pop never calls it.
                    actual = try_pop_concrete_operand();
                    return {v::core3_operand_effective_type(actual), actual.is_unknown};
                }};
                auto const push{[&](auto) constexpr UWVM_THROWS { operand_stack.push_back({result_type}); }};
                v::validated_integer_compare_event event{};
                auto const result{v::transition_integer_compare_event<Opcode>(event, is_polymorphic,
                    concrete_operand_count, consume, push, source_offset, control_flow_stack.size())};
                if(result.error == v::typed_stack_error::stack_underflow) [[unlikely]]
                { report_operand_stack_underflow(op_begin, op_name, Opcode == 0x45u || Opcode == 0x50u ? 1uz : 2uz); }
                if(result.error != v::typed_stack_error::ok) [[unlikely]]
                {
                    // [checked opcode] remaining ... | code_end
                    // [safe] ^^ err_curr copies op_begin; no pointer advance/dereference.
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(actual.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return event;
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
                                                  if(!operand_type_matches(operand, expected_operand_type)) [[unlikely]]
                                                  {
                                                      // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                                      // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                                      // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                                                   if(!operand_type_matches(rhs, expected_operand_type)) [[unlikely]]
                                                   {
                                                       // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                                       // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                                       // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                                       err.err_curr = op_begin;
                                                       err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                                                   err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(expected_operand_type);
                                                   err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(rhs.type);
                                                   err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                                   ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                               }

                                               // lhs
                                               auto const lhs{try_pop_concrete_operand()};
                                                   if(!operand_type_matches(lhs, expected_operand_type)) [[unlikely]]
                                                   {
                                                       // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                                       // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                                       // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                                       err.err_curr = op_begin;
                                                       err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                                                   err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(expected_operand_type);
                                                   err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(lhs.type);
                                                   err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                                   ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                               }

                                               operand_stack.push_back({result_type});
                                           }};

        auto const memory_address_type_at{[&](wasm_u32 index) constexpr noexcept
        {
            // [imported memories][local memories]
            // [safe                              ] index was checked by the caller.
            // Imported descriptors borrow immutable parser-owned records.
            auto const& memory{index < imported_memory_count ?
                imported_memories.index_unchecked(index)->imports.storage.memory :
                memsec.memories.index_unchecked(index - imported_memory_count)};
            if constexpr(requires { memory.address64; })
            { return memory.address64 ? storage_address_type::i64 : storage_address_type::i32; }
            else { return storage_address_type::i32; }
        }};
        // The same constant declaration gate above already admitted every memory64 declaration.
        // Core 3 binary memarg offsets are u64 for memory32 as well as memory64.
        // The memory64 switch gates declarations; read_memory_argument64 checks
        // the selected memory32 offset is < 2^32 after the full u64 decode.
        // https://webassembly.github.io/spec/core/binary/instructions.html#binary-memarg
        auto const table_address_type_at{[&](wasm_u32 index) constexpr noexcept
        {
            // [imported tables][local tables]
            // [safe                          ] caller proved index < all_table_count.
            auto const& table{index < imported_table_count ?
                imported_tables.index_unchecked(index)->imports.storage.table :
                tablesec.tables.index_unchecked(index - imported_table_count)};
            if constexpr(requires { table.address64; })
            { return table.address64 ? storage_address_type::i64 : storage_address_type::i32; }
            else { return storage_address_type::i32; }
        }};
        // The same constant declaration gate above already admitted every table64 declaration.
        auto const table_operand_type{[&](wasm_u32 index) constexpr noexcept
        {
            return table_address_type_at(index) == storage_address_type::i64 ?
                curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32;
        }};
        auto const memory_operand_type{[&](wasm_u32 index) constexpr noexcept
        {
            return memory_address_type_at(index) == storage_address_type::i64 ?
                curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32;
        }};
        auto const validate_storage_operand{[&](::std::byte const* op_begin,
            ::uwvm2::utils::container::u8string_view name, curr_operand_stack_value_type expected) constexpr UWVM_THROWS
        {
            if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]]
            { report_operand_stack_underflow(op_begin, name, 1uz); }
            auto const operand{try_pop_concrete_operand()};
            if(!operand_type_matches(operand, expected)) [[unlikely]]
            {
                // [opcode] immediate ... (code_end)
                // [safe  ] unsafe; borrow the dispatch-checked opcode for diagnostics.
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                err.err_selectable.numeric_operand_type_mismatch = {
                    .op_code_name = name, .expected_type = to_wasm1_value_type(expected),
                    .actual_type = to_wasm1_value_type(operand.type)};
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }};

        auto const validate_checked_bulk_memory{[&](::std::byte const* op_begin, ::uwvm2::utils::container::u8string_view name,
            ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const& decoded) constexpr UWVM_THROWS
        {
            namespace bulk = ::uwvm2::validation::standard::wasm3;
            decltype(try_pop_concrete_operand()) actual_bulk_operand{};
            auto const consume_bulk_operand{[&]() constexpr noexcept
            {
                // The common sequence preflight and single-pop proof bound this owned
                // current-frame operand removal. No source or guest pointer advances.
                actual_bulk_operand = try_pop_concrete_operand();
                return bulk::core3_operand{bulk::core3_operand_effective_type(actual_bulk_operand),
                    !actual_bulk_operand.from_stack || actual_bulk_operand.is_unknown};
            }};
            auto const failure{bulk::validate_bulk_memory_operand_sequence(
                decoded, is_polymorphic, concrete_operand_count, consume_bulk_operand)};
            if(failure.error == bulk::typed_stack_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, name, bulk::bulk_memory_operand_count(decoded)); }
            if(failure.error != bulk::typed_stack_error::ok) [[unlikely]]
            {
                // [dispatch-checked FC][checked data/memory immediates] | code_end
                // [safe opcode        ][safe                         ] | one-past not read
                // ^^ op_begin -> err.err_curr: copy the original diagnostic span only.
                err.err_curr = op_begin;
                auto const core{bulk::bulk_memory_expected_operand_type(decoded, failure.failed_pop_index)};
                auto const expected{core.kind == ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::i64 ?
                    curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
                err.err_selectable.numeric_operand_type_mismatch = {.op_code_name = name,
                    .expected_type = to_wasm1_value_type(expected), .actual_type = to_wasm1_value_type(actual_bulk_operand.type)};
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }};

        auto const validate_checked_scalar_memory{[&](::std::byte const* op_begin,
            ::uwvm2::utils::container::u8string_view op_name,
            ::uwvm2::validation::standard::wasm3::typed_memory_argument const& memarg,
            bool store, curr_operand_stack_value_type scalar_value_type) constexpr UWVM_THROWS
        {
            namespace scalar = ::uwvm2::validation::standard::wasm3;
            decltype(try_pop_concrete_operand()) actual_scalar_operand{};
            auto const consume_scalar_operand{[&]() constexpr noexcept
            {
                // The shared entire-arity preflight and bounded concrete-pop proof
                // authorize one current-frame owned operand removal, not a guest access.
                actual_scalar_operand = try_pop_concrete_operand();
                return scalar::core3_operand{scalar::core3_operand_effective_type(actual_scalar_operand),
                    !actual_scalar_operand.from_stack || actual_scalar_operand.is_unknown};
            }};
            auto const failure{scalar::validate_scalar_memory_operand_sequence(memarg, store,
                scalar::core3_legacy_carrier_type(scalar_value_type), is_polymorphic,
                concrete_operand_count, consume_scalar_operand)};
            if(failure.error == scalar::typed_stack_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, op_name, store ? 2uz : 1uz); }
            if(failure.error != scalar::typed_stack_error::ok) [[unlikely]]
            {
                // [dispatch-checked opcode][bounded checked memarg] | code_end
                // [safe                  ][safe                  ] | one-past not read
                // ^^ op_begin -> err.err_curr: copy the original borrowed diagnostic span.
                err.err_curr = op_begin;
                if(store && failure.failed_pop_index == 0u)
                {
                    err.err_selectable.store_value_type_mismatch = {.op_code_name = op_name,
                        .expected_type = to_wasm1_value_type(scalar_value_type), .actual_type = to_wasm1_value_type(actual_scalar_operand.type)};
                    err.err_code = code_validation_error_code::store_value_type_mismatch;
                }
                else if(memarg.address_type == scalar::storage_address_type::i64)
                {
                    err.err_selectable.numeric_operand_type_mismatch = {.op_code_name = op_name,
                        .expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i64), .actual_type = to_wasm1_value_type(actual_scalar_operand.type)};
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                }
                else
                {
                    err.err_selectable.memarg_address_type_not_i32 = {.op_code_name = op_name,
                        .addr_type = to_wasm1_value_type(actual_scalar_operand.type)};
                    err.err_code = code_validation_error_code::memarg_address_type_not_i32;
                }
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }};

        auto const validate_checked_memory_page{[&](::std::byte const* op_begin,
            ::uwvm2::validation::standard::wasm3::storage_address_type address_type, bool grow) constexpr UWVM_THROWS
        {
            namespace page = ::uwvm2::validation::standard::wasm3;
            decltype(try_pop_concrete_operand()) actual_page_operand{};
            auto const consume_page_operand{[&]() constexpr noexcept
            {
                // The common sequence's reachable arity and concrete-pop proof bound
                // this current-frame owned top. Size never invokes this callback.
                actual_page_operand = try_pop_concrete_operand();
                return page::core3_operand{page::core3_operand_effective_type(actual_page_operand),
                    !actual_page_operand.from_stack || actual_page_operand.is_unknown};
            }};
            auto const failure{page::validate_memory_page_operand_sequence(
                address_type, grow, is_polymorphic, concrete_operand_count, consume_page_operand)};
            if(failure.error == page::typed_stack_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, u8"memory.grow", 1uz); }
            if(failure.error != page::typed_stack_error::ok) [[unlikely]]
            {
                // [dispatch-checked page opcode][bounded checked memidx] | code_end
                // [safe                        ][safe                 ] | one-past not read
                // ^^ op_begin -> err.err_curr: borrow only the original diagnostic span.
                err.err_curr = op_begin;
                if(address_type == page::storage_address_type::i64)
                {
                    err.err_selectable.numeric_operand_type_mismatch = {.op_code_name = u8"memory.grow",
                        .expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i64),
                        .actual_type = to_wasm1_value_type(actual_page_operand.type)};
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                }
                else
                {
                    err.err_selectable.memory_grow_delta_type_not_i32.delta_type = to_wasm1_value_type(actual_page_operand.type);
                    err.err_code = code_validation_error_code::memory_grow_delta_type_not_i32;
                }
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }};

        auto const validate_mem_load{[&](::uwvm2::utils::container::u8string_view op_name,
                                         ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const max_align,
                                         curr_operand_stack_value_type const result_type) constexpr UWVM_THROWS
                                     {
                                         // op_name memarg ...
                                         // [safe ] unsafe (could be code_end); dispatch checked the opcode byte.
                                         // ^^ code_curr: the complete opcode byte may be consumed.
                                         auto const op_begin{code_curr};
                                         ++code_curr;
                                         // op_name memarg ...
                                         // [safe ] unsafe (could be code_end)
                                         //         ^^ code_curr
                                         // The bounded scanner checks flags, optional memory index and offset before committing code_curr.
                                         auto const memarg{::uwvm2::validation::standard::wasm3::read_memory_argument64(
                                             code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, true, all_memory_count, memory_address_type_at, max_align, op_name, err)};
                                         // op_name [validated memarg] ...
                                         // [safe                   ] unsafe (could be code_end)
                                         //                           ^^ code_curr
                                         static_cast<void>(memarg);

                                         validate_checked_scalar_memory(op_begin, op_name, memarg, false, result_type);

                                         operand_stack.push_back({result_type});
                                     }};

        auto const validate_mem_store{[&](::uwvm2::utils::container::u8string_view op_name,
                                          ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const max_align,
                                          curr_operand_stack_value_type const expected_value_type) constexpr UWVM_THROWS
                                      {
                                          // op_name memarg ...
                                          // [safe ] unsafe (could be code_end); dispatch checked the opcode byte.
                                          // ^^ code_curr: the complete opcode byte may be consumed.
                                          auto const op_begin{code_curr};
                                          ++code_curr;
                                          // op_name memarg ...
                                          // [safe ] unsafe (could be code_end)
                                          //         ^^ code_curr
                                          // The bounded scanner checks flags, optional memory index and offset before committing code_curr.
                                          auto const memarg{::uwvm2::validation::standard::wasm3::read_memory_argument64(
                                              code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, true, all_memory_count, memory_address_type_at, max_align, op_name, err)};
                                          // op_name [validated memarg] ...
                                          // [safe                   ] unsafe (could be code_end)
                                          //                           ^^ code_curr
                                          static_cast<void>(memarg);

                                          validate_checked_scalar_memory(op_begin, op_name, memarg, true, expected_value_type);

                                      }};

        auto const check_data_index{[&](::std::byte const* op_begin, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 data_index) constexpr UWVM_THROWS
                                    {
                                        if(!datacountsec.present || data_index >= datacountsec.count) [[unlikely]]
                                        {
                                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.illegal_element_index.element_index = element_index;
                    err.err_selectable.illegal_element_index.all_element_count = all_element_count;
                    err.err_code = code_validation_error_code::illegal_element_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }};

        /// @brief Validate a table immediate and enforce the WebAssembly 2.0 multiple-tables policy at the code-validation extension point.
        auto const check_table_index{
            [&](::std::byte const* op_begin,
                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_index,
                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 opcode) constexpr UWVM_THROWS
            {
                if(!feature_enabled(wasm2_feature_kind::multiple_tables) && table_index != 0u) [[unlikely]]
                {
                    details::fail_wasm2_feature_required(op_begin,
                                                         err,
                                                         opcode,
                                                         ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables,
                                                         ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
                }

                if(table_index >= all_table_count) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.illegal_table_index.table_index = table_index;
                    err.err_selectable.illegal_table_index.all_table_count = all_table_count;
                    err.err_code = code_validation_error_code::illegal_table_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }};

        auto const check_memory_index{[&](::std::byte const* op_begin,
                                               ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memory_index,
                                               ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                                           {
                                               if(all_memory_count == 0u) [[unlikely]]
                                               {
                                                   // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                                   // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                                   // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                                   err.err_curr = op_begin;
                                                   err.err_selectable.no_memory.op_code_name = op_name;
                                                   err.err_selectable.no_memory.align = 0u;
                                                   err.err_selectable.no_memory.offset = 0u;
                                                   err.err_code = code_validation_error_code::no_memory;
                                                   ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                               }

                                               if(memory_index >= all_memory_count) [[unlikely]]
                                               {
                                                   // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                                   // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                                   // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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

        auto const get_table_core_type{[&](wasm_u32 table_index) noexcept
        {
            if(table_index < imported_table_count)
            {
                auto const declaration{imported_tables.index_unchecked(table_index)->imports.storage.table};
                return ::uwvm2::validation::standard::wasm3::core3_declaration_effective_type(
                    declaration, get_table_value_type(table_index));
            }
            auto const declaration{tablesec.tables.index_unchecked(table_index - imported_table_count)};
            return ::uwvm2::validation::standard::wasm3::core3_declaration_effective_type(
                declaration, get_table_value_type(table_index));
        }};

        auto const get_global_core_type{[&](wasm_u32 global_index) noexcept
        {
            if(global_index < imported_global_count)
            {
                auto const declaration{imported_globals.index_unchecked(global_index)->imports.storage.global};
                return ::uwvm2::validation::standard::wasm3::core3_declaration_effective_type(
                    declaration, declaration.type);
            }
            auto const declaration{globalsec.local_globals.index_unchecked(global_index - imported_global_count).global};
            return ::uwvm2::validation::standard::wasm3::core3_declaration_effective_type(
                declaration, declaration.type);
        }};

        auto const check_ref_func_index{
            [&](::std::byte const* op_begin, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_index) constexpr UWVM_THROWS
            {
                auto const all_function_size{import_func_count + local_func_count};
                if(static_cast<::std::size_t>(func_index) >= all_function_size) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.invalid_function_index.function_index = func_index;
                    err.err_selectable.invalid_function_index.all_function_size = all_function_size;
                    err.err_code = code_validation_error_code::invalid_function_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                if(::std::find(declared_refs.begin(), declared_refs.end(), func_index) == declared_refs.end()) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        auto const actual_operand{operand_stack.index_unchecked(operand_stack.size() - 1uz - i)};
                        if(!stack_entry_type_matches(actual_operand, curr_operand_stack_value_type::i32)) [[unlikely]]
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                            err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i32);
                            err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                            err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                }

                pop_available_concrete_operands(count);
            }};

        auto const validate_numeric_unary_stack_effect{[&](::std::byte const* op_begin,
                                                           ::uwvm2::utils::container::u8string_view op_name,
                                                           curr_operand_stack_value_type expected_operand_type,
                                                           curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
                                                       {
                                                           if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]]
                                                           {
                                                               report_operand_stack_underflow(op_begin, op_name, 1uz);
                                                           }

                                                           auto const operand{try_pop_concrete_operand()};
                                                               if(!operand_type_matches(operand, expected_operand_type)) [[unlikely]]
                                                               {
                                                                   // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                                                   // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                                                   // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                                                   err.err_curr = op_begin;
                                                                   err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                                                               err.err_selectable.numeric_operand_type_mismatch.expected_type =
                                                                   to_wasm1_value_type(expected_operand_type);
                                                               err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(operand.type);
                                                               err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                                               ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                                           }

                                                           operand_stack.push_back({result_type});
                                                       }};

        // The bounded tableidx decoder and table-index policy run BEFORE this
        // shared first typing. No source byte is reread or cursor modified here.
        auto const validate_table_access{
            [&]<unsigned Opcode>(::std::byte const* op_begin, auto table_index,
                ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                -> ::uwvm2::validation::standard::wasm3::validated_table_access_event
        {
            namespace v = ::uwvm2::validation::standard::wasm3;
            auto const table_type{get_table_value_type(table_index)};
            auto const element_type{get_table_core_type(table_index)};
            auto const address_type{table_operand_type(table_index)};
            decltype(try_pop_concrete_operand()) actual{};
            auto const consume{[&]() constexpr noexcept -> v::core3_operand
            {
                // Common full-arity/frame-base proof precedes this one real owned pop.
                actual = try_pop_concrete_operand();
                return {v::core3_operand_effective_type(actual), actual.is_unknown};
            }};
            auto const matches{[&](auto a, auto e) constexpr noexcept { return v::core3_value_type_matches_in_section(a, e, typesec); }};
            auto const push{[&](auto) constexpr UWVM_THROWS
            {
                operand_stack.push_back({table_type});
                operand_stack.back_unchecked().core_type = element_type;
                operand_stack.back_unchecked().has_core_type = true;
            }};
            v::validated_table_access_event event{};
            // [code_begin ... checked opcode ... complete u32 LEB] | code_curr <= code_end
            // [safe one parser-owned expression allocation] | one-past
            // Subtract only actual same-owner cursors; neither pointer advances or reads.
            auto const result{v::transition_table_access_event<Opcode>(event, is_polymorphic,
                concrete_operand_count, consume, matches, push, static_cast<::std::uint_least32_t>(table_index),
                address_type == curr_operand_stack_value_type::i64, element_type, static_cast<unsigned>(table_type),
                static_cast<::std::size_t>(op_begin - code_begin), static_cast<::std::size_t>(code_curr - op_begin),
                control_flow_stack.size())};
            if(result.error == v::typed_stack_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, op_name, Opcode == 0x25u ? 1uz : 2uz); }
            if(result.error != v::typed_stack_error::ok) [[unlikely]]
            {
                // [same checked opcode ... completed immediate] remaining ... | code_end
                // [safe same expression allocation] | one-past never dereferenced
                // ^^ op_begin -> err.err_curr: diagnostic copy only; no pointer advance.
                err.err_curr = op_begin;
                if constexpr(Opcode == 0x26u)
                {
                    if(result.failed_pop_index == 0u)
                    {
                        err.err_selectable.br_value_type_mismatch.op_code_name = op_name;
                        err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_type);
                        err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual.type);
                        err.err_code = code_validation_error_code::br_value_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                }
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(address_type);
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(actual.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            return event;
        }};

        // First complete typed-select valtype/count admission stays in the original
        // opcode handler. This shared transition never rereads bytes or advances cursors.
        auto const validate_typed_select{
            [&](::std::byte const* op_begin, curr_operand_stack_value_type result_type,
                ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type result_core_type) constexpr UWVM_THROWS
                -> ::uwvm2::validation::standard::wasm3::validated_typed_select_event
        {
            namespace v = ::uwvm2::validation::standard::wasm3;
            decltype(try_pop_concrete_operand()) actual{};
            auto const consume{[&]() constexpr noexcept -> v::core3_operand
            {
                // Common full three-operand/current-frame proof precedes this real pop.
                actual = try_pop_concrete_operand();
                return {v::core3_operand_effective_type(actual), actual.is_unknown};
            }};
            auto const matches{[&](auto a, auto e) constexpr noexcept { return v::core3_value_type_matches_in_section(a, e, typesec); }};
            auto const push{[&](auto) constexpr UWVM_THROWS
            {
                operand_stack.push_back({result_type});
                operand_stack.back_unchecked().core_type = result_core_type;
                operand_stack.back_unchecked().has_core_type = true;
            }};
            v::validated_typed_select_event event{};
            // [code_begin ... original opcode ... complete count and valtype] | code_curr <= code_end
            // [safe one actual expression owner] one-past: subtraction only, no advance/read.
            auto const result{v::transition_typed_select_event(event, is_polymorphic,
                concrete_operand_count, consume, matches, push, result_core_type, static_cast<unsigned>(result_type),
                static_cast<::std::size_t>(op_begin - code_begin), static_cast<::std::size_t>(code_curr - op_begin),
                control_flow_stack.size())};
            if(result.error == v::typed_stack_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, u8"select", 3uz); }
            if(result.error != v::typed_stack_error::ok) [[unlikely]]
            {
                // [checked opcode ... complete immediate] remaining ... | code_end
                // [safe same actual expression] one-past never read
                // ^^ op_begin -> err.err_curr: diagnostic copy only, not a cursor advance.
                err.err_curr = op_begin;
                if(result.failed_pop_index == 0u)
                {
                    err.err_selectable.select_cond_type_not_i32.cond_type = to_wasm1_value_type(actual.type);
                    err.err_code = code_validation_error_code::select_cond_type_not_i32;
                }
                else
                {
                    err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_value_type(result_type);
                    err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_value_type(actual.type);
                    err.err_code = code_validation_error_code::select_type_mismatch;
                }
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            return event;
        }};

        // [before_section ... ] | opbase opextent
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ code_curr

        // a WebAssembly function with type '() -> ()' (often written as returning “nil”) can have no meaningful code, but it still must have a valid
        // instruction sequence—at minimum an end.

        // No sink closures or per-instruction dispatch exist in the normal
        // validator instantiation: every retention site is an if-constexpr.
        for(;;)
        {
            if(code_curr == code_end) [[unlikely]]
            {
                // [... ] | (end)
                // [safe] | unsafe (could be the section_end)
                //          ^^ code_curr

                // Validation completes when the end is reached, so this condition can never be met. If it were met, it would indicate a missing end.
                // [decoded prefix] remaining bytes ... | code_end
                // [readable bytes]                    | one-past is not dereferenced
                // ^^ code_curr -> err.err_curr: diagnostic copy; it may equal code_end.
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
            if constexpr(ValidatedOperationSink::retains_operations)
            {
                // [code_begin ... code_curr opcode] ... | code_end
                // [safe: current opcode exists    ]     | one-past
                // ^^ code_curr -> integer offset only; dispatch proved code_curr != code_end.
                validated_operations.begin_instruction(static_cast<::std::uint_least8_t>(curr_opbase),
                    static_cast<::std::size_t>(code_curr - code_begin), operand_stack.size(),
                    control_flow_stack.size(), is_polymorphic,
                    operand_stack.empty() ? ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type{} :
                        operand_stack.back_unchecked().has_core_type ? operand_stack.back_unchecked().core_type :
                        core3_legacy_carrier_type(to_wasm1_value_type(operand_stack.back_unchecked().type)));
            }

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
                case static_cast<wasm_byte>(0x08): // Core 3 throw
                case static_cast<wasm_byte>(0x0a): // Core 3 throw_ref
                {
                    // [throw/throw_ref] tagidx? ... code_end
                    // [safe          ] unsafe (could be code_end)
                    // ^^ op_begin borrows the dispatch-checked code_curr.
                    auto const op_begin{code_curr};
                    require_exceptions_enabled(!wasm1p1_para.disable_exceptions, curr_opbase, op_begin, err);
                    ++code_curr;
                    // [throw/throw_ref] tagidx? ... code_end
                    // [safe          ] unsafe (could be code_end)
                    //                  ^^ code_curr: one checked opcode consumed; never dereferenced without a new bound.
                    if(curr_opbase == static_cast<wasm_byte>(0x08))
                    {
                        auto const index{details::read_leb128<wasm_u32>(code_curr, code_end, op_begin, err, u8"tagidx")};
                        // [throw checked tagidx] next ... code_end
                        // [safe               ] unsafe (could be code_end)
                        //                       ^^ code_curr: bounded LEB reader committed the complete immediate.
                        auto const tag{exception_tag_parameters(index, op_begin)};
                        pop_expected_operands(op_begin, u8"throw", tag.parameters, tag.type_index);
                    }
                    else
                    {
                        namespace v3 = ::uwvm2::validation::standard::wasm3;
                        decltype(try_pop_concrete_operand()) operand{};
                        auto const consume{[&]() constexpr noexcept
                        {
                            // The shared count check proves an actual top above this frame.
                            // Copy its rich type before the actual compiler stack entry retires.
                            operand = try_pop_concrete_operand();
                            return v3::core3_operand{v3::core3_operand_effective_type(operand), !operand.from_stack || operand.is_unknown};
                        }};
                        auto const matches{[&](auto actual, auto expected) constexpr noexcept
                        { return v3::core3_value_type_matches_in_section(actual, expected, typesec); }};
                        auto const failure{v3::pop_core3_expected_operand(is_polymorphic, concrete_operand_count, consume,
                            v3::exception_validation_details::exception_reference(true), matches)};
                        if(failure == v3::typed_stack_error::stack_underflow) [[unlikely]]
                        { report_operand_stack_underflow(op_begin, u8"throw_ref", 1uz); }
                        if(failure != v3::typed_stack_error::ok) [[unlikely]]
                        {
                            // [throw_ref] ... code_end; no input read or cursor movement while reporting the operand mismatch.
                            // [safe     ]
                            // ^^ err_curr
                            err.err_curr = op_begin; err.err_code = code_validation_error_code::br_value_type_mismatch;
                            err.err_selectable.br_value_type_mismatch = {.op_code_name = u8"throw_ref",
                                .expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(0x69u),
                                .actual_type = to_wasm1_value_type(operand.type)};
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    ::uwvm2::validation::standard::wasm3::make_core3_frame_unreachable(is_polymorphic,
                        [&]() constexpr noexcept
                        {
                            if(!control_flow_stack.empty())
                            {
                                // Actual frame base <= owned operand size; only this frame's live suffix retires.
                                auto const height{control_flow_stack.back_unchecked().operand_stack_base};
                                while(operand_stack.size() > height) { operand_stack.pop_back_unchecked(); }
                            }
                        });
                    break;
                }
                case static_cast<wasm_byte>(0x1f): // Core 3 try_table
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

                    if(curr_opbase == static_cast<wasm_byte>(0x1f))
                    { require_exceptions_enabled(!wasm1p1_para.disable_exceptions, 0x1fu, op_begin, err); }
                    auto const signature{parse_block_type(op_begin, u8"block")};
                    if(curr_opbase == static_cast<wasm_byte>(0x1f))
                    {
                        auto const catches{scan_exception_catches(code_curr, code_end)};
                        // [try_table blocktype checked catch vector] next ... code_end
                        // [safe                                   ] unsafe (could be code_end)
                        //                                           ^^ code_curr on success; unchanged on malformed vector.
                        if(catches.error != exception_immediate_error::ok) [[unlikely]]
                        {
                            // [try_table] ... code_end; borrow the dispatch-checked diagnostic address.
                            // [safe    ]
                            // ^^ err_curr
                            err.err_curr = op_begin; err.err_code = code_validation_error_code::illegal_opbase; err.err_selectable.u8 = 0x1fu;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                        for(auto const& clause: catches.clauses)
                        {
                            auto const tagged{clause.kind == exception_catch_kind::tagged || clause.kind == exception_catch_kind::tagged_ref};
                            auto const payload{tagged ? exception_tag_parameters(clause.tag_index, op_begin) : exception_tag_signature_t{}};
                            if(clause.label_index >= control_flow_stack.size()) [[unlikely]]
                            {
                                // [try_table] ... code_end; current control stack still contains only OUTER labels.
                                // [safe    ]
                                // ^^ err_curr
                                err.err_curr = op_begin; err.err_code = code_validation_error_code::illegal_label_index;
                                err.err_selectable.illegal_label_index = {clause.label_index, static_cast<wasm_u32>(control_flow_stack.size())};
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                            auto const& target_frame{control_flow_stack.index_unchecked(control_flow_stack.size() - 1uz - clause.label_index)};
                            auto const target{target_frame.label};
                            auto const payload_count{payload.parameters.begin == payload.parameters.end ? 0uz :
                                static_cast<::std::size_t>(payload.parameters.end - payload.parameters.begin)};
                            auto const target_count{target.begin == target.end ? 0uz : static_cast<::std::size_t>(target.end - target.begin)};
                            auto const result{validate_exception_catch_signature(clause.kind, payload_count,
                                [&](::std::size_t i) constexpr noexcept
                                {
                                    if(rich_signatures_available && payload.type_index < typesec.owned_signatures.size())
                                    {
                                        auto const& values{typesec.owned_signatures.index_unchecked(payload.type_index).parameters};
                                        // [values.begin, values.end) is the parser-owned tag tuple; i < payload_count == size.
                                        // [safe                    ] unsafe (one-past)
                                        //         ^^ index_unchecked(i) borrows the proved live payload type.
                                        return values.index_unchecked(i);
                                    }
                                    // [payload.parameters.begin, payload.parameters.end) is the checked tag tuple.
                                    // [safe                                         ] unsafe (one-past)
                                    //                      ^^ begin[i] is readable because i < payload_count.
                                    return ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(payload.parameters.begin[i]);
                                }, target_count,
                                [&](::std::size_t i) constexpr noexcept
                                {
                                    auto const rich{block_core_type_at(target, target_frame.signature_type_index,
                                        target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
                                        target_frame.singleton_result_core_type, i)};
                                    if(rich.has_type) { return rich.type; }
                                    // [target.begin, target.end) is the checked outer-label tuple.
                                    // [safe                      ] unsafe (one-past)
                                    //         ^^ begin[i] is readable because i < target_count.
                                    return ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(target.begin[i]);
                                }, exception_core_matching{::std::addressof(typesec)})};
                            if(result != core3_exception_error::ok) [[unlikely]]
                            {
                                // [try_table] ... code_end; callbacks above are bounded by checked tuple arities.
                                // [safe    ]
                                // ^^ err_curr
                                err.err_curr = op_begin; err.err_code = code_validation_error_code::end_result_mismatch;
                                bool const with_reference{clause.kind == exception_catch_kind::tagged_ref || clause.kind == exception_catch_kind::all_ref};
                                // Tag payload arity is parser-bounded far below SIZE_MAX; adding its one exn value is safe.
                                err.err_selectable.end_result_mismatch = {.block_kind = u8"try_table catch", .expected_count = target_count,
                                    .actual_count = payload_count + static_cast<::std::size_t>(with_reference),
                                    .expected_type = target_count == 0uz ? to_wasm1_value_type(curr_operand_stack_value_type{}) : to_wasm1_value_type(target.begin[0]),
                                    .actual_type = payload_count == 0uz ? to_wasm1_value_type(curr_operand_stack_value_type{}) : to_wasm1_value_type(payload.parameters.begin[0])};
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                    }
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
                    // Empty parser-owned signature ranges may be null/null; equality is checked before subtraction.
                    auto const if_param_count{signature.start.begin == signature.start.end ? 0uz :
                        static_cast<::std::size_t>(signature.start.end - signature.start.begin)};
                    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
                    auto const if_required_overflows{if_param_count == max_operand_stack_requirement};
                    auto const if_required_stack_size{if_required_overflows ? max_operand_stack_requirement : (if_param_count + 1uz)};
                    if(!is_polymorphic && (if_required_overflows || concrete_operand_count() < if_required_stack_size)) [[unlikely]]
                    {
                        report_operand_stack_underflow(op_begin, u8"if", if_required_stack_size);
                    }

                    auto const cond{try_pop_concrete_operand()};
                        if(!operand_type_matches(cond, curr_operand_stack_value_type::i32)) [[unlikely]]
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_else;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto& if_frame{control_flow_stack.back_unchecked()};

                    // Validate the then-branch result before switching to else.
                    // Match `end`: polymorphic mode only relaxes underflow, but still rejects extra values
                    // and still checks types when enough concrete values are present.
                    auto const expected_count{if_frame.result.begin == if_frame.result.end ? 0uz :
                        static_cast<::std::size_t>(if_frame.result.end - if_frame.result.begin)};
                    auto const base{if_frame.operand_stack_base};
                    auto const stack_size{operand_stack.size()};
                    auto const actual_count{stack_size >= base ? stack_size - base : 0uz};

                    if(!is_polymorphic ? (actual_count != expected_count) : (actual_count > expected_count))
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                                auto const actual_operand{operand_stack.index_unchecked(stack_size - 1uz - i)};
                                if(!block_value_matches(actual_operand, if_frame.result, if_frame.signature_type_index, true,
                                    if_frame.has_singleton_result_core_type, if_frame.singleton_result_core_type,
                                    expected_count - 1uz - i)) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin;
                                    err.err_selectable.if_then_result_mismatch.expected_count = expected_count;
                                err.err_selectable.if_then_result_mismatch.actual_count = actual_count;
                                    err.err_selectable.if_then_result_mismatch.expected_type =
                                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                                    err.err_selectable.if_then_result_mismatch.actual_type =
                                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_operand.type);
                                    err.err_code = ::uwvm2::validation::error::code_validation_error_code::if_then_result_mismatch;
                                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                }
                        }
                    }

                    // Start else branch with the operand stack at if-entry height.
                    while(operand_stack.size() > if_frame.operand_stack_base) { operand_stack.pop_back_unchecked(); }
                    push_value_types(if_frame.start, if_frame.signature_type_index);
                    // As in the spec's push_ctrl(else, ...), the else-frame itself starts reachable.
                    is_polymorphic = false;

                    // Mark that else has been consumed.
                    // Core 3 pop_ctrl restores all non-defaultable locals set inside the then arm.
                    if(!initialized_locals.restore(if_frame.local_init_checkpoint)) [[unlikely]]
                    { ::fast_io::fast_terminate(); }
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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

                    auto const expected_count{frame.result.begin == frame.result.end ? 0uz :
                        static_cast<::std::size_t>(frame.result.end - frame.result.begin)};

                    // Without an explicit `else`, the false arm is the identity function over the block parameters.
                    // It is valid only when that implicit arm already has exactly the declared result tuple.
                    bool implicit_else_matches_result{true};
                    if(frame.type == block_type::if_)
                    {
                        auto const start_count{frame.start.begin == frame.start.end ? 0uz :
                            static_cast<::std::size_t>(frame.start.end - frame.start.begin)};
                        implicit_else_matches_result = start_count == expected_count;
                        for(::std::size_t i{}; implicit_else_matches_result && i != expected_count; ++i)
                        {
                            auto const start_rich{block_core_type_at(frame.start, frame.signature_type_index, false,
                                frame.has_singleton_result_core_type, frame.singleton_result_core_type, i)};
                            auto const result_rich{block_core_type_at(frame.result, frame.signature_type_index, true,
                                frame.has_singleton_result_core_type, frame.singleton_result_core_type, i)};
                            auto const start_core{start_rich.has_type ? start_rich.type :
                                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(frame.start.begin[i])};
                            auto const result_core{result_rich.has_type ? result_rich.type :
                                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(frame.result.begin[i])};
                            implicit_else_matches_result = frame.start.begin[i] == frame.result.begin[i] &&
                                ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(start_core, result_core,
                                    typesec);
                        }
                    }
                    if(frame.type == block_type::if_ && !implicit_else_matches_result) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_selectable.if_missing_else.expected_count = expected_count;
                        err.err_selectable.if_missing_else.expected_type =
                            expected_count == 1uz ? static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(*frame.result.begin) :
                                                   ::uwvm2::parser::wasm::standard::wasm1::type::value_type{};
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                                auto const actual_operand{operand_stack.index_unchecked(stack_size - 1uz - i)};
                                auto const matches{block_value_matches(actual_operand, frame.result,
                                    frame.signature_type_index, true, frame.has_singleton_result_core_type,
                                    frame.singleton_result_core_type, expected_count - 1uz - i)};
                                if(!matches) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin;
                                    err.err_selectable.end_result_mismatch.block_kind = block_kind;
                                err.err_selectable.end_result_mismatch.expected_count = expected_count;
                                err.err_selectable.end_result_mismatch.actual_count = actual_count;
                                    err.err_selectable.end_result_mismatch.expected_type =
                                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                                    err.err_selectable.end_result_mismatch.actual_type =
                                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_operand.type);
                                    err.err_code = ::uwvm2::validation::error::code_validation_error_code::end_result_mismatch;
                                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                }
                        }
                    }

                    // Leave the frame: discard any intermediate values and push the declared results for outer typing.
                    while(operand_stack.size() > base) { operand_stack.pop_back_unchecked(); }
                    push_value_types(frame.result, frame.signature_type_index, true,
                        frame.has_singleton_result_core_type, frame.singleton_result_core_type);

                    // Core 1/2 validation restores the enclosing control frame at `end`.
                    // Its unreachable flag is not a control-flow merge: even two terminating
                    // if arms (including br 0, which reaches this end) cannot make a later
                    // missing operand valid. See Core 2, appendix 7.3, pop_ctrl/end.
                    is_polymorphic = frame.polymorphic_base;

                    // Pop the control frame.
                    // Core 3 pop_ctrl does not export local.set effects from a nested frame.
                    if(!initialized_locals.restore(frame.local_init_checkpoint)) [[unlikely]]
                    { ::fast_io::fast_terminate(); }
                    control_flow_stack.pop_back_unchecked();

                    // The function body is a single expression terminated by `end`. When the function frame is closed,
                    // validation of this function is complete and `end` must be the last opcode in the body.
                    if(is_function_frame)
                    {
                        if(code_curr != code_end) [[unlikely]]
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::trailing_code_after_end;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if constexpr(ValidatedOperationSink::retains_operations)
                        {
                            // [owned expression ... bounded cursor == code_end]
                            // [safe] integer offset copy only; one-past is never read.
                            validated_operations.finish_instruction(static_cast<::std::size_t>(code_curr - code_begin), operand_stack.size(),
                                operand_stack.empty() ? ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type{} :
                                    operand_stack.back_unchecked().has_core_type ? operand_stack.back_unchecked().core_type :
                                    core3_legacy_carrier_type(to_wasm1_value_type(operand_stack.back_unchecked().type)));
                            validated_operations.finish_function();
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                    auto const target_arity{target_frame.label.begin == target_frame.label.end ? 0uz :
                        static_cast<::std::size_t>(target_frame.label.end - target_frame.label.begin)};

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
                                auto const actual_operand{operand_stack.index_unchecked(operand_stack.size() - 1uz - i)};
                                if(!block_value_matches(actual_operand, target_frame.label, target_frame.signature_type_index,
                                    target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
                                    target_frame.singleton_result_core_type, target_arity - 1uz - i)) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin;
                                    err.err_selectable.br_value_type_mismatch.op_code_name = u8"br";
                                    err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                                    err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                    auto const target_arity{target_frame.label.begin == target_frame.label.end ? 0uz :
                        static_cast<::std::size_t>(target_frame.label.end - target_frame.label.begin)};

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
                        if(!operand_type_matches(cond, curr_operand_stack_value_type::i32)) [[unlikely]]
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                                auto const actual_operand{operand_stack.index_unchecked(operand_stack.size() - 1uz - i)};
                                if(!block_value_matches(actual_operand, target_frame.label, target_frame.signature_type_index,
                                    target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
                                    target_frame.singleton_result_core_type, target_arity - 1uz - i)) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin;
                                    err.err_selectable.br_value_type_mismatch.op_code_name = u8"br_if";
                                    err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                                    err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                                    err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                }
                        }

                        // WebAssembly 3.0 pop_vals/push_vals reifies label arguments on every fallthrough,
                        // including reachable code: a subtype consumed by this instruction cannot retain a
                        // narrower type than the target label declares. Polymorphic missing operands are
                        // supplied by push_value_types after the available concrete values are removed.
                        pop_available_concrete_operands(concrete_to_check);
                        push_value_types(target_frame.label, target_frame.signature_type_index,
                            target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
                            target_frame.singleton_result_core_type);
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                                                      // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                                      // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                                      // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                                      err.err_curr = op_begin;
                                                      err.err_selectable.illegal_label_index.label_index = li;
                                                      err.err_selectable.illegal_label_index.all_label_count =
                                                          static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_label_count_uz);
                                                      err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_label_index;
                                                      ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                                  }
                                              }};

                    struct get_sig_result_t
                    { block_result_type<Fs...> types{}; };

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

                            // Empty signatures use a null borrowed range; never subtract null pointers.
                            auto const expected_arity{expected_label_types.begin == expected_label_types.end ? 0uz :
                                static_cast<::std::size_t>(expected_label_types.end - expected_label_types.begin)};
                            auto const actual_arity{actual_types.begin == actual_types.end ? 0uz :
                                static_cast<::std::size_t>(actual_types.end - actual_types.begin)};
                            bool mismatch{expected_arity != actual_arity};
                            curr_operand_stack_value_type expected_type{};
                            curr_operand_stack_value_type actual_type{};

                            auto const comparable_count{expected_arity < actual_arity ? expected_arity : actual_arity};
                            for(::std::size_t i{}; i != comparable_count; ++i)
                            {
                                auto const& expected_frame{control_flow_stack.index_unchecked(
                                    all_label_count_uz - 1uz - static_cast<::std::size_t>(expected_label))};
                                auto const& actual_frame{control_flow_stack.index_unchecked(
                                    all_label_count_uz - 1uz - static_cast<::std::size_t>(li))};
                                bool matches{};
                                if(::uwvm2::parser::wasm::standard::wasm1p1::features::uses_mvp_validation_rules(wasm1p1_para))
                                {
                                    // The explicit MVP policy requires identical target labels.
                                    auto const expected_rich{block_core_type_at(expected_label_types, expected_frame.signature_type_index,
                                        expected_frame.type != block_type::loop, expected_frame.has_singleton_result_core_type,
                                        expected_frame.singleton_result_core_type, i)};
                                    auto const actual_rich{block_core_type_at(actual_types, actual_frame.signature_type_index,
                                        actual_frame.type != block_type::loop, actual_frame.has_singleton_result_core_type,
                                        actual_frame.singleton_result_core_type, i)};
                                    auto const expected_core{expected_rich.has_type ? expected_rich.type :
                                        ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(expected_label_types.begin[i])};
                                    auto const actual_core{actual_rich.has_type ? actual_rich.type :
                                        ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(actual_types.begin[i])};
                                    matches = expected_label_types.begin[i] == actual_types.begin[i] &&
                                        ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(expected_core, actual_core,
                                            typesec) &&
                                        ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(actual_core, expected_core,
                                            typesec);
                                }
                                else
                                {
                                    // Core 3 permits different label types when the same stack argument is a
                                    // subtype of each. The selector is still on top at depth zero.
                                    auto const depth_from_top{expected_arity - i};
                                    if(concrete_operand_count() <= depth_from_top)
                                    {
                                        // A missing argument is bottom only in polymorphic code; reachable
                                        // underflow is reported after all target immediates are decoded.
                                        matches = true;
                                    }
                                    else
                                    {
                                        // [frame base ... argument ... selector] is the live operand range.
                                        // [safe       | safe     | safe    ] unsafe (vector end)
                                        //               ^^ index_unchecked(size - 1 - depth_from_top)
                                        // No parser or operand-stack pointer advances in this target check.
                                        auto const& argument{operand_stack.index_unchecked(
                                            operand_stack.size() - 1uz - depth_from_top)};
                                        matches = block_value_matches(argument, actual_types, actual_frame.signature_type_index,
                                            actual_frame.type != block_type::loop, actual_frame.has_singleton_result_core_type,
                                            actual_frame.singleton_result_core_type, i);
                                    }
                                }
                                if(!matches)
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

                                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(li_err);
                        }

                        // ...   | curr_target ...
                        // [safe | safe      ] unsafe (could be the section_end)
                        //         ^^ code_curr: successful bounded LEB scan proved [code_curr, li_next).

                        code_curr = reinterpret_cast<::std::byte const*>(li_next);

                        // ...   | curr_target ...
                        // [safe | safe      ] unsafe (could be the section_end)
                        //                     ^^ code_curr: the next byte is read only after a new bound check.

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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(def_err);
                    }

                    // ... last_target | default_label ...
                    // [         safe  |      safe   ] unsafe (could be the section_end)
                    //                   ^^ code_curr: successful bounded LEB scan proved [code_curr, def_next).

                    code_curr = reinterpret_cast<::std::byte const*>(def_next);

                    // ... last_target | default_label ...
                    // [         safe  |      safe   ] unsafe (could be the section_end)
                    //                                 ^^ code_curr: this may be section_end and is not dereferenced.

                    validate_label(default_label);

                    check_br_table_sig(default_label, get_sig(default_label).types);

                    // Stack effect: (labelargs..., i32 index) -> unreachable
                    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
                    auto const expected_arity{expected_label_types.begin == expected_label_types.end ? 0uz :
                        static_cast<::std::size_t>(expected_label_types.end - expected_label_types.begin)};
                    auto const expected_arity_plus_index_overflows{expected_arity == max_operand_stack_requirement};
                    auto const required_stack_size{expected_arity_plus_index_overflows ? max_operand_stack_requirement : (expected_arity + 1uz)};

                    if(!is_polymorphic && (expected_arity_plus_index_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
                    {
                        report_operand_stack_underflow(op_begin, u8"br_table", required_stack_size);
                    }

                    auto const idx{try_pop_concrete_operand()};
                        if(!operand_type_matches(idx, curr_operand_stack_value_type::i32)) [[unlikely]]
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_selectable.br_cond_type_not_i32.op_code_name = u8"br_table";
                        err.err_selectable.br_cond_type_not_i32.cond_type = to_wasm1_value_type(idx.type);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_cond_type_not_i32;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(expected_arity != 0uz)
                    {
                        auto const& expected_frame{control_flow_stack.index_unchecked(
                            all_label_count_uz - 1uz - static_cast<::std::size_t>(expected_label))};
                        auto const concrete_to_check{concrete_operand_count() < expected_arity ? concrete_operand_count() : expected_arity};
                            for(::std::size_t i{}; i != concrete_to_check; ++i)
                            {
                                auto const actual_operand{operand_stack.index_unchecked(operand_stack.size() - 1uz - i)};
                                auto const curr_expected_type{expected_label_types.begin[expected_arity - 1uz - i]};
                                if(!block_value_matches(actual_operand, expected_label_types, expected_frame.signature_type_index,
                                    expected_frame.type != block_type::loop, expected_frame.has_singleton_result_core_type,
                                    expected_frame.singleton_result_core_type, expected_arity - 1uz - i)) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin;
                                    err.err_selectable.br_value_type_mismatch.op_code_name = u8"br_table";
                                    err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(curr_expected_type);
                                    err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
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

                    ::std::size_t const return_arity{func_frame.result.begin == func_frame.result.end ? 0uz :
                        static_cast<::std::size_t>(func_frame.result.end - func_frame.result.begin)};

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
                                auto const actual_operand{operand_stack.index_unchecked(operator_stack_size - 1uz - i)};
                                auto const matches{curr_owned_signature != nullptr && !actual_operand.is_unknown ?
                                    ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                        ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual_operand),
                                        curr_owned_signature->results.index_unchecked(return_arity - 1uz - i),
                                        typesec) : stack_entry_type_matches(actual_operand, expected_type)};
                                if(!matches) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin;
                                    err.err_selectable.br_value_type_mismatch.op_code_name = u8"return";
                                    err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                                    err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        // callee type: [checked import's function type record] record end
                        // [safe complete borrowed record                  ] unsafe (past record end)
                        // ^^ callee_type_ptr takes the checked import's function type pointer for this call.
                        callee_type_ptr = imported_func_ptr->imports.storage.function;
                    }
                    else
                    {
                        auto const local_idx{static_cast<::std::size_t>(func_index) - import_func_count};
                        // callee type: typesec.types.begin [validated type index] typesec.types.end
                        // [safe element within typesec.types                     ] unsafe (past end)
                        // ^^ callee_type_ptr uses the parsed local function's validated type index to borrow that element.
                        callee_type_ptr = typesec.types.cbegin() + funcsec.funcs.index_unchecked(local_idx);
                    }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(callee_type_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

                    auto const& callee_type{*callee_type_ptr};

                    auto const param_count{callee_type.parameter.begin == callee_type.parameter.end ? 0uz :
                        static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
                    auto const result_count{callee_type.result.begin == callee_type.result.end ? 0uz :
                        static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};
                    auto const rich_type_index{type_index_from_pointer(callee_type_ptr)};
                    auto const* rich_callee{rich_type_index < typesec.owned_signatures.size() ?
                        ::std::addressof(typesec.owned_signatures.index_unchecked(rich_type_index)) : nullptr};

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
                                auto const actual_operand{operand_stack.index_unchecked(stack_size - 1uz - i)};
                                auto const matches{rich_callee != nullptr && !actual_operand.is_unknown ?
                                    ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                        ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual_operand),
                                        rich_callee->parameters.index_unchecked(param_count - 1uz - i),
                                        typesec) : stack_entry_type_matches(actual_operand, expected_type)};
                                if(!matches) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin;
                                    err.err_selectable.br_value_type_mismatch.op_code_name = u8"call";
                                    err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                                    err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
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
                        for(::std::size_t i{}; i != result_count; ++i)
                        {
                            operand_stack.push_back({callee_type.result.begin[i]});
                            if(rich_callee != nullptr)
                            {
                                operand_stack.back_unchecked().core_type = rich_callee->results.index_unchecked(i);
                                operand_stack.back_unchecked().has_core_type = true;
                            }
                        }
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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

                    // Decode through a local scanner and commit code_curr only after the complete u32 field, so a
                    // malformed/truncated tableidx leaves code_curr at the table_index position shown above.
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_index{};
                    // Core 2.0 section 5.4.1 always encodes the trailing immediate as
                    // `tableidx ::= u32`.  The multiple-tables policy is a validation
                    // constraint on the decoded value; it does not change the grammar.
                    auto const [table_next, table_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(table_index))};
                    if(table_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_table_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(table_err);
                    }
                    // call_indirect type_index table_index ...
                    // [             safe              ] unsafe (could be the section_end)
                    //                   ^^ code_curr: successful bounded LEB scan proves table_next is at most code_end.
                    code_curr = reinterpret_cast<::std::byte const*>(table_next);

                    // call_indirect type_index table_index ...
                    // [                safe              ] unsafe (could be the section_end)
                    //                                      ^^ code_curr

                    // Both immediate fields now have valid encodings.  Semantic checks intentionally start with
                    // type_index so every validator/backend reports the same first error for compound-invalid operands.
                    auto const all_type_count_uz{typesec.types.size()};
                    if(static_cast<::std::size_t>(type_index) >= all_type_count_uz) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_type_index.type_index = type_index;
                        err.err_selectable.illegal_type_index.all_type_count =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_type_count_uz);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    require_core3_function_type_index_policy(::std::addressof(typesec.core3_context),
                        static_cast<::std::size_t>(type_index), op_begin, u8"call_indirect", err);
                    check_table_index(op_begin,
                                      table_index,
                                      static_cast<wasm_u32>(static_cast<wasm_byte>(wasm1_code::call_indirect)));

                    if(!core3_indirect_call_table_type_matches(get_table_core_type(table_index), typesec)) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_selectable.br_value_type_mismatch.op_code_name = u8"call_indirect";
                        err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(
                            static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref));
                        err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(get_table_value_type(table_index));
                        err.err_code = code_validation_error_code::br_value_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Resolve the function signature by type index.
                    auto const& callee_type{typesec.types.index_unchecked(static_cast<::std::size_t>(type_index))};
                    auto const param_count{callee_type.parameter.begin == callee_type.parameter.end ? 0uz :
                        static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
                    auto const result_count{callee_type.result.begin == callee_type.result.end ? 0uz :
                        static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};
                    // The checked type_index identifies the same parser-retained record in both
                    // arrays. Rich parameters/results are validation metadata only.
                    // [typesec.owned_signatures.begin, end) remains live for this validation.
                    // [safe                                ] type_index < types.size() was checked above.
                    //                                    ^^ indexed borrow; no input cursor moves.
                    auto const* rich_callee{rich_signatures_available ?
                        ::std::addressof(typesec.owned_signatures.index_unchecked(static_cast<::std::size_t>(type_index))) : nullptr};

                    // Stack effect: (args..., table_address_type table_element_index) -> (results...)
                    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
                    auto const param_count_plus_element_index_overflows{param_count == max_operand_stack_requirement};
                    auto const required_stack_size{param_count_plus_element_index_overflows ? max_operand_stack_requirement : (param_count + 1uz)};

                    if(!is_polymorphic && (param_count_plus_element_index_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
                    {
                        report_operand_stack_underflow(op_begin, u8"call_indirect", required_stack_size);
                    }

                    // table-element index operand (must match the selected table address type)
                    validate_storage_operand(op_begin, u8"call_indirect", table_operand_type(table_index));

                    auto const stack_size{operand_stack.size()};
                    auto const available_param_count{concrete_operand_count()};
                    if(param_count != 0uz)
                    {
                        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
                            for(::std::size_t i{}; i != concrete_to_check; ++i)
                            {
                                auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
                                auto const actual_operand{operand_stack.index_unchecked(stack_size - 1uz - i)};
                                auto const matches{rich_callee != nullptr && !actual_operand.is_unknown ?
                                    ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                        ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual_operand),
                                        rich_callee->parameters.index_unchecked(param_count - 1uz - i),
                                        typesec) : stack_entry_type_matches(actual_operand, expected_type)};
                                if(!matches) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin;
                                    err.err_selectable.br_value_type_mismatch.op_code_name = u8"call_indirect";
                                    err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                                    err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                                    err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                }
                        }
                    }

                    pop_available_concrete_operands(param_count);

                    if(result_count != 0uz)
                    {
                        for(::std::size_t i{}; i != result_count; ++i)
                        {
                            operand_stack.push_back({callee_type.result.begin[i]});
                            if(rich_callee != nullptr)
                            {
                                operand_stack.back_unchecked().core_type = rich_callee->results.index_unchecked(i);
                                operand_stack.back_unchecked().has_core_type = true;
                            }
                        }
                    }

                    break;
                }
                case static_cast<wasm_byte>(0x12u):
                {
                    require_tail_call_enabled(!wasm1p1_para.disable_tail_call, 0x12u, code_curr, err);
                    // return_call     func_index ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // return_call     func_index ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // return_call     func_index ...
                    // [ safe ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [func_next, func_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                              ::fast_io::mnp::leb128_get(func_index))};
                    if(func_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        // [tail opcode] immediates ... code_end
                        // [safe       ] unsafe (could be code_end); no dereference.
                        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_function_index_encoding;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(func_err);
                    }

                    // return_call func_index ...
                    // [      safe   ] unsafe (could be the section_end)
                    //      ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(func_next);

                    // return_call func_index ...
                    // [      safe   ] unsafe (could be the section_end)
                    //                ^^ code_curr

                    // Validate function index range (imports + locals)
                    auto const all_function_size{import_func_count + local_func_count};
                    if(static_cast<::std::size_t>(func_index) >= all_function_size) [[unlikely]]
                    {
                        // [tail opcode] immediates ... code_end
                        // [safe       ] unsafe (could be code_end); no dereference.
                        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
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
                        // [validated import/type storage] end; function/type index checked above.
                        // [safe                         ]; borrowed until module retirement.
                        // ^^ callee_type_ptr receives the complete function-type record.
                        callee_type_ptr = imported_func_ptr->imports.storage.function;
                    }
                    else
                    {
                        auto const local_idx{static_cast<::std::size_t>(func_index) - import_func_count};
                        // [validated import/type storage] end; function/type index checked above.
                        // [safe                         ]; borrowed until module retirement.
                        // ^^ callee_type_ptr receives the complete function-type record.
                        callee_type_ptr = typesec.types.cbegin() + funcsec.funcs.index_unchecked(local_idx);
                    }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(callee_type_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

                    auto const& callee_type{*callee_type_ptr};

                    auto const param_count{(callee_type.parameter.begin == callee_type.parameter.end ? 0uz : static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin))};
                    validate_tail_call_results(control_flow_stack.index_unchecked(0u).result, callee_type.result,
                                               op_begin, u8"return_call", err);
                    auto const rich_index{type_index_from_pointer(callee_type_ptr)};
                    // [typesec.owned_signatures.begin, end) is retained by the parsed module.
                    // [safe                                ] rich_index is checked before the borrow.
                    //                                    ^^ rich_callee is validation metadata only.
                    auto const* rich_callee{rich_index < typesec.owned_signatures.size() ?
                        ::std::addressof(typesec.owned_signatures.index_unchecked(rich_index)) : nullptr};
                    if(rich_callee != nullptr)
                    {
                        auto const& caller_results{curr_owned_signature->results};
                        if(rich_callee->results.size() != caller_results.size()) [[unlikely]]
                        { details::fail_invalid_immediate(op_begin, err, u8"return_call"); }
                        for(::std::size_t i{}; i != caller_results.size(); ++i)
                        {
                            if(!::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                rich_callee->results.index_unchecked(i), caller_results.index_unchecked(i),
                                typesec)) [[unlikely]]
                            { details::fail_invalid_immediate(op_begin, err, u8"return_call"); }
                        }
                    }

                    if(!is_polymorphic && concrete_operand_count() < param_count) [[unlikely]]
                    {
                        report_operand_stack_underflow(op_begin, u8"return_call", param_count);
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
                                auto const actual_operand{operand_stack.index_unchecked(stack_size - 1uz - i)};
                                auto const matches{rich_callee != nullptr && !actual_operand.is_unknown ?
                                    ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                        ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual_operand),
                                        rich_callee->parameters.index_unchecked(param_count - 1uz - i),
                                        typesec) : stack_entry_type_matches(actual_operand, expected_type)};
                                if(!matches) [[unlikely]]
                                {
                                    // [tail opcode] immediates ... code_end
                                    // [safe       ] unsafe (could be code_end); no dereference.
                                    // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
                                    err.err_curr = op_begin;
                                    err.err_selectable.br_value_type_mismatch.op_code_name = u8"return_call";
                                    err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                                    err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                                    err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                }
                        }
                    }

                    // Consume parameters if present.
                    pop_available_concrete_operands(param_count);

                    // A tail call discards every value above the CURRENT control
                    // frame's base and makes the remainder stack-polymorphic.
                    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
                    while(operand_stack.size() > curr_frame_base) { operand_stack.pop_back_unchecked(); }
                    is_polymorphic = true;

                    break;
                }
                case static_cast<wasm_byte>(0x13u):
                {
                    require_tail_call_enabled(!wasm1p1_para.disable_tail_call, 0x13u, code_curr, err);
                    // return_call_indirect  type_index table_index ...
                    // [ safe      ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // return_call_indirect  type_index table_index ...
                    // [ safe      ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // return_call_indirect type_index table_index ...
                    // [    safe   ] unsafe (could be the section_end)
                    //               ^^ code_curr

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 type_index;  // No initialization necessary
                    auto const [type_next, type_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                              ::fast_io::mnp::leb128_get(type_index))};
                    if(type_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        // [tail opcode] immediates ... code_end
                        // [safe       ] unsafe (could be code_end); no dereference.
                        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(type_err);
                    }

                    // return_call_indirect type_index table_index ...
                    // [          safe        ] unsafe (could be the section_end)
                    //               ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(type_next);

                    // return_call_indirect type_index table_index ...
                    // [          safe        ] unsafe (could be the section_end)
                    //                          ^^ code_curr

                    // Decode through a local scanner and commit code_curr only after the complete u32 field, so a
                    // malformed/truncated tableidx leaves code_curr at the table_index position shown above.
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_index{};
                    // Core 2.0 section 5.4.1 always encodes the trailing immediate as
                    // `tableidx ::= u32`.  The multiple-tables policy is a validation
                    // constraint on the decoded value; it does not change the grammar.
                    auto const [table_next, table_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(table_index))};
                    if(table_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        // [tail opcode] immediates ... code_end
                        // [safe       ] unsafe (could be code_end); no dereference.
                        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_table_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(table_err);
                    }
                    // return_call_indirect type_index table_index ... code_end
                    // [                safe              ] unsafe (could be code_end)
                    //                    ^^ code_curr: successful bounded LEB scan produced table_next.
                    code_curr = reinterpret_cast<::std::byte const*>(table_next);

                    // return_call_indirect type_index table_index ...
                    // [                safe              ] unsafe (could be the section_end)
                    //                                      ^^ code_curr

                    // Both immediate fields now have valid encodings.  Semantic checks intentionally start with
                    // type_index so every validator/backend reports the same first error for compound-invalid operands.
                    auto const all_type_count_uz{typesec.types.size()};
                    if(static_cast<::std::size_t>(type_index) >= all_type_count_uz) [[unlikely]]
                    {
                        // [tail opcode] immediates ... code_end
                        // [safe       ] unsafe (could be code_end); no dereference.
                        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_type_index.type_index = type_index;
                        err.err_selectable.illegal_type_index.all_type_count =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_type_count_uz);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    require_core3_function_type_index_policy(::std::addressof(typesec.core3_context),
                        static_cast<::std::size_t>(type_index), op_begin, u8"return_call_indirect", err);
                    check_table_index(op_begin,
                                      table_index,
                                      static_cast<wasm_u32>(static_cast<wasm_byte>(0x13u)));

                    if(!core3_indirect_call_table_type_matches(get_table_core_type(table_index), typesec)) [[unlikely]]
                    {
                        // [tail opcode] immediates ... code_end
                        // [safe       ] unsafe (could be code_end); no dereference.
                        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
                        err.err_curr = op_begin;
                        err.err_selectable.br_value_type_mismatch.op_code_name = u8"return_call_indirect";
                        err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(
                            static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref));
                        err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(get_table_value_type(table_index));
                        err.err_code = code_validation_error_code::br_value_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Resolve the function signature by type index.
                    auto const& callee_type{typesec.types.index_unchecked(static_cast<::std::size_t>(type_index))};
                    auto const param_count{(callee_type.parameter.begin == callee_type.parameter.end ? 0uz : static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin))};
                    validate_tail_call_results(control_flow_stack.index_unchecked(0u).result, callee_type.result,
                                               op_begin, u8"return_call_indirect", err);
                    // The immediate was range-checked against the carrier type section above.
                    // [typesec.owned_signatures.begin, end) has the same indexed declarations.
                    // [safe                                ] type_index < types.size() was checked.
                    //                                    ^^ rich_callee is borrowed only when metadata exists.
                    auto const* rich_callee{rich_signatures_available ?
                        ::std::addressof(typesec.owned_signatures.index_unchecked(static_cast<::std::size_t>(type_index))) : nullptr};
                    if(rich_callee != nullptr)
                    {
                        auto const& caller_results{curr_owned_signature->results};
                        if(rich_callee->results.size() != caller_results.size()) [[unlikely]]
                        { details::fail_invalid_immediate(op_begin, err, u8"return_call_indirect"); }
                        for(::std::size_t i{}; i != caller_results.size(); ++i)
                        {
                            if(!::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                rich_callee->results.index_unchecked(i), caller_results.index_unchecked(i),
                                typesec)) [[unlikely]]
                            { details::fail_invalid_immediate(op_begin, err, u8"return_call_indirect"); }
                        }
                    }

                    // Stack effect: (args..., table_address_type table_element_index) -> (results...)
                    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
                    auto const param_count_plus_element_index_overflows{param_count == max_operand_stack_requirement};
                    auto const required_stack_size{param_count_plus_element_index_overflows ? max_operand_stack_requirement : (param_count + 1uz)};

                    if(!is_polymorphic && (param_count_plus_element_index_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
                    {
                        report_operand_stack_underflow(op_begin, u8"return_call_indirect", required_stack_size);
                    }

                    // table-element index operand (must match the selected table address type)
                    validate_storage_operand(op_begin, u8"return_call_indirect", table_operand_type(table_index));

                    auto const stack_size{operand_stack.size()};
                    auto const available_param_count{concrete_operand_count()};
                    if(param_count != 0uz)
                    {
                        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
                            for(::std::size_t i{}; i != concrete_to_check; ++i)
                            {
                                auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
                                auto const actual_operand{operand_stack.index_unchecked(stack_size - 1uz - i)};
                                auto const matches{rich_callee != nullptr && !actual_operand.is_unknown ?
                                    ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                        ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual_operand),
                                        rich_callee->parameters.index_unchecked(param_count - 1uz - i),
                                        typesec) : stack_entry_type_matches(actual_operand, expected_type)};
                                if(!matches) [[unlikely]]
                                {
                                    // [tail opcode] immediates ... code_end
                                    // [safe       ] unsafe (could be code_end); no dereference.
                                    // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
                                    err.err_curr = op_begin;
                                    err.err_selectable.br_value_type_mismatch.op_code_name = u8"return_call_indirect";
                                    err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                                    err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                                    err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                }
                        }
                    }

                    pop_available_concrete_operands(param_count);

                    // A tail call discards every value above the CURRENT control
                    // frame's base and makes the remainder stack-polymorphic.
                    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
                    while(operand_stack.size() > curr_frame_base) { operand_stack.pop_back_unchecked(); }
                    is_polymorphic = true;

                    break;
                }
                case static_cast<wasm_byte>(0x14u):
                case static_cast<wasm_byte>(0x15u):
                {
                    // call_ref / return_call_ref typeidx ... code_end
                    // [safe                      ] unsafe (could be code_end)
                    // ^^ op_begin; outer dispatch proved the opcode byte exists.
                    auto const op_begin{code_curr};
                    auto const tail{*code_curr == ::std::byte{0x15u}};
                    ::uwvm2::validation::standard::wasm3::require_function_references_enabled(
                        !wasm1p1_para.disable_function_references, tail ? 0x15u : 0x14u, op_begin, err);
                    if(tail) { require_tail_call_enabled(!wasm1p1_para.disable_tail_call, 0x15u, op_begin, err); }
                    // call_ref / return_call_ref typeidx ... code_end
                    // [safe                      ] unsafe (could be code_end)
                    //                             ^^ code_curr; only the checked opcode was skipped.
                    ++code_curr;
                    auto const op_name{tail ? ::uwvm2::utils::container::u8string_view{u8"return_call_ref"} :
                                              ::uwvm2::utils::container::u8string_view{u8"call_ref"}};
                    auto const type_index{details::read_leb128<wasm_u32>(code_curr, code_end, op_begin, err, op_name)};
                    // call_ref / return_call_ref [complete typeidx] ... code_end
                    // [safe                                      ] unsafe (could be code_end)
                    //                                           ^^ code_curr; bounded LEB decoder committed the complete field.
                    if(static_cast<::std::size_t>(type_index) >= typesec.types.size()) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_type_index.type_index = type_index;
                        err.err_selectable.illegal_type_index.all_type_count = static_cast<wasm_u32>(typesec.types.size());
                        err.err_code = code_validation_error_code::illegal_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    require_core3_function_type_index_policy(::std::addressof(typesec.core3_context),
                        static_cast<::std::size_t>(type_index), op_begin, op_name, err);
                    auto const& callee_type{typesec.types.index_unchecked(static_cast<::std::size_t>(type_index))};
                    auto const param_count{callee_type.parameter.begin == callee_type.parameter.end ? 0uz :
                        static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
                    auto const result_count{callee_type.result.begin == callee_type.result.end ? 0uz :
                        static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};
                    // The parser retains one exact signature per projected carrier type.
                    // Legacy modules without rich metadata keep their existing carrier path.
                    auto const rich_signatures{!typesec.owned_signatures.empty() &&
                        typesec.owned_signatures.size() == typesec.types.size()};
                    auto const* rich_callee{rich_signatures ?
                        ::std::addressof(typesec.owned_signatures.index_unchecked(static_cast<::std::size_t>(type_index))) : nullptr};
                    if(tail)
                    {
                        ::uwvm2::validation::standard::wasm3::validate_tail_call_results(
                            control_flow_stack.index_unchecked(0u).result, callee_type.result, op_begin, u8"return_call_ref", err);
                        if(rich_callee != nullptr)
                        {
                            auto const caller_type_index{static_cast<::std::size_t>(funcsec.funcs.index_unchecked(local_func_idx))};
                            auto const& caller_results{typesec.owned_signatures.index_unchecked(caller_type_index).results};
                            if(rich_callee->results.size() != caller_results.size()) [[unlikely]]
                            { details::fail_invalid_immediate(op_begin, err, u8"return_call_ref"); }
                            for(::std::size_t i{}; i != caller_results.size(); ++i)
                            {
                                if(!::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                    rich_callee->results.index_unchecked(i), caller_results.index_unchecked(i),
                                    typesec)) [[unlikely]]
                                { details::fail_invalid_immediate(op_begin, err, u8"return_call_ref"); }
                            }
                        }
                    }
                    constexpr auto max_size{(::std::numeric_limits<::std::size_t>::max)()};
                    auto const required{param_count == max_size ? max_size : param_count + 1uz};
                    if(!is_polymorphic && (param_count == max_size || concrete_operand_count() < required)) [[unlikely]]
                    { report_operand_stack_underflow(op_begin, op_name, required); }

                    // The function reference is above every argument. An erased funcref supertype
                    // cannot prove `(ref null typeidx)`; ref.func/ref.null carry an exact witness.
                    auto const reference{try_pop_concrete_operand()};
                    auto const ref_matches{::uwvm2::validation::standard::wasm3::core3_call_ref_reference_matches(
                        reference, static_cast<::std::size_t>(type_index), typesec.types.size(), rich_callee != nullptr,
                        [&](auto actual, auto expected) constexpr noexcept
                        { return ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(actual, expected, typesec); },
                        [&](::std::size_t index) constexpr noexcept -> auto const&
                        {
                            // [retained type records 0 ... index ... size) end
                            // [safe] shared matcher proved index<size BEFORE borrow.
                            return typesec.types.index_unchecked(index);
                        })};
                    if(!ref_matches) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_selectable.br_value_type_mismatch.op_code_name = op_name;
                        err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(
                            static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref));
                        err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(reference.type);
                        err.err_code = code_validation_error_code::br_value_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    auto const concrete_to_check{concrete_operand_count() < param_count ? concrete_operand_count() : param_count};
                    for(::std::size_t i{}; i != concrete_to_check; ++i)
                    {
                        auto const expected{callee_type.parameter.begin[param_count - 1uz - i]};
                        auto const actual{operand_stack.index_unchecked(operand_stack.size() - 1uz - i)};
                        auto const matches{rich_callee != nullptr && !actual.is_unknown ?
                            ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual),
                                rich_callee->parameters.index_unchecked(param_count - 1uz - i), typesec) :
                            stack_entry_type_matches(actual, expected)};
                        if(!matches) [[unlikely]]
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_selectable.br_value_type_mismatch.op_code_name = op_name;
                            err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected);
                            err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual.type);
                            err.err_code = code_validation_error_code::br_value_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    pop_available_concrete_operands(param_count);
                    if(tail)
                    {
                        auto const base{control_flow_stack.back_unchecked().operand_stack_base};
                        while(operand_stack.size() > base) { operand_stack.pop_back_unchecked(); }
                        is_polymorphic = true;
                    }
                    else
                    {
                        for(::std::size_t i{}; i != result_count; ++i)
                        {
                            operand_stack.push_back({callee_type.result.begin[i]});
                            if(rich_callee != nullptr)
                            {
                                operand_stack.back_unchecked().core_type = rich_callee->results.index_unchecked(i);
                                operand_stack.back_unchecked().has_core_type = true;
                            }
                        }
                    }
                    break;
                }
                case static_cast<wasm_byte>(wasm1p1_code::table_get):
                {
                    // table.get tableidx ...
                    // [safe   ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // table.get tableidx ...
                    // [safe   ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // table.get tableidx ...
                    // [safe   ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    if(!feature_enabled(wasm2_feature_kind::table_instructions)) [[unlikely]]
                    {
                        details::fail_wasm2_feature_required(op_begin,
                                                             err,
                                                             opcode_u32(wasm1p1_code::table_get),
                                                             ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                                             ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
                    }

                    // [consumed opcode][tableidx bytes ...] | code_end
                    // [safe opcode] unsafe (could be code_end); existing bounded LEB
                    // decoder commits code_curr only after the WHOLE u32 and within code_end.
                    auto const table_index{
                        details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr, code_end, op_begin, err, u8"table.get")};
                    check_table_index(op_begin, table_index, opcode_u32(wasm1p1_code::table_get));

                    (void)validate_table_access.template operator()<0x25u>(op_begin, table_index, u8"table.get");

                    break;
                }
                case static_cast<wasm_byte>(wasm1p1_code::table_set):
                {
                    // table.set tableidx ...
                    // [safe   ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // table.set tableidx ...
                    // [safe   ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // table.set tableidx ...
                    // [safe   ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    if(!feature_enabled(wasm2_feature_kind::table_instructions)) [[unlikely]]
                    {
                        details::fail_wasm2_feature_required(op_begin,
                                                             err,
                                                             opcode_u32(wasm1p1_code::table_set),
                                                             ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                                             ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
                    }

                    // [consumed opcode][tableidx bytes ...] | code_end
                    // [safe opcode] unsafe (could be code_end); existing bounded LEB
                    // decoder commits code_curr only after the WHOLE u32 and within code_end.
                    auto const table_index{
                        details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr, code_end, op_begin, err, u8"table.set")};
                    check_table_index(op_begin, table_index, opcode_u32(wasm1p1_code::table_set));

                    (void)validate_table_access.template operator()<0x26u>(op_begin, table_index, u8"table.set");

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
                        auto const cond{try_pop_concrete_operand()};
                        if(!operand_type_matches(cond, curr_operand_stack_value_type::i32)) [[unlikely]]
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_selectable.select_cond_type_not_i32.cond_type = to_wasm1_value_type(cond.type);
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::select_cond_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // v2
                        auto const v2{try_pop_concrete_operand()};

                        // v1 (kept as result when present, matching existing implementation)
                        auto const v1{try_peek_concrete_operand()};

                        if(v1.from_stack && v2.from_stack && !v1.is_unknown && !v2.is_unknown && v1.type != v2.type) [[unlikely]]
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_value_type(v1.type);
                            err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_value_type(v2.type);
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::select_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        bool const have_known_select_value_type{(v1.from_stack && !v1.is_unknown) || (v2.from_stack && !v2.is_unknown)};
                        auto const select_value_type{(v1.from_stack && !v1.is_unknown) ? v1.type : v2.type};
                        if(have_known_select_value_type && !is_untyped_select_value_type(select_value_type)) [[unlikely]]
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_value_type(select_value_type);
                        err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_value_type(select_value_type);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::select_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // If v1 is not present on the concrete stack but v2 is, we must still produce one result of v2's type.
                        if(!v1.from_stack)
                        {
                            if(v2.from_stack) { operand_stack.push_back({v2.type, v2.is_unknown}); }
                            else if(is_polymorphic) { push_unknown_operand(); }
                        }
                        else if(v1.is_unknown && v2.from_stack && !v2.is_unknown)
                        {
                            // The select result is the meet of both operands. Retaining a
                            // bottom left operand loses the concrete constraint from the right.
                            operand_stack.back_unchecked() = {.type = v2.type};
                        }

                        break;
                    }
                case static_cast<wasm_byte>(wasm1p1_code::select_t):
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

                    if(!feature_enabled(wasm2_feature_kind::reference_types)) [[unlikely]]
                    {
                        details::fail_feature_required(op_begin,
                                                       err,
                                                       opcode_u32(wasm1p1_code::select_t),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                    }

                    auto const result_type_count{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr,
                                                                                                                              code_end,
                                                                                                                              op_begin,
                                                                                                                              err,
                                                                                                                              u8"select.result_types")};

                    // select_t result_type_count result_type ...
                    // [           safe         ] unsafe (could be the section_end)
                    //                            ^^ code_curr

                    if(result_type_count != 1u) [[unlikely]] { details::fail_invalid_immediate(op_begin, err, u8"select.result_types"); }

                    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type result_core_type{};
                    auto const result_type_byte{::uwvm2::validation::standard::wasm3::read_core3_value_carrier(
        code_curr, code_end, !wasm1p1_para.disable_function_references, op_begin, err,
        typesec.types.size(), ::std::addressof(result_core_type), typesec.core3_context,
        !wasm1p1_para.disable_gc, !wasm1p1_para.disable_exceptions)};

                    // select_t result_type_count result_type ...
                    // [                 safe               ] unsafe (could be the section_end)
                    //                                        ^^ code_curr

                    auto const result_type{static_cast<curr_operand_stack_value_type>(result_type_byte)};
                    if(result_type_byte != 0x69u)
                    { ensure_wasm1p1_value_type_enabled(op_begin, result_type,
                        ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction); }

                    (void)validate_typed_select(op_begin, result_type, result_core_type);

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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_local_index.local_index = local_index;
                            err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    if(!initialized_locals.is_initialized(local_index, local_initially_initialized(local_index))) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin; // Borrow the dispatch-checked opcode; no input read.
                        err.err_selectable.br_value_type_mismatch = {
                            .op_code_name = u8"local.get (unset non-null local)",
                            .expected_type = to_wasm1_value_type(curr_local_type),
                            .actual_type = to_wasm1_value_type(curr_local_type)};
                        err.err_code = code_validation_error_code::br_value_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    // local.get always pushes one value of the local's exact type, even in polymorphic mode.
                    operand_stack.push_back({curr_local_type});
                    operand_stack.back_unchecked().core_type = local_core_type(local_index);
                    operand_stack.back_unchecked().has_core_type = true;

                    if constexpr(ValidatedOperationSink::retains_operations)
                    { validated_operations.immediate_u32(local_index); }

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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_local_index.local_index = local_index;
                            err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"local.set", 1uz); }

                        auto const value{try_pop_concrete_operand()};
                        auto const matches{rich_signatures_available && value.from_stack && !value.is_unknown ?
                            ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(value),
                                local_core_type(local_index), typesec) :
                            operand_type_matches(value, curr_local_type)};
                        if(!matches) [[unlikely]]
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_selectable.local_variable_type_mismatch.local_index = local_index;
                            err.err_selectable.local_variable_type_mismatch.expected_type = to_wasm1_value_type(curr_local_type);
                            err.err_selectable.local_variable_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::local_set_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                    initialized_locals.initialize(local_index, local_initially_initialized(local_index));

                    if constexpr(ValidatedOperationSink::retains_operations)
                    { validated_operations.immediate_u32(local_index); }

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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                            auto const matches{rich_signatures_available && value.from_stack && !value.is_unknown ?
                                ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                    ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(value),
                                    local_core_type(local_index), typesec) :
                                operand_type_matches(value, curr_local_type)};
                            if(!matches) [[unlikely]]
                            {
                                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                err.err_curr = op_begin;
                                err.err_selectable.local_variable_type_mismatch.local_index = local_index;
                            err.err_selectable.local_variable_type_mismatch.expected_type = to_wasm1_value_type(curr_local_type);
                            err.err_selectable.local_variable_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                            err.err_code = ::uwvm2::validation::error::code_validation_error_code::local_tee_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    // local.tee consumes t and produces t: a present Unknown is refined,
                    // not retained for a later consumer of an incompatible concrete type.
                    operand_stack.back_unchecked().type = curr_local_type;
                    operand_stack.back_unchecked().is_unknown = false;
                    operand_stack.back_unchecked().is_reference_bottom = false;
                    operand_stack.back_unchecked().core_type = local_core_type(local_index);
                    operand_stack.back_unchecked().has_core_type = true;
                    initialized_locals.initialize(local_index, local_initially_initialized(local_index));

                    if constexpr(ValidatedOperationSink::retains_operations)
                    { validated_operations.immediate_u32(local_index); }

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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                    operand_stack.back_unchecked().core_type = get_global_core_type(global_index);
                    operand_stack.back_unchecked().has_core_type = true;

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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_selectable.immutable_global_set.global_index = global_index;
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::immutable_global_set;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (value) -> () where value must match global's value type
                    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"global.set", 1uz); }

                        auto const value{try_pop_concrete_operand()};
                        auto const matches{rich_signatures_available && value.from_stack && !value.is_unknown ?
                            ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(value),
                                get_global_core_type(global_index), typesec) :
                            operand_type_matches(value, curr_global_type)};
                        if(!matches) [[unlikely]]
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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

                    // [memory.size] memidx ... (code_end); the scanner bounds-checks the entire immediate.
                    auto const memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(
                        code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
                    // [memory.size memidx] ... unsafe (could be code_end)
                    //                      ^^ code_curr
                    ::uwvm2::validation::standard::wasm3::validate_memory_index(memory_index, all_memory_count, op_begin, err);

                    validate_checked_memory_page(op_begin, memory_address_type_at(memory_index), false);

                    // Stack effect: () -> (the selected memory address type)
                    operand_stack.push_back({memory_operand_type(memory_index)});

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

                    // [memory.grow] memidx ... (code_end); the scanner bounds-checks the entire immediate.
                    auto const memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(
                        code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
                    // [memory.grow memidx] ... unsafe (could be code_end)
                    //                      ^^ code_curr
                    ::uwvm2::validation::standard::wasm3::validate_memory_index(memory_index, all_memory_count, op_begin, err);

                    validate_checked_memory_page(op_begin, memory_address_type_at(memory_index), true);

                    operand_stack.push_back({memory_operand_type(memory_index)});

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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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

                    if constexpr(ValidatedOperationSink::retains_operations)
                    { validated_operations.immediate_i32(imm); }

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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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

                    if constexpr(ValidatedOperationSink::retains_operations)
                    { validated_operations.immediate_i64(imm); }

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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                    (void)validate_integer_compare.template operator()<0x45u>(u8"i32.eqz");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_eq):
                {
                    (void)validate_integer_compare.template operator()<0x46u>(u8"i32.eq");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_ne):
                {
                    (void)validate_integer_compare.template operator()<0x47u>(u8"i32.ne");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_lt_s):
                {
                    (void)validate_integer_compare.template operator()<0x48u>(u8"i32.lt_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_lt_u):
                {
                    (void)validate_integer_compare.template operator()<0x49u>(u8"i32.lt_u");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_gt_s):
                {
                    (void)validate_integer_compare.template operator()<0x4au>(u8"i32.gt_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_gt_u):
                {
                    (void)validate_integer_compare.template operator()<0x4bu>(u8"i32.gt_u");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_le_s):
                {
                    (void)validate_integer_compare.template operator()<0x4cu>(u8"i32.le_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_le_u):
                {
                    (void)validate_integer_compare.template operator()<0x4du>(u8"i32.le_u");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_ge_s):
                {
                    (void)validate_integer_compare.template operator()<0x4eu>(u8"i32.ge_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_ge_u):
                {
                    (void)validate_integer_compare.template operator()<0x4fu>(u8"i32.ge_u");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_eqz):
                {
                    (void)validate_integer_compare.template operator()<0x50u>(u8"i64.eqz");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_eq):
                {
                    (void)validate_integer_compare.template operator()<0x51u>(u8"i64.eq");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_ne):
                {
                    (void)validate_integer_compare.template operator()<0x52u>(u8"i64.ne");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_lt_s):
                {
                    (void)validate_integer_compare.template operator()<0x53u>(u8"i64.lt_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_lt_u):
                {
                    (void)validate_integer_compare.template operator()<0x54u>(u8"i64.lt_u");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_gt_s):
                {
                    (void)validate_integer_compare.template operator()<0x55u>(u8"i64.gt_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_gt_u):
                {
                    (void)validate_integer_compare.template operator()<0x56u>(u8"i64.gt_u");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_le_s):
                {
                    (void)validate_integer_compare.template operator()<0x57u>(u8"i64.le_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_le_u):
                {
                    (void)validate_integer_compare.template operator()<0x58u>(u8"i64.le_u");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_ge_s):
                {
                    (void)validate_integer_compare.template operator()<0x59u>(u8"i64.ge_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_ge_u):
                {
                    (void)validate_integer_compare.template operator()<0x5au>(u8"i64.ge_u");
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
                    (void)validate_i32_numeric.template operator()<0x67u>(u8"i32.clz");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_ctz):
                {
                    (void)validate_i32_numeric.template operator()<0x68u>(u8"i32.ctz");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_popcnt):
                {
                    (void)validate_i32_numeric.template operator()<0x69u>(u8"i32.popcnt");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_add):
                {
                    (void)validate_i32_numeric.template operator()<0x6au>(u8"i32.add");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_sub):
                {
                    (void)validate_i32_numeric.template operator()<0x6bu>(u8"i32.sub");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_mul):
                {
                    (void)validate_i32_numeric.template operator()<0x6cu>(u8"i32.mul");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_div_s):
                {
                    (void)validate_i32_numeric.template operator()<0x6du>(u8"i32.div_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_div_u):
                {
                    (void)validate_i32_numeric.template operator()<0x6eu>(u8"i32.div_u");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_rem_s):
                {
                    (void)validate_i32_numeric.template operator()<0x6fu>(u8"i32.rem_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_rem_u):
                {
                    (void)validate_i32_numeric.template operator()<0x70u>(u8"i32.rem_u");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_and):
                {
                    (void)validate_i32_numeric.template operator()<0x71u>(u8"i32.and");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_or):
                {
                    (void)validate_i32_numeric.template operator()<0x72u>(u8"i32.or");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_xor):
                {
                    (void)validate_i32_numeric.template operator()<0x73u>(u8"i32.xor");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_shl):
                {
                    (void)validate_i32_numeric.template operator()<0x74u>(u8"i32.shl");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_shr_s):
                {
                    (void)validate_i32_numeric.template operator()<0x75u>(u8"i32.shr_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_shr_u):
                {
                    (void)validate_i32_numeric.template operator()<0x76u>(u8"i32.shr_u");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_rotl):
                {
                    (void)validate_i32_numeric.template operator()<0x77u>(u8"i32.rotl");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i32_rotr):
                {
                    (void)validate_i32_numeric.template operator()<0x78u>(u8"i32.rotr");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_clz):
                {
                    (void)validate_i64_numeric.template operator()<0x79u>(u8"i64.clz");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_ctz):
                {
                    (void)validate_i64_numeric.template operator()<0x7au>(u8"i64.ctz");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_popcnt):
                {
                    (void)validate_i64_numeric.template operator()<0x7bu>(u8"i64.popcnt");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_add):
                {
                    (void)validate_i64_numeric.template operator()<0x7cu>(u8"i64.add");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_sub):
                {
                    (void)validate_i64_numeric.template operator()<0x7du>(u8"i64.sub");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_mul):
                {
                    (void)validate_i64_numeric.template operator()<0x7eu>(u8"i64.mul");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_div_s):
                {
                    (void)validate_i64_numeric.template operator()<0x7fu>(u8"i64.div_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_div_u):
                {
                    (void)validate_i64_numeric.template operator()<0x80u>(u8"i64.div_u");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_rem_s):
                {
                    (void)validate_i64_numeric.template operator()<0x81u>(u8"i64.rem_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_rem_u):
                {
                    (void)validate_i64_numeric.template operator()<0x82u>(u8"i64.rem_u");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_and):
                {
                    (void)validate_i64_numeric.template operator()<0x83u>(u8"i64.and");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_or):
                {
                    (void)validate_i64_numeric.template operator()<0x84u>(u8"i64.or");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_xor):
                {
                    (void)validate_i64_numeric.template operator()<0x85u>(u8"i64.xor");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_shl):
                {
                    (void)validate_i64_numeric.template operator()<0x86u>(u8"i64.shl");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_shr_s):
                {
                    (void)validate_i64_numeric.template operator()<0x87u>(u8"i64.shr_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_shr_u):
                {
                    (void)validate_i64_numeric.template operator()<0x88u>(u8"i64.shr_u");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_rotl):
                {
                    (void)validate_i64_numeric.template operator()<0x89u>(u8"i64.rotl");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_rotr):
                {
                    (void)validate_i64_numeric.template operator()<0x8au>(u8"i64.rotr");
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
                    (void)validate_integer_width.template operator()<0xa7u>(u8"i32.wrap_i64");
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
                    (void)validate_integer_width.template operator()<0xacu>(u8"i64.extend_i32_s");
                    break;
                }
                case static_cast<wasm_byte>(wasm1_code::i64_extend_i32_u):
                {
                    (void)validate_integer_width.template operator()<0xadu>(u8"i64.extend_i32_u");
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
                case static_cast<wasm_byte>(wasm1p1_code::i32_extend8_s):
                {
                    if(!feature_enabled(wasm2_feature_kind::sign_extension)) [[unlikely]]
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
                case static_cast<wasm_byte>(wasm1p1_code::i32_extend16_s):
                {
                    if(!feature_enabled(wasm2_feature_kind::sign_extension)) [[unlikely]]
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
                case static_cast<wasm_byte>(wasm1p1_code::i64_extend8_s):
                {
                    if(!feature_enabled(wasm2_feature_kind::sign_extension)) [[unlikely]]
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
                case static_cast<wasm_byte>(wasm1p1_code::i64_extend16_s):
                {
                    if(!feature_enabled(wasm2_feature_kind::sign_extension)) [[unlikely]]
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
                case static_cast<wasm_byte>(wasm1p1_code::i64_extend32_s):
                {
                    if(!feature_enabled(wasm2_feature_kind::sign_extension)) [[unlikely]]
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
                case static_cast<wasm_byte>(wasm1p1_code::ref_null):
                {
                    // ref.null reftype ...
                    // [  safe ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};
                    ++code_curr;

                    // ref.null reftype ...
                    // [  safe ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    if(!feature_enabled(wasm2_feature_kind::reference_types)) [[unlikely]]
                    {
                        details::fail_feature_required(op_begin,
                                                       err,
                                                       opcode_u32(wasm1p1_code::ref_null),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null);
                    }

                    // [ref.null] heap type ... code_end
                    // [safe    ] unsafe (could be code_end)
                    //            ^^ heap_begin borrows the bounded decoder input for its exact type witness.
                    auto const heap_begin{code_curr};
                    if(!wasm1p1_para.disable_gc)
                    {
                        namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
                        namespace v3 = ::uwvm2::validation::standard::wasm3;
                        auto const decoded{v3::scan_core3_ref_null_heap(code_curr, code_end,
                            typesec.core3_context, true, !wasm1p1_para.disable_function_references,
                            !wasm1p1_para.disable_exceptions)};
                        // [ref.null][complete signed-33 heap] next ... code_end on success.
                        // [safe                            ] unsafe (possibly code_end)
                        //                                   ^^ code_curr: helper commits only validated bytes.
                        if(decoded.error == v3::core3_ref_null_error::function_references_disabled)
                        { v3::require_function_references_enabled(false, 0xd0u, op_begin, err); }
                        if(decoded.error == v3::core3_ref_null_error::exceptions_disabled)
                        { require_exceptions_enabled(false, 0xd0u, op_begin, err); }
                        if(decoded.error != v3::core3_ref_null_error::ok) [[unlikely]]
                        {
                            // [ref.null] heap ... code_end
                            // [safe    ] unsafe (possibly code_end)
                            // ^^ err_curr / op_begin: checked opcode byte borrowed for diagnostics.
                            err.err_curr = op_begin;
                            if(decoded.error == v3::core3_ref_null_error::unknown_type)
                            {
                                err.err_selectable.illegal_type_index.type_index =
                                    static_cast<wasm_u32>(decoded.heap.code);
                                err.err_selectable.illegal_type_index.all_type_count =
                                    static_cast<wasm_u32>(typesec.core3_context.records.size());
                                err.err_code = code_validation_error_code::illegal_type_index;
                            }
                            else
                            {
                                err.err_selectable.wasm1p1_invalid_reference_type.value =
                                    heap_begin == code_end ? 0u : ::std::to_integer<unsigned>(*heap_begin);
                                err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                            }
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                        auto const carrier{static_cast<curr_operand_stack_value_type>(decoded.carrier)};
                        if(decoded.carrier != 0x69u)
                        { ensure_wasm1p1_value_type_enabled(op_begin, carrier,
                            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type); }
                        // The sole decoder proved defined indices before this
                        // actual context kind witness; no source cursor moves.
                        auto const exact_function_heap{decoded.heap.is_defined() &&
                            typesec.core3_context.records.index_unchecked(
                                static_cast<::std::size_t>(decoded.heap.code)).kind == t3::composite_kind::function};
                        auto const event{v3::make_core3_ref_null_event(decoded.heap, decoded.carrier, exact_function_heap)};
                        operand_stack.push_back({.type = carrier,
                            .exact_function_type_index = event.exact_function_type_index,
                            .core_type = event.result_type, .has_core_type = true});
                        break;
                    }
                    // [ref.null] heap ... code_end
                    // [safe    ] unsafe (could be code_end)
                    //            ^^ code_curr: decoder borrows the validated expression span and commits only success.
                    auto const decoded{::uwvm2::validation::standard::wasm3::scan_function_ref_null_heap(
                        code_curr, code_end, !wasm1p1_para.disable_function_references,
                        typesec.types.size(), true, !wasm1p1_para.disable_gc)};
                    // [ref.null][complete signed heap immediate] next ... code_end on success.
                    // [safe                                   ] unsafe (could be code_end)
                    //                                           ^^ code_curr: unchanged on every decoder failure.
                    if(decoded.error != ::uwvm2::validation::standard::wasm3::function_heap_immediate_error::ok) [[unlikely]]
                    {
                        // The transactional failure kept code_curr at heap_begin. Reuse the diagnostic policy
                        // on this bounded immediate only; successful instructions never need a second decode.
                        // [ref.null] [failed heap ... code_end)
                        // [safe    ] unsafe (could be code_end)
                        //            ^^ code_curr: wrapper retains it on failure and throws before any input access escapes the span.
                        static_cast<void>(::uwvm2::validation::standard::wasm3::read_function_ref_null_carrier(
                            code_curr, code_end, !wasm1p1_para.disable_function_references,
                            typesec.types.size(), op_begin, err, true, !wasm1p1_para.disable_gc));
                        ::fast_io::fast_terminate(); // The same immutable immediate cannot succeed after the failed decode.
                    }
                    auto const rt_byte{decoded.carrier};
                    if(rt_byte == 0x69u)
                    { require_exceptions_enabled(!wasm1p1_para.disable_exceptions, 0xd0u, op_begin, err); }
                    auto const rt{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type>(rt_byte)};
                    using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
                    if(rt_byte != 0x69u && rt != reference_type::funcref && rt != reference_type::externref) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_selectable.wasm1p1_invalid_reference_type.value = rt_byte;
                        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const vt{rt_byte == 0x69u ? static_cast<curr_operand_stack_value_type>(0x69u) :
                        static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(rt))};
                    if(rt_byte != 0x69u)
                    { ensure_wasm1p1_value_type_enabled(op_begin, vt, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type); }
                    // Every successful reference retains its exact heap, including noexn when GC is disabled.
                    // The projected carrier supplies ABI width; it must not erase a bottom heap's Core 3 type.
                    auto const event{::uwvm2::validation::standard::wasm3::make_core3_ref_null_event(
                        decoded.heap, decoded.carrier, decoded.heap.is_defined())};
                    operand_stack.push_back({.type = vt,
                        .exact_function_type_index = event.exact_function_type_index,
                        .core_type = event.result_type, .has_core_type = true});
                    break;
                }
                case static_cast<wasm_byte>(wasm1p1_code::ref_is_null):
                {
                    // [ref.is_null] next opcode ... code_end
                    // [safe       ] unsafe (could be code_end)
                    // ^^ code_curr: outer dispatch proved this opcode byte readable.

                    auto const op_begin{code_curr};
                    ++code_curr;

                    // [ref.is_null] next opcode ... code_end
                    // [safe       ] unsafe (could be code_end)
                    //               ^^ code_curr: checked one-byte advance; no immediate follows.

                    if(!feature_enabled(wasm2_feature_kind::reference_types)) [[unlikely]]
                    {
                        details::fail_feature_required(op_begin,
                                                       err,
                                                       opcode_u32(wasm1p1_code::ref_is_null),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type);
                    }

                    namespace reference_semantics = ::uwvm2::validation::standard::wasm3;
                    decltype(try_pop_concrete_operand()) ref_value{};
                    auto const consume_reference{[&]() constexpr noexcept
                    {
                        // Count > 0 in the common transition proves the actual top entry.
                        // The native pop copies its rich type before retiring that stack slot.
                        ref_value = try_pop_concrete_operand();
                        return reference_semantics::core3_reference_operand{
                            reference_semantics::core3_operand_effective_type(ref_value),
                            !ref_value.from_stack || ref_value.is_unknown, static_cast<unsigned>(ref_value.type)};
                    }};
                    auto const transition{reference_semantics::apply_core3_ref_is_null_typed_transition(
                        is_polymorphic, concrete_operand_count, consume_reference)};
                    if(transition.error == reference_semantics::typed_stack_error::stack_underflow) [[unlikely]]
                    { report_operand_stack_underflow(op_begin, u8"ref.is_null", 1uz); }
                    if(transition.error != reference_semantics::typed_stack_error::ok) [[unlikely]]
                    {
                        // [caller-saved checked opcode] next ... | code_end
                        // [safe                       ]         | one-past is never read.
                        // ^^ op_begin -> err.err_curr: copy only; dispatch owns the range proof.
                        err.err_curr = op_begin;
                        err.err_selectable.wasm1p1_invalid_reference_type.value =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(ref_value.type);
                        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    auto const& event{transition.event};
                    operand_stack.push_back({.type = curr_operand_stack_value_type::i32,
                        .core_type = event.result_type, .has_core_type = true});
                    break;
                }

                case static_cast<wasm_byte>(0xd3u): // Core 3 ref.eq
                {
                    // [ref.eq] next opcode ... code_end
                    // [safe  ] unsafe (could be code_end)
                    // ^^ code_curr: dispatch proved this exact opcode byte readable.
                    auto const op_begin{code_curr};
                    ++code_curr;
                    // [ref.eq] next opcode ... code_end
                    // [safe  ] unsafe (possibly code_end)
                    //          ^^ code_curr: the single checked byte was consumed; no immediate follows.
                    if(wasm1p1_para.disable_gc) [[unlikely]]
                    {
                        details::fail_feature_required(op_begin, err, 0xd3u,
                            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
                            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                    }
                    namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
                    namespace v3 = ::uwvm2::validation::standard::wasm3;
                    struct ref_eq_stack_adapter
                    {
                        curr_operand_stack_type& values;
                        decltype(concrete_operand_count) const& count;
                        decltype(try_pop_concrete_operand) const& consume;
                        bool const& polymorphic;
                        [[nodiscard]] inline v3::core3_reference_error pop_expected(
                            t3::core_value_type expected, v3::recursive_type_context const& context) noexcept
                        {
                            using e = v3::core3_reference_error;
                            if(count() == 0uz) { return polymorphic ? e::ok : e::stack_underflow; }
                            auto const operand{consume()};
                            return operand.is_unknown ||
                                context.matches(v3::core3_operand_effective_type(operand), expected) ? e::ok : e::type_mismatch;
                        }
                        inline void push([[maybe_unused]] t3::core_value_type result) noexcept
                        { values.push_back({curr_operand_stack_value_type::i32}); }
                    };
                    ref_eq_stack_adapter stack{operand_stack, concrete_operand_count,
                        try_pop_concrete_operand, is_polymorphic};
                    auto const failure{v3::validate_core3_ref_eq(stack, typesec.core3_context, true)};
                    if(failure == v3::core3_reference_error::stack_underflow) [[unlikely]]
                    { report_operand_stack_underflow(op_begin, u8"ref.eq", 2uz); }
                    if(failure != v3::core3_reference_error::ok) [[unlikely]]
                    { details::fail_invalid_immediate(op_begin, err, u8"ref.eq operand type"); }
                    break;
                }

                case static_cast<wasm_byte>(0xd5u):
                case static_cast<wasm_byte>(0xd6u):
                {
                    // [br_on_null / br_on_non_null] labelidx ... next opcode ... code_end
                    // [safe                      ] unsafe (could be code_end)
                    // ^^ code_curr: outer dispatch checked this opcode byte.
                    auto const op_begin{code_curr};
                    auto const branch_non_null{static_cast<unsigned>(*code_curr) == 0xd6u};
                    auto const op_name{branch_non_null ? ::uwvm2::utils::container::u8string_view{u8"br_on_non_null"} : ::uwvm2::utils::container::u8string_view{u8"br_on_null"}};
                    ++code_curr;
                    // [branch opcode] labelidx ... next opcode ... code_end
                    // [safe         ] unsafe (could be code_end)
                    //                 ^^ code_curr: bounded decoder must check the complete u32 immediate.
                    ::uwvm2::validation::standard::wasm3::require_function_references_enabled(
                        !wasm1p1_para.disable_function_references, branch_non_null ? 0xd6u : 0xd5u, op_begin, err);
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 label_index;
                    auto const [label_next, label_error]{::fast_io::parse_by_scan(
                        reinterpret_cast<char8_t const*>(code_curr), reinterpret_cast<char8_t const*>(code_end), ::fast_io::mnp::leb128_get(label_index))};
                    if(label_error != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin; // Borrow the checked opcode for diagnostics; no dereference.
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(label_error);
                    }
                    // [branch opcode][complete labelidx] next opcode ... code_end
                    // [safe                           ] unsafe (could be code_end)
                    //                 ^^ code_curr; label_next is proven within [code_curr, code_end].
                    code_curr = reinterpret_cast<::std::byte const*>(label_next);
                    // [branch opcode][complete labelidx] next opcode ... code_end
                    // [safe                           ] unsafe (could be code_end)
                    //                                   ^^ code_curr
                    auto const label_count{control_flow_stack.size()};
                    if(static_cast<::std::uint_least64_t>(label_index) >= label_count) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin; // Diagnostic borrow within the checked instruction.
                        err.err_selectable.illegal_label_index = {.label_index = label_index,
                            .all_label_count = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(label_count)};
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_label_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    auto& target_frame{control_flow_stack.index_unchecked(label_count - 1uz - static_cast<::std::size_t>(label_index))};
                    auto const target_types{target_frame.label};
                    auto const target_arity{target_types.begin == target_types.end ? 0uz : static_cast<::std::size_t>(target_types.end - target_types.begin)};
                    if(branch_non_null && (target_arity == 0uz ||
                       !::uwvm2::validation::standard::wasm3::is_legacy_reference_carrier(target_types.begin[target_arity - 1uz]))) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin; // Diagnostic borrow; the short-circuit above protects the last-type access.
                        err.err_selectable.wasm1p1_invalid_reference_type.value = target_arity == 0uz ? 0x40u : static_cast<unsigned>(target_types.begin[target_arity - 1uz]);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_invalid_reference_type;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    auto const prefix_arity{target_arity - static_cast<::std::size_t>(branch_non_null)};
                    if(!is_polymorphic && concrete_operand_count() <= prefix_arity) [[unlikely]]
                    { report_operand_stack_underflow(op_begin, op_name, prefix_arity + 1uz); }
                    auto const reference{try_pop_concrete_operand()};
                    if(reference.from_stack && !reference.is_unknown &&
                       !is_reference_value_type(reference.type)) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin; // Borrow only, no input-pointer read.
                        err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<unsigned>(reference.type);
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_invalid_reference_type;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    auto const bottom{!reference.from_stack || reference.is_unknown || reference.is_reference_bottom};
                    auto const carrier{bottom ? curr_operand_stack_value_type::funcref : reference.type};
                    auto const require_match{[&](auto const& actual, ::std::size_t type_index) constexpr UWVM_THROWS
                    {
                        // [target_types.begin, target_types.end) is the validated label signature.
                        // [safe                                     ] unsafe (end)
                        //          ^^ begin[type_index], proven by type_index < target_arity.
                        // This also checks the Core 3 heap witness; equal funcref carriers alone
                        // cannot justify a branch to a different concrete function type.
                        if(block_value_matches(actual, target_types, target_frame.signature_type_index,
                            target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
                            target_frame.singleton_result_core_type, type_index)) { return; }
                        auto const expected{target_types.begin[type_index]};
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin; // Diagnostic borrow, safe after the bounded decode above.
                        err.err_selectable.br_value_type_mismatch = {.op_code_name = op_name,
                            .expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected),
                            .actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual.type)};
                        err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }};
                    if(branch_non_null && reference.from_stack)
                    {
                        // br_on_non_null narrows the branch value before matching its target type.
                        auto narrowed{operand_stack_storage_t<Fs...>{.type = reference.type,
                            .is_unknown = reference.is_unknown, .is_reference_bottom = reference.is_reference_bottom,
                            .exact_function_type_index = reference.exact_function_type_index,
                            .core_type = ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(reference),
                            .has_core_type = true}};
                        narrowed.core_type.nullable = false;
                        require_match(narrowed, target_arity - 1uz);
                    }
                    auto const concrete_count{concrete_operand_count() < prefix_arity ? concrete_operand_count() : prefix_arity};
                    for(::std::size_t i{}; i != concrete_count; ++i)
                    { require_match(operand_stack.index_unchecked(operand_stack.size() - 1uz - i), prefix_arity - 1uz - i); }
                    // The branch prefix has its declared label types on every fallthrough, even when the
                    // consumed values had more precise reference types. Reuse the rich signature helper
                    // so typed heap witnesses remain attached to each reified label operand.
                    pop_available_concrete_operands(concrete_count);
                    push_value_types(target_types, target_frame.signature_type_index,
                        target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
                        target_frame.singleton_result_core_type);
                    if(branch_non_null)
                    {
                        // The branch-only reference is the last declared label type, not a fallthrough
                        // value. The earlier target_arity > 0 check proves this pushed entry exists.
                        operand_stack.pop_back_unchecked();
                    }
                    if(!branch_non_null)
                    {
                        // The fallthrough value is reference-only bottom after a polymorphic pop; it never matches numbers.
                        operand_stack.push_back({.type = carrier});
                        operand_stack.back_unchecked().is_reference_bottom = bottom;
                        namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
                        operand_stack.back_unchecked().core_type = bottom ?
                            t3::core_value_type{t3::value_kind::reference, {t3::heap_type::bottom_code}, false} :
                            ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(reference);
                        operand_stack.back_unchecked().core_type.nullable = false;
                        operand_stack.back_unchecked().has_core_type = true;
                    }

                    break;
                }

                case static_cast<wasm_byte>(0xd4u):
                {
                    // [ref.as_non_null] next opcode ... (code_end)
                    // [safe           ] unsafe (could be code_end)
                    // ^^ code_curr / op_begin; the outer dispatch proved this opcode exists.
                    auto const op_begin{code_curr};
                    ++code_curr;
                    // [ref.as_non_null] next opcode ... (code_end)
                    // [safe           ] unsafe (could be code_end)
                    //                   ^^ code_curr; no immediate belongs to this opcode.
                    ::uwvm2::validation::standard::wasm3::require_function_references_enabled(
                        !wasm1p1_para.disable_function_references, 0xd4u, op_begin, err);
                    namespace v3 = ::uwvm2::validation::standard::wasm3;
                    decltype(try_pop_concrete_operand()) reference{};
                    auto const consume{[&]() constexpr noexcept
                    {
                        // Shared count > 0 proves this current-frame top is live; normalize its owned copy.
                        reference = try_pop_concrete_operand();
                        return v3::core3_operand{v3::core3_operand_effective_type(reference), !reference.from_stack || reference.is_unknown};
                    }};
                    v3::core3_operand normalized{};
                    auto const pop_error{v3::pop_core3_typed_operand(is_polymorphic, concrete_operand_count, consume, normalized)};
                    if(pop_error == v3::typed_stack_error::stack_underflow) [[unlikely]]
                    { report_operand_stack_underflow(op_begin, u8"ref.as_non_null", 1uz); }
                    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type narrowed_core{};
                    if(v3::narrow_core3_non_null_reference(normalized, narrowed_core) != v3::typed_stack_error::ok) [[unlikely]]
                    {
                        // [checked opcode] ... (section_end)
                        // [safe         ] unsafe (could be section_end)
                        //                 ^^ err_curr borrows op_begin for diagnostics only.
                        err.err_curr = op_begin; // Diagnostic borrows the checked opcode; no dereference.
                        err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<wasm_byte>(reference.type);
                        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
                    auto const reference_bottom{narrowed_core.heap.code == t3::heap_type::bottom_code};
                    auto const carrier{reference_bottom ? curr_operand_stack_value_type::funcref : reference.type};
                    // Append the exact shared reference result. Heap Bot is reference-only;
                    // the legacy funcref carrier is storage accounting, not numeric polymorphism.
                    operand_stack.push_back({.type = carrier, .is_reference_bottom = reference_bottom,
                        .exact_function_type_index = reference.exact_function_type_index,
                        .core_type = narrowed_core, .has_core_type = true});

                    break;
                }

                case static_cast<wasm_byte>(wasm1p1_code::ref_func):
                {
                    // ref.func funcidx ...
                    // [  safe ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};
                    ++code_curr;

                    // ref.func funcidx ...
                    // [  safe ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    if(!feature_enabled(wasm2_feature_kind::reference_types)) [[unlikely]]
                    {
                        details::fail_feature_required(op_begin,
                                                       err,
                                                       opcode_u32(wasm1p1_code::ref_func),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func);
                    }

                    auto const func_index{
                        details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr, code_end, op_begin, err, u8"ref.func")};
                    // [ref.func][checked u32 immediate] next ... code_end
                    // [safe                           ] unsafe (could be code_end)
                    //                                   ^^ code_curr: bounded read committed all immediate bytes.
                    check_ref_func_index(op_begin, func_index);
                    // Every ref.func denotes its declared function type, never erased funcref.
                    // The imported declaration points into this module's already-validated type section.
                    auto const function_ordinal{static_cast<::std::size_t>(func_index)};
                    auto const declared_type{function_ordinal < import_func_count ?
                        importsec.importdesc.index_unchecked(0u).index_unchecked(function_ordinal)->imports.storage.function :
                        ::std::addressof(typesec.types.index_unchecked(funcsec.funcs.index_unchecked(function_ordinal - import_func_count)))};
                    ::std::size_t exact_type{(::std::numeric_limits<::std::size_t>::max)()};
                    for(::std::size_t i{}; i != typesec.types.size(); ++i)
                    { if(::std::addressof(typesec.types.index_unchecked(i)) == declared_type) { exact_type = i; break; } }
                    if(exact_type == (::std::numeric_limits<::std::size_t>::max)()) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const event{::uwvm2::validation::standard::wasm3::make_core3_ref_func_event(func_index, exact_type)};
                    operand_stack.push_back({.type = static_cast<curr_operand_stack_value_type>(
                                                 ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref),
                                             .exact_function_type_index = event.exact_function_type_index,
                                             .core_type = event.result_type,
                                             .has_core_type = true});
                    break;
                }
                case static_cast<wasm_byte>(wasm1p1_code::atomic_prefix):
                {
                    // FE subopcode reserved ... (code_end)
                    // [safe] unsafe (could be code_end)
                    // ^^ op_begin borrows the dispatch-checked opcode.
                    auto const op_begin{code_curr};
                    ++code_curr;
                    // [FE] subopcode reserved ... (code_end)
                    // [safe] unsafe (could be code_end)
                    //        ^^ code_curr: the prefix was available; the helper checks the entire immediate.
                    auto const decoded{::uwvm2::validation::standard::wasm3::read_atomic_instruction64(
                        code_curr, code_end, op_begin, !wasm1p1_para.disable_threads,
                        !wasm1p1_para.disable_multi_memory, true, all_memory_count, memory_address_type_at, err)};
                    namespace atomic = ::uwvm2::validation::standard::wasm3;
                    auto const& instruction{decoded.immediate};
                    bool const address64{decoded.address_type == atomic::storage_address_type::i64};
                    auto const address_type{address64 ? curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
                    // [FE subopcode immediate] ... (code_end)
                    // [safe                 ] unsafe (could be code_end)
                    //                         ^^ code_curr: complete bounded instruction committed.
                    if(instruction.descriptor.kind != ::uwvm2::validation::standard::wasm3::atomic_instruction_kind::fence)
                    {

                        bool const is_store{instruction.descriptor.kind == ::uwvm2::validation::standard::wasm3::atomic_instruction_kind::store};
                        bool const is_wait{instruction.descriptor.kind == ::uwvm2::validation::standard::wasm3::atomic_instruction_kind::wait32 ||
                                           instruction.descriptor.kind == ::uwvm2::validation::standard::wasm3::atomic_instruction_kind::wait64};
                        ::uwvm2::utils::container::u8string_view const name{is_wait ?
                            (instruction.descriptor.value_i64 ? ::uwvm2::utils::container::u8string_view{u8"memory.atomic.wait64"} :
                                                               ::uwvm2::utils::container::u8string_view{u8"memory.atomic.wait32"}) :
                            instruction.descriptor.kind == ::uwvm2::validation::standard::wasm3::atomic_instruction_kind::notify ?
                                ::uwvm2::utils::container::u8string_view{u8"memory.atomic.notify"} : is_store ?
                                ::uwvm2::utils::container::u8string_view{u8"atomic.store"} :
                            instruction.descriptor.kind == ::uwvm2::validation::standard::wasm3::atomic_instruction_kind::load ?
                                ::uwvm2::utils::container::u8string_view{u8"atomic.load"} : ::uwvm2::utils::container::u8string_view{u8"atomic.rmw"}};
                        auto const count{static_cast<::std::size_t>(instruction.descriptor.operand_count)};
                        decltype(try_pop_concrete_operand()) actual_atomic_operand{};
                        auto const consume_atomic_operand{[&]() constexpr noexcept
                        {
                            // The common kernel proved one owned operand exists above this frame's
                            // base before this single pop; no source cursor or guest pointer moves.
                            actual_atomic_operand = try_pop_concrete_operand();
                            return atomic::core3_operand{atomic::core3_operand_effective_type(actual_atomic_operand),
                                !actual_atomic_operand.from_stack || actual_atomic_operand.is_unknown};
                        }};
                        auto const stack_failure{atomic::validate_atomic_operand_sequence(
                            decoded, is_polymorphic, concrete_operand_count, consume_atomic_operand)};
                        if(stack_failure.error == atomic::typed_stack_error::stack_underflow) [[unlikely]]
                        { report_operand_stack_underflow(op_begin, name, count); }
                        if(stack_failure.error != atomic::typed_stack_error::ok) [[unlikely]]
                        {
                            // [FE] checked subopcode/immediates ... | code_end
                            // [safe dispatch-checked opcode       ] | one-past is never dereferenced
                            // ^^ op_begin -> err.err_curr: borrow the original opcode only for diagnostics.
                            err.err_curr = op_begin;
                            auto const expected_core{atomic::atomic_expected_operand_type(decoded, stack_failure.failed_pop_index)};
                            auto const expected{expected_core.kind == ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::i64 ?
                                curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
                            if(stack_failure.failed_pop_index + 1u == instruction.descriptor.operand_count)
                            {
                                if(address64)
                                {
                                    err.err_selectable.numeric_operand_type_mismatch = {.op_code_name = name,
                                        .expected_type = to_wasm1_value_type(address_type), .actual_type = to_wasm1_value_type(actual_atomic_operand.type)};
                                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                }
                                else
                                {
                                    err.err_selectable.memarg_address_type_not_i32.op_code_name = name;
                                    err.err_selectable.memarg_address_type_not_i32.addr_type = to_wasm1_value_type(actual_atomic_operand.type);
                                    err.err_code = code_validation_error_code::memarg_address_type_not_i32;
                                }
                            }
                            else
                            {
                                err.err_selectable.store_value_type_mismatch.op_code_name = name;
                                err.err_selectable.store_value_type_mismatch.expected_type = to_wasm1_value_type(expected);
                                err.err_selectable.store_value_type_mismatch.actual_type = to_wasm1_value_type(actual_atomic_operand.type);
                                err.err_code = code_validation_error_code::store_value_type_mismatch;
                            }
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                        if(!is_store) { operand_stack.push_back({instruction.descriptor.result_i64 ? curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32}); }
                    }
                    break;
                }
                case static_cast<wasm_byte>(0xfbu):
                {
                    // Core 3 binary/instructions: 0xfb followed by a bounded u32 subopcode.
                    // [0xfb] subopcode/immediates ... code_end
                    // [safe ] unsafe (could be code_end)
                    // ^^ code_curr: the outer loop proved this prefix byte exists.
                    auto const op_begin{code_curr};
                    ++code_curr;
                    // [0xfb] subopcode/immediates ... code_end
                    // [safe ] unsafe (could be code_end)
                    //        ^^ code_curr: advancing the checked prefix may produce code_end.
                    if(wasm1p1_para.disable_gc) [[unlikely]]
                    {
                        details::fail_feature_required(op_begin, err, 0xfbu,
                            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
                            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                    }
                    auto const instruction{::uwvm2::validation::standard::wasm3::scan_gc_instruction(code_curr, code_end)};
                    // [0xfb][checked subopcode/immediates] next ... code_end on success.
                    // [safe                             ] unsafe (could be code_end)
                    //                                    ^^ code_curr: the scanner commits only bounded bytes.
                    if(instruction.error != ::uwvm2::validation::standard::wasm3::gc_immediate_error::ok) [[unlikely]]
                    { details::fail_invalid_immediate(op_begin, err, u8"gc"); }
                    // Apply the same immediate policy before any stack mutation in all modes.
                    require_gc_cast_exception_policy(!wasm1p1_para.disable_exceptions,
                        instruction.opcode, instruction.from, instruction.to, op_begin, err);
                    namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
                    namespace v3 = ::uwvm2::validation::standard::wasm3;
                    struct gc_stack_adapter
                    {
                        curr_operand_stack_type& values;
                        decltype(concrete_operand_count) const& count;
                        decltype(try_pop_concrete_operand) const& consume;
                        bool const& polymorphic;
                        [[nodiscard]] inline bool pop(v3::core3_operand& output) noexcept
                        {
                            if(count() == 0uz)
                            {
                                if(!polymorphic) { return false; }
                                output = {{}, true}; return true;
                            }
                            auto const operand{consume()};
                            output = {v3::core3_operand_effective_type(operand), operand.is_unknown};
                            return true;
                        }
                        [[nodiscard]] inline v3::core3_reference_error pop_expected(
                            t3::core_value_type expected, v3::recursive_type_context const& context) noexcept
                        {
                            auto const owned_consume{[&]() noexcept
                            {
                                // Shared kernel count > 0 proves the actual adapter top is live;
                                // copy its normalized rich type before retiring the stack entry.
                                auto const operand{consume()};
                                return v3::core3_operand{v3::core3_operand_effective_type(operand), operand.is_unknown};
                            }};
                            auto const matches{[&](auto actual, auto wanted) noexcept { return context.matches(actual, wanted); }};
                            auto const error{v3::core3_reference_error_from_typed_stack(
                                v3::pop_core3_expected_operand(polymorphic, count, owned_consume, expected, matches))};

                            return error;
                        }
                        [[nodiscard]] inline v3::core3_reference_error pop_repeated(
                            t3::core_value_type expected, ::std::uint_least32_t requested, v3::recursive_type_context const& context) noexcept
                        {
                            auto const owned_consume{[&]() noexcept
                            {
                                // Bounded shared repetition proves each top exists above the actual frame.
                                auto const operand{consume()};
                                return v3::core3_operand{v3::core3_operand_effective_type(operand), operand.is_unknown};
                            }};
                            auto const matches{[&](auto actual, auto wanted) noexcept { return context.matches(actual, wanted); }};
                            auto const error{v3::core3_reference_error_from_typed_stack(
                                v3::pop_core3_repeated_operands(polymorphic, count, owned_consume, expected, requested, matches))};

                            return error;
                        }
                        inline void push(t3::core_value_type value) noexcept
                        {
                            unsigned carrier{};
                            switch(value.kind)
                            {
                                case t3::value_kind::i32: carrier = 0x7fu; break;
                                case t3::value_kind::i64: carrier = 0x7eu; break;
                                case t3::value_kind::f32: carrier = 0x7du; break;
                                case t3::value_kind::f64: carrier = 0x7cu; break;
                                case t3::value_kind::v128: carrier = 0x7bu; break;
                                case t3::value_kind::reference:
                                    carrier = value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::extern_) ||
                                        value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noextern) ? 0x6fu :
                                        value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::exn) ||
                                        value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noexn) ? 0x69u : 0x70u;
                                    break;
                            }
                            values.push_back({.type = static_cast<curr_operand_stack_value_type>(carrier),
                                .core_type = value, .has_core_type = true});
                        }
                    };
                    gc_stack_adapter stack{operand_stack, concrete_operand_count,
                        try_pop_concrete_operand, is_polymorphic};
                    if(instruction.opcode <= 19u)
                    {
                        if(!gc_environment_ready)
                        {
                            gc_definitions.reserve(static_cast<::std::size_t>(typesec.core3_recursive_types.type_count));
                            for(auto const& group : typesec.core3_recursive_types.groups)
                            {
                                for(auto const& definition : group.types)
                                {
                                    // The parser owns this live subtype throughout synchronous function validation.
                                    // No source cursor or pointer arithmetic is involved in this borrow.
                                    gc_definitions.push_back_unchecked(::std::addressof(definition));
                                }
                            }
                            gc_element_types.reserve(elemsec.elems.size());
                            for(auto const& element : elemsec.elems)
                            {
                                auto const& payload{element.storage.segment};
                                gc_element_types.push_back_unchecked(payload.has_core_type ? payload.core_type :
                                    v3::core3_legacy_carrier_type(payload.reftype));
                            }
                            gc_environment_ready = true;
                        }
                        auto const definitions{::std::span<t3::sub_type const* const>{
                            gc_definitions.cbegin(), gc_definitions.size()}};
                        auto const elements{::std::span<t3::core_value_type const>{
                            gc_element_types.cbegin(), gc_element_types.size()}};
                        v3::core3_gc_environment const environment{definitions, elements,
                            datacountsec.present ? datacountsec.count : 0u};
                        auto const failure{v3::validate_core3_gc_instruction(stack, instruction.opcode,
                            instruction.first, instruction.second, environment, typesec.core3_context, true)};
                        if(failure != v3::core3_reference_error::ok) [[unlikely]]
                        {
                            if(failure == v3::core3_reference_error::stack_underflow)
                            { report_operand_stack_underflow(op_begin, u8"gc", 1uz); }
                            auto name{::uwvm2::utils::container::u8string_view{u8"gc operand/type"}};
                            if(failure == v3::core3_reference_error::unknown_type)
                            { name = ::uwvm2::utils::container::u8string_view{u8"gc type index"}; }
                            else if(failure == v3::core3_reference_error::unknown_field)
                            { name = ::uwvm2::utils::container::u8string_view{u8"gc field index"}; }
                            else if(failure == v3::core3_reference_error::immutable_field)
                            { name = ::uwvm2::utils::container::u8string_view{u8"immutable gc field"}; }
                            else if(failure == v3::core3_reference_error::packed_access_mismatch)
                            { name = ::uwvm2::utils::container::u8string_view{u8"packed gc field access"}; }
                            details::fail_invalid_immediate(op_begin, err, name);
                        }
                        break;
                    }
                    if(instruction.opcode >= 20u && instruction.opcode <= 23u)
                    {
                        auto const failure{v3::validate_core3_ref_cast(stack, instruction.to,
                            typesec.core3_context, instruction.opcode <= 21u, true)};
                        if(failure == v3::core3_reference_error::stack_underflow)
                        { report_operand_stack_underflow(op_begin, u8"ref.test/ref.cast", 1uz); }
                        if(failure != v3::core3_reference_error::ok) [[unlikely]]
                        { details::fail_invalid_immediate(op_begin, err, u8"ref.test/ref.cast type"); }
                        break;
                    }
                    if(instruction.opcode == 24u || instruction.opcode == 25u)
                    {
                        auto const branch_on_failure{instruction.opcode == 25u};
                        auto const op_name{branch_on_failure ?
                            ::uwvm2::utils::container::u8string_view{u8"br_on_cast_fail"} :
                            ::uwvm2::utils::container::u8string_view{u8"br_on_cast"}};
                        if(!v3::reference_cast_details::valid(instruction.from, typesec.core3_context) ||
                           !v3::reference_cast_details::valid(instruction.to, typesec.core3_context) ||
                           !typesec.core3_context.matches(instruction.to, instruction.from)) [[unlikely]]
                        { details::fail_invalid_immediate(op_begin, err, u8"br_on_cast type"); }
                        auto const label_count{control_flow_stack.size()};
                        if(static_cast<::std::uint_least64_t>(instruction.first) >= label_count) [[unlikely]]
                        {
                            // op_begin is the dispatch-checked 0xfb byte; this assignment only borrows it.
                            // [0xfb][checked cast immediates] ... code_end
                            // [safe                        ] unsafe (possibly code_end)
                            // ^^ err_curr / op_begin: diagnostic address, never dereferenced here.
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_label_index = {.label_index = instruction.first,
                                .all_label_count = static_cast<wasm_u32>(label_count)};
                            err.err_code = code_validation_error_code::illegal_label_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                        // instruction.first < label_count proves this reverse control-frame index.
                        auto const& target_frame{control_flow_stack.index_unchecked(
                            label_count - 1uz - static_cast<::std::size_t>(instruction.first))};
                        auto const label_types{target_frame.label};
                        // Both endpoints belong to one parser-owned label signature. Do not subtract null
                        // endpoints: the empty signature is represented by two equal null pointers.
                        auto const target_arity{label_types.begin == label_types.end ? 0uz :
                            static_cast<::std::size_t>(label_types.end - label_types.begin)};
                        if(target_arity == 0uz) [[unlikely]]
                        { details::fail_invalid_immediate(op_begin, err, u8"br_on_cast label"); }
                        auto const prefix_arity{target_arity - 1uz};
                        auto const last{block_core_type_at(label_types, target_frame.signature_type_index,
                            target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
                            target_frame.singleton_result_core_type, prefix_arity)};
                        // [label_types.begin, label_types.end) contains target_arity entries.
                        // [safe                            ] unsafe (end)
                        //                        ^^ begin[prefix_arity], since prefix_arity < target_arity.
                        auto const label_last{last.has_type ? last.type :
                            v3::core3_legacy_carrier_type(label_types.begin[prefix_arity])};
                        auto difference{instruction.from};
                        if(instruction.to.nullable) { difference.nullable = false; }
                        auto const branch_value{branch_on_failure ? difference : instruction.to};
                        if(label_last.kind != t3::value_kind::reference ||
                           !typesec.core3_context.matches(branch_value, label_last)) [[unlikely]]
                        { details::fail_invalid_immediate(op_begin, err, u8"br_on_cast label type"); }
                        if(!is_polymorphic && concrete_operand_count() < target_arity) [[unlikely]]
                        { report_operand_stack_underflow(op_begin, op_name, target_arity); }
                        auto const reference{try_pop_concrete_operand()};
                        if(reference.from_stack && !reference.is_unknown &&
                           !typesec.core3_context.matches(v3::core3_operand_effective_type(reference), instruction.from)) [[unlikely]]
                        { details::fail_invalid_immediate(op_begin, err, u8"br_on_cast source type"); }
                        auto const concrete_prefix{concrete_operand_count() < prefix_arity ?
                            concrete_operand_count() : prefix_arity};
                        for(::std::size_t i{}; i != concrete_prefix; ++i)
                        {
                            // i < concrete_prefix <= concrete_operand_count() proves this top-relative read.
                            auto const actual{operand_stack.index_unchecked(operand_stack.size() - 1uz - i)};
                            if(!block_value_matches(actual, label_types, target_frame.signature_type_index,
                                target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
                                target_frame.singleton_result_core_type, prefix_arity - 1uz - i)) [[unlikely]]
                            { details::fail_invalid_immediate(op_begin, err, u8"br_on_cast prefix type"); }
                        }
                        // The label prefix has the label's declared types on fallthrough, including
                        // reachable code. Keeping an actual subtype here would let a later instruction
                        // consume a precision that br_on_cast's validation rule has discarded.
                        // concrete_prefix <= the available frame operands; the pop stays above its base.
                        pop_available_concrete_operands(concrete_prefix);
                        push_value_types(label_types, target_frame.signature_type_index,
                            target_frame.type != block_type::loop,
                            target_frame.has_singleton_result_core_type,
                            target_frame.singleton_result_core_type);
                        // target_arity > 0 proves the pushed last entry exists. Remove that branch-only
                        // value; the fallthrough keeps exactly the reified prefix and cast-refined ref.
                        // No source byte cursor advances in this stack-only rewrite.
                        operand_stack.pop_back_unchecked();
                        stack.push(branch_on_failure ? instruction.to : difference);
                        break;
                    }
                    if(instruction.opcode == 26u || instruction.opcode == 27u)
                    {
                        auto const failure{v3::validate_core3_convert_reference(stack,
                            typesec.core3_context, instruction.opcode == 26u, true)};
                        if(failure == v3::core3_reference_error::stack_underflow)
                        { report_operand_stack_underflow(op_begin, u8"reference conversion", 1uz); }
                        if(failure != v3::core3_reference_error::ok) [[unlikely]]
                        { details::fail_invalid_immediate(op_begin, err, u8"reference conversion type"); }
                        break;
                    }
                    if(instruction.opcode < 28u || instruction.opcode > 30u) [[unlikely]]
                    { details::fail_invalid_immediate(op_begin, err, u8"gc"); }

                    auto const op_name{instruction.opcode == 28u ?
                        ::uwvm2::utils::container::u8string_view{u8"ref.i31"} :
                        instruction.opcode == 29u ? ::uwvm2::utils::container::u8string_view{u8"i31.get_s"} :
                                                   ::uwvm2::utils::container::u8string_view{u8"i31.get_u"}};
                    decltype(try_pop_concrete_operand()) input{};
                    auto const consume{[&]() constexpr noexcept
                    {
                        // Shared count > 0 proves a live top above the actual frame;
                        // normalize its rich type before retiring the owned entry.
                        input = try_pop_concrete_operand();
                        return v3::core3_operand{v3::core3_operand_effective_type(input), !input.from_stack || input.is_unknown};
                    }};
                    auto const matches{[&](auto actual, auto expected) constexpr noexcept
                    { return v3::core3_value_type_matches_in_section(actual, expected, typesec); }};
                    auto const transition{v3::apply_core3_i31_typed_transition(
                        instruction.opcode, is_polymorphic, concrete_operand_count, consume, matches)};
                    if(!transition.supported) [[unlikely]] { details::fail_invalid_immediate(op_begin, err, u8"gc i31"); }
                    if(transition.error == v3::typed_stack_error::stack_underflow) [[unlikely]]
                    { report_operand_stack_underflow(op_begin, op_name, 1uz); }
                    auto const expected_carrier{instruction.opcode == 28u ? curr_operand_stack_value_type::i32 :
                        static_cast<curr_operand_stack_value_type>(0x70u)};
                    if(transition.error != v3::typed_stack_error::ok) [[unlikely]]
                    {
                        // [0xfb][checked instruction] ... code_end
                        // [safe                       ] unsafe (possibly code_end)
                        // ^^ op_begin / err_curr: copied checked prefix; no cursor moves.
                        err.err_curr = op_begin;
                        err.err_selectable.br_value_type_mismatch = {.op_code_name = op_name,
                            .expected_type = to_wasm1_value_type(expected_carrier),
                            .actual_type = to_wasm1_value_type(input.type)};
                        err.err_code = code_validation_error_code::br_value_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    if(instruction.opcode == 28u)
                    {
                        operand_stack.push_back({.type = static_cast<curr_operand_stack_value_type>(0x70u),
                            .core_type = transition.output, .has_core_type = true});
                    }
                    else { operand_stack.push_back({.type = curr_operand_stack_value_type::i32}); }
                    break;
                }
                case static_cast<wasm_byte>(wasm1p1_code::numeric_prefix):
                {
                    // numeric_prefix subopcode ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};
                    ++code_curr;

                    // numeric_prefix subopcode ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    auto const subopcode{
                        details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr, code_end, op_begin, err, u8"numeric_prefix")};
                    auto const numeric_code{static_cast<wasm1p1_numeric_code>(subopcode)};

                    switch(numeric_code)
                    {
                        case wasm1p1_numeric_code::i32_trunc_sat_f32_s:
                        {
                            if(!feature_enabled(wasm2_feature_kind::nontrapping_float_to_int)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin,
                                                                u8"i32.trunc_sat_f32_s",
                                                                curr_operand_stack_value_type::f32,
                                                                curr_operand_stack_value_type::i32);
                            break;
                        }
                        case wasm1p1_numeric_code::i32_trunc_sat_f32_u:
                        {
                            if(!feature_enabled(wasm2_feature_kind::nontrapping_float_to_int)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin,
                                                                u8"i32.trunc_sat_f32_u",
                                                                curr_operand_stack_value_type::f32,
                                                                curr_operand_stack_value_type::i32);
                            break;
                        }
                        case wasm1p1_numeric_code::i32_trunc_sat_f64_s:
                        {
                            if(!feature_enabled(wasm2_feature_kind::nontrapping_float_to_int)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin,
                                                                u8"i32.trunc_sat_f64_s",
                                                                curr_operand_stack_value_type::f64,
                                                                curr_operand_stack_value_type::i32);
                            break;
                        }
                        case wasm1p1_numeric_code::i32_trunc_sat_f64_u:
                        {
                            if(!feature_enabled(wasm2_feature_kind::nontrapping_float_to_int)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin,
                                                                u8"i32.trunc_sat_f64_u",
                                                                curr_operand_stack_value_type::f64,
                                                                curr_operand_stack_value_type::i32);
                            break;
                        }
                        case wasm1p1_numeric_code::i64_trunc_sat_f32_s:
                        {
                            if(!feature_enabled(wasm2_feature_kind::nontrapping_float_to_int)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin,
                                                                u8"i64.trunc_sat_f32_s",
                                                                curr_operand_stack_value_type::f32,
                                                                curr_operand_stack_value_type::i64);
                            break;
                        }
                        case wasm1p1_numeric_code::i64_trunc_sat_f32_u:
                        {
                            if(!feature_enabled(wasm2_feature_kind::nontrapping_float_to_int)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin,
                                                                u8"i64.trunc_sat_f32_u",
                                                                curr_operand_stack_value_type::f32,
                                                                curr_operand_stack_value_type::i64);
                            break;
                        }
                        case wasm1p1_numeric_code::i64_trunc_sat_f64_s:
                        {
                            if(!feature_enabled(wasm2_feature_kind::nontrapping_float_to_int)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin,
                                                                u8"i64.trunc_sat_f64_s",
                                                                curr_operand_stack_value_type::f64,
                                                                curr_operand_stack_value_type::i64);
                            break;
                        }
                        case wasm1p1_numeric_code::i64_trunc_sat_f64_u:
                        {
                            if(!feature_enabled(wasm2_feature_kind::nontrapping_float_to_int)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            validate_numeric_unary_stack_effect(op_begin,
                                                                u8"i64.trunc_sat_f64_u",
                                                                curr_operand_stack_value_type::f64,
                                                                curr_operand_stack_value_type::i64);
                            break;
                        }
                        case wasm1p1_numeric_code::memory_init:
                        {
                            if(!feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment);
                            }
                            auto const data_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr,
                                                                                                                               code_end,
                                                                                                                               op_begin,
                                                                                                                               err,
                                                                                                                               u8"memory.init.dataidx")};
                            check_data_index(op_begin, data_index);
                            auto const memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
                            check_memory_index(op_begin, memory_index, u8"memory.init");
                            ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const decoded_bulk{
                                .kind = ::uwvm2::validation::standard::wasm3::bulk_memory_instruction_kind::init,
                                .data_index = data_index, .destination_memory_index = memory_index,
                                .destination_address = memory_address_type_at(memory_index)};
                            validate_checked_bulk_memory(op_begin, u8"memory.init", decoded_bulk);
                            break;
                        }
                        case wasm1p1_numeric_code::data_drop:
                        {
                            if(!feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment);
                            }
                            auto const data_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr,
                                                                                                                               code_end,
                                                                                                                               op_begin,
                                                                                                                               err,
                                                                                                                               u8"data.drop")};
                            check_data_index(op_begin, data_index);
                            ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const decoded_bulk{
                                .kind = ::uwvm2::validation::standard::wasm3::bulk_memory_instruction_kind::data_drop, .data_index = data_index};
                            validate_checked_bulk_memory(op_begin, u8"data.drop", decoded_bulk);
                            break;
                        }
                        case wasm1p1_numeric_code::memory_copy:
                        {
                            if(!feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            auto const dst_memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
                            check_memory_index(op_begin, dst_memory_index, u8"memory.copy");
                            auto const src_memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
                            check_memory_index(op_begin, src_memory_index, u8"memory.copy");
                            ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const decoded_bulk{
                                .kind = ::uwvm2::validation::standard::wasm3::bulk_memory_instruction_kind::copy,
                                .destination_memory_index = dst_memory_index, .source_memory_index = src_memory_index,
                                .destination_address = memory_address_type_at(dst_memory_index), .source_address = memory_address_type_at(src_memory_index)};
                            validate_checked_bulk_memory(op_begin, u8"memory.copy", decoded_bulk);
                            break;
                        }
                        case wasm1p1_numeric_code::memory_fill:
                        {
                            if(!feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            auto const memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
                            check_memory_index(op_begin, memory_index, u8"memory.fill");
                            ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const decoded_bulk{
                                .kind = ::uwvm2::validation::standard::wasm3::bulk_memory_instruction_kind::fill,
                                .destination_memory_index = memory_index, .destination_address = memory_address_type_at(memory_index)};
                            validate_checked_bulk_memory(op_begin, u8"memory.fill", decoded_bulk);
                            break;
                        }
                        case wasm1p1_numeric_code::table_init:
                        {
                            if(!feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment);
                            }
                            auto const element_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr,
                                                                                                                                  code_end,
                                                                                                                                  op_begin,
                                                                                                                                  err,
                                                                                                                                  u8"table.init.elemidx")};
                            check_element_index(op_begin, element_index);
                            auto const table_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr,
                                                                                                                                code_end,
                                                                                                                                op_begin,
                                                                                                                                err,
                                                                                                                                u8"table.init.tableidx")};
                            check_table_index(op_begin, table_index, subopcode);

                            // [elemsec.elems begin ... element_index ... end) is parser-owned through validation.
                            // [safe                                          ] check_element_index proved this borrow.
                            //                 ^^ element refers to one declaration; code_curr does not advance here.
                            auto const& element{elemsec.elems.index_unchecked(element_index).storage.segment};
                            auto const element_value_type{static_cast<curr_operand_stack_value_type>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(element.reftype))};
                            auto const table_value_type{get_table_value_type(table_index)};
                            auto const element_core{element.has_core_type ? element.core_type :
                                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(element_value_type)};
                            auto const matches{::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                element_core, get_table_core_type(table_index), typesec)};
                            if(!matches) [[unlikely]]
                            {
                                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.init";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_value_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(element_value_type);
                                err.err_code = code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            // Core 3 table.init retains i32 source/length; only destination
                            // follows the selected table's address type.
                            validate_i32_operands(op_begin, u8"table.init", 2uz);
                            validate_storage_operand(op_begin, u8"table.init", table_operand_type(table_index));
                            break;
                        }
                        case wasm1p1_numeric_code::elem_drop:
                        {
                            if(!feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment);
                            }
                            auto const element_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr,
                                                                                                                                  code_end,
                                                                                                                                  op_begin,
                                                                                                                                  err,
                                                                                                                                  u8"elem.drop")};
                            check_element_index(op_begin, element_index);
                            break;
                        }
                        case wasm1p1_numeric_code::table_copy:
                        {
                            if(!feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                            {
                                details::fail_feature_required(op_begin,
                                                               err,
                                                               subopcode,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            auto const dst_table_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr,
                                                                                                                                    code_end,
                                                                                                                                    op_begin,
                                                                                                                                    err,
                                                                                                                                    u8"table.copy.dst")};
                            check_table_index(op_begin, dst_table_index, subopcode);
                            auto const src_table_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr,
                                                                                                                                    code_end,
                                                                                                                                    op_begin,
                                                                                                                                    err,
                                                                                                                                    u8"table.copy.src")};
                            check_table_index(op_begin, src_table_index, subopcode);

                            auto const dst_type{get_table_value_type(dst_table_index)};
                            auto const src_type{get_table_value_type(src_table_index)};
                            auto const copy_matches{rich_signatures_available ?
                                ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                    get_table_core_type(src_table_index), get_table_core_type(dst_table_index),
                                    typesec) : dst_type == src_type};
                            if(!copy_matches) [[unlikely]]
                            {
                                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.copy";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(dst_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(src_type);
                                err.err_code = code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            auto const length_type{copy_length_address_type(table_address_type_at(dst_table_index), table_address_type_at(src_table_index))};
                            validate_storage_operand(op_begin, u8"table.copy", length_type == storage_address_type::i64 ?
                                curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32);
                            validate_storage_operand(op_begin, u8"table.copy", table_operand_type(src_table_index));
                            validate_storage_operand(op_begin, u8"table.copy", table_operand_type(dst_table_index));
                            break;
                        }
                        case wasm1p1_numeric_code::table_grow:
                        {
                            if(!feature_enabled(wasm2_feature_kind::table_instructions)) [[unlikely]]
                            {
                                details::fail_wasm2_feature_required(op_begin,
                                                                     err,
                                                                     subopcode,
                                                                     ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                                                     ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
                            }
                            auto const table_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr,
                                                                                                                                code_end,
                                                                                                                                op_begin,
                                                                                                                                err,
                                                                                                                                u8"table.grow")};
                            check_table_index(op_begin, table_index, subopcode);
                            auto const table_type{get_table_value_type(table_index)};

                            if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]]
                            {
                                report_operand_stack_underflow(op_begin, u8"table.grow", 2uz);
                            }

                            auto const delta{try_pop_concrete_operand()};
                                if(!operand_type_matches(delta, table_operand_type(table_index))) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin;
                                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.grow";
                                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(table_operand_type(table_index));
                                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(delta.type);
                                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            auto const value{try_pop_concrete_operand()};
                                auto const grow_matches{rich_signatures_available && value.from_stack && !value.is_unknown ?
                                    ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                        ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(value),
                                        get_table_core_type(table_index), typesec) :
                                    operand_type_matches(value, table_type)};
                                if(!grow_matches) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin;
                                    err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.grow";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                                err.err_code = code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            operand_stack.push_back({table_operand_type(table_index)});
                            break;
                        }
                        case wasm1p1_numeric_code::table_size:
                        {
                            if(!feature_enabled(wasm2_feature_kind::table_instructions)) [[unlikely]]
                            {
                                details::fail_wasm2_feature_required(op_begin,
                                                                     err,
                                                                     subopcode,
                                                                     ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                                                     ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
                            }
                            auto const table_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr,
                                                                                                                                code_end,
                                                                                                                                op_begin,
                                                                                                                                err,
                                                                                                                                u8"table.size")};
                            check_table_index(op_begin, table_index, subopcode);
                            operand_stack.push_back({table_operand_type(table_index)});
                            break;
                            }
                            case wasm1p1_numeric_code::table_fill:
                            {
                                if(!feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                                {
                                    details::fail_feature_required(op_begin,
                                                                   err,
                                                                   subopcode,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                                }
                            auto const table_index{details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr,
                                                                                                                                code_end,
                                                                                                                                op_begin,
                                                                                                                                err,
                                                                                                                                u8"table.fill")};
                            check_table_index(op_begin, table_index, subopcode);
                            auto const table_type{get_table_value_type(table_index)};

                            if(!is_polymorphic && concrete_operand_count() < 3uz) [[unlikely]]
                            {
                                report_operand_stack_underflow(op_begin, u8"table.fill", 3uz);
                            }

                            auto const len{try_pop_concrete_operand()};
                                if(!operand_type_matches(len, table_operand_type(table_index))) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin;
                                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.fill";
                                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(table_operand_type(table_index));
                                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(len.type);
                                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            auto const value{try_pop_concrete_operand()};
                                auto const fill_matches{rich_signatures_available && value.from_stack && !value.is_unknown ?
                                    ::uwvm2::validation::standard::wasm3::core3_value_type_matches_in_section(
                                        ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(value),
                                        get_table_core_type(table_index), typesec) :
                                    operand_type_matches(value, table_type)};
                                if(!fill_matches) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin;
                                    err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.fill";
                                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                                err.err_code = code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            auto const index{try_pop_concrete_operand()};
                                if(!operand_type_matches(index, table_operand_type(table_index))) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin;
                                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.fill";
                                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(table_operand_type(table_index));
                                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(index.type);
                                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            break;
                        }
                        [[unlikely]] default:
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(subopcode);
                            err.err_code = code_validation_error_code::illegal_opbase;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    break;
                }
                case static_cast<wasm_byte>(wasm1p1_code::simd_prefix):
                {
                    // simd_prefix subopcode ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};
                    ++code_curr;

                    // simd_prefix subopcode ...
                    // [safe] unsafe (could be the section_end)
                    //        ^^ code_curr

                    if(!feature_enabled(wasm2_feature_kind::simd)) [[unlikely]]
                    {
                        details::fail_feature_required(op_begin,
                                                       err,
                                                       opcode_u32(wasm1p1_code::simd_prefix),
                                                       ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd,
                                                       ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const);
                    }

                    auto const simd_subopcode{
                        details::read_leb128<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(code_curr, code_end, op_begin, err, u8"simd")};
                    // 0xfd subopcode ...
                    // [safe         ] unsafe (could be code_end)
                    //                 ^^ code_curr: bounded read_leb128 consumed the complete u32.
                    if(::uwvm2::validation::standard::wasm3::relaxed_simd_operand_count(simd_subopcode) != 0u && wasm1p1_para.disable_relaxed_simd)
                    {
                        details::fail_feature_required(op_begin, err, simd_subopcode,
                            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::relaxed_simd,
                            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                    }
                    auto const simd_code{static_cast<wasm1p1_simd_code>(
                        ::uwvm2::validation::standard::wasm3::relaxed_simd_canonical_opcode(simd_subopcode))};

                    auto const validate_simd_memarg{
                        [&](::uwvm2::utils::container::u8string_view op_name,
                            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 max_align) constexpr UWVM_THROWS
                        {
                            // The bounded scanner checks flags, optional memory index and offset before committing code_curr.
                            auto const memarg{::uwvm2::validation::standard::wasm3::read_memory_argument64(
                                code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, true, all_memory_count, memory_address_type_at, max_align, op_name, err)};
                            // op_name [validated memarg] ...
                            // [safe                   ] unsafe (could be code_end)
                            //                           ^^ code_curr
                            return memory_operand_type(memarg.immediate.memory_index);
                        }};

                    auto const check_lane_index{[&](::uwvm2::utils::container::u8string_view op_name,
                                                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte lane,
                                                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte lane_count) constexpr UWVM_THROWS
                                                {
                                                    if(lane >= lane_count) [[unlikely]] { details::fail_invalid_immediate(op_begin, err, op_name); }
                                                }};

                    auto const simd_v128_type{static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)};
                    curr_operand_stack_value_type v128_v128_v128_operands[3]{simd_v128_type, simd_v128_type, simd_v128_type};

                    auto const push_simd_v128{[&]() constexpr { operand_stack.push_back({simd_v128_type}); }};

                    auto const validate_simd_unary_v128{[&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                                                        {
                                                            pop_expected_operands(op_begin, op_name, {v128_result_arr, v128_result_arr + 1u});
                                                            push_simd_v128();
                                                        }};

                    auto const validate_simd_binary_v128{[&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                                                         {
                                                             pop_expected_operands(op_begin, op_name, {v128_v128_operands, v128_v128_operands + 2u});
                                                             push_simd_v128();
                                                         }};

                    auto const validate_simd_ternary_v128{[&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                                                          {
                                                              pop_expected_operands(op_begin, op_name, {v128_v128_v128_operands, v128_v128_v128_operands + 3u});
                                                              push_simd_v128();
                                                          }};

                    auto const validate_simd_test_v128{[&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                                                       {
                                                           pop_expected_operands(op_begin, op_name, {v128_result_arr, v128_result_arr + 1u});
                                                           operand_stack.push_back({curr_operand_stack_value_type::i32});
                                                       }};

                    auto const validate_simd_shift_v128{[&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
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
                            auto const address_type{validate_simd_memarg(u8"v128.load", 4u)};
                            validate_storage_operand(op_begin, u8"v128.load", address_type);
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load8x8_s: [[fallthrough]];
                        case wasm1p1_simd_code::v128_load8x8_u:
                        {
                            auto const address_type{validate_simd_memarg(u8"v128.load8x8", 3u)};
                            validate_storage_operand(op_begin, u8"v128.load8x8", address_type);
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load16x4_s: [[fallthrough]];
                        case wasm1p1_simd_code::v128_load16x4_u:
                        {
                            auto const address_type{validate_simd_memarg(u8"v128.load16x4", 3u)};
                            validate_storage_operand(op_begin, u8"v128.load16x4", address_type);
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load32x2_s: [[fallthrough]];
                        case wasm1p1_simd_code::v128_load32x2_u:
                        {
                            auto const address_type{validate_simd_memarg(u8"v128.load32x2", 3u)};
                            validate_storage_operand(op_begin, u8"v128.load32x2", address_type);
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load8_splat:
                        {
                            auto const address_type{validate_simd_memarg(u8"v128.load8_splat", 0u)};
                            validate_storage_operand(op_begin, u8"v128.load8_splat", address_type);
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load16_splat:
                        {
                            auto const address_type{validate_simd_memarg(u8"v128.load16_splat", 1u)};
                            validate_storage_operand(op_begin, u8"v128.load16_splat", address_type);
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load32_splat:
                        {
                            auto const address_type{validate_simd_memarg(u8"v128.load32_splat", 2u)};
                            validate_storage_operand(op_begin, u8"v128.load32_splat", address_type);
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load64_splat:
                        {
                            auto const address_type{validate_simd_memarg(u8"v128.load64_splat", 3u)};
                            validate_storage_operand(op_begin, u8"v128.load64_splat", address_type);
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_store:
                        {
                            auto const address_type{validate_simd_memarg(u8"v128.store", 4u)};
                            // [v128_result_arr: one complete expected type] end
                            // [safe                                       ]
                            // ^^ begin                                     ^^ one-past end
                            // Both borrowed pointers remain in this static array.
                            pop_expected_operands(op_begin, u8"v128.store", {v128_result_arr, v128_result_arr + 1u});
                            validate_storage_operand(op_begin, u8"v128.store", address_type);
                            break;
                        }
                        case wasm1p1_simd_code::v128_const:
                        {
                            details::skip_bytes(code_curr, code_end, op_begin, 16uz, err, u8"v128.const");
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::i8x16_shuffle:
                        {
                            for(::std::size_t lane_index{}; lane_index != 16uz; ++lane_index)
                            {
                                auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"i8x16.shuffle")};
                                if(lane >= 32u) [[unlikely]] { details::fail_invalid_immediate(op_begin, err, u8"i8x16.shuffle"); }
                            }
                            pop_expected_operands(op_begin, u8"i8x16.shuffle", {v128_v128_operands, v128_v128_operands + 2u});
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
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
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
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
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
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
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
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
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
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
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
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
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
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
                            auto const address_type{validate_simd_memarg(u8"v128.load_lane", max_align)};
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"v128.load_lane")};
                            auto const lane_count{simd_code == wasm1p1_simd_code::v128_load8_lane    ? 16u
                                                  : simd_code == wasm1p1_simd_code::v128_load16_lane ? 8u
                                                  : simd_code == wasm1p1_simd_code::v128_load32_lane ? 4u
                                                                                                     : 2u};
                            check_lane_index(u8"v128.load_lane", lane, static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(lane_count));
                            // [v128_result_arr: one complete expected type] end
                            // [safe                                       ]
                            // ^^ begin                                     ^^ one-past end
                            // Both borrowed pointers remain in this static array.
                            pop_expected_operands(op_begin, u8"v128.load_lane", {v128_result_arr, v128_result_arr + 1u});
                            validate_storage_operand(op_begin, u8"v128.load_lane", address_type);
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
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
                            auto const address_type{validate_simd_memarg(u8"v128.store_lane", max_align)};
                            auto const lane{details::read_u8(code_curr, code_end, op_begin, err, u8"v128.store_lane")};
                            auto const lane_count{simd_code == wasm1p1_simd_code::v128_store8_lane    ? 16u
                                                  : simd_code == wasm1p1_simd_code::v128_store16_lane ? 8u
                                                  : simd_code == wasm1p1_simd_code::v128_store32_lane ? 4u
                                                                                                      : 2u};
                            check_lane_index(u8"v128.store_lane", lane, static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(lane_count));
                            // [v128_result_arr: one complete expected type] end
                            // [safe                                       ]
                            // ^^ begin                                     ^^ one-past end
                            // Both borrowed pointers remain in this static array.
                            pop_expected_operands(op_begin, u8"v128.store_lane", {v128_result_arr, v128_result_arr + 1u});
                            validate_storage_operand(op_begin, u8"v128.store_lane", address_type);
                            break;
                        }
                        case wasm1p1_simd_code::v128_load32_zero:
                        {
                            auto const address_type{validate_simd_memarg(u8"v128.load32_zero", 2u)};
                            validate_storage_operand(op_begin, u8"v128.load32_zero", address_type);
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
                            break;
                        }
                        case wasm1p1_simd_code::v128_load64_zero:
                        {
                            auto const address_type{validate_simd_memarg(u8"v128.load64_zero", 3u)};
                            validate_storage_operand(op_begin, u8"v128.load64_zero", address_type);
                            operand_stack.push_back(
                                {static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)});
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
                        case wasm1p1_simd_code::i16x8_relaxed_dot_i8x16_i7x16_s: [[fallthrough]];
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
                        case wasm1p1_simd_code::f32x4_relaxed_madd: [[fallthrough]];
                        case wasm1p1_simd_code::f32x4_relaxed_nmadd: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_relaxed_madd: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_relaxed_nmadd: [[fallthrough]];
                        case wasm1p1_simd_code::i32x4_relaxed_dot_i8x16_i7x16_add_s: [[fallthrough]];
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
                        case wasm1p1_simd_code::f64x2_ceil: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_floor: [[fallthrough]];
                        case wasm1p1_simd_code::f64x2_trunc: [[fallthrough]];
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
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                    // [decoded prefix] remaining bytes ... | code_end
                    // [readable bytes]                    | one-past is not dereferenced
                    // ^^ code_curr -> err.err_curr: diagnostic copy; it may equal code_end.
                    err.err_curr = code_curr;
                    err.err_selectable.u8 = static_cast<::std::uint_least8_t>(curr_opbase);
                    err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_opbase;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    break;
                }
            }
            if constexpr(ValidatedOperationSink::retains_operations)
            {
                // [expression begin ... completely scanned instruction] | code_end
                // [safe                                                ] | one-past
                // ^^ bounded validator cursor -> owned integer offset; no cursor changes.
                validated_operations.finish_instruction(static_cast<::std::size_t>(code_curr - code_begin), operand_stack.size(),
                    operand_stack.empty() ? ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type{} :
                        operand_stack.back_unchecked().has_core_type ? operand_stack.back_unchecked().core_type :
                        core3_legacy_carrier_type(to_wasm1_value_type(operand_stack.back_unchecked().type)));
            }
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code(wasm3_code_version version,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module,
        ::std::size_t function_index, ::std::byte const* begin, ::std::byte const* end,
        ::uwvm2::validation::error::code_validation_error_impl& error,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& features) UWVM_THROWS
    {
        details::discard_validated_operations discarded{};
        validate_code_to_sink(version, module, function_index, begin, end, error, features, discarded);
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline retained_integer_function_plan::owner details::retained_integer_plan_builder::validate(
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module,
        ::std::size_t function_index,
        ::uwvm2::validation::error::code_validation_error_impl& error,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& features,
        retained_module_record_budget* module_budget, bool retain)
    {
        // Like the ordinary module validator, this API requires parser-validated
        // declaration/body metadata. It does not accept a caller-provided byte
        // span or a validation-ready bool. Guest bytes never construct that metadata.
        auto const& imports{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(module.sections)};
        constexpr ::std::size_t function_import_bucket{};
        static_assert(function_import_bucket < imports.importdesc_count);
        // [actual owned parser import buckets] end
        // ^^ bucket 0 is compile-time in range before any metadata borrow.
        auto const imported{imports.importdesc.index_unchecked(function_import_bucket).size()};
        auto const& codes{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(module.sections).codes};
        if(function_index < imported || function_index - imported >= codes.size()) { return {}; }
        auto const& body{codes.index_unchecked(function_index - imported).body};
        // [actual parsed code record][expr_begin, code_end) in its file allocation
        // [safe: caller pins this parsed module's byte owner                 ]
        // ^^ begin/end are metadata-derived copies, never arbitrary external spans.
        auto const begin{reinterpret_cast<::std::byte const*>(body.expr_begin)};
        auto const end{reinterpret_cast<::std::byte const*>(body.code_end)};
        if(begin == nullptr || end == nullptr) { return {}; }
        auto const begin_address{reinterpret_cast<::std::uintptr_t>(begin)};
        auto const end_address{reinterpret_cast<::std::uintptr_t>(end)};
        if(end_address < begin_address || end_address - begin_address >
           static_cast<::std::uintptr_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())) { return {}; }
        // The parsed-module contract proves common allocation membership; the
        // integer extent check above additionally proves representable pointer
        // differences before the sink forms source offsets in the typed loop.
        retained_module_record_budget local_budget{};
        auto& actual_budget{module_budget == nullptr ? local_budget : *module_budget};
        retained_integer_plan_builder builder{actual_budget, retain};
        validate_code_to_sink(wasm3_code_version{}, module, function_index, begin, end, error, features, builder);
        if(!builder.terminal_ || error.err_code != ::uwvm2::validation::error::code_validation_error_code::ok) { return {}; }
        builder.plan_.sealed_ = true;
        return retained_integer_function_plan::owner{new retained_integer_function_plan{::std::move(builder.plan_)}};
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code(wasm3_code_version code_version,
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
            para.disable_multi_value = false;
            para.disable_reference_types = false;
            para.disable_table_instructions = false;
            para.disable_multiple_tables = false;
            para.disable_bulk_memory = false;
            para.disable_sign_extension = false;
            para.disable_nontrapping_float_to_int = false;
            para.disable_simd = false;
            para.disable_extended_const = false;
            para.disable_table_initializer = false;
            para.disable_relaxed_simd = false;
            para.disable_multi_memory = false;
            para.disable_tail_call = false;
            para.disable_memory64 = false;
            para.disable_table64 = false;
            para.disable_function_references = false;
            para.disable_gc = false;
            para.disable_exceptions = false;
        }

        ::uwvm2::validation::standard::wasm3::validate_code(code_version, module_storage, function_index, code_begin, code_end, err, fs_para);
    }

    // Module declaration admission is separate from per-function instruction
    // validation. This reads existing parser metadata once and never walks bytes.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_module_declarations_with_runtime_policy(
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
        ::uwvm2::validation::error::code_validation_error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        auto const& policy{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        if(core3_declaration_policy_fully_enabled(policy)) { return; }
        auto const& types{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(module_storage.sections)};
        auto const& tables{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage.sections)};
        auto const& memories{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>(module_storage.sections)};
        auto const& globals{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections)};
        auto const& elements{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>>(module_storage.sections)};
        auto const& codes{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(module_storage.sections)};
        auto const& imports{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(module_storage.sections)};
        auto const& tags{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1p1::features::tag_section_storage_t<Fs...>>(module_storage.sections)};
        constexpr ::std::size_t tag_import_bucket{4uz};
        static_assert(tag_import_bucket < imports.importdesc_count);
        // [actual import descriptor buckets ... tag_import_bucket ... end)
        // [safe                                                         ] fixed-array bound is proven above at compile time.
        // No bucket pointer is advanced; this borrows the actual parser's tag-import vector.
        // [actual parsed module_begin ... module_end]
        // [safe                                   ] diagnostic borrow only, not a fabricated empty function.
        // ^^ module_begin: parser owns this span; no bytecode cursor or pointer arithmetic is introduced.
        auto const diagnostic{module_storage.module_span.module_begin};
        require_module_declaration_policy(policy,
            {.types = types.requires_gc, .tables = tables.requires_gc,
             .globals = globals.requires_gc, .elements = elements.requires_gc},
            {.types = types.requires_exceptions, .tables = tables.requires_exceptions,
             .globals = globals.requires_exceptions, .elements = elements.requires_exceptions,
             .locals = codes.locals_require_exceptions, .tags = tags.present || !imports.importdesc.index_unchecked(tag_import_bucket).empty()},
            diagnostic, err,
            {.types = types.requires_function_references, .tables = tables.requires_function_references,
             .globals = globals.requires_function_references, .elements = elements.requires_function_references},
            {.simd = types.requires_simd, .reference_types = types.requires_reference_types,
             .multi_value = types.requires_multi_value},
            {.tables_reference_types = tables.requires_reference_types, .globals_reference_types = globals.requires_reference_types,
             .elements_reference_types = elements.requires_reference_types, .globals_simd = globals.requires_simd,
             .table_reference_value = tables.reference_types_diagnostic_value,
             .global_reference_value = globals.reference_types_diagnostic_value,
             .element_reference_value = elements.reference_types_diagnostic_value},
            {.memory64 = memories.requires_memory64, .table64 = tables.requires_table64,
             .shared = memories.requires_threads, .multi_memory = core3_has_multiple_declarations(
                 imports.importdesc.index_unchecked(2uz).size(), memories.memories.size())},
            {.table_initializer = tables.requires_table_initializer,
             .extended_const = globals.constant_expressions_require_extended_const,
             .extended_const_value = globals.extended_const_diagnostic_value,
             .extended_const_subject = globals.extended_const_diagnostic_subject,
             .opcodes = globals.constant_expression_opcode_requirements});
    }

    // Runtime selection remains explicit: complete legacy selectors preserve their
    // validator, while scoped policies use this independently evolving Core 3 body.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code_with_runtime_policy(
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
        ::std::size_t function_index, ::std::byte const* code_begin, ::std::byte const* code_end,
        ::uwvm2::validation::error::code_validation_error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        auto const& policy{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        if(uses_core3_validation_policy(policy))
        {
            validate_code(wasm3_code_version{}, module_storage, function_index, code_begin, code_end, err, fs_para);
        }
        else
        {
            // A parsed module can be reused with the legacy/default policy. Reject
            // its memory64 declarations before delegating to an unchanged legacy
            // validator, even if this particular body does not access memory.
            validate_module_declarations_with_runtime_policy(module_storage, err, fs_para);
            // A previously parsed Core 3 declaration must not lose its policy when this facade
            // delegates instruction validation to an unchanged legacy validator.
            auto const& function_imports{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(module_storage.sections).importdesc.index_unchecked(0uz)};
            auto const& functions{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>(module_storage.sections).funcs};
            auto const& types{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(module_storage.sections)};
            require_gc_recursive_type_policy(false, types.requires_gc, code_begin, err);
            auto const& codes{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(module_storage.sections).codes};
            if(function_index >= function_imports.size())
            {
                auto const local_index{function_index - function_imports.size()};
                // Leave invalid function/type diagnostics to the selected validator. Bounds here prevent
                // this policy precheck from reading an untrusted index before that validator runs.
                if(local_index < functions.size() && local_index < codes.size())
                {
                    auto const type_index{functions.index_unchecked(local_index)};
                    if(type_index < types.types.size())
                    {
                        require_function_declaration_policy(types.types.index_unchecked(type_index), codes.index_unchecked(local_index).locals,
                            policy.disable_function_references && signatures_require_function_references(types), policy, code_begin, err,
                            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage.sections).requires_function_references,
                            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections).requires_function_references,
                            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>>(module_storage.sections).requires_function_references,
                            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1p1::features::tag_section_storage_t<Fs...>>(module_storage.sections).present ||
            !::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(module_storage.sections).importdesc.index_unchecked(4uz).empty(),
                            ::std::addressof(types.core3_context), types.types.size(),
                            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage.sections).requires_gc,
                            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections).requires_gc,
                            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>>(module_storage.sections).requires_gc,
                            {.types = types.requires_exceptions,
                             .tables = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage.sections).requires_exceptions,
                             .globals = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections).requires_exceptions,
                             .elements = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>>(module_storage.sections).requires_exceptions,
                             .locals = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(module_storage.sections).locals_require_exceptions},
                            {.simd = types.requires_simd, .reference_types = types.requires_reference_types,
                             .multi_value = types.requires_multi_value},
                            {.tables_reference_types = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage.sections).requires_reference_types,
                             .globals_reference_types = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections).requires_reference_types,
                             .elements_reference_types = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>>(module_storage.sections).requires_reference_types,
                             .globals_simd = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections).requires_simd,
                             .table_reference_value = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage.sections).reference_types_diagnostic_value,
                             .global_reference_value = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections).reference_types_diagnostic_value,
                             .element_reference_value = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>>(module_storage.sections).reference_types_diagnostic_value},
                            {.memory64 = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>(module_storage.sections).requires_memory64,
                             .table64 = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage.sections).requires_table64,
                             .shared = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>(module_storage.sections).requires_threads,
                             .multi_memory = core3_has_multiple_declarations(
                                ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                    ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(module_storage.sections).importdesc.index_unchecked(2uz).size(),
                                ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                    ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>(module_storage.sections).memories.size())},
                            {.table_initializer = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage.sections).requires_table_initializer,
                             .extended_const = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections).constant_expressions_require_extended_const,
                             .extended_const_value = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections).extended_const_diagnostic_value,
                             .extended_const_subject = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections).extended_const_diagnostic_subject,
                             .opcodes = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections).constant_expression_opcode_requirements});
                    }
                }
            }
            ::uwvm2::validation::standard::wasm2::validate_code_with_runtime_policy(
                module_storage, function_index, code_begin, code_end, err, fs_para);
        }
    }
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif

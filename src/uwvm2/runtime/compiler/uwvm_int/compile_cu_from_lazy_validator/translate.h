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
# include <climits>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <exception>
# include <limits>
# include <memory>
# include <type_traits>
# include <utility>
# include <vector>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/validation/standard/wasm3/relaxed_simd.h>
# include <uwvm2/validation/standard/wasm3/threads.h>
# include <uwvm2/validation/standard/wasm3/tail_call.h>
// import
# include <fast_io.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/thread/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/features/call_indirect_immediate.h>
# include <uwvm2/parser/wasm/standard/wasm2/features/impl.h>
# include <uwvm2/validation/error/impl.h>
# include <uwvm2/validation/standard/wasm1/impl.h>
# include <uwvm2/validation/standard/wasm1p1/impl.h>
# include <uwvm2/validation/standard/wasm3/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/utils/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/optable/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/impl.h>
# include <uwvm2/runtime/lib/uwvm_runtime.h>
# include "checked_plan_lowering.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator
{
    // Keep the public lazy-compiler surface expressed in wasm and interpreter terms. The aliases make the code below read as a
    // translation pipeline rather than as a collection of long fully-qualified storage names.
    using wasm1_code = ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic;
    using wasm1p1_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_basic;
    using wasm1p1_numeric_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_numeric;
    using wasm1p1_simd_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_simd;
    using wasm_byte = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte;
    using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
    /// @warning Extension point: lazy scanner support must be audited when wasm_binfmt1_final_value_type_t gains new value categories.
    using wasm_value_type = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_value_type_t;
    using code_validation_error_code = ::uwvm2::validation::error::code_validation_error_code;

    using runtime_module_storage_t = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
    using parser_module_storage_t = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t;
    using parser_feature_parameter_t = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t;
    using full_function_symbol_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t;
    using local_func_storage_t = ::uwvm2::runtime::compiler::uwvm_int::optable::local_func_storage_t;

    inline constexpr wasm_byte opcode_byte(wasm1p1_code opcode) noexcept { return static_cast<wasm_byte>(opcode); }
    inline constexpr wasm_u32 opcode_u32(wasm1p1_code opcode) noexcept { return static_cast<wasm_u32>(opcode_byte(opcode)); }

    // Lazy validation can either re-run the validator at materialization time or trust that the caller already validated all code.
    // The explicit mode prevents the lazy path from silently weakening validation guarantees.
    enum class lazy_validation_mode : unsigned
    {
        validate_on_lazy_compile,
        assume_full_code_verified
    };

    // Execution units describe structural wasm regions. They are indexing metadata only; actual code generation still materializes
    // the owning function so control-flow repair and interpreter metadata stay identical to the eager compiler.
    enum class lazy_execution_unit_kind : unsigned
    {
        function,
        block,
        loop,
        if_
    };

    // Compile-unit kinds record why a unit exists. This is mainly diagnostic today, but preserving the reason lets runtime logs
    // explain whether a compile happened because of policy, structure, or code-size grouping.
    enum class lazy_compile_unit_kind : unsigned
    {
        function,
        execution_unit,
        code_size_group
    };

    // A compile unit may name a narrow byte range while still requiring whole-function materialization. Keeping the two concepts
    // separate avoids promising partial code generation before the interpreter backend can safely support it.
    enum class lazy_materialization_scope : unsigned
    {
        whole_function,
        execution_unit_range
    };

    // The execution-unit policy controls index granularity independently from compile-unit scheduling. This keeps bytecode scanning
    // decisions decoupled from threading and cache policy decisions.
    enum class lazy_execution_unit_split_policy_t : unsigned
    {
        function_only,
        structured_control
    };

    // Compile-unit policy decides how much lazy work becomes externally schedulable. The enum makes the policy explicit instead of
    // baking one grouping strategy into the scanner.
    enum class lazy_compile_unit_split_policy_t : unsigned
    {
        function,
        execution_unit,
        code_size
    };

    [[nodiscard]] inline constexpr ::fast_io::u8string_view lazy_execution_unit_kind_name(lazy_execution_unit_kind kind) noexcept
    {
        switch(kind)
        {
            case lazy_execution_unit_kind::function: return u8"function";
            case lazy_execution_unit_kind::block: return u8"block";
            case lazy_execution_unit_kind::loop: return u8"loop";
            case lazy_execution_unit_kind::if_:
                return u8"if";
            [[unlikely]] default:
                return u8"unknown";
        }
    }

    [[nodiscard]] inline constexpr ::fast_io::u8string_view lazy_compile_unit_kind_name(lazy_compile_unit_kind kind) noexcept
    {
        switch(kind)
        {
            case lazy_compile_unit_kind::function: return u8"function";
            case lazy_compile_unit_kind::execution_unit: return u8"execution_unit";
            case lazy_compile_unit_kind::code_size_group:
                return u8"code_size_group";
            [[unlikely]] default:
                return u8"unknown";
        }
    }

    [[nodiscard]] inline constexpr ::fast_io::u8string_view lazy_materialization_scope_name(lazy_materialization_scope scope) noexcept
    {
        switch(scope)
        {
            case lazy_materialization_scope::whole_function: return u8"whole_function";
            case lazy_materialization_scope::execution_unit_range:
                return u8"execution_unit_range";
            [[unlikely]] default:
                return u8"unknown";
        }
    }

    struct lazy_split_config
    {
        // Structured-control scanning is the default because it provides useful profiling and scheduling boundaries without needing
        // a full validation pass during module initialization.
        lazy_execution_unit_split_policy_t eu_policy{lazy_execution_unit_split_policy_t::structured_control};
        // Code-size grouping avoids producing one tiny task per block in heavily structured functions, which would waste scheduler time.
        lazy_compile_unit_split_policy_t cu_policy{lazy_compile_unit_split_policy_t::code_size};
        // The target is intentionally a soft threshold: groups are flushed at execution-unit boundaries so wasm control structure is
        // never split in the middle of an immediate or nested construct.
        ::std::size_t cu_code_size{4096uz};
    };

    struct lazy_execution_unit_storage_t
    {
        // The absolute wasm function index is needed by validators and diagnostics; the local index addresses runtime local storage.
        ::std::size_t function_index{};
        ::std::size_t local_function_index{};
        // SIZE_MAX denotes the synthetic root. A parent index is stored so logs and future range materializers can reconstruct nesting.
        ::std::size_t parent_eu_index{SIZE_MAX};
        // Depth is recorded during the single scan so later policies do not need to rebuild a control stack.
        ::std::size_t depth{};
        // Pointers refer into the already-owned wasm code buffer; lazy metadata never copies instruction bytes.
        ::std::byte const* code_begin{};
        ::std::byte const* code_end{};
        // The byte offset is a stable, serializable description of the range for logging and debugging.
        ::std::size_t code_offset{};
        ::std::size_t code_size{};
        lazy_execution_unit_kind kind{lazy_execution_unit_kind::function};
    };

    struct lazy_compile_unit_storage_t
    {
        // Each compile unit carries its own state so a future narrower materializer can synchronize per range.
        ::uwvm2::utils::thread::lazy_compile_unit_state state{};
        // These indices intentionally mirror execution units, letting the request path jump directly to the owning function.
        ::std::size_t function_index{};
        ::std::size_t local_function_index{};
        // [begin_eu_index, end_eu_index) is used rather than a vector of indices to keep metadata compact and cache-friendly.
        ::std::size_t begin_eu_index{};
        ::std::size_t end_eu_index{};
        // The byte span summarizes all execution units covered by this compile unit for diagnostics and prefetch heuristics.
        ::std::byte const* code_begin{};
        ::std::byte const* code_end{};
        ::std::size_t code_offset{};
        ::std::size_t code_size{};
        lazy_compile_unit_kind kind{lazy_compile_unit_kind::function};
        // The current backend compiles a complete function even when the request was triggered by a smaller structural range.
        lazy_materialization_scope materialization_scope{lazy_materialization_scope::whole_function};
    };

    struct lazy_function_storage_t
    {
        // Whole-function state is the authoritative synchronization point while materialization emits complete functions.
        ::uwvm2::utils::thread::lazy_compile_unit_state materialization_state{};
        // Both indices are retained because wasm-visible references include imports while local storage excludes them.
        ::std::size_t function_index{};
        ::std::size_t local_function_index{};
        // Contiguous ranges make module initialization cheap and avoid per-function heap allocation for metadata slices.
        ::std::size_t first_eu_index{};
        ::std::size_t eu_count{};
        ::std::size_t first_cu_index{};
        ::std::size_t cu_count{};
        // SIZE_MAX means no schedulable compile unit has been selected yet; a fallback function unit is created in that case.
        ::std::size_t primary_cu_index{SIZE_MAX};
    };

    namespace details { class checked_register_ring_admission; }

    struct lazy_module_storage_t
    {
        // Only initialize_checked_lazy_module_storage installs this immutable
        // actual-source plan. Declared first, it pins parsed/type closure until
        // all compiled symbols and delayed work have drained and are destroyed.
        checked_integer_module_plan::owner checked_plan{};
        // Default standard admission owns the original checked ring artifact.
        ::std::unique_ptr<details::checked_register_ring_admission> ring_admission{};
        // The compiled symbol table has the same shape as eager compilation so interpreter dispatch does not need a lazy-only ABI.
        full_function_symbol_t compiled{};
        // Metadata is stored in module-wide arrays, allowing requests to pass around small indices instead of owning subobjects.
        ::uwvm2::utils::container::vector<lazy_function_storage_t> functions{};
        ::uwvm2::utils::container::vector<lazy_execution_unit_storage_t> execution_units{};
        ::uwvm2::utils::container::vector<lazy_compile_unit_storage_t> compile_units{};
    };

    struct lazy_compile_options
    {
        // The lazy path forwards normal interpreter compile options to the eager function compiler to keep emitted code identical.
        ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option compile_options{};
        // Optional parser provenance retained for callers; the fused compiler reads runtime declarations.
        parser_module_storage_t const* validator_module_storage{};
        // Validation also needs the exact feature switches used to parse that module.
        parser_feature_parameter_t const* validator_feature_parameter{};
        lazy_validation_mode validation_mode{lazy_validation_mode::validate_on_lazy_compile};
    };

    struct lazy_compile_request_context
    {
        // The request owns no storage; it binds scheduler work to module lifetime managed by the runtime.
        runtime_module_storage_t const* curr_module{};
        lazy_module_storage_t* lazy_storage{};
        lazy_compile_options options{};
        // A compile request is addressed by index so it remains trivially movable and cheap to enqueue.
        ::std::size_t compile_unit_index{};
        // The caller may provide an error object for synchronous reporting; asynchronous callers can fall back to a local one.
        ::uwvm2::validation::error::code_validation_error_impl* err{};
        ::uwvm2::utils::container::u8string_view module_name{};
    };

    namespace details
    {
        struct lazy_split_control_frame
        {
            // The scanner only needs the current unit and its kind to assign parentage and validate else/end placement.
            ::std::size_t eu_index{};
            lazy_execution_unit_kind kind{lazy_execution_unit_kind::function};
        };

        // Centralize the "top of control stack is the current parent" rule so the scanner stays readable at nesting sites.
        [[nodiscard]] inline constexpr ::std::size_t active_parent_eu_index(lazy_split_control_frame const& frame) noexcept { return frame.eu_index; }

        // Offsets are computed from raw pointers during scanning because the wasm body is already memory mapped or owned elsewhere.
        [[nodiscard]] inline constexpr ::std::size_t byte_offset(::std::byte const* base, ::std::byte const* curr) noexcept
        { return static_cast<::std::size_t>(curr - base); }

        // Lazy splitting reports the same validation error categories as the real validator. This keeps diagnostics stable even
        // though this scanner only reads enough wasm to find structural ranges.
        inline constexpr void fail_lazy_split(::std::byte const* op_begin,
                                              code_validation_error_code ec,
                                              ::uwvm2::validation::error::code_validation_error_impl& err,
                                              ::fast_io::parse_code pc = ::fast_io::parse_code::invalid) UWVM_THROWS
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_code = ec;
            // Throw through the parser path so callers observe the same failure mechanism used by normal wasm validation.
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(pc);
        }

        [[noreturn]] inline constexpr void fail_lazy_feature_required(
            ::std::byte const* op_begin,
            ::uwvm2::validation::error::code_validation_error_impl& err,
            ::std::uint_least32_t value,
            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind feature,
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject subject) UWVM_THROWS
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_selectable.wasm1p1_feature_required.value = value;
            err.err_selectable.wasm1p1_feature_required.feature = feature;
            err.err_selectable.wasm1p1_feature_required.subject = subject;
            err.err_code = code_validation_error_code::wasm1p1_feature_required;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        [[noreturn]] inline constexpr void fail_lazy_wasm2_feature_required(
            ::std::byte const* op_begin,
            ::uwvm2::validation::error::code_validation_error_impl& err,
            ::std::uint_least32_t value,
            ::uwvm2::parser::wasm::base::wasm2_feature_kind feature,
            ::uwvm2::parser::wasm::base::wasm2_error_subject subject) UWVM_THROWS
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_selectable.wasm2_feature_required.value = value;
            err.err_selectable.wasm2_feature_required.feature = feature;
            err.err_selectable.wasm2_feature_required.subject = subject;
            err.err_code = code_validation_error_code::wasm2_feature_required;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        template <typename T>
        inline constexpr T read_leb128_immediate(::std::byte const*& code_curr,
                                                 ::std::byte const* code_end,
                                                 ::std::byte const* op_begin,
                                                 code_validation_error_code ec,
                                                 ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            // Immediates are decoded, not interpreted: the scanner must advance exactly like the validator while avoiding semantic work.
            T value;  // No initialization necessary

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const [next, parse_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                  ::fast_io::mnp::leb128_get(value))};
            // Reuse the caller-provided error code so each opcode reports the immediate field that failed, not a generic scan error.
            if(parse_err != ::fast_io::parse_code::ok) [[unlikely]] { fail_lazy_split(op_begin, ec, err, parse_err); }

            // LEB immediate ... code_end
            // [safe parsed bytes] unsafe (could be code_end)
            //                   ^^ next: successful scanner returned within the same input slice.
            // [bounded decoded/immediate cursor] next bytes ... | end
            // [safe consumed bytes]       | one-past is never dereferenced here
            // ^^ code_curr: the successful scanner or checked lookahead supplies a position within the current code slice.
            code_curr = reinterpret_cast<::std::byte const*>(next);
            // LEB immediate ... code_end
            // [safe consumed bytes] unsafe (could be code_end)
            //                       ^^ code_curr may be one-past; this assignment does not read it.
            return value;
        }

        inline constexpr void skip_fixed_immediate(::std::byte const*& code_curr,
                                                   ::std::byte const* code_end,
                                                   ::std::byte const* op_begin,
                                                   ::std::size_t bytes,
                                                   ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            // Fixed-width constants can be skipped by length, but the bounds check must still run here because the lazy scanner may
            // execute before full validation when validation-on-compile is selected.
            auto const remaining{static_cast<::std::size_t>(code_end - code_curr)};
            if(bytes > remaining) [[unlikely]]
            {
                fail_lazy_split(op_begin, code_validation_error_code::invalid_const_immediate, err, ::fast_io::parse_code::end_of_file);
            }
            // fixed immediate[bytes] ... code_end
            // [safe bytes            ] unsafe (could be code_end)
            // ^^ code_curr: bytes <= remaining was checked before advancing.
            code_curr += bytes;
            // fixed immediate[bytes] ... code_end
            // [safe bytes            ] unsafe (could be code_end)
            //                         ^^ code_curr may be one-past.
        }

        [[nodiscard]] inline constexpr wasm_byte read_u8_immediate(::std::byte const*& code_curr,
                                                                    ::std::byte const* code_end,
                                                                    ::std::byte const* op_begin,
                                                                    ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            auto const imm_pos{code_curr};
            skip_fixed_immediate(code_curr, code_end, op_begin, 1uz, err);

            wasm_byte value{};  // No initialization necessary.
            ::std::memcpy(::std::addressof(value), imm_pos, sizeof(value));
#if CHAR_BIT > 8
            value = static_cast<wasm_byte>(static_cast<::std::uint_least8_t>(value) & 0xFFu);
#endif
            return value;
        }

        inline constexpr void ensure_lazy_wasm1p1_value_type_enabled(
            ::std::byte const* op_begin,
            wasm_byte type_byte,
            parser_feature_parameter_t const& wasm_feature_parameter,
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject subject,
            ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            auto const vt{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>(type_byte)};
            if(!::uwvm2::parser::wasm::standard::wasm1p1::type::is_valid_value_type(vt)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.wasm1p1_invalid_reference_type.value = type_byte;
                err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            if(!::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(vt, wasm_feature_parameter)) [[unlikely]]
            {
                auto const feature{vt == ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128
                                       ? ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd
                                       : ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types};
                fail_lazy_feature_required(op_begin, err, type_byte, feature, subject);
            }
        }

        inline constexpr void skip_reserved_memory_index_byte(::std::byte const*& code_curr,
                                                              ::std::byte const* code_end,
                                                              ::std::byte const* op_begin,
                                                              ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            if(code_curr == code_end) [[unlikely]]
            {
                fail_lazy_split(op_begin, code_validation_error_code::invalid_memory_index, err, ::fast_io::parse_code::end_of_file);
            }
            // reserved memory-index byte ... code_end
            // [safe byte                 ] unsafe (could be code_end)
            // ^^ code_curr: equality check proved the byte exists.
            ++code_curr;
            // [safe byte                 ] unsafe (could be code_end)
            //                             ^^ code_curr may be one-past.
        }

        inline constexpr void skip_wasm1p1_block_type(::std::byte const*& code_curr,
                                                      ::std::byte const* code_end,
                                                      ::std::byte const* op_begin,
                                                      runtime_module_storage_t const& curr_module,
                                                      parser_feature_parameter_t const& wasm_feature_parameter,
                                                      ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            if(code_curr == code_end) [[unlikely]]
            {
                fail_lazy_split(op_begin, code_validation_error_code::missing_block_type, err, ::fast_io::parse_code::end_of_file);
            }

            // control_op blocktype ...
            // [  safe  ] unsafe (could be the section_end)
            //            ^^ code_curr

            // [control opcode][valtype or s33 index ... code_end)
            // [safe          ] nonempty check above proves the prefix readable.
            if(::uwvm2::validation::standard::wasm3::is_core3_extended_block_reference_prefix(
                ::std::to_integer<unsigned>(*code_curr)))
            {
                auto const& policy{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(wasm_feature_parameter)};
                auto const types_begin{curr_module.type_section_storage.type_section_begin};
                auto const types_end{curr_module.type_section_storage.type_section_end};
                // Both type-table endpoints are from the initializer's retained allocation or both null.
                // [types_begin, types_end) is subtracted only when both endpoints are present.
                // [safe                  ] no pointer moves; the count bounds concrete heap indices.
                auto const type_count{types_begin == nullptr || types_end == nullptr ? 0uz :
                    static_cast<::std::size_t>(types_end - types_begin)};
                ::uwvm2::validation::standard::wasm3::recursive_type_context const empty_context{};
                // The initializer borrows a validated, immutable Core 3 context for this module.
                // A legacy module has no context; the decoder then classifies its bounded type table as functions.
                auto const* const retained_context{curr_module.type_section_storage.core3_context_ptr};
                auto const& context{retained_context == nullptr ? empty_context : *retained_context};
                auto const carrier{::uwvm2::validation::standard::wasm3::read_core3_value_carrier(
                    code_curr, code_end, !policy.disable_function_references, op_begin, err,
                    type_count, nullptr, context, !policy.disable_gc, !policy.disable_exceptions)};
                // [opcode][checked valtype] next ... code_end
                // [safe                  ] unsafe (could be code_end)
                //                          ^^ code_curr: bounded decoder skips the entire immediate.
                // The Core 3 decoder has checked GC, exception and function
                // reference gates against the exact heap. A legacy check on
                // the projected 0x70 carrier would reject GC-only blocktypes.
                static_cast<void>(carrier);
                return;
            }

            auto const blocktype_begin{code_curr};
            auto const blocktype{
                read_leb128_immediate<wasm_i64>(code_curr, code_end, op_begin, code_validation_error_code::illegal_block_type, err)};
            auto const blocktype_encoded_size{static_cast<::std::size_t>(code_curr - blocktype_begin)};

            // control_op blocktype ...
            // [       safe       ] unsafe (could be the section_end)
            //                      ^^ code_curr

            // read_leb128_immediate commits code_curr only after validating the complete immediate. A decode failure
            // throws with code_curr still equal to blocktype_begin; grammar failures below occur after the commit.
            auto const fail_illegal_block_type{[&]() constexpr UWVM_THROWS
                                               {
                                                   wasm_byte first_blocktype_byte{};
                                                   ::std::memcpy(::std::addressof(first_blocktype_byte), blocktype_begin, sizeof(first_blocktype_byte));
#if CHAR_BIT > 8
                                                   first_blocktype_byte =
                                                       static_cast<wasm_byte>(static_cast<::std::uint_least8_t>(first_blocktype_byte) & 0xFFu);
#endif
                                                   // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                                   // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                                   // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                                   err.err_curr = op_begin;
                                                   err.err_selectable.u8 = first_blocktype_byte;
                                                   err.err_code = code_validation_error_code::illegal_block_type;
                                                   ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                               }};

            if(blocktype_encoded_size > 5uz || (blocktype < 0 && blocktype_encoded_size != 1uz)) [[unlikely]]
            {
                fail_illegal_block_type();
            }

            auto const& wasm1p1_para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(wasm_feature_parameter)};
            using wasm2_feature_kind = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind;
            auto const wasm2_feature_enabled{[&](wasm2_feature_kind const feature) constexpr noexcept
                                             { return ::uwvm2::parser::wasm::standard::wasm2::features::feature_enabled(wasm1p1_para, feature); }};
            switch(blocktype)
            {
                case -64:
                case -1:
                case -2:
                case -3:
                case -4:
                {
                    return;
                }
                case -5:
                {
                    if(!wasm2_feature_enabled(wasm2_feature_kind::simd)) [[unlikely]]
                    {
                        fail_lazy_feature_required(op_begin,
                                                   err,
                                                   static_cast<::std::uint_least32_t>(static_cast<wasm_byte>(
                                                       ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)),
                                                   ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd,
                                                   ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                    }
                    return;
                }
                case -16:
                case -17:
                {
                    if(!wasm2_feature_enabled(wasm2_feature_kind::reference_types)) [[unlikely]]
                    {
                        auto const vt{blocktype == -16 ? ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref
                                                       : ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::externref};
                        fail_lazy_feature_required(op_begin,
                                                   err,
                                                   static_cast<::std::uint_least32_t>(static_cast<wasm_byte>(vt)),
                                                   ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                   ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                    }
                    return;
                }
                default:
                {
                    break;
                }
            }

            if(blocktype >= 0)
            {
                if(!wasm2_feature_enabled(wasm2_feature_kind::multi_value)) [[unlikely]]
                {
                    fail_lazy_feature_required(op_begin,
                                               err,
                                               static_cast<::std::uint_least32_t>(static_cast<wasm_u32>(blocktype)),
                                               ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::multi_value,
                                               ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                }

                auto const types_begin{curr_module.type_section_storage.type_section_begin};
                auto const types_end{curr_module.type_section_storage.type_section_end};
                auto const all_type_count_uz{
                    (types_begin == nullptr || types_end == nullptr) ? 0uz : static_cast<::std::size_t>(types_end - types_begin)};
                if(static_cast<::std::uint_least64_t>(blocktype) > ::std::numeric_limits<wasm_u32>::max() ||
                   static_cast<::std::size_t>(blocktype) >= all_type_count_uz) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.illegal_type_index.type_index = blocktype > 0 ? static_cast<wasm_u32>(blocktype) : 0u;
                    err.err_selectable.illegal_type_index.all_type_count = static_cast<wasm_u32>(all_type_count_uz);
                    err.err_code = code_validation_error_code::illegal_type_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                // The caller proved op_begin is the structural opcode; this borrow never advances it.
                using opcode_name_t = ::uwvm2::utils::container::u8string_view;
                auto const op_name{*op_begin == ::std::byte{0x02u} ? opcode_name_t{u8"block"} :
                    *op_begin == ::std::byte{0x03u} ? opcode_name_t{u8"loop"} :
                    *op_begin == ::std::byte{0x04u} ? opcode_name_t{u8"if"} : opcode_name_t{u8"try_table"}};
                ::uwvm2::validation::standard::wasm3::require_core3_function_type_index_policy(
                    curr_module.type_section_storage.core3_context_ptr, static_cast<::std::size_t>(blocktype), op_begin, op_name, err);
                return;
            }

            fail_illegal_block_type();
        }

        inline constexpr ::std::size_t append_execution_unit(lazy_module_storage_t& storage,
                                                             ::std::size_t function_index,
                                                             ::std::size_t local_function_index,
                                                             ::std::size_t parent_eu_index,
                                                             ::std::size_t depth,
                                                             ::std::byte const* function_code_begin,
                                                             ::std::byte const* code_begin,
                                                             ::std::byte const* code_end,
                                                             lazy_execution_unit_kind kind) noexcept
        {
            // Append-only indices remain stable for the rest of initialization, which lets control-stack frames store plain indices.
            auto const eu_index{storage.execution_units.size()};
            // Open structural units are appended with a null end and closed when the matching end opcode is seen.
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

        inline constexpr void set_execution_unit_end(lazy_module_storage_t& storage, ::std::size_t eu_index, ::std::byte const* code_end) noexcept
        {
            auto& eu{storage.execution_units.index_unchecked(eu_index)};
            // The end pointer is exclusive and includes the closing opcode, matching wasm body slicing conventions elsewhere.
            eu.code_end = code_end;
            eu.code_size = static_cast<::std::size_t>(code_end - eu.code_begin);
        }

        inline constexpr ::std::size_t append_compile_unit_from_eu_range(lazy_module_storage_t& storage,
                                                                         lazy_function_storage_t const& fn,
                                                                         ::std::size_t begin_eu_index,
                                                                         ::std::size_t end_eu_index,
                                                                         lazy_compile_unit_kind kind,
                                                                         lazy_materialization_scope scope) noexcept
        {
            // Compile units cover a contiguous execution-unit range. The byte range is widened to the maximum end because nested
            // units can have different sizes while still sharing the same function-owned code buffer.
            auto const cu_index{storage.compile_units.size()};
            auto const& first_eu{storage.execution_units.index_unchecked(begin_eu_index)};
            auto const code_begin{first_eu.code_begin};
            auto code_end{first_eu.code_end};
            for(::std::size_t i{begin_eu_index + 1uz}; i != end_eu_index; ++i)
            {
                auto const curr_end{storage.execution_units.index_unchecked(i).code_end};
                // [closed same-function execution units] | function end
                // [bounded function-owned bytes         ] | end is exclusive
                // ^^ code_end: the compile-unit range is function-owned; select the furthest end without dereferencing.
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

        inline constexpr void append_function_compile_units(lazy_module_storage_t& storage, lazy_function_storage_t& fn, lazy_split_config cfg) noexcept
        {
            fn.first_cu_index = storage.compile_units.size();

            // A single whole-function unit is the conservative fallback and also the requested behavior for function-only policy.
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
                                        // The synthetic function unit would duplicate the fallback range, so only nested structures
                                        // are eligible for structural or size-based compile-unit grouping.
                                        if(eu.kind == lazy_execution_unit_kind::function) { return false; }
                                        return true;
                                    }};

            if(cfg.cu_policy == lazy_compile_unit_split_policy_t::execution_unit)
            {
                // One compile unit per structural range maximizes scheduling precision and gives logs the clearest trigger point.
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
                // Size grouping is a compromise: it keeps lazy work coarse enough for the scheduler while still preserving useful
                // structural boundaries for diagnostics and future partial materialization.
                ::std::size_t group_begin{SIZE_MAX};
                ::std::size_t group_end{SIZE_MAX};
                ::std::size_t group_size{};

                auto const flush_group{[&]() constexpr noexcept
                                       {
                                           // Empty groups are represented by SIZE_MAX so the hot loop can flush unconditionally.
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
                            // Saturating here is enough: the next threshold check will flush the group without risking overflow.
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
                // Functions without nested structural candidates still need a schedulable trigger, so create a whole-function unit.
                fn.primary_cu_index = append_compile_unit_from_eu_range(storage,
                                                                        fn,
                                                                        fn.first_eu_index,
                                                                        fn.first_eu_index + fn.eu_count,
                                                                        lazy_compile_unit_kind::function,
                                                                        lazy_materialization_scope::whole_function);
            }

            fn.cu_count = storage.compile_units.size() - fn.first_cu_index;
        }

        inline constexpr void skip_wasm1_non_structural_immediates(::std::byte const*& code_curr,
                                                                   ::std::byte const* code_end,
                                                                   ::std::byte const* op_begin,
                                                                   wasm1_code curr_opbase,
                                                                   runtime_module_storage_t const& curr_module,
                                                                   parser_feature_parameter_t const& wasm_feature_parameter,
                                                                   ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            // Non-structural immediates are skipped precisely so the scanner can continue finding block/loop/if/else/end opcodes
            // without performing full stack validation.  Where this indexing pass can reject an instruction, it preserves the
            // eager validator's immediate-first semantic diagnostic order.
            /// @warning Extension point: every opcode with immediates must be listed here or lazy structural splitting will desynchronize.
            auto const& wasm1p1_para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(wasm_feature_parameter)};
            using wasm2_feature_kind = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind;
            auto const wasm2_feature_enabled{[&](wasm2_feature_kind const feature) constexpr noexcept
                                             { return ::uwvm2::parser::wasm::standard::wasm2::features::feature_enabled(wasm1p1_para, feature); }};
            auto const read_table_index = [&](wasm_u32 opcode) constexpr UWVM_THROWS -> wasm_u32
            {
                auto const table_index{
                    read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_table_index, err)};
                if(!wasm2_feature_enabled(wasm2_feature_kind::multiple_tables) && table_index != 0u) [[unlikely]]
                {
                    fail_lazy_wasm2_feature_required(op_begin,
                                                     err,
                                                     opcode,
                                                     ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables,
                                                     ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
                }
                return table_index;
            };
            switch(curr_opbase)
            {
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wswitch" // Core 3 exception opcode extends the shared wasm1 enum.
#endif
                case static_cast<wasm1_code>(0x0au):
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
                {
                    ::uwvm2::validation::standard::wasm3::require_exceptions_enabled(!wasm1p1_para.disable_exceptions, 0x0au, op_begin, err);
                    // [throw_ref] next ... code_end; outer scanner consumed the checked opcode.
                    // [safe     ] unsafe (could be code_end)
                    //             ^^ code_curr: no immediate, no read or movement here.
                    // Whole-function compilation checks the operand and rejects reachable throw_ref.
                    return;
                }
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wswitch" // Core 3 exception opcode extends the shared wasm1 enum.
#endif
                case static_cast<wasm1_code>(0x08u):
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
                {
                    ::uwvm2::validation::standard::wasm3::require_exceptions_enabled(!wasm1p1_para.disable_exceptions, 0x08u, op_begin, err);
                    // [throw] tagidx ... code_end; bounded reader commits only the complete u32.
                    // [safe ] unsafe (could be code_end)
                    //         ^^ code_curr before decode
                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_label_index, err);
                    // [throw checked tagidx] next ... code_end
                    // [safe               ] unsafe (could be code_end)
                    //                       ^^ code_curr; whole-function compilation checks tag/payload/handler semantics.
                    return;
                }
                case wasm1_code::br:
                case wasm1_code::br_if:
                {
                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_label_index, err);
                    return;
                }
                case wasm1_code::br_table:
                {
                    // br_table is length-prefixed, so validate the count before reading the target list to avoid walking past code_end.
                    auto const target_count{
                        read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_label_index, err)};

                    auto const remaining_bytes{static_cast<::std::size_t>(code_end - code_curr)};
                    auto const target_count_uz{static_cast<::std::size_t>(target_count)};
                    auto const target_count_exceeds_size_t{static_cast<wasm_u32>(target_count_uz) != target_count};
                    auto const target_count_plus_default_overflows{!target_count_exceeds_size_t &&
                                                                   target_count_uz == ::std::numeric_limits<::std::size_t>::max()};
                    if(target_count_exceeds_size_t || target_count_plus_default_overflows || target_count_uz + 1uz > remaining_bytes) [[unlikely]]
                    {
                        // Populate the richer selectable payload used by the standard validator for the same malformed table case.
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wswitch" // Tail-call opcode extends the shared wasm1 enum.
#endif
                case static_cast<wasm1_code>(0x12u):
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
                {
                    ::uwvm2::validation::standard::wasm3::require_tail_call_enabled(!wasm1p1_para.disable_tail_call, 0x12u, op_begin, err);
                    // [return_call] funcidx ... end
                    // [safe       ] unsafe; bounded scanner commits after the complete u32.
                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_function_index_encoding, err);
                    // [return_call funcidx] ... end
                    // [safe              ] unsafe (could be end)
                    //                      ^^ code_curr
                    return;
                }
                case wasm1_code::call:
                {
                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_function_index_encoding, err);
                    return;
                }
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wswitch" // Typed-reference opcodes extend the shared wasm1 enum.
#endif
                case static_cast<wasm1_code>(0x14u):
                case static_cast<wasm1_code>(0x15u):
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
                {
                    auto const tail{curr_opbase == static_cast<wasm1_code>(0x15u)};
                    ::uwvm2::validation::standard::wasm3::require_function_references_enabled(
                        !wasm1p1_para.disable_function_references, tail ? 0x15u : 0x14u, op_begin, err);
                    if(tail)
                    { ::uwvm2::validation::standard::wasm3::require_tail_call_enabled(!wasm1p1_para.disable_tail_call, 0x15u, op_begin, err); }
                    // [call_ref/return_call_ref] typeidx ... code_end
                    // [safe                    ] unsafe (could be code_end)
                    //                            ^^ code_curr: bounded scanner owns the u32 advance.
                    auto const type_index{read_leb128_immediate<wasm_u32>(
                        code_curr, code_end, op_begin, code_validation_error_code::invalid_type_index, err)};
                    // [opcode][complete typeidx] next ... code_end
                    // [safe                    ] unsafe (could be code_end)
                    //                            ^^ code_curr; the complete immediate is now validated.
                    auto const types_begin{curr_module.type_section_storage.type_section_begin};
                    auto const types_end{curr_module.type_section_storage.type_section_end};
                    auto const type_count{types_begin == nullptr || types_end == nullptr ? 0uz :
                        static_cast<::std::size_t>(types_end - types_begin)};
                    if(static_cast<::std::size_t>(type_index) >= type_count) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_type_index.type_index = type_index;
                        err.err_selectable.illegal_type_index.all_type_count = static_cast<wasm_u32>(type_count);
                        err.err_code = code_validation_error_code::illegal_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    ::uwvm2::validation::standard::wasm3::require_core3_function_type_index_policy(
                        curr_module.type_section_storage.core3_context_ptr, static_cast<::std::size_t>(type_index),
                        op_begin, tail ? ::uwvm2::utils::container::u8string_view{u8"return_call_ref"} :
                                         ::uwvm2::utils::container::u8string_view{u8"call_ref"}, err);
                    return;
                }
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wswitch" // Tail-call opcode extends the shared wasm1 enum.
#endif
                case static_cast<wasm1_code>(0x13u):
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
                    ::uwvm2::validation::standard::wasm3::require_tail_call_enabled(!wasm1p1_para.disable_tail_call, 0x13u, op_begin, err);
                    [[fallthrough]];
                case wasm1_code::call_indirect:
                {
                    // call_indirect type_index table_index ...
                    // [    safe   ] unsafe (could be the section_end)
                    //               ^^ code_curr

                    auto const type_index{
                        read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_type_index, err)};

                    // call_indirect type_index table_index ...
                    // [          safe        ] unsafe (could be the section_end)
                    //                          ^^ code_curr

                    auto const mvp_reserved_zero_byte{
                        curr_opbase != static_cast<wasm1_code>(0x13u) &&
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::uses_mvp_call_indirect_reserved_byte(wasm1p1_para)};
                    wasm_u32 table_index{};
                    if(!::uwvm2::parser::wasm::standard::wasm1p1::features::parse_call_indirect_trailing_immediate(
                           code_curr, code_end, mvp_reserved_zero_byte, table_index)) [[unlikely]]
                    {
                        // The transactional decoder leaves code_curr at the trailing-immediate start on failure.
                        // The splitter has already consumed the opcode and type index and does not rewind the whole instruction.
                        fail_lazy_split(op_begin, code_validation_error_code::invalid_table_index, err);
                    }

                    // call_indirect type_index table_index ...
                    // [                safe              ] unsafe (could be the section_end)
                    //                                      ^^ code_curr

                    // Both immediate fields are now completely decoded.  Match eager uwvm-int/LLVM/AOT for compound-invalid
                    // operands: type bounds precede the multiple-tables feature gate; table cardinality remains the next check in
                    // whole-function materialization.  Every semantic failure still identifies the opcode through err_curr.
                    auto const types_begin{curr_module.type_section_storage.type_section_begin};
                    auto const types_end{curr_module.type_section_storage.type_section_end};
                    auto const all_type_count_uz{
                        (types_begin == nullptr || types_end == nullptr) ? 0uz : static_cast<::std::size_t>(types_end - types_begin)};
                    if(static_cast<::std::size_t>(type_index) >= all_type_count_uz) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_type_index.type_index = type_index;
                        err.err_selectable.illegal_type_index.all_type_count = static_cast<wasm_u32>(all_type_count_uz);
                        err.err_code = code_validation_error_code::illegal_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    ::uwvm2::validation::standard::wasm3::require_core3_function_type_index_policy(
                        curr_module.type_section_storage.core3_context_ptr, static_cast<::std::size_t>(type_index),
                        op_begin, curr_opbase == static_cast<wasm1_code>(0x13u) ?
                            ::uwvm2::utils::container::u8string_view{u8"return_call_indirect"} :
                            ::uwvm2::utils::container::u8string_view{u8"call_indirect"}, err);

                    // Reference Types/Core 2.0 keep the u32 grammar even when policy restricts the decoded value to zero.
                    if(!mvp_reserved_zero_byte && !wasm2_feature_enabled(wasm2_feature_kind::multiple_tables) && table_index != 0u)
                        [[unlikely]]
                    {
                        fail_lazy_wasm2_feature_required(
                            op_begin,
                            err,
                            static_cast<wasm_u32>(static_cast<wasm_byte>(wasm1_code::call_indirect)),
                            ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables,
                            ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
                    }

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
                    // [opcode] memarg ... (code_end); bounded structural scan, typed validation follows materialization.
                    ::uwvm2::validation::standard::wasm3::require_memory_immediate(
                        ::uwvm2::validation::standard::wasm3::scan_memory_argument64(code_curr, code_end, !wasm1p1_para.disable_multi_memory, ::uwvm2::validation::standard::wasm3::uses_core3_validation_policy(wasm1p1_para)).error, op_begin, err);
                    // [opcode memarg] ... unsafe (could be code_end); code_curr follows the complete memarg.
                    return;
                }
                case wasm1_code::memory_size:
                case wasm1_code::memory_grow:
                {
                    (void)::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err);
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
                    switch(static_cast<wasm_byte>(op_byte))
                    {
                        case static_cast<wasm_byte>(wasm1p1_code::select_t):
                        {
                            if(!wasm2_feature_enabled(wasm2_feature_kind::reference_types)) [[unlikely]]
                            {
                                fail_lazy_feature_required(op_begin,
                                                           err,
                                                           opcode_u32(wasm1p1_code::select_t),
                                                           ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                           ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            auto const result_type_count{
                                read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_const_immediate, err)};

                            // select_t result_type_count result_type ...
                            // [           safe         ] unsafe (could be the section_end)
                            //                            ^^ code_curr

                            if(result_type_count != 1u) [[unlikely]]
                            {
                                fail_lazy_split(op_begin, code_validation_error_code::invalid_const_immediate, err);
                            }
                            auto const type_begin{curr_module.type_section_storage.type_section_begin};
                            auto const type_end{curr_module.type_section_storage.type_section_end};
                            // [type_begin, type_end) is one retained module allocation, or an empty pair.
                            // [safe                ] avoid subtracting two null endpoints.
                            auto const type_count{type_begin == type_end ? 0uz : static_cast<::std::size_t>(type_end - type_begin)};
                            ::uwvm2::validation::standard::wasm3::recursive_type_context const empty_context{};
                            // The runtime borrows this validated context for the module lifetime; no
                            // type-section or code cursor moves while selecting the fallback.
                            auto const* const retained_context{curr_module.type_section_storage.core3_context_ptr};
                            auto const& context{retained_context == nullptr ? empty_context : *retained_context};
                            auto const result_type_byte{static_cast<wasm_byte>(::uwvm2::validation::standard::wasm3::read_core3_value_carrier(
                                code_curr, code_end, !wasm1p1_para.disable_function_references, op_begin, err,
                                type_count, nullptr, context, !wasm1p1_para.disable_gc,
                                !wasm1p1_para.disable_exceptions))};

                            // select_t result_type_count result_type ...
                            // [                 safe               ] unsafe (could be the section_end)
                            //                                        ^^ code_curr

                            // Field commits are transactional: a count-LEB decode failure leaves the cursor after the opcode;
                            // a decoded-count arity rejection or type decode failure leaves it after the count, and a
                            // type-policy rejection follows the complete value encoding.
                            // The complete Core 3 decoder checked exn/noexn and exceptions;
                            // select_t checked reference-types above. The legacy enum has no 0x69.
                            if(result_type_byte != 0x69u)
                            {
                                ensure_lazy_wasm1p1_value_type_enabled(op_begin,
                                                                        result_type_byte,
                                                                        wasm_feature_parameter,
                                                                        ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction,
                                                                        err);
                            }
                            return;
                        }
                        case static_cast<wasm_byte>(wasm1p1_code::table_get):
                        case static_cast<wasm_byte>(wasm1p1_code::table_set):
                        {
                            if(!wasm2_feature_enabled(wasm2_feature_kind::table_instructions)) [[unlikely]]
                            {
                                fail_lazy_wasm2_feature_required(op_begin,
                                                                 err,
                                                                 static_cast<::std::uint_least32_t>(op_byte),
                                                                 ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                                                 ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
                            }
                            (void)read_table_index(static_cast<wasm_u32>(op_byte));
                            return;
                        }
                        case static_cast<wasm_byte>(wasm1p1_code::i32_extend8_s):
                        case static_cast<wasm_byte>(wasm1p1_code::i32_extend16_s):
                        case static_cast<wasm_byte>(wasm1p1_code::i64_extend8_s):
                        case static_cast<wasm_byte>(wasm1p1_code::i64_extend16_s):
                        case static_cast<wasm_byte>(wasm1p1_code::i64_extend32_s):
                        {
                            if(!wasm2_feature_enabled(wasm2_feature_kind::sign_extension)) [[unlikely]]
                            {
                                fail_lazy_feature_required(op_begin,
                                                           err,
                                                           static_cast<::std::uint_least32_t>(op_byte),
                                                           ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                                           ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            return;
                        }
                        case static_cast<wasm_byte>(wasm1p1_code::ref_null):
                        {
                            if(!wasm2_feature_enabled(wasm2_feature_kind::reference_types)) [[unlikely]]
                            {
                                fail_lazy_feature_required(op_begin,
                                                           err,
                                                           opcode_u32(wasm1p1_code::ref_null),
                                                           ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                           ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null);
                            }
                            // Both endpoints borrow the same initializer-owned type array; equal/null endpoints mean no types.
                            auto const type_begin{curr_module.type_section_storage.type_section_begin};
                            auto const type_end{curr_module.type_section_storage.type_section_end};
                            auto const type_count{type_begin == type_end ? 0uz : static_cast<::std::size_t>(type_end - type_begin)};
                            auto const* retained_context{curr_module.type_section_storage.core3_context_ptr};
                            if(retained_context != nullptr || !wasm1p1_para.disable_gc)
                            {
                                ::uwvm2::validation::standard::wasm3::recursive_type_context const empty_context{};
                                auto const& context{retained_context == nullptr ? empty_context : *retained_context};
                                auto const decoded{::uwvm2::validation::standard::wasm3::scan_core3_ref_null_heap(
                                    code_curr, code_end, context, !wasm1p1_para.disable_gc,
                                    !wasm1p1_para.disable_function_references, !wasm1p1_para.disable_exceptions)};
                                // [ref.null][checked signed-33 heap] next ... code_end
                                // [safe                            ] unsafe (could be code_end)
                                //                                  ^^ code_curr: scanner commits only a complete enabled heap.
                                using error = ::uwvm2::validation::standard::wasm3::core3_ref_null_error;
                                if(decoded.error == error::gc_disabled) [[unlikely]]
                                { fail_lazy_feature_required(op_begin, err, opcode_u32(wasm1p1_code::ref_null),
                                    ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
                                    ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction); }
                                if(decoded.error == error::function_references_disabled) [[unlikely]]
                                { ::uwvm2::validation::standard::wasm3::require_function_references_enabled(
                                    false, 0xd0u, op_begin, err); }
                                if(decoded.error == error::exceptions_disabled) [[unlikely]]
                                { ::uwvm2::validation::standard::wasm3::require_exceptions_enabled(
                                    false, 0xd0u, op_begin, err); }
                                if(decoded.error != error::ok) [[unlikely]]
                                {
                                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                    err.err_curr = op_begin; // Borrowed checked opcode; no pointer movement.
                                    err.err_selectable.wasm1p1_invalid_reference_type.value =
                                        code_curr == code_end ? 0u : ::std::to_integer<unsigned>(*code_curr);
                                    err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                }
                                return;
                            }
                            auto const rt_byte{::uwvm2::validation::standard::wasm3::read_function_ref_null_carrier(
                                code_curr, code_end, !wasm1p1_para.disable_function_references, type_count, op_begin, err, true, !wasm1p1_para.disable_gc)};
                            if(rt_byte == 0x69u)
                            {
                                ::uwvm2::validation::standard::wasm3::require_exceptions_enabled(
                                    !wasm1p1_para.disable_exceptions, 0xd0u, op_begin, err);
                                // [ref.null][checked exn/noexn heap] next ... code_end
                                // [safe                             ] unsafe (could be code_end)
                                //                                     ^^ code_curr: whole-function compiler validates
                                // the required adjacent throw_ref and emits the null trap.
                                return;
                            }
                            auto const rt{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type>(rt_byte)};
                            using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
                            if(rt != reference_type::funcref && rt != reference_type::externref) [[unlikely]]
                            {
                                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                err.err_curr = op_begin;
                                err.err_selectable.wasm1p1_invalid_reference_type.value = rt_byte;
                                err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                            ensure_lazy_wasm1p1_value_type_enabled(
                                op_begin,
                                static_cast<wasm_byte>(::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(rt)),
                                wasm_feature_parameter,
                                ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type,
                                err);
                            return;
                        }
                        case static_cast<wasm_byte>(0xd3u): // Core 3 ref.eq has no immediate.
                        {
                            if(wasm1p1_para.disable_gc) [[unlikely]]
                            { fail_lazy_feature_required(op_begin, err, 0xd3u,
                                ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
                                ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction); }
                            // [ref.eq] next ... code_end; outer splitter already consumed the checked byte.
                            // [safe  ] unsafe (could be code_end)
                            //          ^^ code_curr: no immediate, no pointer movement here.
                            return;
                        }
                        case static_cast<wasm_byte>(0xd5u):
                        case static_cast<wasm_byte>(0xd6u):
                        {
                            // [branch opcode] labelidx ... code_end
                            // [safe         ] unsafe (could be code_end)
                            //                 ^^ code_curr: checked by bounded decoder before any advance.
                            ::uwvm2::validation::standard::wasm3::require_function_references_enabled(
                                !wasm1p1_para.disable_function_references, static_cast<unsigned>(op_byte), op_begin, err);
                            (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::invalid_label_index, err);
                            // [branch opcode][labelidx] next ... code_end
                            // [safe                  ] unsafe (could be code_end)
                            //                          ^^ code_curr: decoder owns/annotates the bounded advance.
                            return;
                        }
                        case static_cast<wasm_byte>(0xd4u):
                        {
                            // [ref.as_non_null] next opcode ... end
                            // [safe           ] unsafe (could be end)
                            //                   ^^ code_curr: outer scanner consumed the checked opcode; no immediate.
                            ::uwvm2::validation::standard::wasm3::require_function_references_enabled(
                                !wasm1p1_para.disable_function_references, 0xd4u, op_begin, err);
                            return;
                        }

                        case static_cast<wasm_byte>(wasm1p1_code::ref_is_null):
                        {
                            if(!wasm2_feature_enabled(wasm2_feature_kind::reference_types)) [[unlikely]]
                            {
                                fail_lazy_feature_required(op_begin,
                                                           err,
                                                           opcode_u32(wasm1p1_code::ref_is_null),
                                                           ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                           ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type);
                            }
                            return;
                        }
                        case static_cast<wasm_byte>(wasm1p1_code::ref_func):
                        {
                            if(!wasm2_feature_enabled(wasm2_feature_kind::reference_types)) [[unlikely]]
                            {
                                fail_lazy_feature_required(op_begin,
                                                           err,
                                                           opcode_u32(wasm1p1_code::ref_func),
                                                           ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                                           ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func);
                            }
                            (void)read_leb128_immediate<wasm_u32>(code_curr,
                                                                   code_end,
                                                                   op_begin,
                                                                   code_validation_error_code::invalid_function_index_encoding,
                                                                   err);
                            return;
                        }
                        case static_cast<wasm_byte>(wasm1p1_code::atomic_prefix):
                        {
                            // [FE] subopcode reserved ... (code_end)
                            // [safe] unsafe (could be code_end)
                            //        ^^ code_curr: the outer scanner already consumed the checked prefix.
                            auto const memory_count{curr_module.imported_memory_vec_storage.size() + curr_module.local_defined_memory_vec_storage.size()};
                            auto const address_type_at{[&](::std::uint_least32_t index) constexpr noexcept
                            {
                                // The decoder bounds-checks the combined index before
                                // this resolver borrows imported/local declaration metadata.
                                return ::uwvm2::uwvm::runtime::storage::runtime_memory_is_address64(curr_module, index) ?
                                    ::uwvm2::validation::standard::wasm3::storage_address_type::i64 :
                                    ::uwvm2::validation::standard::wasm3::storage_address_type::i32;
                            }};
                            (void)::uwvm2::validation::standard::wasm3::read_atomic_instruction64(
                                code_curr, code_end, op_begin, !wasm1p1_para.disable_threads,
                                !wasm1p1_para.disable_multi_memory, ::uwvm2::validation::standard::wasm3::uses_core3_validation_policy(wasm1p1_para),
                                memory_count, address_type_at, err);
                            // [FE subopcode validated immediates] ... (code_end)
                            // [safe                            ] unsafe (could be code_end)
                            //                                    ^^ code_curr; bounded atomic immediates add no structural nesting.
                            return;
                        }
                        case static_cast<wasm_byte>(0xfbu):
                        {
                            // [FB] subopcode/immediate ... code_end
                            // [safe] unsafe (could be code_end)
                            //        ^^ code_curr: outer scanner already consumed the checked prefix.
                            if(wasm1p1_para.disable_gc) [[unlikely]]
                            {
                                fail_lazy_feature_required(op_begin, err, 0xfbu,
                                    ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
                                    ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                            }
                            auto const decoded{::uwvm2::validation::standard::wasm3::scan_gc_instruction(code_curr, code_end)};
                            if(decoded.error != ::uwvm2::validation::standard::wasm3::gc_immediate_error::ok) [[unlikely]]
                            { fail_lazy_split(op_begin, code_validation_error_code::invalid_const_immediate, err); }
                            // [FB][checked complete immediate] next ... code_end
                            // [safe                          ] unsafe (could be code_end)
                            //                                 ^^ code_curr: bounded scanner committed the advance.
                            return;
                        }
                        case static_cast<wasm_byte>(wasm1p1_code::numeric_prefix):
                        {
                            auto const subopcode{
                                read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::illegal_opbase, err)};
                            auto const numeric_code{static_cast<wasm1p1_numeric_code>(subopcode)};
                            switch(numeric_code)
                            {
                                case wasm1p1_numeric_code::i32_trunc_sat_f32_s:
                                case wasm1p1_numeric_code::i32_trunc_sat_f32_u:
                                case wasm1p1_numeric_code::i32_trunc_sat_f64_s:
                                case wasm1p1_numeric_code::i32_trunc_sat_f64_u:
                                case wasm1p1_numeric_code::i64_trunc_sat_f32_s:
                                case wasm1p1_numeric_code::i64_trunc_sat_f32_u:
                                case wasm1p1_numeric_code::i64_trunc_sat_f64_s:
                                case wasm1p1_numeric_code::i64_trunc_sat_f64_u:
                                {
                                    if(!wasm2_feature_enabled(wasm2_feature_kind::nontrapping_float_to_int)) [[unlikely]]
                                    {
                                        fail_lazy_feature_required(op_begin,
                                                                   err,
                                                                   subopcode,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                                    }
                                    return;
                                }
                                case wasm1p1_numeric_code::memory_init:
                                {
                                    if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                                    {
                                        fail_lazy_feature_required(op_begin,
                                                                   err,
                                                                   subopcode,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment);
                                    }
                                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::illegal_data_index, err);
                                    // [opcode] memidx ... (code_end): scanner commits only within the checked byte range.
                                    (void)::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err);
                                    // [opcode memidx] ... unsafe (could be code_end); code_curr follows the complete index.
                                    return;
                                }
                                case wasm1p1_numeric_code::data_drop:
                                {
                                    if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                                    {
                                        fail_lazy_feature_required(op_begin,
                                                                   err,
                                                                   subopcode,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment);
                                    }
                                    (void)read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::illegal_data_index, err);
                                    return;
                                }
                                case wasm1p1_numeric_code::memory_copy:
                                {
                                    if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                                    {
                                        fail_lazy_feature_required(op_begin,
                                                                   err,
                                                                   subopcode,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                                    }
                                    // [opcode] memidx ... (code_end): scanner commits only within the checked byte range.
                                    (void)::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err);
                                    // [opcode memidx] ... unsafe (could be code_end); code_curr follows the complete index.
                                    // [opcode] memidx ... (code_end): scanner commits only within the checked byte range.
                                    (void)::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err);
                                    // [opcode memidx] ... unsafe (could be code_end); code_curr follows the complete index.
                                    return;
                                }
                                case wasm1p1_numeric_code::memory_fill:
                                {
                                    if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                                    {
                                        fail_lazy_feature_required(op_begin,
                                                                   err,
                                                                   subopcode,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                                    }
                                    // [opcode] memidx ... (code_end): scanner commits only within the checked byte range.
                                    (void)::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err);
                                    // [opcode memidx] ... unsafe (could be code_end); code_curr follows the complete index.
                                    return;
                                }
                                case wasm1p1_numeric_code::table_init:
                                {
                                    if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                                    {
                                        fail_lazy_feature_required(op_begin,
                                                                   err,
                                                                   subopcode,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment);
                                    }
                                    (void)read_leb128_immediate<wasm_u32>(code_curr,
                                                                          code_end,
                                                                          op_begin,
                                                                          code_validation_error_code::illegal_element_index,
                                                                          err);
                                    (void)read_table_index(subopcode);
                                    return;
                                }
                                case wasm1p1_numeric_code::elem_drop:
                                {
                                    if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                                    {
                                        fail_lazy_feature_required(op_begin,
                                                                   err,
                                                                   subopcode,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment);
                                    }
                                    (void)read_leb128_immediate<wasm_u32>(code_curr,
                                                                          code_end,
                                                                          op_begin,
                                                                          code_validation_error_code::illegal_element_index,
                                                                          err);
                                    return;
                                }
                                case wasm1p1_numeric_code::table_copy:
                                {
                                    if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                                    {
                                        fail_lazy_feature_required(op_begin,
                                                                   err,
                                                                   subopcode,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                                    }
                                    (void)read_table_index(subopcode);
                                    (void)read_table_index(subopcode);
                                    return;
                                }
                                case wasm1p1_numeric_code::table_grow:
                                case wasm1p1_numeric_code::table_size:
                                {
                                    if(!wasm2_feature_enabled(wasm2_feature_kind::table_instructions)) [[unlikely]]
                                    {
                                        fail_lazy_wasm2_feature_required(op_begin,
                                                                         err,
                                                                         subopcode,
                                                                         ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                                                         ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
                                    }
                                    (void)read_table_index(subopcode);
                                    return;
                                }
                                case wasm1p1_numeric_code::table_fill:
                                {
                                    if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
                                    {
                                        fail_lazy_feature_required(op_begin,
                                                                   err,
                                                                   subopcode,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                                                   ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                                    }
                                    (void)read_table_index(subopcode);
                                    return;
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
                        }
                        case static_cast<wasm_byte>(wasm1p1_code::simd_prefix):
                        {
                            if(!wasm2_feature_enabled(wasm2_feature_kind::simd)) [[unlikely]]
                            {
                                fail_lazy_feature_required(op_begin,
                                                           err,
                                                           opcode_u32(wasm1p1_code::simd_prefix),
                                                           ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd,
                                                           ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const);
                            }

                            auto const simd_subopcode{
                                read_leb128_immediate<wasm_u32>(code_curr, code_end, op_begin, code_validation_error_code::illegal_opbase, err)};
                            // 0xfd subopcode ...
                            // [safe         ] unsafe (could be code_end)
                            //                 ^^ code_curr: bounded read_leb128_immediate consumed u32.
                            if(::uwvm2::validation::standard::wasm3::relaxed_simd_operand_count(simd_subopcode) != 0u)
                            {
                                if(wasm1p1_para.disable_relaxed_simd)
                                {
                                    fail_lazy_feature_required(op_begin, err, simd_subopcode,
                                        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::relaxed_simd,
                                        ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                                }
                                // All 20 relaxed instructions have no trailing immediates. Stack validation
                                // belongs to materialization or the explicit lazy+verification pass.
                                return;
                            }
                            auto const simd_code{static_cast<wasm1p1_simd_code>(simd_subopcode)};
                            auto const skip_simd_memarg{
                                [&]() constexpr UWVM_THROWS
                                {
                                    // [opcode] memarg ... (code_end); bounded structural scan, typed validation follows materialization.
                    ::uwvm2::validation::standard::wasm3::require_memory_immediate(
                        ::uwvm2::validation::standard::wasm3::scan_memory_argument64(code_curr, code_end, !wasm1p1_para.disable_multi_memory, ::uwvm2::validation::standard::wasm3::uses_core3_validation_policy(wasm1p1_para)).error, op_begin, err);
                    // [opcode memarg] ... unsafe (could be code_end); code_curr follows the complete memarg.
                                }};

                            switch(simd_code)
                            {
                                case wasm1p1_simd_code::v128_load:
                                case wasm1p1_simd_code::v128_load8x8_s:
                                case wasm1p1_simd_code::v128_load8x8_u:
                                case wasm1p1_simd_code::v128_load16x4_s:
                                case wasm1p1_simd_code::v128_load16x4_u:
                                case wasm1p1_simd_code::v128_load32x2_s:
                                case wasm1p1_simd_code::v128_load32x2_u:
                                case wasm1p1_simd_code::v128_load8_splat:
                                case wasm1p1_simd_code::v128_load16_splat:
                                case wasm1p1_simd_code::v128_load32_splat:
                                case wasm1p1_simd_code::v128_load64_splat:
                                case wasm1p1_simd_code::v128_store:
                                case wasm1p1_simd_code::v128_load32_zero:
                                case wasm1p1_simd_code::v128_load64_zero:
                                {
                                    skip_simd_memarg();
                                    return;
                                }
                                case wasm1p1_simd_code::v128_load8_lane:
                                case wasm1p1_simd_code::v128_load16_lane:
                                case wasm1p1_simd_code::v128_load32_lane:
                                case wasm1p1_simd_code::v128_load64_lane:
                                case wasm1p1_simd_code::v128_store8_lane:
                                case wasm1p1_simd_code::v128_store16_lane:
                                case wasm1p1_simd_code::v128_store32_lane:
                                case wasm1p1_simd_code::v128_store64_lane:
                                {
                                    skip_simd_memarg();
                                    (void)read_u8_immediate(code_curr, code_end, op_begin, err);
                                    return;
                                }
                                case wasm1p1_simd_code::v128_const:
                                {
                                    skip_fixed_immediate(code_curr, code_end, op_begin, 16uz, err);
                                    return;
                                }
                                case wasm1p1_simd_code::i8x16_shuffle:
                                {
                                    skip_fixed_immediate(code_curr, code_end, op_begin, 16uz, err);
                                    return;
                                }
                                case wasm1p1_simd_code::i8x16_extract_lane_s:
                                case wasm1p1_simd_code::i8x16_extract_lane_u:
                                case wasm1p1_simd_code::i8x16_replace_lane:
                                case wasm1p1_simd_code::i16x8_extract_lane_s:
                                case wasm1p1_simd_code::i16x8_extract_lane_u:
                                case wasm1p1_simd_code::i16x8_replace_lane:
                                case wasm1p1_simd_code::i32x4_extract_lane:
                                case wasm1p1_simd_code::i32x4_replace_lane:
                                case wasm1p1_simd_code::i64x2_extract_lane:
                                case wasm1p1_simd_code::i64x2_replace_lane:
                                case wasm1p1_simd_code::f32x4_extract_lane:
                                case wasm1p1_simd_code::f32x4_replace_lane:
                                case wasm1p1_simd_code::f64x2_extract_lane:
                                case wasm1p1_simd_code::f64x2_replace_lane:
                                {
                                    (void)read_u8_immediate(code_curr, code_end, op_begin, err);
                                    return;
                                }
                                case wasm1p1_simd_code::i8x16_swizzle:
                                case wasm1p1_simd_code::i8x16_splat:
                                case wasm1p1_simd_code::i16x8_splat:
                                case wasm1p1_simd_code::i32x4_splat:
                                case wasm1p1_simd_code::i64x2_splat:
                                case wasm1p1_simd_code::f32x4_splat:
                                case wasm1p1_simd_code::f64x2_splat:
                                case wasm1p1_simd_code::i8x16_eq:
                                case wasm1p1_simd_code::i8x16_ne:
                                case wasm1p1_simd_code::i8x16_lt_s:
                                case wasm1p1_simd_code::i8x16_lt_u:
                                case wasm1p1_simd_code::i8x16_gt_s:
                                case wasm1p1_simd_code::i8x16_gt_u:
                                case wasm1p1_simd_code::i8x16_le_s:
                                case wasm1p1_simd_code::i8x16_le_u:
                                case wasm1p1_simd_code::i8x16_ge_s:
                                case wasm1p1_simd_code::i8x16_ge_u:
                                case wasm1p1_simd_code::i16x8_eq:
                                case wasm1p1_simd_code::i16x8_ne:
                                case wasm1p1_simd_code::i16x8_lt_s:
                                case wasm1p1_simd_code::i16x8_lt_u:
                                case wasm1p1_simd_code::i16x8_gt_s:
                                case wasm1p1_simd_code::i16x8_gt_u:
                                case wasm1p1_simd_code::i16x8_le_s:
                                case wasm1p1_simd_code::i16x8_le_u:
                                case wasm1p1_simd_code::i16x8_ge_s:
                                case wasm1p1_simd_code::i16x8_ge_u:
                                case wasm1p1_simd_code::i32x4_eq:
                                case wasm1p1_simd_code::i32x4_ne:
                                case wasm1p1_simd_code::i32x4_lt_s:
                                case wasm1p1_simd_code::i32x4_lt_u:
                                case wasm1p1_simd_code::i32x4_gt_s:
                                case wasm1p1_simd_code::i32x4_gt_u:
                                case wasm1p1_simd_code::i32x4_le_s:
                                case wasm1p1_simd_code::i32x4_le_u:
                                case wasm1p1_simd_code::i32x4_ge_s:
                                case wasm1p1_simd_code::i32x4_ge_u:
                                case wasm1p1_simd_code::f32x4_eq:
                                case wasm1p1_simd_code::f32x4_ne:
                                case wasm1p1_simd_code::f32x4_lt:
                                case wasm1p1_simd_code::f32x4_gt:
                                case wasm1p1_simd_code::f32x4_le:
                                case wasm1p1_simd_code::f32x4_ge:
                                case wasm1p1_simd_code::f64x2_eq:
                                case wasm1p1_simd_code::f64x2_ne:
                                case wasm1p1_simd_code::f64x2_lt:
                                case wasm1p1_simd_code::f64x2_gt:
                                case wasm1p1_simd_code::f64x2_le:
                                case wasm1p1_simd_code::f64x2_ge:
                                case wasm1p1_simd_code::v128_not:
                                case wasm1p1_simd_code::v128_and:
                                case wasm1p1_simd_code::v128_andnot:
                                case wasm1p1_simd_code::v128_or:
                                case wasm1p1_simd_code::v128_xor:
                                case wasm1p1_simd_code::v128_bitselect:
                                case wasm1p1_simd_code::v128_any_true:
                                case wasm1p1_simd_code::f32x4_demote_f64x2_zero:
                                case wasm1p1_simd_code::f64x2_promote_low_f32x4:
                                case wasm1p1_simd_code::i8x16_abs:
                                case wasm1p1_simd_code::i8x16_neg:
                                case wasm1p1_simd_code::i8x16_popcnt:
                                case wasm1p1_simd_code::i8x16_all_true:
                                case wasm1p1_simd_code::i8x16_bitmask:
                                case wasm1p1_simd_code::i8x16_narrow_i16x8_s:
                                case wasm1p1_simd_code::i8x16_narrow_i16x8_u:
                                case wasm1p1_simd_code::f32x4_ceil:
                                case wasm1p1_simd_code::f32x4_floor:
                                case wasm1p1_simd_code::f32x4_trunc:
                                case wasm1p1_simd_code::f32x4_nearest:
                                case wasm1p1_simd_code::i8x16_shl:
                                case wasm1p1_simd_code::i8x16_shr_s:
                                case wasm1p1_simd_code::i8x16_shr_u:
                                case wasm1p1_simd_code::i8x16_add:
                                case wasm1p1_simd_code::i8x16_add_sat_s:
                                case wasm1p1_simd_code::i8x16_add_sat_u:
                                case wasm1p1_simd_code::i8x16_sub:
                                case wasm1p1_simd_code::i8x16_sub_sat_s:
                                case wasm1p1_simd_code::i8x16_sub_sat_u:
                                case wasm1p1_simd_code::f64x2_ceil:
                                case wasm1p1_simd_code::f64x2_floor:
                                case wasm1p1_simd_code::i8x16_min_s:
                                case wasm1p1_simd_code::i8x16_min_u:
                                case wasm1p1_simd_code::i8x16_max_s:
                                case wasm1p1_simd_code::i8x16_max_u:
                                case wasm1p1_simd_code::f64x2_trunc:
                                case wasm1p1_simd_code::i8x16_avgr_u:
                                case wasm1p1_simd_code::i16x8_extadd_pairwise_i8x16_s:
                                case wasm1p1_simd_code::i16x8_extadd_pairwise_i8x16_u:
                                case wasm1p1_simd_code::i32x4_extadd_pairwise_i16x8_s:
                                case wasm1p1_simd_code::i32x4_extadd_pairwise_i16x8_u:
                                case wasm1p1_simd_code::i16x8_abs:
                                case wasm1p1_simd_code::i16x8_neg:
                                case wasm1p1_simd_code::i16x8_q15mulr_sat_s:
                                case wasm1p1_simd_code::i16x8_all_true:
                                case wasm1p1_simd_code::i16x8_bitmask:
                                case wasm1p1_simd_code::i16x8_narrow_i32x4_s:
                                case wasm1p1_simd_code::i16x8_narrow_i32x4_u:
                                case wasm1p1_simd_code::i16x8_extend_low_i8x16_s:
                                case wasm1p1_simd_code::i16x8_extend_high_i8x16_s:
                                case wasm1p1_simd_code::i16x8_extend_low_i8x16_u:
                                case wasm1p1_simd_code::i16x8_extend_high_i8x16_u:
                                case wasm1p1_simd_code::i16x8_shl:
                                case wasm1p1_simd_code::i16x8_shr_s:
                                case wasm1p1_simd_code::i16x8_shr_u:
                                case wasm1p1_simd_code::i16x8_add:
                                case wasm1p1_simd_code::i16x8_add_sat_s:
                                case wasm1p1_simd_code::i16x8_add_sat_u:
                                case wasm1p1_simd_code::i16x8_sub:
                                case wasm1p1_simd_code::i16x8_sub_sat_s:
                                case wasm1p1_simd_code::i16x8_sub_sat_u:
                                case wasm1p1_simd_code::f64x2_nearest:
                                case wasm1p1_simd_code::i16x8_mul:
                                case wasm1p1_simd_code::i16x8_min_s:
                                case wasm1p1_simd_code::i16x8_min_u:
                                case wasm1p1_simd_code::i16x8_max_s:
                                case wasm1p1_simd_code::i16x8_max_u:
                                case wasm1p1_simd_code::i16x8_avgr_u:
                                case wasm1p1_simd_code::i16x8_extmul_low_i8x16_s:
                                case wasm1p1_simd_code::i16x8_extmul_high_i8x16_s:
                                case wasm1p1_simd_code::i16x8_extmul_low_i8x16_u:
                                case wasm1p1_simd_code::i16x8_extmul_high_i8x16_u:
                                case wasm1p1_simd_code::i32x4_abs:
                                case wasm1p1_simd_code::i32x4_neg:
                                case wasm1p1_simd_code::i32x4_all_true:
                                case wasm1p1_simd_code::i32x4_bitmask:
                                case wasm1p1_simd_code::i32x4_extend_low_i16x8_s:
                                case wasm1p1_simd_code::i32x4_extend_high_i16x8_s:
                                case wasm1p1_simd_code::i32x4_extend_low_i16x8_u:
                                case wasm1p1_simd_code::i32x4_extend_high_i16x8_u:
                                case wasm1p1_simd_code::i32x4_shl:
                                case wasm1p1_simd_code::i32x4_shr_s:
                                case wasm1p1_simd_code::i32x4_shr_u:
                                case wasm1p1_simd_code::i32x4_add:
                                case wasm1p1_simd_code::i32x4_sub:
                                case wasm1p1_simd_code::i32x4_mul:
                                case wasm1p1_simd_code::i32x4_min_s:
                                case wasm1p1_simd_code::i32x4_min_u:
                                case wasm1p1_simd_code::i32x4_max_s:
                                case wasm1p1_simd_code::i32x4_max_u:
                                case wasm1p1_simd_code::i32x4_dot_i16x8_s:
                                case wasm1p1_simd_code::i32x4_extmul_low_i16x8_s:
                                case wasm1p1_simd_code::i32x4_extmul_high_i16x8_s:
                                case wasm1p1_simd_code::i32x4_extmul_low_i16x8_u:
                                case wasm1p1_simd_code::i32x4_extmul_high_i16x8_u:
                                case wasm1p1_simd_code::i64x2_abs:
                                case wasm1p1_simd_code::i64x2_neg:
                                case wasm1p1_simd_code::i64x2_all_true:
                                case wasm1p1_simd_code::i64x2_bitmask:
                                case wasm1p1_simd_code::i64x2_extend_low_i32x4_s:
                                case wasm1p1_simd_code::i64x2_extend_high_i32x4_s:
                                case wasm1p1_simd_code::i64x2_extend_low_i32x4_u:
                                case wasm1p1_simd_code::i64x2_extend_high_i32x4_u:
                                case wasm1p1_simd_code::i64x2_shl:
                                case wasm1p1_simd_code::i64x2_shr_s:
                                case wasm1p1_simd_code::i64x2_shr_u:
                                case wasm1p1_simd_code::i64x2_add:
                                case wasm1p1_simd_code::i64x2_sub:
                                case wasm1p1_simd_code::i64x2_mul:
                                case wasm1p1_simd_code::i64x2_eq:
                                case wasm1p1_simd_code::i64x2_ne:
                                case wasm1p1_simd_code::i64x2_lt_s:
                                case wasm1p1_simd_code::i64x2_gt_s:
                                case wasm1p1_simd_code::i64x2_le_s:
                                case wasm1p1_simd_code::i64x2_ge_s:
                                case wasm1p1_simd_code::i64x2_extmul_low_i32x4_s:
                                case wasm1p1_simd_code::i64x2_extmul_high_i32x4_s:
                                case wasm1p1_simd_code::i64x2_extmul_low_i32x4_u:
                                case wasm1p1_simd_code::i64x2_extmul_high_i32x4_u:
                                case wasm1p1_simd_code::f32x4_abs:
                                case wasm1p1_simd_code::f32x4_neg:
                                case wasm1p1_simd_code::f32x4_sqrt:
                                case wasm1p1_simd_code::f32x4_add:
                                case wasm1p1_simd_code::f32x4_sub:
                                case wasm1p1_simd_code::f32x4_mul:
                                case wasm1p1_simd_code::f32x4_div:
                                case wasm1p1_simd_code::f32x4_min:
                                case wasm1p1_simd_code::f32x4_max:
                                case wasm1p1_simd_code::f32x4_pmin:
                                case wasm1p1_simd_code::f32x4_pmax:
                                case wasm1p1_simd_code::f64x2_abs:
                                case wasm1p1_simd_code::f64x2_neg:
                                case wasm1p1_simd_code::f64x2_sqrt:
                                case wasm1p1_simd_code::f64x2_add:
                                case wasm1p1_simd_code::f64x2_sub:
                                case wasm1p1_simd_code::f64x2_mul:
                                case wasm1p1_simd_code::f64x2_div:
                                case wasm1p1_simd_code::f64x2_min:
                                case wasm1p1_simd_code::f64x2_max:
                                case wasm1p1_simd_code::f64x2_pmin:
                                case wasm1p1_simd_code::f64x2_pmax:
                                case wasm1p1_simd_code::i32x4_trunc_sat_f32x4_s:
                                case wasm1p1_simd_code::i32x4_trunc_sat_f32x4_u:
                                case wasm1p1_simd_code::f32x4_convert_i32x4_s:
                                case wasm1p1_simd_code::f32x4_convert_i32x4_u:
                                case wasm1p1_simd_code::i32x4_trunc_sat_f64x2_s_zero:
                                case wasm1p1_simd_code::i32x4_trunc_sat_f64x2_u_zero:
                                case wasm1p1_simd_code::f64x2_convert_low_i32x4_s:
                                case wasm1p1_simd_code::f64x2_convert_low_i32x4_u:
                                {
                                    return;
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
                        }
                    }

                    if((op_byte >= static_cast<unsigned>(wasm1_code::i32_eqz) && op_byte <= static_cast<unsigned>(wasm1_code::f64_reinterpret_i64)) ||
                       curr_opbase == wasm1_code::unreachable || curr_opbase == wasm1_code::nop || curr_opbase == wasm1_code::drop ||
                       curr_opbase == wasm1_code::select || curr_opbase == wasm1_code::return_)
                    {
                        // MVP numeric and simple stack/control opcodes carry no immediates that matter to structural splitting.
                        return;
                    }

                    // Unknown opcodes are rejected early because a wrong skip length would corrupt every later structural boundary.
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.u8 = static_cast<::std::uint_least8_t>(op_byte);
                    err.err_code = code_validation_error_code::illegal_opbase;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }
        }

        inline constexpr void build_lazy_function_execution_units(runtime_module_storage_t const& curr_module,
                                                                  lazy_module_storage_t& storage,
                                                                  ::std::size_t local_function_index,
                                                                  lazy_split_config cfg,
                                                                  parser_feature_parameter_t const& wasm_feature_parameter,
                                                                  ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            // Build the lazy index from the runtime module because it owns the final function ordering used by interpreter dispatch.
            auto const import_func_count{curr_module.imported_function_vec_storage.size()};
            auto const function_index{import_func_count + local_function_index};
            auto const& curr_local_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_index)};
            auto const& curr_code{*curr_local_func.wasm_code_ptr};
            auto const code_begin{reinterpret_cast<::std::byte const*>(curr_code.body.expr_begin)};
            auto const code_end{reinterpret_cast<::std::byte const*>(curr_code.body.code_end)};
            auto const& policy{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(wasm_feature_parameter)};
            auto const* declaration_context{policy.disable_gc ?
                ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::runtime_declaration_type_context(curr_module) : nullptr};
            ::uwvm2::validation::standard::wasm3::require_function_declaration_policy(*curr_local_func.function_type_ptr, curr_code.locals,
                curr_module.type_section_storage.requires_function_references, policy, code_begin, err,
        curr_module.table_declarations_require_function_references, curr_module.global_declarations_require_function_references,
        curr_module.element_declarations_require_function_references, curr_module.tag_section_present || !curr_module.imported_tag_vec_storage.empty(),
                declaration_context, curr_module.type_section_storage.type_section_count,
                curr_module.table_declarations_require_gc, curr_module.global_declarations_require_gc, curr_module.element_declarations_require_gc,
                ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::runtime_exception_declaration_requirements(curr_module),
                {.simd = curr_module.type_section_storage.requires_simd,
                 .reference_types = curr_module.type_section_storage.requires_reference_types,
                 .multi_value = curr_module.type_section_storage.requires_multi_value},
                ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::runtime_storage_declaration_requirements(curr_module),
                ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::runtime_address_declaration_requirements(curr_module),
                ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::runtime_constant_declaration_requirements(curr_module));

            auto& fn{storage.functions.index_unchecked(local_function_index)};
            fn.function_index = function_index;
            fn.local_function_index = local_function_index;
            fn.first_eu_index = storage.execution_units.size();

            // The root execution unit covers the entire function and gives every function a legal fallback materialization range.
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
                // Function-only splitting avoids the structural scan cost while preserving the same lazy compilation interface.
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

                // Read the opcode byte with memcpy to avoid aliasing and alignment assumptions on the wasm byte stream.
                wasm1_code curr_opbase;  // No initialization necessary
                ::std::memcpy(::std::addressof(curr_opbase), code_curr, sizeof(wasm1_code));
                // opcode ... section_end
                // [safe] unsafe (could be section_end)
                // ^^ code_curr: loop entry proved a complete opcode byte.
                ++code_curr;
                // opcode ... section_end
                // [safe] unsafe (could be section_end)
                //        ^^ code_curr may be one-past.

                /// @warning Extension point: structural opcode additions must update this lazy execution-unit scanner.
                switch(curr_opbase)
                {
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wswitch" // Core 3 try_table extends the shared wasm1 enum.
#endif
                    case static_cast<wasm1_code>(0x1f): // Core 3 try_table
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
                    case wasm1_code::block:
                    case wasm1_code::loop:
                    case wasm1_code::if_:
                    {
                        // block blocktype ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        if(curr_opbase == static_cast<wasm1_code>(0x1f))
                        { ::uwvm2::validation::standard::wasm3::require_exceptions_enabled(
                            !::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(wasm_feature_parameter).disable_exceptions,
                            0x1fu, op_begin, err); }
                        skip_wasm1p1_block_type(code_curr, code_end, op_begin, curr_module, wasm_feature_parameter, err);
                        if(curr_opbase == static_cast<wasm1_code>(0x1f))
                        {
                            auto const catches{::uwvm2::validation::standard::wasm3::scan_exception_catches(code_curr, code_end)};
                            // [try_table blocktype checked vector] next ... code_end
                            // [safe                             ] unsafe (could be code_end)
                            //                                     ^^ code_curr: complete bounded scan, unchanged on failure.
                            if(catches.error != ::uwvm2::validation::standard::wasm3::exception_immediate_error::ok)
                            { fail_lazy_split(op_begin, code_validation_error_code::illegal_opbase, err); }
                            // [try_table blocktype checked catches] next ... code_end
                            // [safe                              ] unsafe (could be code_end)
                            //                                      ^^ code_curr: checked before any execution-unit publication.
                        }


                        // block blocktype ...
                        // [     safe    ] unsafe (could be the section_end)
                        //                 ^^ code_curr

                        // Opening a structural instruction starts a child execution unit at the opcode itself so the range remains
                        // directly readable in diagnostics and can be re-parsed independently later.
                        auto const parent_eu_index{active_parent_eu_index(control_stack.back_unchecked())};
                        auto const depth{control_stack.size()};
                        auto const kind{(curr_opbase == wasm1_code::block || curr_opbase == static_cast<wasm1_code>(0x1f)) ? lazy_execution_unit_kind::block
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
                            // An else only belongs to the currently open if; catching this here prevents an invalid split tree.
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
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(curr_opbase);
                            err.err_code = code_validation_error_code::illegal_opbase;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const frame{control_stack.back_unchecked()};
                        // Closing the unit at code_curr includes the end opcode, which matches the complete structured expression.
                        set_execution_unit_end(storage, frame.eu_index, code_curr);
                        control_stack.pop_back_unchecked();

                        if(frame.kind == lazy_execution_unit_kind::function)
                        {
                            // The function root closes the wasm body. Anything after that would make offsets ambiguous and invalid.
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
                        skip_wasm1_non_structural_immediates(code_curr, code_end, op_begin, curr_opbase, curr_module, wasm_feature_parameter, err);
                        break;
                    }
                }
            }
        }

        [[nodiscard]] inline constexpr auto
            match_trivial_call_inline_body(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_code_t const* code_ptr) noexcept
        { return ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::match_trivial_call_inline_body(code_ptr); }

        // Precompute local call metadata before any function is materialized. Lazy compilation still needs direct-call targets and
        // trivial inline-call recognition to be available as soon as the first function compiles.
        inline constexpr void fill_lazy_local_defined_call_info(runtime_module_storage_t const& curr_module,
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

// The include fragment fills per-function call information in this lexical scope, reusing the eager compiler's exact logic so lazy
// and eager modes agree on function symbols, inline-call candidates, and metadata layout.
# include "../compile_all_from_uwvm/translate/single_func_call_info.h"
        }

        // The interpreter allocates local and operand-stack storage from module-wide maxima. Recompute those maxima after call-info
        // setup because lazy module initialization does not compile every function immediately.
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

        #include "checked_register_ring_admission.h"

        struct lazy_compile_state_notifier
        {
            inline constexpr void operator()(::uwvm2::utils::thread::lazy_compile_unit_state& unit) const noexcept
            { ::uwvm2::utils::thread::lazy_compile_notify_unit(unit); }
        };

        template <typename Notify = lazy_compile_state_notifier>
        inline constexpr void mark_function_compile_units_state(lazy_module_storage_t& storage,
                                                                lazy_function_storage_t& fn,
                                                                ::uwvm2::utils::thread::lazy_compile_state state,
                                                                Notify notify = {}) noexcept
        {
            static_assert(noexcept(notify(fn.materialization_state)), "lazy terminal-state notifiers must not throw");
            // Publish and wake every compile-unit observer before the authoritative function state.  A waiter that acquires a
            // terminal function state then also observes all sibling mirrors, while explicit notify calls wake Linux atomic::wait
            // users instead of leaving them asleep after a plain atomic store.
            for(::std::size_t i{fn.first_cu_index}; i != fn.first_cu_index + fn.cu_count; ++i)
            {
                auto& cu_state{storage.compile_units.index_unchecked(i).state};
                cu_state.state.store(state, ::std::memory_order_release);
                notify(cu_state);
            }
            fn.materialization_state.state.store(state, ::std::memory_order_release);
            notify(fn.materialization_state);
        }

        [[noreturn]] inline void fail_checked_lazy_lowering(checked_integer_lowering_status result,
                                                            ::std::size_t local_function_index) noexcept
        {
            // Refuse unsupported or retired representations explicitly. A
            // checked-mode request never falls back to raw Wasm decoding.
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                u8"uwvm: ", ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                u8"[fatal] ", ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                u8"Checked lazy integer lowering is unavailable for function ", ::fast_io::mnp::dec(local_function_index),
                u8" (reason=", checked_integer_lowering_status_name(result), u8"). (runtime)\n\n",
                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            ::fast_io::fast_terminate();
        }
        inline void require_current_checked_function(runtime_module_storage_t const& curr_module,
                                                     lazy_module_storage_t const& storage,
                                                     ::std::size_t local_function_index) noexcept
        {
            // This cold request/cache gate runs under the existing worker-drain
            // lifetime contract. Even a compiled cache hit must not report new
            // materialization success for a plan from a retired real generation.
            if(storage.checked_plan && !storage.checked_plan->matches_function_source(curr_module, local_function_index))
            { fail_checked_lazy_lowering(checked_integer_lowering_status::missing_or_retired_source, local_function_index); }
        }

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
        inline constexpr void compile_lazy_local_function(runtime_module_storage_t const& curr_module,
                                                          lazy_module_storage_t& storage,
                                                          lazy_compile_options& options,
                                                          ::std::size_t local_function_index,
                                                          ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            if(storage.checked_plan)
            {
                if(local_function_index >= storage.compiled.local_funcs.size()) { ::fast_io::fast_terminate(); }
                auto const result{lower_checked_integer_function<CompileOption>(storage.checked_plan, curr_module,
                    local_function_index, storage.compiled.local_funcs.index_unchecked(local_function_index))};
                if(result != checked_integer_lowering_status::ok) { fail_checked_lazy_lowering(result, local_function_index); }
                return;
            }
            // The factory already emitted this exact register-ring/fixup/ABI
            // artifact during authoritative admission. First use only publishes
            // readiness; it never decodes or matches the original body again.
            if(storage.ring_admission == nullptr || !storage.ring_admission->matches_function<CompileOption>(
                curr_module, options.compile_options.curr_wasm_id, local_function_index))
            { fail_checked_lazy_lowering(checked_integer_lowering_status::missing_or_retired_source, local_function_index); }

        }

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
        inline constexpr void lazy_compile_request_entry(void* user_data) noexcept
        {
            // Scheduler callbacks are noexcept and receive erased user data, so every pointer/index check must fail closed.
            auto const ctx{static_cast<lazy_compile_request_context*>(user_data)};
            if(ctx == nullptr || ctx->curr_module == nullptr || ctx->lazy_storage == nullptr) [[unlikely]] { return; }

            auto& storage{*ctx->lazy_storage};
            if(ctx->compile_unit_index >= storage.compile_units.size()) [[unlikely]] { return; }

            auto& cu{storage.compile_units.index_unchecked(ctx->compile_unit_index)};
            if(cu.local_function_index >= storage.functions.size()) [[unlikely]] { return; }
            auto& fn{storage.functions.index_unchecked(cu.local_function_index)};
            require_current_checked_function(*ctx->curr_module, storage, cu.local_function_index);
            if(storage.ring_admission != nullptr && !storage.ring_admission->matches_function<CompileOption>(
                *ctx->curr_module, ctx->options.compile_options.curr_wasm_id, cu.local_function_index))
            { fail_checked_lazy_lowering(checked_integer_lowering_status::missing_or_retired_source, cu.local_function_index); }

            ::uwvm2::validation::error::code_validation_error_impl local_err{};
            auto& err{ctx->err == nullptr ? local_err : *ctx->err};
            ::fast_io::unix_timestamp compile_start_time{};
            if(::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::enabled()) [[unlikely]]
            {
                // Timing is best-effort diagnostic data; logging must never make lazy compilation fail.
# ifdef UWVM_CPP_EXCEPTIONS
                try
# endif
                {
                    compile_start_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                }
# ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    // do nothing
                }
# endif
            }

            ::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::line(u8"compile-start module=\"",
                                                                         ctx->module_name,
                                                                         u8"\" module_id=",
                                                                         ctx->options.compile_options.curr_wasm_id,
                                                                         u8" local_fn=",
                                                                         cu.local_function_index,
                                                                         u8" fn=",
                                                                         fn.function_index,
                                                                         u8" cu=",
                                                                         ctx->compile_unit_index,
                                                                         u8" cu_kind=",
                                                                         lazy_compile_unit_kind_name(cu.kind),
                                                                         u8" scope=",
                                                                         lazy_materialization_scope_name(cu.materialization_scope),
                                                                         u8" eu=[",
                                                                         cu.begin_eu_index,
                                                                         u8",",
                                                                         cu.end_eu_index,
                                                                         u8") offset=",
                                                                         cu.code_offset,
                                                                         u8" size=",
                                                                         cu.code_size);

# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                // The request may name a subrange, but the current materializer compiles the entire owning function once.
                compile_lazy_local_function<CompileOption>(*ctx->curr_module, storage, ctx->options, cu.local_function_index, err);
                // A successful whole-function compile satisfies all compile units for that function.
                mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::compiled);
                ::fast_io::unix_timestamp compile_end_time{};
                if(::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::enabled()) [[unlikely]]
                {
# ifdef UWVM_CPP_EXCEPTIONS
                    try
# endif
                    {
                        compile_end_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                    }
# ifdef UWVM_CPP_EXCEPTIONS
                    catch(::fast_io::error)
                    {
                        // do nothing
                    }
# endif
                }
                ::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::line(u8"compile-end module=\"",
                                                                             ctx->module_name,
                                                                             u8"\" module_id=",
                                                                             ctx->options.compile_options.curr_wasm_id,
                                                                             u8" local_fn=",
                                                                             cu.local_function_index,
                                                                             u8" fn=",
                                                                             fn.function_index,
                                                                             u8" cu=",
                                                                             ctx->compile_unit_index,
                                                                             u8" state=compiled cu_count=",
                                                                             fn.cu_count,
                                                                             u8" eu_count=",
                                                                             fn.eu_count,
                                                                             u8" time=",
                                                                             compile_end_time - compile_start_time);
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(...)
            {
                // Mark every unit for this function failed so waiters do not spin on a compile that cannot complete.
                mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::failed);
                ::fast_io::unix_timestamp compile_end_time{};
                if(::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::enabled()) [[unlikely]]
                {
                    try
                    {
                        compile_end_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                    }
                    catch(::fast_io::error)
                    {
                        // do nothing
                    }
                }
                ::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::line(u8"compile-end module=\"",
                                                                             ctx->module_name,
                                                                             u8"\" module_id=",
                                                                             ctx->options.compile_options.curr_wasm_id,
                                                                             u8" local_fn=",
                                                                             cu.local_function_index,
                                                                             u8" fn=",
                                                                             fn.function_index,
                                                                             u8" cu=",
                                                                             ctx->compile_unit_index,
                                                                             u8" state=failed time=",
                                                                             compile_end_time - compile_start_time);
            }
# endif
        }
    }  // namespace details

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption = {}>
    inline constexpr lazy_module_storage_t initialize_lazy_module_storage(runtime_module_storage_t const& curr_module,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option const& options,
                                                                          ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                          lazy_split_config split_config = {},
                                                                          parser_feature_parameter_t const* wasm_feature_parameter = nullptr) UWVM_THROWS
    {
        if(wasm_feature_parameter == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        // Validate runtime pointers/counts once without decoding any function body.
        ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::validate_runtime_module_storage(curr_module);
        ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::require_runtime_module_declaration_policy(
            curr_module, *wasm_feature_parameter, err);

        lazy_module_storage_t storage{};

        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        // Call metadata is required before lazy materialization because direct-call targets may be queried by the first compiled body.
        details::fill_lazy_local_defined_call_info(curr_module, options, storage.compiled);

        storage.functions.clear();
        storage.functions.resize(local_func_count);
        storage.execution_units.clear();
        storage.compile_units.clear();
        storage.execution_units.reserve(local_func_count);
        storage.compile_units.reserve(local_func_count);

        // This backend materializes whole functions. Build truthful whole-body
        // units directly, avoiding a second structural scanner over Wasm bytes.
        // Finer EU/CU scheduling flags no longer split this checked artifact.
        split_config.eu_policy = lazy_execution_unit_split_policy_t::function_only;
        split_config.cu_policy = lazy_compile_unit_split_policy_t::function;
        // Authoritative body validation is fused with original ring emission.
        for(::std::size_t local_function_index{}; local_function_index != local_func_count; ++local_function_index)
        {
            details::build_lazy_function_execution_units(curr_module, storage, local_function_index, split_config, *wasm_feature_parameter, err);
        }
        storage.ring_admission = details::checked_register_ring_admission::admit<CompileOption>(
            curr_module, storage, options, wasm_feature_parameter, err);
        return storage;
    }

    // Experimental actual record-consumer slice. This initializer emits zero
    // opfuncs and creates one whole-function unit from retained extents. Neither
    // a raw structural scanner nor the raw trivial-inline matcher is invoked.
    [[nodiscard]] inline lazy_module_storage_t initialize_checked_lazy_module_storage(
        runtime_module_storage_t const& curr_module,
        ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option const& options,
        checked_integer_module_plan::owner plan)
    {
        if(!plan || !plan->matches_current_source(curr_module) ||
           plan->function_count() != curr_module.local_defined_function_vec_storage.size()) { ::fast_io::fast_terminate(); }
        lazy_module_storage_t storage{};
        storage.checked_plan = ::std::move(plan);
        auto const count{storage.checked_plan->function_count()};
        storage.compiled.local_funcs.resize(count);
        storage.compiled.local_defined_call_info.resize(count);
        storage.functions.resize(count);
        storage.execution_units.reserve(count); storage.compile_units.reserve(count);
        auto const imported{curr_module.imported_function_vec_storage.size()};
        for(::std::size_t index{}; index != count; ++index)
        {
            auto const* function{storage.checked_plan->function(index)};
            if(function == nullptr || !storage.checked_plan->matches_function_source(curr_module, index))
            { ::fast_io::fast_terminate(); }
            auto& information{storage.compiled.local_defined_call_info.index_unchecked(index)};
            information.module_id = options.curr_wasm_id; information.function_index = imported + index;
            information.runtime_func = ::std::addressof(curr_module.local_defined_function_vec_storage.index_unchecked(index));
            information.compiled_func = ::std::addressof(storage.compiled.local_funcs.index_unchecked(index));
            for(auto const parameter: function->parameters())
            {
                auto const width{checked_plan_details::local_width(parameter)};
                if(width == 0uz || width > (::std::numeric_limits<::std::size_t>::max)() - information.param_bytes) { ::fast_io::fast_terminate(); }
                information.param_bytes += width;
            }
            for(auto const result: function->results())
            {
                auto const width{checked_plan_details::local_width(result)};
                if(width == 0uz || width > (::std::numeric_limits<::std::size_t>::max)() - information.result_bytes) { ::fast_io::fast_terminate(); }
                information.result_bytes += width;
            }
            auto& state{storage.functions.index_unchecked(index)};
            state.function_index = imported + index; state.local_function_index = index;
            state.first_eu_index = index; state.eu_count = 1uz;
            state.first_cu_index = index; state.cu_count = 1uz; state.primary_cu_index = index;
            auto const& code{*curr_module.local_defined_function_vec_storage.index_unchecked(index).wasm_code_ptr};
            // The private plan was minted from this actual pinned descriptor.
            // [owned expression bytes ... expr_end] | one-past
            // [safe: source/type lifetime pinned  ] | borrow only, no byte reads
            // ^^ descriptor begin/end -> immutable scheduling metadata copies.
            auto const begin{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
            auto const end{reinterpret_cast<::std::byte const*>(code.body.code_end)};
            storage.execution_units.push_back({.function_index = imported + index, .local_function_index = index,
                .code_begin = begin, .code_end = end, .code_size = function->expression_bytes()});
            storage.compile_units.push_back({.function_index = imported + index, .local_function_index = index,
                .begin_eu_index = index, .end_eu_index = index + 1uz,
                .code_begin = begin, .code_end = end, .code_size = function->expression_bytes()});
        }
        return storage;
    }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    inline constexpr void compile_cu_from_lazy_validator(runtime_module_storage_t const& curr_module,
                                                         lazy_module_storage_t& storage,
                                                         lazy_compile_options& options,
                                                         ::std::size_t compile_unit_index,
                                                         ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        // Public synchronous entry point: invalid indices are internal runtime bugs, so terminate rather than fabricating a wasm error.
        if(compile_unit_index >= storage.compile_units.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto& cu{storage.compile_units.index_unchecked(compile_unit_index)};
        if(cu.local_function_index >= storage.functions.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto& fn{storage.functions.index_unchecked(cu.local_function_index)};
        details::require_current_checked_function(curr_module, storage, cu.local_function_index);
        if(storage.ring_admission != nullptr && !storage.ring_admission->matches_function<CompileOption>(
            curr_module, options.compile_options.curr_wasm_id, cu.local_function_index))
        { details::fail_checked_lazy_lowering(checked_integer_lowering_status::missing_or_retired_source, cu.local_function_index); }

        ::fast_io::unix_timestamp compile_start_time{};
        if(::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::enabled()) [[unlikely]]
        {
            // Logging timestamps remain optional because platforms or builds may not support the clock path.
# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                compile_start_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                // do nothing
            }
# endif
        }

        ::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::line(u8"compile-cu-start module_id=",
                                                                     options.compile_options.curr_wasm_id,
                                                                     u8" local_fn=",
                                                                     cu.local_function_index,
                                                                     u8" cu=",
                                                                     compile_unit_index,
                                                                     u8" cu_kind=",
                                                                     lazy_compile_unit_kind_name(cu.kind),
                                                                     u8" scope=",
                                                                     lazy_materialization_scope_name(cu.materialization_scope),
                                                                     u8" eu=[",
                                                                     cu.begin_eu_index,
                                                                     u8",",
                                                                     cu.end_eu_index,
                                                                     u8") offset=",
                                                                     cu.code_offset,
                                                                     u8" size=",
                                                                     cu.code_size);

        bool counted_wait{};
        for(;;)
        {
            // Acquire pairs with release stores from mark_function_compile_units_state so a compiled state also publishes code data.
            auto const st{fn.materialization_state.state.load(::std::memory_order_acquire)};
            if(st == ::uwvm2::utils::thread::lazy_compile_state::compiled)
            {
                // Another thread already materialized this function; the requested compile unit is therefore satisfied.
                ::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::line(u8"compile-cu-hit module_id=",
                                                                             options.compile_options.curr_wasm_id,
                                                                             u8" local_fn=",
                                                                             cu.local_function_index,
                                                                             u8" cu=",
                                                                             compile_unit_index,
                                                                             u8" state=compiled");
                return;
            }
            if(st == ::uwvm2::utils::thread::lazy_compile_state::failed) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(st == ::uwvm2::utils::thread::lazy_compile_state::uncompiled)
            {
                auto expected{::uwvm2::utils::thread::lazy_compile_state::uncompiled};
                // Claim exactly one compiler for the function. Even multiple compile-unit requests converge on the same materializer.
                if(fn.materialization_state.state.compare_exchange_strong(expected,
                                                                          ::uwvm2::utils::thread::lazy_compile_state::compiling,
                                                                          ::std::memory_order_acq_rel,
                                                                          ::std::memory_order_acquire))
                {
                    ::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::line(u8"compile-cu-claim module_id=",
                                                                                 options.compile_options.curr_wasm_id,
                                                                                 u8" local_fn=",
                                                                                 cu.local_function_index,
                                                                                 u8" cu=",
                                                                                 compile_unit_index,
                                                                                 u8" state=uncompiled->compiling");
                    break;
                }
                continue;
            }

            if(!counted_wait)
            {
                // Log the first wait only; repeated wait logging would hide the useful compile-start/compile-end events.
                ::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::line(u8"compile-cu-wait module_id=",
                                                                             options.compile_options.curr_wasm_id,
                                                                             u8" local_fn=",
                                                                             cu.local_function_index,
                                                                             u8" cu=",
                                                                             compile_unit_index,
                                                                             u8" state=",
                                                                             ::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::compile_state_name(st));
                counted_wait = true;
            }
            ::uwvm2::utils::thread::lazy_compile_thread_yield();
        }

        // Once this thread owns compilation, emit the full function and publish completion to all sibling compile units.
# ifdef UWVM_CPP_EXCEPTIONS
        try
# endif
        {
            details::compile_lazy_local_function<CompileOption>(curr_module, storage, options, cu.local_function_index, err);
            details::mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::compiled);
        }
# ifdef UWVM_CPP_EXCEPTIONS
        catch(...)
        {
            // This synchronous entry point has already published `compiling`.  Publish failure to the function and every sibling CU
            // before preserving the original exception; otherwise concurrent waiters can spin forever on an abandoned owner.
            details::mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::failed);
            throw;
        }
# endif
        ::fast_io::unix_timestamp compile_end_time{};
        if(::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::enabled()) [[unlikely]]
        {
# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                compile_end_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                // do nothing
            }
# endif
        }
        ::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::line(u8"compile-cu-end module_id=",
                                                                     options.compile_options.curr_wasm_id,
                                                                     u8" local_fn=",
                                                                     cu.local_function_index,
                                                                     u8" cu=",
                                                                     compile_unit_index,
                                                                     u8" state=compiled time=",
                                                                     compile_end_time - compile_start_time);
    }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    [[nodiscard]] inline constexpr ::uwvm2::utils::thread::lazy_compile_request make_lazy_compile_request(lazy_compile_request_context & ctx,
                                                                                                          unsigned priority = 0u) noexcept
    {
        // Invalid requests produce an empty descriptor so callers can skip enqueueing without throwing from a scheduling path.
        if(ctx.lazy_storage == nullptr || ctx.compile_unit_index >= ctx.lazy_storage->compile_units.size()) [[unlikely]] { return {}; }

        auto& cu{ctx.lazy_storage->compile_units.index_unchecked(ctx.compile_unit_index)};
        if(cu.local_function_index >= ctx.lazy_storage->functions.size()) [[unlikely]] { return {}; }
        auto& fn{ctx.lazy_storage->functions.index_unchecked(cu.local_function_index)};

        // Choose the synchronization object according to the materialization scope. Today this is normally the whole-function state,
        // but preserving the distinction keeps the request ABI ready for true execution-unit materialization.
        auto unit{cu.materialization_scope == lazy_materialization_scope::whole_function ? ::std::addressof(fn.materialization_state)
                                                                                         : ::std::addressof(cu.state)};
        return {.unit = unit, .compile = details::lazy_compile_request_entry<CompileOption>, .user_data = ::std::addressof(ctx), .priority = priority};
    }
}
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

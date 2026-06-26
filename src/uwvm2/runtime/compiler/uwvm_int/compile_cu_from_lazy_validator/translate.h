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
# include <uwvm2/validation/standard/wasm1p1/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/utils/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/optable/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/impl.h>
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

    struct lazy_module_storage_t
    {
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
        // Validation needs parser-level module metadata. It is optional only when the caller explicitly promises prior validation.
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
            err.err_curr = op_begin;
            err.err_code = ec;
            // Throw through the parser path so callers observe the same failure mechanism used by normal wasm validation.
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(pc);
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

            code_curr = reinterpret_cast<::std::byte const*>(next);
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
            code_curr += bytes;
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
            ++code_curr;
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
                                                                   ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            // Non-structural immediates are skipped precisely so the scanner can continue finding block/loop/if/else/end opcodes
            // without performing full stack validation or semantic checks.
            /// @warning Extension point: every opcode with immediates must be listed here or lazy structural splitting will desynchronize.
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
                    skip_reserved_memory_index_byte(code_curr, code_end, op_begin, err);
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
                        // MVP numeric and simple stack/control opcodes carry no immediates that matter to structural splitting.
                        return;
                    }

                    // Unknown opcodes are rejected early because a wrong skip length would corrupt every later structural boundary.
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
                                                                  ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            // Build the lazy index from the runtime module because it owns the final function ordering used by interpreter dispatch.
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
                ++code_curr;

                /// @warning Extension point: structural opcode additions must update this lazy execution-unit scanner.
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

                        // Opening a structural instruction starts a child execution unit at the opcode itself so the range remains
                        // directly readable in diagnostics and can be re-parsed independently later.
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
                        skip_wasm1_non_structural_immediates(code_curr, code_end, op_begin, curr_opbase, err);
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

        inline constexpr void mark_function_compile_units_state(lazy_module_storage_t& storage,
                                                                lazy_function_storage_t& fn,
                                                                ::uwvm2::utils::thread::lazy_compile_state state) noexcept
        {
            // Publish the function state first because whole-function materialization is currently authoritative for execution.
            fn.materialization_state.state.store(state, ::std::memory_order_release);
            for(::std::size_t i{fn.first_cu_index}; i != fn.first_cu_index + fn.cu_count; ++i)
            {
                // Mirror the state onto child compile units so diagnostics and future per-range scheduling observe a consistent view.
                storage.compile_units.index_unchecked(i).state.state.store(state, ::std::memory_order_release);
            }
        }

        inline constexpr void validate_function_if_needed(runtime_module_storage_t const& curr_module,
                                                          lazy_compile_options const& options,
                                                          ::std::size_t local_function_index,
                                                          ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            // Validation is intentionally delayed to materialization when requested, allowing module setup to build only lightweight
            // structural metadata while preserving a strict validation barrier before code becomes executable.
            if(options.validation_mode == lazy_validation_mode::validate_on_lazy_compile)
            {
                // Lazy validation must use the standard wasm1p1 validator directly. This keeps the lazy path aligned with
                // src/uwvm2/validation/standard/wasm1p1/validator.h, including its memory-safety boundary comments and parse checks.
                if(options.validator_module_storage == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                if(options.validator_feature_parameter == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const import_func_count{curr_module.imported_function_vec_storage.size()};
                auto const function_index{import_func_count + local_function_index};
                auto const& curr_local_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_index)};
                auto const& curr_code{*curr_local_func.wasm_code_ptr};
                auto const code_begin{reinterpret_cast<::std::byte const*>(curr_code.body.expr_begin)};
                auto const code_end{reinterpret_cast<::std::byte const*>(curr_code.body.code_end)};

                ::uwvm2::validation::standard::wasm1p1::validate_code(::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
                                                                      *options.validator_module_storage,
                                                                      function_index,
                                                                      code_begin,
                                                                      code_end,
                                                                      err,
                                                                      *options.validator_feature_parameter);
            }
        }

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
        inline constexpr void compile_lazy_local_function(runtime_module_storage_t const& curr_module,
                                                          lazy_module_storage_t& storage,
                                                          lazy_compile_options& options,
                                                          ::std::size_t local_function_index,
                                                          ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            // Validate immediately before compilation so a function cannot be installed into the interpreter if validation fails.
            validate_function_if_needed(curr_module, options, local_function_index, err);

            // Materialize with the eager single-function compiler. This deliberately trades partial compilation for identical emitted
            // opfunc streams, stack metadata, and call handling across lazy and non-lazy modes.
            ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::compile_all_from_uwvm_local_func<CompileOption>(curr_module,
                                                                                                                                  options.compile_options,
                                                                                                                                  storage.compiled,
                                                                                                                                  local_function_index,
                                                                                                                                  err);
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

    inline constexpr lazy_module_storage_t initialize_lazy_module_storage(runtime_module_storage_t const& curr_module,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option const& options,
                                                                          ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                          lazy_split_config split_config = {}) UWVM_THROWS
    {
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

        // Build only indexing metadata during initialization. Real validation and bytecode emission can be deferred to the first use.
        for(::std::size_t local_function_index{}; local_function_index != local_func_count; ++local_function_index)
        {
            details::build_lazy_function_execution_units(curr_module, storage, local_function_index, split_config, err);
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

        auto& cu{storage.compile_units.index_unchecked(compile_unit_index)};
        if(cu.local_function_index >= storage.functions.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto& fn{storage.functions.index_unchecked(cu.local_function_index)};

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
        details::compile_lazy_local_function<CompileOption>(curr_module, storage, options, cu.local_function_index, err);
        details::mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::compiled);
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
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/impl.h>
# include "exception_immediate.h"
# include "tail_call.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    inline constexpr void require_exceptions_enabled(bool enabled, unsigned opcode,
        ::std::byte const* op_begin, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        if(enabled) { return; }
        // [exception opcode] immediate ... end
        // [safe            ] unsafe (could be end)
        // ^^ err_curr borrows the dispatch-checked opcode, without reading or advancing it.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
        err.err_selectable.wasm1p1_feature_required = {
            .value = opcode, .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions,
            .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction};
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    // GC cast immediates can name exn/noexn even when the operand stack is
    // polymorphic. Apply the same exception feature policy in the standalone
    // validator and in each compiler's single-pass validation/emission loop.
    inline constexpr void require_gc_cast_exception_policy(bool enabled, unsigned opcode,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type from,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type to,
        ::std::byte const* op_begin, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        if(enabled || opcode < 20u || opcode > 25u) { return; }
        namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
        auto const is_exception_heap{[](t3::core_value_type value) constexpr noexcept
        {
            return value.kind == t3::value_kind::reference &&
                (value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::exn) ||
                 value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noexn));
        }};
        if(is_exception_heap(to) || (opcode >= 24u && is_exception_heap(from))) [[unlikely]]
        {
            // [0xfb][bounded cast immediate] next ... code_end
            // [safe                       ] unsafe (could be code_end)
            // ^^ op_begin is a borrowed prefix for diagnostics; no cursor moves.
            require_exceptions_enabled(false, 0xfbu, op_begin, err);
        }
    }

    // A zero-handler try_table has the exact control/stack semantics of block and can use the same
    // generated code. Nonempty tables must NEVER silently lose their exceptional edges: until each
    // backend has handler lowering, reject them before publishing a control frame or generated code.
    inline constexpr void read_empty_exception_catches(::std::byte const*& code_curr, ::std::byte const* code_end,
        ::std::byte const* op_begin, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        // [try_table blocktype] count clauses ... code_end
        // [safe               ] unsafe (could be code_end)
        //                       ^^ next: private cursor, preserves caller state on every rejection.
        auto next{code_curr};
        auto const catches{scan_exception_catches(next, code_end)};
        // [try_table blocktype][checked vector] next opcode ... code_end
        // [safe                              ] unsafe (could be code_end)
        //                                      ^^ next on successful decode, unchanged on malformed input.
        if(catches.error != exception_immediate_error::ok || !catches.clauses.empty()) [[unlikely]]
        {
            // [try_table] ... end; op_begin is a dispatch-checked diagnostic address, never dereferenced here.
            // [safe    ]
            // ^^ err_curr
            err.err_curr = op_begin;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_opbase;
            err.err_selectable.u8 = 0x1fu;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        // [try_table blocktype complete empty vector] next opcode ... code_end
        // [safe                                    ] unsafe (could be code_end)
        //                                            ^^ code_curr commits only the decoder-proven endpoint.
        code_curr = next;
        // [try_table blocktype complete empty vector] next opcode ... code_end
        // [safe                                    ] unsafe (could be code_end)
        //                                            ^^ code_curr: one-past is legal; caller checks before reading.
    }
}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif

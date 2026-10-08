/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <type_traits>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include "wasm1p1.h"
# include "memory64_bulk.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    enum class table64_operation : unsigned { get, set, init, copy, grow, size, fill };
    namespace details::table64
    {
        namespace refs = wasm1p1_details;
        using operation = table64_operation;
        UWVM_ALWAYS_INLINE inline constexpr void check_range(::std::uint_least64_t offset,
            ::std::uint_least64_t length, ::std::size_t bound) noexcept
        {
            // Check both unsigned operands before host-width conversion or addition.
            if(offset > bound || length > bound - offset) [[unlikely]] { refs::table_oob_terminate(); }
        }
        template<typename Value, uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr void push(Value const& value, Type&... args) noexcept
        {
            // [live operands][reserved result slot] ... operand allocation
            // [safe                              ] translation computed maximum extent.
            //                ^^ SP; reference/table op handlers use the flushed stack.
            ::std::memcpy(args...[1], ::std::addressof(value), sizeof(value));
            args...[1] += sizeof(value);
            // [live operands result] ... reserved operand allocation
            // [safe                ]
            //                       ^^ SP, including its permitted one-past position.
        }
        template<bool Extern, uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr auto pop_reference(Type&... args) noexcept
        {
            using value_type = ::std::conditional_t<Extern, refs::wasm_externref, refs::wasm_funcref>;
            // [older operands][reference] <- SP; the validator proved this type,
            // and translation flushed cached operands before the reference op.
            auto const value{get_curr_val_from_operand_stack_cache<value_type>(args...)};
            // [older operands] <- SP, moved back by the complete reference width.
            return value;
        }
        template<bool Extern, bool ExnRef, bool GCRef, typename Value>
        UWVM_ALWAYS_INLINE inline constexpr auto to_element(refs::runtime_module_storage_t const* module, Value const& value) noexcept
        {
            if constexpr(GCRef) { return ::uwvm2::uwvm::runtime::storage::runtime_table_slot_from_gc_reference(value.ref); }
            else if constexpr(Extern) { return refs::table_elem_from_reference<ExnRef>(value); }
            else { return refs::table_elem_from_funcref(module, value); }
        }
        template<operation Op, bool Extern, bool Destination64, bool Source64,
                 uwvm_interpreter_translate_option_t Option, bool ExnRef = false, bool GCRef = false, uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr void execute(Type&... args) UWVM_THROWS
        {
            static_assert((!ExnRef && !GCRef) || Extern);
            // [handler][table*][operation-specific owners][successor] owned stream
            // [safe                                                ]
            // ^^ IP: translation emitted the entire record and validated stack types.
            if constexpr(Option.is_tail_call) { args...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>); }
            else { args...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); }
            // [handler][table*][operation-specific owners][successor]
            // [safe   ][safe                                        ]
            //          ^^ IP, now at the first complete pointer immediate.
            auto const table{refs::read_imm<refs::runtime_table_storage_t*>(args...[0])};
            // [handler table*][operation-specific owners][successor]
            // [safe         ][safe                                 ]
            //                 ^^ IP advanced by sizeof(table); table is module-owned.
            if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            [[maybe_unused]] refs::runtime_module_storage_t const* caller_module{};
            if constexpr(Op == operation::get && (ExnRef || GCRef))
            {
                // [caller module*][successor] ... bytecode_end
                // [safe         ] read_imm consumes exactly one compiler-owned pointer immediate.
                caller_module = refs::read_imm<refs::runtime_module_storage_t const*>(args...[0]);
                if(caller_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            }
            if constexpr(Op == operation::size)
            {
                static_assert(Destination64);
                push(static_cast<refs::wasm_i64>(table->elems.size()), args...);
            }
            else if constexpr(Op == operation::get)
            {
                auto const index{memory64_bulk::pop<Destination64>(args...)};
                check_range(index, 1u, table->elems.size());
                // [table slots ... index ...] | end; complete bound permits narrowing.
                auto const& elem{table->elems.index_unchecked(static_cast<::std::size_t>(index))};
                if constexpr(Extern)
                {
                    auto const value{[&]() noexcept
                    {
                        if constexpr(GCRef)
                        {
                            refs::wasm_externref out{};
                            out.ref = ::uwvm2::uwvm::runtime::storage::runtime_table_slot_to_gc_reference(elem);
                            return out;
                        }
                        else { return refs::table_reference_from_elem<ExnRef>(elem); }
                    }()};
                    if constexpr(ExnRef || GCRef)
                    {
                        if(::uwvm2::uwvm::runtime::storage::uwvm2_gc_retain_reference(
                               caller_module->gc_store.get(), ::std::addressof(value.ref)) !=
                               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                        { ::fast_io::fast_terminate(); }
                    }
                    push(value, args...);
                }
                else { push(refs::funcref_from_table_elem(elem), args...); }
            }
            else if constexpr(Op == operation::init)
            {
                // [consumed][element*][module* if funcref][successor]
                // [safe    ][safe                                 ]
                //           ^^ IP: both owners have module lifetime.
                auto const element{refs::read_imm<refs::runtime_element_storage_t*>(args...[0])};
                // [consumed element*][module* if funcref][successor] <- IP at module/successor.
                refs::runtime_module_storage_t const* module{};
                if constexpr(!Extern)
                {
                    module = refs::read_imm<refs::runtime_module_storage_t const*>(args...[0]);
                    // [consumed owners][successor] <- IP; complete module pointer consumed.
                }
                if(element == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const length{memory64_bulk::pop<false>(args...)};
                auto const source{memory64_bulk::pop<false>(args...)};
                auto const destination{memory64_bulk::pop<Destination64>(args...)};
                check_range(destination, length, table->elems.size());
                if constexpr(Extern)
                {
                    // One acquire snapshot: elem.drop publishes visibility, never rewrites payload pointers.
                    auto const payload{::uwvm2::uwvm::runtime::storage::load_wasm_element_segment_payload(element->element)};
                    auto const begin{GCRef ? nullptr : payload.externref_begin};
                    auto const end{GCRef ? nullptr : payload.externref_end};
                    auto const gc_begin{payload.gc_ref_begin};
                    auto const gc_end{payload.gc_ref_end};
                    if((begin == nullptr) != (end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if constexpr(GCRef)
                    { if((gc_begin == nullptr) != (gc_end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); } }
                    auto const size{GCRef ? (gc_begin == nullptr ? 0uz : static_cast<::std::size_t>(gc_end - gc_begin)) :
                        (begin == nullptr ? 0uz : static_cast<::std::size_t>(end - begin))};
                    check_range(source, length, size);
                    // Both full ranges passed. No mutation occurred before the source proof.
                    for(::std::size_t i{}; i != length; ++i)
                    {
                        auto& slot{table->elems.index_unchecked(static_cast<::std::size_t>(destination) + i)};
                        if constexpr(GCRef)
                        {
                            auto const& value{gc_begin[static_cast<::std::size_t>(source) + i]};
                            auto const next{::uwvm2::uwvm::runtime::storage::runtime_table_slot_from_gc_reference(value)};
                            if(::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(table, value) !=
                               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                            { ::fast_io::fast_terminate(); }
                            slot = next;
                        }
                        else
                        {
                            auto const token{begin[static_cast<::std::size_t>(source) + i]};
                            refs::retain_table_payload<ExnRef>(table, token);
                            slot.storage.extern_ptr = token;
                            slot.type = ExnRef ? refs::runtime_table_elem_type::exn_ref : refs::runtime_table_elem_type::extern_ref;
                        }
                    }
                }
                else
                {
                    // Destination passed the full-width check; source/length are u32.
                    // The helper takes one source snapshot and checks it before mutation.
                    refs::copy_funcref_element_segment(*table, *element, module, static_cast<::std::size_t>(destination),
                        static_cast<::std::size_t>(source), static_cast<::std::size_t>(length));
                    if(length != 0u) { refs::refresh_llvm_call_indirect_table_views_after_funcref_write(
                        table, refs::runtime_table_mutation_kind::init, static_cast<::std::size_t>(destination), static_cast<::std::size_t>(length)); }
                }
            }
            else if constexpr(Op == operation::copy)
            {
                // [consumed][source table*][successor] owned stream
                // [safe    ][safe                    ]
                //           ^^ IP: read_imm consumes the complete source owner pointer.
                auto const source_table{refs::read_imm<refs::runtime_table_storage_t*>(args...[0])};
                // [consumed source table*][successor] <- IP at a complete successor.
                if(source_table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const length{memory64_bulk::pop<Destination64 && Source64>(args...)};
                auto const source{memory64_bulk::pop<Source64>(args...)};
                auto const destination{memory64_bulk::pop<Destination64>(args...)};
                check_range(source, length, source_table->elems.size());
                check_range(destination, length, table->elems.size());
                if(length != 0u)
                {
                    if constexpr(Extern)
                    {
                        for(::std::size_t i{}; i != length; ++i)
                        {
                            if constexpr(GCRef)
                            {
                                auto const value{::uwvm2::uwvm::runtime::storage::runtime_table_slot_to_gc_reference(
                                    source_table->elems.index_unchecked(static_cast<::std::size_t>(source) + i))};
                                if(table != source_table &&
                                   ::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(table, value) !=
                                   ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                                { ::fast_io::fast_terminate(); }
                            }
                            else
                            {
                                auto const value{refs::table_reference_from_elem<ExnRef>(
                                    source_table->elems.index_unchecked(static_cast<::std::size_t>(source) + i))};
                                refs::retain_table_reference<ExnRef>(table, value);
                            }
                        }
                    }
                    // [source slots ... source ... source+length] | end
                    // [safe                                     ] nonempty checked range.
                    auto const from{source_table->elems.data() + static_cast<::std::size_t>(source)};
                    // [destination slots ... destination ... destination+length] | end
                    // [safe                                                    ]
                    auto const to{table->elems.data() + static_cast<::std::size_t>(destination)};
                    // Both extents are actual vector allocations, proving byte-product representability.
                    ::std::memmove(to, from, static_cast<::std::size_t>(length) * sizeof(refs::runtime_table_elem_storage_t));
                    if constexpr(!Extern) { refs::refresh_llvm_call_indirect_table_views_after_funcref_write(
                        table, refs::runtime_table_mutation_kind::copy, static_cast<::std::size_t>(destination), static_cast<::std::size_t>(length)); }
                }
            }
            else
            {
                refs::runtime_module_storage_t const* module{};
                if constexpr(!Extern)
                {
                    // [consumed][module*][successor] complete serialized owner immediate.
                    //           ^^ IP before; successor after the bounded-size read.
                    module = refs::read_imm<refs::runtime_module_storage_t const*>(args...[0]);
                }
                // [consumed owners][successor] <- IP, no remaining immediates.
                ::std::uint_least64_t length{};
                if constexpr(Op == operation::grow || Op == operation::fill)
                { length = memory64_bulk::pop<Destination64>(args...); }
                auto const value{pop_reference<Extern>(args...)};
                auto const elem{to_element<Extern, ExnRef, GCRef>(module, value)};
                if constexpr(Op == operation::grow)
                {
                    auto const old_size{table->elems.size()};
                    constexpr auto native_max{(::std::numeric_limits<::std::size_t>::max)() / sizeof(refs::runtime_table_elem_storage_t)};
                    auto const declared_max{table->table_type_ptr->limits.max};
                    auto const maximum{declared_max < native_max ? declared_max : native_max};
                    using result_type = ::std::conditional_t<Destination64, refs::wasm_i64, refs::wasm_i32>;
                    result_type result{-1};
                    if(old_size <= maximum && length <= maximum - old_size)
                    {
                        auto const count{static_cast<::std::size_t>(length)};
                        auto const new_size{old_size + count};
                        if constexpr(GCRef)
                        { if(count != 0uz && ::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(table, value.ref) !=
                          ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]] { ::fast_io::fast_terminate(); } }
                        else if constexpr(Extern) { if(count != 0uz) { refs::retain_table_reference<ExnRef>(table, value); } }
                        if(::uwvm2::uwvm::runtime::storage::try_grow_table_elements(*table, new_size, elem))
                        {
                            result = static_cast<result_type>(old_size);
                            if constexpr(!Extern) { if(count != 0uz) { refs::refresh_llvm_call_indirect_table_views_after_funcref_write(
                                table, refs::runtime_table_mutation_kind::grow, old_size, count); } }
                        }
                    }
                    push(result, args...);
                }
                else
                {
                    auto const destination{memory64_bulk::pop<Destination64>(args...)};
                    if constexpr(Op == operation::set) { length = 1u; }
                    check_range(destination, length, table->elems.size());
                    auto const offset{static_cast<::std::size_t>(destination)};
                    auto const count{static_cast<::std::size_t>(length)};
                    if constexpr(GCRef)
                    { if(count != 0uz && ::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(table, value.ref) !=
                      ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]] { ::fast_io::fast_terminate(); } }
                    else if constexpr(Extern) { if(count != 0uz) { refs::retain_table_reference<ExnRef>(table, value); } }
                    // Checked whole range, including zero-length table.fill at the end.
                    for(::std::size_t i{}; i != count; ++i) { table->elems.index_unchecked(offset + i) = elem; }
                    if constexpr(!Extern) { if(count != 0uz) { refs::refresh_llvm_call_indirect_table_views_after_funcref_write(
                        table, Op == operation::set ? refs::runtime_table_mutation_kind::set : refs::runtime_table_mutation_kind::fill, offset, count); } }
                }
            }
        }
    }
    template<table64_operation Op, bool Extern, bool Destination64, bool Source64,
             uwvm_interpreter_translate_option_t Option, bool ExnRef = false, bool GCRef = false, uwvm_int_stack_top_type... Type>
        requires(Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table64(Type... args) UWVM_THROWS
    {
        details::table64::execute<Op, Extern, Destination64, Source64, Option, ExnRef, GCRef>(args...);
        // [consumed operation][successor] ... validated owned stream
        // [safe              ][safe     ]
        //                      ^^ IP from execute; dispatch never advances it.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), args...[0], sizeof(next));
        UWVM_MUSTTAIL return next(args...);
    }
    template<table64_operation Op, bool Extern, bool Destination64, bool Source64,
             uwvm_interpreter_translate_option_t Option, bool ExnRef = false, bool GCRef = false, uwvm_int_stack_top_type... Type>
        requires(!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table64(Type&... args) UWVM_THROWS
    { details::table64::execute<Op, Extern, Destination64, Source64, Option, ExnRef, GCRef>(args...); }
    namespace translate
    {
        template<table64_operation Op, bool Extern, bool Destination64, bool Source64,
                 uwvm_interpreter_translate_option_t Option, bool ExnRef = false, bool GCRef = false, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_table64_fptr_from_tuple(::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_table64<Op, Extern, Destination64, Source64, Option, ExnRef, GCRef, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_table64<Op, Extern, Destination64, Source64, Option, ExnRef, GCRef, Type...>); }
        }
    }
}
#endif
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

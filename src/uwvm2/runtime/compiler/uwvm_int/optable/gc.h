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
# include <concepts>
# include <memory>
# include <limits>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <fast_io.h>
# include <uwvm2/object/impl.h>
# include <uwvm2/uwvm/runtime/storage/wasm_module.h>
# include "wasm1p1.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    namespace gc_details
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_funcref = ::uwvm2::object::global::wasm_funcref_t;

        template<bool Signed>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_i32 read_i31(wasm_funcref const& ref) noexcept
        {
            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            if(ref.ref.kind != ref_kind::wasm_i31) [[unlikely]]
            {
                if(ref.ref.kind == ref_kind::wasm_null) { wasm1p1_details::null_reference_terminate(); }
                // A validated i31.get cannot receive another non-null heap kind. A corrupt host
                // reference must fail before treating its pointer payload as an integer value.
                ::fast_io::fast_terminate();
            }
            if constexpr(Signed) { return static_cast<wasm_i32>(ref.ref.storage.wasm_i31.get_s()); }
            else { return details::from_u32_bits<wasm_i32>(ref.ref.storage.wasm_i31.get_u()); }
        }
    }

    template<uwvm_interpreter_translate_option_t CompileOption, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_ref_i31(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        auto const input{get_curr_val_from_operand_stack_top<CompileOption, gc_details::wasm_i32, I32Pos>(type...)};
        gc_details::wasm_funcref const out{{::uwvm2::object::global::make_wasm_i31_reference(input)}};
        // [older operands][reserved reference slot] ... stack allocation end
        // [safe                                  ] the validator's maximum frame includes this 16/8-byte result.
        //                 ^^ type...[1u] is the first byte of the writable result slot.
        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        type...[1u] += sizeof(out);
        // [older operands][reference] next free slot ... allocation end
        // [safe                     ] ^^ type...[1u] may be one-past, never dereferenced here.

        // [current handler][next handler] ... validated bytecode end
        // [safe                         ] translator emits a successor for nonterminating ref.i31.
        // ^^ type...[0] points at the complete current handler slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        // [current handler][next handler] ... validated bytecode end
        // [safe                         ] ^^ type...[0] points at the successor slot.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }

    template<uwvm_interpreter_translate_option_t CompileOption, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_ref_i31(Type&... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        // [current handler][next handler] ... validated bytecode end
        // [safe                         ] dispatcher proved the complete current handler slot.
        // ^^ type...[0] advances only to the following slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>);
        auto const input{get_curr_val_from_operand_stack_cache<gc_details::wasm_i32>(type...)};
        gc_details::wasm_funcref const out{{::uwvm2::object::global::make_wasm_i31_reference(input)}};
        // [older operands][reserved reference slot] ... stack allocation end
        // [safe                                  ] validated frame size covers the complete write.
        //                 ^^ type...[1u]
        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        type...[1u] += sizeof(out);
        // [older operands][reference] next free slot ... allocation end
        // [safe                     ] ^^ type...[1u], possibly one-past.
    }

    template<uwvm_interpreter_translate_option_t CompileOption, bool Signed, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i31_get(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        auto const ref{get_curr_val_from_operand_stack_cache<gc_details::wasm_funcref>(type...)};
        auto const out{gc_details::read_i31<Signed>(ref)};
        wasm1p1_details::push_out_value<CompileOption, gc_details::wasm_i32, I32Pos>(out, type...);
        // [current handler][next handler] ... validated bytecode end
        // [safe                         ] translator emitted the successor.
        // ^^ type...[0] advances by one complete handler pointer.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next;
        // [current handler][next handler] ... validated bytecode end
        // [safe                         ] ^^ type...[0] is the readable successor slot.
        ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }

    template<uwvm_interpreter_translate_option_t CompileOption, bool Signed, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i31_get(Type&... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        // [current handler][next handler] ... validated bytecode end
        // [safe                         ] the dispatcher proved the current handler.
        // ^^ type...[0] advances to its successor slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>);
        auto const ref{get_curr_val_from_operand_stack_cache<gc_details::wasm_funcref>(type...)};
        auto const out{gc_details::read_i31<Signed>(ref)};
        // [older operands][reserved i32 slot] ... stack allocation end
        // [safe                            ] validated maximum frame covers this i32 result.
        //                 ^^ type...[1u]
        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        type...[1u] += sizeof(out);
        // [older operands][i32] next free slot ... allocation end
        // [safe               ] ^^ type...[1u], possibly one-past.
    }

    enum class gc_aggregate_operation : unsigned
    {
        struct_new, struct_new_default, struct_get, struct_get_s, struct_get_u, struct_set,
        array_new, array_new_default, array_new_fixed, array_new_data, array_new_elem,
        array_get, array_get_s, array_get_u, array_set, array_len,
        array_init_data, array_init_elem, array_fill, array_copy
    };

    namespace gc_details
    {
        using store_t = ::uwvm2::uwvm::runtime::storage::gc_object_store;
        using module_t = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
        using slot_t = ::uwvm2::uwvm::runtime::storage::gc_object_value;
        using status_t = ::uwvm2::uwvm::runtime::storage::gc_object_status;
        using ref_t = ::uwvm2::object::global::wasm_global_ref_t;
        using kind_t = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
        using packed_t = ::uwvm2::parser::wasm::standard::wasm3::type::packed_kind;

        UWVM_GNU_COLD [[noreturn]] inline void trap(status_t status) noexcept
        {
            if(status == status_t::null_reference) { wasm1p1_details::null_reference_terminate(); }
            if(status == status_t::out_of_bounds)
            {
                if(trap_array_out_of_bounds_func != nullptr) { trap_array_out_of_bounds_func(); }
            }
            if(status == status_t::out_of_memory || status == status_t::size_overflow)
            {
                if(trap_gc_allocation_failure_func != nullptr) { trap_gc_allocation_failure_func(); }
            }
            // An invalid type/reference/value after validation represents a corrupt host
            // bridge or a VM bug. Never reinterpret an untrusted payload after this point.
            ::fast_io::fast_terminate();
        }
        [[nodiscard]] UWVM_ALWAYS_INLINE inline ::std::size_t value_width(
            ::uwvm2::parser::wasm::standard::wasm3::type::storage_type storage) noexcept
        {
            return slot_t::wasm_value_size(storage.packed == packed_t::none ?
                storage.value.kind : kind_t::i32);
        }
        template<gc_aggregate_operation Op, uwvm_interpreter_translate_option_t CompileOption,
                 uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline void execute(Type&... args) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            auto& ip{args...[0u]};
            auto& sp{args...[1u]};
            // [handler][store*][type/field/byte immediates][successor] validated bytecode
            // [safe                                                ]
            // ^^ ip: the dispatcher owns this complete handler pointer.
            if constexpr(CompileOption.is_tail_call) { ip += sizeof(uwvm_interpreter_opfunc_t<Type...>); }
            else { ip += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); }
            // [handler][store*][type/field/byte immediates][successor]
            // [safe   ][safe                                     ]
            //          ^^ ip: now at the complete store-pointer immediate.
            auto* store{wasm1p1_details::read_imm<store_t*>(ip)};
            // [consumed handler/store*][type/field/byte immediates][successor]
            // [safe                   ][safe                                   ]
            //                         ^^ ip: read_imm consumed exactly sizeof(store*).
            if(store == nullptr || !store->valid()) [[unlikely]] { ::fast_io::fast_terminate(); }
            ::std::uint_least32_t type_index{};
            if constexpr(Op != gc_aggregate_operation::array_len)
            {
                // [typeidx][optional field/byte immediates][successor] all emitted by translator.
                // ^^ ip before read_imm; after it points to the next complete immediate.
                type_index = wasm1p1_details::read_imm<::std::uint_least32_t>(ip);
                // [consumed typeidx][optional field/byte immediates][successor]
                //                 ^^ ip; this may be the successor if no more immediates.
            }
            ::std::uint_least32_t source_type_index{};
            if constexpr(Op == gc_aggregate_operation::array_copy)
            {
                // [source typeidx][successor] is the second array.copy type immediate.
                // ^^ ip advances by one complete u32 and then points to the successor.
                source_type_index = wasm1p1_details::read_imm<::std::uint_least32_t>(ip);
            }
            ::std::uint_least32_t field_index{};
            if constexpr(Op == gc_aggregate_operation::struct_get || Op == gc_aggregate_operation::struct_get_s ||
                         Op == gc_aggregate_operation::struct_get_u || Op == gc_aggregate_operation::struct_set)
            {
                // [fieldidx][successor] the compiler emitted one full u32 immediate.
                // ^^ ip before read_imm; after it points to the successor.
                field_index = wasm1p1_details::read_imm<::std::uint_least32_t>(ip);
                // [consumed fieldidx][successor] ^^ ip at successor.
            }
            ::std::size_t input_bytes{};
            if constexpr(Op == gc_aggregate_operation::struct_new || Op == gc_aggregate_operation::array_new_fixed)
            {
                // [input_bytes][fixed_count if any][successor] all complete immediates.
                // ^^ ip before read_imm; after it advances by sizeof(size_t).
                input_bytes = wasm1p1_details::read_imm<::std::size_t>(ip);
                // [consumed input_bytes][fixed_count if any][successor]
                //                       ^^ ip; possible successor for struct.new.
            }
            ::std::uint_least32_t fixed_count{};
            if constexpr(Op == gc_aggregate_operation::array_new_fixed)
            {
                // [fixed_count][successor] complete u32 immediate.
                // ^^ ip before read_imm; afterward at successor.
                fixed_count = wasm1p1_details::read_imm<::std::uint_least32_t>(ip);
                // [consumed fixed_count][successor] ^^ ip.
            }
            module_t const* module{};
            ::std::uint_least32_t segment_index{};
            if constexpr(Op == gc_aggregate_operation::array_new_data ||
                         Op == gc_aggregate_operation::array_new_elem ||
                         Op == gc_aggregate_operation::array_init_data ||
                         Op == gc_aggregate_operation::array_init_elem)
            {
                // [module*][segmentidx][successor] are complete translator-owned immediates.
                // ^^ ip advances by exactly one pointer-sized immediate.
                module = wasm1p1_details::read_imm<module_t const*>(ip);
                // [consumed module*][segmentidx][successor]
                //                    ^^ ip advances by one checked u32 immediate.
                segment_index = wasm1p1_details::read_imm<::std::uint_least32_t>(ip);
                // [consumed module*/segmentidx][successor]
                //                                   ^^ ip is at the complete next handler.
                if(module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            }
            auto const field_at = [&](::std::size_t index) noexcept
            {
                auto const* field{store->field_at(type_index, index)};
                if(field == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                return *field;
            };
            auto const pop_slot = [&](::std::size_t width) noexcept
            {
                if(width == 0uz || width > sizeof(slot_t)) [[unlikely]] { ::fast_io::fast_terminate(); }
                // [older operands][complete typed value] <- sp; validated stack
                // [safe                                    ] width is compiler-established.
                // [validated operand frame bytes] ... frame end
                // [safe                        ] unsafe (possibly one-past)
                // ^^ sp: typed-stack and maximum-frame proofs bound this move; no byte is read here.
                sp -= width;
                // [validated operand frame bytes] ... frame end
                // [safe                        ] unsafe (possibly one-past)
                // ^^ sp: typed-stack and maximum-frame proofs bound this move; no byte is read here.
                // [older operands][typed value] ... previous sp
                //                 ^^ sp: within the validated operand stack allocation.
                slot_t value{};
                ::std::memcpy(value.bits.data(), sp, width);
                return value;
            };
            auto const pop_ref = [&]() noexcept
            {
                auto const slot{pop_slot(sizeof(ref_t))};
                return slot.template as<ref_t>();
            };
            auto const pop_i32 = [&]() noexcept
            {
                auto const slot{pop_slot(sizeof(wasm_i32))};
                return slot.template as<::std::uint32_t>();
            };
            auto const push_slot = [&](slot_t const& slot, ::std::size_t width) noexcept
            {
                if(width == 0uz || width > sizeof(slot_t)) [[unlikely]] { ::fast_io::fast_terminate(); }
                // [older operands][reserved result bytes] ... frame allocation end
                // [safe                                  ] compiler maximum-stack proof covers width.
                //                 ^^ sp: first writable result byte.
                ::std::memcpy(sp, slot.bits.data(), width);
                sp += width;
                // [older operands][result] next free byte ... allocation end
                // [safe                   ] ^^ sp may be one-past; it is not dereferenced.
            };
            auto const push_ref = [&](ref_t ref) noexcept
            { push_slot(slot_t::reference(ref), sizeof(ref)); };
            auto const checked = [&](status_t status) noexcept
            { if(status != status_t::ok) [[unlikely]] { trap(status); } };

            if constexpr(Op == gc_aggregate_operation::struct_new)
            {
                // [older operands][all struct fields] <- sp; count and byte width
                // were fixed by type metadata at translation, including packed i32 inputs.
                // [allocated operand frame][live input values] | frame end
                // [safe writable frame bytes               ] | no guest-memory access
                // ^^ sp: validated type arity and generated frame allocation cover input_bytes; no guest-memory access occurs here.
                sp -= input_bytes;
                // [validated operand frame bytes] ... frame end
                // [safe                        ] unsafe (possibly one-past)
                // ^^ sp: typed-stack and maximum-frame proofs bound this move; no byte is read here.
                // [older operands][first struct field ... last field]
                //                 ^^ sp: complete source range of input_bytes bytes.
                ref_t result{};
                checked(store->struct_new_from_stack(type_index, sp, input_bytes, result));
                push_ref(result);
            }
            else if constexpr(Op == gc_aggregate_operation::struct_new_default)
            {
                ref_t result{};
                checked(store->struct_new_default(type_index, result));
                push_ref(result);
            }
            else if constexpr(Op == gc_aggregate_operation::struct_get ||
                              Op == gc_aggregate_operation::struct_get_s ||
                              Op == gc_aggregate_operation::struct_get_u)
            {
                auto const ref{pop_ref()};
                auto const field{field_at(field_index)};
                slot_t result{};
                checked(store->struct_get(ref, field_index, Op == gc_aggregate_operation::struct_get_s, result));
                push_slot(result, value_width(field.storage));
            }
            else if constexpr(Op == gc_aggregate_operation::struct_set)
            {
                auto const field{field_at(field_index)};
                auto const value{pop_slot(value_width(field.storage))};
                auto const ref{pop_ref()};
                checked(store->struct_set(ref, field_index, value));
            }
            else if constexpr(Op == gc_aggregate_operation::array_new)
            {
                auto const length{pop_i32()};
                auto const field{field_at(0uz)};
                auto const value{pop_slot(value_width(field.storage))};
                ref_t result{};
                checked(store->array_new(type_index, value, length, result));
                push_ref(result);
            }
            else if constexpr(Op == gc_aggregate_operation::array_new_default)
            {
                auto const length{pop_i32()};
                ref_t result{};
                checked(store->array_new_default(type_index, length, result));
                push_ref(result);
            }
            else if constexpr(Op == gc_aggregate_operation::array_new_fixed)
            {
                // [older operands][fixed element values] <- sp; translator checked
                // fixed_count * element width without overflow before emitting bytes.
                // [allocated operand frame][live input values] | frame end
                // [safe writable frame bytes               ] | no guest-memory access
                // ^^ sp: validated type arity and generated frame allocation cover input_bytes; no guest-memory access occurs here.
                sp -= input_bytes;
                // [validated operand frame bytes] ... frame end
                // [safe                        ] unsafe (possibly one-past)
                // ^^ sp: typed-stack and maximum-frame proofs bound this move; no byte is read here.
                // [older operands][first fixed element ... last element]
                //                 ^^ sp: complete input_bytes source range.
                ref_t result{};
                checked(store->array_new_fixed_from_stack(type_index, sp, input_bytes, fixed_count, result));
                push_ref(result);
            }
            else if constexpr(Op == gc_aggregate_operation::array_new_data ||
                              Op == gc_aggregate_operation::array_new_elem)
            {
                auto const length{pop_i32()};
                auto const source_offset{pop_i32()};
                ref_t result{};
                if constexpr(Op == gc_aggregate_operation::array_new_data)
                { checked(::uwvm2::uwvm::runtime::storage::uwvm2_gc_array_new_data(
                    store, module, type_index, segment_index, source_offset, length, ::std::addressof(result))); }
                else
                { checked(::uwvm2::uwvm::runtime::storage::uwvm2_gc_array_new_elem(
                    store, module, type_index, segment_index, source_offset, length, ::std::addressof(result))); }
                push_ref(result);
            }
            else if constexpr(Op == gc_aggregate_operation::array_get ||
                              Op == gc_aggregate_operation::array_get_s ||
                              Op == gc_aggregate_operation::array_get_u)
            {
                auto const index{pop_i32()};
                auto const ref{pop_ref()};
                auto const field{field_at(0uz)};
                slot_t result{};
                checked(store->array_get(ref, index, Op == gc_aggregate_operation::array_get_s, result));
                push_slot(result, value_width(field.storage));
            }
            else if constexpr(Op == gc_aggregate_operation::array_set)
            {
                auto const field{field_at(0uz)};
                auto const value{pop_slot(value_width(field.storage))};
                auto const index{pop_i32()};
                auto const ref{pop_ref()};
                checked(store->array_set(ref, index, value));
            }
            else if constexpr(Op == gc_aggregate_operation::array_len)
            {
                auto const ref{pop_ref()};
                ::std::size_t length{};
                checked(store->array_length(ref, length));
                if(length > (::std::numeric_limits<::std::uint32_t>::max)()) [[unlikely]]
                { ::fast_io::fast_terminate(); }
                push_slot(slot_t::i32(static_cast<::std::uint32_t>(length)), sizeof(wasm_i32));
            }
            else if constexpr(Op == gc_aggregate_operation::array_init_data ||
                              Op == gc_aggregate_operation::array_init_elem)
            {
                auto const length{pop_i32()};
                auto const source_offset{pop_i32()};
                auto const destination_offset{pop_i32()};
                auto const array{pop_ref()};
                if constexpr(Op == gc_aggregate_operation::array_init_data)
                { checked(::uwvm2::uwvm::runtime::storage::uwvm2_gc_array_init_data(
                    store, module, type_index, segment_index, ::std::addressof(array),
                    destination_offset, source_offset, length)); }
                else
                { checked(::uwvm2::uwvm::runtime::storage::uwvm2_gc_array_init_elem(
                    store, module, type_index, segment_index, ::std::addressof(array),
                    destination_offset, source_offset, length)); }
            }
            else if constexpr(Op == gc_aggregate_operation::array_fill)
            {
                auto const length{pop_i32()};
                auto const field{field_at(0uz)};
                auto const value{pop_slot(value_width(field.storage))};
                auto const offset{pop_i32()};
                auto const array{pop_ref()};
                if(array.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null)
                { checked(status_t::null_reference); }
                if(!store->reference_type_matches(array, {kind_t::reference, {type_index}, false}))
                { ::fast_io::fast_terminate(); }
                checked(store->array_fill(array, offset, value, length));
            }
            else if constexpr(Op == gc_aggregate_operation::array_copy)
            {
                auto const length{pop_i32()};
                auto const source_offset{pop_i32()};
                auto const source{pop_ref()};
                auto const destination_offset{pop_i32()};
                auto const destination{pop_ref()};
                if(destination.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null ||
                   source.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null)
                { checked(status_t::null_reference); }
                if(!store->reference_type_matches(destination, {kind_t::reference, {type_index}, false}) ||
                   !store->reference_type_matches(source, {kind_t::reference, {source_type_index}, false}))
                { ::fast_io::fast_terminate(); }
                checked(store->array_copy(destination, destination_offset, source, source_offset, length));
            }
        }

        template<uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline void execute_ref_eq(Type&... args) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            auto& ip{args...[0u]};
            auto& sp{args...[1u]};
            // [ref.eq handler][successor] validated bytecode
            // [safe          ][safe     ]
            // ^^ ip: dispatcher proved the complete handler pointer.
            if constexpr(CompileOption.is_tail_call) { ip += sizeof(uwvm_interpreter_opfunc_t<Type...>); }
            else { ip += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); }
            // [consumed handler][successor]
            //                   ^^ ip: the emitted successor is complete.
            // [older operands][lhs ref][rhs ref] <- sp; validation proved two complete reference values.
            // [safe                             ] sizeof(ref_t) is the interpreter's reference carrier width.
            // [validated operand frame bytes] ... frame end
            // [safe                        ] unsafe (possibly one-past)
            // ^^ sp: typed-stack and maximum-frame proofs bound this move; no byte is read here.
            sp -= 2uz * sizeof(ref_t);
            // [validated operand frame bytes] ... frame end
            // [safe                        ] unsafe (possibly one-past)
            // ^^ sp: typed-stack and maximum-frame proofs bound this move; no byte is read here.
            // [older operands][lhs ref][rhs ref]
            //                 ^^ sp: first byte of the complete two-reference range.
            ref_t lhs{};
            ref_t rhs{};
            ::std::memcpy(::std::addressof(lhs), sp, sizeof(lhs));
            // [lhs ref][rhs ref]
            //          ^^ sp + sizeof(lhs): this complete second reference is within the proven range.
            ::std::memcpy(::std::addressof(rhs), sp + sizeof(lhs), sizeof(rhs));
            bool equal{lhs.kind == rhs.kind};
            if(equal)
            {
                switch(lhs.kind)
                {
                    case ::uwvm2::object::global::wasm_ref_kind::wasm_null: break;
                    case ::uwvm2::object::global::wasm_ref_kind::wasm_i31:
                        equal = lhs.storage.wasm_i31.get_u() == rhs.storage.wasm_i31.get_u(); break;
                    case ::uwvm2::object::global::wasm_ref_kind::wasm_struct:
                    case ::uwvm2::object::global::wasm_ref_kind::wasm_array:
                        equal = lhs.storage.ptr == rhs.storage.ptr; break;
                    default: equal = false; break; // ref.eq accepts eqref, never a function or extern reference.
                }
            }
            auto const result{static_cast<wasm_i32>(equal)};
            // [older operands][now free former reference slots] ... validated frame allocation end
            // [safe                                    ] first four bytes are writable after both pops.
            //                 ^^ sp: first result byte.
            ::std::memcpy(sp, ::std::addressof(result), sizeof(result));
            sp += sizeof(result);
            // [older operands][i32 result] next free byte ... allocation end
            // [safe                      ] ^^ sp may be one-past and is not dereferenced here.
        }

        template<bool IsTest, uwvm_interpreter_translate_option_t CompileOption,
                 uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline void execute_ref_cast(Type&... args) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            auto& ip{args...[0u]};
            auto& sp{args...[1u]};
            // [ref.test/cast handler][store*][heap code][nullable][successor]
            // [safe                 ][safe                                ]
            // ^^ ip: dispatcher proved the complete handler pointer.
            if constexpr(CompileOption.is_tail_call) { ip += sizeof(uwvm_interpreter_opfunc_t<Type...>); }
            // [compiler-emitted handler/immediates/successor] ... stream end
            // [safe                                       ] unsafe (possibly one-past)
            // ^^ ip: validated dispatch or patched target bounds this move; no byte is read here.
            else { ip += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); }
            // [compiler-emitted handler/immediates/successor] ... stream end
            // [safe                                       ] unsafe (possibly one-past)
            // ^^ ip: validated dispatch or patched target bounds this move; no byte is read here.
            // [consumed handler][store*][heap code][nullable][successor]
            //                    ^^ ip: complete pointer immediate.
            auto* store{wasm1p1_details::read_imm<store_t*>(ip)};
            // [consumed handler/store*][heap code][nullable][successor]
            //                           ^^ ip: complete signed heap immediate.
            auto const heap_code{wasm1p1_details::read_imm<::std::int_least64_t>(ip)};
            // [consumed handler/store*/heap code][nullable][successor]
            //                                       ^^ ip: complete nullability byte.
            auto const nullable{wasm1p1_details::read_imm<bool>(ip)};
            // [consumed handler/immediates][successor]
            //                              ^^ ip: complete successor handler pointer.
            if(store == nullptr || !store->valid()) [[unlikely]] { ::fast_io::fast_terminate(); }
            // [older operands][complete reference] <- sp; validation proved a reference carrier.
            // [safe                             ]
            // [validated operand frame bytes] ... frame end
            // [safe                        ] unsafe (possibly one-past)
            // ^^ sp: typed-stack and maximum-frame proofs bound this move; no byte is read here.
            sp -= sizeof(ref_t);
            // [validated operand frame bytes] ... frame end
            // [safe                        ] unsafe (possibly one-past)
            // ^^ sp: typed-stack and maximum-frame proofs bound this move; no byte is read here.
            // [older operands][reference]
            //                 ^^ sp: complete readable reference slot.
            ref_t reference{};
            ::std::memcpy(::std::addressof(reference), sp, sizeof(reference));
            auto const matches{store->reference_type_matches(reference,
                {kind_t::reference, {heap_code}, nullable})};
            if constexpr(IsTest)
            {
                auto const result{static_cast<wasm_i32>(matches)};
                // [older operands][freed reference bytes] ... validated frame allocation end
                // [safe                               ] first sizeof(i32) bytes are writable.
                //                 ^^ sp: first result byte.
                ::std::memcpy(sp, ::std::addressof(result), sizeof(result));
                sp += sizeof(result);
                // [older operands][i32 result] next free byte ... frame end
                // [safe                      ] ^^ sp may be one-past, never dereferenced here.
            }
            else
            {
                if(!matches) [[unlikely]]
                {
                    if(trap_cast_failure_func != nullptr) { trap_cast_failure_func(); }
                    ::fast_io::fast_terminate();
                }
                // [older operands][same reference] ... validated frame allocation end
                // [safe                        ] no payload move or reinterpretation occurs.
                sp += sizeof(ref_t);
                // [older operands][same reference] next free byte ... frame end
                // [safe                        ] ^^ sp restored to the validated pre-cast position.
            }
        }

        template<bool BranchOnSuccess, uwvm_interpreter_translate_option_t CompileOption,
                 uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline void execute_br_on_cast(Type&... args) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            auto& ip{args...[0u]};
            auto& sp{args...[1u]};
            // [handler][store*][target heap][nullable][patched target][successor]
            // [safe                                                       ]
            // ^^ ip: dispatcher proved the complete handler slot.
            if constexpr(CompileOption.is_tail_call) { ip += sizeof(uwvm_interpreter_opfunc_t<Type...>); }
            // [compiler-emitted handler/immediates/successor] ... stream end
            // [safe                                       ] unsafe (possibly one-past)
            // ^^ ip: validated dispatch or patched target bounds this move; no byte is read here.
            else { ip += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); }
            // [compiler-emitted handler/immediates/successor] ... stream end
            // [safe                                       ] unsafe (possibly one-past)
            // ^^ ip: validated dispatch or patched target bounds this move; no byte is read here.
            // [consumed handler][store*][target heap][nullable][patched target][successor]
            //                    ^^ ip: the store-pointer immediate is complete.
            auto* store{wasm1p1_details::read_imm<store_t*>(ip)};
            // [consumed handler/store*][target heap][nullable][patched target][successor]
            //                           ^^ ip: the signed heap immediate is complete.
            auto const heap_code{wasm1p1_details::read_imm<::std::int_least64_t>(ip)};
            // [consumed handler/store*/heap][nullable][patched target][successor]
            //                                ^^ ip: the nullability byte is complete.
            auto const nullable{wasm1p1_details::read_imm<bool>(ip)};
            // [consumed immediates][patched target][successor]
            //                       ^^ ip: translator emitted a complete target pointer.
            auto const target{wasm1p1_details::read_imm<::std::byte const*>(ip)};
            // [consumed handler/immediates][successor]
            //                              ^^ ip: complete fallthrough handler pointer.
            if(store == nullptr || !store->valid()) [[unlikely]] { ::fast_io::fast_terminate(); }
            // [older operands][complete reference] <- sp; validation proved this live slot.
            // [safe                             ]
            //                 ^^ sp - sizeof(ref_t): read-only, with no stack movement on either edge.
            ref_t reference{};
            ::std::memcpy(::std::addressof(reference), sp - sizeof(ref_t), sizeof(reference));
            auto const matches{store->reference_type_matches(reference,
                {kind_t::reference, {heap_code}, nullable})};
            if(matches == BranchOnSuccess)
            {
                // [validated target handler] ... bytecode end
                // ^^ ip changes only to the translator-patched live handler slot.
                // [compiler-emitted bytecode slots][successor] | stream end
                // [complete typed slots                    ] | no guest Wasm read
                // ^^ ip: the emitted target slot was fixed up to a live handler; this branch copies that address.
                ip = target;
                // [compiler-emitted handler/immediates/successor] ... stream end
                // [safe                                       ] unsafe (possibly one-past)
                // ^^ ip: validated dispatch or patched target bounds this move; no byte is read here.
            }
        }

        template<bool ToAny, uwvm_interpreter_translate_option_t CompileOption,
                 uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline void execute_extern_convert(Type&... args) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            auto& ip{args...[0u]};
            auto& sp{args...[1u]};
            // [handler][store*][successor] validated bytecode
            // [safe                       ]
            // ^^ ip: dispatcher proved the complete handler slot.
            if constexpr(CompileOption.is_tail_call) { ip += sizeof(uwvm_interpreter_opfunc_t<Type...>); }
            // [compiler-emitted handler/immediates/successor] ... stream end
            // [safe                                       ] unsafe (possibly one-past)
            // ^^ ip: validated dispatch or patched target bounds this move; no byte is read here.
            else { ip += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); }
            // [consumed handler][store*][successor]
            //                    ^^ ip: complete store-pointer immediate.
            auto* store{wasm1p1_details::read_imm<store_t*>(ip)};
            // [consumed handler/store*][successor]
            //                           ^^ ip: complete successor handler pointer.
            if(store == nullptr || !store->valid()) [[unlikely]] { ::fast_io::fast_terminate(); }
            // [older operands][complete reference] <- sp; static validation proved this slot.
            // [safe                             ]
            //                 ^^ slot: in-frame top reference; SP never changes.
            auto* slot{sp - sizeof(ref_t)};
            ref_t source{};
            ::std::memcpy(::std::addressof(source), slot, sizeof(source));
            ref_t result{};
            auto const status{ToAny ? store->any_convert_extern(source, result) :
                                      store->extern_convert_any(source, result)};
            if(status != status_t::ok) [[unlikely]] { trap(status); }
            // [older operands][same 16-byte reference slot] <- sp
            // [safe                                      ] slot remains live and writable.
            ::std::memcpy(slot, ::std::addressof(result), sizeof(result));
        }
    }

    template<gc_aggregate_operation Op, uwvm_interpreter_translate_option_t CompileOption,
             uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_gc_aggregate(Type... type) UWVM_THROWS
    {
        gc_details::execute<Op, CompileOption>(type...);
        // [consumed handler and immediates][successor] validated bytecode
        //                                  ^^ type...[0]: execute left IP at a full successor.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }
    template<gc_aggregate_operation Op, uwvm_interpreter_translate_option_t CompileOption,
             uwvm_int_stack_top_type... Type>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_gc_aggregate(Type&... type) UWVM_THROWS
    { gc_details::execute<Op, CompileOption>(type...); }

    template<uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_gc_ref_eq(Type... type) UWVM_THROWS
    {
        gc_details::execute_ref_eq<CompileOption>(type...);
        // [consumed ref.eq handler][successor] validated bytecode
        //                          ^^ type...[0]: complete successor handler pointer.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }
    template<uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_gc_ref_eq(Type&... type) UWVM_THROWS
    { gc_details::execute_ref_eq<CompileOption>(type...); }

    template<bool IsTest, uwvm_interpreter_translate_option_t CompileOption,
             uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_gc_ref_cast(Type... type) UWVM_THROWS
    {
        gc_details::execute_ref_cast<IsTest, CompileOption>(type...);
        // [consumed cast/test handler and immediates][successor]
        //                                        ^^ type...[0]: complete successor handler pointer.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }
    template<bool IsTest, uwvm_interpreter_translate_option_t CompileOption,
             uwvm_int_stack_top_type... Type>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_gc_ref_cast(Type&... type) UWVM_THROWS
    { gc_details::execute_ref_cast<IsTest, CompileOption>(type...); }

    template<bool BranchOnSuccess, uwvm_interpreter_translate_option_t CompileOption,
             uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_gc_br_on_cast(Type... type) UWVM_THROWS
    {
        gc_details::execute_br_on_cast<BranchOnSuccess, CompileOption>(type...);
        // [consumed branch immediates][selected successor] validated bytecode
        //                             ^^ type...[0]: complete handler on either edge.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }
    template<bool BranchOnSuccess, uwvm_interpreter_translate_option_t CompileOption,
             uwvm_int_stack_top_type... Type>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_gc_br_on_cast(Type&... type) UWVM_THROWS
    { gc_details::execute_br_on_cast<BranchOnSuccess, CompileOption>(type...); }

    template<bool ToAny, uwvm_interpreter_translate_option_t CompileOption,
             uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_gc_extern_convert(Type... type) UWVM_THROWS
    {
        gc_details::execute_extern_convert<ToAny, CompileOption>(type...);
        // [consumed conversion immediates][successor] validated bytecode
        //                                 ^^ type...[0]: complete next handler slot.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }
    template<bool ToAny, uwvm_interpreter_translate_option_t CompileOption,
             uwvm_int_stack_top_type... Type>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_gc_extern_convert(Type&... type) UWVM_THROWS
    { gc_details::execute_extern_convert<ToAny, CompileOption>(type...); }

    namespace translate
    {
        namespace details
        {
            struct ref_i31_op
            {
                template<uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_ref_i31<Opt, Pos, Type...>; }
            };
            template<bool Signed> struct i31_get_op
            {
                template<uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i31_get<Opt, Signed, Pos, Type...>; }
            };
        }

        template<uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_ref_i31_fptr(
            uwvm_interpreter_stacktop_currpos_t const& position) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_stack<CompileOption,
                    CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos,
                    details::ref_i31_op, Type...>(position.i32_stack_top_curr_pos);
            }
            else { return uwvmint_ref_i31<CompileOption, 0uz, Type...>; }
        }
        template<uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_ref_i31_fptr(
            uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_ref_i31<CompileOption, 0uz, Type...>; }
        template<uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_ref_i31_fptr_from_tuple(
            uwvm_interpreter_stacktop_currpos_t const& position, ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        { return get_uwvmint_ref_i31_fptr<CompileOption, Type...>(position); }

        template<uwvm_interpreter_translate_option_t CompileOption, bool Signed, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i31_get_fptr(
            uwvm_interpreter_stacktop_currpos_t const& position) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_stack<CompileOption,
                    CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos,
                    details::i31_get_op<Signed>, Type...>(position.i32_stack_top_curr_pos);
            }
            else { return uwvmint_i31_get<CompileOption, Signed, 0uz, Type...>; }
        }
        template<uwvm_interpreter_translate_option_t CompileOption, bool Signed, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i31_get_fptr(
            uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i31_get<CompileOption, Signed, 0uz, Type...>; }
        template<uwvm_interpreter_translate_option_t CompileOption, bool Signed, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_i31_get_fptr_from_tuple(
            uwvm_interpreter_stacktop_currpos_t const& position, ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        { return get_uwvmint_i31_get_fptr<CompileOption, Signed, Type...>(position); }

        template<gc_aggregate_operation Op, uwvm_interpreter_translate_option_t CompileOption,
                 uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_gc_aggregate_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        { return uwvmint_gc_aggregate<Op, CompileOption, Type...>; }
        template<gc_aggregate_operation Op, uwvm_interpreter_translate_option_t CompileOption,
                 uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_gc_aggregate_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        { return uwvmint_gc_aggregate<Op, CompileOption, Type...>; }

        template<uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_gc_ref_eq_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        { return uwvmint_gc_ref_eq<CompileOption, Type...>; }
        template<uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_gc_ref_eq_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        { return uwvmint_gc_ref_eq<CompileOption, Type...>; }

        template<bool IsTest, uwvm_interpreter_translate_option_t CompileOption,
                 uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_gc_ref_cast_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        { return uwvmint_gc_ref_cast<IsTest, CompileOption, Type...>; }
        template<bool IsTest, uwvm_interpreter_translate_option_t CompileOption,
                 uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_gc_ref_cast_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        { return uwvmint_gc_ref_cast<IsTest, CompileOption, Type...>; }

        template<bool BranchOnSuccess, uwvm_interpreter_translate_option_t CompileOption,
                 uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_gc_br_on_cast_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        { return uwvmint_gc_br_on_cast<BranchOnSuccess, CompileOption, Type...>; }
        template<bool BranchOnSuccess, uwvm_interpreter_translate_option_t CompileOption,
                 uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_gc_br_on_cast_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        { return uwvmint_gc_br_on_cast<BranchOnSuccess, CompileOption, Type...>; }

        template<bool ToAny, uwvm_interpreter_translate_option_t CompileOption,
                 uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_gc_extern_convert_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        { return uwvmint_gc_extern_convert<ToAny, CompileOption, Type...>; }
        template<bool ToAny, uwvm_interpreter_translate_option_t CompileOption,
                 uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        [[nodiscard]] inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_gc_extern_convert_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        { return uwvmint_gc_extern_convert<ToAny, CompileOption, Type...>; }
    }
}
#endif

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

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
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/runtime/compiler/shared/wasm_memory64.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include "define.h"
# include "memory.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    namespace details::memory64_bulk
    {
        namespace wide = ::uwvm2::runtime::compiler::shared::wasm_memory64;

        template<bool Address64, uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr ::std::uint_least64_t pop(Type&... args) noexcept
        {
            using value_type = ::std::conditional_t<Address64, wasm_i64, wasm_i32>;
            using unsigned_type = ::std::make_unsigned_t<value_type>;
            // [older operands][value_type] <- SP; validator proved this operand.
            // [safe                      ] translator flushed the bulk operands.
            auto const value{get_curr_val_from_operand_stack_cache<value_type>(args...)};
            // [older operands] <- SP; pop consumed exactly sizeof(value_type).
            return static_cast<unsigned_type>(value);
        }

        // The current diagnostic ABI stores a native access size. Saturate only
        // that diagnostic field AFTER rejecting the complete wide range.
        [[nodiscard]] inline constexpr ::std::size_t diagnostic_length(::std::uint_least64_t length) noexcept
        {
            constexpr auto maximum{::std::numeric_limits<::std::size_t>::max()};
            return length > maximum ? maximum : static_cast<::std::size_t>(length);
        }

        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr void execute_init(Type&... args) UWVM_THROWS
        {
            using data_storage = ::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t;
            // [handler][memory*][data instance*][successor] owned bytecode
            // [safe                                      ]
            // ^^ IP: translation proved both owner immediates and successor.
            if constexpr(Option.is_tail_call) { args...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>); }
            else { args...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); }
            // [handler][memory*][data instance*][successor]
            //          ^^ IP after the complete handler slot.
            auto const memory{read_imm<native_memory_t*>(args...[0])};
            // [handler][memory*][data instance*][successor]
            //                   ^^ IP after the complete native memory pointer.
            auto const data{read_imm<data_storage*>(args...[0])};
            // [handler][memory*][data instance*][successor]
            //                                    ^^ IP after the complete data pointer.
            // Core 3 memory.init is [i64 destination, i32 source, i32 length].
            auto const length{pop<false>(args...)};
            // [older][i64 destination][i32 source] <- SP; four length bytes consumed.
            auto const source{pop<false>(args...)};
            // [older][i64 destination] <- SP; four source bytes consumed.
            auto const destination{pop<true>(args...)};
            // [older] <- SP; complete eight-byte destination consumed.
            auto const payload{::uwvm2::uwvm::runtime::storage::load_wasm_data_segment_payload(data->data)};
            if((payload.byte_begin == nullptr) != (payload.byte_end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
            // [immutable module bytes ...] end or both null after drop.
            //  ^^ payload.begin           ^^ payload.end, same owned allocation.
            auto const source_size{payload.byte_begin == nullptr ? 0uz : static_cast<::std::size_t>(payload.byte_end - payload.byte_begin)};
            enter_memory_operation_memory_lock(*memory); // stable mmap: no pin
            auto const destination_size{load_memory_length_for_oob_unlocked(*memory)};
            auto const status{wide::copy(memory->memory_begin, destination_size, payload.byte_begin, source_size,
                                         destination, source, length)};
            if(status != wide::bulk_error::none) [[unlikely]]
            {
                bool const source_failed{status == wide::bulk_error::source};
                memory_oob_terminate(*memory, 0u, {source_failed ? source : destination, false},
                    source_failed ? source_size : destination_size, diagnostic_length(length));
            }
            exit_memory_operation_memory_lock(*memory);
        }

        template<bool Copy, bool Destination64, bool Source64,
                 uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr void execute(Type&... args) UWVM_THROWS
        {
            // Bulk operations follow the existing compiler convention: materialize
            // all consumed ring operands first. The copied byte range dominates
            // their runtime cost; successor dispatch remains a mandatory tail call.
            // [handler][destination memory][source memory if Copy][successor] ...
            // [safe                                                         ] owned bytecode
            // ^^ IP; immediate slots were emitted by the validated translator.
            if constexpr(Option.is_tail_call) { args...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>); }
            else { args...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); }
            // [handler][destination memory][source memory if Copy][successor]
            //          ^^ IP after the proved dispatch slot.
            auto const destination_memory{read_imm<native_memory_t*>(args...[0])};
            // [handler][destination memory][source memory if Copy][successor]
            //                               ^^ IP after the complete pointer slot.
            if constexpr(Copy)
            {
                auto const source_memory{read_imm<native_memory_t*>(args...[0])};
                // [handler][destination memory][source memory][successor]
                //                                             ^^ IP after the complete source slot.
                // Core 3: copy length is i64 only for an i64/i64 pair. Mixed
                // memories retain distinct destination/source operand widths.
                auto const length{pop<Destination64 && Source64>(args...)};
                // [older][destination][source] <- SP after the validated length.
                auto const source{pop<Source64>(args...)};
                // [older][destination] <- SP after the validated source.
                auto const destination{pop<Destination64>(args...)};
                // [older] <- SP after the validated destination.
                // A total native object order prevents opposite-direction copies
                // from inverting allocator relocation locks. Aliases pin once.
                bool const source_first{reinterpret_cast<::std::uintptr_t>(source_memory) < reinterpret_cast<::std::uintptr_t>(destination_memory)};
                auto& first{source_first ? *source_memory : *destination_memory};
                auto& second{source_first ? *destination_memory : *source_memory};
                enter_memory_operation_memory_lock(first);
                if(source_memory != destination_memory) { enter_memory_operation_memory_lock(second); }
                auto const destination_size{load_memory_length_for_oob_unlocked(*destination_memory)};
                auto const source_size{load_memory_length_for_oob_unlocked(*source_memory)};
                auto const status{wide::copy(destination_memory->memory_begin, destination_size,
                    source_memory->memory_begin, source_size, destination, source, length)};
                if(status != wide::bulk_error::none) [[unlikely]]
                {
                    bool const source_failed{status == wide::bulk_error::source};
                    memory_oob_terminate(source_failed ? *source_memory : *destination_memory, 0u,
                        {source_failed ? source : destination, false}, source_failed ? source_size : destination_size,
                        diagnostic_length(length));
                }
                if(source_memory != destination_memory) { exit_memory_operation_memory_lock(second); }
                exit_memory_operation_memory_lock(first);
            }
            else
            {
                auto const length{pop<Destination64>(args...)};
                // [older][destination][i32 value] <- SP after the validated length.
                auto const value{static_cast<::std::uint_least32_t>(pop<false>(args...))};
                // [older][destination] <- SP after the four-byte fill value.
                auto const destination{pop<Destination64>(args...)};
                // [older] <- SP after the validated destination.
                enter_memory_operation_memory_lock(*destination_memory);
                auto const destination_size{load_memory_length_for_oob_unlocked(*destination_memory)};
                if(wide::fill(destination_memory->memory_begin, destination_size, destination, value, length) != wide::bulk_error::none) [[unlikely]]
                { memory_oob_terminate(*destination_memory, 0u, {destination, false}, destination_size, diagnostic_length(length)); }
                exit_memory_operation_memory_lock(*destination_memory);
            }
        }
    }

    template<bool Copy, bool Destination64, bool Source64,
             uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires(Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_bulk(Type... args) UWVM_THROWS
    {
        details::memory64_bulk::execute<Copy, Destination64, Source64, Option>(args...);
        // [consumed bulk operation][successor] ... owned bytecode
        // [safe                   ][safe     ]
        //                           ^^ IP from execute; no pointer movement below.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), args...[0], sizeof(next));
        UWVM_MUSTTAIL return next(args...);
    }

    template<bool Copy, bool Destination64, bool Source64,
             uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires(!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_bulk(Type&... args) UWVM_THROWS
    { details::memory64_bulk::execute<Copy, Destination64, Source64, Option>(args...); }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires(Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_init(Type... args) UWVM_THROWS
    {
        details::memory64_bulk::execute_init<Option>(args...);
        // [consumed init][successor] ... owned bytecode
        // [safe         ][safe     ]
        //                ^^ IP: complete next slot, no further pointer advance.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), args...[0], sizeof(next));
        UWVM_MUSTTAIL return next(args...);
    }
    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires(!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory64_init(Type&... args) UWVM_THROWS
    { details::memory64_bulk::execute_init<Option>(args...); }

    namespace translate
    {
        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_memory64_init_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_memory64_init<Option, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_memory64_init<Option, Type...>); }
        }

        template<bool Copy, bool Destination64, bool Source64,
                 uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_memory64_bulk_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_memory64_bulk<Copy, Destination64, Source64, Option, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_memory64_bulk<Copy, Destination64, Source64, Option, Type...>); }
        }
    }
}
#endif
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

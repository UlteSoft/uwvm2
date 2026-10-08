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
# include <concepts>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/object/impl.h>
# include "define.h"
# include "storage.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    namespace details
    {
        inline constexpr void request_tail_transfer(::std::byte const* ip, ::std::byte const* top,
            ::std::byte const* locals, ::std::size_t handler_bytes) noexcept
        {
            // [handler][module, function, argument bytes] end
            // [safe                                    ]
            // ^^ ip; compiler emits all three fields atomically.
            ip += handler_bytes;
            // [handler][module, function, argument bytes] end
            //          ^^ ip; complete three-field immediate remains.
            ::std::size_t immediate[3];
            ::std::memcpy(immediate, ip, sizeof(immediate));
            // [activation pointer][padding?][params + locals ...] frame end
            // [safe                                             ]
            //                               ^^ locals; only has_tail_transfer
            // functions emit this handler and their owner reserves this prefix.
            auto const header{locals - sizeof(void*)};
            // [activation pointer][params + locals ...] frame end
            // [safe                                     ]
            // ^^ header; memcpy tolerates an unaligned prefix.
            void* activation;
            ::std::memcpy(::std::addressof(activation), header, sizeof(activation));
            if(activation == nullptr || tail_transfer_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            // [other operands][callee arguments] top
            // [safe                             ]; validation checks the full tuple.
            auto const arguments{top - immediate[2]};
            tail_transfer_func(activation, immediate[0], immediate[1], arguments, immediate[2]);
        }
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_return_call_transfer(Type... type) UWVM_THROWS
    {
        details::request_tail_transfer(type...[0], type...[1], type...[2], sizeof(uwvm_interpreter_opfunc_t<Type...>));
        // End this opfunc chain. Its native/frame owner loops only AFTER this
        // return, so neither Wasm nor native frames accumulate across tail calls.
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_return_call_transfer(Type&... type) UWVM_THROWS
    {
        details::request_tail_transfer(type...[0], type...[1], type...[2], sizeof(uwvm_interpreter_opfunc_byref_t<Type...>));
        // [bytecode end] no further reads: nullptr is the byref return sentinel.
        type...[0] = nullptr;
    }

    namespace translate
    {
        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_return_call_transfer_fptr_from_tuple(::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call) { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_return_call_transfer<Option, Type...>); }
            else { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_return_call_transfer<Option, Type...>); }
        }
    }

    namespace details
    {
        inline constexpr void request_indirect_tail_transfer(::std::byte const* ip, ::std::byte const* top,
            ::std::byte const* locals, ::std::size_t handler_bytes) noexcept
        {
            // [handler][module, type, table, argument bytes] end
            // [safe                                    ]
            // ^^ ip; compiler emits all four fields atomically.
            ip += handler_bytes;
            // [handler][module, type, table, argument bytes] end
            //          ^^ ip; complete four-field immediate remains.
            ::std::size_t immediate[4];
            ::std::memcpy(immediate, ip, sizeof(immediate));
            // [activation pointer][padding?][params + locals ...] frame end
            // [safe                                             ]
            //                               ^^ locals; only has_tail_transfer
            // functions emit this handler and their owner reserves this prefix.
            auto const header{locals - sizeof(void*)};
            // [activation pointer][params + locals ...] frame end
            // [safe                                     ]
            // ^^ header; memcpy tolerates an unaligned prefix.
            void* activation;
            ::std::memcpy(::std::addressof(activation), header, sizeof(activation));
            if(activation == nullptr || indirect_tail_transfer_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            // [other operands][callee arguments] top
            // [safe                             ]; validation checks the full tuple.
            // Keep the dynamic selector above the args. The resolver consumes
            // that i32 and performs bounds/null/signature checks before transfer.
            indirect_tail_transfer_func(activation, immediate[0], immediate[1], immediate[2], top, immediate[3]);
        }
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_return_call_indirect_transfer(Type... type) UWVM_THROWS
    {
        details::request_indirect_tail_transfer(type...[0], type...[1], type...[2], sizeof(uwvm_interpreter_opfunc_t<Type...>));
        // End this opfunc chain. Its native/frame owner loops only AFTER this
        // return, so neither Wasm nor native frames accumulate across tail calls.
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_return_call_indirect_transfer(Type&... type) UWVM_THROWS
    {
        details::request_indirect_tail_transfer(type...[0], type...[1], type...[2], sizeof(uwvm_interpreter_opfunc_byref_t<Type...>));
        // [bytecode end] no further reads: nullptr is the byref return sentinel.
        type...[0] = nullptr;
    }

    namespace translate
    {
        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_return_call_indirect_transfer_fptr_from_tuple(::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call) { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_return_call_indirect_transfer<Option, Type...>); }
            else { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_return_call_indirect_transfer<Option, Type...>); }
        }
    }

    namespace details
    {
        inline constexpr void request_ref_tail_transfer(::std::byte const* ip, ::std::byte const* top,
            ::std::byte const* locals, ::std::size_t handler_bytes) noexcept
        {
            // return_call_ref [handler][module, type, argument bytes] bytecode end
            // [safe          ^^^^^^^] compiler emitted the complete handler slot.
            //                 ^^ ip; advance to the first complete immediate.
            ip += handler_bytes;
            // return_call_ref [handler][module, type, argument bytes] bytecode end
            //                         [safe                        ]
            //                         ^^ ip; all three fields are compiler-owned.
            ::std::size_t immediate[3];
            ::std::memcpy(immediate, ip, sizeof(immediate));
            // [activation pointer][params + locals ...] frame end
            // [safe                                      ] locals is the validated frame's local base.
            // ^^ header; only functions marked has_tail_transfer reserve this prefix.
            auto const header{locals - sizeof(void*)};
            void* activation;
            ::std::memcpy(::std::addressof(activation), header, sizeof(activation));
            if(activation == nullptr || ref_tail_transfer_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            // [callee arguments][wasm_funcref_t] operand_top
            // [safe                                      ] translator validated and materialized both operands.
            //                                      ^^ top; callback synchronously consumes the ref and copies arguments.
            ref_tail_transfer_func(activation, immediate[0], immediate[1], top, immediate[2]);
        }
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_return_call_ref_transfer(Type... type) UWVM_THROWS
    {
        details::request_ref_tail_transfer(type...[0], type...[1], type...[2], sizeof(uwvm_interpreter_opfunc_t<Type...>));
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_return_call_ref_transfer(Type&... type) UWVM_THROWS
    {
        details::request_ref_tail_transfer(type...[0], type...[1], type...[2], sizeof(uwvm_interpreter_opfunc_byref_t<Type...>));
        // [bytecode end] the frame owner recognizes this sentinel; no further read occurs here.
        type...[0] = nullptr;
    }

    namespace translate
    {
        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_return_call_ref_transfer_fptr_from_tuple(::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_return_call_ref_transfer<Option, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_return_call_ref_transfer<Option, Type...>); }
        }
    }

    namespace details
    {
        // Small Wasm tuples are common in tail recursion. Fixed-size memcpy/
        // memset lower to unaligned scalar/vector accesses without libc calls.
        // The first/last chunks can overlap each other; source and destination
        // regions are disjoint, and every chunk lies inside the checked extent.
        template<bool Zero>
        UWVM_ALWAYS_INLINE inline constexpr void self_tail_small_bytes(
            ::std::byte* destination, ::std::byte const* source, ::std::size_t size) noexcept
        {
            if(size == 0uz) { return; }
            auto const chunks{[&]<::std::size_t N>() constexpr noexcept
            {
                // [destination bytes ...] end; size >= N, same bound for source.
                // [safe                 ]; final chunk ends exactly at end.
                auto const last_destination{destination + (size - N)};
                if constexpr(Zero)
                {
                    ::std::memset(destination, 0, N);
                    ::std::memset(last_destination, 0, N);
                }
                else
                {
                    // [source bytes ...] end; size >= N was checked by caller.
                    // [safe            ]; no read extends past the parameter tuple.
                    auto const last_source{source + (size - N)};
                    ::std::memcpy(destination, source, N);
                    ::std::memcpy(last_destination, last_source, N);
                }
            }};
            if(size <= 32uz)
            {
                if(size >= 16uz) { chunks.template operator()<16uz>(); return; }
                if(size >= 8uz) { chunks.template operator()<8uz>(); return; }
                if(size >= 4uz) { chunks.template operator()<4uz>(); return; }
            }
            if constexpr(Zero) { ::std::memset(destination, 0, size); }
            else { ::std::memcpy(destination, source, size); }
        }

        // Compiler-owned bytecode: [handler][param bytes][zero bytes][stack bytes][entry pointer].
        // The three extents were checked while constructing this function's frame.
        inline constexpr void reset_self_tail_call_frame(::std::byte const*& ip,
            ::std::byte*& top, ::std::byte* locals, ::std::size_t handler_bytes) noexcept
        {
            // [handler] [three size_t fields] [entry pointer]
            // [safe                                      ]
            // ^^ ip
            ip += handler_bytes;
            // [handler] [three size_t fields] [entry pointer]
            //           ^^ ip; all three fields are emitted together by the compiler.
            ::std::size_t extents[3];
            ::std::memcpy(extents, ip, sizeof(extents));
            ip += sizeof(extents);
            // [handler] [three size_t fields] [entry pointer]
            //                                 ^^ ip; complete entry-pointer slot exists.
            // [operand base ... discarded operands ... parameters] top
            // [safe                                                ]
            //                                         ^^ args; extents[0] <= extents[2].
            auto const args{top - extents[0]};
            // Locals and operands are disjoint regions of the current frame.
            // [parameters][declared locals][internal temporaries] local end
            // [safe                                               ]
            // ^^ locals; parameter byte extent matches this function's signature.
            self_tail_small_bytes<false>(locals, args, extents[0]);
            if(extents[1] != 0uz)
            {
                auto const zero_begin{locals + extents[0]};
                // [parameters][declared locals][internal temporaries]
                // [safe                                            ]
                //             ^^ zero_begin; extents[1] excludes internal scratch.
                self_tail_small_bytes<true>(zero_begin, nullptr, extents[1]);
            }
            top -= extents[2];
            // [operand base ... previously live operands] old top
            // [safe                                     ]
            // ^^ top; reset to this frame's allocated operand base.
            // [handler][three extents][entry pointer] end
            // [safe                                ]; pointer relocation was
            // patched only after the complete bytecode allocation was finalized.
            ::std::byte const* entry;
            ::std::memcpy(::std::addressof(entry), ip, sizeof(entry));
            ip = entry;
            // [function entry handler] ... bytecode end
            // [safe                  ]; an empty Wasm body still has end/return.
            // ^^ ip: jump directly, without a second branch-handler dispatch.
        }
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_return_call_self(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        details::reset_self_tail_call_frame(type...[0], type...[1], type...[2], sizeof(uwvm_interpreter_opfunc_t<Type...>));
        // A fresh function entry has an empty register ring. Incoming cached
        // values were spilled before return_call and are dead after reset. Do
        // not preserve/spill them across memcpy/memset merely to pass them on.
        [&]<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
        { ((type...[I + 3uz] = Type...[I + 3uz]{}), ...); }(::std::make_index_sequence<sizeof...(Type) - 3uz>{});
        // [entry handler] ...
        // [safe             ]; reset helper installed the relocated entry.
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_return_call_self(Type&... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        details::reset_self_tail_call_frame(type...[0], type...[1], type...[2], sizeof(uwvm_interpreter_opfunc_byref_t<Type...>));
        // [entry handler] ...
        // [safe             ]; byref dispatch reads it on its next iteration.
    }

    namespace translate
    {
        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_return_call_self_fptr_from_tuple(::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call) { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_return_call_self<Option, Type...>); }
            else { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_return_call_self<Option, Type...>); }
        }
    }

    namespace details
    {
        /// @brief Runtime call bridge: performs a single Wasm function call.
        /// @details
        /// - Stack-top optimization: not applicable (this is only a thin wrapper around `call_func`; stack-top caching is constrained by `uwvmint_call`).
        /// - `type[0]` layout: not applicable (this helper does not read/advance the bytecode stream pointer).
        /// @note `call_func` must be set during interpreter initialization; debug builds may trap on null.
        [[nodiscard]] UWVM_GNU_HOT inline constexpr ::std::byte*
            call(::std::size_t curr_module_id, ::std::size_t call_function, ::std::byte* uwvm_int_operand_stack_top) UWVM_THROWS
        {
            if(::uwvm2::runtime::compiler::uwvm_int::optable::call_func == nullptr) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif

                ::fast_io::fast_terminate();
            }

            return ::uwvm2::runtime::compiler::uwvm_int::optable::call_func(curr_module_id, call_function, uwvm_int_operand_stack_top);
        }

        /// @brief Runtime call bridge: performs a single Wasm `call_indirect`.
        /// @details
        /// - Stack-top optimization: not applicable (same constraints as `call`).
        /// - Bytecode layout: not applicable (this helper does not read/advance the bytecode stream pointer).
        /// @note `call_indirect_func` must be set during interpreter initialization; debug builds may trap on null.
        [[nodiscard]] UWVM_GNU_HOT inline constexpr ::std::byte* call_indirect(::std::size_t curr_module_id,
                                                                               ::std::size_t type_index,
                                                                               ::std::size_t table_index,
                                                                               ::std::byte* uwvm_int_operand_stack_top) UWVM_THROWS
        {
            if(::uwvm2::runtime::compiler::uwvm_int::optable::call_indirect_func == nullptr) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                ::fast_io::fast_terminate();
            }

            return ::uwvm2::runtime::compiler::uwvm_int::optable::call_indirect_func(
                curr_module_id, type_index, table_index, uwvm_int_operand_stack_top);
        }

        [[nodiscard]] UWVM_GNU_HOT inline constexpr ::std::byte* call_ref(::std::size_t curr_module_id,
                                                                           ::std::size_t type_index,
                                                                           ::std::byte* operand_stack_top) UWVM_THROWS
        {
            if(call_ref_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            // [callee arguments][wasm_funcref_t] operand_stack_top
            // [safe                                      ] the translator validated both operands and spilled the ring.
            //                                      ^^ operand_stack_top; callback consumes the complete reference.
            return call_ref_func(curr_module_id, type_index, operand_stack_top);
        }
    }  // namespace details

    /// @brief `call` opcode (tail-call): calls a function and then tail-calls the next interpreter op.
    /// @details
    /// - Stack-top optimization: requires all arguments to reside in the operand stack memory. When stack-top caching is enabled, the compiler must emit
    ///   stack-top spills so `type...[1u]` points at the full operand stack before executing `call`.
    /// - `type[0]` layout: `[opfunc_ptr][curr_module_id][call_function][next_opfunc_ptr]` (reads two `size_t` immediates, then loads the next opfunc pointer).
    /// @note `type...[0]` may be unaligned for function-pointer / `size_t` slots; always load via `memcpy` as done here.
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        // ^^ type...[0]

        type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        //                   ^^ type...[0]

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), type...[0], sizeof(curr_module_id));

        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ type...[0] advances by sizeof(curr_module_id); translation emitted the matching typed slot.
        type...[0] += sizeof(curr_module_id);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        //                                  ^^ type...[0]

        ::std::size_t call_function;  // no init
        ::std::memcpy(::std::addressof(call_function), type...[0], sizeof(call_function));

        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ type...[0] advances by sizeof(call_function); translation emitted the matching typed slot.
        type...[0] += sizeof(call_function);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        //                                                ^^ type...[0]

        // The bridge returns the updated top by value, so no opfunc parameter address can escape across musttail.
        // caller frame: operand_base ... [live prefix][validated call results] | frame_end
        //               [compiled post-call byte maximum               ] unsafe past frame_end
        // ^^ type...[1] takes the synchronous bridge's returned top; typed call results fit this caller frame.
        type...[1] = details::call(curr_module_id, call_function, type...[1]);

        // next op
        ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `call_indirect` opcode (tail-call): calls a function through a table entry and then tail-calls the next interpreter op.
    /// @details
    /// - Stack-top optimization: requires all arguments (and the table-element index operand, distinct from the encoded `table_index`) to reside in the operand stack memory. When stack-top caching is enabled,
    ///   the compiler must emit stack-top spills so `type...[1u]` points at the full operand stack before executing `call_indirect`.
    /// - `type[0]` layout: `[opfunc_ptr][curr_module_id][type_index][table_index][next_opfunc_ptr]`.
    /// @note The actual bounds/null/type checks are performed by `call_indirect_func` provided by the runtime.
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_indirect(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), type...[0], sizeof(curr_module_id));
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ type...[0] advances by sizeof(curr_module_id); translation emitted the matching typed slot.
        type...[0] += sizeof(curr_module_id);

        ::std::size_t type_index;  // no init
        ::std::memcpy(::std::addressof(type_index), type...[0], sizeof(type_index));
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ type...[0] advances by sizeof(type_index); translation emitted the matching typed slot.
        type...[0] += sizeof(type_index);

        ::std::size_t table_index;  // no init
        ::std::memcpy(::std::addressof(table_index), type...[0], sizeof(table_index));
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ type...[0] advances by sizeof(table_index); translation emitted the matching typed slot.
        type...[0] += sizeof(table_index);

        // caller frame: operand_base ... [live prefix][validated call results] | frame_end
        //               [compiled post-call byte maximum               ] unsafe past frame_end
        // ^^ type...[1] takes the synchronous bridge's returned top; typed call results fit this caller frame.
        type...[1] = details::call_indirect(curr_module_id, type_index, table_index, type...[1]);

        ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `call` opcode (non-tail-call/byref): advances `typeref...[0]` and triggers the call.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching, and `call` requires arguments on the operand stack).
    /// - `type[0]` layout: `[opfunc_ptr][curr_module_id][call_function][next_opfunc_ptr]`; after execution `typeref...[0]` points at `next_opfunc_ptr`,
    ///   and the upper-level dispatcher continues execution.
    /// @note In non-tail-call mode, the next-op dispatch is driven by the outer interpreter loop, so this function does not load/call `next_opfunc_ptr`.
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        // ^^ type...[0]

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        //                   ^^ type...[0]

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), typeref...[0], sizeof(curr_module_id));

        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ typeref...[0] advances by sizeof(curr_module_id); translation emitted the matching typed slot.
        typeref...[0] += sizeof(curr_module_id);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        //                                  ^^ type...[0]

        ::std::size_t call_function;  // no init
        ::std::memcpy(::std::addressof(call_function), typeref...[0], sizeof(call_function));

        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ typeref...[0] advances by sizeof(call_function); translation emitted the matching typed slot.
        typeref...[0] += sizeof(call_function);

        // curr_uwvmint_call curr_module_id call_function next_op
        // safe
        //                                                ^^ type...[0]

        // call function
        typeref...[1] = details::call(curr_module_id, call_function, typeref...[1]);

        // Function calls are initiated by higher-level functions.
    }

    /// @brief `call_indirect` opcode (non-tail-call/byref): advances `typeref...[0]` and triggers the indirect call.
    /// @details
    /// - Stack-top optimization: not supported.
    /// - `type[0]` layout: `[opfunc_ptr][curr_module_id][type_index][table_index][next_opfunc_ptr]`; after execution `typeref...[0]` points at
    ///   `next_opfunc_ptr`.
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_indirect(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), typeref...[0], sizeof(curr_module_id));
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ typeref...[0] advances by sizeof(curr_module_id); translation emitted the matching typed slot.
        typeref...[0] += sizeof(curr_module_id);

        ::std::size_t type_index;  // no init
        ::std::memcpy(::std::addressof(type_index), typeref...[0], sizeof(type_index));
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ typeref...[0] advances by sizeof(type_index); translation emitted the matching typed slot.
        typeref...[0] += sizeof(type_index);

        ::std::size_t table_index;  // no init
        ::std::memcpy(::std::addressof(table_index), typeref...[0], sizeof(table_index));
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ typeref...[0] advances by sizeof(table_index); translation emitted the matching typed slot.
        typeref...[0] += sizeof(table_index);

        typeref...[1] = details::call_indirect(curr_module_id, type_index, table_index, typeref...[1]);
    }

    /// Runtime `call_ref`: the operand stack ends in the complete provider-owned funcref.
    /// The bridge checks null and the expected function type before entering the target.
    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_ref(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0], ::std::byte const*>);
        // [handler][module id][type index][next handler]
        // [safe  ] compiler emitted a complete handler pointer.
        //          ^^ ip after advancing; module id is a complete immediate.
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        ::std::size_t module_id;
        ::std::memcpy(::std::addressof(module_id), type...[0], sizeof(module_id));
        // [handler][module id][type index][next handler]
        //           [safe    ] compiler emitted the complete module field.
        //                      ^^ ip after advancing; type index is a complete immediate.
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ type...[0] advances by sizeof(module_id); translation emitted the matching typed slot.
        type...[0] += sizeof(module_id);
        ::std::size_t type_index;
        ::std::memcpy(::std::addressof(type_index), type...[0], sizeof(type_index));
        // [handler][module id][type index][next handler]
        //                      [safe     ] compiler emitted the complete type field.
        //                                  ^^ ip after advancing; next handler exists.
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ type...[0] advances by sizeof(type_index); translation emitted the matching typed slot.
        type...[0] += sizeof(type_index);
        // caller frame: operand_base ... [live prefix][validated call results] | frame_end
        //               [compiled post-call byte maximum               ] unsafe past frame_end
        // ^^ type...[1] takes the synchronous bridge's returned top; typed call results fit this caller frame.
        type...[1] = details::call_ref(module_id, type_index, type...[1]);
        uwvm_interpreter_opfunc_t<Type...> next;
        ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_ref(Type&... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0], ::std::byte const*>);
        // [handler][module id][type index][next handler]
        // [safe  ] byref dispatcher has read a complete handler pointer.
        //          ^^ ip after advancing; complete module id follows.
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<Type...>);
        ::std::size_t module_id;
        ::std::memcpy(::std::addressof(module_id), type...[0], sizeof(module_id));
        // [handler][module id][type index][next handler]
        //           [safe    ] compiler emitted this field.
        //                      ^^ ip after advancing; complete type index follows.
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ type...[0] advances by sizeof(module_id); translation emitted the matching typed slot.
        type...[0] += sizeof(module_id);
        ::std::size_t type_index;
        ::std::memcpy(::std::addressof(type_index), type...[0], sizeof(type_index));
        // [handler][module id][type index][next handler]
        //                      [safe     ] compiler emitted this field.
        //                                  ^^ ip after advancing; outer dispatcher reads next handler.
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ type...[0] advances by sizeof(type_index); translation emitted the matching typed slot.
        type...[0] += sizeof(type_index);
        // caller frame: operand_base ... [live prefix][validated call results] | frame_end
        //               [compiled post-call byte maximum               ] unsafe past frame_end
        // ^^ type...[1] takes the synchronous bridge's returned top; typed call results fit this caller frame.
        type...[1] = details::call_ref(module_id, type_index, type...[1]);
    }

    namespace translate
    {
        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_call_ref_fptr_from_tuple(
            uwvm_interpreter_stacktop_currpos_t const&, ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_call_ref<Option, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_call_ref<Option, Type...>); }
        }

        /// @brief Translator: returns the interpreter function pointer for `call` (tail-call).
        /// @details
        /// - Stack-top optimization: not applicable (`call` always disables stack-top caching; this only returns a function pointer).
        /// - `type[0]` layout: not applicable (translation does not manipulate the bytecode stream pointer).
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_call_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            // Because there is no top-of-stack dependency, there is only a single version here.
            return uwvmint_call<CompileOption, Type...>;
        }

        /// @brief Translator: infers types from a tuple and returns the `call` function pointer (tail-call).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_call_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        /// @brief Translator: returns the interpreter function pointer for `call_indirect` (tail-call).
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_call_indirect_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_call_indirect<CompileOption, Type...>; }

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_call_indirect_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_indirect_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        /// @brief Translator: returns the interpreter function pointer for `call` (non-tail-call/byref).
        /// @details
        /// - Stack-top optimization: not applicable (byref mode disables stack-top caching).
        /// - `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_call_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            // Because there is no top-of-stack dependency, there is only a single version here.
            return uwvmint_call<CompileOption, Type...>;
        }

        /// @brief Translator: infers types from a tuple and returns the `call` function pointer (non-tail-call/byref).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_call_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        /// @brief Translator: returns the interpreter function pointer for `call_indirect` (non-tail-call/byref).
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_call_indirect_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_call_indirect<CompileOption, Type...>; }

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_call_indirect_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_indirect_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate
}
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

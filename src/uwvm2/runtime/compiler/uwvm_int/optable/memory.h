/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <bit>
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/object/impl.h>
# include "define.h"
# include "register_ring.h"
#endif

#if !(__cpp_pack_indexing >= 202311L)
# error "UWVM requires at least C++26 standard compiler. See https://en.cppreference.com/w/cpp/feature_test#cpp_pack_indexing"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    namespace details
    {
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval bool stacktop_enabled_for() noexcept
        {
            if constexpr(::std::same_as<OperandT, wasm_i32>) { return CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_i64>) { return CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f32>) { return CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f64>) { return CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos; }
            else
            {
                return false;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption>
        inline consteval bool scalar_ranges_all_merged() noexcept
        {
            return CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                   CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos &&
                   CompileOption.i32_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                   CompileOption.i32_stack_top_end_pos == CompileOption.f32_stack_top_end_pos &&
                   CompileOption.i32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                   CompileOption.i32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos;
        }

        template <uwvm_interpreter_translate_option_t CompileOption>
        inline consteval bool i32_i64_ranges_merged() noexcept
        {
            return CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                   CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos;
        }

        template <uwvm_interpreter_translate_option_t CompileOption>
        inline consteval bool i32_f32_ranges_merged() noexcept
        {
            return CompileOption.i32_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                   CompileOption.i32_stack_top_end_pos == CompileOption.f32_stack_top_end_pos;
        }

        template <typename T>
        UWVM_ALWAYS_INLINE inline constexpr T read_imm(::std::byte const*& ip) noexcept
        {
            T v;  // no init
            ::std::memcpy(::std::addressof(v), ip, sizeof(v));
            ip += sizeof(v);
            return v;
        }

        UWVM_ALWAYS_INLINE inline constexpr ::std::uint_least8_t load_u8(::std::byte const* p) noexcept
        {
            ::std::uint_least8_t v;  // no init
            ::std::memcpy(::std::addressof(v), p, sizeof(v));
            return v;
        }

        UWVM_ALWAYS_INLINE inline constexpr wasm_i32 load_i32_le(::std::byte const* p) noexcept
        {
            ::std::uint_least32_t tmp;  // no init
            ::std::memcpy(::std::addressof(tmp), p, sizeof(tmp));
            tmp = ::fast_io::little_endian(tmp);
            return ::std::bit_cast<wasm_i32>(tmp);
        }

        UWVM_ALWAYS_INLINE inline constexpr wasm_i64 load_i64_le(::std::byte const* p) noexcept
        {
            ::std::uint_least64_t tmp;  // no init
            ::std::memcpy(::std::addressof(tmp), p, sizeof(tmp));
            tmp = ::fast_io::little_endian(tmp);
            return ::std::bit_cast<wasm_i64>(tmp);
        }

        UWVM_ALWAYS_INLINE inline constexpr wasm_f32 load_f32_le(::std::byte const* p) noexcept
        {
            ::std::uint_least32_t tmp;  // no init
            ::std::memcpy(::std::addressof(tmp), p, sizeof(tmp));
            tmp = ::fast_io::little_endian(tmp);
            return ::std::bit_cast<wasm_f32>(tmp);
        }

        UWVM_ALWAYS_INLINE inline constexpr wasm_f64 load_f64_le(::std::byte const* p) noexcept
        {
            ::std::uint_least64_t tmp;  // no init
            ::std::memcpy(::std::addressof(tmp), p, sizeof(tmp));
            tmp = ::fast_io::little_endian(tmp);
            return ::std::bit_cast<wasm_f64>(tmp);
        }

        UWVM_ALWAYS_INLINE inline constexpr void store_u8(::std::byte* p, ::std::uint_least8_t v) noexcept { ::std::memcpy(p, ::std::addressof(v), sizeof(v)); }

        UWVM_ALWAYS_INLINE inline constexpr void store_u16_le(::std::byte* p, ::std::uint_least16_t v) noexcept
        {
            v = ::fast_io::little_endian(v);
            ::std::memcpy(p, ::std::addressof(v), sizeof(v));
        }

        UWVM_ALWAYS_INLINE inline constexpr void store_u32_le(::std::byte* p, ::std::uint_least32_t v) noexcept
        {
            v = ::fast_io::little_endian(v);
            ::std::memcpy(p, ::std::addressof(v), sizeof(v));
        }

        UWVM_ALWAYS_INLINE inline constexpr void store_u64_le(::std::byte* p, ::std::uint_least64_t v) noexcept
        {
            v = ::fast_io::little_endian(v);
            ::std::memcpy(p, ::std::addressof(v), sizeof(v));
        }

        UWVM_ALWAYS_INLINE inline constexpr void store_i32_le(::std::byte* p, wasm_i32 v) noexcept
        { store_u32_le(p, ::std::bit_cast<::std::uint_least32_t>(v)); }

        UWVM_ALWAYS_INLINE inline constexpr void store_i64_le(::std::byte* p, wasm_i64 v) noexcept
        { store_u64_le(p, ::std::bit_cast<::std::uint_least64_t>(v)); }

        UWVM_ALWAYS_INLINE inline constexpr void store_f32_le(::std::byte* p, wasm_f32 v) noexcept
        { store_u32_le(p, ::std::bit_cast<::std::uint_least32_t>(v)); }

        UWVM_ALWAYS_INLINE inline constexpr void store_f64_le(::std::byte* p, wasm_f64 v) noexcept
        { store_u64_le(p, ::std::bit_cast<::std::uint_least64_t>(v)); }

        template <unsigned Shift, typename OffsetT>
        UWVM_ALWAYS_INLINE inline constexpr bool offset_in_pow2_bound(OffsetT offset) noexcept
        {
            using U = ::std::make_unsigned_t<OffsetT>;
            if constexpr(Shift >= ::std::numeric_limits<U>::digits) { return true; }
            else
            {
                return (static_cast<U>(offset) >> Shift) == 0u;
            }
        }

        UWVM_ALWAYS_INLINE UWVM_GNU_COLD [[noreturn]] inline void memory_oob_terminate(::std::size_t memory_idx,
                                                                                       ::std::uint_least64_t memory_static_offset,
                                                                                       ::std::size_t effective_offset,
                                                                                       ::std::size_t memory_length,
                                                                                       ::std::size_t wasm_bytes) noexcept
        {
            ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                .memory_idx = memory_idx,
                .memory_offset = {.offset = static_cast<::std::uint_least64_t>(effective_offset), .offset_65_bit = false},
                .memory_static_offset = memory_static_offset,
                .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                .memory_type_size = wasm_bytes
            });
        }

        template <typename MemoryT>
        UWVM_ALWAYS_INLINE inline constexpr void check_memory_bounds_unlocked(MemoryT const& memory,
                                                                              ::std::size_t memory_idx,
                                                                              ::std::uint_least64_t memory_static_offset,
                                                                              ::std::size_t effective_offset,
                                                                              ::std::size_t wasm_bytes) noexcept
        {
            if constexpr(MemoryT::can_mmap)
            {
                [[maybe_unused]] auto const memory_begin{memory.memory_begin};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory_begin == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

                // mmap backend:
                // - Full protection: no check on hot path, rely on page protection.
                // - Partial fixed protection: only check the fixed max (power-of-two) to avoid UB pointer overflow; the rest relies on page protection.
                // - custom_page < platform_page: must do per-access dynamic bounds check using the atomic memory length.
                if(memory.require_dynamic_determination_memory_size())
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                    auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                    if(wasm_bytes > memory_length || effective_offset > memory_length - wasm_bytes) [[unlikely]]
                    {
                        memory_oob_terminate(memory_idx, memory_static_offset, effective_offset, memory_length, wasm_bytes);
                    }
                }
                else
                {
                    if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
                    {
                        // 64-bit platform:
                        // - wasm32: full protection → no check
                        // - wasm64: partial fixed protection (1<<::uwvm2::object::memory::linear::max_partial_protection_wasm64_index)
#if defined(UWVM_SUPPORT_MMAP)
                        if(memory.status == ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm64) [[unlikely]]
                        {
                            if(!offset_in_pow2_bound<::uwvm2::object::memory::linear::max_partial_protection_wasm64_index>(effective_offset))
                            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
                                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                                memory_oob_terminate(memory_idx, memory_static_offset, effective_offset, memory_length, wasm_bytes);
                            }
                        }
#endif
                    }
                    else
                    {
                        // 32-bit platform: always partial fixed protection (1<<::uwvm2::object::memory::linear::max_partial_protection_wasm32_index).
                        if(!offset_in_pow2_bound<::uwvm2::object::memory::linear::max_partial_protection_wasm32_index>(effective_offset))
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                            auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                            memory_oob_terminate(memory_idx, memory_static_offset, effective_offset, memory_length, wasm_bytes);
                        }
                    }
                }
            }
            else
            {
                [[maybe_unused]] auto const memory_begin{memory.memory_begin};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory_begin == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

                auto const memory_length{memory.memory_length};
                if(wasm_bytes > memory_length || effective_offset > memory_length - wasm_bytes) [[unlikely]]
                {
                    memory_oob_terminate(memory_idx, memory_static_offset, effective_offset, memory_length, wasm_bytes);
                }
            }
        }

        template <typename MemoryT>
        UWVM_ALWAYS_INLINE inline auto lock_memory(MemoryT const& memory) noexcept
        {
            // Only allocator-backed memories may relocate `memory_begin` during grow(), so only they need the memory_operation_guard.
            // mmap-backed memories keep a stable base address, so they are always lock-free on the hot path.
            if constexpr(!MemoryT::can_mmap && MemoryT::support_multi_thread)
            {
                return ::uwvm2::object::memory::linear::memory_operation_guard_t{memory.growing_flag_p, memory.active_ops_p};
            }
            else
            {
                return ::uwvm2::object::memory::linear::dummy_memory_operation_guard_t{};
            }
        }

        // Tail-call interpreter ops use UWVM_MUSTTAIL and therefore cannot keep any object with a non-trivial destructor alive across the tail call.
        // For multithread allocator-backed memories we still need to participate in the grow() relocation protocol, so provide explicit enter/exit helpers
        // (no RAII).
        template <typename MemoryT>
        UWVM_ALWAYS_INLINE inline void enter_memory_operation_memory_lock([[maybe_unused]] MemoryT const& memory) noexcept
        {
            if constexpr(!MemoryT::can_mmap && MemoryT::support_multi_thread)
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.growing_flag_p == nullptr || memory.active_ops_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

                unsigned spin_count{};
                for(;;)
                {
                    while(memory.growing_flag_p->test(::std::memory_order_acquire))
                    {
                        if(++spin_count > 1000u)
                        {
                            memory.growing_flag_p->wait(true, ::std::memory_order_acquire);
                            spin_count = 0u;
                        }
                        else
                        {
                            ::uwvm2::utils::mutex::rwlock_pause();
                        }
                    }

                    memory.active_ops_p->fetch_add(1uz, ::std::memory_order_acquire);

                    if(!memory.growing_flag_p->test(::std::memory_order_acquire)) [[likely]] { return; }

                    memory.active_ops_p->fetch_sub(1uz, ::std::memory_order_release);
                    memory.active_ops_p->notify_one();
                }
            }
        }

        template <typename MemoryT>
        UWVM_ALWAYS_INLINE inline void exit_memory_operation_memory_lock([[maybe_unused]] MemoryT const& memory) noexcept
        {
            if constexpr(!MemoryT::can_mmap && MemoryT::support_multi_thread)
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.growing_flag_p == nullptr || memory.active_ops_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

                memory.active_ops_p->fetch_sub(1uz, ::std::memory_order_release);
                memory.active_ops_p->notify_one();
            }
        }

        // =========================================================
        // Bounds-check helpers (selected by translator)
        // =========================================================

        UWVM_ALWAYS_INLINE inline constexpr void bounds_check_generic(native_memory_t const& memory,
                                                                      ::std::size_t memory_idx,
                                                                      ::std::uint_least64_t memory_static_offset,
                                                                      ::std::size_t effective_offset,
                                                                      ::std::size_t wasm_bytes) noexcept
        { check_memory_bounds_unlocked(memory, memory_idx, memory_static_offset, effective_offset, wasm_bytes); }

#if defined(UWVM_SUPPORT_MMAP)
        UWVM_ALWAYS_INLINE inline constexpr void
            bounds_check_mmap_full(native_memory_t const& memory, ::std::size_t, ::std::uint_least64_t, ::std::size_t, ::std::size_t) noexcept
        {
            [[maybe_unused]] auto const memory_begin{memory.memory_begin};
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(memory_begin == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
        }

        UWVM_ALWAYS_INLINE inline constexpr void bounds_check_mmap_path(native_memory_t const& memory,
                                                                        ::std::size_t memory_idx,
                                                                        ::std::uint_least64_t memory_static_offset,
                                                                        ::std::size_t effective_offset,
                                                                        ::std::size_t wasm_bytes) noexcept
        {
            [[maybe_unused]] auto const memory_begin{memory.memory_begin};
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(memory_begin == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
            if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

            if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
            {
                if(!offset_in_pow2_bound<::uwvm2::object::memory::linear::max_partial_protection_wasm64_index>(effective_offset)) [[unlikely]]
                {
                    auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                    memory_oob_terminate(memory_idx, memory_static_offset, effective_offset, memory_length, wasm_bytes);
                }
            }
            else
            {
                if(!offset_in_pow2_bound<::uwvm2::object::memory::linear::max_partial_protection_wasm32_index>(effective_offset)) [[unlikely]]
                {
                    auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                    memory_oob_terminate(memory_idx, memory_static_offset, effective_offset, memory_length, wasm_bytes);
                }
            }
        }

        UWVM_ALWAYS_INLINE inline constexpr void bounds_check_mmap_judge(native_memory_t const& memory,
                                                                         ::std::size_t memory_idx,
                                                                         ::std::uint_least64_t memory_static_offset,
                                                                         ::std::size_t effective_offset,
                                                                         ::std::size_t wasm_bytes) noexcept
        {
            [[maybe_unused]] auto const memory_begin{memory.memory_begin};
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(memory_begin == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
            if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

            auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
            if(wasm_bytes > memory_length || effective_offset > memory_length - wasm_bytes) [[unlikely]]
            {
                memory_oob_terminate(memory_idx, memory_static_offset, effective_offset, memory_length, wasm_bytes);
            }
        }
#else
        UWVM_ALWAYS_INLINE inline constexpr void bounds_check_allocator(native_memory_t const& memory,
                                                                        ::std::size_t memory_idx,
                                                                        ::std::uint_least64_t memory_static_offset,
                                                                        ::std::size_t effective_offset,
                                                                        ::std::size_t wasm_bytes) noexcept
        {
            [[maybe_unused]] auto const memory_begin{memory.memory_begin};
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(memory_begin == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

            auto const memory_length{memory.memory_length};
            if(wasm_bytes > memory_length || effective_offset > memory_length - wasm_bytes) [[unlikely]]
            {
                memory_oob_terminate(memory_idx, memory_static_offset, effective_offset, memory_length, wasm_bytes);
            }
        }
#endif

        UWVM_ALWAYS_INLINE inline constexpr ::std::size_t wasm32_effective_offset(wasm_i32 addr, wasm_u32 static_offset) noexcept
        {
            // wasm32 address arithmetic is modulo 2^32.
            auto const a{static_cast<::std::uint_least32_t>(addr)};
            auto const o{static_cast<::std::uint_least32_t>(static_offset)};
            auto const eff{static_cast<::std::uint_least32_t>(a + o)};
            return static_cast<::std::size_t>(eff);
        }
    }  // namespace details

    // =========================
    // Tailcall (stacktop-aware)
    // =========================

    namespace details::memop
    {
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void i32_load(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 4uz);

            auto const out{details::load_i32_le(memory.memory_begin + eff)};
            details::exit_memory_operation_memory_lock(memory);
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                constexpr ::std::size_t range_begin{CompileOption.i32_stack_top_begin_pos};
                constexpr ::std::size_t range_end{CompileOption.i32_stack_top_end_pos};
                static_assert(range_begin <= curr_i32_stack_top && curr_i32_stack_top < range_end);

                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void i64_load(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_i64 = details::wasm_i64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                static_assert(details::i32_i64_ranges_merged<CompileOption>(),
                              "register_ring memory i64.load requires i32 and i64 stack-top ranges to be merged");
            }

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 8uz);

            auto const out{details::load_i64_le(memory.memory_begin + eff)};
            details::exit_memory_operation_memory_lock(memory);
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_i32_stack_top>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void f32_load(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f32 = details::wasm_f32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                static_assert(details::i32_f32_ranges_merged<CompileOption>(),
                              "register_ring memory f32.load requires i32 and f32 stack-top ranges to be merged");
            }

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 4uz);

            auto const out{details::load_f32_le(memory.memory_begin + eff)};
            details::exit_memory_operation_memory_lock(memory);
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, curr_i32_stack_top>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void f64_load(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f64 = details::wasm_f64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                static_assert(details::scalar_ranges_all_merged<CompileOption>(),
                              "register_ring memory f64.load requires i32/i64/f32/f64 stack-top ranges to be merged");
            }

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 8uz);

            auto const out{details::load_f64_le(memory.memory_begin + eff)};
            details::exit_memory_operation_memory_lock(memory);
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, curr_i32_stack_top>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }
    }  // namespace details::memop

    /// @brief `i32.load` opcode (tail-call): loads a 32-bit little-endian value from linear memory.
    /// @details
    /// - Stack-top optimization: supported for the i32 address operand and for the i32 result when i32 stack-top caching is enabled.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    /// @note The effective address is computed with wasm32 modulo-2^32 arithmetic; bounds checks are performed before the load.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load(Type... type) UWVM_THROWS
    { return details::memop::i32_load<details::bounds_check_generic, CompileOption, curr_i32_stack_top, Type...>(type...); }

#if defined(UWVM_SUPPORT_MMAP)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load_mmap_full(Type... type) UWVM_THROWS
    { return details::memop::i32_load<details::bounds_check_mmap_full, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load_mmap_path(Type... type) UWVM_THROWS
    { return details::memop::i32_load<details::bounds_check_mmap_path, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load_mmap_judge(Type... type) UWVM_THROWS
    { return details::memop::i32_load<details::bounds_check_mmap_judge, CompileOption, curr_i32_stack_top, Type...>(type...); }
#else
# if defined(UWVM_USE_MULTITHREAD_ALLOCATOR)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load_multithread_allocator(Type... type) UWVM_THROWS
    { return details::memop::i32_load<details::bounds_check_allocator, CompileOption, curr_i32_stack_top, Type...>(type...); }
# else
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load_singlethread_allocator(Type... type) UWVM_THROWS
    { return details::memop::i32_load<details::bounds_check_allocator, CompileOption, curr_i32_stack_top, Type...>(type...); }
# endif
#endif

    /// @brief `i64.load` opcode (tail-call): loads a 64-bit little-endian value from linear memory.
    /// @details
    /// - Stack-top optimization: supported for the i32 address operand and for the i64 result when i32 stack-top caching is enabled; requires i32 and i64
    /// stack-top
    ///   ranges to be merged because the i64 result is written back into the scalar ring slot.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load(Type... type) UWVM_THROWS
    { return details::memop::i64_load<details::bounds_check_generic, CompileOption, curr_i32_stack_top, Type...>(type...); }

#if defined(UWVM_SUPPORT_MMAP)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load_mmap_full(Type... type) UWVM_THROWS
    { return details::memop::i64_load<details::bounds_check_mmap_full, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load_mmap_path(Type... type) UWVM_THROWS
    { return details::memop::i64_load<details::bounds_check_mmap_path, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load_mmap_judge(Type... type) UWVM_THROWS
    { return details::memop::i64_load<details::bounds_check_mmap_judge, CompileOption, curr_i32_stack_top, Type...>(type...); }
#else
# if defined(UWVM_USE_MULTITHREAD_ALLOCATOR)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load_multithread_allocator(Type... type) UWVM_THROWS
    { return details::memop::i64_load<details::bounds_check_allocator, CompileOption, curr_i32_stack_top, Type...>(type...); }
# else
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load_singlethread_allocator(Type... type) UWVM_THROWS
    { return details::memop::i64_load<details::bounds_check_allocator, CompileOption, curr_i32_stack_top, Type...>(type...); }
# endif
#endif

    /// @brief `f32.load` opcode (tail-call): loads a 32-bit little-endian float from linear memory.
    /// @details
    /// - Stack-top optimization: supported for the i32 address operand and for the f32 result when i32 stack-top caching is enabled; requires i32 and f32
    /// stack-top
    ///   ranges to be merged because the f32 result is written back into the scalar ring slot.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_load(Type... type) UWVM_THROWS
    { return details::memop::f32_load<details::bounds_check_generic, CompileOption, curr_i32_stack_top, Type...>(type...); }

#if defined(UWVM_SUPPORT_MMAP)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_load_mmap_full(Type... type) UWVM_THROWS
    { return details::memop::f32_load<details::bounds_check_mmap_full, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_load_mmap_path(Type... type) UWVM_THROWS
    { return details::memop::f32_load<details::bounds_check_mmap_path, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_load_mmap_judge(Type... type) UWVM_THROWS
    { return details::memop::f32_load<details::bounds_check_mmap_judge, CompileOption, curr_i32_stack_top, Type...>(type...); }
#else
# if defined(UWVM_USE_MULTITHREAD_ALLOCATOR)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_load_multithread_allocator(Type... type) UWVM_THROWS
    { return details::memop::f32_load<details::bounds_check_allocator, CompileOption, curr_i32_stack_top, Type...>(type...); }
# else
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_load_singlethread_allocator(Type... type) UWVM_THROWS
    { return details::memop::f32_load<details::bounds_check_allocator, CompileOption, curr_i32_stack_top, Type...>(type...); }
# endif
#endif

    /// @brief `f64.load` opcode (tail-call): loads a 64-bit little-endian float from linear memory.
    /// @details
    /// - Stack-top optimization: supported for the i32 address operand and for the f64 result when i32 stack-top caching is enabled; requires scalar stack-top
    /// ranges
    ///   to be fully merged because the f64 result is written back into the scalar ring slot.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_load(Type... type) UWVM_THROWS
    { return details::memop::f64_load<details::bounds_check_generic, CompileOption, curr_i32_stack_top, Type...>(type...); }

#if defined(UWVM_SUPPORT_MMAP)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_load_mmap_full(Type... type) UWVM_THROWS
    { return details::memop::f64_load<details::bounds_check_mmap_full, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_load_mmap_path(Type... type) UWVM_THROWS
    { return details::memop::f64_load<details::bounds_check_mmap_path, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_load_mmap_judge(Type... type) UWVM_THROWS
    { return details::memop::f64_load<details::bounds_check_mmap_judge, CompileOption, curr_i32_stack_top, Type...>(type...); }
#else
# if defined(UWVM_USE_MULTITHREAD_ALLOCATOR)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_load_multithread_allocator(Type... type) UWVM_THROWS
    { return details::memop::f64_load<details::bounds_check_allocator, CompileOption, curr_i32_stack_top, Type...>(type...); }
# else
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_load_singlethread_allocator(Type... type) UWVM_THROWS
    { return details::memop::f64_load<details::bounds_check_allocator, CompileOption, curr_i32_stack_top, Type...>(type...); }
# endif
#endif

    namespace details::memop
    {
        template <auto BoundsCheckFn,
                  bool Signed,
                  uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t curr_i32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void i32_load8(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 1uz);

            ::std::uint_least8_t b{details::load_u8(memory.memory_begin + eff)};
            details::exit_memory_operation_memory_lock(memory);
            wasm_i32 out{};
            if constexpr(Signed) { out = static_cast<wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least8_t>(b))); }
            else
            {
                out = static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(b));
            }

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <auto BoundsCheckFn,
                  bool Signed,
                  uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t curr_i32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void i32_load16(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 2uz);

            ::std::uint_least16_t tmp;  // no init
            ::std::memcpy(::std::addressof(tmp), memory.memory_begin + eff, sizeof(tmp));
            tmp = ::fast_io::little_endian(tmp);
            details::exit_memory_operation_memory_lock(memory);

            wasm_i32 out{};
            if constexpr(Signed) { out = static_cast<wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least16_t>(tmp))); }
            else
            {
                out = static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(tmp));
            }

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <auto BoundsCheckFn,
                  bool Signed,
                  uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t curr_i32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void i64_load8(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_i64 = details::wasm_i64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                static_assert(details::i32_i64_ranges_merged<CompileOption>(),
                              "register_ring memory i64.load* requires i32 and i64 stack-top ranges to be merged");
            }

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 1uz);

            ::std::uint_least8_t b{details::load_u8(memory.memory_begin + eff)};
            details::exit_memory_operation_memory_lock(memory);
            wasm_i64 out{};
            if constexpr(Signed) { out = static_cast<wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least8_t>(b))); }
            else
            {
                out = static_cast<wasm_i64>(static_cast<::std::uint_least64_t>(b));
            }

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_i32_stack_top>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <auto BoundsCheckFn,
                  bool Signed,
                  uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t curr_i32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void i64_load16(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_i64 = details::wasm_i64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                static_assert(details::i32_i64_ranges_merged<CompileOption>(),
                              "register_ring memory i64.load* requires i32 and i64 stack-top ranges to be merged");
            }

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 2uz);

            ::std::uint_least16_t tmp;  // no init
            ::std::memcpy(::std::addressof(tmp), memory.memory_begin + eff, sizeof(tmp));
            tmp = ::fast_io::little_endian(tmp);
            details::exit_memory_operation_memory_lock(memory);

            wasm_i64 out{};
            if constexpr(Signed) { out = static_cast<wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least16_t>(tmp))); }
            else
            {
                out = static_cast<wasm_i64>(static_cast<::std::uint_least64_t>(tmp));
            }

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_i32_stack_top>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <auto BoundsCheckFn,
                  bool Signed,
                  uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t curr_i32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void i64_load32(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_i64 = details::wasm_i64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                static_assert(details::i32_i64_ranges_merged<CompileOption>(),
                              "register_ring memory i64.load* requires i32 and i64 stack-top ranges to be merged");
            }

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 4uz);

            ::std::uint_least32_t tmp;  // no init
            ::std::memcpy(::std::addressof(tmp), memory.memory_begin + eff, sizeof(tmp));
            tmp = ::fast_io::little_endian(tmp);
            details::exit_memory_operation_memory_lock(memory);

            wasm_i64 out{};
            if constexpr(Signed) { out = static_cast<wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least32_t>(tmp))); }
            else
            {
                out = static_cast<wasm_i64>(static_cast<::std::uint_least64_t>(tmp));
            }

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_i32_stack_top>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }
    }  // namespace details::memop

    /// @brief `i32.load8_{s,u}` core (tail-call): loads 1 byte and extends to i32.
    /// @details
    /// - Stack-top optimization: supported for address and i32 result when enabled.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    template <bool Signed, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load8(Type... type) UWVM_THROWS
    { return details::memop::i32_load8<details::bounds_check_generic, Signed, CompileOption, curr_i32_stack_top, Type...>(type...); }

    /// @brief `i32.load16_{s,u}` core (tail-call): loads 2 bytes and extends to i32.
    /// @details
    /// - Stack-top optimization: supported for address and i32 result when enabled.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    template <bool Signed, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load16(Type... type) UWVM_THROWS
    { return details::memop::i32_load16<details::bounds_check_generic, Signed, CompileOption, curr_i32_stack_top, Type...>(type...); }

    /// @brief `i64.load8_{s,u}` core (tail-call): loads 1 byte and extends to i64.
    /// @details
    /// - Stack-top optimization: supported when i32 stack-top caching is enabled; requires i32 and i64 stack-top ranges to be merged.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    template <bool Signed, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load8(Type... type) UWVM_THROWS
    { return details::memop::i64_load8<details::bounds_check_generic, Signed, CompileOption, curr_i32_stack_top, Type...>(type...); }

    /// @brief `i64.load16_{s,u}` core (tail-call): loads 2 bytes and extends to i64.
    /// @details
    /// - Stack-top optimization: supported when i32 stack-top caching is enabled; requires i32 and i64 stack-top ranges to be merged.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    template <bool Signed, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load16(Type... type) UWVM_THROWS
    { return details::memop::i64_load16<details::bounds_check_generic, Signed, CompileOption, curr_i32_stack_top, Type...>(type...); }

    /// @brief `i64.load32_{s,u}` core (tail-call): loads 4 bytes and extends to i64.
    /// @details
    /// - Stack-top optimization: supported when i32 stack-top caching is enabled; requires i32 and i64 stack-top ranges to be merged.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    template <bool Signed, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load32(Type... type) UWVM_THROWS
    { return details::memop::i64_load32<details::bounds_check_generic, Signed, CompileOption, curr_i32_stack_top, Type...>(type...); }

    namespace details::memop
    {
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void i32_store(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const value{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            constexpr ::std::size_t range_begin{CompileOption.i32_stack_top_begin_pos};
            constexpr ::std::size_t range_end{CompileOption.i32_stack_top_end_pos};
            constexpr ::std::size_t addr_pos{details::ring_next_pos(curr_i32_stack_top, range_begin, range_end)};
            wasm_i32 const addr{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, addr_pos>(type...)};
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 4uz);
            details::store_i32_le(memory.memory_begin + eff, value);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void i64_store(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_i64 = details::wasm_i64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i64 value{};
            wasm_i32 addr{};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                static_assert(details::i32_i64_ranges_merged<CompileOption>(),
                              "register_ring memory i64.store* requires i32 and i64 stack-top ranges to be merged");

                constexpr ::std::size_t range_begin{CompileOption.i64_stack_top_begin_pos};
                constexpr ::std::size_t range_end{CompileOption.i64_stack_top_end_pos};
                constexpr ::std::size_t addr_pos{details::ring_next_pos(curr_i64_stack_top, range_begin, range_end)};

                value = get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...);
                addr = get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, addr_pos>(type...);
            }
            else
            {
                value = get_curr_val_from_operand_stack_cache<wasm_i64>(type...);
                addr = get_curr_val_from_operand_stack_cache<wasm_i32>(type...);
            }
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 8uz);
            details::store_i64_le(memory.memory_begin + eff, value);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void f32_store(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f32 = details::wasm_f32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_f32 value{};
            wasm_i32 addr{};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
            {
                static_assert(details::i32_f32_ranges_merged<CompileOption>(),
                              "register_ring memory f32.store requires i32 and f32 stack-top ranges to be merged");

                constexpr ::std::size_t range_begin{CompileOption.f32_stack_top_begin_pos};
                constexpr ::std::size_t range_end{CompileOption.f32_stack_top_end_pos};
                constexpr ::std::size_t addr_pos{details::ring_next_pos(curr_f32_stack_top, range_begin, range_end)};

                value = get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, curr_f32_stack_top>(type...);
                addr = get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, addr_pos>(type...);
            }
            else
            {
                value = get_curr_val_from_operand_stack_cache<wasm_f32>(type...);
                addr = get_curr_val_from_operand_stack_cache<wasm_i32>(type...);
            }
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 4uz);
            details::store_f32_le(memory.memory_begin + eff, value);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void f64_store(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f64 = details::wasm_f64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_f64 value{};
            wasm_i32 addr{};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
            {
                static_assert(details::scalar_ranges_all_merged<CompileOption>(),
                              "register_ring memory f64.store requires i32/i64/f32/f64 stack-top ranges to be merged");

                constexpr ::std::size_t range_begin{CompileOption.f64_stack_top_begin_pos};
                constexpr ::std::size_t range_end{CompileOption.f64_stack_top_end_pos};
                constexpr ::std::size_t addr_pos{details::ring_next_pos(curr_f64_stack_top, range_begin, range_end)};

                value = get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, curr_f64_stack_top>(type...);
                addr = get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, addr_pos>(type...);
            }
            else
            {
                value = get_curr_val_from_operand_stack_cache<wasm_f64>(type...);
                addr = get_curr_val_from_operand_stack_cache<wasm_i32>(type...);
            }
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 8uz);
            details::store_f64_le(memory.memory_begin + eff, value);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <auto BoundsCheckFn,
                  unsigned StoreBytes,
                  uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t curr_i32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void i32_storeN(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(StoreBytes == 1u || StoreBytes == 2u);
            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const value{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            constexpr ::std::size_t range_begin{CompileOption.i32_stack_top_begin_pos};
            constexpr ::std::size_t range_end{CompileOption.i32_stack_top_end_pos};
            constexpr ::std::size_t addr_pos{details::ring_next_pos(curr_i32_stack_top, range_begin, range_end)};
            wasm_i32 const addr{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, addr_pos>(type...)};
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, static_cast<::std::size_t>(StoreBytes));

            if constexpr(StoreBytes == 1u)
            {
                details::store_u8(memory.memory_begin + eff, static_cast<::std::uint_least8_t>(::std::bit_cast<::std::uint_least32_t>(value)));
            }
            else
            {
                details::store_u16_le(memory.memory_begin + eff, static_cast<::std::uint_least16_t>(::std::bit_cast<::std::uint_least32_t>(value)));
            }
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <auto BoundsCheckFn,
                  unsigned StoreBytes,
                  uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t curr_i64_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void i64_storeN(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_i64 = details::wasm_i64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(StoreBytes == 1u || StoreBytes == 2u || StoreBytes == 4u);
            static_assert(sizeof...(Type) >= 2uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i64 value{};
            wasm_i32 addr{};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                static_assert(details::i32_i64_ranges_merged<CompileOption>(),
                              "register_ring memory i64.store* requires i32 and i64 stack-top ranges to be merged");

                constexpr ::std::size_t range_begin{CompileOption.i64_stack_top_begin_pos};
                constexpr ::std::size_t range_end{CompileOption.i64_stack_top_end_pos};
                constexpr ::std::size_t addr_pos{details::ring_next_pos(curr_i64_stack_top, range_begin, range_end)};

                value = get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...);
                addr = get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, addr_pos>(type...);
            }
            else
            {
                value = get_curr_val_from_operand_stack_cache<wasm_i64>(type...);
                addr = get_curr_val_from_operand_stack_cache<wasm_i32>(type...);
            }
            ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, static_cast<::std::size_t>(StoreBytes));

            if constexpr(StoreBytes == 1u)
            {
                details::store_u8(memory.memory_begin + eff, static_cast<::std::uint_least8_t>(::std::bit_cast<::std::uint_least64_t>(value)));
            }
            else if constexpr(StoreBytes == 2u)
            {
                details::store_u16_le(memory.memory_begin + eff, static_cast<::std::uint_least16_t>(::std::bit_cast<::std::uint_least64_t>(value)));
            }
            else
            {
                details::store_u32_le(memory.memory_begin + eff, static_cast<::std::uint_least32_t>(::std::bit_cast<::std::uint_least64_t>(value)));
            }
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }
    }  // namespace details::memop

    /// @brief `i32.store` opcode (tail-call): stores a 32-bit value to linear memory.
    /// @details
    /// - Stack-top optimization: required; the implementation reads both `value` and `addr` from the i32 stack-top ring (value at `curr_i32_stack_top`, addr at
    /// `ring_next_pos`).
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    /// @note Stores perform bounds checks and use endian-safe stores as needed.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_store(Type... type) UWVM_THROWS
    { return details::memop::i32_store<details::bounds_check_generic, CompileOption, curr_i32_stack_top, Type...>(type...); }

#if defined(UWVM_SUPPORT_MMAP)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_store_mmap_full(Type... type) UWVM_THROWS
    { return details::memop::i32_store<details::bounds_check_mmap_full, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_store_mmap_path(Type... type) UWVM_THROWS
    { return details::memop::i32_store<details::bounds_check_mmap_path, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_store_mmap_judge(Type... type) UWVM_THROWS
    { return details::memop::i32_store<details::bounds_check_mmap_judge, CompileOption, curr_i32_stack_top, Type...>(type...); }
#else
# if defined(UWVM_USE_MULTITHREAD_ALLOCATOR)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_store_multithread_allocator(Type... type) UWVM_THROWS
    { return details::memop::i32_store<details::bounds_check_allocator, CompileOption, curr_i32_stack_top, Type...>(type...); }
# else
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_store_singlethread_allocator(Type... type) UWVM_THROWS
    { return details::memop::i32_store<details::bounds_check_allocator, CompileOption, curr_i32_stack_top, Type...>(type...); }
# endif
#endif

    /// @brief `i64.store` opcode (tail-call): stores a 64-bit value to linear memory.
    /// @details
    /// - Stack-top optimization: supported when i64 stack-top caching is enabled; requires i32 and i64 stack-top ranges to be merged so the address can be read
    /// from the same ring.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_store(Type... type) UWVM_THROWS
    { return details::memop::i64_store<details::bounds_check_generic, CompileOption, curr_i64_stack_top, Type...>(type...); }

#if defined(UWVM_SUPPORT_MMAP)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_store_mmap_full(Type... type) UWVM_THROWS
    { return details::memop::i64_store<details::bounds_check_mmap_full, CompileOption, curr_i64_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_store_mmap_path(Type... type) UWVM_THROWS
    { return details::memop::i64_store<details::bounds_check_mmap_path, CompileOption, curr_i64_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_store_mmap_judge(Type... type) UWVM_THROWS
    { return details::memop::i64_store<details::bounds_check_mmap_judge, CompileOption, curr_i64_stack_top, Type...>(type...); }
#else
# if defined(UWVM_USE_MULTITHREAD_ALLOCATOR)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_store_multithread_allocator(Type... type) UWVM_THROWS
    { return details::memop::i64_store<details::bounds_check_allocator, CompileOption, curr_i64_stack_top, Type...>(type...); }
# else
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_store_singlethread_allocator(Type... type) UWVM_THROWS
    { return details::memop::i64_store<details::bounds_check_allocator, CompileOption, curr_i64_stack_top, Type...>(type...); }
# endif
#endif

    /// @brief `f32.store` opcode (tail-call): stores a 32-bit float to linear memory.
    /// @details
    /// - Stack-top optimization: supported when f32 stack-top caching is enabled; requires i32 and f32 stack-top ranges to be merged so the address can be read
    /// from the same ring.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_store(Type... type) UWVM_THROWS
    { return details::memop::f32_store<details::bounds_check_generic, CompileOption, curr_f32_stack_top, Type...>(type...); }

#if defined(UWVM_SUPPORT_MMAP)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_store_mmap_full(Type... type) UWVM_THROWS
    { return details::memop::f32_store<details::bounds_check_mmap_full, CompileOption, curr_f32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_store_mmap_path(Type... type) UWVM_THROWS
    { return details::memop::f32_store<details::bounds_check_mmap_path, CompileOption, curr_f32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_store_mmap_judge(Type... type) UWVM_THROWS
    { return details::memop::f32_store<details::bounds_check_mmap_judge, CompileOption, curr_f32_stack_top, Type...>(type...); }
#else
# if defined(UWVM_USE_MULTITHREAD_ALLOCATOR)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_store_multithread_allocator(Type... type) UWVM_THROWS
    { return details::memop::f32_store<details::bounds_check_allocator, CompileOption, curr_f32_stack_top, Type...>(type...); }
# else
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_store_singlethread_allocator(Type... type) UWVM_THROWS
    { return details::memop::f32_store<details::bounds_check_allocator, CompileOption, curr_f32_stack_top, Type...>(type...); }
# endif
#endif

    /// @brief `f64.store` opcode (tail-call): stores a 64-bit float to linear memory.
    /// @details
    /// - Stack-top optimization: supported when f64 stack-top caching is enabled; requires scalar stack-top ranges to be fully merged so the address can be
    /// read from the same ring.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_store(Type... type) UWVM_THROWS
    { return details::memop::f64_store<details::bounds_check_generic, CompileOption, curr_f64_stack_top, Type...>(type...); }

#if defined(UWVM_SUPPORT_MMAP)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_store_mmap_full(Type... type) UWVM_THROWS
    { return details::memop::f64_store<details::bounds_check_mmap_full, CompileOption, curr_f64_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_store_mmap_path(Type... type) UWVM_THROWS
    { return details::memop::f64_store<details::bounds_check_mmap_path, CompileOption, curr_f64_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_store_mmap_judge(Type... type) UWVM_THROWS
    { return details::memop::f64_store<details::bounds_check_mmap_judge, CompileOption, curr_f64_stack_top, Type...>(type...); }
#else
# if defined(UWVM_USE_MULTITHREAD_ALLOCATOR)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_store_multithread_allocator(Type... type) UWVM_THROWS
    { return details::memop::f64_store<details::bounds_check_allocator, CompileOption, curr_f64_stack_top, Type...>(type...); }
# else
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_store_singlethread_allocator(Type... type) UWVM_THROWS
    { return details::memop::f64_store<details::bounds_check_allocator, CompileOption, curr_f64_stack_top, Type...>(type...); }
# endif
#endif

    /// @brief `i32.store{8,16}` core (tail-call): stores the low N bytes of an i32 value to linear memory.
    /// @details
    /// - Stack-top optimization: required; reads `value` and `addr` from the i32 stack-top ring.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    template <unsigned StoreBytes, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_storeN(Type... type) UWVM_THROWS
    { return details::memop::i32_storeN<details::bounds_check_generic, StoreBytes, CompileOption, curr_i32_stack_top, Type...>(type...); }

    /// @brief `i64.store{8,16,32}` core (tail-call): stores the low N bytes of an i64 value to linear memory.
    /// @details
    /// - Stack-top optimization: supported when i64 stack-top caching is enabled; requires i32 and i64 stack-top ranges to be merged.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][align:u32][offset:u32][next_opfunc_ptr]`.
    template <unsigned StoreBytes, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_storeN(Type... type) UWVM_THROWS
    { return details::memop::i64_storeN<details::bounds_check_generic, StoreBytes, CompileOption, curr_i64_stack_top, Type...>(type...); }

    /// @brief `memory.size` opcode (tail-call): returns the current memory size in pages.
    /// @details
    /// - Stack-top optimization: supported; pushes an i32 result (may be placed into the i32 stack-top ring).
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][next_opfunc_ptr]`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_memory_size(Type... type) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
        auto const pages{static_cast<wasm_i32>(memory_p->get_page_size())};

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t range_begin{CompileOption.i32_stack_top_begin_pos};
            constexpr ::std::size_t range_end{CompileOption.i32_stack_top_end_pos};
            static_assert(range_begin <= curr_i32_stack_top && curr_i32_stack_top < range_end);

            constexpr ::std::size_t new_pos{details::ring_prev_pos(curr_i32_stack_top, range_begin, range_end)};
            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_pos>(pages, type...);
        }
        else
        {
            ::std::memcpy(type...[1u], ::std::addressof(pages), sizeof(pages));
            type...[1u] += sizeof(pages);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `memory.grow` opcode (tail-call): grows memory by a delta (in pages) and returns the previous size or -1.
    /// @details
    /// - Stack-top optimization: supported for the i32 delta operand and i32 result when i32 stack-top caching is enabled.
    /// - `type[0]` layout: `[opfunc_ptr][native_memory_t*][max_limit_memory_length:size_t][next_opfunc_ptr]`.
    /// @note Growth may be strict or silent depending on `grow_strict` configuration; the Wasm result uses `-1` on failure for strict growth.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_memory_grow(Type... type) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
        ::std::size_t const max_limit_memory_length{details::read_imm<::std::size_t>(type...[0])};

        wasm_i32 const delta_i32{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
        auto const delta_pages{static_cast<::std::size_t>(static_cast<::std::uint_least32_t>(delta_i32))};

        auto const old_pages{static_cast<::std::size_t>(memory_p->get_page_size())};

        wasm_i32 result_pages{};

        // We intentionally keep a single `memory.grow` opcode and branch on the global `grow_strict` flag here instead of emitting two separate opcodes
        // (strict vs. silent). `memory.grow` is an inherently heavyweight operation (may allocate/relocate/commit pages, update metadata, and synchronize),
        // so the extra predictable branch has no measurable advantage to split at the opcode level, but it *does* increase code size and translation-table
        // surface area.
        if(::uwvm2::object::memory::flags::grow_strict)
        {
            bool const ok{memory_p->grow_strictly(delta_pages, max_limit_memory_length)};
            if(ok) { result_pages = static_cast<wasm_i32>(old_pages); }
            else
            {
                result_pages = static_cast<wasm_i32>(-1);
            }
        }
        else
        {
            memory_p->grow_silently(delta_pages, max_limit_memory_length);
            result_pages = static_cast<wasm_i32>(old_pages);
        }

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_i32_stack_top>(result_pages, type...);
        }
        else
        {
            ::std::memcpy(type...[1u], ::std::addressof(result_pages), sizeof(result_pages));
            type...[1u] += sizeof(result_pages);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    // =========================
    // Non-tailcall (byref mode)
    // =========================

    /// @brief Memory opcodes (non-tail-call/byref): operate purely on the operand stack.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching; all stack-top ranges must be `SIZE_MAX`).
    /// - `type[0]` layout: loads/stores still consume the same immediates as tail-call mode, but use `[opfunc_byref_ptr]` instead of `[opfunc_ptr]` and do not
    ///   dispatch the next opfunc (the outer interpreter loop drives execution).

    /// @brief `i32.load` opcode (non-tail-call/byref): loads a 32-bit little-endian value from linear memory.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 4uz);

        auto const out{details::load_i32_le(memory.memory_begin + eff)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i64.load` opcode (non-tail-call/byref): loads a 64-bit little-endian value from linear memory.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_i64 = details::wasm_i64;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 8uz);

        auto const out{details::load_i64_le(memory.memory_begin + eff)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f32.load` opcode (non-tail-call/byref): loads a 32-bit little-endian float from linear memory.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_load(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f32 = details::wasm_f32;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 4uz);

        auto const out{details::load_f32_le(memory.memory_begin + eff)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f64.load` opcode (non-tail-call/byref): loads a 64-bit little-endian float from linear memory.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_load(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f64 = details::wasm_f64;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 8uz);

        auto const out{details::load_f64_le(memory.memory_begin + eff)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i32.load8_{s,u}` core (non-tail-call/byref): loads 1 byte and extends to i32.
    template <bool Signed, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load8(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 1uz);

        ::std::uint_least8_t b{details::load_u8(memory.memory_begin + eff)};
        wasm_i32 out{};
        if constexpr(Signed) { out = static_cast<wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least8_t>(b))); }
        else
        {
            out = static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(b));
        }

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i32.load16_{s,u}` core (non-tail-call/byref): loads 2 bytes and extends to i32.
    template <bool Signed, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load16(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 2uz);

        ::std::uint_least16_t tmp;  // no init
        ::std::memcpy(::std::addressof(tmp), memory.memory_begin + eff, sizeof(tmp));
        tmp = ::fast_io::little_endian(tmp);

        wasm_i32 out{};
        if constexpr(Signed) { out = static_cast<wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least16_t>(tmp))); }
        else
        {
            out = static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(tmp));
        }

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i64.load8_{s,u}` core (non-tail-call/byref): loads 1 byte and extends to i64.
    template <bool Signed, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load8(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_i64 = details::wasm_i64;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 1uz);

        ::std::uint_least8_t b{details::load_u8(memory.memory_begin + eff)};
        wasm_i64 out{};
        if constexpr(Signed) { out = static_cast<wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least8_t>(b))); }
        else
        {
            out = static_cast<wasm_i64>(static_cast<::std::uint_least64_t>(b));
        }

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i64.load16_{s,u}` core (non-tail-call/byref): loads 2 bytes and extends to i64.
    template <bool Signed, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load16(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_i64 = details::wasm_i64;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 2uz);

        ::std::uint_least16_t tmp;  // no init
        ::std::memcpy(::std::addressof(tmp), memory.memory_begin + eff, sizeof(tmp));
        tmp = ::fast_io::little_endian(tmp);

        wasm_i64 out{};
        if constexpr(Signed) { out = static_cast<wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least16_t>(tmp))); }
        else
        {
            out = static_cast<wasm_i64>(static_cast<::std::uint_least64_t>(tmp));
        }

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i64.load32_{s,u}` core (non-tail-call/byref): loads 4 bytes and extends to i64.
    template <bool Signed, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load32(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_i64 = details::wasm_i64;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 4uz);

        ::std::uint_least32_t tmp;  // no init
        ::std::memcpy(::std::addressof(tmp), memory.memory_begin + eff, sizeof(tmp));
        tmp = ::fast_io::little_endian(tmp);

        wasm_i64 out{};
        if constexpr(Signed) { out = static_cast<wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least32_t>(tmp))); }
        else
        {
            out = static_cast<wasm_i64>(static_cast<::std::uint_least64_t>(tmp));
        }

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_store(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const value{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 4uz);
        details::store_i32_le(memory.memory_begin + eff, value);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_store(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_i64 = details::wasm_i64;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i64 const value{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 8uz);
        details::store_i64_le(memory.memory_begin + eff, value);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_store(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f32 = details::wasm_f32;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_f32 const value{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 4uz);
        details::store_f32_le(memory.memory_begin + eff, value);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_store(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f64 = details::wasm_f64;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_f64 const value{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, 8uz);
        details::store_f64_le(memory.memory_begin + eff, value);
    }

    template <unsigned StoreBytes, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_storeN(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(StoreBytes == 1u || StoreBytes == 2u);
        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const value{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, static_cast<::std::size_t>(StoreBytes));

        if constexpr(StoreBytes == 1u)
        {
            details::store_u8(memory.memory_begin + eff, static_cast<::std::uint_least8_t>(::std::bit_cast<::std::uint_least32_t>(value)));
        }
        else
        {
            details::store_u16_le(memory.memory_begin + eff, static_cast<::std::uint_least16_t>(::std::bit_cast<::std::uint_least32_t>(value)));
        }
    }

    template <unsigned StoreBytes, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_storeN(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_i64 = details::wasm_i64;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(StoreBytes == 1u || StoreBytes == 2u || StoreBytes == 4u);
        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        [[maybe_unused]] wasm_u32 const align{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i64 const value{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        wasm_i32 const addr{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::size_t const eff{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff, static_cast<::std::size_t>(StoreBytes));

        auto const bits{::std::bit_cast<::std::uint_least64_t>(value)};
        if constexpr(StoreBytes == 1u) { details::store_u8(memory.memory_begin + eff, static_cast<::std::uint_least8_t>(bits)); }
        else if constexpr(StoreBytes == 2u) { details::store_u16_le(memory.memory_begin + eff, static_cast<::std::uint_least16_t>(bits)); }
        else
        {
            details::store_u32_le(memory.memory_begin + eff, static_cast<::std::uint_least32_t>(bits));
        }
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_memory_size(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        auto const pages{static_cast<wasm_i32>(memory_p->get_page_size())};

        ::std::memcpy(typeref...[1u], ::std::addressof(pages), sizeof(pages));
        typeref...[1u] += sizeof(pages);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_memory_grow(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        ::std::size_t const max_limit_memory_length{details::read_imm<::std::size_t>(typeref...[0])};

        wasm_i32 const delta_i32{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        auto const delta_pages{static_cast<::std::size_t>(static_cast<::std::uint_least32_t>(delta_i32))};

        auto const old_pages{static_cast<::std::size_t>(memory_p->get_page_size())};

        wasm_i32 result_pages{};
        // Same rationale as the tail-call version: do not split strict/silent growth into separate opcodes.
        // The branch is negligible compared to the growth work itself; splitting would only bloat the opcode set and the translator without speeding up
        // the hot path (because `memory.grow` is not a hot-path instruction).
        if(::uwvm2::object::memory::flags::grow_strict)
        {
            bool const ok{memory_p->grow_strictly(delta_pages, max_limit_memory_length)};
            if(ok) { result_pages = static_cast<wasm_i32>(old_pages); }
            else
            {
                result_pages = static_cast<wasm_i32>(-1);
            }
        }
        else
        {
            memory_p->grow_silently(delta_pages, max_limit_memory_length);
            result_pages = static_cast<wasm_i32>(old_pages);
        }

        ::std::memcpy(typeref...[1u], ::std::addressof(result_pages), sizeof(result_pages));
        typeref...[1u] += sizeof(result_pages);
    }

    // =========================
    // Opcode aliases (spec names)
    // =========================

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load8_s(Type... type) UWVM_THROWS
    { return uwvmint_i32_load8<true, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load8_u(Type... type) UWVM_THROWS
    { return uwvmint_i32_load8<false, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load16_s(Type... type) UWVM_THROWS
    { return uwvmint_i32_load16<true, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load16_u(Type... type) UWVM_THROWS
    { return uwvmint_i32_load16<false, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load8_s(Type... type) UWVM_THROWS
    { return uwvmint_i64_load8<true, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load8_u(Type... type) UWVM_THROWS
    { return uwvmint_i64_load8<false, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load16_s(Type... type) UWVM_THROWS
    { return uwvmint_i64_load16<true, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load16_u(Type... type) UWVM_THROWS
    { return uwvmint_i64_load16<false, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load32_s(Type... type) UWVM_THROWS
    { return uwvmint_i64_load32<true, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load32_u(Type... type) UWVM_THROWS
    { return uwvmint_i64_load32<false, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_store8(Type... type) UWVM_THROWS
    { return uwvmint_i32_storeN<1u, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_store16(Type... type) UWVM_THROWS
    { return uwvmint_i32_storeN<2u, CompileOption, curr_i32_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_store8(Type... type) UWVM_THROWS
    { return uwvmint_i64_storeN<1u, CompileOption, curr_i64_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_store16(Type... type) UWVM_THROWS
    { return uwvmint_i64_storeN<2u, CompileOption, curr_i64_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_store32(Type... type) UWVM_THROWS
    { return uwvmint_i64_storeN<4u, CompileOption, curr_i64_stack_top, Type...>(type...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load8_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_load8<true, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load8_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_load8<false, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load16_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_load16<true, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_load16_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_load16<false, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load8_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_load8<true, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load8_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_load8<false, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load16_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_load16<true, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load16_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_load16<false, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load32_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_load32<true, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_load32_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_load32<false, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_store8(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_storeN<1u, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_store16(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_storeN<2u, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_store8(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_storeN<1u, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_store16(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_storeN<2u, CompileOption, TypeRef...>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_store32(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_storeN<4u, CompileOption, TypeRef...>(typeref...); }

    /// @brief Translation helpers for memory opcodes.
    /// @details
    /// - Tail-call mode: selects a specialization based on the current stack-top cursor position so that stack-top cached operands are accessed via the correct
    ///   `curr_*_stack_top` template parameter.
    /// - Non-tail-call/byref mode: stack-top caching is disabled; translation typically returns the byref variant directly.
    /// - `type[0]` layout: not applicable in translation; these helpers do not manipulate the bytecode stream pointer.
    namespace translate
    {
        namespace details
        {
            namespace op_details = ::uwvm2::runtime::compiler::uwvm_int::optable::details;

            /// @brief Compile-time selector for stack-top-aware opfuncs (tail-call).
            /// @details `OpWrapper` is a variable template that evaluates to the target function pointer:
            /// `OpWrapper<Opt, Pos, Type...> -> uwvm_interpreter_opfunc_t<Type...>`.
            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t Curr,
                      ::std::size_t End,
                      template <uwvm_interpreter_translate_option_t, ::std::size_t, uwvm_int_stack_top_type...> auto OpWrapper,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_by_currpos_impl(::std::size_t pos) noexcept
            {
                if(pos == Curr) { return OpWrapper<CompileOption, Curr, Type...>; }
                else
                {
                    if constexpr(Curr + 1uz < End) { return select_stacktop_fptr_by_currpos_impl<CompileOption, Curr + 1uz, End, OpWrapper, Type...>(pos); }
                    else
                    {
                        return OpWrapper<CompileOption, Curr, Type...>;
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t Begin,
                      ::std::size_t End,
                      template <uwvm_interpreter_translate_option_t, ::std::size_t, uwvm_int_stack_top_type...> auto OpWrapper,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            UWVM_ALWAYS_INLINE inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_or_default(::std::size_t pos) noexcept
            {
                if constexpr(Begin != End) { return select_stacktop_fptr_by_currpos_impl<CompileOption, Begin, End, OpWrapper, Type...>(pos); }
                else
                {
                    return OpWrapper<CompileOption, 0uz, Type...>;
                }
            }

            /// @brief Compile-time selector for stack-top-aware opfuncs with a chosen bounds-check policy (tail-call).
            /// @details `OpWithBoundsCheck` is a variable template that evaluates to the target function pointer:
            /// `OpWithBoundsCheck<BoundsCheckFn, Extra, Opt, Pos, Type...> -> uwvm_interpreter_opfunc_t<Type...>`.
            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t Curr,
                      ::std::size_t End,
                      template <auto, auto, uwvm_interpreter_translate_option_t, ::std::size_t, uwvm_int_stack_top_type...> auto OpWithBoundsCheck,
                      auto BoundsCheckFn,
                      auto Extra,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_by_currpos_with_impl(::std::size_t pos) noexcept
            {
                if(pos == Curr) { return OpWithBoundsCheck<BoundsCheckFn, Extra, CompileOption, Curr, Type...>; }
                else
                {
                    if constexpr(Curr + 1uz < End)
                    {
                        return select_stacktop_fptr_by_currpos_with_impl<CompileOption, Curr + 1uz, End, OpWithBoundsCheck, BoundsCheckFn, Extra, Type...>(pos);
                    }
                    else
                    {
                        return OpWithBoundsCheck<BoundsCheckFn, Extra, CompileOption, Curr, Type...>;
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t Begin,
                      ::std::size_t End,
                      template <auto, auto, uwvm_interpreter_translate_option_t, ::std::size_t, uwvm_int_stack_top_type...> auto OpWithBoundsCheck,
                      auto BoundsCheckFn,
                      auto Extra,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            UWVM_ALWAYS_INLINE inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_or_default_with(::std::size_t pos) noexcept
            {
                if constexpr(Begin != End)
                {
                    return select_stacktop_fptr_by_currpos_with_impl<CompileOption, Begin, End, OpWithBoundsCheck, BoundsCheckFn, Extra, Type...>(pos);
                }
                else
                {
                    return OpWithBoundsCheck<BoundsCheckFn, Extra, CompileOption, 0uz, Type...>;
                }
            }

#if defined(UWVM_SUPPORT_MMAP)
            enum class mmap_variant : unsigned
            {
                full,
                path,
                judge
            };

            UWVM_ALWAYS_INLINE inline constexpr mmap_variant select_mmap_variant(op_details::native_memory_t const& memory) noexcept
            {
                if(memory.require_dynamic_determination_memory_size()) { return mmap_variant::judge; }

                if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
                {
                    // 64-bit: wasm32 full protection; wasm64 partial protection.
                    if(memory.status == ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm32) { return mmap_variant::full; }
                    return mmap_variant::path;
                }
                else
                {
                    // 32-bit: always partial protection.
                    return mmap_variant::path;
                }
            }
#endif

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t Begin,
                      ::std::size_t End,
                      template <auto, auto, uwvm_interpreter_translate_option_t, ::std::size_t, uwvm_int_stack_top_type...> auto OpWithBoundsCheck,
                      auto Extra,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            UWVM_ALWAYS_INLINE inline constexpr uwvm_interpreter_opfunc_t<Type...>
                select_mem_fptr_or_default(::std::size_t pos, op_details::native_memory_t const& memory) noexcept
            {
#if defined(UWVM_SUPPORT_MMAP)
                switch(select_mmap_variant(memory))
                {
                    case mmap_variant::full:
                    {
                        return select_stacktop_fptr_or_default_with<CompileOption,
                                                                    Begin,
                                                                    End,
                                                                    OpWithBoundsCheck,
                                                                    &op_details::bounds_check_mmap_full,
                                                                    Extra,
                                                                    Type...>(pos);
                    }
                    case mmap_variant::path:
                    {
                        return select_stacktop_fptr_or_default_with<CompileOption,
                                                                    Begin,
                                                                    End,
                                                                    OpWithBoundsCheck,
                                                                    &op_details::bounds_check_mmap_path,
                                                                    Extra,
                                                                    Type...>(pos);
                    }
                    case mmap_variant::judge:
                    {
                        return select_stacktop_fptr_or_default_with<CompileOption,
                                                                    Begin,
                                                                    End,
                                                                    OpWithBoundsCheck,
                                                                    &op_details::bounds_check_mmap_judge,
                                                                    Extra,
                                                                    Type...>(pos);
                    }
                    [[unlikely]] default:
                    {
                        return select_stacktop_fptr_or_default_with<CompileOption,
                                                                    Begin,
                                                                    End,
                                                                    OpWithBoundsCheck,
                                                                    &op_details::bounds_check_mmap_path,
                                                                    Extra,
                                                                    Type...>(pos);
                    }
                }
#else
                (void)memory;
                return select_stacktop_fptr_or_default_with<CompileOption, Begin, End, OpWithBoundsCheck, &op_details::bounds_check_allocator, Extra, Type...>(
                    pos);
#endif
            }

            // ===== Memory-aware op wrappers (bounds policy is chosen by translator) =====

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i32_load_op_with{op_details::memop::i32_load<BoundsCheckFn, CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_load_op_with{op_details::memop::i64_load<BoundsCheckFn, CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> f32_load_op_with{op_details::memop::f32_load<BoundsCheckFn, CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> f64_load_op_with{op_details::memop::f64_load<BoundsCheckFn, CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i32_load8_op_with{
                op_details::memop::i32_load8<BoundsCheckFn, static_cast<bool>(Extra), CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i32_load16_op_with{
                op_details::memop::i32_load16<BoundsCheckFn, static_cast<bool>(Extra), CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_load8_op_with{
                op_details::memop::i64_load8<BoundsCheckFn, static_cast<bool>(Extra), CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_load16_op_with{
                op_details::memop::i64_load16<BoundsCheckFn, static_cast<bool>(Extra), CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_load32_op_with{
                op_details::memop::i64_load32<BoundsCheckFn, static_cast<bool>(Extra), CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i32_store_op_with{op_details::memop::i32_store<BoundsCheckFn, CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_store_op_with{op_details::memop::i64_store<BoundsCheckFn, CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> f32_store_op_with{op_details::memop::f32_store<BoundsCheckFn, CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> f64_store_op_with{op_details::memop::f64_store<BoundsCheckFn, CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i32_storeN_op_with{
                op_details::memop::i32_storeN<BoundsCheckFn, static_cast<unsigned>(Extra), CompileOption, Pos, Type...>};

            template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_storeN_op_with{
                op_details::memop::i64_storeN<BoundsCheckFn, static_cast<unsigned>(Extra), CompileOption, Pos, Type...>};

            // ===== Default (non-memory-aware) op wrappers =====

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i32_load_op{uwvmint_i32_load<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_load_op{uwvmint_i64_load<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> f32_load_op{uwvmint_f32_load<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> f64_load_op{uwvmint_f64_load<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i32_load8_s_op{uwvmint_i32_load8_s<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i32_load8_u_op{uwvmint_i32_load8_u<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i32_load16_s_op{uwvmint_i32_load16_s<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i32_load16_u_op{uwvmint_i32_load16_u<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_load8_s_op{uwvmint_i64_load8_s<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_load8_u_op{uwvmint_i64_load8_u<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_load16_s_op{uwvmint_i64_load16_s<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_load16_u_op{uwvmint_i64_load16_u<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_load32_s_op{uwvmint_i64_load32_s<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_load32_u_op{uwvmint_i64_load32_u<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i32_store_op{uwvmint_i32_store<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_store_op{uwvmint_i64_store<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> f32_store_op{uwvmint_f32_store<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> f64_store_op{uwvmint_f64_store<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i32_store8_op{uwvmint_i32_store8<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i32_store16_op{uwvmint_i32_store16<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_store8_op{uwvmint_i64_store8<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_store16_op{uwvmint_i64_store16<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> i64_store32_op{uwvmint_i64_store32<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> memory_size_op{uwvmint_memory_size<CompileOption, Pos, Type...>};

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> memory_grow_op{uwvmint_memory_grow<CompileOption, Pos, Type...>};
        }  // namespace details

        // =====================================
        // Memory-aware translator overloads
        // =====================================

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                      details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_load_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   details::op_details::native_memory_t const& memory,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                      details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i64_load_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   details::op_details::native_memory_t const& memory,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_load_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                      details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::f32_load_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   details::op_details::native_memory_t const& memory,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_load_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                      details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::f64_load_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   details::op_details::native_memory_t const& memory,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load8_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                         details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_load8_op_with,
                                                       true,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load8_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      details::op_details::native_memory_t const& memory,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load8_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load8_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                         details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_load8_op_with,
                                                       false,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load8_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      details::op_details::native_memory_t const& memory,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load8_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load16_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                          details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_load16_op_with,
                                                       true,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load16_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       details::op_details::native_memory_t const& memory,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load16_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load16_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                          details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_load16_op_with,
                                                       false,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load16_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       details::op_details::native_memory_t const& memory,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load16_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load8_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                         details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i64_load8_op_with,
                                                       true,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load8_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      details::op_details::native_memory_t const& memory,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load8_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load8_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                         details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i64_load8_op_with,
                                                       false,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load8_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      details::op_details::native_memory_t const& memory,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load8_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load16_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                          details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i64_load16_op_with,
                                                       true,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load16_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       details::op_details::native_memory_t const& memory,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load16_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load16_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                          details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i64_load16_op_with,
                                                       false,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load16_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       details::op_details::native_memory_t const& memory,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load16_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load32_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                          details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i64_load32_op_with,
                                                       true,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       details::op_details::native_memory_t const& memory,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load32_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                          details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i64_load32_op_with,
                                                       false,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       details::op_details::native_memory_t const& memory,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load32_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_store_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                       details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_store_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    details::op_details::native_memory_t const& memory,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_store_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                       details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i64_stack_top_begin_pos,
                                                       CompileOption.i64_stack_top_end_pos,
                                                       details::i64_store_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i64_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    details::op_details::native_memory_t const& memory,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_store_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                       details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.f32_stack_top_begin_pos,
                                                       CompileOption.f32_stack_top_end_pos,
                                                       details::f32_store_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.f32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    details::op_details::native_memory_t const& memory,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_store_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                       details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.f64_stack_top_begin_pos,
                                                       CompileOption.f64_stack_top_end_pos,
                                                       details::f64_store_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.f64_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    details::op_details::native_memory_t const& memory,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_store8_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                        details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_storeN_op_with,
                                                       1u,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store8_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     details::op_details::native_memory_t const& memory,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store8_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_store16_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                         details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_storeN_op_with,
                                                       2u,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store16_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      details::op_details::native_memory_t const& memory,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store16_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_store8_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                        details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i64_stack_top_begin_pos,
                                                       CompileOption.i64_stack_top_end_pos,
                                                       details::i64_storeN_op_with,
                                                       1u,
                                                       Type...>(curr_stacktop.i64_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_store8_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     details::op_details::native_memory_t const& memory,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_store8_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_store16_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                         details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i64_stack_top_begin_pos,
                                                       CompileOption.i64_stack_top_end_pos,
                                                       details::i64_storeN_op_with,
                                                       2u,
                                                       Type...>(curr_stacktop.i64_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_store16_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      details::op_details::native_memory_t const& memory,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_store16_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_store32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                         details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i64_stack_top_begin_pos,
                                                       CompileOption.i64_stack_top_end_pos,
                                                       details::i64_storeN_op_with,
                                                       4u,
                                                       Type...>(curr_stacktop.i64_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_store32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      details::op_details::native_memory_t const& memory,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_store32_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_load_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_load<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i64_load_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_load<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_load_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::f32_load_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f32_load<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_load_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::f64_load_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f64_load<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        // The remaining load/store ops follow the same stack-top driver (address is i32 for loads; value-type range for stores).

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load8_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_load8_s_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_load8_s<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load8_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load8_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load8_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_load8_u_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_load8_u<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load8_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load8_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load16_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_load16_s_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_load16_s<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load16_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load16_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load16_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_load16_u_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_load16_u<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load16_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load16_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load8_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i64_load8_s_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_load8_s<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load8_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load8_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load8_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i64_load8_u_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_load8_u<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load8_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load8_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load16_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i64_load16_s_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_load16_s<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load16_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load16_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load16_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i64_load16_u_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_load16_u<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load16_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load16_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load32_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i64_load32_s_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_load32_s<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load32_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i64_load32_u_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_load32_u<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load32_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_store_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_store_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_store<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_store_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_store_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_store<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_store_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f32_stack_top_begin_pos,
                                                                     CompileOption.f32_stack_top_end_pos,
                                                                     details::f32_store_op,
                                                                     Type...>(curr_stacktop.f32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f32_store<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_store_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f64_stack_top_begin_pos,
                                                                     CompileOption.f64_stack_top_end_pos,
                                                                     details::f64_store_op,
                                                                     Type...>(curr_stacktop.f64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f64_store<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_store8_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_store8_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_store8<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store8_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store8_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_store16_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_store16_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_store16<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store16_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store16_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_store8_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_store8_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_store8<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_store8_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_store8_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_store16_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_store16_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_store16<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_store16_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_store16_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_store32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_store32_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_store32<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_store32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_store32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_memory_size_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::memory_size_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_memory_size<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_memory_size_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_memory_size_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_memory_grow_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::memory_grow_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_memory_grow<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_memory_grow_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_memory_grow_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        // Non-tailcall: single version per op.
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_load_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_load<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_load_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_load<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_load_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_load<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_load_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_load<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        // For brevity, map remaining byref getters directly to the byref op templates.
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_load8_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_load8_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load8_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load8_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_load8_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_load8_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load8_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load8_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_load16_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_load16_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load16_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load16_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_load16_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_load16_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load16_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load16_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_load8_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_load8_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load8_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load8_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_load8_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_load8_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load8_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load8_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_load16_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_load16_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load16_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load16_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_load16_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_load16_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load16_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load16_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_load32_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_load32_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_load32_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_load32_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_load32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_load32_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_store_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_store<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_store_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_store<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_store_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_store<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_store_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_store<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_store8_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_store8<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store8_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store8_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_store16_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_store16<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store16_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store16_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_store8_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_store8<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_store8_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_store8_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_store16_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_store16<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_store16_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_store16_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_store32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_store32<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_store32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_store32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_memory_size_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_memory_size<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_memory_size_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_memory_size_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_memory_grow_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_memory_grow<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_memory_grow_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_memory_grow_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

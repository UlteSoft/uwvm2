/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

namespace uwvm2::runtime::lib::details
{
    // Eager runtime registries contain backend- and policy-specific pointers. A live publication is reusable only when every
    // setting that shaped those registries is unchanged; all other transitions require an explicit quiescent reset.
    enum class runtime_state_kind : ::std::uint_least8_t
    {
        empty,
        full
    };

    struct runtime_state_signature
    {
        runtime_state_kind kind{};
        ::std::uint_least32_t runtime_compiler{};
        ::std::uint_least32_t runtime_mode{};

        bool runtime_compile_threads_existed{};
        ::std::uint_least32_t runtime_compile_threads_policy{};
        ::std::size_t runtime_compile_threads_resolved{};
        bool runtime_scheduling_policy_existed{};
        ::std::uint_least32_t runtime_scheduling_policy{};
        ::std::size_t runtime_scheduling_size{};

        bool uwvm_int_disable_loop_unwind{};
        ::std::uint_least32_t uwvm_int_opcode_conbination_level{};
        bool uwvm_int_disable_delay_local{};
        bool uwvm_int_enable_instruction_reorder{};
        ::std::size_t uwvm_int_loop_unwind_max_size{};

        bool llvm_jit_policy_existed{};
        ::std::uint_least32_t llvm_jit_policy{};
        bool llvm_jit_full_policy_existed{};
        ::std::uint_least32_t llvm_jit_full_policy{};
        bool llvm_jit_call_stack_existed{};
        ::std::uint_least32_t llvm_jit_call_stack{};
        bool llvm_jit_cache_path_existed{};
        ::std::uint_least32_t llvm_jit_cache_path_mode{};
        ::std::size_t llvm_jit_cache_path_size{};
        ::std::uint_least64_t llvm_jit_cache_path_hash{};
        bool llvm_jit_cache_enabled{};
        bool llvm_jit_cache_generate_signature{};
        bool llvm_jit_cache_verify_signature{};
    };

    [[nodiscard]] inline constexpr bool operator== (runtime_state_signature const& lhs, runtime_state_signature const& rhs) noexcept
    {
        return lhs.kind == rhs.kind && lhs.runtime_compiler == rhs.runtime_compiler && lhs.runtime_mode == rhs.runtime_mode &&
               lhs.runtime_compile_threads_existed == rhs.runtime_compile_threads_existed &&
               lhs.runtime_compile_threads_policy == rhs.runtime_compile_threads_policy &&
               lhs.runtime_compile_threads_resolved == rhs.runtime_compile_threads_resolved &&
               lhs.runtime_scheduling_policy_existed == rhs.runtime_scheduling_policy_existed &&
               lhs.runtime_scheduling_policy == rhs.runtime_scheduling_policy && lhs.runtime_scheduling_size == rhs.runtime_scheduling_size &&
               lhs.uwvm_int_disable_loop_unwind == rhs.uwvm_int_disable_loop_unwind &&
               lhs.uwvm_int_opcode_conbination_level == rhs.uwvm_int_opcode_conbination_level &&
               lhs.uwvm_int_disable_delay_local == rhs.uwvm_int_disable_delay_local &&
               lhs.uwvm_int_enable_instruction_reorder == rhs.uwvm_int_enable_instruction_reorder &&
               lhs.uwvm_int_loop_unwind_max_size == rhs.uwvm_int_loop_unwind_max_size &&
               lhs.llvm_jit_policy_existed == rhs.llvm_jit_policy_existed && lhs.llvm_jit_policy == rhs.llvm_jit_policy &&
               lhs.llvm_jit_full_policy_existed == rhs.llvm_jit_full_policy_existed && lhs.llvm_jit_full_policy == rhs.llvm_jit_full_policy &&
               lhs.llvm_jit_call_stack_existed == rhs.llvm_jit_call_stack_existed && lhs.llvm_jit_call_stack == rhs.llvm_jit_call_stack &&
               lhs.llvm_jit_cache_path_existed == rhs.llvm_jit_cache_path_existed &&
               lhs.llvm_jit_cache_path_mode == rhs.llvm_jit_cache_path_mode && lhs.llvm_jit_cache_path_size == rhs.llvm_jit_cache_path_size &&
               lhs.llvm_jit_cache_path_hash == rhs.llvm_jit_cache_path_hash && lhs.llvm_jit_cache_enabled == rhs.llvm_jit_cache_enabled &&
               lhs.llvm_jit_cache_generate_signature == rhs.llvm_jit_cache_generate_signature &&
               lhs.llvm_jit_cache_verify_signature == rhs.llvm_jit_cache_verify_signature;
    }

    [[nodiscard]] inline constexpr bool operator!= (runtime_state_signature const& lhs, runtime_state_signature const& rhs) noexcept
    { return !(lhs == rhs); }

    enum class runtime_state_transition : ::std::uint_least8_t
    {
        initialize,
        reuse,
        reject_requires_reset
    };

    // FNV-1a is a stable content fingerprint for the configured cache path. The extent is retained separately so a path
    // change cannot be hidden by a different-length byte sequence with the same accidental fingerprint.
    [[nodiscard]] inline constexpr ::std::uint_least64_t stable_runtime_state_u8_hash(char8_t const* data, ::std::size_t size) noexcept
    {
        constexpr ::std::uint_least64_t offset_basis{static_cast<::std::uint_least64_t>(14695981039346656037ULL)};
        constexpr ::std::uint_least64_t prime{static_cast<::std::uint_least64_t>(1099511628211ULL)};
        ::std::uint_least64_t hash{offset_basis};
        for(::std::size_t i{}; i != size; ++i)
        {
            hash ^= static_cast<::std::uint_least8_t>(data[i]);
            hash *= prime;
        }
        return hash;
    }

    [[nodiscard]] inline constexpr runtime_state_transition classify_runtime_state_transition(runtime_state_signature const& published,
                                                                                               runtime_state_signature const& requested) noexcept
    {
        if(requested.kind == runtime_state_kind::empty) [[unlikely]] { return runtime_state_transition::reject_requires_reset; }
        if(published.kind == runtime_state_kind::empty) { return runtime_state_transition::initialize; }
        return published == requested ? runtime_state_transition::reuse : runtime_state_transition::reject_requires_reset;
    }
}

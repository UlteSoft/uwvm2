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
# include <bit>
# include <cstddef>
# include <cstdint>
# include <concepts>
# include <cstring>
# include <limits>
# include <memory>
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
# include <uwvm2/object/impl.h>
# include "define.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

/**
 * @file register_ring.h
 * @brief High-performance stack-top cache ⇄ operand stack materialization for the UWVM interpreter.
 *
 * @details
 * ## What this provides (layer 1: "what is it doing?")
 * WebAssembly is a stack machine. This header implements the "搬运 system" that moves values between:
 * - The real operand stack in memory (`sp`), and
 * - A cached stack-top segment ("stack-top cache") carried in the interpreter opfunc argument pack
 *   (i.e. ABI-friendly locals / registers).
 *
 * The two main operations are:
 * - **spill**: stack-top cache → operand stack (materialize to memory)
 * - **fill**:  operand stack → stack-top cache (dematerialize from memory)
 *
 * ## Why it exists (layer 2: "why do we need it?")
 * A naïve interpreter performs frequent loads/stores to the operand stack in memory. That is expensive due to
 * cache misses and memory bandwidth. UWVM keeps the hottest part of the operand stack in registers/locals by
 * treating a fixed set of opfunc argument slots as a **stack-top cache**.
 *
 * ## How it achieves performance (layer 3: "how is it fast?")
 * - The stack-top cache is modeled as a **ring buffer** for each value-category (i32/i64/f32/f64/v128).
 * - The spill/fill size, start position and (optionally) per-slot value types are known at compile time
 *   and expanded via templates/`consteval` so runtime work reduces to pointer adjusts + `memcpy`.
 * - Ranges can be **merged** (shared slots with union layouts) to reduce register pressure while still
 *   keeping fully compile-time selection and validation.
 *
 * ## Why this beats classic M3-style threaded interpreters on x86_64 SysV ABI
 * M3/Wasm3's "meta machine" maps a small fixed set of VM registers (pc/sp/mem/r0/fp0) to hardware registers
 * and relies on tail calls / indirect dispatch. That already removes the outer `switch` loop overhead.
 *
 * ### ABI note (why modern ABIs matter)
 * u2 benefits most on modern ABIs that pass many arguments in registers (integer + SIMD/FP), such as:
 * - **x86_64 SysV ABI** (multiple GPR + XMM argument registers), and
 * - **AArch64 AAPCS64** (x0–x7 for integer/pointers and v0–v7 for SIMD/FP arguments).
 *
 * On these ABIs, the opfunc argument pack can keep a wider stack-top cache segment in registers/locals, so
 * more stack-machine operations stay on the cache-hit fast path (register-register ALU), and spill/fill is
 * amortized over larger batches.
 *
 * By contrast, classic M3 typically models only a very small "register file" (pc/sp/mem/r0/fp0). That design
 * already minimizes dispatch cost, but it still needs frequent operand-stack memory loads because most stack
 * values are not resident in registers.
 *
 * However, in a stack machine the dominant cost is often not dispatch but **operand stack traffic**.
 * For example, an M3 u64-or between `r0` and a stack operand typically performs an extra dependent memory load
 * for the stack operand every time:
 * - load an offset/immediate (from pc) → form an address → load the stack slot → ALU op
 *
 * UWVM's u2 interpreter instead caches multiple stack-top values in an explicit ring buffer (per value-category),
 * so common stack ops become **register-register** ALU ops most of the time. Only when the cache boundary is hit
 * (or at specific control-flow / memory-exposure points) do we spill/fill in bulk.
 *
 * ### Worked example (x86_64 SysV ABI): `i64.or` on two stack values
 * The following micro-example is intentionally tiny: it shows why a stack-top cache can beat an M3-style
 * "one register + operand stack loads" topology on stack-heavy code.
 *
 * **Wasm instruction snippet (wat, not a full function/module):**
 * @code{.wat}
 * ;; ... inside some function body ...
 * ;; stack effect: (i64 i64 -- i64)
 * local.get 0
 * local.get 1
 * i64.or
 * @endcode
 *
 * **M3 (from the M3 docs, x86_64 SysV ABI):**
 * @code{.asm}
 * m3`op_u64_Or_sr:
 *     0x1000062c0 <+0>:  movslq (%rdi), %rax             ; load operand stack offset
 *     0x1000062c3 <+3>:  orq    (%rsi,%rax,8), %rcx      ; or r0 with stack operand
 *     0x1000062c7 <+7>:  movq   0x8(%rdi), %rax          ; fetch next operation
 *     0x1000062cb <+11>: addq   $0x10, %rdi              ; increment program counter
 *     0x1000062cf <+15>: jmpq   *%rax                    ; jump to next operation
 * @endcode
 *
 * **u2 (expected shape when both operands hit in the stack-top cache ring):**
 * @code{.asm}
 * ; Preconditions for the cache-hit fast path (selected by `translate::get_*_fptr(...)`):
 * ;   - curr_pos == StartPos  (StartPos denotes the logical top of the ring)
 * ;   - remain_size >= 2      (at least two cached i64 values available)
 * ;
 * ; Stack-top cache ring mapping (i64 ring shown; indices are in the opfunc argument pack):
 * ;   cache[StartPos]                 = top (TOS)
 * ;   cache[ring_next_pos(StartPos)]  = next (NOS, deeper than TOS)
 * ;
 * ; Important: TOS/NOS are not arbitrary two registers. They are *adjacent* in the ring by construction of
 * ; the stack machine semantics (binary ops consume the top two values). Therefore, the code generator only
 * ; needs to specialize by `StartPos` (and `Count`), not by an (i,j) pair:
 * ;   - possible `StartPos` values: N
 * ;   - NOS position is uniquely `ring_next_pos(StartPos)`
 * ; This keeps specialization growth ~O(N) for 2-operand ops, rather than O(N^2) combinations.
 * ;
 * ; Operands are already in registers/locals because cache slots are carried in the opfunc arguments.
 * ; No operand-stack memory load is needed here.
 * orq    %r_cache_nos, %r_cache_tos      ; TOS |= NOS   (exact operand order is opcode-specific)
 *
 * ; threaded dispatch (musttail-style): load next op + jump
 * movq   0x8(%r_ip), %r_tmp             ; fetch next operation pointer
 * addq   $0x10, %r_ip                   ; advance meta-machine pc
 * jmpq   *%r_tmp
 * @endcode
 *
 * @note The u2 block above is a *model* of the steady-state fast path (cache hit). The exact register names
 * and instruction selection depend on the concrete opfunc signature and compiler, but the key property is:
 * the stack operand is typically not loaded from memory at all.
 *
 * ### Rough cycle accounting (illustrative, L1-hit, predicted indirect jump)
 * Assumptions (typical modern x86_64 core; exact numbers vary by micro-architecture):
 * - L1 load-to-use latency: ~4 cycles
 * - `or` ALU latency: ~1 cycle
 * - `add` (pointer increment): ~1 cycle (often single-µop; may overlap)
 * - Indirect `jmp *reg` predicted: ~1 cycle (front-end / predictor dependent; still not "free")
 *
 * @note Yes: the `addq $0x10, %r_ip` and `jmpq *%r_tmp` style dispatch steps are unavoidable in both models.
 * The comparison below focuses on the *extra* work caused by operand-stack traffic; dispatch cost largely
 * cancels out because both M3 and u2 are threaded interpreters.
 *
 * Critical-path intuition for M3 `Or_sr` (steady-state, predicted):
 * - Dispatch (shared baseline): `movq 0x8(%pc), %tmp` (~4) + `addq $0x10, %pc` (~1) + `jmp *%tmp` (~1)
 * - Operand-stack traffic (extra vs u2 cache-hit):
 *   - `movslq (%pc), %rax` (L1 load of offset) → ~4 cycles
 *   - dependent address + `orq (%sp,%rax,8), %r0` (L1 load-to-use of stack operand) → ~4 cycles
 * - Extra operand-related dependency chain ≈ ~8 cycles on top of the shared dispatch baseline.
 *
 * Critical-path intuition for u2 cache-hit:
 * - Operand compute: `orq %r_cache_nos, %r_cache_tos` ≈ ~1 cycle (operands already available)
 * - Dispatch (shared baseline): `movq 0x8(%r_ip), %r_tmp` (~4) + `addq $0x10, %r_ip` (~1) + `jmp *%r_tmp` (~1)
 * - No per-op operand-stack loads on the fast path.
 *
 * **Bottom line (this example, cache-hit):**
 * - M3 steady-state per-op cost ≈ shared dispatch (~6) + extra operand-stack chain (~8) ≈ **~14 cycles/op**
 * - u2 steady-state per-op cost ≈ shared dispatch (~6) + register ALU (~1) ≈ **~7 cycles/op**
 * - Estimated speedup for the hot op core: **~14 / 7 ≈ ~2.0×**
 *
 * In practice this varies with cache-hit rate, front-end/rename/AGU pressure, and whether the two M3 loads
 * overlap off the critical path, but the key point holds: u2 removes the per-op operand-stack memory dependency.
 *
 * ### Relation to JIT (why u2 is near the interpreter ceiling)
 * A baseline/optimizing JIT commonly turns a hot `i64.or` (with operands already in registers) into a single
 * machine instruction (e.g. `orq reg, reg`) inside a straight-line block; there is no per-op interpreter
 * dispatch to pay.
 *
 * In a threaded interpreter, however, the **dispatch sequence** (fetch next-op pointer + advance pc + indirect
 * tail-jump) is structural and shared by almost every opcode. It is difficult to eliminate without turning the
 * interpreter into a JIT (or aggressively fusing long opcode sequences).
 *
 * Therefore, once operand-stack traffic is removed on the cache-hit fast path (as u2 does), the remaining
 * dominant cost is dispatch itself. At that point u2 is already approaching the practical performance ceiling
 * for a non-JIT interpreter on modern ABIs.
 *
 * ### Superscalar / OoO cores (why this gap persists on modern x86)
 * Modern x86_64 cores (e.g. Skylake-family, Zen-family, Raptor Lake-family) are wide and out-of-order:
 * they can decode/issue multiple µops per cycle and hide *independent* latency via a large reorder window.
 * However, **L1 load-to-use latency (≈4 cycles)** is largely fixed, and **dependent** load→use chains remain
 * hard to hide.
 *
 * In this worked example:
 * - u2 cache-hit is dominated by register-register ALU + shared dispatch. The ALU op is 1-cycle latency and
 *   can often overlap with the dispatch sequence on an OoO core.
 * - M3-style `Or_sr` pays extra operand-stack traffic: an offset/immediate load feeding an indexed load feeding
 *   the ALU. This forms a dependent chain that consumes load/AGU resources and exposes the ~4-cycle L1 latency.
 *
 * A compact comparison for the hot path (illustrative):
 * @code{.txt}
 * Dimension             u2 (cache hit)                      M3 (default)
 * -------------------  ----------------------------------  -----------------------------------------
 * Stack value access    register/locals (adjacent ring)     load offset + dependent indexed load
 * Latency chain         ~1c ALU (often overlaps)            ~4c + ~4c dependent load-to-use chain
 * µop/port pressure     ALU/branch heavy, few loads         more load/AGU/LSQ pressure
 * OoO headroom          high (few hard deps)                limited by true deps on loads
 * Front-end bandwidth   smaller steady-state instruction    more instructions/uops per opcode
 * @endcode
 *
 * @note Exact numbers depend on micro-architecture and cache-hit behavior. The key structural point is that
 * u2 shifts hot stack-machine ops from "load-dependent" to "register-dependent", which scales better with
 * wider pipelines and larger OoO windows.
 *
 * @note Direction conventions (critical for correctness):
 * - Operand stack memory is laid out **deep → top** in **ascending addresses**.
 *   The stack pointer `sp` points to the byte **past** the top element (as a normal stack).
 * - In the stack-top cache ring, `StartPos` denotes the **logical top**.
 *   Moving **towards deeper** stack elements uses `ring_next_pos()` (+1 with wrap).
 *   Moving **towards the logical top** uses `ring_prev_pos()` (-1 with wrap).
 */

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    /**
     * @brief Internal compile-time utilities for the register-ring stack-top cache.
     *
     * @details
     * This namespace contains only "plumbing": ring index arithmetic, type/slot layout helpers, and
     * compile-time expanded spill/fill primitives. Public entry points live in `manipulate`.
     */

    namespace details
    {
        inline consteval bool range_enabled(::std::size_t begin_pos, ::std::size_t end_pos) noexcept { return begin_pos != end_pos; }

        inline consteval bool in_range(::std::size_t pos, ::std::size_t begin_pos, ::std::size_t end_pos) noexcept
        { return range_enabled(begin_pos, end_pos) && begin_pos <= pos && pos < end_pos; }

        /**
         * @brief Advance one slot in the cache ring towards deeper stack elements.
         *
         * @details
         * The stack-top cache is a ring over the half-open interval `[begin_pos, end_pos)`.
         * `ring_next_pos()` moves in "depth direction" (away from the logical top):
         * `end_pos-1 -> begin_pos` wraps around.
         */

        inline consteval ::std::size_t ring_next_pos(::std::size_t curr_pos, ::std::size_t begin_pos, ::std::size_t end_pos) noexcept
        {
            // Ring order is [begin_pos, end_pos).
            // next_pos wraps end_pos-1 -> begin_pos.
            return (curr_pos + 1uz == end_pos) ? begin_pos : (curr_pos + 1uz);
        }

        /**
         * @brief Retreat one slot in the cache ring towards the logical stack top.
         *
         * @details
         * This is the inverse of `ring_next_pos()`:
         * `begin_pos -> end_pos-1` wraps around.
         */

        inline consteval ::std::size_t ring_prev_pos(::std::size_t curr_pos, ::std::size_t begin_pos, ::std::size_t end_pos) noexcept
        {
            // Ring order is [begin_pos, end_pos).
            // prev_pos wraps begin_pos -> end_pos-1.
            return (curr_pos == begin_pos) ? (end_pos - 1uz) : (curr_pos - 1uz);
        }

        template <::std::size_t Pos, ::std::size_t Steps, ::std::size_t Begin, ::std::size_t End>
        inline consteval ::std::size_t ring_advance_next_pos() noexcept
        {
            if constexpr(Steps == 0uz) { return Pos; }
            else
            {
                static_assert(Begin < End);
                static_assert(in_range(Pos, Begin, End));
                constexpr ::std::size_t next_pos{ring_next_pos(Pos, Begin, End)};
                return ring_advance_next_pos<next_pos, Steps - 1uz, Begin, End>();
            }
        }

        UWVM_ALWAYS_INLINE inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64
            make_f64_slot_low_from_f32(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 v) noexcept
        {
            using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
            using u32_t = ::std::uint_least32_t;
            using u64_t = ::std::uint_least64_t;

            u32_t const u32{::std::bit_cast<u32_t>(v)};
            u64_t u64{};
            if constexpr(::std::endian::native == ::std::endian::big) { u64 = static_cast<u64_t>(u32) << 32; }
            else
            {
                u64 = static_cast<u64_t>(u32);
            }
            return ::std::bit_cast<wasm_f64>(u64);
        }

        template <typename Scalar>
        UWVM_ALWAYS_INLINE inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128 make_v128_slot_low_from_scalar(Scalar v) noexcept
        {
            using wasm_v128 = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;
            static_assert(sizeof(Scalar) <= sizeof(wasm_v128));

            wasm_v128 out{};  // zero
            if constexpr(::std::endian::native == ::std::endian::big)
            {
                ::std::byte* dst{reinterpret_cast<::std::byte*>(::std::addressof(out)) + (sizeof(wasm_v128) - sizeof(Scalar))};
                ::std::memcpy(dst, ::std::addressof(v), sizeof(Scalar));
            }
            else
            {
                ::std::memcpy(::std::addressof(out), ::std::addressof(v), sizeof(Scalar));
            }
            return out;
        }

        /**
         * @brief Store a value into the stack-top cache slot at `WritePos`.
         *
         * @details
         * The stack-top cache can be configured as multiple rings (i32/i64/f32/f64/v128), and ranges may be
         * merged (shared slots) to reduce register pressure. When merged, a slot can be a union layout.
         *
         * This helper performs the correct compile-time slot selection and write (no runtime branching),
         * ensuring ABI/layout correctness for merged configurations.
         */

        template <uwvm_interpreter_translate_option_t CompileOption, typename ValType, ::std::size_t WritePos, uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void set_curr_val_to_stacktop_cache(ValType v, TypeRef&... typeref) noexcept
        {
            static_assert(is_uwvm_interpreter_valtype_supported<ValType>());
            static_assert(sizeof...(TypeRef) >= 3uz);

            if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>)
            {
                constexpr bool has_stack_top{CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos};
                static_assert(has_stack_top);
                static_assert(CompileOption.i32_stack_top_begin_pos <= WritePos && WritePos < CompileOption.i32_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.i32_stack_top_end_pos);

                constexpr bool is_i32_i64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                                CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos};
                constexpr bool is_i32_f32_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                                CompileOption.i32_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool is_i32_f64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                                CompileOption.i32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                constexpr bool is_i32_f32_i64_f64_merge{is_i32_i64_merge && is_i32_f32_merge && is_i32_f64_merge};

                using slot_t = ::std::remove_cvref_t<TypeRef...[WritePos]>;

                if constexpr(is_i32_f32_i64_f64_merge)
                {
                    static_assert(::std::same_as<slot_t, wasm_stack_top_i32_i64_f32_f64_u>);
                    typeref...[WritePos].i32 = v;
                }
                else if constexpr(is_i32_i64_merge)
                {
                    static_assert(::std::same_as<slot_t, wasm_stack_top_i32_with_i64_u>);
                    typeref...[WritePos].i32 = v;
                }
                else if constexpr(is_i32_f32_merge)
                {
                    static_assert(::std::same_as<slot_t, wasm_stack_top_i32_with_f32_u>);
                    typeref...[WritePos].i32 = v;
                }
                else
                {
                    static_assert(!is_i32_f64_merge);
                    static_assert(::std::same_as<slot_t, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>);
                    typeref...[WritePos] = v;
                }
            }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>)
            {
                constexpr bool has_stack_top{CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos};
                static_assert(has_stack_top);
                static_assert(CompileOption.i64_stack_top_begin_pos <= WritePos && WritePos < CompileOption.i64_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.i64_stack_top_end_pos);

                constexpr bool is_i64_i32_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.i32_stack_top_begin_pos &&
                                                CompileOption.i64_stack_top_end_pos == CompileOption.i32_stack_top_end_pos};
                constexpr bool is_i64_f32_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                                CompileOption.i64_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool is_i64_f64_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                                CompileOption.i64_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                constexpr bool is_i32_f32_i64_f64_merge{is_i64_i32_merge && is_i64_f32_merge && is_i64_f64_merge};

                using slot_t = ::std::remove_cvref_t<TypeRef...[WritePos]>;

                if constexpr(is_i32_f32_i64_f64_merge)
                {
                    static_assert(::std::same_as<slot_t, wasm_stack_top_i32_i64_f32_f64_u>);
                    typeref...[WritePos].i64 = v;
                }
                else if constexpr(is_i64_i32_merge)
                {
                    static_assert(::std::same_as<slot_t, wasm_stack_top_i32_with_i64_u>);
                    typeref...[WritePos].i64 = v;
                }
                else
                {
                    static_assert(!(is_i64_f32_merge || is_i64_f64_merge));
                    static_assert(::std::same_as<slot_t, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>);
                    typeref...[WritePos] = v;
                }
            }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>)
            {
                constexpr bool has_stack_top{CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos};
                static_assert(has_stack_top);
                static_assert(CompileOption.f32_stack_top_begin_pos <= WritePos && WritePos < CompileOption.f32_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.f32_stack_top_end_pos);

                constexpr bool is_f32_i32_merge{CompileOption.f32_stack_top_begin_pos == CompileOption.i32_stack_top_begin_pos &&
                                                CompileOption.f32_stack_top_end_pos == CompileOption.i32_stack_top_end_pos};
                constexpr bool is_f32_i64_merge{CompileOption.f32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                                CompileOption.f32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos};
                constexpr bool is_f32_f64_merge{CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                                CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                constexpr bool is_f32_v128_merge{CompileOption.f32_stack_top_begin_pos == CompileOption.v128_stack_top_begin_pos &&
                                                 CompileOption.f32_stack_top_end_pos == CompileOption.v128_stack_top_end_pos};
                constexpr bool is_i32_f32_i64_f64_merge{is_f32_i32_merge && is_f32_i64_merge && is_f32_f64_merge};
                constexpr bool is_f32_f64_v128_merge{is_f32_f64_merge && is_f32_v128_merge};

                using slot_t = ::std::remove_cvref_t<TypeRef...[WritePos]>;

                if constexpr(is_f32_f64_v128_merge)
                {
                    static_assert(::std::same_as<slot_t, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>);
                    typeref...[WritePos] = make_v128_slot_low_from_scalar(v);
                }
                else if constexpr(is_i32_f32_i64_f64_merge)
                {
                    static_assert(::std::same_as<slot_t, wasm_stack_top_i32_i64_f32_f64_u>);
                    typeref...[WritePos].f32 = v;
                }
                else if constexpr(is_f32_f64_merge)
                {
                    static_assert(::std::same_as<slot_t, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>);
                    typeref...[WritePos] = make_f64_slot_low_from_f32(v);
                }
                else if constexpr(is_f32_i32_merge)
                {
                    static_assert(::std::same_as<slot_t, wasm_stack_top_i32_with_f32_u>);
                    typeref...[WritePos].f32 = v;
                }
                else
                {
                    static_assert(!is_f32_i64_merge);
                    static_assert(::std::same_as<slot_t, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>);
                    typeref...[WritePos] = v;
                }
            }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>)
            {
                constexpr bool has_stack_top{CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos};
                static_assert(has_stack_top);
                static_assert(CompileOption.f64_stack_top_begin_pos <= WritePos && WritePos < CompileOption.f64_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.f64_stack_top_end_pos);

                constexpr bool is_f64_i32_merge{CompileOption.f64_stack_top_begin_pos == CompileOption.i32_stack_top_begin_pos &&
                                                CompileOption.f64_stack_top_end_pos == CompileOption.i32_stack_top_end_pos};
                constexpr bool is_f64_i64_merge{CompileOption.f64_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                                CompileOption.f64_stack_top_end_pos == CompileOption.i64_stack_top_end_pos};
                constexpr bool is_f64_f32_merge{CompileOption.f64_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                                CompileOption.f64_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool is_f64_v128_merge{CompileOption.f64_stack_top_begin_pos == CompileOption.v128_stack_top_begin_pos &&
                                                 CompileOption.f64_stack_top_end_pos == CompileOption.v128_stack_top_end_pos};
                constexpr bool is_i32_f32_i64_f64_merge{is_f64_i32_merge && is_f64_i64_merge && is_f64_f32_merge};
                constexpr bool is_f32_f64_v128_merge{is_f64_f32_merge && is_f64_v128_merge};

                using slot_t = ::std::remove_cvref_t<TypeRef...[WritePos]>;

                if constexpr(is_f32_f64_v128_merge)
                {
                    static_assert(::std::same_as<slot_t, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>);
                    typeref...[WritePos] = make_v128_slot_low_from_scalar(v);
                }
                else if constexpr(is_i32_f32_i64_f64_merge)
                {
                    static_assert(::std::same_as<slot_t, wasm_stack_top_i32_i64_f32_f64_u>);
                    typeref...[WritePos].f64 = v;
                }
                else
                {
                    static_assert(!(is_f64_i32_merge || is_f64_i64_merge));
                    static_assert(::std::same_as<slot_t, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>);
                    typeref...[WritePos] = v;
                }
            }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>)
            {
                constexpr bool has_stack_top{CompileOption.v128_stack_top_begin_pos != CompileOption.v128_stack_top_end_pos};
                static_assert(has_stack_top);
                static_assert(CompileOption.v128_stack_top_begin_pos <= WritePos && WritePos < CompileOption.v128_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.v128_stack_top_end_pos);

                using slot_t = ::std::remove_cvref_t<TypeRef...[WritePos]>;
                static_assert(::std::same_as<slot_t, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>);
                typeref...[WritePos] = v;
            }
            else
            {
                static_assert(is_uwvm_interpreter_valtype_supported<ValType>());
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename ValType>
        inline consteval ::std::size_t stacktop_range_begin_pos() noexcept
        {
            if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>) { return CompileOption.i32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>) { return CompileOption.i64_stack_top_begin_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>) { return CompileOption.f32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>) { return CompileOption.f64_stack_top_begin_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>)
            {
                return CompileOption.v128_stack_top_begin_pos;
            }
            else
            {
                static_assert(is_uwvm_interpreter_valtype_supported<ValType>());
                return SIZE_MAX;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename ValType>
        inline consteval ::std::size_t stacktop_range_end_pos() noexcept
        {
            if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>) { return CompileOption.i32_stack_top_end_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>) { return CompileOption.i64_stack_top_end_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>) { return CompileOption.f32_stack_top_end_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>) { return CompileOption.f64_stack_top_end_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>)
            {
                return CompileOption.v128_stack_top_end_pos;
            }
            else
            {
                static_assert(is_uwvm_interpreter_valtype_supported<ValType>());
                return SIZE_MAX;
            }
        }

        template <typename ValType,
                  uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t WritePos,
                  ::std::size_t Remaining,
                  ::std::size_t RangeBegin,
                  ::std::size_t RangeEnd,
                  uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void spill_stacktop_desc_to_operand_stack(::std::byte*& write_ptr, TypeRef&... typeref) noexcept
        {
            static_assert(Remaining != 0uz);

            ValType const v{get_curr_val_from_operand_stack_top<CompileOption, ValType, WritePos>(typeref...)};
            write_ptr -= sizeof(ValType);
            ::std::memcpy(write_ptr, ::std::addressof(v), sizeof(ValType));

            if constexpr(Remaining > 1uz)
            {
                static_assert(RangeBegin < RangeEnd);
                static_assert(in_range(WritePos, RangeBegin, RangeEnd));
                constexpr ::std::size_t next_pos{ring_next_pos(WritePos, RangeBegin, RangeEnd)};
                spill_stacktop_desc_to_operand_stack<ValType, CompileOption, next_pos, Remaining - 1uz, RangeBegin, RangeEnd>(write_ptr, typeref...);
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t StartPos,
                  ::std::size_t Count,
                  typename ValType,
                  ::std::size_t RangeBegin,
                  ::std::size_t RangeEnd,
                  uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void spill_stacktop_range_to_operand_stack(TypeRef&... typeref) noexcept
        {
            static_assert(Count != 0uz);
            static_assert(StartPos < RangeEnd);
            static_assert(RangeBegin <= StartPos);
            static_assert(Count <= (RangeEnd - RangeBegin));

            static_assert(sizeof...(TypeRef) >= 2uz);
            using stack_ptr_t = ::std::remove_cvref_t<TypeRef...[1u]>;
            static_assert(::std::same_as<stack_ptr_t, ::std::byte*>);

            typeref...[1u] += sizeof(ValType) * Count;

            ::std::byte* write_ptr{typeref...[1u]};
            spill_stacktop_desc_to_operand_stack<ValType, CompileOption, StartPos, Count, RangeBegin, RangeEnd>(write_ptr, typeref...);

            static_assert(::std::same_as<decltype(write_ptr), ::std::byte*>);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t StartPos,
                  ::std::size_t RangeBegin,
                  ::std::size_t RangeEnd,
                  typename ValType,
                  typename... RestValType,
                  uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void spill_stacktop_desc_by_types_to_operand_stack(::std::byte*& write_ptr, TypeRef&... typeref) noexcept
        {
            static_assert(is_uwvm_interpreter_valtype_supported<ValType>());

            ValType const v{get_curr_val_from_operand_stack_top<CompileOption, ValType, StartPos>(typeref...)};
            write_ptr -= sizeof(ValType);
            ::std::memcpy(write_ptr, ::std::addressof(v), sizeof(ValType));

            if constexpr(sizeof...(RestValType) > 0uz)
            {
                static_assert(RangeBegin < RangeEnd);
                static_assert(in_range(StartPos, RangeBegin, RangeEnd));
                constexpr ::std::size_t next_pos{ring_next_pos(StartPos, RangeBegin, RangeEnd)};
                spill_stacktop_desc_by_types_to_operand_stack<CompileOption, next_pos, RangeBegin, RangeEnd, RestValType...>(write_ptr, typeref...);
            }
        }

        template <typename ValType,
                  uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t WritePos,
                  ::std::size_t Remaining,
                  ::std::size_t RangeBegin,
                  ::std::size_t RangeEnd,
                  uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void fill_stacktop_asc_from_operand_stack(::std::byte*& read_ptr, TypeRef&... typeref) noexcept
        {
            static_assert(Remaining != 0uz);

            ValType v;  // no init
            ::std::memcpy(::std::addressof(v), read_ptr, sizeof(ValType));
            read_ptr += sizeof(ValType);
            set_curr_val_to_stacktop_cache<CompileOption, ValType, WritePos>(v, typeref...);

            if constexpr(Remaining > 1uz)
            {
                static_assert(RangeBegin < RangeEnd);
                static_assert(in_range(WritePos, RangeBegin, RangeEnd));
                constexpr ::std::size_t prev_pos{ring_prev_pos(WritePos, RangeBegin, RangeEnd)};
                fill_stacktop_asc_from_operand_stack<ValType, CompileOption, prev_pos, Remaining - 1uz, RangeBegin, RangeEnd>(read_ptr, typeref...);
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t StartPos,
                  ::std::size_t Count,
                  typename ValType,
                  ::std::size_t RangeBegin,
                  ::std::size_t RangeEnd,
                  uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void operand_stack_to_stacktop_range(TypeRef&... typeref) noexcept
        {
            static_assert(Count != 0uz);
            static_assert(StartPos < RangeEnd);
            static_assert(RangeBegin <= StartPos);
            static_assert(Count <= (RangeEnd - RangeBegin));

            static_assert(sizeof...(TypeRef) >= 2uz);
            using stack_ptr_t = ::std::remove_cvref_t<TypeRef...[1u]>;
            static_assert(::std::same_as<stack_ptr_t, ::std::byte*>);

            // Operand stack memory is laid out deep->top in ascending addresses.
            // Load it in that order, and fill the ring in stack-top direction (towards StartPos).
            typeref...[1u] -= sizeof(ValType) * Count;
            ::std::byte* read_ptr{typeref...[1u]};
            constexpr ::std::size_t deepest_pos{ring_advance_next_pos<StartPos, Count - 1uz, RangeBegin, RangeEnd>()};
            fill_stacktop_asc_from_operand_stack<ValType, CompileOption, deepest_pos, Count, RangeBegin, RangeEnd>(read_ptr, typeref...);

            static_assert(::std::same_as<decltype(read_ptr), ::std::byte*>);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t StartPos,
                  ::std::size_t RangeBegin,
                  ::std::size_t RangeEnd,
                  typename ValType,
                  typename... RestValType,
                  uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void fill_stacktop_asc_by_types_from_operand_stack(::std::byte*& read_ptr, TypeRef&... typeref) noexcept
        {
            static_assert(is_uwvm_interpreter_valtype_supported<ValType>());

            if constexpr(sizeof...(RestValType) > 0uz)
            {
                static_assert(RangeBegin < RangeEnd);
                static_assert(in_range(StartPos, RangeBegin, RangeEnd));
                constexpr ::std::size_t next_pos{ring_next_pos(StartPos, RangeBegin, RangeEnd)};
                fill_stacktop_asc_by_types_from_operand_stack<CompileOption, next_pos, RangeBegin, RangeEnd, RestValType...>(read_ptr, typeref...);
            }

            ValType v;  // no init
            ::std::memcpy(::std::addressof(v), read_ptr, sizeof(ValType));
            read_ptr += sizeof(ValType);
            set_curr_val_to_stacktop_cache<CompileOption, ValType, StartPos>(v, typeref...);
        }
    }  // namespace details

    /**
     * @brief Public spill/fill APIs for the interpreter stack-top cache.
     *
     * @details
     * This layer provides stable, easy-to-call entry points (`spill_*` / `operand_stack_to_*`) which:
     * - Validate the configured stack-top rings at compile time,
     * - Select the correct underlying ring implementation (including merged/typed cases), and
     * - Emit minimal runtime code (pointer math + `memcpy`).
     *
     * Design note (multi-ring advantage):
     * - Separate rings per value category keep the hottest operand-stack values in registers/locals with
     *   predictable layout, reducing memory traffic and avoiding per-op type dispatch.
     * - Optional range merging allows trading some type separation for fewer live registers while keeping
     *   compile-time validation of the shared slot layout.
     */

    namespace manipulate
    {

        /**
         * @brief Spill (materialize) a contiguous cached stack-top segment back into the operand stack.
         *
         * @details
         * This is the "cache → memory" direction. It is typically needed when:
         * - The interpreter must expose the operand stack to generic memory operations,
         * - The cached segment is about to be overwritten, or
         * - A control-flow boundary requires a consistent memory stack state.
         *
         * Semantics:
         * - `StartPos`/`Count` refer to indices in the opfunc argument pack (stack-top cache slots).
         * - The cached segment is traversed in **ring depth direction**:
         *   `StartPos (top)`, `ring_next_pos(StartPos)`, ..., `ring_next_pos^(Count-1)(StartPos)`.
         * - Operand stack memory is laid out deep→top in ascending addresses, so the implementation
         *   adjusts `sp` once and stores values such that memory ends up in the correct order.
         *
         * Compile-time constraints:
         * - `StartPos` must belong to exactly one enabled stack-top range (i32/i64/f32/f64/v128).
         * - `Count` must not exceed the ring size for that range.
         *
         * @tparam CompileOption Interpreter translation options (stack-top ranges and merge layout).
         * @tparam StartPos      Stack-top cache slot index for the logical top.
         * @tparam Count         Number of cached slots to materialize.
         */

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StartPos, ::std::size_t Count, uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void spill_stacktop_to_operand_stack(TypeRef&... typeref) noexcept
        {
            if constexpr(Count == 0uz) { return; }

            constexpr bool i32_hit{::uwvm2::runtime::compiler::uwvm_int::optable::details::in_range(StartPos,
                                                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                                                    CompileOption.i32_stack_top_end_pos)};
            constexpr bool i64_hit{::uwvm2::runtime::compiler::uwvm_int::optable::details::in_range(StartPos,
                                                                                                    CompileOption.i64_stack_top_begin_pos,
                                                                                                    CompileOption.i64_stack_top_end_pos)};
            constexpr bool f32_hit{::uwvm2::runtime::compiler::uwvm_int::optable::details::in_range(StartPos,
                                                                                                    CompileOption.f32_stack_top_begin_pos,
                                                                                                    CompileOption.f32_stack_top_end_pos)};
            constexpr bool f64_hit{::uwvm2::runtime::compiler::uwvm_int::optable::details::in_range(StartPos,
                                                                                                    CompileOption.f64_stack_top_begin_pos,
                                                                                                    CompileOption.f64_stack_top_end_pos)};
            constexpr bool v128_hit{::uwvm2::runtime::compiler::uwvm_int::optable::details::in_range(StartPos,
                                                                                                     CompileOption.v128_stack_top_begin_pos,
                                                                                                     CompileOption.v128_stack_top_end_pos)};

            constexpr ::std::size_t hit_count{static_cast<::std::size_t>(i32_hit) + static_cast<::std::size_t>(i64_hit) + static_cast<::std::size_t>(f32_hit) +
                                              static_cast<::std::size_t>(f64_hit) + static_cast<::std::size_t>(v128_hit)};

            static_assert(hit_count == 1uz,
                          "StartPos must belong to exactly one stack-top range; if your stack-top ranges are merged, use the typed spill API overload.");

            if constexpr(i32_hit)
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::details::spill_stacktop_range_to_operand_stack<
                    CompileOption,
                    StartPos,
                    Count,
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32,
                    CompileOption.i32_stack_top_begin_pos,
                    CompileOption.i32_stack_top_end_pos>(typeref...);
            }
            else if constexpr(i64_hit)
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::details::spill_stacktop_range_to_operand_stack<
                    CompileOption,
                    StartPos,
                    Count,
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64,
                    CompileOption.i64_stack_top_begin_pos,
                    CompileOption.i64_stack_top_end_pos>(typeref...);
            }
            else if constexpr(f32_hit)
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::details::spill_stacktop_range_to_operand_stack<
                    CompileOption,
                    StartPos,
                    Count,
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32,
                    CompileOption.f32_stack_top_begin_pos,
                    CompileOption.f32_stack_top_end_pos>(typeref...);
            }
            else if constexpr(f64_hit)
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::details::spill_stacktop_range_to_operand_stack<
                    CompileOption,
                    StartPos,
                    Count,
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64,
                    CompileOption.f64_stack_top_begin_pos,
                    CompileOption.f64_stack_top_end_pos>(typeref...);
            }
            else if constexpr(v128_hit)
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::details::spill_stacktop_range_to_operand_stack<
                    CompileOption,
                    StartPos,
                    Count,
                    ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128,
                    CompileOption.v128_stack_top_begin_pos,
                    CompileOption.v128_stack_top_end_pos>(typeref...);
            }
        }

        // Typed spill: explicitly selects the value type to spill, which is required when stack-top ranges are merged.
        template <uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t StartPos,
                  ::std::size_t Count,
                  typename ValType,
                  uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void spill_stacktop_to_operand_stack(TypeRef&... typeref) noexcept
        {
            static_assert(::uwvm2::runtime::compiler::uwvm_int::optable::details::is_uwvm_interpreter_valtype_supported<ValType>());
            if constexpr(Count == 0uz) { return; }

            constexpr ::std::size_t range_begin{::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_begin_pos<CompileOption, ValType>()};
            constexpr ::std::size_t range_end{::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_end_pos<CompileOption, ValType>()};
            static_assert(::uwvm2::runtime::compiler::uwvm_int::optable::details::range_enabled(range_begin, range_end),
                          "ValType stack-top range is disabled; nothing to spill.");

            ::uwvm2::runtime::compiler::uwvm_int::optable::details::
                spill_stacktop_range_to_operand_stack<CompileOption, StartPos, Count, ValType, range_begin, range_end>(typeref...);
        }

        // Typed spill (mixed): spill a contiguous segment with an explicit per-slot value type list.
        // The type list is in ring order: StartPos (top), next_pos(StartPos), ..., next_pos^(N-1)(StartPos).
        template <uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t StartPos,
                  typename FirstValType,
                  typename... RestValType,
                  uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void spill_stacktop_to_operand_stack(TypeRef&... typeref) noexcept
        {
            static_assert(::uwvm2::runtime::compiler::uwvm_int::optable::details::is_uwvm_interpreter_valtype_supported<FirstValType>());
            static_assert((::uwvm2::runtime::compiler::uwvm_int::optable::details::is_uwvm_interpreter_valtype_supported<RestValType>() && ...));

            static_assert(sizeof...(TypeRef) >= 2uz);
            using stack_ptr_t = ::std::remove_cvref_t<TypeRef...[1u]>;
            static_assert(::std::same_as<stack_ptr_t, ::std::byte*>);

            constexpr ::std::size_t range_begin{
                ::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_begin_pos<CompileOption, FirstValType>()};
            constexpr ::std::size_t range_end{::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_end_pos<CompileOption, FirstValType>()};
            static_assert(::uwvm2::runtime::compiler::uwvm_int::optable::details::range_enabled(range_begin, range_end),
                          "FirstValType stack-top range is disabled; nothing to spill.");

            static_assert(::uwvm2::runtime::compiler::uwvm_int::optable::details::in_range(StartPos, range_begin, range_end),
                          "StartPos must be within the FirstValType stack-top range for ring-ordered mixed spill.");

            static_assert(((::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_begin_pos<CompileOption, RestValType>() == range_begin &&
                            ::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_end_pos<CompileOption, RestValType>() == range_end) &&
                           ...),
                          "All value types in a ring-ordered mixed spill must share the same stack-top range (i.e. be merged).");

            static_assert((sizeof...(RestValType) + 1uz) <= (range_end - range_begin), "Type list length exceeds ring size.");

            constexpr ::std::size_t total_size{(sizeof(FirstValType) + ... + sizeof(RestValType))};
            typeref...[1u] += total_size;

            ::std::byte* write_ptr{typeref...[1u]};
            ::uwvm2::runtime::compiler::uwvm_int::optable::details::
                spill_stacktop_desc_by_types_to_operand_stack<CompileOption, StartPos, range_begin, range_end, FirstValType, RestValType...>(write_ptr,
                                                                                                                                             typeref...);

            static_assert(::std::same_as<decltype(write_ptr), ::std::byte*>);
        }

        /**
         * @brief Fill (dematerialize) a contiguous segment from the operand stack into the stack-top cache.
         *
         * @details
         * This is the inverse of `spill_stacktop_to_operand_stack()` ("memory → cache").
         * It consumes (pops) values from the operand stack and writes them into the stack-top cache ring.
         *
         * Direction conventions:
         * - Operand stack memory is deep→top in ascending addresses.
         * - `StartPos` denotes the logical top in the cache ring.
         * - The fill reads memory in deep→top order and fills the ring **towards the logical top**
         *   (using `ring_prev_pos()`), ending at `StartPos`.
         */

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StartPos, ::std::size_t Count, uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void operand_stack_to_stacktop(TypeRef&... typeref) noexcept
        {
            if constexpr(Count == 0uz) { return; }

            constexpr bool i32_hit{::uwvm2::runtime::compiler::uwvm_int::optable::details::in_range(StartPos,
                                                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                                                    CompileOption.i32_stack_top_end_pos)};
            constexpr bool i64_hit{::uwvm2::runtime::compiler::uwvm_int::optable::details::in_range(StartPos,
                                                                                                    CompileOption.i64_stack_top_begin_pos,
                                                                                                    CompileOption.i64_stack_top_end_pos)};
            constexpr bool f32_hit{::uwvm2::runtime::compiler::uwvm_int::optable::details::in_range(StartPos,
                                                                                                    CompileOption.f32_stack_top_begin_pos,
                                                                                                    CompileOption.f32_stack_top_end_pos)};
            constexpr bool f64_hit{::uwvm2::runtime::compiler::uwvm_int::optable::details::in_range(StartPos,
                                                                                                    CompileOption.f64_stack_top_begin_pos,
                                                                                                    CompileOption.f64_stack_top_end_pos)};
            constexpr bool v128_hit{::uwvm2::runtime::compiler::uwvm_int::optable::details::in_range(StartPos,
                                                                                                     CompileOption.v128_stack_top_begin_pos,
                                                                                                     CompileOption.v128_stack_top_end_pos)};

            constexpr ::std::size_t hit_count{static_cast<::std::size_t>(i32_hit) + static_cast<::std::size_t>(i64_hit) + static_cast<::std::size_t>(f32_hit) +
                                              static_cast<::std::size_t>(f64_hit) + static_cast<::std::size_t>(v128_hit)};

            static_assert(
                hit_count == 1uz,
                "StartPos must belong to exactly one stack-top range; if your stack-top ranges are merged, use the typed operand_stack_to_stacktop overload.");

            if constexpr(i32_hit)
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::details::operand_stack_to_stacktop_range<CompileOption,
                                                                                                        StartPos,
                                                                                                        Count,
                                                                                                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32,
                                                                                                        CompileOption.i32_stack_top_begin_pos,
                                                                                                        CompileOption.i32_stack_top_end_pos>(typeref...);
            }
            else if constexpr(i64_hit)
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::details::operand_stack_to_stacktop_range<CompileOption,
                                                                                                        StartPos,
                                                                                                        Count,
                                                                                                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64,
                                                                                                        CompileOption.i64_stack_top_begin_pos,
                                                                                                        CompileOption.i64_stack_top_end_pos>(typeref...);
            }
            else if constexpr(f32_hit)
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::details::operand_stack_to_stacktop_range<CompileOption,
                                                                                                        StartPos,
                                                                                                        Count,
                                                                                                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32,
                                                                                                        CompileOption.f32_stack_top_begin_pos,
                                                                                                        CompileOption.f32_stack_top_end_pos>(typeref...);
            }
            else if constexpr(f64_hit)
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::details::operand_stack_to_stacktop_range<CompileOption,
                                                                                                        StartPos,
                                                                                                        Count,
                                                                                                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64,
                                                                                                        CompileOption.f64_stack_top_begin_pos,
                                                                                                        CompileOption.f64_stack_top_end_pos>(typeref...);
            }
            else if constexpr(v128_hit)
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::details::operand_stack_to_stacktop_range<
                    CompileOption,
                    StartPos,
                    Count,
                    ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128,
                    CompileOption.v128_stack_top_begin_pos,
                    CompileOption.v128_stack_top_end_pos>(typeref...);
            }
        }

        // Typed load: explicitly selects the value type to load, which is required when stack-top ranges are merged.
        template <uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t StartPos,
                  ::std::size_t Count,
                  typename ValType,
                  uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void operand_stack_to_stacktop(TypeRef&... typeref) noexcept
        {
            static_assert(::uwvm2::runtime::compiler::uwvm_int::optable::details::is_uwvm_interpreter_valtype_supported<ValType>());
            if constexpr(Count == 0uz) { return; }

            constexpr ::std::size_t range_begin{::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_begin_pos<CompileOption, ValType>()};
            constexpr ::std::size_t range_end{::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_end_pos<CompileOption, ValType>()};
            static_assert(::uwvm2::runtime::compiler::uwvm_int::optable::details::range_enabled(range_begin, range_end),
                          "ValType stack-top range is disabled; nothing to load.");

            ::uwvm2::runtime::compiler::uwvm_int::optable::details::
                operand_stack_to_stacktop_range<CompileOption, StartPos, Count, ValType, range_begin, range_end>(typeref...);
        }

        // Typed load (mixed): load a contiguous segment with an explicit per-slot value type list.
        // The type list is in ring order: StartPos (top), next_pos(StartPos), ..., next_pos^(N-1)(StartPos).
        template <uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t StartPos,
                  typename FirstValType,
                  typename... RestValType,
                  uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void operand_stack_to_stacktop(TypeRef&... typeref) noexcept
        {
            static_assert(::uwvm2::runtime::compiler::uwvm_int::optable::details::is_uwvm_interpreter_valtype_supported<FirstValType>());
            static_assert((::uwvm2::runtime::compiler::uwvm_int::optable::details::is_uwvm_interpreter_valtype_supported<RestValType>() && ...));

            static_assert(sizeof...(TypeRef) >= 2uz);
            using stack_ptr_t = ::std::remove_cvref_t<TypeRef...[1u]>;
            static_assert(::std::same_as<stack_ptr_t, ::std::byte*>);

            constexpr ::std::size_t range_begin{
                ::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_begin_pos<CompileOption, FirstValType>()};
            constexpr ::std::size_t range_end{::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_end_pos<CompileOption, FirstValType>()};
            static_assert(::uwvm2::runtime::compiler::uwvm_int::optable::details::range_enabled(range_begin, range_end),
                          "FirstValType stack-top range is disabled; nothing to load.");

            static_assert(::uwvm2::runtime::compiler::uwvm_int::optable::details::in_range(StartPos, range_begin, range_end),
                          "StartPos must be within the FirstValType stack-top range for ring-ordered mixed load.");

            static_assert(((::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_begin_pos<CompileOption, RestValType>() == range_begin &&
                            ::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_end_pos<CompileOption, RestValType>() == range_end) &&
                           ...),
                          "All value types in a ring-ordered mixed load must share the same stack-top range (i.e. be merged).");

            static_assert((sizeof...(RestValType) + 1uz) <= (range_end - range_begin), "Type list length exceeds ring size.");

            constexpr ::std::size_t total_size{(sizeof(FirstValType) + ... + sizeof(RestValType))};
            typeref...[1u] -= total_size;
            ::std::byte* read_ptr{typeref...[1u]};
            ::uwvm2::runtime::compiler::uwvm_int::optable::details::
                fill_stacktop_asc_by_types_from_operand_stack<CompileOption, StartPos, range_begin, range_end, FirstValType, RestValType...>(read_ptr,
                                                                                                                                             typeref...);

            static_assert(::std::same_as<decltype(read_ptr), ::std::byte*>);
        }
    }  // namespace manipulate

    /**
     * @brief Interpreter opfunc: spill stack-top cache to operand stack and tail-call the next opfunc.
     *
     * @details
     * This is a real threaded-interpreter instruction. The stack-top cache lives in the opfunc argument pack,
     * and this op materializes (`spill`) a contiguous segment to memory, advances the instruction pointer,
     * loads the next opfunc pointer, then performs a `UWVM_MUSTTAIL` tail-call.
     */

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t StartPos,
              ::std::size_t Count,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_stacktop_to_operand_stack(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        manipulate::spill_stacktop_to_operand_stack<CompileOption, StartPos, Count, Type...>(type...);

        // curr_uwvmint_op ...
        // safe
        // ^^ type...[0]

        type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        // curr_uwvmint_op next_interpreter
        // safe
        //                 ^^ type...[0]

        ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

        if constexpr(CompileOption.is_tail_call) { UWVM_MUSTTAIL return next_interpreter(type...); }
    }

    /**
     * @brief Interpreter opfunc: fill stack-top cache from operand stack and tail-call the next opfunc.
     *
     * @details
     * Symmetric to `uwvmint_stacktop_to_operand_stack`: it dematerializes (`fill`) a contiguous segment from
     * memory into the cache, advances the instruction pointer, loads the next opfunc pointer, then `musttail`
     * calls it.
     */

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t StartPos,
              ::std::size_t Count,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_operand_stack_to_stacktop(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        manipulate::operand_stack_to_stacktop<CompileOption, StartPos, Count, Type...>(type...);

        // curr_uwvmint_op ...
        // safe
        // ^^ type...[0]

        type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        // curr_uwvmint_op next_interpreter
        // safe
        //                 ^^ type...[0]

        ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

        if constexpr(CompileOption.is_tail_call) { UWVM_MUSTTAIL return next_interpreter(type...); }
    }

    namespace translate
    {
        /**
         * @brief Compile-time specialization selector for stack-top spill/fill ops.
         *
         * @details
         * The interpreter maintains runtime state:
         * - current ring position (`curr_pos`) and
         * - remaining cached slots (`remain_size`)
         *
         * This code maps that runtime state to a concrete, fully-specialized opfunc instantiation
         * `uwvmint_{stacktop_to_operand_stack|operand_stack_to_stacktop}<CompileOption, StartPos, Count, ...>`.
         *
         * The selection is implemented via `constexpr` recursion to avoid runtime switch tables and to keep the
         * generated opfunc bodies branch-free (aside from the interpreter dispatch itself).
         */

        namespace details
        {
            template <typename ValType>
            inline constexpr ::std::size_t get_currpos(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
            {
                if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>) { return curr.i32_stack_top_curr_pos; }
                else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>) { return curr.i64_stack_top_curr_pos; }
                else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>) { return curr.f32_stack_top_curr_pos; }
                else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>) { return curr.f64_stack_top_curr_pos; }
                else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>) { return curr.v128_stack_top_curr_pos; }
                else
                {
                    static_assert(::uwvm2::runtime::compiler::uwvm_int::optable::details::is_uwvm_interpreter_valtype_supported<ValType>());
                    return SIZE_MAX;
                }
            }

            template <typename ValType>
            inline constexpr ::std::size_t get_remain(uwvm_interpreter_stacktop_remain_size_t const& remain) noexcept
            {
                if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>) { return remain.i32_stack_top_remain_size; }
                else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>) { return remain.i64_stack_top_remain_size; }
                else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>) { return remain.f32_stack_top_remain_size; }
                else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>) { return remain.f64_stack_top_remain_size; }
                else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>)
                {
                    return remain.v128_stack_top_remain_size;
                }
                else
                {
                    static_assert(::uwvm2::runtime::compiler::uwvm_int::optable::details::is_uwvm_interpreter_valtype_supported<ValType>());
                    return 0uz;
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t StartPos,
                      ::std::size_t CountCurr,
                      ::std::size_t CountEnd,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_stacktop_to_operand_stack_fptr_count_impl(::std::size_t count) noexcept
            {
                static_assert(CountCurr < CountEnd);

                if(count == CountCurr) { return uwvmint_stacktop_to_operand_stack<CompileOption, StartPos, CountCurr, Type...>; }
                else
                {
                    if constexpr(CountCurr + 1uz < CountEnd)
                    {
                        return get_uwvmint_stacktop_to_operand_stack_fptr_count_impl<CompileOption, StartPos, CountCurr + 1uz, CountEnd, Type...>(count);
                    }
                    else
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t RangeBegin,
                      ::std::size_t RangeEnd,
                      ::std::size_t StartPosCurr,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_stacktop_to_operand_stack_fptr_startpos_impl(::std::size_t start_pos,
                                                                                                                         ::std::size_t count) noexcept
            {
                static_assert(RangeBegin < RangeEnd);
                static_assert(StartPosCurr < RangeEnd);
                static_assert(RangeBegin <= StartPosCurr);

                if(start_pos == StartPosCurr)
                {
                    constexpr ::std::size_t count_end{(RangeEnd - RangeBegin) + 1uz};  // [1, ring_size]
                    return get_uwvmint_stacktop_to_operand_stack_fptr_count_impl<CompileOption, StartPosCurr, 1uz, count_end, Type...>(count);
                }
                else
                {
                    if constexpr(StartPosCurr + 1uz < RangeEnd)
                    {
                        return get_uwvmint_stacktop_to_operand_stack_fptr_startpos_impl<CompileOption, RangeBegin, RangeEnd, StartPosCurr + 1uz, Type...>(
                            start_pos,
                            count);
                    }
                    else
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t StartPos,
                      ::std::size_t CountCurr,
                      ::std::size_t CountEnd,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_operand_stack_to_stacktop_fptr_count_impl(::std::size_t count) noexcept
            {
                static_assert(CountCurr < CountEnd);

                if(count == CountCurr) { return uwvmint_operand_stack_to_stacktop<CompileOption, StartPos, CountCurr, Type...>; }
                else
                {
                    if constexpr(CountCurr + 1uz < CountEnd)
                    {
                        return get_uwvmint_operand_stack_to_stacktop_fptr_count_impl<CompileOption, StartPos, CountCurr + 1uz, CountEnd, Type...>(count);
                    }
                    else
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t RangeBegin,
                      ::std::size_t RangeEnd,
                      ::std::size_t StartPosCurr,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_operand_stack_to_stacktop_fptr_startpos_impl(::std::size_t start_pos,
                                                                                                                         ::std::size_t count) noexcept
            {
                static_assert(RangeBegin < RangeEnd);
                static_assert(StartPosCurr < RangeEnd);
                static_assert(RangeBegin <= StartPosCurr);

                if(start_pos == StartPosCurr)
                {
                    constexpr ::std::size_t count_end{(RangeEnd - RangeBegin) + 1uz};  // [1, ring_size]
                    return get_uwvmint_operand_stack_to_stacktop_fptr_count_impl<CompileOption, StartPosCurr, 1uz, count_end, Type...>(count);
                }
                else
                {
                    if constexpr(StartPosCurr + 1uz < RangeEnd)
                    {
                        return get_uwvmint_operand_stack_to_stacktop_fptr_startpos_impl<CompileOption, RangeBegin, RangeEnd, StartPosCurr + 1uz, Type...>(
                            start_pos,
                            count);
                    }
                    else
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }
            }
        }  // namespace details

        template <uwvm_interpreter_translate_option_t CompileOption, typename ValType, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_stacktop_to_operand_stack_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                       uwvm_interpreter_stacktop_remain_size_t const& remain) noexcept
        {
            constexpr ::std::size_t range_begin{::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_begin_pos<CompileOption, ValType>()};
            constexpr ::std::size_t range_end{::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_end_pos<CompileOption, ValType>()};

            if constexpr(!::uwvm2::runtime::compiler::uwvm_int::optable::details::range_enabled(range_begin, range_end))
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            ::std::size_t const count{details::get_remain<ValType>(remain)};
            if(count == 0uz)
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            ::std::size_t const start_pos{details::get_currpos<ValType>(curr_stacktop)};
            if(start_pos < range_begin || start_pos >= range_end)
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            return details::get_uwvmint_stacktop_to_operand_stack_fptr_startpos_impl<CompileOption, range_begin, range_end, range_begin, Type...>(start_pos,
                                                                                                                                                  count);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename ValType, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_stacktop_to_operand_stack_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                    uwvm_interpreter_stacktop_remain_size_t const& remain,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_stacktop_to_operand_stack_fptr<CompileOption, ValType, TypeInTuple...>(curr_stacktop, remain); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename ValType, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_operand_stack_to_stacktop_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                       uwvm_interpreter_stacktop_remain_size_t const& remain) noexcept
        {
            constexpr ::std::size_t range_begin{::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_begin_pos<CompileOption, ValType>()};
            constexpr ::std::size_t range_end{::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_range_end_pos<CompileOption, ValType>()};

            if constexpr(!::uwvm2::runtime::compiler::uwvm_int::optable::details::range_enabled(range_begin, range_end))
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            ::std::size_t const count{details::get_remain<ValType>(remain)};
            if(count == 0uz)
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            ::std::size_t const start_pos{details::get_currpos<ValType>(curr_stacktop)};
            if(start_pos < range_begin || start_pos >= range_end)
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            return details::get_uwvmint_operand_stack_to_stacktop_fptr_startpos_impl<CompileOption, range_begin, range_end, range_begin, Type...>(start_pos,
                                                                                                                                                  count);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename ValType, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_operand_stack_to_stacktop_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                    uwvm_interpreter_stacktop_remain_size_t const& remain,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_operand_stack_to_stacktop_fptr<CompileOption, ValType, TypeInTuple...>(curr_stacktop, remain); }
    }  // namespace translate
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

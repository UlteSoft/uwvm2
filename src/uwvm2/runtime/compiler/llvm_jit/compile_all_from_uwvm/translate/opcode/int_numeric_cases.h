// Integer numeric operators for WebAssembly 1.0/MVP scalar i32/i64 values.
// Proposal opcodes such as SIMD or saturating/nontrapping conversions live in extended opcode spaces and must extend the
// dispatch layer plus stack-value model before being routed through this MVP numeric family.
// `validate_numeric_unary` / `validate_numeric_binary` consume the opcode byte, validate the
// current WebAssembly operand stack, and push the result type.  The inline LLVM JIT path then emits
// an IR operation with matching runtime stack-value tags.  The cases below avoid LLVM undefined
// behavior explicitly where WebAssembly defines a trap or modulo-count behavior.

// i32.clz
// Stack effect: (i32) -> (i32).  Counts leading zero bits and returns 32 for an input of zero; the
// LLVM `ctlz` intrinsic receives `is_zero_undef = false` to preserve that Wasm-defined zero case.
case wasm1_code::i32_clz:
{
    auto const event{validate_i32_numeric.template operator()<0x67u>(u8"i32.clz")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.ctz
// Stack effect: (i32) -> (i32).  Counts trailing zero bits and returns 32 for an input of zero; the
// LLVM `cttz` intrinsic is emitted with `is_zero_undef = false`.
case wasm1_code::i32_ctz:
{
    auto const event{validate_i32_numeric.template operator()<0x68u>(u8"i32.ctz")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.popcnt
// Stack effect: (i32) -> (i32).  Counts the number of one bits in the 32-bit operand using LLVM's
// target-independent population-count intrinsic.
case wasm1_code::i32_popcnt:
{
    auto const event{validate_i32_numeric.template operator()<0x69u>(u8"i32.popcnt")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.add
// Stack effect: (i32, i32) -> (i32).  WebAssembly integer addition wraps modulo 2^32; LLVM `add`
// without `nsw`/`nuw` flags has the matching two's-complement wraparound behavior.
case wasm1_code::i32_add:
{
    auto const event{validate_i32_numeric.template operator()<0x6au>(u8"i32.add")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.sub
// Stack effect: (i32, i32) -> (i32).  Subtraction is also defined modulo 2^32 and must not carry
// signed or unsigned no-overflow flags.
case wasm1_code::i32_sub:
{
    auto const event{validate_i32_numeric.template operator()<0x6bu>(u8"i32.sub")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.mul
// Stack effect: (i32, i32) -> (i32).  Multiplication wraps modulo 2^32; overflow is not a trap for
// WebAssembly integer arithmetic.
case wasm1_code::i32_mul:
{
    auto const event{validate_i32_numeric.template operator()<0x6cu>(u8"i32.mul")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.div_s
// Stack effect: (i32, i32) -> (i32).  Signed division traps on divide-by-zero and on INT_MIN / -1;
// those guards are emitted before LLVM `sdiv`, where the overflow case would otherwise be poison.
case wasm1_code::i32_div_s:
{
    auto const event{validate_i32_numeric.template operator()<0x6du>(u8"i32.div_s")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.div_u
// Stack effect: (i32, i32) -> (i32).  Unsigned division only needs WebAssembly's divide-by-zero
// trap before the LLVM `udiv` instruction.
case wasm1_code::i32_div_u:
{
    auto const event{validate_i32_numeric.template operator()<0x6eu>(u8"i32.div_u")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.rem_s
// Stack effect: (i32, i32) -> (i32).  Signed remainder traps on a zero divisor, but INT_MIN % -1 is
// defined by WebAssembly as zero; the helper emits a control-flow diamond to avoid LLVM `srem`
// poison in that special case.
case wasm1_code::i32_rem_s:
{
    auto const event{validate_i32_numeric.template operator()<0x6fu>(u8"i32.rem_s")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.rem_u
// Stack effect: (i32, i32) -> (i32).  Unsigned remainder traps on a zero divisor and otherwise maps
// directly to LLVM `urem`.
case wasm1_code::i32_rem_u:
{
    auto const event{validate_i32_numeric.template operator()<0x70u>(u8"i32.rem_u")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.and
// Stack effect: (i32, i32) -> (i32).  Bitwise conjunction is a pure lane-wise operation with no
// trapping or overflow behavior.
case wasm1_code::i32_and:
{
    auto const event{validate_i32_numeric.template operator()<0x71u>(u8"i32.and")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.or
// Stack effect: (i32, i32) -> (i32).  Bitwise inclusive-or preserves the 32-bit lane width exactly.
case wasm1_code::i32_or:
{
    auto const event{validate_i32_numeric.template operator()<0x72u>(u8"i32.or")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.xor
// Stack effect: (i32, i32) -> (i32).  Bitwise exclusive-or maps directly to LLVM `xor`.
case wasm1_code::i32_xor:
{
    auto const event{validate_i32_numeric.template operator()<0x73u>(u8"i32.xor")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.shl
// Stack effect: (i32, i32) -> (i32).  WebAssembly masks the shift count modulo 32, while LLVM
// shifts are undefined for counts >= bit width; `emit_llvm_shift_count_mask` performs that guard.
case wasm1_code::i32_shl:
{
    auto const event{validate_i32_numeric.template operator()<0x74u>(u8"i32.shl")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.shr_s
// Stack effect: (i32, i32) -> (i32).  Signed right shift is arithmetic and must sign-extend the
// high bits; the shift count is still masked modulo 32 before LLVM `ashr`.
case wasm1_code::i32_shr_s:
{
    auto const event{validate_i32_numeric.template operator()<0x75u>(u8"i32.shr_s")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.shr_u
// Stack effect: (i32, i32) -> (i32).  Unsigned right shift is logical and fills high bits with zero
// after masking the shift count modulo 32.
case wasm1_code::i32_shr_u:
{
    auto const event{validate_i32_numeric.template operator()<0x76u>(u8"i32.shr_u")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.rotl
// Stack effect: (i32, i32) -> (i32).  Rotate-left is defined modulo the lane width; the helper
// lowers it to two masked shifts plus an OR so LLVM never sees an out-of-range shift count.
case wasm1_code::i32_rotl:
{
    auto const event{validate_i32_numeric.template operator()<0x77u>(u8"i32.rotl")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i32.rotr
// Stack effect: (i32, i32) -> (i32).  Rotate-right mirrors `rotl` with the opposite shift
// directions and the same modulo-32 count semantics.
case wasm1_code::i32_rotr:
{
    auto const event{validate_i32_numeric.template operator()<0x78u>(u8"i32.rotr")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i32_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.clz
// Stack effect: (i64) -> (i64).  Counts leading zero bits and returns 64 for zero; LLVM `ctlz` is
// called with `is_zero_undef = false` to keep the zero case defined.
case wasm1_code::i64_clz:
{
    auto const event{validate_i64_numeric.template operator()<0x79u>(u8"i64.clz")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.ctz
// Stack effect: (i64) -> (i64).  Counts trailing zero bits and returns 64 for zero; the emitted
// intrinsic uses the Wasm-compatible zero-defined mode.
case wasm1_code::i64_ctz:
{
    auto const event{validate_i64_numeric.template operator()<0x7au>(u8"i64.ctz")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.popcnt
// Stack effect: (i64) -> (i64).  Counts one bits across the full 64-bit lane using LLVM `ctpop`.
case wasm1_code::i64_popcnt:
{
    auto const event{validate_i64_numeric.template operator()<0x7bu>(u8"i64.popcnt")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.add
// Stack effect: (i64, i64) -> (i64).  Addition wraps modulo 2^64 and is emitted without no-overflow
// flags.
case wasm1_code::i64_add:
{
    auto const event{validate_i64_numeric.template operator()<0x7cu>(u8"i64.add")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.sub
// Stack effect: (i64, i64) -> (i64).  Subtraction wraps modulo 2^64 just like the WebAssembly
// integer arithmetic model.
case wasm1_code::i64_sub:
{
    auto const event{validate_i64_numeric.template operator()<0x7du>(u8"i64.sub")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.mul
// Stack effect: (i64, i64) -> (i64).  Multiplication keeps the low 64 bits of the product; overflow
// is normal wraparound, not a trap.
case wasm1_code::i64_mul:
{
    auto const event{validate_i64_numeric.template operator()<0x7eu>(u8"i64.mul")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.div_s
// Stack effect: (i64, i64) -> (i64).  Signed division emits WebAssembly's divide-by-zero and
// INT64_MIN / -1 overflow traps before reaching LLVM `sdiv`.
case wasm1_code::i64_div_s:
{
    auto const event{validate_i64_numeric.template operator()<0x7fu>(u8"i64.div_s")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.div_u
// Stack effect: (i64, i64) -> (i64).  Unsigned division traps only for a zero divisor, then lowers
// to LLVM `udiv`.
case wasm1_code::i64_div_u:
{
    auto const event{validate_i64_numeric.template operator()<0x80u>(u8"i64.div_u")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.rem_s
// Stack effect: (i64, i64) -> (i64).  Signed remainder follows the same Wasm special case as i32:
// INT64_MIN % -1 yields zero rather than invoking LLVM's undefined `srem` overflow behavior.
case wasm1_code::i64_rem_s:
{
    auto const event{validate_i64_numeric.template operator()<0x81u>(u8"i64.rem_s")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.rem_u
// Stack effect: (i64, i64) -> (i64).  Unsigned remainder checks for divide-by-zero and otherwise
// emits LLVM `urem`.
case wasm1_code::i64_rem_u:
{
    auto const event{validate_i64_numeric.template operator()<0x82u>(u8"i64.rem_u")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.and
// Stack effect: (i64, i64) -> (i64).  Bitwise conjunction over the full 64-bit lane.
case wasm1_code::i64_and:
{
    auto const event{validate_i64_numeric.template operator()<0x83u>(u8"i64.and")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.or
// Stack effect: (i64, i64) -> (i64).  Bitwise inclusive-or over 64-bit operands.
case wasm1_code::i64_or:
{
    auto const event{validate_i64_numeric.template operator()<0x84u>(u8"i64.or")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.xor
// Stack effect: (i64, i64) -> (i64).  Bitwise exclusive-or over 64-bit operands.
case wasm1_code::i64_xor:
{
    auto const event{validate_i64_numeric.template operator()<0x85u>(u8"i64.xor")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.shl
// Stack effect: (i64, i64) -> (i64).  The shift count is masked modulo 64 before LLVM `shl` to
// preserve WebAssembly's defined behavior for large counts.
case wasm1_code::i64_shl:
{
    auto const event{validate_i64_numeric.template operator()<0x86u>(u8"i64.shl")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.shr_s
// Stack effect: (i64, i64) -> (i64).  Arithmetic right shift sign-extends the high bits and masks
// the shift count modulo 64.
case wasm1_code::i64_shr_s:
{
    auto const event{validate_i64_numeric.template operator()<0x87u>(u8"i64.shr_s")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.shr_u
// Stack effect: (i64, i64) -> (i64).  Logical right shift zero-fills the high bits and uses the
// same modulo-64 count masking required by Wasm.
case wasm1_code::i64_shr_u:
{
    auto const event{validate_i64_numeric.template operator()<0x88u>(u8"i64.shr_u")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.rotl
// Stack effect: (i64, i64) -> (i64).  Rotate-left wraps shifted-out bits back into the low end; the
// helper emits masked shifts to avoid LLVM out-of-range shift undefined behavior.
case wasm1_code::i64_rotl:
{
    auto const event{validate_i64_numeric.template operator()<0x89u>(u8"i64.rotl")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// i64.rotr
// Stack effect: (i64, i64) -> (i64).  Rotate-right wraps shifted-out low bits into the high end and
// is lowered with the same explicit modulo-64 count handling.
case wasm1_code::i64_rotr:
{
    auto const event{validate_i64_numeric.template operator()<0x8au>(u8"i64.rotr")};
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_i64_numeric(llvm_jit_emit_state, event)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

// Scalar memory load/store cases decode and validate memarg once. Core 3
// memarg includes alignment flags, an optional selected-memory index and a u64
// offset; the selected declaration supplies the i32 or i64 address type.
// Successful typed transitions pass owned normalized DATA to the LLVM emitter,
// which uses the existing direct-memory IR or runtime bridge without rereading
// opcode or immediate bytes. memory.size/grow remain separate pending migration.

// i32.load
// Stack effect: (selected i32/i64 address) -> (i32).  Validates a 4-byte integer load with max alignment
// exponent 2; the actual little-endian read and bounds handling are emitted later.
case wasm1_code::i32_load:
{
    auto const checked_memarg{validate_mem_load(u8"i32.load", 2u, curr_operand_stack_value_type::i32)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load
// Stack effect: (selected i32/i64 address) -> (i64).  Validates an 8-byte integer load with max alignment
// exponent 3.
case wasm1_code::i64_load:
{
    auto const checked_memarg{validate_mem_load(u8"i64.load", 3u, curr_operand_stack_value_type::i64)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.load
// Stack effect: (selected i32/i64 address) -> (f32).  Validates a 4-byte floating-point load; the payload bits
// are interpreted as IEEE-754 f32 by the emit/runtime path.
case wasm1_code::f32_load:
{
    auto const checked_memarg{validate_mem_load(u8"f32.load", 2u, curr_operand_stack_value_type::f32)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f64.load
// Stack effect: (selected i32/i64 address) -> (f64).  Validates an 8-byte floating-point load from linear
// memory.
case wasm1_code::f64_load:
{
    auto const checked_memarg{validate_mem_load(u8"f64.load", 3u, curr_operand_stack_value_type::f64)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.load8_s
// Stack effect: (selected i32/i64 address) -> (i32).  Validates a one-byte load whose byte is sign-extended to
// i32 by the JIT emitter or runtime bridge.
case wasm1_code::i32_load8_s:
{
    auto const checked_memarg{validate_mem_load(u8"i32.load8_s", 0u, curr_operand_stack_value_type::i32)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.load8_u
// Stack effect: (selected i32/i64 address) -> (i32).  Validates a one-byte load whose byte is zero-extended to
// i32.
case wasm1_code::i32_load8_u:
{
    auto const checked_memarg{validate_mem_load(u8"i32.load8_u", 0u, curr_operand_stack_value_type::i32)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.load16_s
// Stack effect: (selected i32/i64 address) -> (i32).  Validates a two-byte load with sign-extension to i32.
case wasm1_code::i32_load16_s:
{
    auto const checked_memarg{validate_mem_load(u8"i32.load16_s", 1u, curr_operand_stack_value_type::i32)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.load16_u
// Stack effect: (selected i32/i64 address) -> (i32).  Validates a two-byte load with zero-extension to i32.
case wasm1_code::i32_load16_u:
{
    auto const checked_memarg{validate_mem_load(u8"i32.load16_u", 1u, curr_operand_stack_value_type::i32)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load8_s
// Stack effect: (selected i32/i64 address) -> (i64).  Validates a one-byte load with sign-extension to i64.
case wasm1_code::i64_load8_s:
{
    auto const checked_memarg{validate_mem_load(u8"i64.load8_s", 0u, curr_operand_stack_value_type::i64)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load8_u
// Stack effect: (selected i32/i64 address) -> (i64).  Validates a one-byte load with zero-extension to i64.
case wasm1_code::i64_load8_u:
{
    auto const checked_memarg{validate_mem_load(u8"i64.load8_u", 0u, curr_operand_stack_value_type::i64)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load16_s
// Stack effect: (selected i32/i64 address) -> (i64).  Validates a two-byte load with sign-extension to i64.
case wasm1_code::i64_load16_s:
{
    auto const checked_memarg{validate_mem_load(u8"i64.load16_s", 1u, curr_operand_stack_value_type::i64)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load16_u
// Stack effect: (selected i32/i64 address) -> (i64).  Validates a two-byte load with zero-extension to i64.
case wasm1_code::i64_load16_u:
{
    auto const checked_memarg{validate_mem_load(u8"i64.load16_u", 1u, curr_operand_stack_value_type::i64)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load32_s
// Stack effect: (selected i32/i64 address) -> (i64).  Validates a four-byte load with sign-extension to i64.
case wasm1_code::i64_load32_s:
{
    auto const checked_memarg{validate_mem_load(u8"i64.load32_s", 2u, curr_operand_stack_value_type::i64)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load32_u
// Stack effect: (selected i32/i64 address) -> (i64).  Validates a four-byte load with zero-extension to i64.
case wasm1_code::i64_load32_u:
{
    auto const checked_memarg{validate_mem_load(u8"i64.load32_u", 2u, curr_operand_stack_value_type::i64)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.store
// Stack effect: (i32 address, i32 value) -> ().  Validates a full-width 4-byte integer store.
case wasm1_code::i32_store:
{
    auto const checked_memarg{validate_mem_store(u8"i32.store", 2u, curr_operand_stack_value_type::i32)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.store
// Stack effect: (i32 address, i64 value) -> ().  Validates a full-width 8-byte integer store.
case wasm1_code::i64_store:
{
    auto const checked_memarg{validate_mem_store(u8"i64.store", 3u, curr_operand_stack_value_type::i64)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.store
// Stack effect: (i32 address, f32 value) -> ().  Validates a 4-byte floating-point store; the
// value's exact IEEE bit pattern is written by the emit/runtime path.
case wasm1_code::f32_store:
{
    auto const checked_memarg{validate_mem_store(u8"f32.store", 2u, curr_operand_stack_value_type::f32)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f64.store
// Stack effect: (i32 address, f64 value) -> ().  Validates an 8-byte floating-point store.
case wasm1_code::f64_store:
{
    auto const checked_memarg{validate_mem_store(u8"f64.store", 3u, curr_operand_stack_value_type::f64)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.store8
// Stack effect: (i32 address, i32 value) -> ().  Validates a one-byte store; the stored byte is the
// low 8 bits of the i32 value.
case wasm1_code::i32_store8:
{
    auto const checked_memarg{validate_mem_store(u8"i32.store8", 0u, curr_operand_stack_value_type::i32)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.store16
// Stack effect: (i32 address, i32 value) -> ().  Validates a two-byte store; the stored bytes are
// the low 16 bits of the i32 value.
case wasm1_code::i32_store16:
{
    auto const checked_memarg{validate_mem_store(u8"i32.store16", 1u, curr_operand_stack_value_type::i32)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.store8
// Stack effect: (i32 address, i64 value) -> ().  Validates a one-byte store from the low 8 bits of
// the i64 value.
case wasm1_code::i64_store8:
{
    auto const checked_memarg{validate_mem_store(u8"i64.store8", 0u, curr_operand_stack_value_type::i64)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.store16
// Stack effect: (i32 address, i64 value) -> ().  Validates a two-byte store from the low 16 bits of
// the i64 value.
case wasm1_code::i64_store16:
{
    auto const checked_memarg{validate_mem_store(u8"i64.store16", 1u, curr_operand_stack_value_type::i64)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.store32
// Stack effect: (i32 address, i64 value) -> ().  Validates a four-byte store from the low 32 bits
// of the i64 value.
case wasm1_code::i64_store32:
{
    auto const checked_memarg{validate_mem_store(u8"i64.store32", 2u, curr_operand_stack_value_type::i64)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_scalar_memory_event(
               memory_event, static_cast<unsigned>(curr_opbase), checked_memarg,
               static_cast<::std::size_t>(instruction_begin - code_begin),
               static_cast<::std::size_t>(code_curr - instruction_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_scalar_memory(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// memory.size
// The validated memory index selects the declaration and its page-count type.
case wasm1_code::memory_size:
{
    // memory.size memidx ...
    // [ safe    ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // memory.size memidx ...
    // [ safe    ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // memory.size memidx ...
    // [ safe    ] unsafe (could be the section_end)
    //             ^^ code_curr

    // [memory.size] memidx ... (code_end); the scanner bounds-checks the entire immediate.
    auto const memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(
        code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
    // [memory.size memidx] ... unsafe (could be code_end)
    //                      ^^ code_curr
    ::uwvm2::validation::standard::wasm3::validate_memory_index(memory_index, all_memory_count, op_begin, err);
    auto const address_type{memory_address_type_at(memory_index) == ::uwvm2::validation::standard::wasm3::storage_address_type::i64 ?
        runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32};

    validate_checked_memory_page(op_begin, memory_address_type_at(memory_index), false);

    operand_stack_push(address_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_memory_page_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_memory_page_event(
               memory_event, static_cast<unsigned>(curr_opbase), memory_index,
               address_type == runtime_operand_stack_value_type::i64 ?
                   ::uwvm2::validation::standard::wasm3::storage_address_type::i64 :
                   ::uwvm2::validation::standard::wasm3::storage_address_type::i32,
               static_cast<::std::size_t>(op_begin - code_begin),
               static_cast<::std::size_t>(code_curr - op_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_memory_page(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// memory.grow
// Delta and result use the selected memory address type; failure is the all-ones value.
case wasm1_code::memory_grow:
{
    // memory.grow memidx ...
    // [ safe    ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // memory.grow memidx ...
    // [ safe    ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // memory.grow memidx ...
    // [ safe    ] unsafe (could be the section_end)
    //             ^^ code_curr

    // [memory.grow] memidx ... (code_end); the scanner bounds-checks the entire immediate.
    auto const memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(
        code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
    // [memory.grow memidx] ... unsafe (could be code_end)
    //                      ^^ code_curr
    ::uwvm2::validation::standard::wasm3::validate_memory_index(memory_index, all_memory_count, op_begin, err);
    auto const address_type{memory_address_type_at(memory_index) == ::uwvm2::validation::standard::wasm3::storage_address_type::i64 ?
        runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32};

    validate_checked_memory_page(op_begin, memory_address_type_at(memory_index), true);

    operand_stack_push(address_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        ::uwvm2::validation::standard::wasm3::validated_memory_page_event memory_event{};
        // [original checked opcode ... code_curr] | code_end
        // [safe same expression allocation    ] | one-past; offsets only, no source reread.
        if(!::uwvm2::validation::standard::wasm3::complete_memory_page_event(
               memory_event, static_cast<unsigned>(curr_opbase), memory_index,
               address_type == runtime_operand_stack_value_type::i64 ?
                   ::uwvm2::validation::standard::wasm3::storage_address_type::i64 :
                   ::uwvm2::validation::standard::wasm3::storage_address_type::i32,
               static_cast<::std::size_t>(op_begin - code_begin),
               static_cast<::std::size_t>(code_curr - op_begin), control_flow_stack.size(), !is_polymorphic) ||
           !try_emit_runtime_local_func_llvm_jit_memory_page(llvm_jit_emit_state, memory_event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

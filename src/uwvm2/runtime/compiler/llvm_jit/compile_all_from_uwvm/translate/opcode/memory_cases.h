case wasm1_code::i32_load:
{
    validate_mem_load(u8"i32.load", 2u, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i64_load:
{
    validate_mem_load(u8"i64.load", 3u, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::f32_load:
{
    validate_mem_load(u8"f32.load", 2u, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f64_load:
{
    validate_mem_load(u8"f64.load", 3u, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::i32_load8_s:
{
    validate_mem_load(u8"i32.load8_s", 0u, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_load8_u:
{
    validate_mem_load(u8"i32.load8_u", 0u, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_load16_s:
{
    validate_mem_load(u8"i32.load16_s", 1u, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_load16_u:
{
    validate_mem_load(u8"i32.load16_u", 1u, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i64_load8_s:
{
    validate_mem_load(u8"i64.load8_s", 0u, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_load8_u:
{
    validate_mem_load(u8"i64.load8_u", 0u, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_load16_s:
{
    validate_mem_load(u8"i64.load16_s", 1u, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_load16_u:
{
    validate_mem_load(u8"i64.load16_u", 1u, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_load32_s:
{
    validate_mem_load(u8"i64.load32_s", 2u, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_load32_u:
{
    validate_mem_load(u8"i64.load32_u", 2u, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i32_store:
{
    validate_mem_store(u8"i32.store", 2u, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i64_store:
{
    validate_mem_store(u8"i64.store", 3u, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::f32_store:
{
    validate_mem_store(u8"f32.store", 2u, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f64_store:
{
    validate_mem_store(u8"f64.store", 3u, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::i32_store8:
{
    validate_mem_store(u8"i32.store8", 0u, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_store16:
{
    validate_mem_store(u8"i32.store16", 1u, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i64_store8:
{
    validate_mem_store(u8"i64.store8", 0u, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_store16:
{
    validate_mem_store(u8"i64.store16", 1u, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_store32:
{
    validate_mem_store(u8"i64.store32", 2u, curr_operand_stack_value_type::i64);
    break;
}
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

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memidx;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    auto const [mem_next, mem_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(memidx))};
    if(mem_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_memory_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(mem_err);
    }

    // memory.size memidx ...
    // [ safe           ] unsafe (could be the section_end)
    //             ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(mem_next);

    // memory.size memidx ...
    // [ safe           ] unsafe (could be the section_end)
    //                    ^^ code_curr

    // MVP: only memory 0 exists and the encoding must be memidx=0 (reserved byte 0x00).
    if(memidx != 0u) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_memory_index.memory_index = memidx;
        err.err_selectable.illegal_memory_index.all_memory_count = all_memory_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_memory_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(all_memory_count == 0u) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.no_memory.op_code_name = u8"memory.size";
        err.err_selectable.no_memory.align = 0u;
        err.err_selectable.no_memory.offset = 0u;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::no_memory;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Stack effect: () -> (i32)
    operand_stack_push(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32);

    break;
}
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

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memidx;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    auto const [mem_next, mem_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(memidx))};
    if(mem_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_memory_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(mem_err);
    }

    // memory.grow memidx ...
    // [        safe    ] unsafe (could be the section_end)
    //             ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(mem_next);

    // memory.grow memidx ...
    // [        safe    ] unsafe (could be the section_end)
    //                    ^^ code_curr

    // MVP: only memory 0 exists and the encoding must be memidx=0 (reserved byte 0x00).
    if(memidx != 0u) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_memory_index.memory_index = memidx;
        err.err_selectable.illegal_memory_index.all_memory_count = all_memory_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_memory_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(all_memory_count == 0u) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.no_memory.op_code_name = u8"memory.grow";
        err.err_selectable.no_memory.align = 0u;
        err.err_selectable.no_memory.offset = 0u;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::no_memory;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Stack effect: (i32 delta_pages) -> (i32 previous_pages_or_minus1)
    if(!is_polymorphic)
    {
        if(operand_stack.empty()) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.operand_stack_underflow.op_code_name = u8"memory.grow";
            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::operand_stack_underflow;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        auto const delta{operand_stack_pop_unchecked()};

        if(delta.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.memory_grow_delta_type_not_i32.delta_type = delta.type;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::memory_grow_delta_type_not_i32;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
    else
    {
        if(!operand_stack.empty()) { static_cast<void>(operand_stack_pop_unchecked()); }
    }

    operand_stack_push(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32);

    break;
}

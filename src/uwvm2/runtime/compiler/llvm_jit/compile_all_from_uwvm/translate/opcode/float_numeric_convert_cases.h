case wasm1_code::f32_abs:
{
    validate_numeric_unary(u8"f32.abs", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_neg:
{
    validate_numeric_unary(u8"f32.neg", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_ceil:
{
    validate_numeric_unary(u8"f32.ceil", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_floor:
{
    validate_numeric_unary(u8"f32.floor", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_trunc:
{
    validate_numeric_unary(u8"f32.trunc", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_nearest:
{
    validate_numeric_unary(u8"f32.nearest", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_sqrt:
{
    validate_numeric_unary(u8"f32.sqrt", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_add:
{
    validate_numeric_binary(u8"f32.add", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_sub:
{
    validate_numeric_binary(u8"f32.sub", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_mul:
{
    validate_numeric_binary(u8"f32.mul", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_div:
{
    validate_numeric_binary(u8"f32.div", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_min:
{
    validate_numeric_binary(u8"f32.min", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_max:
{
    validate_numeric_binary(u8"f32.max", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_copysign:
{
    validate_numeric_binary(u8"f32.copysign", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f64_abs:
{
    validate_numeric_unary(u8"f64.abs", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_neg:
{
    validate_numeric_unary(u8"f64.neg", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_ceil:
{
    validate_numeric_unary(u8"f64.ceil", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_floor:
{
    validate_numeric_unary(u8"f64.floor", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_trunc:
{
    validate_numeric_unary(u8"f64.trunc", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_nearest:
{
    validate_numeric_unary(u8"f64.nearest", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_sqrt:
{
    validate_numeric_unary(u8"f64.sqrt", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_add:
{
    validate_numeric_binary(u8"f64.add", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_sub:
{
    validate_numeric_binary(u8"f64.sub", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_mul:
{
    validate_numeric_binary(u8"f64.mul", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_div:
{
    validate_numeric_binary(u8"f64.div", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_min:
{
    validate_numeric_binary(u8"f64.min", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_max:
{
    validate_numeric_binary(u8"f64.max", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_copysign:
{
    validate_numeric_binary(u8"f64.copysign", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::i32_wrap_i64:
{
    validate_numeric_unary(u8"i32.wrap_i64", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_trunc_f32_s:
{
    validate_numeric_unary(u8"i32.trunc_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_trunc_f32_u:
{
    validate_numeric_unary(u8"i32.trunc_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_trunc_f64_s:
{
    validate_numeric_unary(u8"i32.trunc_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_trunc_f64_u:
{
    validate_numeric_unary(u8"i32.trunc_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i64_extend_i32_s:
{
    validate_numeric_unary(u8"i64.extend_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_extend_i32_u:
{
    validate_numeric_unary(u8"i64.extend_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_trunc_f32_s:
{
    validate_numeric_unary(u8"i64.trunc_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_trunc_f32_u:
{
    validate_numeric_unary(u8"i64.trunc_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_trunc_f64_s:
{
    validate_numeric_unary(u8"i64.trunc_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_trunc_f64_u:
{
    validate_numeric_unary(u8"i64.trunc_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::f32_convert_i32_s:
{
    validate_numeric_unary(u8"f32.convert_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_convert_i32_u:
{
    validate_numeric_unary(u8"f32.convert_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_convert_i64_s:
{
    validate_numeric_unary(u8"f32.convert_i64_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_convert_i64_u:
{
    validate_numeric_unary(u8"f32.convert_i64_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f32_demote_f64:
{
    validate_numeric_unary(u8"f32.demote_f64", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f64_convert_i32_s:
{
    validate_numeric_unary(u8"f64.convert_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_convert_i32_u:
{
    validate_numeric_unary(u8"f64.convert_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_convert_i64_s:
{
    validate_numeric_unary(u8"f64.convert_i64_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_convert_i64_u:
{
    validate_numeric_unary(u8"f64.convert_i64_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::f64_promote_f32:
{
    validate_numeric_unary(u8"f64.promote_f32", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f64);
    break;
}
case wasm1_code::i32_reinterpret_f32:
{
    validate_numeric_unary(u8"i32.reinterpret_f32", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i64_reinterpret_f64:
{
    validate_numeric_unary(u8"i64.reinterpret_f64", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::f32_reinterpret_i32:
{
    validate_numeric_unary(u8"f32.reinterpret_i32", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);
    break;
}
case wasm1_code::f64_reinterpret_i64:
{
    validate_numeric_unary(u8"f64.reinterpret_i64", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);
    break;
}

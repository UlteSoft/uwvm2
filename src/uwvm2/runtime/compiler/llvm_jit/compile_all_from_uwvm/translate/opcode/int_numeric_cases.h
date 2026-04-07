case wasm1_code::i32_clz:
{
    validate_numeric_unary(u8"i32.clz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_ctz:
{
    validate_numeric_unary(u8"i32.ctz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_popcnt:
{
    validate_numeric_unary(u8"i32.popcnt", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_add:
{
    validate_numeric_binary(u8"i32.add", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_sub:
{
    validate_numeric_binary(u8"i32.sub", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_mul:
{
    validate_numeric_binary(u8"i32.mul", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_div_s:
{
    validate_numeric_binary(u8"i32.div_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_div_u:
{
    validate_numeric_binary(u8"i32.div_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_rem_s:
{
    validate_numeric_binary(u8"i32.rem_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_rem_u:
{
    validate_numeric_binary(u8"i32.rem_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_and:
{
    validate_numeric_binary(u8"i32.and", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_or:
{
    validate_numeric_binary(u8"i32.or", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_xor:
{
    validate_numeric_binary(u8"i32.xor", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_shl:
{
    validate_numeric_binary(u8"i32.shl", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_shr_s:
{
    validate_numeric_binary(u8"i32.shr_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_shr_u:
{
    validate_numeric_binary(u8"i32.shr_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_rotl:
{
    validate_numeric_binary(u8"i32.rotl", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i32_rotr:
{
    validate_numeric_binary(u8"i32.rotr", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    break;
}
case wasm1_code::i64_clz:
{
    validate_numeric_unary(u8"i64.clz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_ctz:
{
    validate_numeric_unary(u8"i64.ctz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_popcnt:
{
    validate_numeric_unary(u8"i64.popcnt", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_add:
{
    validate_numeric_binary(u8"i64.add", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_sub:
{
    validate_numeric_binary(u8"i64.sub", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_mul:
{
    validate_numeric_binary(u8"i64.mul", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_div_s:
{
    validate_numeric_binary(u8"i64.div_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_div_u:
{
    validate_numeric_binary(u8"i64.div_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_rem_s:
{
    validate_numeric_binary(u8"i64.rem_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_rem_u:
{
    validate_numeric_binary(u8"i64.rem_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_and:
{
    validate_numeric_binary(u8"i64.and", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_or:
{
    validate_numeric_binary(u8"i64.or", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_xor:
{
    validate_numeric_binary(u8"i64.xor", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_shl:
{
    validate_numeric_binary(u8"i64.shl", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_shr_s:
{
    validate_numeric_binary(u8"i64.shr_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_shr_u:
{
    validate_numeric_binary(u8"i64.shr_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_rotl:
{
    validate_numeric_binary(u8"i64.rotl", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}
case wasm1_code::i64_rotr:
{
    validate_numeric_binary(u8"i64.rotr", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    break;
}

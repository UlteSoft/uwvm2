// Normalized scalar memory DATA; no opcode byte, source cursor, LEB or memarg parser.
case wasm1_code::i32_load:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_i32, 4uz>, 32u, 4uz, false>(offset,
                                                                                                      align,
                                                                                                      runtime_operand_stack_value_type::i32,
                                                                                                      ::llvm::Type::getInt32Ty(llvm_context),
                                                                                                      4uz,
                                                                                                      false);
case wasm1_code::i64_load:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_i64, 8uz>, 64u, 8uz, false>(offset,
                                                                                                      align,
                                                                                                      runtime_operand_stack_value_type::i64,
                                                                                                      ::llvm::Type::getInt64Ty(llvm_context),
                                                                                                      8uz,
                                                                                                      false);
case wasm1_code::f32_load:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_f32, 4uz>, 32u, 4uz, false>(offset,
                                                                                                      align,
                                                                                                      runtime_operand_stack_value_type::f32,
                                                                                                      ::llvm::Type::getFloatTy(llvm_context),
                                                                                                      4uz,
                                                                                                      false);
case wasm1_code::f64_load:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_f64, 8uz>, 64u, 8uz, false>(offset,
                                                                                                      align,
                                                                                                      runtime_operand_stack_value_type::f64,
                                                                                                      ::llvm::Type::getDoubleTy(llvm_context),
                                                                                                      8uz,
                                                                                                      false);
case wasm1_code::i32_load8_s:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_i32, 1uz, true>, 32u, 1uz, true>(offset,
                                                                                                            align,
                                                                                                            runtime_operand_stack_value_type::i32,
                                                                                                            ::llvm::Type::getInt32Ty(llvm_context),
                                                                                                            1uz,
                                                                                                            true);
case wasm1_code::i32_load8_u:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_i32, 1uz, false>, 32u, 1uz, false>(offset,
                                                                                                             align,
                                                                                                             runtime_operand_stack_value_type::i32,
                                                                                                             ::llvm::Type::getInt32Ty(llvm_context),
                                                                                                             1uz,
                                                                                                             false);
case wasm1_code::i32_load16_s:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_i32, 2uz, true>, 32u, 2uz, true>(offset,
                                                                                                            align,
                                                                                                            runtime_operand_stack_value_type::i32,
                                                                                                            ::llvm::Type::getInt32Ty(llvm_context),
                                                                                                            2uz,
                                                                                                            true);
case wasm1_code::i32_load16_u:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_i32, 2uz, false>, 32u, 2uz, false>(offset,
                                                                                                             align,
                                                                                                             runtime_operand_stack_value_type::i32,
                                                                                                             ::llvm::Type::getInt32Ty(llvm_context),
                                                                                                             2uz,
                                                                                                             false);
case wasm1_code::i64_load8_s:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_i64, 1uz, true>, 64u, 1uz, true>(offset,
                                                                                                            align,
                                                                                                            runtime_operand_stack_value_type::i64,
                                                                                                            ::llvm::Type::getInt64Ty(llvm_context),
                                                                                                            1uz,
                                                                                                            true);
case wasm1_code::i64_load8_u:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_i64, 1uz, false>, 64u, 1uz, false>(offset,
                                                                                                             align,
                                                                                                             runtime_operand_stack_value_type::i64,
                                                                                                             ::llvm::Type::getInt64Ty(llvm_context),
                                                                                                             1uz,
                                                                                                             false);
case wasm1_code::i64_load16_s:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_i64, 2uz, true>, 64u, 2uz, true>(offset,
                                                                                                            align,
                                                                                                            runtime_operand_stack_value_type::i64,
                                                                                                            ::llvm::Type::getInt64Ty(llvm_context),
                                                                                                            2uz,
                                                                                                            true);
case wasm1_code::i64_load16_u:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_i64, 2uz, false>, 64u, 2uz, false>(offset,
                                                                                                             align,
                                                                                                             runtime_operand_stack_value_type::i64,
                                                                                                             ::llvm::Type::getInt64Ty(llvm_context),
                                                                                                             2uz,
                                                                                                             false);
case wasm1_code::i64_load32_s:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_i64, 4uz, true>, 64u, 4uz, true>(offset,
                                                                                                            align,
                                                                                                            runtime_operand_stack_value_type::i64,
                                                                                                            ::llvm::Type::getInt64Ty(llvm_context),
                                                                                                            4uz,
                                                                                                            true);
case wasm1_code::i64_load32_u:
    return emit_memory_load_call.template operator()<llvm_jit_memory_load_bridge<runtime_wasm_i64, 4uz, false>, 64u, 4uz, false>(offset,
                                                                                                             align,
                                                                                                             runtime_operand_stack_value_type::i64,
                                                                                                             ::llvm::Type::getInt64Ty(llvm_context),
                                                                                                             4uz,
                                                                                                             false);
case wasm1_code::i32_store:
    return emit_memory_store_call.template operator()<llvm_jit_memory_store_bridge<runtime_wasm_i32, 4uz>, 32u, 4uz, false>(offset,
                                                                                                        align,
                                                                                                        runtime_operand_stack_value_type::i32,
                                                                                                        ::llvm::Type::getInt32Ty(llvm_context),
                                                                                                        4uz);
case wasm1_code::i64_store:
    return emit_memory_store_call.template operator()<llvm_jit_memory_store_bridge<runtime_wasm_i64, 8uz>, 64u, 8uz, false>(offset,
                                                                                                        align,
                                                                                                        runtime_operand_stack_value_type::i64,
                                                                                                        ::llvm::Type::getInt64Ty(llvm_context),
                                                                                                        8uz);
case wasm1_code::f32_store:
    return emit_memory_store_call.template operator()<llvm_jit_memory_store_bridge<runtime_wasm_f32, 4uz>, 32u, 4uz, false>(offset,
                                                                                                        align,
                                                                                                        runtime_operand_stack_value_type::f32,
                                                                                                        ::llvm::Type::getFloatTy(llvm_context),
                                                                                                        4uz);
case wasm1_code::f64_store:
    return emit_memory_store_call.template operator()<llvm_jit_memory_store_bridge<runtime_wasm_f64, 8uz>, 64u, 8uz, false>(offset,
                                                                                                        align,
                                                                                                        runtime_operand_stack_value_type::f64,
                                                                                                        ::llvm::Type::getDoubleTy(llvm_context),
                                                                                                        8uz);
case wasm1_code::i32_store8:
    return emit_memory_store_call.template operator()<llvm_jit_memory_store_bridge<runtime_wasm_i32, 1uz>, 32u, 1uz, false>(offset,
                                                                                                        align,
                                                                                                        runtime_operand_stack_value_type::i32,
                                                                                                        ::llvm::Type::getInt32Ty(llvm_context),
                                                                                                        1uz);
case wasm1_code::i32_store16:
    return emit_memory_store_call.template operator()<llvm_jit_memory_store_bridge<runtime_wasm_i32, 2uz>, 32u, 2uz, false>(offset,
                                                                                                        align,
                                                                                                        runtime_operand_stack_value_type::i32,
                                                                                                        ::llvm::Type::getInt32Ty(llvm_context),
                                                                                                        2uz);
case wasm1_code::i64_store8:
    return emit_memory_store_call.template operator()<llvm_jit_memory_store_bridge<runtime_wasm_i64, 1uz>, 64u, 1uz, false>(offset,
                                                                                                        align,
                                                                                                        runtime_operand_stack_value_type::i64,
                                                                                                        ::llvm::Type::getInt64Ty(llvm_context),
                                                                                                        1uz);
case wasm1_code::i64_store16:
    return emit_memory_store_call.template operator()<llvm_jit_memory_store_bridge<runtime_wasm_i64, 2uz>, 64u, 2uz, false>(offset,
                                                                                                        align,
                                                                                                        runtime_operand_stack_value_type::i64,
                                                                                                        ::llvm::Type::getInt64Ty(llvm_context),
                                                                                                        2uz);
case wasm1_code::i64_store32:
    return emit_memory_store_call.template operator()<llvm_jit_memory_store_bridge<runtime_wasm_i64, 4uz>, 64u, 4uz, false>(offset,
                                                                                                        align,
                                                                                                        runtime_operand_stack_value_type::i64,
                                                                                                        ::llvm::Type::getInt64Ty(llvm_context),
                                                                                                        4uz);

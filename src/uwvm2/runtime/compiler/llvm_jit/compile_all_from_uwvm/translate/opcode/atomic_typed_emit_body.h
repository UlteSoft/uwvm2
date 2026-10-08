// Actual original atomic LLVM IR/bridge body; the caller supplies checked DATA.
        if(instruction.descriptor.kind == atomic::atomic_instruction_kind::fence)
        {
            ir_builder.CreateFence(::llvm::AtomicOrdering::SequentiallyConsistent);
            return true;
        }
        select_memory(instruction.memory.memory_index);
        if(!ensure_selected_memory_access_info() || selected_memory_access_info.memory_p == nullptr) { return false; }
        bool const address64{selected_memory_is_address64()};
        if(!address64 && instruction.memory.offset > 0xffff'ffffull) { return false; }
        auto const address_type{address64 ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32};
        // Clang omits the specialization arguments of a function-pointer NTTP
        // in __PRETTY_FUNCTION__. Same-signature atomic bridges would otherwise
        // overwrite one another in MCJIT's global symbol registry. FE opcode is
        // the complete stable operation/width identity; no process address enters
        // this cache-visible key. Ordinary directly lowered atomics emit no bridge.
        auto const atomic_bridge_identity{::uwvm2::utils::container::u8concat_uwvm(address64 ? ::uwvm2::utils::container::u8string_view{u8"threads-fe-v3-memory64-"} :
            ::uwvm2::utils::container::u8string_view{u8"threads-fe-v2-native-scalars-"}, instruction.opcode)};
        auto const atomic_bridge_discriminator{::uwvm2::utils::container::u8string_view{
            atomic_bridge_identity.data(), atomic_bridge_identity.size()}};
        // [owned identity characters] safe for the synchronous bridge emitter.
        auto const llvm_owner_type{ir_builder.getIntNTy(sizeof(::std::uintptr_t) * 8u)};
        auto const llvm_address_carrier_type{ir_builder.getIntNTy(sizeof(llvm_jit_atomic_address_carrier_t) * 8u)};
        auto const llvm_guest_address_type{address64 ? ir_builder.getInt64Ty() : llvm_address_carrier_type};
        if(instruction.descriptor.kind == atomic::atomic_instruction_kind::notify ||
           instruction.descriptor.kind == atomic::atomic_instruction_kind::wait32 ||
           instruction.descriptor.kind == atomic::atomic_instruction_kind::wait64)
        {
            if(operand_stack.size() < instruction.descriptor.operand_count) { return false; }
            bool const waiting{instruction.opcode != 0u};
            auto const timeout{waiting ? operand_stack.back() :
                llvm_jit_stack_value_t{}};
            if(waiting)
            {
                if(timeout.type != runtime_operand_stack_value_type::i64 || timeout.value == nullptr) { return false; }
                operand_stack.pop_back();
            }
            auto const expected{operand_stack.back()};
            operand_stack.pop_back();
            if(expected.value == nullptr || expected.type != (instruction.opcode == 2u ?
                runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32)) { return false; }
            auto const address{operand_stack.back()};
            operand_stack.pop_back();
            if(address.type != address_type || address.value == nullptr) { return false; }
            auto const owner{emit_native_memory_object_address()};
            if(owner == nullptr) { return false; }
            auto const bridge_type{::llvm::FunctionType::get(llvm_address_carrier_type,
                {llvm_owner_type, llvm_guest_address_type, llvm_guest_address_type,
                 ir_builder.getInt64Ty(), ir_builder.getInt64Ty()}, false)};
            auto bridge{[&]<unsigned Operation>() noexcept -> ::llvm::CallBase*
            {
#if defined(UWVM_CPP_EXCEPTIONS) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
                if constexpr(Operation != 0u)
                {
                    if(state.emit_debug_safe_points && state.debug_activation_enabled && state.native_guest_exceptions)
                    {
                        // Cancellation is a private foreign unwind after the
                        // linked node is unregistered, never a fourth Wasm wait
                        // result. The real Invoke bypasses Wasm handlers and
                        // retires the current activation/GC/native frame.
                        auto const target{address64 ?
                            get_llvm_runtime_bridge_function_symbol_value<llvm_jit_memory64_wait_notify_bridge<Operation, true>>(
                                ir_builder, bridge_type, atomic_bridge_discriminator) :
                            get_llvm_runtime_bridge_function_symbol_value<llvm_jit_atomic_wait_notify_bridge<Operation, true>>(
                                ir_builder, bridge_type, atomic_bridge_discriminator)};
                        if(target == nullptr) { return nullptr; }
                        return apply_llvm_jit_host_calling_conv(emit_runtime_local_func_llvm_jit_may_throw_call(state,
                            bridge_type, target, {owner, ::llvm::ConstantInt::get(llvm_guest_address_type, instruction.memory.offset),
                                ir_builder.CreateZExtOrTrunc(address.value, llvm_guest_address_type),
                                ir_builder.CreateZExtOrTrunc(expected.value, ir_builder.getInt64Ty()), timeout.value}, false));
                    }
                }
#endif
                if(address64)
                {
                    return emit_runtime_bridge_call.template operator()<llvm_jit_memory64_wait_notify_bridge<Operation>>(
                        bridge_type, {owner, ::llvm::ConstantInt::get(llvm_guest_address_type, instruction.memory.offset),
                            ir_builder.CreateZExtOrTrunc(address.value, llvm_guest_address_type),
                            ir_builder.CreateZExtOrTrunc(expected.value, ir_builder.getInt64Ty()),
                            waiting ? timeout.value : ir_builder.getInt64(0)}, atomic_bridge_discriminator);
                }
                return emit_runtime_bridge_call.template operator()<llvm_jit_atomic_wait_notify_bridge<Operation>>(
                    bridge_type, {owner, ::llvm::ConstantInt::get(llvm_guest_address_type, instruction.memory.offset),
                        ir_builder.CreateZExtOrTrunc(address.value, llvm_guest_address_type),
                        ir_builder.CreateZExtOrTrunc(expected.value, ir_builder.getInt64Ty()),
                        waiting ? timeout.value : ir_builder.getInt64(0)}, atomic_bridge_discriminator);
            }};
            // SSA values and call argument lifetimes are owned by this function;
            // no raw linear-memory address is emitted or carried across the call.
            auto const result{instruction.opcode == 0u ? bridge.template operator()<0u>() :
                instruction.opcode == 1u ? bridge.template operator()<1u>() : bridge.template operator()<2u>()};
            if(result == nullptr) { return false; }
            push_operand(runtime_operand_stack_value_type::i32, ir_builder.CreateZExtOrTrunc(result, ir_builder.getInt32Ty()));
            return true;
        }
        bool const is_store{instruction.descriptor.kind == atomic::atomic_instruction_kind::store};
        bool const is_rmw{instruction.descriptor.kind >= atomic::atomic_instruction_kind::add};
        bool const is_compare{instruction.descriptor.kind == atomic::atomic_instruction_kind::compare_exchange};
        if((!is_store && !is_rmw && instruction.descriptor.kind != atomic::atomic_instruction_kind::load) ||
           operand_stack.size() < instruction.descriptor.operand_count) { return false; }
        ::llvm::Value* store_value{};
        ::llvm::Value* expected_value{};
        if(is_store || is_rmw)
        {
            auto const value{operand_stack.back()};
            operand_stack.pop_back();
            auto const expected{instruction.descriptor.value_i64 ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32};
            if(value.type != expected || value.value == nullptr) { return false; }
            store_value = value.value;
            // store_value borrows a checked LLVM operand owned by this function.
            if(is_compare)
            {
                auto const compared{operand_stack.back()};
                operand_stack.pop_back();
                if(compared.type != expected || compared.value == nullptr) { return false; }
                expected_value = compared.value;
                // expected_value borrows the checked same-type LLVM operand.
            }
        }
        auto const address{operand_stack.back()};
        operand_stack.pop_back();
        if(address.type != address_type || address.value == nullptr) { return false; }
        auto const bytes{::std::size_t{1} << instruction.descriptor.natural_alignment};
        auto const offset{instruction.memory.offset};
        auto const wide{instruction.descriptor.value_i64};
        auto const result_type{wide ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32};
        auto const llvm_result_type{wide ? ir_builder.getInt64Ty() : ir_builder.getInt32Ty()};
        auto const llvm_value_carrier_type{wide ? ir_builder.getInt64Ty() : llvm_address_carrier_type};
        if(!emit_llvm_jit_atomic_alignment(ir_builder, address.value, offset, bytes)) { return false; }
        auto const pointer{emit_llvm_jit_direct_memory_pointer(state, offset, bytes, address.value, is_store || is_rmw)};
        if(is_rmw)
        {
            if(pointer != nullptr)
            {
                using rmw_operation = ::uwvm2::runtime::compiler::shared::wasm_threads::atomic_rmw_operation;
                rmw_operation operation{};
                switch(instruction.descriptor.kind)
                {
                    case atomic::atomic_instruction_kind::add: operation = rmw_operation::add; break;
                    case atomic::atomic_instruction_kind::sub: operation = rmw_operation::sub; break;
                    case atomic::atomic_instruction_kind::and_: operation = rmw_operation::and_; break;
                    case atomic::atomic_instruction_kind::or_: operation = rmw_operation::or_; break;
                    case atomic::atomic_instruction_kind::xor_: operation = rmw_operation::xor_; break;
                    case atomic::atomic_instruction_kind::exchange: operation = rmw_operation::exchange; break;
                    case atomic::atomic_instruction_kind::compare_exchange: operation = rmw_operation::compare_exchange; break;
                    default: return false;
                }
                auto const old{emit_llvm_jit_atomic_rmw(ir_builder, pointer, operation, bytes, store_value, expected_value)};
                if(old == nullptr) { return false; }
                push_operand(result_type, old);
                return true;
            }
            auto const memory_address{emit_native_memory_object_address()};
            if(memory_address == nullptr) { return false; }
            namespace shared_atomic = ::uwvm2::runtime::compiler::shared::wasm_threads;
            auto bridge_family{[&]<shared_atomic::atomic_rmw_operation Operation>() noexcept -> ::llvm::Value*
            {
                auto width{[&]<typename ValueType, ::std::size_t Bytes>() noexcept -> ::llvm::Value*
                {
                    auto const bridge_type{::llvm::FunctionType::get(llvm_value_carrier_type,
                        {llvm_owner_type, llvm_guest_address_type, llvm_guest_address_type, llvm_value_carrier_type, llvm_value_carrier_type}, false)};
                    auto const expected{is_compare ? expected_value : ::llvm::ConstantInt::get(llvm_result_type, 0u)};
                    if(address64)
                    {
                        return emit_runtime_bridge_call.template operator()<llvm_jit_atomic_rmw_bridge<Operation, ValueType, Bytes, true>>(
                            bridge_type, {memory_address, ::llvm::ConstantInt::get(llvm_guest_address_type, offset),
                                ir_builder.CreateZExtOrTrunc(address.value, llvm_guest_address_type),
                                ir_builder.CreateZExtOrTrunc(store_value, llvm_value_carrier_type),
                                ir_builder.CreateZExtOrTrunc(expected, llvm_value_carrier_type)}, atomic_bridge_discriminator);
                    }
                    return emit_runtime_bridge_call.template operator()<llvm_jit_atomic_rmw_bridge<Operation, ValueType, Bytes>>(
                        bridge_type, {memory_address, ::llvm::ConstantInt::get(llvm_guest_address_type, offset),
                            ir_builder.CreateZExtOrTrunc(address.value, llvm_guest_address_type),
                            ir_builder.CreateZExtOrTrunc(store_value, llvm_value_carrier_type),
                            ir_builder.CreateZExtOrTrunc(expected, llvm_value_carrier_type)}, atomic_bridge_discriminator);
                }};
                switch((instruction.opcode - 0x1eu) % 7u)
                {
                    case 0u: return width.template operator()<runtime_wasm_i32,4uz>();
                    case 1u: return width.template operator()<runtime_wasm_i64,8uz>();
                    case 2u: return width.template operator()<runtime_wasm_i32,1uz>();
                    case 3u: return width.template operator()<runtime_wasm_i32,2uz>();
                    case 4u: return width.template operator()<runtime_wasm_i64,1uz>();
                    case 5u: return width.template operator()<runtime_wasm_i64,2uz>();
                    case 6u: return width.template operator()<runtime_wasm_i64,4uz>();
                    default: return nullptr;
                }
            }};
            ::llvm::Value* loaded{};
            switch(instruction.descriptor.kind)
            {
                case atomic::atomic_instruction_kind::add: loaded = bridge_family.template operator()<shared_atomic::atomic_rmw_operation::add>(); break;
                case atomic::atomic_instruction_kind::sub: loaded = bridge_family.template operator()<shared_atomic::atomic_rmw_operation::sub>(); break;
                case atomic::atomic_instruction_kind::and_: loaded = bridge_family.template operator()<shared_atomic::atomic_rmw_operation::and_>(); break;
                case atomic::atomic_instruction_kind::or_: loaded = bridge_family.template operator()<shared_atomic::atomic_rmw_operation::or_>(); break;
                case atomic::atomic_instruction_kind::xor_: loaded = bridge_family.template operator()<shared_atomic::atomic_rmw_operation::xor_>(); break;
                case atomic::atomic_instruction_kind::exchange: loaded = bridge_family.template operator()<shared_atomic::atomic_rmw_operation::exchange>(); break;
                case atomic::atomic_instruction_kind::compare_exchange: loaded = bridge_family.template operator()<shared_atomic::atomic_rmw_operation::compare_exchange>(); break;
                default: return false;
            }
            // loaded is null on bridge failure, otherwise a value owned by this function.
            if(loaded == nullptr) { return false; }
            push_operand(result_type, ir_builder.CreateZExtOrTrunc(loaded, llvm_result_type));
            return true;
        }
        if(is_store)
        {
            if(pointer != nullptr)
            {
                return emit_llvm_jit_atomic_store(ir_builder, pointer, bytes, store_value);
            }
            auto const memory_address{emit_native_memory_object_address()};
            if(memory_address == nullptr) { return false; }
            auto bridge{[&]<typename ValueType, ::std::size_t Bytes>() noexcept -> bool
            {
                auto const bridge_type{::llvm::FunctionType::get(ir_builder.getVoidTy(),
                    {llvm_owner_type, llvm_guest_address_type, llvm_guest_address_type, llvm_value_carrier_type}, false)};
                if(address64)
                {
                    return emit_runtime_bridge_call.template operator()<llvm_jit_atomic_store_bridge<ValueType, Bytes, true>>(
                        bridge_type, {memory_address, ::llvm::ConstantInt::get(llvm_guest_address_type, offset),
                            ir_builder.CreateZExtOrTrunc(address.value, llvm_guest_address_type),
                            ir_builder.CreateZExtOrTrunc(store_value, llvm_value_carrier_type)}, atomic_bridge_discriminator) != nullptr;
                }
                return emit_runtime_bridge_call.template operator()<llvm_jit_atomic_store_bridge<ValueType, Bytes>>(
                    bridge_type, {memory_address, ::llvm::ConstantInt::get(llvm_guest_address_type, offset),
                        ir_builder.CreateZExtOrTrunc(address.value, llvm_guest_address_type),
                        ir_builder.CreateZExtOrTrunc(store_value, llvm_value_carrier_type)}, atomic_bridge_discriminator) != nullptr;
            }};
            switch(instruction.opcode)
            {
                case 0x17u: return bridge.template operator()<runtime_wasm_i32,4uz>();
                case 0x18u: return bridge.template operator()<runtime_wasm_i64,8uz>();
                case 0x19u: return bridge.template operator()<runtime_wasm_i32,1uz>();
                case 0x1au: return bridge.template operator()<runtime_wasm_i32,2uz>();
                case 0x1bu: return bridge.template operator()<runtime_wasm_i64,1uz>();
                case 0x1cu: return bridge.template operator()<runtime_wasm_i64,2uz>();
                case 0x1du: return bridge.template operator()<runtime_wasm_i64,4uz>();
                default: return false;
            }
        }
        if(pointer != nullptr)
        {
            auto const value{emit_llvm_jit_atomic_load(ir_builder, pointer, bytes, llvm_result_type)};
            if(value == nullptr) { return false; }
            push_operand(result_type, value);
            return true;
        }
        auto const memory_address{emit_native_memory_object_address()};
        if(memory_address == nullptr) { return false; }
        auto bridge{[&]<typename ResultType, ::std::size_t Bytes>() noexcept -> ::llvm::Value*
        {
            auto const bridge_type{::llvm::FunctionType::get(llvm_value_carrier_type,
                {llvm_owner_type, llvm_guest_address_type, llvm_guest_address_type}, false)};
            if(address64)
            {
                return emit_runtime_bridge_call.template operator()<llvm_jit_atomic_load_bridge<ResultType, Bytes, true>>(
                    bridge_type, {memory_address, ::llvm::ConstantInt::get(llvm_guest_address_type, offset),
                        ir_builder.CreateZExtOrTrunc(address.value, llvm_guest_address_type)}, atomic_bridge_discriminator);
            }
            return emit_runtime_bridge_call.template operator()<llvm_jit_atomic_load_bridge<ResultType, Bytes>>(
                bridge_type, {memory_address, ::llvm::ConstantInt::get(llvm_guest_address_type, offset),
                    ir_builder.CreateZExtOrTrunc(address.value, llvm_guest_address_type)}, atomic_bridge_discriminator);
        }};
        ::llvm::Value* loaded{};
        switch(instruction.opcode)
        {
            case 0x10u: loaded = bridge.template operator()<runtime_wasm_i32,4uz>(); break;
            case 0x11u: loaded = bridge.template operator()<runtime_wasm_i64,8uz>(); break;
            case 0x12u: loaded = bridge.template operator()<runtime_wasm_i32,1uz>(); break;
            case 0x13u: loaded = bridge.template operator()<runtime_wasm_i32,2uz>(); break;
            case 0x14u: loaded = bridge.template operator()<runtime_wasm_i64,1uz>(); break;
            case 0x15u: loaded = bridge.template operator()<runtime_wasm_i64,2uz>(); break;
            case 0x16u: loaded = bridge.template operator()<runtime_wasm_i64,4uz>(); break;
            default: return false;
        }
        if(loaded == nullptr) { return false; }
        push_operand(result_type, ir_builder.CreateZExtOrTrunc(loaded, llvm_result_type));
        return true;

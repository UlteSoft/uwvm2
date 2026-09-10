/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)            *
 * Licensed under the APL-2.0 License (see LICENSE file).      *
 *************************************************************/

// Internal implementation fragment, included in the runtime's anonymous namespace after the LLVM aliases and
// ensure_llvm_jit_native_target_initialized declaration. No platform register layout or logical Wasm stack is used here.
#if UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE
        struct runtime_llvm_jit_posix_unwind_probe_state
        {
            ::std::uintptr_t recursive_address{};
            ::std::uintptr_t root_address{};
            unsigned recursive_frames{};
            unsigned root_frames{};
            bool bad_order{};
        };

        inline constexpr _Unwind_Reason_Code runtime_llvm_jit_posix_unwind_probe_frame(_Unwind_Context* context, void* opaque) noexcept
        {
            auto& state{*static_cast<runtime_llvm_jit_posix_unwind_probe_state*>(opaque)};
            // GetRegionStart names the registered FDE's function, unlike a guessed address+4096 interval which can
            // accidentally match an unrelated generated function. Repeated regions are distinct recursive activations.
            auto const start{static_cast<::std::uintptr_t>(_Unwind_GetRegionStart(context))};
            if(start == state.recursive_address)
            {
                if(state.root_frames != 0u) { state.bad_order = true; }
                ++state.recursive_frames;
            }
            else if(start == state.root_address)
            {
                if(state.recursive_frames != 3u) { state.bad_order = true; }
                ++state.root_frames;
            }
            return _URC_NO_REASON;
        }

        UWVM_NOINLINE inline constexpr void runtime_llvm_jit_posix_unwind_probe_capture(void* opaque) noexcept
        {
            static_cast<void>(_Unwind_Backtrace(runtime_llvm_jit_posix_unwind_probe_frame, opaque));
            ::std::atomic_signal_fence(::std::memory_order_seq_cst);
        }

        [[nodiscard]] inline constexpr bool runtime_llvm_jit_posix_live_unwind_probe() noexcept
        {
            if(!ensure_llvm_jit_native_target_initialized()) [[unlikely]] { return false; }
            namespace emit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            ::llvm::LLVMContext context{};
            auto module{::uwvm2::utils::container::make_delete_owned<::llvm::Module>("uwvm2_posix_unwind_probe", context)};
            ::llvm::EngineBuilder target_builder{};
            target_builder.setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None);
            ::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine> target_machine{target_builder.selectTarget()};
            if(target_machine == nullptr) [[unlikely]] { return false; }
            set_llvm_module_target_triple_from_machine(*module, *target_machine);
            module->setDataLayout(target_machine->createDataLayout());

            auto const void_type{::llvm::Type::getVoidTy(context)};
            auto const ptr_type{::llvm::PointerType::getUnqual(context)};
            auto const i32_type{::llvm::Type::getInt32Ty(context)};
            auto const root_type{::llvm::FunctionType::get(void_type, {ptr_type}, false)};
            auto const recursive_type{::llvm::FunctionType::get(void_type, {ptr_type, i32_type}, false)};
            auto const capture{::llvm::Function::Create(root_type, ::llvm::GlobalValue::ExternalLinkage, "uwvm2_posix_unwind_capture", *module)};
            auto const recursive{::llvm::Function::Create(recursive_type, ::llvm::GlobalValue::ExternalLinkage, "uwvm2_posix_unwind_recursive", *module)};
            auto const root{::llvm::Function::Create(root_type, ::llvm::GlobalValue::ExternalLinkage, "uwvm2_posix_unwind_root", *module)};
            for(auto function: {recursive, root})
            {
                emit::apply_llvm_jit_common_function_attrs(*function);
                emit::apply_llvm_jit_unwind_call_stack_function_attrs(*function);
            }

            auto const entry{::llvm::BasicBlock::Create(context, "entry", recursive)};
            auto const leaf{::llvm::BasicBlock::Create(context, "leaf", recursive)};
            auto const recurse{::llvm::BasicBlock::Create(context, "recurse", recursive)};
            ::llvm::IRBuilder<> builder{entry};
            builder.CreateCondBr(builder.CreateICmpEQ(recursive->getArg(1), builder.getInt32(0)), leaf, recurse);
            builder.SetInsertPoint(leaf);
            builder.CreateCall(capture, {recursive->getArg(0)});
            builder.CreateRetVoid();
            builder.SetInsertPoint(recurse);
            builder.CreateCall(recursive, {recursive->getArg(0), builder.CreateSub(recursive->getArg(1), builder.getInt32(1))});
            builder.CreateRetVoid();
            builder.SetInsertPoint(::llvm::BasicBlock::Create(context, "entry", root));
            builder.CreateCall(recursive, {root->getArg(0), builder.getInt32(2)});
            builder.CreateRetVoid();
            if(::llvm::verifyModule(*module)) [[unlikely]] { return false; }

            ::llvm::sys::DynamicLibrary::AddSymbol("uwvm2_posix_unwind_capture",
                reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(&runtime_llvm_jit_posix_unwind_probe_capture)));
            auto memory_manager{
                ::uwvm2::utils::container::make_delete_owned<::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()};
            auto const memory_manager_observer{memory_manager.get()};
            auto const raw_engine{::llvm::EngineBuilder(llvm_module_owner_t{module.release()})
                .setEngineKind(::llvm::EngineKind::JIT)
                .setOptLevel(::llvm::CodeGenOptLevel::None)
                .setMCJITMemoryManager(llvm_jit_memory_manager_owner_t{memory_manager.release()})
                .create(target_machine.release())};
            if(raw_engine == nullptr) [[unlikely]] { return false; }
            ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> engine{raw_engine};
            engine->finalizeObject();
            if(memory_manager_observer->has_finalization_failure()) [[unlikely]] { return false; }
            runtime_llvm_jit_posix_unwind_probe_state state{};
            state.recursive_address = reinterpret_cast<::std::uintptr_t>(engine->getPointerToFunction(recursive));
            state.root_address = reinterpret_cast<::std::uintptr_t>(engine->getPointerToFunction(root));
            if(state.recursive_address == 0u || state.root_address == 0u) [[unlikely]] { return false; }
            using root_func_t = void (*)(void*) noexcept;
            reinterpret_cast<root_func_t>(state.root_address)(::std::addressof(state));
            return !state.bad_order && state.recursive_frames == 3u && state.root_frames == 1u;
        }
#endif

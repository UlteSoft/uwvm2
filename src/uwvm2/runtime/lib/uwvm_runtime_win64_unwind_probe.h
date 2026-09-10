/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

// Internal implementation fragment, included in the runtime's anonymous namespace after the LLVM aliases, Win64
// CONTEXT accessors, and ensure_llvm_jit_native_target_initialized declaration.
#if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        struct runtime_llvm_jit_win64_unwind_probe_state
        {
            ::std::uintptr_t recursive_address{};
            ::std::uintptr_t root_address{};
            unsigned recursive_frames{};
            unsigned root_frames{};
            bool capture_called{};
            bool escaped_generated_code{};
            bool failed{};
        };

        [[nodiscard]] inline constexpr bool runtime_llvm_jit_win64_probe_unwind_exact(win64_context_t& context,
                                                                                      ::std::uintptr_t expected_function) noexcept
        {
            auto const control_pc{llvm_jit_win64_context_control_pc(context)};
            if(control_pc == 0u || expected_function == 0u) [[unlikely]] { return false; }

            ::fast_io::win32::win_current_unwind_address image_base{};
            auto const function_entry{::fast_io::win32::nt::RtlLookupFunctionEntry(
                static_cast<::fast_io::win32::win_current_unwind_address>(control_pc), ::std::addressof(image_base), nullptr)};
            // A null lookup is the documented leaf fallback. This probe deliberately rejects it: checked unwind mode is
            // enabled only after Windows has found the dynamic table for every generated activation.
            if(function_entry == nullptr) [[unlikely]] { return false; }

            auto const base{static_cast<::std::uintptr_t>(image_base)};
            auto const begin_rva{static_cast<::std::uintptr_t>(function_entry->BeginAddress)};
            if(base > (::std::numeric_limits<::std::uintptr_t>::max)() - begin_rva) [[unlikely]] { return false; }
            if(base + begin_rva != expected_function) [[unlikely]] { return false; }

            void* handler_data{};
            ::fast_io::win32::win_current_unwind_address establisher_frame{};
            static_cast<void>(::fast_io::win32::nt::RtlVirtualUnwind(
                ::fast_io::win32::win64_unwind_flag_nhandler,
                image_base,
                static_cast<::fast_io::win32::win_current_unwind_address>(control_pc),
                function_entry,
                ::std::addressof(context),
                ::std::addressof(handler_data),
                ::std::addressof(establisher_frame),
                nullptr));
            return llvm_jit_win64_context_instruction_pointer(context) != 0u;
        }

        UWVM_NOINLINE inline constexpr void runtime_llvm_jit_win64_unwind_probe_capture(runtime_llvm_jit_win64_unwind_probe_state* state,
                                                                                         ::std::uintptr_t frame_address,
                                                                                         ::std::uintptr_t stack_pointer) noexcept
        {
            if(state == nullptr || state->capture_called) [[unlikely]]
            {
                if(state != nullptr) { state->failed = true; }
                return;
            }
            state->capture_called = true;

# if UWVM_HAS_BUILTIN(__builtin_return_address)
            auto const return_address{reinterpret_cast<::std::uintptr_t>(__builtin_return_address(0))};
# else
            constexpr ::std::uintptr_t return_address{};
# endif
            if(return_address == 0u || frame_address == 0u || stack_pointer == 0u ||
               !llvm_jit_frame_record_address_aligned(frame_address) || !llvm_jit_frame_record_address_aligned(stack_pointer)) [[unlikely]]
            {
                state->failed = true;
                return;
            }

            win64_context_t context{};
            llvm_jit_win64_context_set_instruction_pointer(context, return_address);
            llvm_jit_win64_context_set_stack_pointer(context, stack_pointer);
            llvm_jit_win64_context_set_frame_pointer(context, frame_address);
            llvm_jit_win64_context_initialize_link_register(context, frame_address);

            for(unsigned i{}; i != 3u; ++i)
            {
                if(!runtime_llvm_jit_win64_probe_unwind_exact(context, state->recursive_address)) [[unlikely]]
                {
                    state->failed = true;
                    return;
                }
                ++state->recursive_frames;
            }
            if(!runtime_llvm_jit_win64_probe_unwind_exact(context, state->root_address)) [[unlikely]]
            {
                state->failed = true;
                return;
            }
            ++state->root_frames;

            auto const host_control_pc{llvm_jit_win64_context_control_pc(context)};
            if(host_control_pc == 0u) [[unlikely]]
            {
                state->failed = true;
                return;
            }
            ::fast_io::win32::win_current_unwind_address host_image_base{};
            if(auto const host_entry{::fast_io::win32::nt::RtlLookupFunctionEntry(
                   static_cast<::fast_io::win32::win_current_unwind_address>(host_control_pc), ::std::addressof(host_image_base), nullptr)};
               host_entry != nullptr)
            {
                auto const base{static_cast<::std::uintptr_t>(host_image_base)};
                auto const begin_rva{static_cast<::std::uintptr_t>(host_entry->BeginAddress)};
                if(base <= (::std::numeric_limits<::std::uintptr_t>::max)() - begin_rva)
                {
                    auto const host_function{base + begin_rva};
                    if(host_function == state->recursive_address || host_function == state->root_address) [[unlikely]]
                    {
                        state->failed = true;
                        return;
                    }
                }
            }
            state->escaped_generated_code = true;
            ::std::atomic_signal_fence(::std::memory_order_seq_cst);
        }

        [[nodiscard]] inline constexpr bool runtime_llvm_jit_win64_live_unwind_probe() noexcept
        {
            if(!ensure_llvm_jit_native_target_initialized()) [[unlikely]] { return false; }
            namespace emit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;

            ::llvm::LLVMContext context{};
            auto module{::uwvm2::utils::container::make_delete_owned<::llvm::Module>("uwvm2_win64_unwind_probe", context)};
            ::llvm::EngineBuilder target_builder{};
            target_builder.setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None);
            ::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine> target_machine{target_builder.selectTarget()};
            if(target_machine == nullptr) [[unlikely]] { return false; }
            set_llvm_module_target_triple_from_machine(*module, *target_machine);
            module->setDataLayout(target_machine->createDataLayout());

            auto const void_type{::llvm::Type::getVoidTy(context)};
            auto const ptr_type{::llvm::PointerType::getUnqual(context)};
            auto const intptr_type{::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto const i32_type{::llvm::Type::getInt32Ty(context)};
            auto const root_type{::llvm::FunctionType::get(void_type, {ptr_type}, false)};
            auto const recursive_type{::llvm::FunctionType::get(void_type, {ptr_type, i32_type}, false)};
            auto const capture_type{::llvm::FunctionType::get(void_type, {ptr_type, intptr_type, intptr_type}, false)};
            auto const capture{
                ::llvm::Function::Create(capture_type, ::llvm::GlobalValue::ExternalLinkage, "uwvm2_win64_unwind_capture", *module)};
            auto const recursive{
                ::llvm::Function::Create(recursive_type, ::llvm::GlobalValue::ExternalLinkage, "uwvm2_win64_unwind_recursive", *module)};
            auto const root{::llvm::Function::Create(root_type, ::llvm::GlobalValue::ExternalLinkage, "uwvm2_win64_unwind_root", *module)};
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
# if defined(__aarch64__) || defined(_M_ARM64)
            auto const frame_register_name{"x29"};
            auto const stack_register_name{"sp"};
# else
            auto const frame_register_name{"rbp"};
            auto const stack_register_name{"rsp"};
# endif
            auto const frame_metadata{::llvm::MDNode::get(context, {::llvm::MDString::get(context, frame_register_name)})};
            auto const stack_metadata{::llvm::MDNode::get(context, {::llvm::MDString::get(context, stack_register_name)})};
            auto const frame_address{
                builder.CreateIntrinsic(::llvm::Intrinsic::read_register, {intptr_type}, {::llvm::MetadataAsValue::get(context, frame_metadata)})};
            auto const stack_pointer{
                builder.CreateIntrinsic(::llvm::Intrinsic::read_register, {intptr_type}, {::llvm::MetadataAsValue::get(context, stack_metadata)})};
            auto const capture_call{builder.CreateCall(capture, {recursive->getArg(0), frame_address, stack_pointer})};
            capture_call->setTailCallKind(::llvm::CallInst::TCK_NoTail);
            builder.CreateRetVoid();
            builder.SetInsertPoint(recurse);
            auto const recursive_call{
                builder.CreateCall(recursive, {recursive->getArg(0), builder.CreateSub(recursive->getArg(1), builder.getInt32(1))})};
            recursive_call->setTailCallKind(::llvm::CallInst::TCK_NoTail);
            builder.CreateRetVoid();
            builder.SetInsertPoint(::llvm::BasicBlock::Create(context, "entry", root));
            auto const root_call{builder.CreateCall(recursive, {root->getArg(0), builder.getInt32(2)})};
            root_call->setTailCallKind(::llvm::CallInst::TCK_NoTail);
            builder.CreateRetVoid();
            if(::llvm::verifyModule(*module)) [[unlikely]] { return false; }

            ::llvm::sys::DynamicLibrary::AddSymbol(
                "uwvm2_win64_unwind_capture",
                reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(&runtime_llvm_jit_win64_unwind_probe_capture)));
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

            runtime_llvm_jit_win64_unwind_probe_state state{};
            state.recursive_address = reinterpret_cast<::std::uintptr_t>(engine->getPointerToFunction(recursive));
            state.root_address = reinterpret_cast<::std::uintptr_t>(engine->getPointerToFunction(root));
            if(state.recursive_address == 0u || state.root_address == 0u) [[unlikely]] { return false; }
            using root_func_t = void (*)(runtime_llvm_jit_win64_unwind_probe_state*) noexcept;
            reinterpret_cast<root_func_t>(state.root_address)(::std::addressof(state));
            return state.capture_called && state.escaped_generated_code && !state.failed && state.recursive_frames == 3u && state.root_frames == 1u;
        }
#endif

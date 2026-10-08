// Included after LLVM imports and before the exported compiler namespace.
// Native compiler context only: no generated instruction reads this TLS.
#pragma once
UWVM_MODULE_EXPORT namespace uwvm2::runtime::lib
{
    extern "C++" { class runtime_checkpoint_staged_llvm_full_engine; }
}
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::initializer
{
    extern "C++" { class staged_compiler_module_owner; }
}
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details
{
    extern "C++"
    {
        class private_host_object_emission_scope final
        {
            friend class ::uwvm2::runtime::lib::runtime_checkpoint_staged_llvm_full_engine;
            using source_record = ::uwvm2::uwvm::runtime::initializer::staged_compiler_module_owner;
            inline static thread_local private_host_object_emission_scope const* current_{};
            source_record const* owner_{};
            ::llvm::Module const* module_{};
            private_host_object_emission_scope const* previous_{};
            // Only the native nonmoving factory, after real source membership,
            // can select uncached owned-address emission for its exact module.
            // The genuine record remains alive in that factory throughout this
            // lexical scope; no raw caller flag/module attribute grants entry.
            private_host_object_emission_scope(source_record const& owner, ::llvm::Module const& module) noexcept
                : owner_{::std::addressof(owner)}, module_{::std::addressof(module)}, previous_{current_}
            { current_ = this; }
            private_host_object_emission_scope(private_host_object_emission_scope const&) = delete;
            private_host_object_emission_scope& operator=(private_host_object_emission_scope const&) = delete;
            private_host_object_emission_scope(private_host_object_emission_scope&&) = delete;
            private_host_object_emission_scope& operator=(private_host_object_emission_scope&&) = delete;
            mutable bool valid_{true};
        public:
            enum class selection : unsigned { ordinary, owned_address, wrong_module };
            ~private_host_object_emission_scope() { current_ = previous_; }
            [[nodiscard]] bool valid() const noexcept { return valid_; }
            [[nodiscard]] static selection select(::llvm::Module const* module) noexcept
            {
                // Native-only same-thread lexical records. Exact LLVM module
                // identity excludes an unrelated nested compilation. A mismatch
                // rejects this attempt BEFORE the process-global symbol map.
                // No source/serialized address is dereferenced or published.
                if(current_ == nullptr) { return selection::ordinary; }
                if(current_->owner_ != nullptr && module != nullptr && current_->module_ == module)
                { return selection::owned_address; }
                current_->valid_ = false;
                return selection::wrong_module;
            }
        };
    }
}

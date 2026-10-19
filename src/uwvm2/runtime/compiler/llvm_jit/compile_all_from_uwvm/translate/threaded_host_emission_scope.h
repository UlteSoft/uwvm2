#pragma once
// Native COLD compiler scope only. It emits no ordinary guest instruction.
// This is an independent threaded owner route, not checkpoint/Restore authority.
#include <cstddef>
#include <cstdint>
#include <vector>
namespace llvm { class Module; class Function; }
namespace uwvm2::uwvm::runtime::initializer { extern "C++" { class ThreadedChildWorld; } }
namespace uwvm2::runtime::lib { extern "C++" { class ThreadedLlvmChild; } }
namespace uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details
{
    struct threaded_native_bridge final
    {
        ::llvm::Function* declaration{};
        ::std::uintptr_t original{}; // actual compiler-selected native Fn, not wire DATA
    };
    class threaded_host_emission_scope final
    {
        friend class ::uwvm2::runtime::lib::ThreadedLlvmChild;
        inline static thread_local threaded_host_emission_scope* current_{};
        ::uwvm2::uwvm::runtime::initializer::ThreadedChildWorld const* world_{};
        ::llvm::Module const* module_{};
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* wasm_module_{};
        ::std::uintptr_t original_views_{},owned_views_{};
        ::std::vector<threaded_native_bridge>* bridges_{};
        threaded_host_emission_scope* previous_{};
        bool valid_{true};
        threaded_host_emission_scope(
            ::uwvm2::uwvm::runtime::initializer::ThreadedChildWorld const& world,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& wasm_module,
            ::llvm::Module const& module, ::std::vector<threaded_native_bridge>& bridges,
            ::std::uintptr_t original_views, ::std::uintptr_t owned_views) noexcept
            : world_{::std::addressof(world)},module_{::std::addressof(module)},wasm_module_{::std::addressof(wasm_module)},original_views_{original_views},owned_views_{owned_views},bridges_{::std::addressof(bridges)},previous_{current_}
        {
            // The factory has already checked genuine owner/module membership
            // and reserved the finite bridge array before lowering can allocate.
            if(previous_!=nullptr || bridges.capacity()<256u || !bridges.empty())valid_=false;
            current_=this;
        }
        threaded_host_emission_scope(threaded_host_emission_scope const&)=delete;
        threaded_host_emission_scope& operator=(threaded_host_emission_scope const&)=delete;
    public:
        enum class selection : unsigned { ordinary, owned, rejected };
        ~threaded_host_emission_scope(){current_=previous_;}
        [[nodiscard]] bool valid() const noexcept{return valid_;}
        [[nodiscard]] static ::std::uintptr_t object_address(::std::uintptr_t original) noexcept
        {
            if(current_!=nullptr&&original!=0u&&original==current_->original_views_)
            {if(current_->owned_views_==0u){current_->valid_=false;return 0u;}return current_->owned_views_;}
            return original;
        }
        [[nodiscard]] static bool owns_module(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module) noexcept
        {
            if(current_==nullptr)return false;
            if(current_->valid_&&current_->wasm_module_==::std::addressof(module))return true;
            current_->valid_=false;return false;
        }
        [[nodiscard]] static ::std::uintptr_t owned_table_views() noexcept
        {return current_!=nullptr&&current_->valid_?current_->owned_views_:0u;}
        [[nodiscard]] static bool selected() noexcept{return current_!=nullptr;}
        [[nodiscard]] static selection select(::llvm::Module const* module) noexcept
        {
            if(current_==nullptr)return selection::ordinary;
            if(current_->valid_&&current_->world_!=nullptr&&current_->module_==module)return selection::owned;
            current_->valid_=false;return selection::rejected;
        }
        [[nodiscard]] static selection bind(::llvm::Module const* module,::llvm::Function& declaration,
            ::std::uintptr_t original) noexcept
        {
            auto const choice=select(module);if(choice!=selection::owned)return choice;
            auto& self=*current_;auto& bridges=*self.bridges_;
            if(original==0u){self.valid_=false;return selection::rejected;}
            for(auto const& entry:bridges)if(entry.declaration==::std::addressof(declaration))
            {if(entry.original==original)return selection::owned;self.valid_=false;return selection::rejected;}
            if(bridges.size()>=256u||bridges.size()==bridges.capacity())
            {self.valid_=false;return selection::rejected;}
            bridges.push_back({::std::addressof(declaration),original});return selection::owned;
        }
    };
}

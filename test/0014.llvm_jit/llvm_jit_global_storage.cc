#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

#include <array>
#include <climits>
#include <cstdint>
#include <memory>

int main()
{
    namespace llvm_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
    using value_type = llvm_details::runtime_operand_stack_value_type;
    ::llvm::LLVMContext context{};
    ::llvm::Module module{"wasm-global-storage", context};
    auto function_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context), false)};
    auto function{::llvm::Function::Create(function_type, ::llvm::Function::ExternalLinkage, "probe", module)};
    auto block{::llvm::BasicBlock::Create(context, "entry", function)};
    ::llvm::IRBuilder<> builder{block};
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t runtime_module{};
    ::std::array<::uwvm2::object::global::wasm_global_storage_t, 7u> globals{};
    constexpr value_type types[]{value_type::i32,
                                 value_type::i64,
                                 value_type::f32,
                                 value_type::f64,
                                 value_type::v128,
                                 value_type::funcref,
                                 value_type::externref};

    for(::std::uint_least32_t index{}; index != globals.size(); ++index)
    {
        auto const type{types[index]};
        auto llvm_type{llvm_details::get_llvm_type_from_wasm_value_type(context, type)};
        auto pointer{llvm_details::get_llvm_global_storage_pointer(context, builder, runtime_module, index, ::std::addressof(globals[index]), type)};
        if(llvm_type == nullptr || pointer == nullptr) { return 1; }
        if(type == value_type::v128 && !llvm_type->isIntegerTy(128u)) { return 2; }
        if((type == value_type::funcref || type == value_type::externref) &&
           !llvm_type->isIntegerTy(static_cast<unsigned>(sizeof(::uwvm2::object::global::wasm_global_ref_t) * CHAR_BIT)))
        {
            return 3;
        }

        auto const alignment{llvm_details::get_llvm_global_storage_alignment()};
        if(auto global{::llvm::dyn_cast<::llvm::GlobalVariable>(pointer)}; global != nullptr && global->getAlign() != alignment) { return 4; }
        // This mirrors the direct global.get/global.set memory instructions. WAT integration tests exercise the actual
        // opcode emitters and mutable/imported reference round-trips; this probe also pins the external object's IR ABI.
        auto load{builder.CreateLoad(llvm_type, pointer)};
        load->setAlignment(alignment);
        auto store{builder.CreateStore(load, pointer)};
        store->setAlignment(alignment);
        if(load->getAlign().value() != alignof(::uwvm2::object::global::wasm_global_storage_u) || store->getAlign() != load->getAlign()) { return 5; }
    }
    builder.CreateRetVoid();
    return ::llvm::verifyModule(module, ::std::addressof(::llvm::errs())) ? 6 : 0;
}

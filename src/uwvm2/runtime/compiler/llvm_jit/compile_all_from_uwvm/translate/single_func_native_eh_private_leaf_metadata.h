// Only private-leaf Stage 3 owns these versioned call markers. They are not
// source admission, generation identity, loaded-code proof or cache permission.
namespace native_eh_private_leaf
{
    class staged_module;
    inline constexpr char call_metadata_name[]{"uwvm.native.eh.private-leaf.call.r1"};
    struct call_identity
    {
        ::std::size_t function_index{}, expression_offset{}, event_ordinal{}, callsite_index{}, target_index{};
        friend constexpr bool operator==(call_identity const&, call_identity const&) noexcept = default;
    };
    [[nodiscard]] inline bool attach_actual_call_metadata(::llvm::CallBase& call, call_identity identity) noexcept
    {
        if(call.getMetadata(call_metadata_name) != nullptr || call.getParent() == nullptr ||
           identity.function_index >= 256uz || identity.target_index >= 256uz ||
           identity.expression_offset >= 8uz * 1'024uz * 1'024uz || identity.event_ordinal >= 1'000'000uz ||
           identity.callsite_index >= 4'096uz) { return false; }
#ifdef UWVM_CPP_EXCEPTIONS
        try // Only this prototype's own metadata allocation, not emission/validation.
        {
            auto& context{call.getContext()};
            ::llvm::Metadata* values[6uz]{};
            ::std::uint_least64_t const integers[6uz]{1u, identity.function_index, identity.expression_offset,
                identity.event_ordinal, identity.callsite_index, identity.target_index};
            for(::std::size_t index{}; index != 6uz; ++index)
            {
                // [private fixed metadata fields][index < 6] end
                // [safe] LLVM owns each node; no pointer into this stack array survives.
                values[index] = ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(::llvm::Type::getInt64Ty(context), integers[index]));
            }
            call.setMetadata(call_metadata_name, ::llvm::MDNode::get(context, values));
            return true;
        }
        catch(...) { return false; }
#else
        return false;
#endif
    }
    [[nodiscard]] inline bool read_actual_call_metadata(::llvm::CallBase const& call, call_identity& identity) noexcept
    {
        auto const node{call.getMetadata(call_metadata_name)};
        if(node == nullptr || node->getNumOperands() != 6u) { return false; }
        ::std::size_t values[6uz]{};
        for(unsigned index{}; index != 6u; ++index)
        {
            // [actual fixed LLVM node operands][index < 6] end
            // [safe] Validate representation/width before extracting any number.
            auto const metadata{::llvm::dyn_cast<::llvm::ConstantAsMetadata>(node->getOperand(index).get())};
            if(metadata == nullptr) { return false; }
            auto const value{::llvm::dyn_cast<::llvm::ConstantInt>(metadata->getValue())};
            if(value == nullptr || value->getBitWidth() != 64u || value->getZExtValue() > (::std::numeric_limits<::std::size_t>::max)())
            { return false; }
            values[index] = static_cast<::std::size_t>(value->getZExtValue());
        }
        if(values[0u] != 1uz) { return false; }
        identity = {values[1u], values[2u], values[3u], values[4u], values[5u]};
        return true; // Only decoded data. Typed source/witness/final-node binding still required.
    }
}

#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Copied after the SAME fused instruction's original signature/operand
    // checks. These indices are compiler DATA, not a callable address, a guest
    // validity certificate or permission to read a runtime table entry.
    enum class validated_call_dependency_kind : unsigned
    { direct_function, indirect_table, reference_signature };
    struct validated_call_dependency
    {
        validated_call_dependency_kind kind{};
        ::std::size_t index{};
    };
}

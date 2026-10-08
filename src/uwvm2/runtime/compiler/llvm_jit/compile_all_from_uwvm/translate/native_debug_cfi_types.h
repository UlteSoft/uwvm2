/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)            *
 * Licensed under the APL-2.0 License (see LICENSE file).      *
 *************************************************************/
#pragma once
// Owned integer unwind metadata. No LLVM ABI dependency or native authority.
#ifndef UWVM_MODULE
# include <array>
# include <cstdint>
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::llvm_jit::details
{
    enum class native_debug_cfi_rule_kind : unsigned char
    { unavailable, same, cfa_value, cfa_memory, register_value, register_memory };
    struct native_debug_cfi_rule
    {
        native_debug_cfi_rule_kind kind{};
        ::std::uint32_t reg{};
        ::std::int32_t offset{};
    };
    // CFA-only metadata cannot recover a return address or authorize a read.
    struct native_debug_cfa_row
    {
        ::std::uintptr_t begin{}, end{};
        ::std::uint32_t cfa_register{};
        ::std::int32_t cfa_offset{};
        bool usable{};
    };
    struct native_debug_cfi_row
    {
        ::std::uintptr_t begin{}, end{};
        ::std::uint32_t cfa_register{};
        ::std::int32_t cfa_offset{};
        ::std::array<native_debug_cfi_rule, 32u> registers{}; // Qualified target DWARF GPR/RA numbering.
        bool usable{}, cfa_usable{};
    };

}

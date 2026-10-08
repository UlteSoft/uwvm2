/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)            *
 * Licensed under the APL-2.0 License (see LICENSE file).      *
 *************************************************************/

#pragma once

#include <cstdint>
#include <cstring>

namespace uwvm2::runtime::lib::details
{
    // Input is a live, trusted callable address published by the native loader,
    // never a Wasm integer or an arbitrary PC supplied by the guest. Keep that
    // callable address for invocation; only unwind lookup needs the code address.
    [[nodiscard]] inline ::std::uintptr_t native_function_code_address(::std::uintptr_t callable_address) noexcept
    {
        if(callable_address == 0u) { return 0u; }
#if defined(__ELF__) && defined(__powerpc64__) && (!defined(_CALL_ELF) || _CALL_ELF == 1)
        // PPC64 ELFv1 function pointers name a three-word descriptor (entry, TOC,
        // environment). _Unwind_GetRegionStart/IP name instructions instead.
        // Read just the entry word from the live loader-owned descriptor. Copying
        // bytes avoids alignment/aliasing assumptions about its C++ object type.
        // [safe: entry word][safe: TOC][safe: environment] one-past descriptor
        //  ^ descriptor; sizeof(code_address) bytes are readable, no advancement.
        auto const descriptor{reinterpret_cast<void const*>(callable_address)};
        ::std::uintptr_t code_address{};
        ::std::memcpy(&code_address, descriptor, sizeof(code_address));
        return code_address;
#elif defined(__arm__) || defined(__thumb__)
        // A Thumb callable carries an ISA-state tag; unwind regions are byte PCs.
        return callable_address & ~::std::uintptr_t{1u};
#else
        return callable_address;
#endif
    }
}

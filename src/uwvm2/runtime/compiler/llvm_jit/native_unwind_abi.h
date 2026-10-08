/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

// Native backtrace accessors and FDE registration never propagate a C++
// exception. Bind their exact platform C symbols with a noexcept declaration,
// rather than forcing a potentially throwing SDK declaration through a C++
// wrapper. _Unwind_RaiseException/_Unwind_Resume and __cxa_rethrow deliberately
// do not belong here: genuine exception propagation must remain unwindable.
#if !defined(_WIN32) && (defined(__GNUC__) || defined(__clang__)) && __has_include(<unwind.h>)
# include <cstdint>
# include <unwind.h>
# if defined(__APPLE__)
#  define UWVM2_NATIVE_UNWIND_C_PREFIX "_"
# else
#  define UWVM2_NATIVE_UNWIND_C_PREFIX ""
# endif
namespace uwvm2::runtime::compiler::llvm_jit::native_unwind_abi
{
    // Some SDKs use _Unwind_Word for IPInfo whereas others use _Unwind_Ptr.
    // Preserve the selected header's precise return type on every target ABI.
    using ip_info_result = decltype(::_Unwind_GetIPInfo(nullptr, nullptr));
    using cfa_result = decltype(::_Unwind_GetCFA(nullptr));
    using region_start_result = decltype(::_Unwind_GetRegionStart(nullptr));
# if defined(__arm__) && !defined(__ARM_DWARF_EH__) && !defined(__USING_SJLJ_EXCEPTIONS__) && !defined(__SEH__)
    // GNU ARM exposes IPInfo only as an inline EHABI accessor, not a dynamic
    // libgcc symbol. Read its real virtual PC through the matching VRS ABI.
    // This native unwinder helper grants no debugger register/memory authority.
    extern "C" _Unwind_VRS_Result vrs_get_noexcept(_Unwind_Context*, _Unwind_VRS_RegClass,
        ::std::uint32_t, _Unwind_VRS_DataRepresentation, void*) noexcept __asm__("_Unwind_VRS_Get");
    [[nodiscard]] inline ip_info_result get_ip_info_noexcept(_Unwind_Context* context, int* before) noexcept
    {
        if(before != nullptr) { *before = 0; }
        ::std::uint32_t pc{};
        if(context == nullptr || vrs_get_noexcept(context, _UVRSC_CORE, 15u, _UVRSD_UINT32, &pc) != _UVRSR_OK)
        { return 0u; }
        return static_cast<ip_info_result>(pc & ~::std::uint32_t{1u});
    }
# else
    extern "C" ip_info_result get_ip_info_noexcept(_Unwind_Context*, int*) noexcept
        __asm__(UWVM2_NATIVE_UNWIND_C_PREFIX "_Unwind_GetIPInfo");
# endif
    extern "C" cfa_result get_cfa_noexcept(_Unwind_Context*) noexcept
        __asm__(UWVM2_NATIVE_UNWIND_C_PREFIX "_Unwind_GetCFA");
    extern "C" region_start_result get_region_start_noexcept(_Unwind_Context*) noexcept
        __asm__(UWVM2_NATIVE_UNWIND_C_PREFIX "_Unwind_GetRegionStart");
    // Runtime callbacks are noexcept too; no callback can unwind across the C
    // unwinder. The typedef retains the library's exact calling convention.
    extern "C" _Unwind_Reason_Code backtrace_noexcept(_Unwind_Trace_Fn, void*) noexcept
        __asm__(UWVM2_NATIVE_UNWIND_C_PREFIX "_Unwind_Backtrace");
# if defined(__APPLE__)
    // Darwin registers individual FDEs. The section manager proves each FDE's
    // full extent before passing its borrowed address, and deregisters before
    // freeing code/unwind storage. These functions cannot throw C++ exceptions.
    extern "C" void register_frame_noexcept(void const*) noexcept
        __asm__("___register_frame");
    extern "C" void deregister_frame_noexcept(void const*) noexcept
        __asm__("___deregister_frame");
# endif
}
# undef UWVM2_NATIVE_UNWIND_C_PREFIX
#endif

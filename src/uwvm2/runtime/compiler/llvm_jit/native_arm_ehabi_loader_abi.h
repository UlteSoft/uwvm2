/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include <cstdint>
#if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && defined(__linux__) && defined(__arm__)
// Headers alone do not qualify the actual linked ARM RuntimeDyld loader.
// A missing/stale archive is unavailable before compiling guest EH objects.
extern "C" ::std::uint32_t uwvm_llvm_arm_ehabi_target2_abi_v1() noexcept __attribute__((weak));
#endif

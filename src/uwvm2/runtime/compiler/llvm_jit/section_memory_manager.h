/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V / | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <algorithm>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
// platform
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <llvm/ExecutionEngine/SectionMemoryManager.h>
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT) && defined(_WIN64) &&                                                                                                      \
     ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) && !defined(__CYGWIN__) &&              \
     __has_include(<windows.h>)
#  ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN 1
#  endif
#  ifndef NOMINMAX
#   define NOMINMAX 1
#  endif
#  include <windows.h>
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT) && defined(__APPLE__) && !defined(_WIN32) && __has_include(<unwind.h>)
#  include <unwind.h>
extern "C" void __register_frame(void const*);
extern "C" void __deregister_frame(void const*);
# endif
// import
# include <uwvm2/utils/container/impl.h>
#endif

#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH")
#undef UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(_WIN64) &&                                                                                                       \
    ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) && !defined(__CYGWIN__) &&               \
    __has_include(<windows.h>)
# define UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH 0
#endif

#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_APPLE_EH_FRAME")
#undef UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_APPLE_EH_FRAME
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(__APPLE__) && !defined(_WIN32) && __has_include(<unwind.h>)
# define UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_APPLE_EH_FRAME 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_APPLE_EH_FRAME 0
#endif

namespace uwvm2::runtime::compiler::llvm_jit::details
{
#if defined(UWVM_RUNTIME_LLVM_JIT)
# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_APPLE_EH_FRAME
    struct runtime_llvm_jit_eh_frame_record
    {
        ::std::uint8_t* addr{};
        ::std::size_t size{};
    };

    template <typename Visit>
    inline void visit_runtime_llvm_jit_eh_frame_fdes(::std::uint8_t* addr, ::std::size_t size, Visit visit) noexcept
    {
        auto const end{addr + size};
        auto curr{addr};
        while(curr < end)
        {
            auto const record{curr};
            if(static_cast<::std::size_t>(end - curr) < sizeof(::std::uint_least32_t)) [[unlikely]] { return; }

            ::std::uint_least32_t length32{};
            ::std::memcpy(::std::addressof(length32), curr, sizeof(length32));
            curr += sizeof(length32);
            if(length32 == 0u) { return; }

            ::std::size_t length{};
            if(length32 == 0xffffffffu)
            {
                if(static_cast<::std::size_t>(end - curr) < sizeof(::std::uint_least64_t)) [[unlikely]] { return; }
                ::std::uint_least64_t length64{};
                ::std::memcpy(::std::addressof(length64), curr, sizeof(length64));
                curr += sizeof(length64);
                if(length64 > static_cast<::std::uint_least64_t>((::std::numeric_limits<::std::size_t>::max)())) [[unlikely]] { return; }
                length = static_cast<::std::size_t>(length64);
            }
            else
            {
                length = static_cast<::std::size_t>(length32);
            }

            if(length > static_cast<::std::size_t>(end - curr)) [[unlikely]] { return; }
            auto const next{curr + length};
            if(length < sizeof(::std::uint_least32_t)) [[unlikely]]
            {
                curr = next;
                continue;
            }

            ::std::uint_least32_t cie_offset{};
            ::std::memcpy(::std::addressof(cie_offset), curr, sizeof(cie_offset));
            if(cie_offset != 0u) { visit(record); }
            curr = next;
        }
    }
# endif

    class runtime_llvm_jit_section_memory_manager final : public ::llvm::SectionMemoryManager
    {
    public:
        runtime_llvm_jit_section_memory_manager() : ::llvm::SectionMemoryManager(nullptr, UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH != 0) {}

        ~runtime_llvm_jit_section_memory_manager() override { deregisterEHFrames(); }

        ::std::uint8_t* allocateCodeSection(::std::uintptr_t size, unsigned alignment, unsigned section_id, ::llvm::StringRef section_name) override
        {
            auto* const addr{::llvm::SectionMemoryManager::allocateCodeSection(size, alignment, section_id, section_name)};
            record_win64_image_base(addr);
            return addr;
        }

        ::std::uint8_t*
            allocateDataSection(::std::uintptr_t size, unsigned alignment, unsigned section_id, ::llvm::StringRef section_name, bool is_read_only) override
        {
            auto* const addr{::llvm::SectionMemoryManager::allocateDataSection(size, alignment, section_id, section_name, is_read_only)};
            record_win64_image_base(addr);
            return addr;
        }

        void registerEHFrames(::std::uint8_t* addr, ::std::uint64_t load_addr, ::std::size_t size) override
        {
# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
            static_cast<void>(load_addr);
            register_win64_seh_function_table(addr, size);
# elif UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_APPLE_EH_FRAME
            static_cast<void>(load_addr);
            visit_runtime_llvm_jit_eh_frame_fdes(addr, size, [](::std::uint8_t* fde) noexcept { __register_frame(fde); });
            eh_frame_records_.push_back(runtime_llvm_jit_eh_frame_record{addr, size});
# else
            ::llvm::SectionMemoryManager::registerEHFrames(addr, load_addr, size);
# endif
        }

        void deregisterEHFrames() override
        {
# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
            for(auto const& frame: win64_seh_records_) { ::RtlDeleteFunctionTable(frame.function_table); }
            win64_seh_records_.clear();
# elif UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_APPLE_EH_FRAME
            for(auto const& frame: eh_frame_records_)
            {
                visit_runtime_llvm_jit_eh_frame_fdes(frame.addr, frame.size, [](::std::uint8_t* fde) noexcept { __deregister_frame(fde); });
            }
            eh_frame_records_.clear();
# else
            ::llvm::SectionMemoryManager::deregisterEHFrames();
# endif
        }

    private:
# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
        struct runtime_llvm_jit_win64_seh_record
        {
            ::RUNTIME_FUNCTION* function_table{};
            ::DWORD function_count{};
        };

        ::std::uintptr_t win64_image_base_{(::std::numeric_limits<::std::uintptr_t>::max)()};
        ::uwvm2::utils::container::vector<runtime_llvm_jit_win64_seh_record> win64_seh_records_{};

        inline void record_win64_image_base(::std::uint8_t* addr) noexcept
        {
            auto const address{reinterpret_cast<::std::uintptr_t>(addr)};
            if(address != 0u && address < win64_image_base_) { win64_image_base_ = address; }
        }

        inline bool register_win64_seh_function_table(::std::uint8_t* addr, ::std::size_t size) noexcept
        {
            if(addr == nullptr || size == 0uz) [[unlikely]] { return false; }
            if(size % sizeof(::RUNTIME_FUNCTION) != 0uz) [[unlikely]] { return false; }

            auto const count{size / sizeof(::RUNTIME_FUNCTION)};
            if(count == 0uz || count > static_cast<::std::size_t>((::std::numeric_limits<::DWORD>::max)())) [[unlikely]] { return false; }

            auto const image_base{win64_image_base_ == (::std::numeric_limits<::std::uintptr_t>::max)() ? reinterpret_cast<::std::uintptr_t>(addr)
                                                                                                        : win64_image_base_};
            auto* const function_table{reinterpret_cast<::RUNTIME_FUNCTION*>(addr)};
            auto const function_count{static_cast<::DWORD>(count)};
            if(!::RtlAddFunctionTable(function_table, function_count, static_cast<::DWORD64>(image_base))) [[unlikely]] { return false; }

            win64_seh_records_.push_back(runtime_llvm_jit_win64_seh_record{function_table, function_count});
            return true;
        }
# else
        inline void record_win64_image_base(::std::uint8_t*) noexcept {}
# endif

# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_APPLE_EH_FRAME
        ::uwvm2::utils::container::vector<runtime_llvm_jit_eh_frame_record> eh_frame_records_{};
# endif
    };
#endif
}  // namespace uwvm2::runtime::compiler::llvm_jit::details

#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_APPLE_EH_FRAME")
#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH")

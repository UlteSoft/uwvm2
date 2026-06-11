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
# if defined(UWVM_RUNTIME_LLVM_JIT) && defined(__APPLE__) && !defined(_WIN32) && __has_include(<unwind.h>)
#  include <unwind.h>
extern "C" void __register_frame(void const*);
extern "C" void __deregister_frame(void const*);
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
#endif

#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH")
#undef UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(_WIN64) &&                                                                                                       \
    ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) && !defined(__CYGWIN__)
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
    inline constexpr void visit_runtime_llvm_jit_eh_frame_fdes(::std::uint8_t* addr, ::std::size_t size, Visit visit) noexcept
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
                if(length64 > static_cast<::std::uint_least64_t>(::std::numeric_limits<::std::size_t>::max())) [[unlikely]] { return; }
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
        inline constexpr runtime_llvm_jit_section_memory_manager() noexcept :
            ::llvm::SectionMemoryManager(nullptr, UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH != 0)
        {}

        inline constexpr ~runtime_llvm_jit_section_memory_manager() override { deregisterEHFrames(); }

        inline constexpr ::std::uint8_t*
            allocateCodeSection(::std::uintptr_t size, unsigned alignment, unsigned section_id, ::llvm::StringRef section_name) noexcept override
        {
            auto const addr{::llvm::SectionMemoryManager::allocateCodeSection(size, alignment, section_id, section_name)};
            record_win64_loaded_section(addr, size);
            return addr;
        }

        inline constexpr ::std::uint8_t*
            allocateDataSection(::std::uintptr_t size, unsigned alignment, unsigned section_id, ::llvm::StringRef section_name, bool is_read_only) noexcept override
        {
            auto const addr{::llvm::SectionMemoryManager::allocateDataSection(size, alignment, section_id, section_name, is_read_only)};
            record_win64_loaded_section(addr, size);
            return addr;
        }

        inline constexpr void registerEHFrames(::std::uint8_t* addr, ::std::uint64_t load_addr, ::std::size_t size) noexcept override
        {
# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
            static_cast<void>(register_win64_seh_function_table(addr, load_addr, size));
# elif UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_APPLE_EH_FRAME
            static_cast<void>(load_addr);
            visit_runtime_llvm_jit_eh_frame_fdes(addr, size, [](::std::uint8_t* fde) constexpr noexcept { __register_frame(fde); });
            eh_frame_records_.push_back(runtime_llvm_jit_eh_frame_record{addr, size});
# else
            ::llvm::SectionMemoryManager::registerEHFrames(addr, load_addr, size);
# endif
        }

        inline constexpr void deregisterEHFrames() noexcept override
        {
# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
            for(auto const& record: win64_seh_records_)
            {
                static_cast<void>(::fast_io::win32::nt::RtlDeleteFunctionTable(record.function_table));
            }
            win64_seh_records_.clear();
# elif UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_APPLE_EH_FRAME
            for(auto const& frame: eh_frame_records_)
            {
                visit_runtime_llvm_jit_eh_frame_fdes(frame.addr, frame.size, [](::std::uint8_t* fde) constexpr noexcept { __deregister_frame(fde); });
            }
            eh_frame_records_.clear();
# else
            ::llvm::SectionMemoryManager::deregisterEHFrames();
# endif
        }

    private:
# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
        struct runtime_llvm_jit_win64_loaded_section
        {
            ::std::uintptr_t address{};
            ::std::uintptr_t size{};
        };

        struct runtime_llvm_jit_win64_seh_record
        {
            ::fast_io::win32::win_current_runtime_function* function_table{};
            ::std::uint32_t function_count{};
        };

        ::uwvm2::utils::container::vector<runtime_llvm_jit_win64_loaded_section> win64_loaded_sections_{};
        ::uwvm2::utils::container::vector<runtime_llvm_jit_win64_seh_record> win64_seh_records_{};

        inline constexpr void record_win64_loaded_section(::std::uint8_t* addr, ::std::uintptr_t size) noexcept
        {
            if(addr == nullptr || size == 0uz) [[unlikely]] { return; }
            auto const address{reinterpret_cast<::std::uintptr_t>(addr)};
            if(address == 0u) [[unlikely]] { return; }
            win64_loaded_sections_.push_back(runtime_llvm_jit_win64_loaded_section{address, size});
        }

        [[nodiscard]] inline constexpr ::std::uintptr_t get_win64_image_base(::std::uintptr_t fallback) const noexcept
        {
            auto image_base{(::std::numeric_limits<::std::uintptr_t>::max)()};
            for(auto const& section: win64_loaded_sections_)
            {
                if(section.address != 0u && section.address < image_base) { image_base = section.address; }
            }
            if(image_base != (::std::numeric_limits<::std::uintptr_t>::max)()) { return image_base; }
            return fallback;
        }

        inline constexpr bool register_win64_seh_function_table(::std::uint8_t* addr, ::std::uint64_t load_addr, ::std::size_t size) noexcept
        {
            if(addr == nullptr || size == 0uz) [[unlikely]] { return false; }
            if(size % sizeof(::fast_io::win32::win_current_runtime_function) != 0uz) [[unlikely]] { return false; }

            auto const count{size / sizeof(::fast_io::win32::win_current_runtime_function)};
            if(count == 0uz || count > static_cast<::std::size_t>((::std::numeric_limits<::std::uint32_t>::max)())) [[unlikely]] { return false; }

            // COFF x64 encodes .pdata RVAs relative to RuntimeDyld's synthetic __ImageBase.  LLVM computes that base as
            // the lowest loaded section address; RtlAddFunctionTable must receive the same value or Windows unwinding
            auto constind inlined/generated frames reliably.
            auto const fallback_base{load_addr == 0u ? reinterpret_cast<::std::uintptr_t>(addr) : static_cast<::std::uintptr_t>(load_addr)};
            auto const image_base{get_win64_image_base(fallback_base)};
            auto const function_table{reinterpret_cast<::fast_io::win32::win_current_runtime_function*>(addr)};
            auto const function_count{static_cast<::std::uint32_t>(count)};
            if(!::fast_io::win32::nt::RtlAddFunctionTable(
                   function_table, function_count, static_cast<::fast_io::win32::win_current_unwind_address>(image_base))) [[unlikely]]
            {
                return false;
            }

            win64_seh_records_.push_back(runtime_llvm_jit_win64_seh_record{function_table, function_count});
            return true;
        }
# else
        inline constexpr void record_win64_loaded_section(::std::uint8_t*, ::std::uintptr_t) noexcept {}
# endif

# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_APPLE_EH_FRAME
        ::uwvm2::utils::container::vector<runtime_llvm_jit_eh_frame_record> eh_frame_records_{};
# endif
    };
#endif
}  // namespace uwvm2::runtime::compiler::llvm_jit::details

#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_APPLE_EH_FRAME")
#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH")

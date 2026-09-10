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
# include <cerrno>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <string>
# include <system_error>
// platform
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <llvm/Config/llvm-config.h>
#  include <llvm/ExecutionEngine/SectionMemoryManager.h>
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT) && defined(__linux__) && defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
#  include <sys/mman.h>
#  include <unistd.h>
#  ifndef MAP_FIXED_NOREPLACE
#   define MAP_FIXED_NOREPLACE 0x100000
#  endif
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT) && !defined(_WIN32) && !defined(__arm__) && !defined(__thumb__) && __has_include(<unwind.h>)
#  include <unwind.h>
#  include "dwarf_eh_frame_registration.h"
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH")
#undef UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(_WIN64) && !(defined(__arm64ec__) || defined(_M_ARM64EC)) && !defined(__CYGWIN__) &&                             \
    (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64))
# define UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH 0
#endif

#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RESERVE_ALLOC")
#undef UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RESERVE_ALLOC
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(LLVM_VERSION_MAJOR) && LLVM_VERSION_MAJOR >= 22
# define UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RESERVE_ALLOC 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RESERVE_ALLOC 0
#endif

#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_DWARF_EH_FRAME")
#undef UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_DWARF_EH_FRAME
#if defined(UWVM_RUNTIME_LLVM_JIT) && !defined(_WIN32) && !defined(__arm__) && !defined(__thumb__) && __has_include(<unwind.h>)
# define UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_DWARF_EH_FRAME 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_DWARF_EH_FRAME 0
#endif

#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RISCV64_LOW_MAPPER")
#undef UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RISCV64_LOW_MAPPER
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(__linux__) && defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
# define UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RISCV64_LOW_MAPPER 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RISCV64_LOW_MAPPER 0
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::llvm_jit::details
{
#if defined(UWVM_RUNTIME_LLVM_JIT)
# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RISCV64_LOW_MAPPER
    class runtime_llvm_jit_riscv64_low_address_memory_mapper final : public ::llvm::SectionMemoryManager::MemoryMapper
    {
        [[nodiscard]] inline static ::std::size_t page_size() noexcept
        {
            auto const value{::sysconf(_SC_PAGESIZE)};
            if(value <= 0) [[unlikely]] { return 4096uz; }
            return static_cast<::std::size_t>(value);
        }

        [[nodiscard]] inline static bool align_to_page(::std::size_t value, ::std::size_t& aligned) noexcept
        {
            auto const ps{page_size()};
            auto const mask{ps - 1uz};
            if(value > (::std::numeric_limits<::std::size_t>::max)() - mask) [[unlikely]] { return false; }
            aligned = (value + mask) & ~mask;
            return true;
        }

        [[nodiscard]] inline static unsigned mmap_prot(unsigned flags) noexcept
        {
            unsigned prot{};
            if((flags & ::llvm::sys::Memory::MF_READ) != 0u) { prot |= PROT_READ; }
            if((flags & ::llvm::sys::Memory::MF_WRITE) != 0u) { prot |= PROT_WRITE; }
            if((flags & ::llvm::sys::Memory::MF_EXEC) != 0u) { prot |= PROT_EXEC; }
            return prot;
        }

        [[nodiscard]] inline static ::std::uintptr_t align_address_up(::std::uintptr_t value) noexcept
        {
            auto const ps{static_cast<::std::uintptr_t>(page_size())};
            auto const mask{ps - 1u};
            if(value > (::std::numeric_limits<::std::uintptr_t>::max)() - mask) [[unlikely]] { return 0u; }
            return (value + mask) & ~mask;
        }

        [[nodiscard]] inline static ::std::uintptr_t near_block_hint(::llvm::sys::MemoryBlock const* near_block) noexcept
        {
            if(near_block == nullptr || near_block->base() == nullptr || near_block->allocatedSize() == 0uz) { return 0u; }
            auto const base{reinterpret_cast<::std::uintptr_t>(near_block->base())};
            auto const size{static_cast<::std::uintptr_t>(near_block->allocatedSize())};
            if(base > (::std::numeric_limits<::std::uintptr_t>::max)() - size) [[unlikely]] { return 0u; }
            return align_address_up(base + size);
        }

    public:
        inline ::llvm::sys::MemoryBlock allocateMappedMemory(::llvm::SectionMemoryManager::AllocationPurpose,
                                                             ::std::size_t num_bytes,
                                                             ::llvm::sys::MemoryBlock const* const near_block,
                                                             unsigned flags,
                                                             ::std::error_code& ec) override
        {
            constexpr ::std::uintptr_t low_begin{0x10000000u};
            constexpr ::std::uintptr_t low_end{0x70000000u};
            constexpr ::std::uintptr_t scan_stride{0x01000000u};

            ::std::size_t size{};
            if(!align_to_page(num_bytes == 0uz ? page_size() : num_bytes, size)) [[unlikely]]
            {
                ec = ::std::make_error_code(::std::errc::value_too_large);
                return {};
            }
            auto const prot{mmap_prot(flags)};
            auto const base_map_flags{MAP_PRIVATE | MAP_ANONYMOUS};
            int cleanup_errno{};

            auto try_map{[&](::std::uintptr_t hint) noexcept -> void*
                         {
                             if(hint < low_begin || hint > low_end || static_cast<::std::uintptr_t>(size) > low_end - hint) { return MAP_FAILED; }
                             auto const requested{reinterpret_cast<void*>(hint)};
                             auto mapped{::mmap(requested, size, prot, base_map_flags | MAP_FIXED_NOREPLACE, -1, 0)};
                             if(mapped == requested) { return mapped; }
                             if(mapped != MAP_FAILED)
                             {
                                 // Old kernels may ignore an unknown MAP_FIXED_NOREPLACE bit and treat `hint` as advisory.
                                 // Never return that unrelated mapping to the low-address allocator.
                                 if(::munmap(mapped, size) != 0)
                                 {
                                     cleanup_errno = errno == 0 ? EIO : errno;
                                     return MAP_FAILED;
                                 }
                                 errno = EEXIST;
                                 return MAP_FAILED;
                             }
                             if(errno != EINVAL) { return MAP_FAILED; }

                             // MAP_FIXED_NOREPLACE was added after the original mmap ABI. On an EINVAL-only kernel, an
                             // ordinary hint is safe only when the kernel returns the exact requested address.
                             mapped = ::mmap(requested, size, prot, base_map_flags, -1, 0);
                             if(mapped == requested) { return mapped; }
                             if(mapped == MAP_FAILED) { return MAP_FAILED; }
                             if(::munmap(mapped, size) != 0)
                             {
                                 cleanup_errno = errno == 0 ? EIO : errno;
                                 return MAP_FAILED;
                             }
                             errno = EEXIST;
                             return MAP_FAILED;
                         }};

            if(auto const hint{near_block_hint(near_block)}; hint != 0u)
            {
                auto const mapped{try_map(hint)};
                if(mapped != MAP_FAILED)
                {
                    ec.clear();
                    return ::llvm::sys::MemoryBlock{mapped, size};
                }
                if(cleanup_errno != 0)
                {
                    ec = ::std::error_code(cleanup_errno, ::std::generic_category());
                    return {};
                }
            }

            for(::std::uintptr_t hint{low_begin}; hint <= low_end && static_cast<::std::uintptr_t>(size) <= low_end - hint; hint += scan_stride)
            {
                auto const mapped{try_map(hint)};
                if(mapped != MAP_FAILED)
                {
                    ec.clear();
                    return ::llvm::sys::MemoryBlock{mapped, size};
                }
                if(cleanup_errno != 0)
                {
                    ec = ::std::error_code(cleanup_errno, ::std::generic_category());
                    return {};
                }
            }

            ec = ::std::error_code(errno == 0 ? ENOMEM : errno, ::std::generic_category());
            return {};
        }

        inline ::std::error_code protectMappedMemory(::llvm::sys::MemoryBlock const& block, unsigned flags) override
        {
            if(block.base() == nullptr || block.allocatedSize() == 0uz) { return {}; }
            if(::mprotect(block.base(), block.allocatedSize(), static_cast<int>(mmap_prot(flags))) == 0) { return {}; }
            return ::std::error_code(errno, ::std::generic_category());
        }

        inline ::std::error_code releaseMappedMemory(::llvm::sys::MemoryBlock& block) override
        {
            if(block.base() == nullptr || block.allocatedSize() == 0uz) { return {}; }
            if(::munmap(block.base(), block.allocatedSize()) == 0)
            {
                block = {};
                return {};
            }
            return ::std::error_code(errno, ::std::generic_category());
        }
    };

    [[nodiscard]] inline runtime_llvm_jit_riscv64_low_address_memory_mapper& get_runtime_llvm_jit_riscv64_low_address_memory_mapper() noexcept
    {
        static runtime_llvm_jit_riscv64_low_address_memory_mapper mapper{};
        return mapper;
    }
# endif

# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_DWARF_EH_FRAME
    struct runtime_llvm_jit_eh_frame_record
    {
        ::std::uint8_t* addr{};
        ::std::size_t size{};
    };

    template <typename Visit>
    inline constexpr void visit_runtime_llvm_jit_eh_frame_fdes(::std::uint8_t* addr, ::std::size_t size, Visit visit) noexcept
    {
        // Apple's __register_frame ABI accepts one FDE, unlike GNU libgcc's whole-section ABI. Keep this bounded
        // parser for the Apple path: a CIE has a zero ID, and a zero-length record terminates .eh_frame. Neither is
        // itself an FDE. LLVM's RTDyldMemoryManager.cpp documents and selects the other configured unwind ABIs.
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
            ::std::size_t cie_offset_size{sizeof(::std::uint_least32_t)};
            if(length32 == 0xffffffffu)
            {
                if(static_cast<::std::size_t>(end - curr) < sizeof(::std::uint_least64_t)) [[unlikely]] { return; }
                ::std::uint_least64_t length64{};
                ::std::memcpy(::std::addressof(length64), curr, sizeof(length64));
                curr += sizeof(length64);
                if(length64 > static_cast<::std::uint_least64_t>(::std::numeric_limits<::std::size_t>::max())) [[unlikely]] { return; }
                length = static_cast<::std::size_t>(length64);
                cie_offset_size = sizeof(::std::uint_least64_t);
            }
            else
            {
                length = static_cast<::std::size_t>(length32);
            }

            if(length > static_cast<::std::size_t>(end - curr)) [[unlikely]] { return; }
            auto const next{curr + length};
            if(length < cie_offset_size) [[unlikely]]
            {
                curr = next;
                continue;
            }

            ::std::uint_least64_t cie_offset{};
            ::std::memcpy(::std::addressof(cie_offset), curr, cie_offset_size);
            if(cie_offset != 0u) { visit(record); }
            curr = next;
        }
    }
# endif

    class runtime_llvm_jit_section_memory_manager final : public ::llvm::SectionMemoryManager
    {
    public:
        inline constexpr runtime_llvm_jit_section_memory_manager() noexcept :
# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RESERVE_ALLOC
#  if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RISCV64_LOW_MAPPER
            ::llvm::SectionMemoryManager(::std::addressof(get_runtime_llvm_jit_riscv64_low_address_memory_mapper()), true)
#  else
            ::llvm::SectionMemoryManager(nullptr, UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH != 0)
#  endif
# else
#  if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RISCV64_LOW_MAPPER
            ::llvm::SectionMemoryManager(::std::addressof(get_runtime_llvm_jit_riscv64_low_address_memory_mapper()))
#  else
            ::llvm::SectionMemoryManager(nullptr)
#  endif
# endif
        {
        }

        inline constexpr ~runtime_llvm_jit_section_memory_manager() override { deregisterEHFrames(); }

        [[nodiscard]] inline ::llvm::JITSymbol findSymbol(::std::string const& name) override
        {
            auto const address{this->getSymbolAddress(name)};
            if(address == 0u) { return nullptr; }
            return ::llvm::JITSymbol{address, ::llvm::JITSymbolFlags::Exported};
        }

        inline constexpr ::std::uint8_t*
            allocateCodeSection(::std::uintptr_t size, unsigned alignment, unsigned section_id, ::llvm::StringRef section_name) noexcept override
        {
            auto const addr{::llvm::SectionMemoryManager::allocateCodeSection(size, alignment, section_id, section_name)};
            record_win64_loaded_section(addr, size);
            return addr;
        }

        inline constexpr ::std::uint8_t* allocateDataSection(::std::uintptr_t size,
                                                             unsigned alignment,
                                                             unsigned section_id,
                                                             ::llvm::StringRef section_name,
                                                             bool is_read_only) noexcept override
        {
            auto const addr{::llvm::SectionMemoryManager::allocateDataSection(size, alignment, section_id, section_name, is_read_only)};
            record_win64_loaded_section(addr, size);
            record_win64_pdata_section(addr, size, section_name);
            return addr;
        }

        inline bool finalizeMemory(::std::string* error_message = nullptr) override
        {
            // MCJIT 20 through current LLVM discards this virtual's bool result. Retain a sticky result that every uwvm2
            // engine owner can inspect after finalizeObject, before it publishes or executes generated addresses.
            auto const base_failed{::llvm::SectionMemoryManager::finalizeMemory(error_message)};
            finalization_failure_ = finalization_failure_ || base_failed;

# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
            // RuntimeDyldCOFFAArch64::registerEHFrames is empty in LLVM 20/21/22 and current main. Allocation is the
            // only stable callback carrying the .pdata name, but registration must wait until RuntimeDyld has copied
            // the section and applied its image-relative relocations. finalizeMemory is that post-relocation boundary.
            if(!finalization_failure_)
            {
                for(auto& pending: win64_pending_pdata_sections_)
                {
                    if(pending.registration_attempted) { continue; }
                    pending.registration_attempted = true;
                    if(!register_win64_seh_function_table(pending.addr, 0u, pending.size)) [[unlikely]]
                    {
                        finalization_failure_ = true;
                        break;
                    }
                }
            }
            if(finalization_failure_ && !base_failed && error_message != nullptr && error_message->empty())
            {
                *error_message = "failed to register relocated Win64 JIT unwind metadata";
            }
# endif
            return finalization_failure_;
        }

        [[nodiscard]] inline constexpr bool has_finalization_failure() const noexcept { return finalization_failure_; }

        inline constexpr void registerEHFrames(::std::uint8_t* addr, ::std::uint64_t load_addr, ::std::size_t size) noexcept override
        {
# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
            // On Win64 the "EH frame" callback receives the COFF .pdata section, not DWARF CFI.  Windows unwinding is
            // table driven through RUNTIME_FUNCTION entries and the UNWIND_INFO records they reference in .xdata, so
            // registration must go through RtlAddFunctionTable instead of __register_frame/libgcc-style APIs.
            if(finalization_failure_) [[unlikely]] { return; }
            for(auto& pending: win64_pending_pdata_sections_)
            {
                if(pending.addr != addr) { continue; }
                if(pending.registration_attempted) { return; }
                pending.registration_attempted = true;
                break;
            }
            if(!register_win64_seh_function_table(addr, load_addr, size)) [[unlikely]] { finalization_failure_ = true; }
# elif UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_DWARF_EH_FRAME && defined(__APPLE__)
            static_cast<void>(load_addr);
            // The Apple unwinder accepts FDE pointers one at a time for JIT code. Keep the original section
            // range so the exact same FDE set can be deregistered before MCJIT releases the underlying memory.
            visit_runtime_llvm_jit_eh_frame_fdes(addr, size, [](::std::uint8_t* fde) constexpr noexcept { __register_frame(fde); });
            eh_frame_records_.push_back(runtime_llvm_jit_eh_frame_record{addr, size});
# else
            // Do not infer the registration ABI from the compiler or the presence of <unwind.h>. LLVM's
            // RTDyldMemoryManager selects per-FDE registration for its configured LLVM libunwind backend and
            // whole-section registration for GNU libgcc. Registering every suffix as a libgcc section duplicates
            // overlapping FDE ranges. Delegate this backend choice and its matching ownership to LLVM.
            // https://github.com/llvm/llvm-project/blob/main/llvm/lib/ExecutionEngine/RuntimeDyld/RTDyldMemoryManager.cpp
            ::llvm::SectionMemoryManager::registerEHFrames(addr, load_addr, size);
# endif
        }

        inline constexpr void deregisterEHFrames() noexcept override
        {
# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
            // The Windows runtime does not own the JIT memory.  Remove every dynamic function table before the code and
            // .pdata storage can disappear, otherwise a later stack walk may dereference stale unwind metadata.
            for(auto const& record: win64_seh_records_)
            {
                // Continuing into SectionMemoryManager destruction after a failed removal would release the .pdata
                // storage while Windows still has a live dynamic-table pointer.  The destructor is noexcept, so fail
                // closed before the base class can free that storage.
                if(!::fast_io::win32::nt::RtlDeleteFunctionTable(record.function_table)) [[unlikely]] { ::fast_io::fast_terminate(); }
            }
            win64_seh_records_.clear();
            win64_pending_pdata_sections_.clear();
# elif UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_DWARF_EH_FRAME && defined(__APPLE__)
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
        bool finalization_failure_{};

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
            ::std::uintptr_t image_base{};
        };

        struct runtime_llvm_jit_win64_pending_pdata_section
        {
            ::std::uint8_t* addr{};
            ::std::size_t size{};
            bool registration_attempted{};
        };

        ::uwvm2::utils::container::vector<runtime_llvm_jit_win64_loaded_section> win64_loaded_sections_{};
        ::uwvm2::utils::container::vector<runtime_llvm_jit_win64_seh_record> win64_seh_records_{};
        ::uwvm2::utils::container::vector<runtime_llvm_jit_win64_pending_pdata_section> win64_pending_pdata_sections_{};
        ::std::uintptr_t win64_image_base_{(::std::numeric_limits<::std::uintptr_t>::max)()};

        inline constexpr void record_win64_loaded_section(::std::uint8_t* addr, ::std::uintptr_t size) noexcept
        {
            // RuntimeDyld resolves COFF RVAs against a synthetic image base derived from the loaded sections.  Track all
            // code/data allocations so the Win64 function table can be registered with the same base later.
            if(addr == nullptr || size == 0uz) [[unlikely]] { return; }
            auto const address{reinterpret_cast<::std::uintptr_t>(addr)};
            if(address == 0u) [[unlikely]] { return; }
            win64_loaded_sections_.push_back(runtime_llvm_jit_win64_loaded_section{address, size});
        }

        inline constexpr void record_win64_pdata_section(::std::uint8_t* addr,
                                                         ::std::size_t size,
                                                         ::llvm::StringRef section_name) noexcept
        {
            if(addr == nullptr || size == 0uz || section_name != ".pdata") [[unlikely]] { return; }
            win64_pending_pdata_sections_.push_back(runtime_llvm_jit_win64_pending_pdata_section{addr, size, false});
        }

        [[nodiscard]] inline constexpr ::std::uintptr_t get_win64_image_base(::std::uintptr_t fallback) noexcept
        {
            if(win64_image_base_ != (::std::numeric_limits<::std::uintptr_t>::max)()) { return win64_image_base_; }
            for(auto const& section: win64_loaded_sections_)
            {
                if(section.address != 0u && section.address < win64_image_base_) { win64_image_base_ = section.address; }
            }
            if(win64_image_base_ == (::std::numeric_limits<::std::uintptr_t>::max)()) { win64_image_base_ = fallback; }
            return win64_image_base_;
        }

        [[nodiscard]] inline constexpr bool win64_loaded_sections_fit_image_base(::std::uintptr_t image_base) const noexcept
        {
            constexpr auto max_rva{static_cast<::std::uintptr_t>((::std::numeric_limits<::std::uint32_t>::max)())};
            for(auto const& section: win64_loaded_sections_)
            {
                if(section.address < image_base) [[unlikely]] { return false; }
                auto const section_rva{section.address - image_base};
                if(section_rva > max_rva) [[unlikely]] { return false; }
                if(section.size != 0u && section.size - 1u > max_rva - section_rva) [[unlikely]] { return false; }
            }
            return true;
        }

        inline constexpr bool register_win64_seh_function_table(::std::uint8_t* addr, ::std::uint64_t load_addr, ::std::size_t size) noexcept
        {
            if(addr == nullptr || size == 0uz) [[unlikely]] { return false; }
            if(reinterpret_cast<::std::uintptr_t>(addr) % alignof(::fast_io::win32::win_current_runtime_function) != 0u) [[unlikely]] { return false; }
            if(size % sizeof(::fast_io::win32::win_current_runtime_function) != 0uz) [[unlikely]] { return false; }

            auto const count{size / sizeof(::fast_io::win32::win_current_runtime_function)};
            if(count == 0uz || count > static_cast<::std::size_t>((::std::numeric_limits<::std::uint32_t>::max)())) [[unlikely]] { return false; }

            // COFF x64 encodes .pdata RVAs relative to RuntimeDyld's synthetic __ImageBase.  LLVM computes that base as
            // the lowest loaded section address; RtlAddFunctionTable must receive the same value or Windows unwinding
            // will not be able to resolve JIT PCs back to their UNWIND_INFO when code-layout changes move the active PC
            // away from the public entry symbol.
            auto const fallback_base{load_addr == 0u ? reinterpret_cast<::std::uintptr_t>(addr) : static_cast<::std::uintptr_t>(load_addr)};
            auto const image_base{get_win64_image_base(fallback_base)};
            auto const function_table{reinterpret_cast<::fast_io::win32::win_current_runtime_function*>(addr)};
            auto const function_count{static_cast<::std::uint32_t>(count)};
            if(!win64_loaded_sections_fit_image_base(image_base)) [[unlikely]] { return false; }
            for(auto const& record: win64_seh_records_)
            {
                if(record.function_table != function_table) { continue; }
                return record.function_count == function_count && record.image_base == image_base;
            }
            if(!::fast_io::win32::nt::RtlAddFunctionTable(function_table,
                                                          function_count,
                                                          static_cast<::fast_io::win32::win_current_unwind_address>(image_base))) [[unlikely]]
            {
                return false;
            }

            win64_seh_records_.push_back(runtime_llvm_jit_win64_seh_record{function_table, function_count, image_base});
            return true;
        }
# else
        inline constexpr void record_win64_loaded_section(::std::uint8_t*, ::std::uintptr_t) noexcept {}
        inline constexpr void record_win64_pdata_section(::std::uint8_t*, ::std::size_t, ::llvm::StringRef) noexcept {}
# endif

# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_DWARF_EH_FRAME && defined(__APPLE__)
        ::uwvm2::utils::container::vector<runtime_llvm_jit_eh_frame_record> eh_frame_records_{};
# endif
    };
#endif
}  // namespace uwvm2::runtime::compiler::llvm_jit::details

#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_DWARF_EH_FRAME")
#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RESERVE_ALLOC")
#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH")
#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RISCV64_LOW_MAPPER")

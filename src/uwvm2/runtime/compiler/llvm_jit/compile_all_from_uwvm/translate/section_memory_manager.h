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
#include <uwvm2/runtime/lib/uwvm_runtime_posix_abi.h>
#ifndef UWVM_MODULE
# include "arm_ehabi_registration.h"
#endif

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
# if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
#  include <bit>
#  include <vector>
#  include <llvm/DebugInfo/DWARF/DWARFDataExtractor.h>
#  include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>
# endif
#  if defined(_WIN64)
#   include "coff_headers.h"
#  endif
#  include <llvm/ExecutionEngine/SectionMemoryManager.h>
#  if defined(LLVM_VERSION_MAJOR) && LLVM_VERSION_MAJOR >= 23 && !defined(_WIN32) && \
      defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
#   include <llvm/ExecutionEngine/ExecutionEngine.h>
#   include <llvm/ExecutionEngine/JITEventListener.h>
#  endif
#  if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35))
#   include <llvm/ExecutionEngine/ExecutionEngine.h>
#   include <llvm/ExecutionEngine/JITEventListener.h>
#  endif
#  if defined(_WIN64)
#   include <llvm/ExecutionEngine/ExecutionEngine.h>
#   include <llvm/ExecutionEngine/JITEventListener.h>
#  endif
#  if defined(__APPLE__) && defined(__aarch64__)
#   include "macho_headers.h"
#  endif
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT) && defined(__linux__) && defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
#  include <sys/mman.h>
#  include <unistd.h>
#  ifndef MAP_FIXED_NOREPLACE
#   define MAP_FIXED_NOREPLACE 0x100000
#  endif
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT) && !defined(_WIN32) && \
    (!(defined(__arm__) || defined(__thumb__)) || defined(__ARM_DWARF_EH__)) && __has_include(<unwind.h>)
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
#if defined(UWVM_RUNTIME_LLVM_JIT) && !defined(_WIN32) && \
    (!(defined(__arm__) || defined(__thumb__)) || defined(__ARM_DWARF_EH__)) && __has_include(<unwind.h>)
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

#pragma push_macro("UWVM2_RUNTIME_DEBUG_REGISTERED_CFI")
#undef UWVM2_RUNTIME_DEBUG_REGISTERED_CFI
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(LLVM_VERSION_MAJOR) && LLVM_VERSION_MAJOR >= 23 && !defined(_WIN32) && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
# define UWVM2_RUNTIME_DEBUG_REGISTERED_CFI 1
# include "native_debug_cfi.h"
#else
# define UWVM2_RUNTIME_DEBUG_REGISTERED_CFI 0
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::llvm_jit::details
{
#if defined(UWVM_RUNTIME_LLVM_JIT)
# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_RISCV64_LOW_MAPPER
    class runtime_llvm_jit_riscv64_low_address_memory_mapper final : public ::llvm::SectionMemoryManager::MemoryMapper
    {
        [[nodiscard]] inline static ::std::size_t page_size() noexcept
        {
            auto const value{::uwvm2::runtime::lib::posix_abi::sysconf_noexcept(_SC_PAGESIZE)};
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
                             auto mapped{::uwvm2::runtime::lib::posix_abi::mmap_noexcept(requested, size, prot, base_map_flags | MAP_FIXED_NOREPLACE, -1, 0)};
                             if(mapped == requested) { return mapped; }
                             if(mapped != MAP_FAILED)
                             {
                                 // Old kernels may ignore an unknown MAP_FIXED_NOREPLACE bit and treat `hint` as advisory.
                                 // Never return that unrelated mapping to the low-address allocator.
                                 if(::uwvm2::runtime::lib::posix_abi::munmap_noexcept(mapped, size) != 0)
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
                             mapped = ::uwvm2::runtime::lib::posix_abi::mmap_noexcept(requested, size, prot, base_map_flags, -1, 0);
                             if(mapped == requested) { return mapped; }
                             if(mapped == MAP_FAILED) { return MAP_FAILED; }
                             if(::uwvm2::runtime::lib::posix_abi::munmap_noexcept(mapped, size) != 0)
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
            if(::uwvm2::runtime::lib::posix_abi::mprotect_noexcept(block.base(), block.allocatedSize(), static_cast<int>(mmap_prot(flags))) == 0) { return {}; }
            return ::std::error_code(errno, ::std::generic_category());
        }

        inline ::std::error_code releaseMappedMemory(::llvm::sys::MemoryBlock& block) override
        {
            if(block.base() == nullptr || block.allocatedSize() == 0uz) { return {}; }
            if(::uwvm2::runtime::lib::posix_abi::munmap_noexcept(block.base(), block.allocatedSize()) == 0)
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
            // CFI length32 ... end
            // [safe 4 bytes] unsafe (could be end)
            // ^^ curr: end-curr check proved this complete field.
            curr += sizeof(length32);
            // [safe 4 bytes] unsafe (could be end)
            //                ^^ curr may be one-past.
            if(length32 == 0u) { return; }

            ::std::size_t length{};
            ::std::size_t cie_offset_size{sizeof(::std::uint_least32_t)};
            if(length32 == 0xffffffffu)
            {
                if(static_cast<::std::size_t>(end - curr) < sizeof(::std::uint_least64_t)) [[unlikely]] { return; }
                ::std::uint_least64_t length64{};
                ::std::memcpy(::std::addressof(length64), curr, sizeof(length64));
                // CFI length64 ... end
                // [safe 8 bytes] unsafe (could be end)
                // ^^ curr: end-curr check proved this complete field.
                curr += sizeof(length64);
                // [safe 8 bytes] unsafe (could be end)
                //                ^^ curr may be one-past.
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
                // bounded CFI record ... code_end
                // [safe consumed bytes] unsafe (could be code_end)
                // ^^ next: length <= end-curr proved next is within this section.
                curr = next;
                // bounded CFI record ... code_end
                // [safe consumed bytes] unsafe (could be code_end)
                //                       ^^ cursor may be one-past; no read occurs here.
                continue;
            }

            ::std::uint_least64_t cie_offset{};
            ::std::memcpy(::std::addressof(cie_offset), curr, cie_offset_size);
            if(cie_offset != 0u) { visit(record); }
            // bounded CFI record ... code_end
            // [safe consumed bytes] unsafe (could be code_end)
            // ^^ next: length <= end-curr proved next is within this section.
            curr = next;
            // bounded CFI record ... code_end
            // [safe consumed bytes] unsafe (could be code_end)
            //                       ^^ cursor may be one-past; no read occurs here.
        }
    }
# endif

    class runtime_llvm_jit_section_memory_manager final : public ::llvm::SectionMemoryManager
# if UWVM2_RUNTIME_DEBUG_REGISTERED_CFI || UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH || (defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35)))
        , public ::llvm::JITEventListener
# endif
# if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35))
        , private arm_ehabi_registration_owner
# endif
    {
# if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
#  include "native_eh_private_leaf_cfi_observer.h"
# endif
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

# if UWVM2_RUNTIME_DEBUG_REGISTERED_CFI && !(defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35)))
        inline void notifyObjectLoaded(::llvm::ExecutionEngine* engine, ::llvm::object::ObjectFile const& object) override
        {
            debug_native_cfi_.observe_object(object.getArch(), object.getBytesInAddress(), object.isLittleEndian());
            if(object.getArch() != ::llvm::Triple::arm) { return; }
            if(engine == nullptr || (debug_cfi_listener_engine_ != nullptr && debug_cfi_listener_engine_ != engine))
            { debug_native_cfi_.retire(); return; }
            if(debug_cfi_listener_engine_ != nullptr) { return; }
            // Identity borrow only. MCJIT drains its listeners before releasing
            // this manager; our destructor never dereferences the engine.
            debug_cfi_listener_engine_ = engine;
            engine->RegisterJITEventListener(this);
        }
        inline void notifyObjectLoaded(ObjectKey, ::llvm::object::ObjectFile const& object,
            ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) noexcept override
        { debug_native_cfi_.observe_arm_debug_frame(object, loaded); }
# endif

# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
        using ::llvm::SectionMemoryManager::notifyObjectLoaded;

        inline void notifyObjectLoaded(::llvm::ExecutionEngine* engine, ::llvm::object::ObjectFile const&) override
        {
            if(engine == nullptr) { finalization_failure_ = true; return; }
            if(win64_listener_engine_ != nullptr)
            {
                if(win64_listener_engine_ != engine) { finalization_failure_ = true; }
                return;
            }
            // [owning MCJIT engine] [its memory-manager/listener subobject]
            // [safe              ] MCJIT calls this before iterating listeners,
            // so registration also observes the first object. Its lock is recursive.
            // ^^ win64_listener_engine_: borrowed identity only; never dereferenced
            // by our destructor. MCJIT sends freeing events, destroys its listener
            // vector, then releases the owning memory manager (LLVM 20 through 23).
            win64_listener_engine_ = engine;
            engine->RegisterJITEventListener(this);
        }

        inline void notifyObjectLoaded(ObjectKey, ::llvm::object::ObjectFile const& object,
                                       ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) noexcept override
        {
            if(!finalization_failure_ && !capture_win64_object_unwind_metadata(object, loaded))
            { finalization_failure_ = true; }
        }
# endif

# if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35))
        inline void notifyObjectLoaded(::llvm::ExecutionEngine* engine, ::llvm::object::ObjectFile const&) override
        {
            if(engine == nullptr || (arm_ehabi_listener_engine_ != nullptr && arm_ehabi_listener_engine_ != engine))
            { finalization_failure_ = true; return; }
            if(arm_ehabi_listener_engine_ != nullptr) { return; }
            arm_ehabi_listener_engine_ = engine;
            engine->RegisterJITEventListener(this);
        }
        inline void notifyObjectLoaded(ObjectKey, ::llvm::object::ObjectFile const& object,
            ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) noexcept override
        {
# if UWVM2_RUNTIME_DEBUG_REGISTERED_CFI
            debug_native_cfi_.observe_arm_debug_frame(object, loaded);
# endif
            if(!object.isELF() || object.getArch() != ::llvm::Triple::arm || !object.isLittleEndian() || object.getBytesInAddress() != 4u)
            { finalization_failure_ = true; return; }
            // Allocation sizes include RuntimeDyld veneer/padding space. Use
            // the original object's exact extent, never parse those extra bytes
            // as exception entries or let them enlarge a function range.
            for(auto const& section: object.sections())
            {
                auto name{section.getName()};
                if(!name) { ::llvm::consumeError(name.takeError()); finalization_failure_ = true; return; }
                bool const index{*name == ".ARM.exidx" || name->starts_with(".ARM.exidx.")};
                bool const table{*name == ".ARM.extab" || name->starts_with(".ARM.extab.")};
                if(!section.isText() && !index && !table) { continue; }
                auto const size{section.getSize()}, address{loaded.getSectionLoadAddress(section)};
                if(size == 0u) { continue; }
                arm_ehabi_section* pending{};
                for(auto& owned: arm_ehabi_sections_)
                { if(!owned.authenticated && reinterpret_cast<::std::uintptr_t>(owned.address) == address)
                  { if(pending != nullptr) { finalization_failure_ = true; return; } pending = &owned; } }
                if(pending == nullptr || size > pending->allocation_size || pending->code != section.isText() ||
                   pending->index != index || pending->table != table)
                { finalization_failure_ = true; return; }
                pending->size = static_cast<::std::size_t>(size); pending->authenticated = true;
            }
        }
# endif

# if UWVM2_RUNTIME_DEBUG_REGISTERED_CFI && !(defined(__APPLE__) && defined(__aarch64__))
        using ::llvm::SectionMemoryManager::notifyObjectLoaded;
        inline void notifyObjectLoaded(::llvm::RuntimeDyld&, ::llvm::object::ObjectFile const& object) override
        {
            debug_native_cfi_.observe_object(object.getArch(), object.getBytesInAddress(), object.isLittleEndian());
        }
# endif

# if UWVM2_RUNTIME_DEBUG_REGISTERED_CFI
        // Internal metadata query only. The runtime must separately authenticate
        // the real trap, stack copy, code owner, generation and publication.
        [[nodiscard]] bool copy_debug_native_cfi_row(::std::uintptr_t begin, ::std::uintptr_t end,
            ::std::uintptr_t pc, native_debug_cfi_row& out) const noexcept
        {
            out = {};
            return !finalization_failure_ && debug_native_cfi_.copy_row(begin, end, pc, out);
        }
        // Internal scalar CFA metadata for a separately authenticated epilogue.
        // Missing/expressive RA rules remain unavailable to caller recovery.
        [[nodiscard]] bool copy_debug_native_cfa_row(::std::uintptr_t begin, ::std::uintptr_t end,
            ::std::uintptr_t pc, native_debug_cfa_row& out) const noexcept
        {
            out = {};
            native_debug_cfi_row row{};
            if(finalization_failure_ || !debug_native_cfi_.copy_row(begin,end,pc,row,false) || !row.cfa_usable) { return false; }
            out = {row.begin,row.end,row.cfa_register,row.cfa_offset,true};
            return true;
        }
# endif

        [[nodiscard]] inline ::llvm::JITSymbol findSymbol(::std::string const& name) override
        {
            auto const address{this->getSymbolAddress(name)};
            if(address == 0u) { return nullptr; }
            return ::llvm::JITSymbol{address, ::llvm::JITSymbolFlags::Exported};
        }

# if defined(__APPLE__) && defined(__aarch64__)
        using ::llvm::SectionMemoryManager::notifyObjectLoaded;

        inline void notifyObjectLoaded(::llvm::RuntimeDyld& dyld, ::llvm::object::ObjectFile const& object) override
        {
#  if UWVM2_RUNTIME_DEBUG_REGISTERED_CFI
            debug_native_cfi_.observe_object(object.getArch(), object.getBytesInAddress(), object.isLittleEndian());
#  endif
            // LLVM 20's MachO/AArch64 loader resolves SUBTRACTOR/UNSIGNED EH
            // relocations, then its legacy processFDE applies a section delta
            // again. The resulting FDE can name unmapped memory instead of the
            // function. Save the exact relocation expression before that pass,
            // and reapply it immediately before registering CFI. This is also
            // idempotent on LLVM versions that already produce the right value.
            // Do not guess a function from an address range or a frame pointer.
            // https://github.com/llvm/llvm-project/blob/llvmorg-20.1.8/llvm/lib/ExecutionEngine/RuntimeDyld/RuntimeDyldMachO.cpp
            auto const macho{::llvm::dyn_cast<::llvm::object::MachOObjectFile>(&object)};
            if(macho == nullptr || !macho->is64Bit() || !macho->isLittleEndian() || object.getArch() != ::llvm::Triple::aarch64) { return; }
            for(auto const& section: object.sections())
            {
                auto name{section.getName()};
                if(!name) { ::llvm::consumeError(name.takeError()); finalization_failure_ = true; return; }
                if(*name != "__eh_frame") { continue; }
                auto contents{section.getContents()};
                if(!contents) { ::llvm::consumeError(contents.takeError()); finalization_failure_ = true; return; }
                auto const end{section.relocation_end()};
                for(auto it{section.relocation_begin()}; it != end; ++it)
                {
                    if(it->getType() != ::llvm::MachO::ARM64_RELOC_SUBTRACTOR) { continue; }
                    auto const offset{it->getOffset()};
                    auto const sub{macho->getRelocation(it->getRawDataRefImpl())};
                    if(macho->getAnyRelocationLength(sub) != 3u || macho->getAnyRelocationPCRel(sub) ||
                       offset > contents->size() || contents->size() - offset < sizeof(::std::uint64_t))
                    { finalization_failure_ = true; return; }
                    auto const sub_symbol{it->getSymbol()};
                    if(sub_symbol == object.symbol_end()) { finalization_failure_ = true; return; }
                    auto sub_section{sub_symbol->getSection()};
                    if(!sub_section) { ::llvm::consumeError(sub_section.takeError()); finalization_failure_ = true; return; }
                    if(*sub_section == object.section_end() || **sub_section != section) { finalization_failure_ = true; return; }
                    auto sub_name{sub_symbol->getName()};
                    if(!sub_name) { ::llvm::consumeError(sub_name.takeError()); finalization_failure_ = true; return; }
                    if(++it == end || it->getType() != ::llvm::MachO::ARM64_RELOC_UNSIGNED || it->getOffset() != offset)
                    { finalization_failure_ = true; return; }
                    auto const add{macho->getRelocation(it->getRawDataRefImpl())};
                    if(macho->getAnyRelocationLength(add) != 3u || macho->getAnyRelocationPCRel(add))
                    { finalization_failure_ = true; return; }
                    auto const add_symbol{it->getSymbol()};
                    if(add_symbol == object.symbol_end()) { finalization_failure_ = true; return; }
                    auto add_name{add_symbol->getName()};
                    if(!add_name) { ::llvm::consumeError(add_name.takeError()); finalization_failure_ = true; return; }
                    auto const sub_address{dyld.getSymbol(*sub_name).getAddress()};
                    auto const add_address{dyld.getSymbol(*add_name).getAddress()};
                    if(sub_address == 0u || add_address == 0u) { finalization_failure_ = true; return; }
                    auto const loaded{dyld.getSectionContent(dyld.getSymbolSectionID(*sub_name))};
                    if(offset > loaded.size() || loaded.size() - offset < sizeof(::std::uint64_t))
                    { finalization_failure_ = true; return; }
                    ::std::uint64_t addend{};
                    ::std::memcpy(&addend, contents->data() + offset, sizeof(addend));
                    // Mach-O's relocation expression uses modulo-2^64 arithmetic,
                    // including negative addends. The original object is immutable;
                    // no already-relocated bytes feed this calculation.
                    auto const address{reinterpret_cast<::std::uint8_t*>(const_cast<char*>(loaded.data() + offset))};
                    macho_eh_relocations_.push_back(macho_eh_relocation{address, add_address - sub_address + addend});
                }
            }
        }
# endif

        inline constexpr ::std::uint8_t*
            allocateCodeSection(::std::uintptr_t size, unsigned alignment, unsigned section_id, ::llvm::StringRef section_name) noexcept override
        {
            auto const addr{::llvm::SectionMemoryManager::allocateCodeSection(size, alignment, section_id, section_name)};
# if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35))
            record_arm_ehabi_section(addr, size, section_name, true);
# endif
            record_win64_loaded_section(addr, size, true);
            return addr;
        }

        inline constexpr ::std::uint8_t* allocateDataSection(::std::uintptr_t size,
                                                             unsigned alignment,
                                                             unsigned section_id,
                                                             ::llvm::StringRef section_name,
                                                             bool is_read_only) noexcept override
        {
            auto const addr{::llvm::SectionMemoryManager::allocateDataSection(size, alignment, section_id, section_name, is_read_only)};
# if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35))
            record_arm_ehabi_section(addr, size, section_name, false);
# endif
            record_win64_loaded_section(addr, size, false);
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
                    if(!pending.authenticated) { finalization_failure_ = true; break; }
                    if(pending.actual_size == 0uz) { continue; }
                    if(!register_win64_seh_function_table(pending.addr, 0u, pending.actual_size)) [[unlikely]]
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
# if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35))
            if(!finalization_failure_ && !arm_ehabi_published_ && !publish_arm_ehabi_sections())
            { finalization_failure_ = true; }
            if(finalization_failure_ && error_message != nullptr && error_message->empty())
            { *error_message = "failed to register relocated ARM EHABI JIT tables"; }
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
#  if defined(__aarch64__)
            if(finalization_failure_) [[unlikely]] { return; }
            auto const begin{reinterpret_cast<::std::uintptr_t>(addr)};
            for(auto const& relocation: macho_eh_relocations_)
            {
                auto const location{reinterpret_cast<::std::uintptr_t>(relocation.address)};
                if(location < begin || location - begin > size || size - (location - begin) < sizeof(relocation.value)) { continue; }
                ::std::memcpy(relocation.address, &relocation.value, sizeof(relocation.value));
            }
#  endif
            // The Apple unwinder accepts FDE pointers one at a time for JIT code. Keep the original section
            // range so the exact same FDE set can be deregistered before MCJIT releases the underlying memory.
            visit_runtime_llvm_jit_eh_frame_fdes(addr, size, [](::std::uint8_t* fde) constexpr noexcept
            { ::uwvm2::runtime::compiler::llvm_jit::native_unwind_abi::register_frame_noexcept(fde); });
            eh_frame_records_.push_back(runtime_llvm_jit_eh_frame_record{addr, size});
# else
            // Do not infer the registration ABI from the compiler or the presence of <unwind.h>. LLVM's
            // RTDyldMemoryManager selects per-FDE registration for its configured LLVM libunwind backend and
            // whole-section registration for GNU libgcc. Registering every suffix as a libgcc section duplicates
            // overlapping FDE ranges. Delegate this backend choice and its matching ownership to LLVM.
            // https://github.com/llvm/llvm-project/blob/main/llvm/lib/ExecutionEngine/RuntimeDyld/RTDyldMemoryManager.cpp
            ::llvm::SectionMemoryManager::registerEHFrames(addr, load_addr, size);
# endif
# if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
            observe_private_leaf_registered_eh_frames(addr, load_addr, size);
# endif
# if UWVM2_RUNTIME_DEBUG_REGISTERED_CFI
            if(!finalization_failure_) { debug_native_cfi_.observe(addr, load_addr, size); }
# endif
        }

        inline constexpr void deregisterEHFrames() noexcept override
        {
# if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35))
            retire_arm_ehabi_tables(); arm_ehabi_published_ = false;
            arm_ehabi_sections_.clear();
# endif
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
            win64_loaded_sections_.clear();
            win64_image_base_ = (::std::numeric_limits<::std::uintptr_t>::max)();
# elif UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_DWARF_EH_FRAME && defined(__APPLE__)
            for(auto const& frame: eh_frame_records_)
            {
                visit_runtime_llvm_jit_eh_frame_fdes(frame.addr, frame.size, [](::std::uint8_t* fde) constexpr noexcept
                { ::uwvm2::runtime::compiler::llvm_jit::native_unwind_abi::deregister_frame_noexcept(fde); });
            }
            eh_frame_records_.clear();
# else
            ::llvm::SectionMemoryManager::deregisterEHFrames();
# endif
# if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
            private_leaf_cfi_valid_ = false;
            private_leaf_fde_ranges_.clear();
# endif
# if UWVM2_RUNTIME_DEBUG_REGISTERED_CFI
            debug_native_cfi_.retire();
# endif
        }

    private:
# if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35))
        struct arm_ehabi_section
        { ::std::uint8_t* address{}; ::std::size_t size{}, allocation_size{}; bool code{}, index{}, table{}, authenticated{}; };
        ::std::vector<arm_ehabi_section> arm_ehabi_sections_{};
        bool arm_ehabi_published_{};
        ::llvm::ExecutionEngine* arm_ehabi_listener_engine_{}; // identity only; MCJIT drains listeners before releasing this manager.
        void record_arm_ehabi_section(::std::uint8_t* address, ::std::uintptr_t size,
            ::llvm::StringRef name, bool code) noexcept
        {
            if(address == nullptr || size > UINTPTR_MAX - reinterpret_cast<::std::uintptr_t>(address))
            { finalization_failure_ = true; return; }
            if(size == 0u) { return; }
            bool const index{name == ".ARM.exidx" || name.starts_with(".ARM.exidx.")};
            bool const table{name == ".ARM.extab" || name.starts_with(".ARM.extab.")};
            arm_ehabi_sections_.push_back({address, 0u, static_cast<::std::size_t>(size), code, index, table, false});
            arm_ehabi_published_ = false;
        }
        [[nodiscard]] bool publish_arm_ehabi_sections()
        {
            if(!arm_ehabi_lookup_is_exported()) { return false; }
            for(auto const& section: arm_ehabi_sections_)
            { if((section.code || section.index || section.table) && !section.authenticated) { return false; } }
            ::std::vector<arm_ehabi_owned_table> tables{};
            for(auto const& section: arm_ehabi_sections_)
            {
                if(!section.index) { continue; }
                if(section.code || section.size % 8u != 0u || section.size / 8u > static_cast<unsigned>(INT_MAX) ||
                   reinterpret_cast<::std::uintptr_t>(section.address) % 4u != 0u) { return false; }
                arm_ehabi_owned_table table{reinterpret_cast<::std::uint32_t const*>(section.address),
                    static_cast<unsigned>(section.size / 8u), {}};
                ::std::uintptr_t previous{};
                for(unsigned i{}; i != table.count; ++i)
                {
                    auto const* entry{table.entries + i * 2u};
                    auto const start{arm_ehabi_prel31(entry) & ~::std::uintptr_t{1u}};
                    if(start == 0u || (i != 0u && start <= previous)) { return false; }
                    previous = start;
                    arm_ehabi_section const* allocation{};
                    for(auto const& owned: arm_ehabi_sections_)
                    { if(owned.code && start >= reinterpret_cast<::std::uintptr_t>(owned.address) &&
                         start - reinterpret_cast<::std::uintptr_t>(owned.address) < owned.size)
                      { if(allocation != nullptr) { return false; } allocation = &owned; } }
                    if(allocation == nullptr) { return false; }
                    auto end{reinterpret_cast<::std::uintptr_t>(allocation->address) + allocation->size};
                    if(i + 1u != table.count)
                    { auto const next{arm_ehabi_prel31(entry + 2u) & ~::std::uintptr_t{1u}};
                      if(next <= start) { return false; } end = (::std::min)(end, next); }
                    ::std::uint32_t content{}; ::std::memcpy(&content, entry + 1u, sizeof(content));
                    if(content != 1u && (content & 0x80000000u) == 0u)
                    {
                        auto const extab{arm_ehabi_prel31(entry + 1u)}; bool bounded{};
                        for(auto const& owned: arm_ehabi_sections_)
                        { auto const begin{reinterpret_cast<::std::uintptr_t>(owned.address)};
                          if(owned.table && extab >= begin && extab - begin <= owned.size &&
                             owned.size - (extab - begin) >= 4u) { bounded = true; } }
                        if(!bounded || extab % 4u != 0u) { return false; }
                    }
                    table.functions.push_back({start, end - start});
                }
                tables.push_back(::std::move(table));
            }
            if(!publish_arm_ehabi_tables(::std::move(tables))) { return false; }
            arm_ehabi_published_ = true; return true;
        }
# endif
        bool finalization_failure_{};
# if UWVM2_RUNTIME_DEBUG_REGISTERED_CFI
        native_debug_registered_cfi debug_native_cfi_{};
        ::llvm::ExecutionEngine* debug_cfi_listener_engine_{}; // identity only; no destructor access
# endif

# if defined(__APPLE__) && defined(__aarch64__)
        struct macho_eh_relocation
        {
            ::std::uint8_t* address{};
            ::std::uint64_t value{};
        };
        // Load-time bookkeeping only: it changes neither generated instructions
        // nor per-call overhead, and remains owned by the engine with its CFI.
        ::uwvm2::utils::container::vector<macho_eh_relocation> macho_eh_relocations_{};
# endif

# if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_WIN64_SEH
        struct runtime_llvm_jit_win64_loaded_section
        {
            ::std::uintptr_t address{};
            ::std::uintptr_t size{};
            ::std::uintptr_t object_size{};
            bool is_code{};
            bool is_xdata{};
            bool authenticated{};
        };

        struct runtime_llvm_jit_win64_seh_record
        {
            ::fast_io::win32::win_current_runtime_function* function_table{};
            ::std::uint32_t function_count{};
            ::std::uintptr_t image_base{};
        };

        struct runtime_llvm_jit_win64_pdata_entry
        {
            ::std::uint8_t const* row{};
            ::std::uintptr_t targets[3]{};
            ::std::uint32_t packed_unwind{};
            ::std::uint_least8_t fields{};
        };

        struct runtime_llvm_jit_win64_pending_pdata_section
        {
            ::std::uint8_t* addr{};
            ::std::size_t allocation_size{};
            ::std::size_t actual_size{};
            ::uwvm2::utils::container::vector<runtime_llvm_jit_win64_pdata_entry> entries{};
            bool registration_attempted{};
            bool authenticated{};
        };

        ::uwvm2::utils::container::vector<runtime_llvm_jit_win64_loaded_section> win64_loaded_sections_{};
        ::uwvm2::utils::container::vector<runtime_llvm_jit_win64_seh_record> win64_seh_records_{};
        ::uwvm2::utils::container::vector<runtime_llvm_jit_win64_pending_pdata_section> win64_pending_pdata_sections_{};
        ::std::uintptr_t win64_image_base_{(::std::numeric_limits<::std::uintptr_t>::max)()};
        ::llvm::ExecutionEngine* win64_listener_engine_{}; // identity borrow; no destructor engine access

        inline constexpr void record_win64_loaded_section(::std::uint8_t* addr, ::std::uintptr_t size, bool is_code) noexcept
        {
            // Allocation extents include reserved stubs/padding. Only object-specific
            // LoadedObjectInfo can authenticate the logical .text/.xdata/.pdata spans.
            if(addr == nullptr || size == 0uz) [[unlikely]] { return; }
            auto const address{reinterpret_cast<::std::uintptr_t>(addr)};
            if(address == 0u || size > (::std::numeric_limits<::std::uintptr_t>::max)() - address) [[unlikely]]
            { finalization_failure_ = true; return; }
            win64_loaded_sections_.push_back(runtime_llvm_jit_win64_loaded_section{address, size, 0u, is_code, false, false});
        }

        inline constexpr void record_win64_pdata_section(::std::uint8_t* addr,
                                                         ::std::size_t size,
                                                         ::llvm::StringRef section_name) noexcept
        {
            // COFF also emits per-COMDAT .pdata$<symbol> unwind tables. RuntimeDyld only calls registerEHFrames for
            // the unsuffixed .pdata section, so retain every allocated table for post-relocation finalizeMemory.
            if(addr == nullptr || size == 0uz ||
               (section_name != ".pdata" && !section_name.starts_with(".pdata$"))) [[unlikely]] { return; }
            win64_pending_pdata_sections_.push_back(runtime_llvm_jit_win64_pending_pdata_section{addr, size});
        }

        [[nodiscard]] inline static bool win64_xdata_name(::llvm::StringRef name) noexcept
        {
            return name == ".xdata" || name.starts_with(".xdata$");
        }

        [[nodiscard]] inline static ::std::uint32_t win64_read_u32(::std::uint8_t const* field) noexcept
        {
            // The caller authenticates a complete four-byte field in a retained
            // little-endian native COFF allocation before forming this borrow.
            ::std::uint32_t value{};
            ::std::memcpy(::std::addressof(value), field, sizeof(value));
            return value;
        }

        [[nodiscard]] inline ::std::uint8_t const* win64_owned_span(
            ::std::uintptr_t address, ::std::uintptr_t size, bool is_code, bool is_xdata) const noexcept
        {
            for(auto const& section: win64_loaded_sections_)
            {
                if(!section.authenticated || section.is_code != is_code ||
                   (is_xdata && !section.is_xdata) || address < section.address) { continue; }
                auto const offset{address - section.address};
                if(offset > section.object_size || size > section.object_size - offset) { continue; }
                // [live allocation ... checked object span ... reserved stubs]
                // [safe                                                      ]
                // ^^ result: authenticate the real allocation first; offset/size
                // stay in the logical object bytes. No integer token is read as a pointer.
                return reinterpret_cast<::std::uint8_t const*>(section.address) + offset;
            }
            return nullptr;
        }

        [[nodiscard]] inline bool capture_win64_object_unwind_metadata(
            ::llvm::object::ObjectFile const& object, ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) noexcept
        {
            auto const coff{::llvm::dyn_cast<::llvm::object::COFFObjectFile>(::std::addressof(object))};
            // [callback-owned immutable COFF object] dyn_cast borrows only during
            // [safe                               ] this synchronous callback.
            // ^^ coff: never retain ObjectFile/SectionRef/SymbolRef beyond notification.
            if(coff == nullptr || !object.isLittleEndian()) { return false; }
#  if defined(__aarch64__) || defined(_M_ARM64)
            if(coff->getMachine() != ::uwvm2::runtime::compiler::llvm_jit::win64_coff_constants::machine_arm64) { return false; }
#  else
            if(coff->getMachine() != ::uwvm2::runtime::compiler::llvm_jit::win64_coff_constants::machine_amd64) { return false; }
#  endif
            constexpr auto host_limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
            constexpr auto row_size{sizeof(::fast_io::win32::win_current_runtime_function)};
            static_assert(row_size == 8uz || row_size == 12uz);

            // First authenticate exact logical section spans; object-relative
            // symbol targets may refer forward to another section in this object.
            for(auto const& section: object.sections())
            {
                auto const address{loaded.getSectionLoadAddress(section)};
                if(address == 0u) { continue; } // unallocated debug/empty sections
                auto const size{section.getSize()};
                auto name{section.getName()};
                if(!name) { ::llvm::consumeError(name.takeError()); return false; }
                if(address > host_limit || size > host_limit - address) { return false; }
                bool found{};
                for(auto& allocation: win64_loaded_sections_)
                {
                    if(allocation.address != address) { continue; }
                    if(size > allocation.size || allocation.is_code != section.isText() ||
                       (allocation.authenticated && (allocation.object_size != size || allocation.is_xdata != win64_xdata_name(*name))))
                    { return false; }
                    allocation.object_size = static_cast<::std::uintptr_t>(size);
                    allocation.is_xdata = win64_xdata_name(*name);
                    allocation.authenticated = true;
                    found = true;
                    break;
                }
                if(!found) { return false; } // no remote/remapped or foreign-memory table
            }

            for(auto const& section: object.sections())
            {
                auto name{section.getName()};
                if(!name) { ::llvm::consumeError(name.takeError()); return false; }
                if(*name != ".pdata" && !name->starts_with(".pdata$")) { continue; }
                auto const address{loaded.getSectionLoadAddress(section)};
                if(address == 0u) { continue; }
                auto const size{section.getSize()};
                if(size > host_limit || size % row_size != 0u) { return false; }
                auto contents{section.getContents()};
                if(!contents) { ::llvm::consumeError(contents.takeError()); return false; }
                if(contents->size() != size) { return false; }
                runtime_llvm_jit_win64_pending_pdata_section* pending{};
                for(auto& candidate: win64_pending_pdata_sections_)
                {
                    if(reinterpret_cast<::std::uintptr_t>(candidate.addr) != address) { continue; }
                    // [live pending vector element] [its real retained allocation]
                    // [safe                       ] this borrow ends before any
                    // vector growth; LoadedObjectInfo only reads existing mappings.
                    // ^^ pending: matched by exact allocated address, not a symbol name.
                    pending = ::std::addressof(candidate);
                    break;
                }
                if(pending == nullptr || pending->authenticated || size > pending->allocation_size ||
                   win64_owned_span(static_cast<::std::uintptr_t>(address), static_cast<::std::uintptr_t>(size), false, false) == nullptr)
                { return false; }
                pending->actual_size = static_cast<::std::size_t>(size);
                auto const count{pending->actual_size / row_size};
                if(count > static_cast<::std::size_t>((::std::numeric_limits<::std::uint32_t>::max)())) { return false; }
                pending->entries.resize(count);
                for(::std::size_t index{}; index != count; ++index)
                {
                    auto& entry{pending->entries.index_unchecked(index)};
                    auto const offset{index * row_size}; // index<count bounds multiplication within actual_size
                    // [pdata allocation ... row ... actual end ... stub capacity]
                    // [safe                                                   ]
                    // ^^ entry.row: row_size complete bytes lie in actual_size;
                    // the allocation remains engine-owned through deregistration.
                    entry.row = pending->addr + offset;
                    if constexpr(row_size == 8uz)
                    {
                        // [immutable object contents ... row word 1 ... end]
                        // [safe                                           ] offset+8<=size.
                        // ^^ packed_field: a temporary original-byte borrow, never relocated input.
                        auto const packed_field{reinterpret_cast<::std::uint8_t const*>(contents->data()) + offset + 4uz};
                        entry.packed_unwind = win64_read_u32(packed_field);
                        if((entry.packed_unwind & 3u) == 3u) { return false; }
                    }
                }

                for(auto const& relocation: section.relocations())
                {
                    auto const offset{relocation.getOffset()};
#  if defined(__aarch64__) || defined(_M_ARM64)
                    if(relocation.getType() != ::uwvm2::runtime::compiler::llvm_jit::win64_coff_constants::relocation_arm64_addr32nb) { return false; }
#  else
                    if(relocation.getType() != ::uwvm2::runtime::compiler::llvm_jit::win64_coff_constants::relocation_amd64_addr32nb) { return false; }
#  endif
                    if(offset % 4u != 0u || offset > size || size - offset < 4u) { return false; }
                    auto const index{static_cast<::std::size_t>(offset / row_size)};
                    auto const field{static_cast<::std::size_t>((offset % row_size) / 4u)};
                    // [entries.begin ... index<count ... end] offset+4<=size
                    // [safe                                ] proves index and field.
                    auto& entry{pending->entries.index_unchecked(index)};
                    auto const mask{static_cast<::std::uint_least8_t>(1u << field)};
                    if((entry.fields & mask) != 0u ||
                       (row_size == 8uz && field == 1uz && (entry.packed_unwind & 3u) != 0u)) { return false; }
                    auto const symbol{relocation.getSymbol()};
                    // [object symbol ... symbol_end] validate the sentinel before
                    // [safe                       ] dereferencing its metadata.
                    if(symbol == object.symbol_end()) { return false; }
                    auto target_section{symbol->getSection()};
                    if(!target_section) { ::llvm::consumeError(target_section.takeError()); return false; }
                    if(*target_section == object.section_end()) { return false; }
                    auto value{symbol->getValue()};
                    if(!value) { ::llvm::consumeError(value.takeError()); return false; }
                    auto target_name{(*target_section)->getName()};
                    if(!target_name) { ::llvm::consumeError(target_name.takeError()); return false; }
                    bool const is_end{row_size == 12uz && field == 1uz};
                    bool const is_code{field == 0uz || is_end};
                    if((*target_section)->isText() != is_code || (!is_code && !win64_xdata_name(*target_name))) { return false; }
                    auto const target_size{(*target_section)->getSize()};
                    // [immutable pdata object ... offset ... offset+4<=size]
                    // [safe                                                 ]
                    // ^^ original_field: read the original ADDR32NB addend, never
                    // an already relocated word or RuntimeDyld's global-symbol alias.
                    auto const original_field{reinterpret_cast<::std::uint8_t const*>(contents->data()) + static_cast<::std::size_t>(offset)};
                    auto const addend{win64_read_u32(original_field)};
                    if(*value > target_size || addend > target_size - *value) { return false; }
                    auto const target_offset{*value + addend};
                    if(!is_end && target_offset == target_size) { return false; }
                    auto const target_address{loaded.getSectionLoadAddress(**target_section)};
                    if(target_address == 0u || target_address > host_limit || target_size > host_limit - target_address ||
                       win64_owned_span(static_cast<::std::uintptr_t>(target_address), static_cast<::std::uintptr_t>(target_size), is_code, !is_code) == nullptr)
                    { return false; }
                    // [exact loaded object section + checked symbol/addend offset]
                    // [safe                                                     ]
                    // ^^ entry.targets[field]: copy a host-width integer anchor;
                    // no ObjectFile borrow or raw guest pointer escapes this callback.
                    entry.targets[field] = static_cast<::std::uintptr_t>(target_address + target_offset);
                    entry.fields = static_cast<::std::uint_least8_t>(entry.fields | mask);
                }
                for(auto const& entry: pending->entries)
                {
                    auto const expected{row_size == 12uz ? 7u : ((entry.packed_unwind & 3u) == 0u ? 3u : 1u)};
                    if(entry.fields != expected) { return false; }
                }
                pending->authenticated = true;
            }
            return true;
        }

        [[nodiscard]] inline bool get_win64_image_base(::std::uintptr_t& image_base) noexcept
        {
            constexpr auto missing{(::std::numeric_limits<::std::uintptr_t>::max)()};
            constexpr auto row_size{sizeof(::fast_io::win32::win_current_runtime_function)};
            auto candidate{missing};
            for(auto const& pending: win64_pending_pdata_sections_)
            {
                if(!pending.authenticated) { return false; }
                for(auto const& entry: pending.entries)
                {
                    for(::std::size_t field{}; field != row_size / 4uz; ++field)
                    {
                        if((entry.fields & (1u << field)) == 0u) { continue; }
                        // [authenticated pdata row ... four-byte RVA field ... row end]
                        // [safe                                                      ]
                        // ^^ field_address: field<row_size/4 keeps the borrow inside
                        // the original logical table, after RuntimeDyld relocation.
                        auto const field_address{entry.row + field * 4uz};
                        auto const rva{win64_read_u32(field_address)};
                        auto const target{entry.targets[field]};
                        if(target < rva) { return false; }
                        auto const base{target - rva};
                        if(base == 0u || base == missing || (candidate != missing && candidate != base)) { return false; }
                        candidate = base;
                    }
                }
            }
            if(candidate == missing || (win64_image_base_ != missing && win64_image_base_ != candidate)) { return false; }
            bool owned_base{};
            for(auto const& allocation: win64_loaded_sections_)
            {
                if(allocation.address == candidate) { owned_base = true; break; }
            }
            // RuntimeDyld's cached base is a real loaded section start, never a
            // guessed intermediate address produced by a truncated RVA equation.
            if(!owned_base) { return false; }
            // Only independent, object-specific relocation equations establish
            // this value. Later objects must corroborate the same cached base;
            // the current allocation minimum or .pdata address is never a fallback.
            win64_image_base_ = candidate;
            image_base = candidate;
            return true;
        }

        [[nodiscard]] inline bool validate_win64_pdata_entries(runtime_llvm_jit_win64_pending_pdata_section const& pending) const noexcept
        {
            constexpr auto row_size{sizeof(::fast_io::win32::win_current_runtime_function)};
            ::std::uintptr_t previous_end{};
            for(auto const& entry: pending.entries)
            {
                auto const begin{entry.targets[0]};
                ::std::uintptr_t length{};
                if constexpr(row_size == 12uz)
                {
                    auto const end{entry.targets[1]};
                    if(end <= begin) { return false; }
                    length = end - begin;
                    if((entry.targets[2] & 3u) != 0u || win64_owned_span(entry.targets[2], 4u, false, true) == nullptr) { return false; }
                }
                else
                {
                    // https://learn.microsoft.com/en-us/cpp/build/arm64-exception-handling
                    // Packed ARM64 records carry a length, not an .xdata RVA.
                    // Flag 3 is reserved; unpacked records use the real .xdata header.
                    auto const packed{entry.packed_unwind};
                    // [authenticated eight-byte row ... second word ... row end]
                    // [safe                                                   ]
                    // ^^ unwind_field: word 1 fits in the same retained real row.
                    auto const unwind_field{entry.row + 4uz};
                    if((packed & 3u) != 0u)
                    {
                        if(win64_read_u32(unwind_field) != packed) { return false; }
                        length = static_cast<::std::uintptr_t>((packed >> 2u) & 0x7ffu) * 4u;
                    }
                    else
                    {
                        if((entry.targets[1] & 3u) != 0u) { return false; }
                        auto const header{win64_owned_span(entry.targets[1], 4u, false, true)};
                        // [authenticated xdata allocation ... header ... logical end]
                        // [safe                                                    ]
                        // ^^ header: nullable until the entire four-byte span is proved.
                        if(header == nullptr) { return false; }
                        auto const word{win64_read_u32(header)};
                        if(((word >> 18u) & 3u) != 0u) { return false; }
                        length = static_cast<::std::uintptr_t>(word & 0x3ffffu) * 4u;
                    }
                }
                if(length == 0u || length > (::std::numeric_limits<::std::uintptr_t>::max)() - begin ||
                   begin < previous_end || win64_owned_span(begin, length, true, false) == nullptr) { return false; }
                previous_end = begin + length; // integer-only checked exclusive end; sorted/nonoverlapping table
            }
            return true;
        }

        inline bool register_win64_seh_function_table(::std::uint8_t* addr, ::std::uint64_t load_addr, ::std::size_t size) noexcept
        {
            if(addr == nullptr || (load_addr != 0u && load_addr != reinterpret_cast<::std::uintptr_t>(addr))) [[unlikely]] { return false; }
            runtime_llvm_jit_win64_pending_pdata_section const* pending{};
            for(auto const& candidate: win64_pending_pdata_sections_)
            {
                if(candidate.addr != addr) { continue; }
                // [live pending vector element] no allocation/mutation until this
                // [safe                       ] cold registration borrow ends.
                // ^^ pending: only a matched, object-authenticated allocation may
                // be passed to Windows; a callback alone does not establish size.
                pending = ::std::addressof(candidate);
                break;
            }
            if(pending == nullptr || !pending->authenticated || size != pending->actual_size) [[unlikely]] { return false; }
            if(size == 0uz) { return true; } // real empty object section, no table to register
            if(reinterpret_cast<::std::uintptr_t>(addr) % alignof(::fast_io::win32::win_current_runtime_function) != 0u) [[unlikely]] { return false; }
            if(size % sizeof(::fast_io::win32::win_current_runtime_function) != 0uz) [[unlikely]] { return false; }

            auto const count{size / sizeof(::fast_io::win32::win_current_runtime_function)};
            if(count == 0uz || count > static_cast<::std::size_t>((::std::numeric_limits<::std::uint32_t>::max)())) [[unlikely]] { return false; }

            // RuntimeDyld caches its synthetic base at the first ADDR32NB, which
            // can precede later object allocations. Derive the actual cached base
            // from all authenticated relocated fields, never from today's minimum.
            ::std::uintptr_t image_base{};
            if(!get_win64_image_base(image_base) || !validate_win64_pdata_entries(*pending)) [[unlikely]] { return false; }
            // [authenticated actual pdata bytes] [complete aligned runtime table]
            // [safe                            ] Windows borrows this engine-owned
            // allocation only until successful RtlDeleteFunctionTable at retirement.
            // ^^ function_table: reinterpretation does not widen the logical table;
            // count excludes the allocation's unused stub/padding capacity.
            auto const function_table{reinterpret_cast<::fast_io::win32::win_current_runtime_function*>(addr)};
            auto const function_count{static_cast<::std::uint32_t>(count)};
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
        inline constexpr void record_win64_loaded_section(::std::uint8_t*, ::std::uintptr_t, bool) noexcept {}
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

#pragma pop_macro("UWVM2_RUNTIME_DEBUG_REGISTERED_CFI")

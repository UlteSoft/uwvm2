/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include <algorithm>
#include <vector>
#include <cstdint>
#include <limits>
#include <memory>
#include <llvm/ADT/StringMap.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/macho_headers.h>
#include <llvm/DebugInfo/DIContext.h>
#include <llvm/Object/MachO.h>
#include <llvm/Support/MemoryBuffer.h>

namespace uwvm2::runtime::lib::details::native_loaded_provenance
{
    // LLVM's generic DWARF relocation resolver does not implement Mach-O
    // ARM64 relocations. Copy only DWARF sections, then relocate their absolute
    // addresses using each actual loaded section. Never apply one image slide:
    // RuntimeDyld allocates code and data independently. Unsupported records
    // invalidate this observation instead of manufacturing source provenance.
    [[nodiscard]] inline bool copy_macho_dwarf(::llvm::object::MachOObjectFile const& object,
        ::llvm::LoadedObjectInfo const& loaded,
        ::llvm::StringMap<::std::unique_ptr<::llvm::MemoryBuffer>>& sections)
    {
        if(!object.isLittleEndian() || !object.is64Bit()) { return false; }
        auto const cpu{object.getArch()};
        if(cpu != ::llvm::Triple::aarch64 && cpu != ::llvm::Triple::x86_64) { return false; }
        ::std::size_t total{};
        for(auto const& section : object.sections())
        {
            auto name{section.getName()};
            if(!name) { ::llvm::consumeError(name.takeError()); return false; }
            if(!name->starts_with("__debug_")) { continue; }
            if(section.getSize() > 64u * 1024u * 1024u - total) { return false; }
            total += section.getSize();
            auto bytes{section.getContents()};
            if(!bytes) { ::llvm::consumeError(bytes.takeError()); return false; }
            if(bytes->size() != section.getSize()) { return false; }
            auto copy{::llvm::WritableMemoryBuffer::getNewMemBuffer(bytes->size())};
            if(!copy) { return false; }
            ::std::copy(bytes->begin(), bytes->end(), copy->getBufferStart());
            ::std::vector<::std::pair<::std::uint64_t, unsigned>> patched{};
            for(auto const& relocation : section.relocations())
            {
                auto const raw{relocation.getRawDataRefImpl()};
                auto const info{object.getRelocation(raw)};
                // Both supported 64-bit Mach-O targets use type 0 for UNSIGNED.
                if(object.isRelocationScattered(info) || relocation.getType() != 0u ||
                   object.getAnyRelocationPCRel(info)) { return false; }
                auto const length{object.getRelocationLength(raw)};
                if(length != 2u && length != 3u) { return false; }
                unsigned const width{1u << length};
                auto const offset{relocation.getOffset()};
                if(offset > bytes->size() || width > bytes->size() - offset) { return false; }
                for(auto const& old : patched)
                { if(offset < old.first + old.second && old.first < offset + width) { return false; } }
                patched.emplace_back(offset, width);
                auto target{object.section_end()};
                ::std::uint64_t symbol_address{};
                bool const external{object.getPlainRelocationExternal(info)};
                if(external)
                {
                    auto symbol{relocation.getSymbol()};
                    if(symbol == object.symbol_end()) { return false; }
                    auto address{symbol->getAddress()}; auto owner{symbol->getSection()};
                    if(!address || !owner)
                    {
                        if(!address) { ::llvm::consumeError(address.takeError()); }
                        if(!owner) { ::llvm::consumeError(owner.takeError()); }
                        return false;
                    }
                    symbol_address = *address; target = *owner;
                }
                else { target = object.getRelocationSection(raw); }
                if(target == object.section_end()) { return false; }
                ::std::uint64_t value{};
                for(unsigned i{}; i != width; ++i)
                { value |= static_cast<::std::uint64_t>(static_cast<unsigned char>((*bytes)[offset + i])) << (8u * i); }
                constexpr auto limit{(::std::numeric_limits<::std::uint64_t>::max)()};
                if(external)
                { if(symbol_address > limit - value) { return false; } value += symbol_address; }
                auto const original{target->getAddress()};
                if(value < original || value - original > target->getSize()) { return false; }
                value -= original;
                // DWARF-to-DWARF references are section offsets, not live pointers.
                auto target_name{target->getName()};
                if(!target_name) { ::llvm::consumeError(target_name.takeError()); return false; }
                auto const base{target_name->starts_with("__debug_") ? 0u : loaded.getSectionLoadAddress(*target)};
                if((base == 0u && !target_name->starts_with("__debug_")) || value > limit - base) { return false; }
                value += base;
                if(width == 4u && value > UINT32_MAX) { return false; }
                for(unsigned i{}; i != width; ++i)
                { copy->getBufferStart()[offset + i] = static_cast<char>(value >> (8u * i)); }
            }
            if(!sections.try_emplace(object.mapDebugSectionName(name->drop_front(2u)), ::std::move(copy)).second) { return false; }
        }
        return true;
    }
}

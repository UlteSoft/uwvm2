/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)            *
 * Licensed under the APL-2.0 License (see LICENSE file).      *
 *************************************************************/
#pragma once
#include <cstdint>
#include <limits>
#include "uwvm_runtime_ppc64_elfv1_loaded_function_range.h"
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <llvm/ExecutionEngine/RuntimeDyld.h>
# include <llvm/Object/ObjectFile.h>
# include <llvm/Object/SymbolSize.h>
# include <llvm/Support/Error.h>

namespace uwvm2::runtime::lib::details
{
    // Object-relative symbols remain valid for cached and parallel objects. Use
    // RuntimeDyld's section relocation, never an IR address or an unmangled-name
    // lookup: private host-tail adapters are native functions too. LLVM reads ELF
    // symbol sizes and computes COFF/Mach-O extents from all object symbols.
    template<typename Emit>
    inline void for_each_llvm_jit_loaded_function_range(
        ::llvm::object::ObjectFile const& object,
        ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded, Emit&& emit)
    {
        // Cold symbol-size scratch and the selected receiver may throw. The
        // pending listener owns its recovery boundary; do not terminate before
        // that receiver can invalidate a partially observed private object.
        ::std::optional<ppc64_elfv1_relocation_index> descriptor_index{};
        for(auto const& symbol_size: ::llvm::object::computeSymbolSizes(object))
        {
            auto const& symbol{symbol_size.first};
            auto type{symbol.getType()};
            if(!type) { ::llvm::consumeError(type.takeError()); continue; }
            if(*type != ::llvm::object::SymbolRef::ST_Function || symbol_size.second == 0u) { continue; }
            auto section_result{symbol.getSection()};
            if(!section_result) { ::llvm::consumeError(section_result.takeError()); continue; }
            auto const section{*section_result};
            // [object-owned section iterator] [section_end]
            // [safe                        ] check the sentinel before dereference;
            // the callback only copies integers, never retaining this borrow.
            if(section == object.section_end()) { continue; }
            if(!section->isText())
            {
                // Real ELFv1 descriptors are non-text STT_FUNC. Their own
                // object relocation and exact declared body extent can name
                // loaded text; never read an arbitrary descriptor integer.
                if(!descriptor_index) { descriptor_index.emplace(object); }
                auto const entry{get_ppc64_elfv1_loaded_function_range(object, symbol, loaded, symbol_size.second,*descriptor_index)};
                if(entry.begin != 0u) { emit(entry.begin, entry.size); }
                continue;
            }
            auto address{symbol.getAddress()};
            if(!address) { ::llvm::consumeError(address.takeError()); continue; }
            auto const section_address{section->getAddress()};
            auto const section_size{section->getSize()};
            if(*address < section_address) { continue; }
            auto const offset{*address - section_address};
            if(offset >= section_size || symbol_size.second > section_size - offset) { continue; }
            auto const load_address{loaded.getSectionLoadAddress(*section)};
            constexpr auto limit{::std::numeric_limits<::std::uintptr_t>::max()};
            if(load_address == 0u || load_address > limit || offset > limit - load_address) { continue; }
            auto const begin{load_address + offset};
            if(symbol_size.second > limit - begin) { continue; }
            // [loaded text + checked offset ... checked function end]
            // [safe                                                ] integer-only
            // relocation stays within the loaded section; no code is dereferenced.
            emit(static_cast<::std::uintptr_t>(begin), static_cast<::std::uintptr_t>(symbol_size.second));
        }
    }
}
#endif

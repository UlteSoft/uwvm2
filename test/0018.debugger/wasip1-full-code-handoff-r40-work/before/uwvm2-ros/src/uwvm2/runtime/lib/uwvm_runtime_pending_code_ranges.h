/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "uwvm_runtime_loaded_function_ranges.h"
#include "uwvm_runtime_native_provenance_rows.h"
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <llvm/ExecutionEngine/ExecutionEngine.h>
# include <llvm/ExecutionEngine/JITEventListener.h>
# include <llvm/ExecutionEngine/RuntimeDyld.h>
# include <llvm/Object/ObjectFile.h>
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/elf_headers.h>
# include <llvm/Object/ELFObjectFile.h>
#endif

namespace uwvm2::runtime::lib::details
{
    // Cold full/T2 materialization transaction. Failed symbol resolution must
    // not leave executable ranges belonging to the abandoned engine in the
    // process diagnostic map. CFI registration remains the memory manager's job.
    class pending_llvm_jit_code_ranges final : public ::llvm::JITEventListener
    {
        struct range { ::std::uintptr_t begin{}, size{}; };
        ::std::vector<range> pending{};
        ::std::vector<range> pending_functions{};
        native_loaded_provenance::image debug_native_rows_{};
        bool debug_full_capture_{}, committed_{}, observation_failed_{};
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
        // Explicit ELF st_size only. Ordinary diagnostic ranges may instead
        // use computeSymbolSizes' COFF/Mach-O next-symbol estimates.
        ::std::vector<range> private_explicit_elf_functions_{};
        bool private_explicit_elf_enabled_{}, private_explicit_elf_valid_{};
        void observe_private_explicit_elf_symbols(::llvm::object::ObjectFile const& object,
            ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) noexcept
        {
            if(!private_explicit_elf_enabled_ || !private_explicit_elf_valid_) { return; }
#ifdef UWVM_CPP_EXCEPTIONS
            try // Own cold observation only; original diagnostic callback stays unchanged.
            {
                auto const elf{::llvm::dyn_cast<::llvm::object::ELFObjectFileBase>(::std::addressof(object))};
                if(elf == nullptr) { private_explicit_elf_valid_ = false; return; }
                for(auto const& symbol: elf->symbols())
                {
                    auto kind{symbol.getType()};
                    if(!kind) { ::llvm::consumeError(kind.takeError()); continue; }
                    if(*kind != ::llvm::object::SymbolRef::ST_Function) { continue; }
                    auto const width{::llvm::object::ELFSymbolRef{symbol}.getSize()};
                    if(width == 0u) { continue; }
                    auto selected{symbol.getSection()};
                    if(!selected) { ::llvm::consumeError(selected.takeError()); continue; }
                    auto const section{*selected};
                    // [actual object-owned section iterator][section_end]
                    // [safe] Sentinel is checked before any synchronous borrow.
                    if(section == object.section_end() || !section->isText()) { continue; }
                    auto address{symbol.getAddress()};
                    if(!address) { ::llvm::consumeError(address.takeError()); continue; }
                    auto const original_begin{section->getAddress()}, original_size{section->getSize()};
                    if(*address < original_begin) { continue; }
                    auto const offset{*address - original_begin};
                    if(offset >= original_size || width > original_size - offset) { continue; }
                    auto const relocation{loaded.getSectionLoadAddress(*section)};
                    constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
                    if(relocation == 0u || relocation > limit || offset > limit - relocation) { continue; }
                    auto const begin{relocation + offset};
                    if(width > limit - begin || private_explicit_elf_functions_.size() == 4'096uz)
                    { private_explicit_elf_valid_ = false; return; }
                    // [actual loaded text + checked offset][explicit ELF st_size]
                    // [safe] Integers only, confined to the real loaded section;
                    // no next-symbol boundary or guest pointer authenticates size.
                    private_explicit_elf_functions_.push_back({static_cast<::std::uintptr_t>(begin), static_cast<::std::uintptr_t>(width)});
                }
            }
            catch(...) { private_explicit_elf_valid_ = false; }
#else
            static_cast<void>(object); static_cast<void>(loaded); private_explicit_elf_valid_ = false;
#endif
        }
#endif
        ::llvm::ExecutionEngine* engine{};

        void revoke_observation() noexcept
        {
            // Revoke the WHOLE still-private candidate. A released object or
            // partially failed collection cannot leave stale text/functions
            // available to resolution, publication, or a later notification.
            observation_failed_ = true;
            pending.clear(); pending_functions.clear();
            debug_full_capture_ = false;
            debug_native_rows_.invalidate_runtime_generation();
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
            private_explicit_elf_valid_ = false;
            private_explicit_elf_functions_.clear();
#endif
        }

        void detach() noexcept
        {
            if(engine == nullptr) { return; }
            engine->UnregisterJITEventListener(this);
            // Unregister before this listener can die. Moving the engine's owner
            // does not change its address; the caller keeps it alive through us.
            engine = nullptr;
        }

    public:
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
        explicit pending_llvm_jit_code_ranges(::llvm::ExecutionEngine& owner, bool enabled, bool private_explicit_elf = false) noexcept
            : private_explicit_elf_enabled_{private_explicit_elf}, private_explicit_elf_valid_{private_explicit_elf}
#else
        explicit pending_llvm_jit_code_ranges(::llvm::ExecutionEngine& owner, bool enabled) noexcept
#endif
        {
            if(enabled)
            {
                engine = ::std::addressof(owner);
                // [live engine] this scope must end before the borrowed engine.
                engine->RegisterJITEventListener(this);
            }
        }
        pending_llvm_jit_code_ranges(pending_llvm_jit_code_ranges const&) = delete;
        pending_llvm_jit_code_ranges& operator=(pending_llvm_jit_code_ranges const&) = delete;
        ~pending_llvm_jit_code_ranges() { detach(); } // Abort discards all private ranges.

        // Host's real debug-full opt-in, before any object notification. This
        // private listener is not the singleton lazy/unwind listener, and an
        // ordinary full engine performs no line-table parsing or executed call.
        void configure_debug_full_capture(bool enabled) noexcept
        {
            if(enabled && engine != nullptr && pending.empty() && pending_functions.empty() && !committed_ && !observation_failed_)
            { debug_full_capture_ = true; }
            else { debug_full_capture_ = false; debug_native_rows_.invalidate_runtime_generation(); }
        }

        void notifyObjectLoaded(ObjectKey key, ::llvm::object::ObjectFile const& object,
                                ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) noexcept override
        {
            if(observation_failed_) { return; }
#ifdef UWVM_CPP_EXCEPTIONS
            try
            {
#endif
            if(debug_full_capture_) { static_cast<void>(debug_native_rows_.observe(key, object, loaded)); }
            for(auto const& section: object.sections())
            {
                if(!section.isText()) { continue; }
                auto const begin{static_cast<::std::uintptr_t>(loaded.getSectionLoadAddress(section))};
                auto const size{static_cast<::std::uintptr_t>(section.getSize())};
                if(begin == 0u || size == 0u || begin + size <= begin) { continue; }
                pending.push_back({begin, size});
            }
            for_each_llvm_jit_loaded_function_range(object, loaded,
                [this](::std::uintptr_t begin, ::std::uintptr_t size)
                { pending_functions.push_back({begin, size}); });
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
            observe_private_explicit_elf_symbols(object, loaded);
#endif
#ifdef UWVM_CPP_EXCEPTIONS
            }
            catch(...)
            {
                // Actual std::vector copies may fail after one or more text or
                // function extents were collected. Revoke the WHOLE candidate
                // before returning across LLVM's noexcept listener boundary.
                // clear() destroys only integer DATA; no new allocation occurs.
                revoke_observation();
            }
#endif
        }
        void notifyFreeingObject(ObjectKey) noexcept override
        {
            // Any premature object retirement invalidates the entire private
            // engine candidate, including text/function/explicit-ELF ranges.
            // Its borrowed loaded rows cannot authorize a later resolution or
            // commit, and further load notifications cannot resurrect them.
            // After commit the listener is detached and the real publication
            // owns the engine lifetime; there is no pending candidate to revoke.
            if(!committed_) { revoke_observation(); }
        }
        [[nodiscard]] bool empty() const noexcept { return pending.empty(); }
        [[nodiscard]] bool has_observation_failure() const noexcept { return observation_failed_; }
        // Diagnostic integer ownership only. The same synchronous listener and
        // still-private engine must have observed this EXACT function entry,
        // confined to one of its actual relocated text sections. This is not
        // an executable/debugger/CFI capability or an arbitrary-PC read API.
        [[nodiscard]] bool owns_pending_loaded_function_entry(::std::uintptr_t address) const noexcept
        {
            if(engine == nullptr || committed_ || observation_failed_ || address == 0u) { return false; }
            ::std::uintptr_t extent{};
            for(auto const& function: pending_functions)
            {
                if(function.begin != address) { continue; }
                if(function.size == 0u || function.size > (::std::numeric_limits<::std::uintptr_t>::max)() - address ||
                   (extent != 0u && extent != function.size)) { return false; }
                extent = function.size;
            }
            if(extent == 0u) { return false; }
            for(auto const& section: pending)
            {
                // [actual private loaded text section ... size] integer end
                // [safe] offset subtraction only after begin comparison;
                // neither code pointer nor object/section borrow escapes here.
                if(address >= section.begin && address - section.begin < section.size &&
                   extent <= section.size - (address - section.begin)) { return true; }
            }
            return false;
        }
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
        [[nodiscard]] ::std::uintptr_t private_leaf_exact_loaded_function_size(::std::uintptr_t begin) const noexcept
        {
            // Private transaction only, before detach/commit. The original
            // engine and actual object listener own these relocation results.
            if(engine == nullptr || observation_failed_ || !private_explicit_elf_enabled_ || !private_explicit_elf_valid_ || begin == 0u) { return 0u; }
            ::std::uintptr_t size{};
            for(auto const& function: private_explicit_elf_functions_)
            {
                if(function.begin != begin) { continue; }
                // Duplicates/aliases with disagreeing extents decline; no
                // inferred next-entry boundary authenticates a private clone.
                if(size != 0u && size != function.size) { return 0u; }
                size = function.size;
            }
            if(size == 0u || size > (::std::numeric_limits<::std::uintptr_t>::max)() - begin) { return 0u; }
            for(auto const& section: pending)
            {
                if(begin >= section.begin && begin - section.begin < section.size &&
                   size <= section.size - (begin - section.begin)) { return size; }
            }
            return 0u;
        }
#endif
        template<typename Publish, typename PublishFunction>
        void commit(Publish&& publish, PublishFunction&& publish_function) noexcept
        {
            // Caller resolved EVERY entry and installed the owning engine before
            // committing; no callable slot may be published until this returns.
            detach();
            if(observation_failed_) { return; } // Never publish partial/failed observation DATA.
            for(auto const& item: pending) { publish(item.begin, item.size); }
            for(auto const& item: pending_functions) { publish_function(item.begin, item.size); }
            pending.clear();
            pending_functions.clear();
            committed_ = true;
        }
        [[nodiscard]] native_loaded_provenance::image take_debug_full_capture(::std::uint_least64_t actual_epoch) noexcept
        {
            native_loaded_provenance::image result{};
            result.invalidate_runtime_generation();
            if(observation_failed_ || !debug_full_capture_ || !committed_ || engine != nullptr) { return result; }
            result = ::std::move(debug_native_rows_);
            result.bind_actual_runtime_epoch(actual_epoch);
            debug_full_capture_ = false;
            return result;
        }
        template<typename Publish>
        void commit(Publish&& publish) noexcept
        {
            commit(publish, [](::std::uintptr_t, ::std::uintptr_t) noexcept {});
        }
    };
}
#endif

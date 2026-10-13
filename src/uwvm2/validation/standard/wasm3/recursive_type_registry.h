/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <limits>
# include <utility>
# include "recursive_type_validation.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // One registry is owned by a VM/linking domain. IDs are stable for its lifetime, cannot be reused,
    // and carry no guest pointer. Module parsing may run concurrently, but callers serialize insertion
    // and exclude it while reading this mutable builder. Execution will consume a frozen snapshot.
    // This class does not itself install a global process singleton or grant guest code registry access.
    class recursive_type_registry
    {
        struct record
        {
            ::uwvm2::parser::wasm::standard::wasm3::type::composite_kind kind{};
            ::std::size_t parent{recursive_validation_details::no_index}, depth{}, jump{};
        };
        ::uwvm2::utils::container::map<recursive_validation_details::words, ::std::size_t,
                                     recursive_validation_details::words_less> groups_{};
        ::uwvm2::utils::container::vector<record> records_{};
        [[nodiscard]] inline ::std::size_t ancestor(::std::size_t index, ::std::size_t depth) const noexcept
        {
            // Internal precondition: index is a live registry ID and depth <= its stored depth.
            // Each hop strictly decreases depth. A low-bit jump skips a power-of-two suffix of the chain;
            // no recursive calls or quadratic arrays of ancestors are needed.
            while(records_.index_unchecked(index).depth > depth)
            {
                auto const& current{records_.index_unchecked(index)};
                auto const jump{current.jump};
                index = records_.index_unchecked(jump).depth >= depth ? jump : current.parent;
            }
            return index;
        }
    public:
        [[nodiscard]] inline ::std::size_t size() const noexcept { return records_.size(); }
        [[nodiscard]] inline bool matches(::std::size_t from, ::std::size_t to) const noexcept
        {
            if(from >= records_.size() || to >= records_.size()) { return false; }
            // The common exact-type path is one ID comparison, with no walk through recursive structures.
            if(from == to) { return true; }
            auto const& a{records_.index_unchecked(from)};
            auto const& b{records_.index_unchecked(to)};
            return a.kind == b.kind && a.depth >= b.depth && ancestor(from, b.depth) == to;
        }
        [[nodiscard]] inline recursive_type_validation_result validate_and_intern(
            ::uwvm2::parser::wasm::standard::wasm3::type::recursive_type_section const& section,
            ::uwvm2::utils::container::vector<::std::size_t>& module_ids) noexcept
        {
            namespace d = recursive_validation_details;
            recursive_type_context checked{};
            auto const validation{validate_core3_type_section(section, checked)};
            if(validation.error != recursive_type_validation_error::ok) { return validation; }
            // A failed validation does not mutate the registry or the caller's mapping. No borrowed section
            // objects are retained: keys replace external references with stable IDs, internal ones with binders.
            auto const count{checked.records.size()};
            auto const max_records{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(record)};
            if(count > max_records - records_.size())
            { return {recursive_type_validation_error::inconsistent_section, 0, section.type_count}; }
            ::uwvm2::utils::container::vector<::std::size_t> ids{};
            ids.resize(count);
            for(auto const& group : section.groups)
            {
                auto const first{static_cast<::std::size_t>(group.first_type_index)};
                auto key{d::group_key(group, ids)};
                auto const [position, inserted]{groups_.try_emplace(::std::move(key), records_.size())};
                auto const canonical_first{position->second}; // A live iterator returned by successful insertion/lookup.
                for(::std::size_t local{}; local != group.types.size(); ++local)
                {
                    // The validated expanded count bounds first + local; group size is part of the intern key.
                    ids.index_unchecked(first + local) = canonical_first + local;
                }
                if(!inserted) { continue; }
                for(::std::size_t local{}; local != group.types.size(); ++local)
                {
                    auto const index{canonical_first + local};
                    auto const& type{group.types.index_unchecked(local)};
                    record item{.kind = type.kind, .jump = index};
                    if(!type.supertypes.empty())
                    {
                        // Validation proved parent < first + local. Its global ID was inserted by an earlier
                        // group or an earlier iteration, so every following record access is within live storage.
                        item.parent = ids.index_unchecked(type.supertypes.index_unchecked(0));
                        item.depth = records_.index_unchecked(item.parent).depth + 1uz;
                        item.jump = ancestor(item.parent, item.depth & (item.depth - 1uz));
                    }
                    records_.push_back(item);
                }
            }
            module_ids = ::std::move(ids);
            return {};
        }
    };
}

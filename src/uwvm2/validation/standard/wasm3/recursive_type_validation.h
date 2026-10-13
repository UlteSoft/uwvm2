/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <utility>
# include <uwvm2/parser/wasm/standard/wasm3/type/recursive_type.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    enum class recursive_type_validation_error : unsigned
    {
        ok, inconsistent_section, invalid_composite, invalid_value, unknown_type,
        multiple_supertypes, forward_supertype, final_supertype, incompatible_supertype
    };
    struct recursive_type_validation_result
    {
        recursive_type_validation_error error{};
        ::std::size_t binary_offset{};
        ::std::uint_least64_t type_index{};
    };
    namespace recursive_validation_details
    {
        namespace types = ::uwvm2::parser::wasm::standard::wasm3::type;
        inline constexpr ::std::size_t no_index{(::std::numeric_limits<::std::size_t>::max)()};
        using words = ::uwvm2::utils::container::vector<::std::uint_least64_t>;
        struct words_less
        {
            [[nodiscard]] inline bool operator()(words const& a, words const& b) const noexcept
            {
                auto const count{a.size() < b.size() ? a.size() : b.size()};
                for(::std::size_t i{}; i != count; ++i)
                {
                    // [0, count) lies inside both live vectors; no input pointers are traversed.
                    auto const av{a.index_unchecked(i)}, bv{b.index_unchecked(i)};
                    if(av != bv) { return av < bv; }
                }
                return a.size() < b.size();
            }
        };
        [[nodiscard]] inline constexpr bool abstract_valid(types::heap_type heap) noexcept
        { return heap.code >= -23 && heap.code <= -12; }
        [[nodiscard]] inline constexpr bool abstract_matches(types::heap_type from, types::heap_type to) noexcept
        {
            using h = types::abstract_heap_type;
            if(!abstract_valid(from) || !abstract_valid(to)) { return false; }
            if(from == to) { return true; }
            auto const a{static_cast<h>(from.code)}, b{static_cast<h>(to.code)};
            switch(a)
            {
                case h::none: return b == h::i31 || b == h::struct_ || b == h::array || b == h::eq || b == h::any;
                case h::i31: case h::struct_: case h::array: return b == h::eq || b == h::any;
                case h::eq: return b == h::any;
                case h::nofunc: return b == h::func;
                case h::noextern: return b == h::extern_;
                case h::noexn: return b == h::exn;
                default: return false;
            }
        }
        // Caller has validated reference scope and supplies one canonical ID per earlier module type.
        [[nodiscard]] inline words group_key(types::recursive_group const& group,
            ::uwvm2::utils::container::vector<::std::size_t> const& canonical) noexcept
        {
            namespace t = types;
            auto const first{static_cast<::std::size_t>(group.first_type_index)};
            words key{};
            key.push_back(group.types.size());
            auto heap = [&](t::heap_type value) noexcept
            {
                if(!value.is_defined()) { key.push_back(0); key.push_back(static_cast<::std::uint_least64_t>(value.code)); }
                else if(static_cast<::std::uint_least64_t>(value.code) >= first)
                { key.push_back(1); key.push_back(static_cast<::std::uint_least64_t>(value.code) - first); }
                else
                {
                    key.push_back(2);
                    // Nonnegative value.code < first <= count; earlier groups have already been interned.
                    key.push_back(canonical.index_unchecked(static_cast<::std::size_t>(value.code)));
                }
            };
            auto value = [&](t::core_value_type val) noexcept
            {
                key.push_back(static_cast<unsigned>(val.kind));
                if(val.kind == t::value_kind::reference) { key.push_back(val.nullable); heap(val.heap); }
            };
            for(auto const& type : group.types)
            {
                key.push_back(type.final_); key.push_back(type.supertypes.size());
                for(auto parent : type.supertypes) { heap({parent}); }
                key.push_back(static_cast<unsigned>(type.kind));
                key.push_back(type.parameters.size()); for(auto val : type.parameters) { value(val); }
                key.push_back(type.results.size()); for(auto val : type.results) { value(val); }
                key.push_back(type.fields.size());
                for(auto const& field : type.fields)
                {
                    key.push_back(field.mutable_); key.push_back(static_cast<unsigned>(field.storage.packed));
                    if(field.storage.packed == t::packed_kind::none) { value(field.storage.value); }
                }
            }
            return key;
        }
    }
    struct recursive_type_record
    {
        ::uwvm2::parser::wasm::standard::wasm3::type::composite_kind kind{};
        // Canonical indices are local to this context. Never compare indices from unrelated modules directly.
        ::std::size_t canonical{}, parent{recursive_validation_details::no_index}, preorder{}, subtree_end{};
    };
    struct recursive_type_context
    {
        ::uwvm2::utils::container::vector<recursive_type_record> records{};
        [[nodiscard]] inline constexpr bool contains(::std::uint_least64_t index) const noexcept { return index < records.size(); }
        [[nodiscard]] inline constexpr bool matches(
            ::uwvm2::parser::wasm::standard::wasm3::type::heap_type from,
            ::uwvm2::parser::wasm::standard::wasm3::type::heap_type to) const noexcept
        {
            namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
            using h = t::abstract_heap_type;
            if(from.is_defined() && !contains(static_cast<::std::uint_least64_t>(from.code))) { return false; }
            if(to.is_defined() && !contains(static_cast<::std::uint_least64_t>(to.code))) { return false; }
            if(from.code == t::heap_type::bottom_code)
            { return to.is_defined() || to.code == t::heap_type::bottom_code || recursive_validation_details::abstract_valid(to); }
            if(to.code == t::heap_type::bottom_code) { return false; }
            if(from.is_defined() && to.is_defined())
            {
                // [0, records.size()) both type indices were checked before either metadata access.
                auto const& a{records.index_unchecked(static_cast<::std::size_t>(from.code))};
                auto const& b{records.index_unchecked(static_cast<::std::size_t>(to.code))};
                // Single inheritance becomes a forest of canonical types. Interval containment is constant time,
                // including deep subtype chains, and does not recurse on the native stack.
                return a.preorder >= b.preorder && a.preorder < b.subtree_end;
            }
            if(from.is_defined())
            {
                // The same preceding index check bounds this access; conversion to size_t cannot truncate.
                auto const kind{records.index_unchecked(static_cast<::std::size_t>(from.code)).kind};
                from.code = static_cast<::std::int_least64_t>(kind == t::composite_kind::function ? h::func :
                            kind == t::composite_kind::struct_ ? h::struct_ : h::array);
            }
            else if(to.is_defined())
            {
                // A bottom heap matches every defined type in its own hierarchy, never another hierarchy.
                auto const kind{records.index_unchecked(static_cast<::std::size_t>(to.code)).kind};
                return from.code == static_cast<::std::int_least64_t>(kind == t::composite_kind::function ? h::nofunc : h::none);
            }
            return recursive_validation_details::abstract_matches(from, to);
        }
        [[nodiscard]] inline constexpr bool matches(
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type from,
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type to) const noexcept
        {
            using k = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
            return from.kind == to.kind && (from.kind == k::reference ?
                ((!from.nullable || to.nullable) && matches(from.heap, to.heap)) : from.kind <= k::v128);
        }
    };
    // Core 3 valid/types + valid/matching + valid/conventions (closed recursive groups).
    // The binary decoder owns all vectors. This pass borrows them synchronously and publishes an independent
    // context only after every declared subtype is proved valid. No source-buffer pointers survive this pass.
    [[nodiscard]] inline recursive_type_validation_result validate_core3_type_section(
        ::uwvm2::parser::wasm::standard::wasm3::type::recursive_type_section const& section,
        recursive_type_context& output) noexcept
    {
        namespace d = recursive_validation_details;
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        using e = recursive_type_validation_error;
        // First verify counts without trusting editable metadata to size an allocation or index a vector.
        ::std::uint_least64_t total{};
        for(auto const& group : section.groups)
        {
            if(group.first_type_index != total || group.types.size() > 0x1'0000'0000ull - total)
            { return {e::inconsistent_section, group.binary_offset, total}; }
            total += group.types.size();
        }
        if(total != section.type_count || total > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(recursive_type_record))
        { return {e::inconsistent_section, 0, total}; }
        auto const count{static_cast<::std::size_t>(total)};
        recursive_type_context result{};
        result.records.resize(count);
        ::uwvm2::utils::container::vector<t::sub_type const*> definitions{};
        definitions.reserve(count);
        for(auto const& group : section.groups) for(auto const& type : group.types)
        {
            // [group.types live objects] type is a live element, not a source byte or one-past iterator.
            // Store a borrowed object address for this synchronous pass; the caller must not mutate section.
            definitions.push_back_unchecked(::std::addressof(type));
        }
        for(auto const& group : section.groups)
        {
            auto const visible{group.first_type_index + group.types.size()};
            for(::std::size_t local{}; local != group.types.size(); ++local)
            {
                auto const index{static_cast<::std::size_t>(group.first_type_index) + local};
                // [0, group.types.size()) local is bounded, and index < the checked expanded count.
                auto const& type{group.types.index_unchecked(local)};
                auto fail = [&](e error) noexcept { return recursive_type_validation_result{error, type.binary_offset, index}; };
                if(type.supertypes.size() > 1uz) { return fail(e::multiple_supertypes); }
                if(type.kind > t::composite_kind::array ||
                   (type.kind == t::composite_kind::function && !type.fields.empty()) ||
                   (type.kind != t::composite_kind::function && (!type.parameters.empty() || !type.results.empty())) ||
                   (type.kind == t::composite_kind::array && type.fields.size() != 1uz))
                { return fail(e::invalid_composite); }
                if(!type.supertypes.empty())
                {
                    auto const parent{type.supertypes.index_unchecked(0)};
                    if(parent >= index) { return fail(e::forward_supertype); }
                    // parent < index < count proves this borrowed definition is present.
                    auto const& super{*definitions.index_unchecked(parent)};
                    if(super.final_) { return fail(e::final_supertype); }
                    if(type.kind != super.kind) { return fail(e::incompatible_supertype); }
                }
                auto check_value = [&](t::core_value_type value) noexcept
                {
                    if(value.kind > t::value_kind::reference) { return e::invalid_value; }
                    if(value.kind != t::value_kind::reference) { return e::ok; }
                    if(value.heap.is_defined())
                    { return static_cast<::std::uint_least64_t>(value.heap.code) < visible ? e::ok : e::unknown_type; }
                    return d::abstract_valid(value.heap) ? e::ok : e::invalid_value;
                };
                for(auto value : type.parameters) { auto error{check_value(value)}; if(error != e::ok) { return fail(error); } }
                for(auto value : type.results) { auto error{check_value(value)}; if(error != e::ok) { return fail(error); } }
                for(auto const& field : type.fields)
                {
                    if(field.storage.packed > t::packed_kind::i16) { return fail(e::invalid_value); }
                    if(field.storage.packed == t::packed_kind::none)
                    { auto error{check_value(field.storage.value)}; if(error != e::ok) { return fail(error); } }
                }
                result.records.index_unchecked(index).kind = type.kind;
            }
        }
        // Ordered interning avoids attacker-chosen hash collision chains. A key preserves the whole group,
        // projection positions, finality, declared parents, and local binders; merely equal unfolded shapes
        // are insufficient. Closed references use canonical indices of PREVIOUS groups.
        ::uwvm2::utils::container::map<d::words, ::std::size_t, d::words_less> groups{};
        ::uwvm2::utils::container::vector<::std::size_t> canonical_indices{};
        canonical_indices.resize(count);
        for(auto const& group : section.groups)
        {
            auto const first{static_cast<::std::size_t>(group.first_type_index)};
            auto key{d::group_key(group, canonical_indices)};
            auto const [position, inserted]{groups.try_emplace(::std::move(key), first)};
            auto const canonical_first{position->second}; // try_emplace returns a live map iterator.
            for(::std::size_t local{}; local != group.types.size(); ++local)
            {
                // first + local is in this group; duplicate group size was part of the exact key.
                canonical_indices.index_unchecked(first + local) = canonical_first + local;
                result.records.index_unchecked(first + local).canonical = canonical_first + local;
            }
        }
        // Build the single-inheritance forest in O(type_count) space, then assign DFS intervals without recursion.
        ::uwvm2::utils::container::vector<::std::size_t> first_child{}, next_sibling{};
        first_child.resize(count); next_sibling.resize(count);
        for(::std::size_t i{}; i != count; ++i) { first_child.index_unchecked(i) = next_sibling.index_unchecked(i) = d::no_index; }
        for(::std::size_t i{}; i != count; ++i)
        {
            auto& record{result.records.index_unchecked(i)};
            if(record.canonical != i) { continue; }
            auto const& type{*definitions.index_unchecked(i)};
            if(type.supertypes.empty()) { continue; }
            auto const parent{result.records.index_unchecked(type.supertypes.index_unchecked(0)).canonical};
            // Every canonical parent precedes the first occurrence of its child, so no forest cycle is possible.
            record.parent = parent;
            next_sibling.index_unchecked(i) = first_child.index_unchecked(parent);
            first_child.index_unchecked(parent) = i;
        }
        struct visit { ::std::size_t index{}; bool leave{}; };
        ::uwvm2::utils::container::vector<visit> stack{};
        ::std::size_t clock{};
        for(::std::size_t root{}; root != count; ++root)
        {
            if(result.records.index_unchecked(root).canonical != root || result.records.index_unchecked(root).parent != d::no_index) { continue; }
            stack.push_back({root, false});
            while(!stack.empty())
            {
                auto const current{stack.back_unchecked()}; stack.pop_back_unchecked();
                // Every pushed index is a root < count or an initialized child link from the bounded loop above.
                auto& record{result.records.index_unchecked(current.index)};
                if(current.leave) { record.subtree_end = clock; continue; }
                record.preorder = clock++;
                stack.push_back({current.index, true});
                for(auto child{first_child.index_unchecked(current.index)}; child != d::no_index; child = next_sibling.index_unchecked(child))
                { stack.push_back({child, false}); }
            }
        }
        for(::std::size_t i{}; i != count; ++i)
        {
            auto& record{result.records.index_unchecked(i)};
            auto const& canonical{result.records.index_unchecked(record.canonical)};
            record.preorder = canonical.preorder; record.subtree_end = canonical.subtree_end; record.parent = canonical.parent;
        }
        auto storage_matches = [&](t::storage_type a, t::storage_type b) noexcept
        { return a.packed == b.packed && (a.packed != t::packed_kind::none || result.matches(a.value, b.value)); };
        auto field_matches = [&](t::field_type a, t::field_type b) noexcept
        { return a.mutable_ == b.mutable_ && storage_matches(a.storage, b.storage) && (!a.mutable_ || storage_matches(b.storage, a.storage)); };
        for(::std::size_t i{}; i != count; ++i)
        {
            auto const& type{*definitions.index_unchecked(i)};
            if(type.supertypes.empty()) { continue; }
            // The earlier parent-index check dominates this lookup; neither definition escapes the pass.
            auto const& parent{*definitions.index_unchecked(type.supertypes.index_unchecked(0))};
            bool valid{true};
            if(type.kind == t::composite_kind::function)
            {
                valid = type.parameters.size() == parent.parameters.size() && type.results.size() == parent.results.size();
                if(valid)
                {
                    for(::std::size_t j{}; j != type.parameters.size(); ++j)
                    { valid &= result.matches(parent.parameters.index_unchecked(j), type.parameters.index_unchecked(j)); }
                    for(::std::size_t j{}; j != type.results.size(); ++j)
                    { valid &= result.matches(type.results.index_unchecked(j), parent.results.index_unchecked(j)); }
                }
            }
            else
            {
                valid = type.fields.size() >= parent.fields.size(); // Arrays have exactly one field by the shape check.
                if(valid) for(::std::size_t j{}; j != parent.fields.size(); ++j)
                { valid &= field_matches(type.fields.index_unchecked(j), parent.fields.index_unchecked(j)); }
            }
            if(!valid) { return {e::incompatible_supertype, type.binary_offset, i}; }
        }
        output = ::std::move(result);
        return {};
    }
}

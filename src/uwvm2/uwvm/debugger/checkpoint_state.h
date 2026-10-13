/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <fast_io.h>
# include <algorithm>
# include <array>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <span>
# include <utility>
# include <vector>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::checkpoint
{
    // These are file-local logical identifiers, never native addresses, tokens,
    // OS thread IDs, runtime leases, pause tickets or debugger authorization.
    using object_id = ::std::uint64_t;
    enum class object_kind : ::std::uint16_t
    {
        module = 1u, instance, function, memory, memory_chunk, table, table_chunk,
        global, tag, data, element, structure, array, exception, external,
        thread, frame, control, handler, atomic_wait, host_resource, event, exception_trace, host_reference
    };
    enum class value_kind : ::std::uint8_t { i32 = 1u, i64, f32, f64, v128, reference, i8, i16 };
    enum class heap_kind : ::std::uint8_t
    {
        none = 0u, func, external, any, eq, i31, structure, array, exception,
        nofunc, noextern, bottom, noexception, defined
    };
    enum class reference_kind : ::std::uint8_t { null = 0u, function, external, structure, array, exception, i31, host };
    inline constexpr ::std::uint64_t known_feature_mask{0x3ffffu};
    // Binding-only DATA: this ID never selects an executable host adapter.
    inline constexpr ::std::uint64_t builtin_wasip1_binding_kind{0x3150495341575755u};
    inline constexpr ::std::size_t builtin_wasip1_binding_bytes{108u};

    struct value_type
    {
        value_kind kind{value_kind::i32};
        heap_kind heap{};
        bool nullable{};
        object_id type_module{};
        ::std::uint32_t type_index{};
        friend bool operator==(value_type const&, value_type const&) = default;
    };
    struct value
    {
        value_type type{};
        reference_kind reference{};
        // False denotes a typed uninitialized NONDEFAULTABLE frame local or
        // the TYPE-ONLY descriptor of an EMPTY table (length exactly zero).
        // It is never a null value, table element, GC field or display value.
        bool initialized{true};
        object_id target{};
        // Numbers are raw IEEE/integer bits, including NaN payloads and signed
        // zero. v128 uses the Wasm byte order, low lane bytes in low_bits.
        // i31 carries its unsigned 31 bits here; it has no object target.
        ::std::uint64_t low_bits{}, high_bits{};
        friend bool operator==(value const&, value const&) = default;
    };
    struct object
    {
        object_kind kind{object_kind::module};
        ::std::uint16_t flags{};
        // The normative, kind-specific meanings are in checkpoint_format.md.
        // Every unused word must be zero; no field can encode a host pointer.
        ::std::array<::std::uint64_t, 8u> words{};
        ::std::vector<object_id> links{};
        ::std::vector<value> values{};
        ::std::vector<::std::byte> bytes{};
        friend bool operator==(object const&, object const&) = default;
    };
    struct state
    {
        ::std::array<::std::byte, 16u> recording_id{};
        ::std::uint64_t checkpoint_id{}, parent_checkpoint_id{}, logical_instruction{}, replay_event_cursor{};
        ::std::uint64_t required_features{}, next_logical_thread{1u};
        // Objects have dense IDs index+1, preserving aliases and cycles.
        ::std::vector<object> objects{};
        ::std::vector<object_id> root_instances{};
        ::std::vector<value> retained_roots{};
        friend bool operator==(state const&, state const&) = default;
    };
    struct limits
    {
        ::std::uint64_t max_file_bytes{256u * 1024u * 1024u};
        ::std::uint64_t max_objects{1048576u}, max_links{4194304u}, max_values{4194304u};
        ::std::uint64_t max_payload_bytes{256u * 1024u * 1024u}, max_threads{4096u}, max_frames{1048576u};
    };
    enum class error
    {
        none, truncated, unsupported_version, malformed, limit_exceeded,
        invalid_reference, invalid_shape, incompatible_type, overlapping_chunks,
        incomplete_commit, digest_mismatch, trailing_bytes, unavailable_capability,
        replay_diverged, non_replayable_import, stale_generation
    };
    [[nodiscard]] inline constexpr bool known(object_kind kind) noexcept
    { return kind >= object_kind::module && kind <= object_kind::host_reference; }
    // A diagnostic position may be explicitly unavailable. This marker is
    // never a resume selector/native PC; Core code bodies have u32 extents.
    inline constexpr ::std::uint64_t unknown_diagnostic_instruction{(::std::numeric_limits<::std::uint64_t>::max)()};
    namespace state_details
    {
        [[nodiscard]] inline bool valid_builtin_binding(object const& item) noexcept
        {
            if(item.flags != 3u || item.words[0u] != builtin_wasip1_binding_kind || item.words[1u] != 1u ||
                item.words[2u] == 0u || item.words[2u] > 128u || item.bytes.size() != builtin_wasip1_binding_bytes)
            { return false; }
            constexpr ::std::array<unsigned char, 12u> magic{'U','W','V','M','W','A','S','I','P','1','B','1'};
            for(::std::size_t i{}; i != magic.size(); ++i)
            { if(item.bytes[i] != static_cast<::std::byte>(magic[i])) { return false; } }
            auto const get{[&](::std::size_t offset, ::std::uint64_t& value) noexcept
            {
                if(offset > item.bytes.size() || 8u > item.bytes.size() - offset) { return false; }
                // [owned exact108-byte binding ... offset][8 LE bytes] end
                // [safe] subtraction range proof BEFORE both pointer advances.
                auto const* first{reinterpret_cast<unsigned char const*>(item.bytes.data()) + offset};
                auto const* last{first + 8u};
                auto const parsed{::fast_io::parse_by_scan(first,last,::fast_io::mnp::le_get<64u>(value))};
                return parsed.code == ::fast_io::parse_code::ok && parsed.iter == last;
            }};
            ::std::uint64_t index{}, revision{}, parameters{}, results{};
            if(!get(12u,index) || !get(20u,revision) || !get(60u,parameters) || !get(68u,results) ||
                index >= 128u || index + 1u != item.words[2u] || revision != 1u || parameters > 16u || results > 16u)
            { return false; }
            bool digest{};
            for(::std::size_t i{28u}; i != 60u; ++i) { digest = digest || item.bytes[i] != ::std::byte{}; }
            if(!digest) { return false; } // Nonzero copied digest is DATA, never proof of its actual source.
            for(::std::size_t i{}; i != 16u; ++i)
            {
                // [fixed verified108-byte owner: params76..91/results92..107]
                // [safe] i<16 BEFORE additions/index; no native address or view.
                auto const valid{[](auto byte, bool active) noexcept
                { return active ? byte == ::std::byte{0x7fu} || byte == ::std::byte{0x7eu} : byte == ::std::byte{}; }};
                if(!valid(item.bytes[76u+i],i<parameters) || !valid(item.bytes[92u+i],i<results)) { return false; }
            }
            return true;
        }

        [[nodiscard]] inline constexpr bool charge(::std::uint64_t count, ::std::uint64_t cap, ::std::uint64_t& used) noexcept
        { if(used > cap || count > cap - used) { return false; } used += count; return true; }
        [[nodiscard]] inline object const* lookup(state const& snapshot, object_id id) noexcept
        {
            if(id == 0u || id > snapshot.objects.size()) { return nullptr; }
            // [owned object vector: 0 ... id-1 ... size] end
            // [safe                                  ] unsafe (one-past)
            //                          ^^ id<=size checked before indexing.
            return ::std::addressof(snapshot.objects[static_cast<::std::size_t>(id - 1u)]);
        }
        [[nodiscard]] inline bool has_kind(state const& snapshot, object_id id, object_kind kind) noexcept
        { auto const* item{lookup(snapshot, id)}; return item != nullptr && item->kind == kind; }
        [[nodiscard]] inline bool valid(value const& item, state const& snapshot, bool allow_uninitialized_local = false,
            bool allow_empty_table_descriptor = false) noexcept
        {
            auto const kind{item.type.kind};
            if(kind < value_kind::i32 || kind > value_kind::i16) { return false; }
            if(kind != value_kind::reference)
            {
                if(!item.initialized || item.type.heap != heap_kind::none || item.type.nullable || item.type.type_module != 0u || item.type.type_index != 0u ||
                   item.reference != reference_kind::null || item.target != 0u) { return false; }
                if(kind != value_kind::v128 && item.high_bits != 0u) { return false; }
                if((kind == value_kind::i32 || kind == value_kind::f32) && item.low_bits > 0xffffffffu) { return false; }
                if(kind == value_kind::i8 && item.low_bits > 0xffu) { return false; }
                if(kind == value_kind::i16 && item.low_bits > 0xffffu) { return false; }
                return true;
            }
            if(item.type.heap < heap_kind::func || item.type.heap > heap_kind::defined || item.high_bits != 0u) { return false; }
            if(item.type.heap == heap_kind::defined)
            { if(!has_kind(snapshot, item.type.type_module, object_kind::module)) { return false; } }
            else if(item.type.type_module != 0u || item.type.type_index != 0u) { return false; }
            if(!item.initialized)
            { return (allow_empty_table_descriptor || (allow_uninitialized_local && !item.type.nullable)) &&
                item.reference == reference_kind::null &&
                item.target == 0u && item.low_bits == 0u; }
            if(item.reference == reference_kind::null)
            { return item.type.nullable && item.target == 0u && item.low_bits == 0u; }
            if(item.reference == reference_kind::i31)
            { return item.target == 0u && item.low_bits <= 0x7fffffffu &&
                (item.type.heap == heap_kind::i31 || item.type.heap == heap_kind::eq || item.type.heap == heap_kind::any); }
            if(item.low_bits != 0u) { return false; }
            object_kind expected{};
            switch(item.reference)
            {
                case reference_kind::function: expected = object_kind::function; break;
                case reference_kind::external: expected = object_kind::external; break;
                case reference_kind::structure: expected = object_kind::structure; break;
                case reference_kind::array: expected = object_kind::array; break;
                case reference_kind::exception: expected = object_kind::exception; break;
                case reference_kind::host: expected = object_kind::host_reference; break;
                default: return false;
            }
            if(!has_kind(snapshot, item.target, expected)) { return false; }
            if(item.type.heap == heap_kind::defined) { return expected == object_kind::function || expected == object_kind::structure || expected == object_kind::array; }
            if(expected == object_kind::function) { return item.type.heap == heap_kind::func; }
            if(expected == object_kind::external) { return item.type.heap == heap_kind::external; }
            if(expected == object_kind::host_reference) { return item.type.heap == heap_kind::any; }
            if(expected == object_kind::exception) { return item.type.heap == heap_kind::exception; }
            return item.type.heap == heap_kind::any || item.type.heap == heap_kind::eq ||
                (expected == object_kind::structure && item.type.heap == heap_kind::structure) ||
                (expected == object_kind::array && item.type.heap == heap_kind::array);
        }
        [[nodiscard]] inline bool valid_trace_payload(state const& snapshot, object const& trace) noexcept
        {
            if(trace.words[0u] == 0u) { return trace.bytes.empty(); }
            if(trace.words[0u] != 1u || trace.bytes.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()))
            { return false; }
            ::std::size_t cursor{};
            for(::std::size_t frame{}; frame != trace.links.size(); ++frame)
            {
                if(cursor > trace.bytes.size() || 40u > trace.bytes.size() - cursor) { return false; }
                // [owned payload ... cursor][complete40-byte record header] end
                // [safe] remaining>=40 and PTRDIFF checked BEFORE +cursor/+40.
                auto const* first{reinterpret_cast<unsigned char const*>(trace.bytes.data()) + cursor};
                auto const* header_end{first + 40u};
                ::std::array<::std::uint64_t, 5u> fields{};
                for(auto& field : fields)
                {
                    // [complete record header ... first][8bytes<=header_end] end
                    // [safe] five fixed8 windows BEFORE +8; each parse consumes8.
                    if(static_cast<::std::size_t>(header_end - first) < 8u) { return false; }
                    auto const* end{first + 8u};
                    auto const parsed{::fast_io::parse_by_scan(first, end, ::fast_io::mnp::le_get<64u>(field))};
                    if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != end) { return false; }
                    first = parsed.iter; // [safe] exact8 consumption, never beyond header_end.
                }
                auto const* instance{lookup(snapshot, fields[0u])};
                if(instance == nullptr || instance->kind != object_kind::instance || instance->links.empty() || fields[2u] != 0u ||
                    fields[1u] >= instance->words[0u] || fields[1u] >= instance->links.size() - 1u)
                { return false; }
                // [actual detached instance links1..function_count] end
                // [safe] function index bounded by both declared and actual sizes BEFORE +1.
                if(instance->links[static_cast<::std::size_t>(fields[1u]) + 1u] != trace.links[frame]) { return false; }
                cursor += 40u; // complete40 extent checked before integer advance.
                for(::std::size_t field{3u}; field != fields.size(); ++field)
                {
                    if(fields[field] > trace.bytes.size() - cursor) { return false; }
                    cursor += static_cast<::std::size_t>(fields[field]); // bounded name bytes; no pointers/UTF assumptions.
                }
            }
            return cursor == trace.bytes.size();
        }
    }
    // Structural and canonical validation only. Runtime restore additionally
    // validates embedded modules with the SAME Core 3 fused validator, checks
    // every type/subtype/field/continuation against its exact compiled metadata,
    // and holds a privately minted world-stop+generation lease. This function
    // never manufactures that capability and never dereferences file IDs.
    [[nodiscard]] inline error validate_graph(state const& snapshot, limits const& cap = {})
    {
        if(snapshot.checkpoint_id == 0u || snapshot.parent_checkpoint_id >= snapshot.checkpoint_id || snapshot.next_logical_thread == 0u ||
           (snapshot.required_features & ~known_feature_mask) != 0u)
        { return error::malformed; }
        bool has_recording_id{};
        for(auto byte : snapshot.recording_id) { has_recording_id = has_recording_id || byte != ::std::byte{}; }
        if(!has_recording_id) { return error::malformed; }
        if(snapshot.objects.size() > cap.max_objects || snapshot.objects.empty()) { return error::limit_exceeded; }
        ::std::uint64_t links{}, values{}, bytes{}, threads{}, frames{};
        if(!state_details::charge(snapshot.root_instances.size(), cap.max_links, links) ||
           !state_details::charge(snapshot.retained_roots.size(), cap.max_values, values)) { return error::limit_exceeded; }
        for(auto id : snapshot.root_instances)
        { if(!state_details::has_kind(snapshot, id, object_kind::instance)) { return error::invalid_reference; } }
        if(snapshot.root_instances.empty()) { return error::invalid_shape; }
        for(::std::size_t i{}; i != snapshot.root_instances.size(); ++i)
        { if(i != 0u && snapshot.root_instances[i - 1u] >= snapshot.root_instances[i]) { return error::malformed; } }
        for(auto const& item : snapshot.retained_roots)
        { if(!state_details::valid(item, snapshot)) { return error::incompatible_type; } }
        // Per-parent end offsets establish canonical, non-overlapping sparse
        // chunks in O(objects+links+values), including 64-bit logical offsets.
        ::std::vector<::std::uint64_t> last_bytes(snapshot.objects.size(), 0u);
        ::std::vector<bool> chunk_seen(snapshot.objects.size(), false);
        // Mutable continuation records belong to exactly one containing
        // thread/frame. A single bit census rejects shared and orphaned records
        // without confusing legal store/GC/exception aliases with stack state.
        ::std::vector<bool> continuation_owned(snapshot.objects.size(), false);
        ::std::vector<::std::uint64_t> logical_threads{};
        ::std::uint64_t last_event{}, last_event_instruction{}, event_count{};
        for(auto const& item : snapshot.objects)
        {
            if(!known(item.kind) || !state_details::charge(item.links.size(), cap.max_links, links) ||
               !state_details::charge(item.values.size(), cap.max_values, values) ||
               !state_details::charge(item.bytes.size(), cap.max_payload_bytes, bytes)) { return error::limit_exceeded; }
            for(auto id : item.links) { if(state_details::lookup(snapshot, id) == nullptr) { return error::invalid_reference; } }
            for(::std::size_t field_index{}; field_index != item.values.size(); ++field_index)
            {
                bool const local{item.kind == object_kind::frame && field_index < item.words[1]};
                // A zero-length table has a declared type, no default VALUE.
                // The descriptor cannot escape into elements or other objects.
                bool const empty_table{item.kind == object_kind::table && item.words[1] == 0u &&
                    item.values.size() == 1u && field_index == 0u};
                if(!state_details::valid(item.values[field_index], snapshot, local, empty_table)) { return error::incompatible_type; }
            }
            auto const& w{item.words};
            auto links_are = [&](object_kind kind) { for(auto id : item.links) { if(!state_details::has_kind(snapshot, id, kind)) { return false; } } return true; };
            auto zeros_from = [&](::std::size_t first) { for(auto n{first}; n != w.size(); ++n) { if(w[n] != 0u) { return false; } } return true; };
            auto no_payload = [&] { return item.values.empty() && item.bytes.empty(); };
            switch(item.kind)
            {
                case object_kind::module:
                    if((item.flags == 0u ? !zeros_from(0u) :
                        item.flags != 1u || w[0u] != 1u || w[1u] > 4u ||
                        (w[2u] & ~((::std::uint64_t{1u} << 19u) - 1u)) != 0u || w[3u] > 3u || !zeros_from(4u)) ||
                       !item.links.empty() || !item.values.empty() || item.bytes.size() < 8u ||
                       item.bytes[0] != ::std::byte{} || item.bytes[1] != ::std::byte{'a'} || item.bytes[2] != ::std::byte{'s'} || item.bytes[3] != ::std::byte{'m'} ||
                       item.bytes[4] != ::std::byte{1u} || item.bytes[5] != ::std::byte{} || item.bytes[6] != ::std::byte{} || item.bytes[7] != ::std::byte{})
                    { return error::invalid_shape; } break;
                case object_kind::instance:
                {
                    if(item.flags != 0u || w[7] != 0u || !no_payload() || item.links.empty() ||
                       !state_details::has_kind(snapshot, item.links[0], object_kind::module)) { return error::invalid_shape; }
                    constexpr ::std::array<object_kind, 7u> kinds{object_kind::function, object_kind::table, object_kind::memory,
                        object_kind::global, object_kind::tag, object_kind::data, object_kind::element};
                    ::std::size_t cursor{1u};
                    for(::std::size_t k{}; k != kinds.size(); ++k)
                    {
                        if(w[k] > item.links.size() - cursor) { return error::invalid_shape; }
                        for(::std::uint64_t j{}; j != w[k]; ++j)
                        { if(!state_details::has_kind(snapshot, item.links[cursor++], kinds[k])) { return error::invalid_reference; } }
                    }
                    if(cursor != item.links.size()) { return error::invalid_shape; } break;
                }
                case object_kind::function:
                    if(item.flags > 1u || !zeros_from(3u) || !item.values.empty() || w[1] == 0u || item.links.empty() ||
                       !state_details::has_kind(snapshot, item.links[0], object_kind::instance) || w[0] > 0xffffffffu || w[2] > 0xffffffffu ||
                       (item.flags == 0u && item.links.size() != 1u) || (item.flags == 1u &&
                        (item.links.size() != 2u || !state_details::has_kind(snapshot, item.links[1], object_kind::host_resource) || !item.bytes.empty())))
                    { return error::invalid_shape; }
                    if(item.flags == 1u && state_details::lookup(snapshot,item.links[1u])->flags == 3u && w[1u] != 1u)
                    { return error::invalid_shape; } // Binding-only imports have original generation, no replacement body.
                    break;
                case object_kind::memory:
                case object_kind::table:
                {
                    if(item.flags > 1u || !zeros_from(4u) || !item.links.empty() || !item.bytes.empty() ||
                       (w[0] != 32u && w[0] != 64u) || w[1] < w[2] || w[1] > w[3] ||
                       (w[0] == 32u && w[3] > (item.kind == object_kind::memory ? 65536u : 0xffffffffu))) { return error::invalid_shape; }
                    if(item.kind == object_kind::memory)
                    { if(!item.values.empty() || w[3] > (::std::uint64_t{1u} << 48u) || (item.flags != 0u && w[3] == (::std::numeric_limits<::std::uint64_t>::max)())) { return error::invalid_shape; } }
                    else if(item.values.size() != 1u || item.values[0].type.kind != value_kind::reference || item.flags != 0u ||
                        item.values[0].initialized != (w[1] != 0u)) { return error::invalid_shape; }
                    break;
                }
                case object_kind::memory_chunk:
                case object_kind::table_chunk:
                {
                    auto const parent_kind{item.kind == object_kind::memory_chunk ? object_kind::memory : object_kind::table};
                    if(item.flags != 0u || !zeros_from(1u) || item.links.size() != 1u ||
                       !state_details::has_kind(snapshot, item.links[0], parent_kind)) { return error::invalid_shape; }
                    auto const parent_index{static_cast<::std::size_t>(item.links[0] - 1u)};
                    auto const& parent{snapshot.objects[parent_index]};
                    ::std::uint64_t size{};
                    if(item.kind == object_kind::memory_chunk)
                    {
                        if(!item.values.empty() || item.bytes.empty() || parent.words[1] > (::std::uint64_t{1u} << 48u))
                        { return error::invalid_shape; }
                        size = item.bytes.size();
                    }
                    else
                    {
                        if(!item.bytes.empty() || item.values.empty() || parent.values.size() != 1u) { return error::invalid_shape; }
                        size = item.values.size();
                        for(auto const& entry : item.values)
                        { if(entry.type != parent.values[0].type) { return error::incompatible_type; } }
                    }
                    if(chunk_seen[parent_index] && w[0] <= last_bytes[parent_index]) { return error::overlapping_chunks; }
                    if(size - 1u > (::std::numeric_limits<::std::uint64_t>::max)() - w[0]) { return error::invalid_shape; }
                    auto const last{w[0] + size - 1u};
                    // Inclusive last offsets preserve the valid final byte of
                    // a 2^64-byte memory without wrapping an exclusive end.
                    if(item.kind == object_kind::memory_chunk ? last / 65536u >= parent.words[1] : last >= parent.words[1])
                    { return error::invalid_shape; }
                    last_bytes[parent_index] = last; chunk_seen[parent_index] = true; break;
                }
                case object_kind::global:
                    if(item.flags > 1u || !zeros_from(0u) || !item.links.empty() || item.values.size() != 1u || !item.bytes.empty() ||
                       item.values[0].type.kind == value_kind::i8 || item.values[0].type.kind == value_kind::i16) { return error::invalid_shape; } break;
                case object_kind::tag:
                    if(item.flags != 0u || !zeros_from(1u) || w[0] > 0xffffffffu || item.links.size() != 1u || !links_are(object_kind::module) || !no_payload())
                    { return error::invalid_shape; } break;
                case object_kind::data:
                    if(item.flags > 1u || !zeros_from(0u) || !item.links.empty() || !item.values.empty() || (item.flags == 1u && !item.bytes.empty()))
                    { return error::invalid_shape; } break;
                case object_kind::element:
                    if(item.flags > 1u || !zeros_from(0u) || !item.links.empty() || !item.bytes.empty() || (item.flags == 1u && !item.values.empty()))
                    { return error::invalid_shape; }
                    for(auto const& entry : item.values) { if(entry.type.kind != value_kind::reference) { return error::invalid_shape; } } break;
                case object_kind::structure:
                case object_kind::array:
                    if(item.flags != 0u || !zeros_from(1u) || w[0] > 0xffffffffu || item.links.size() != 1u || !links_are(object_kind::module) || !item.bytes.empty())
                    { return error::invalid_shape; } break;
                case object_kind::exception:
                    if(item.flags != 0u || !zeros_from(0u) || item.links.empty() || item.links.size() > 2u ||
                       !state_details::has_kind(snapshot, item.links[0], object_kind::tag) || !item.bytes.empty() ||
                       (item.links.size() == 2u && !state_details::has_kind(snapshot, item.links[1], object_kind::exception_trace)))
                    { return error::invalid_shape; } break;
                case object_kind::external:
                {
                    // Core 3 ref.extern ref has exactly one NONNULL internal
                    // any reference. ref.host is a separate any reference.
                    // A wrapper cannot contain a wrapper, function, exception
                    // or numeric value. Checking the immediate typed value is
                    // bounded; no recursive wire traversal occurs, so hostile
                    // wrapper cycles fail while mutable GC cycles remain valid.
                    if(item.flags != 0u || !zeros_from(0u) || !item.links.empty() || item.values.size() != 1u || !item.bytes.empty())
                    { return error::invalid_shape; }
                    auto const& inner{item.values[0]};
                    if(inner.type.kind != value_kind::reference ||
                       (inner.reference != reference_kind::host && inner.reference != reference_kind::structure &&
                        inner.reference != reference_kind::array && inner.reference != reference_kind::i31))
                    { return error::incompatible_type; }
                    break;
                }
                case object_kind::host_reference:
                    if(item.flags != 0u || !zeros_from(1u) || w[0] == 0u || item.links.size() != 1u ||
                       !links_are(object_kind::host_resource) || !item.values.empty() ||
                       state_details::lookup(snapshot,item.links[0u])->flags == 3u)
                    { return error::invalid_shape; } break;
                case object_kind::thread:
                    if(item.flags > 3u || !zeros_from(2u) || !no_payload() || w[0] == 0u || w[0] >= snapshot.next_logical_thread ||
                       !state_details::charge(1u, cap.max_threads, threads)) { return error::invalid_shape; }
                    {
                        bool waiting{}, has_frame{};
                        for(auto id : item.links)
                        {
                            auto const kind{state_details::lookup(snapshot, id)->kind};
                            if(kind == object_kind::frame)
                            { if(waiting || item.flags == 3u) { return error::invalid_shape; } has_frame = true; }
                            else if(kind == object_kind::atomic_wait)
                            {
                                if(waiting || item.flags != 2u) { return error::invalid_shape; }
                                waiting = true; // At most one wait, after every live frame.
                            }
                            else { return error::invalid_reference; }
                            // All links passed the complete dense-ID check above.
                            auto const index{static_cast<::std::size_t>(id - 1u)};
                            if(continuation_owned[index]) { return error::invalid_shape; }
                            continuation_owned[index] = true;
                        }
                        if((item.flags == 2u) != waiting || (item.flags != 3u && !has_frame))
                        { return error::invalid_shape; }
                    }
                    logical_threads.push_back(w[0]); break;
                case object_kind::frame:
                    if(item.flags > 2u || !zeros_from(7u) || item.links.empty() || !item.bytes.empty() ||
                       !state_details::has_kind(snapshot, item.links[0], object_kind::function) ||
                       w[1] > item.values.size() || w[2] != item.values.size() - w[1] ||
                       w[3] > item.links.size() - 1u || w[4] != item.links.size() - 1u - w[3] ||
                       w[5] == 0u || w[5] != state_details::lookup(snapshot, item.links[0])->words[1] ||
                       !state_details::charge(1u, cap.max_frames, frames)) { return error::invalid_shape; }
                    for(::std::size_t j{1u}; j != item.links.size(); ++j)
                    {
                        auto const kind{j <= w[3] ? object_kind::control : object_kind::handler};
                        auto const id{item.links[j]};
                        if(!state_details::has_kind(snapshot, id, kind)) { return error::invalid_reference; }
                        // The record is a frame-owned continuation, never a
                        // shared control/handler template. Handler depth indexes
                        // this frame's own outer-to-inner control vector.
                        auto const index{static_cast<::std::size_t>(id - 1u)};
                        if(continuation_owned[index] || (kind == object_kind::handler && snapshot.objects[index].words[0u] >= w[3]))
                        { return error::invalid_shape; }
                        continuation_owned[index] = true;
                    } break;
                case object_kind::control:
                    if(item.flags > 3u || !zeros_from(4u) || !item.links.empty() || !item.bytes.empty() || w[2] != item.values.size() ||
                       w[3] > cap.max_values) { return error::invalid_shape; } break;
                case object_kind::handler:
                    if(item.flags > 3u || !zeros_from(3u) || !no_payload() ||
                       (w[2] != 0u && !state_details::has_kind(snapshot, w[2], object_kind::exception)) ||
                       (item.flags < 2u && (item.links.size() != 1u || !links_are(object_kind::tag))) ||
                       (item.flags >= 2u && !item.links.empty())) { return error::invalid_shape; } break;
                case object_kind::atomic_wait:
                    if(item.flags > 1u || !zeros_from(4u) || !item.values.empty() || !item.bytes.empty() || item.links.size() != 1u ||
                       !links_are(object_kind::memory) || (w[1] != 4u && w[1] != 8u) || (w[0] & (w[1] - 1u)) != 0u)
                    { return error::invalid_shape; }
                    {
                        auto const& memory{*state_details::lookup(snapshot, item.links[0])};
                        if(memory.flags != 1u || memory.words[1] > (::std::uint64_t{1u} << 48u))
                        { return error::invalid_shape; }
                        if(w[1] - 1u > (::std::numeric_limits<::std::uint64_t>::max)() - w[0] ||
                           (w[0] + w[1] - 1u) / 65536u >= memory.words[1] || (w[1] == 4u && w[2] > 0xffffffffu))
                        { return error::invalid_shape; }
                    } break;
                case object_kind::host_resource:
                    if(item.flags > 3u || !zeros_from(3u) || !item.links.empty() || !item.values.empty() || w[0] == 0u || w[1] == 0u ||
                       (item.flags == 3u && !state_details::valid_builtin_binding(item)))
                    { return error::invalid_shape; } break;
                case object_kind::event:
                    if(item.flags > 8u || w[0] == 0u || w[0] <= last_event || w[1] < last_event_instruction ||
                       w[4] > item.values.size() || w[5] != item.values.size() - w[4]) { return error::invalid_shape; }
                    for(auto id : item.links)
                    { auto const* resource{state_details::lookup(snapshot,id)};
                      if(resource->kind == object_kind::host_resource && resource->flags == 3u) { return error::invalid_shape; } }
                    last_event = w[0]; last_event_instruction = w[1]; ++event_count; break;
                case object_kind::exception_trace:
                    if(item.flags > 1u || !zeros_from(1u) || item.values.size() != item.links.size() ||
                        !links_are(object_kind::function) || !state_details::valid_trace_payload(snapshot, item))
                    { return error::invalid_shape; }
                    for(auto const& offset : item.values)
                    {
                        if(offset.type.kind != value_kind::i64 ||
                            (offset.low_bits != unknown_diagnostic_instruction && offset.low_bits > 0xffffffffu))
                        { return error::invalid_shape; }
                    } break;
            }
        }
        if(snapshot.replay_event_cursor > event_count) { return error::invalid_shape; }
        ::std::sort(logical_threads.begin(), logical_threads.end());
        for(::std::size_t i{}; i != logical_threads.size(); ++i)
        { if(i != 0u && logical_threads[i - 1u] == logical_threads[i]) { return error::invalid_shape; } }
        for(::std::size_t i{}; i != snapshot.objects.size(); ++i)
        {
            auto const kind{snapshot.objects[i].kind};
            if((kind == object_kind::frame || kind == object_kind::atomic_wait || kind == object_kind::control || kind == object_kind::handler) &&
               !continuation_owned[i]) { return error::invalid_shape; }
        }
        return error::none;
    }
}

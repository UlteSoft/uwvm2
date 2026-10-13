/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "checkpoint_state.h"
# include <fast_io.h>
# include <fast_io_crypto.h>
# include <algorithm>
# include <memory>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::checkpoint
{
    inline constexpr ::std::uint16_t database_format_version{6u};
    inline constexpr ::std::uint64_t database_magic{0x0036545350435755u}; // UWCPST6\0, canonical LE.
    inline constexpr ::std::uint64_t legacy_database_magic5{0x0035545350435755u}; // State5 is never silently upgraded.
    inline constexpr ::std::uint64_t legacy_database_magic4{0x0034545350435755u}; // State4 is never silently upgraded.
    inline constexpr ::std::uint64_t legacy_database_magic3{0x0033424450435755u}; // Never migrated.
    inline constexpr ::std::uint64_t commit_magic{0x3645545350435755u}; // UWCPSTE6.
    inline constexpr ::std::size_t header_bytes{128u}, footer_bytes{48u}, object_header_bytes{96u}, value_bytes{48u};
    namespace codec_details
    {
        [[nodiscard]] inline bool measure(state const& snapshot, limits const& cap, ::std::uint64_t& body) noexcept
        {
            body = 0u;
            auto count = [&](::std::uint64_t n, ::std::uint64_t width)
            {
                if(n > cap.max_file_bytes / width) { return false; }
                return state_details::charge(n * width, cap.max_file_bytes, body);
            };
            if(!count(snapshot.root_instances.size(), 8u) || !count(snapshot.retained_roots.size(), value_bytes)) { return false; }
            for(auto const& item : snapshot.objects)
            {
                if(!count(1u, object_header_bytes) || !count(item.links.size(), 8u) || !count(item.values.size(), value_bytes) ||
                   !state_details::charge(item.bytes.size(), cap.max_file_bytes, body)) { return false; }
            }
            return cap.max_file_bytes >= header_bytes + footer_bytes && body <= cap.max_file_bytes - header_bytes - footer_bytes;
        }
        // Only a borrowed live host-owned contiguous range is accepted. Wire
        // lengths are integers and never become pointers until <=remaining.
        class reader
        {
            unsigned char const* current_{};
            unsigned char const* end_{};
        public:
            explicit reader(::std::span<::std::byte const> bytes) noexcept
            {
                if(bytes.empty()) { return; }
                // [one owned/immutably mapped byte span ... size] end
                // [safe                                        ] unsafe (one-past)
                //  ^^ first; span extent is caller-proven and <=PTRDIFF_MAX.
                current_ = reinterpret_cast<unsigned char const*>(bytes.data());
                end_ = current_ + bytes.size();
            }
            [[nodiscard]] ::std::size_t remaining() const noexcept
            { return current_ == end_ ? 0u : static_cast<::std::size_t>(end_ - current_); }
            template<unsigned Bits, typename T> [[nodiscard]] bool get(T& output) noexcept
            {
                static_assert(Bits == 8u || Bits == 16u || Bits == 32u || Bits == 64u);
                if(remaining() < Bits / 8u) { return false; }
                T decoded{};
                // [consumed][at least Bits/8 bytes ...] end_
                // [safe                              ] unsafe (one-past)
                //            ^^ current_: length check precedes bounded LE read.
                auto const parsed{::fast_io::parse_by_scan(current_, end_, ::fast_io::mnp::le_get<Bits>(decoded))};
                if(parsed.code != ::fast_io::parse_code::ok || parsed.iter <= current_ || parsed.iter > end_ ||
                   static_cast<::std::size_t>(parsed.iter - current_) != Bits / 8u) { return false; }
                // [consumed including Bits/8 bytes][remaining ...] end_
                // [safe                                         ] unsafe (one-past)
                //                                  ^^ parsed.iter, committed only
                // after exact success and same-span bounds; never dereference end.
                current_ = parsed.iter;
                output = decoded;
                return true;
            }
            [[nodiscard]] bool bytes(::std::span<::std::byte> output) noexcept
            {
                if(output.size() > remaining()) { return false; }
                if(output.empty()) { return true; }
                // [at least output.size live bytes ...] end_
                // [safe                               ] unsafe (one-past)
                //  ^^ current_: both input/output extents precede this copy.
                // Checked nonempty output makes data()+size valid in its owner.
                auto* destination{reinterpret_cast<unsigned char*>(output.data())};
                auto* destination_end{destination + output.size()};
                ::fast_io::basic_ibuffer_view<unsigned char> input{current_, end_};
                ::fast_io::operations::read_all(input, destination, destination_end);
                // [consumed output.size][remaining ...] end_
                // [safe                              ] unsafe (one-past)
                //                        ^^ input.curr_ptr advanced by checked size.
                current_ = input.curr_ptr;
                return true;
            }
        };
        template<typename Output> class writer
        {
            Output& output_;
            ::fast_io::sha256_context hash_{};
        public:
            explicit writer(Output& output) noexcept : output_{output} {}
            void bytes(::std::span<::std::byte const> input)
            {
                if(input.empty()) { return; }
                // [one live input allocation: data ... size] end
                // [safe                                  ] unsafe (one-past)
                //  ^^ input.data(); caller-owned span extent precedes data()+size.
                auto const* first{input.data()};
                auto const* last{first + input.size()};
                ::fast_io::operations::write_all_bytes(output_, first, last);
                hash_.update(first, last);
            }
            template<unsigned Bits, typename T> void put(T input)
            {
                static_assert(Bits == 8u || Bits == 16u || Bits == 32u || Bits == 64u);
                ::std::array<unsigned char, Bits / 8u> scratch{};
                // [exact Bits/8 owned scratch bytes] end
                // [safe                            ] unsafe (one-past)
                //  ^^ output cursor; full fixed-width capacity is proven here.
                ::fast_io::basic_obuffer_view<unsigned char> output{scratch.data(), scratch.data() + scratch.size()};
                ::fast_io::io::print(output, ::fast_io::mnp::le_put<Bits>(input));
                // [Bits/8 encoded bytes] end
                // [safe                ] unsafe (one-past)
                //                        ^^ local output cursor moved exactly
                // Bits/8; fixed scratch span is retained only in this call.
                bytes({reinterpret_cast<::std::byte const*>(scratch.data()), scratch.size()});
            }
            [[nodiscard]] ::std::array<::std::byte, 32u> digest() noexcept
            {
                hash_.do_final();
                ::std::array<::std::byte, 32u> result{};
                hash_.digest_to_byte_ptr(result.data());
                return result;
            }
        };
        template<typename Output> void encode_value(writer<Output>& output, value const& item)
        {
            output.template put<8>(static_cast<::std::uint8_t>(item.type.kind));
            output.template put<8>(static_cast<::std::uint8_t>(item.type.heap));
            output.template put<8>(static_cast<::std::uint8_t>(item.type.nullable));
            output.template put<8>(static_cast<::std::uint8_t>(item.reference));
            output.template put<32>(::std::uint32_t{item.initialized ? 0u : 1u});
            output.template put<64>(item.type.type_module);
            output.template put<32>(item.type.type_index);
            output.template put<32>(::std::uint32_t{});
            output.template put<64>(item.target);
            output.template put<64>(item.low_bits);
            output.template put<64>(item.high_bits);
        }
        [[nodiscard]] inline bool decode_value(reader& input, value& item) noexcept
        {
            if(input.remaining() < value_bytes) { return false; }
            ::std::uint8_t kind{}, heap{}, nullable{}, reference{};
            ::std::uint32_t reserved1{}, reserved2{};
            if(!input.get<8>(kind) || !input.get<8>(heap) || !input.get<8>(nullable) || !input.get<8>(reference) ||
               !input.get<32>(reserved1) || !input.get<64>(item.type.type_module) || !input.get<32>(item.type.type_index) ||
               !input.get<32>(reserved2) || !input.get<64>(item.target) || !input.get<64>(item.low_bits) || !input.get<64>(item.high_bits) ||
               nullable > 1u || reserved1 > 1u || reserved2 != 0u) { return false; }
            item.type.kind = static_cast<value_kind>(kind); item.type.heap = static_cast<heap_kind>(heap);
            item.type.nullable = nullable != 0u; item.reference = static_cast<reference_kind>(reference);
            item.initialized = reserved1 == 0u;
            return true;
        }
    }
    // This stream interface has no file/path or VM authority. In production it
    // writes to the management-owned fast_io::obuf_file temporary transaction.
    // File publication happens only after success, explicit buffer flush, OS
    // file synchronization, checked close and atomic directory publication.
    template<typename Output> [[nodiscard]] error encode_database(Output& output, state const& snapshot, limits const& cap = {})
    {
        if(auto const status{validate_graph(snapshot, cap)}; status != error::none) { return status; }
        ::std::uint64_t body{};
        if(!codec_details::measure(snapshot, cap, body) || body > static_cast<::std::uint64_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) - header_bytes - footer_bytes)
        { return error::limit_exceeded; }
        codec_details::writer<Output> wire{output};
        wire.template put<64>(database_magic); wire.template put<16>(database_format_version);
        wire.template put<16>(static_cast<::std::uint16_t>(header_bytes)); wire.template put<32>(::std::uint32_t{0x04030201u});
        wire.template put<64>(::std::uint64_t{}); wire.bytes(snapshot.recording_id);
        for(auto number : {snapshot.checkpoint_id, snapshot.parent_checkpoint_id, snapshot.logical_instruction, snapshot.replay_event_cursor,
                          snapshot.required_features, snapshot.next_logical_thread, static_cast<::std::uint64_t>(snapshot.objects.size()),
                          static_cast<::std::uint64_t>(snapshot.root_instances.size()), static_cast<::std::uint64_t>(snapshot.retained_roots.size()), body, ::std::uint64_t{}})
        { wire.template put<64>(number); }
        for(auto id : snapshot.root_instances) { wire.template put<64>(id); }
        for(auto const& item : snapshot.retained_roots) { codec_details::encode_value(wire, item); }
        for(auto const& item : snapshot.objects)
        {
            wire.template put<16>(static_cast<::std::uint16_t>(item.kind)); wire.template put<16>(item.flags); wire.template put<32>(::std::uint32_t{});
            for(auto number : item.words) { wire.template put<64>(number); }
            wire.template put<64>(static_cast<::std::uint64_t>(item.links.size()));
            wire.template put<64>(static_cast<::std::uint64_t>(item.values.size()));
            wire.template put<64>(static_cast<::std::uint64_t>(item.bytes.size()));
            for(auto id : item.links) { wire.template put<64>(id); }
            for(auto const& field : item.values) { codec_details::encode_value(wire, field); }
            wire.bytes(item.bytes);
        }
        auto const digest{wire.digest()};
        ::fast_io::io::print(output, ::fast_io::mnp::le_put<64>(commit_magic), ::fast_io::mnp::le_put<64>(body));
        // [complete owned SHA-256 digest: 32 bytes] end
        // [safe                                 ] unsafe (one-past)
        //  ^^ digest.data(): exact array bounds precede the footer copy.
        ::fast_io::operations::write_all_bytes(output, digest.data(), digest.data() + digest.size());
        return error::none;
    }
    // The entire input must remain immutable for this synchronous call. A
    // file-backed mmap is not immutable merely because MAP_PRIVATE is used:
    // the trusted database owner must prevent replacement/truncation/writers.
    // Untrusted wire metadata never authenticates a management request.
    // Failure leaves the caller's prior state unchanged, including corruption
    // detected after complete parsing. A new detached state commits by move.
    [[nodiscard]] inline error decode_database(::std::span<::std::byte const> bytes, state& output, limits const& cap = {})
    {
        if(bytes.size() > cap.max_file_bytes || bytes.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()))
        { return error::limit_exceeded; }
        if(bytes.size() < header_bytes + footer_bytes) { return error::truncated; }
        codec_details::reader input{bytes};
        ::std::uint64_t magic{}, reserved{}, body{}, object_count{}, root_count{}, retained_count{};
        ::std::uint16_t version{}, header{};
        ::std::uint32_t endian{};
        state candidate{};
        if(!input.get<64>(magic) || !input.get<16>(version) || !input.get<16>(header) || !input.get<32>(endian) || !input.get<64>(reserved))
        { return error::truncated; }
        if(magic == legacy_database_magic3 || magic == legacy_database_magic4 || magic == legacy_database_magic5) { return error::unsupported_version; }
        if(magic != database_magic || endian != 0x04030201u || reserved != 0u) { return error::malformed; }
        if(version != database_format_version || header != header_bytes) { return error::unsupported_version; }
        if(!input.bytes(candidate.recording_id) || !input.get<64>(candidate.checkpoint_id) || !input.get<64>(candidate.parent_checkpoint_id) ||
           !input.get<64>(candidate.logical_instruction) || !input.get<64>(candidate.replay_event_cursor) || !input.get<64>(candidate.required_features) ||
           !input.get<64>(candidate.next_logical_thread) || !input.get<64>(object_count) || !input.get<64>(root_count) || !input.get<64>(retained_count) ||
           !input.get<64>(body) || !input.get<64>(reserved)) { return error::truncated; }
        if(reserved != 0u || body != input.remaining() - footer_bytes) { return error::incomplete_commit; }
        if(object_count > cap.max_objects || root_count > cap.max_links || retained_count > cap.max_values ||
           object_count > body / object_header_bytes || root_count > body / 8u || retained_count > body / value_bytes)
        { return error::limit_exceeded; }
        // [128-byte header][exact bounded body][48-byte commit footer] end
        // [safe                                                     ] unsafe (one-past)
        //                   ^^ body_begin      ^^ footer_begin
        // body equals available-minus-footer BEFORE either offset is formed.
        auto const body_size{static_cast<::std::size_t>(body)};
        auto const* footer_begin{bytes.data() + header_bytes + body_size};
        codec_details::reader footer{{footer_begin, footer_bytes}};
        ::std::uint64_t commit{}, committed_body{};
        ::std::array<::std::byte, 32u> expected{}, actual{};
        if(!footer.get<64>(commit) || !footer.get<64>(committed_body) || !footer.bytes(expected) || commit != commit_magic || committed_body != body)
        { return error::incomplete_commit; }
        ::fast_io::sha256_context hash{};
        // [same immutable owner: header and body] footer_begin
        // [safe                                ]
        //  ^^ first: checked footer_begin is one-past hashed bytes, not read.
        hash.update(bytes.data(), footer_begin); hash.do_final(); hash.digest_to_byte_ptr(actual.data());
        if(actual != expected) { return error::digest_mismatch; }
        codec_details::reader payload{{bytes.data() + header_bytes, body_size}};
        ::std::uint64_t links_used{root_count}, values_used{retained_count}, bytes_used{};
        candidate.root_instances.resize(static_cast<::std::size_t>(root_count));
        for(auto& id : candidate.root_instances) { if(!payload.get<64>(id)) { return error::truncated; } }
        if(retained_count > payload.remaining() / value_bytes) { return error::truncated; }
        candidate.retained_roots.resize(static_cast<::std::size_t>(retained_count));
        for(auto& item : candidate.retained_roots) { if(!codec_details::decode_value(payload, item)) { return error::malformed; } }
        if(object_count > payload.remaining() / object_header_bytes) { return error::truncated; }
        candidate.objects.resize(static_cast<::std::size_t>(object_count));
        for(auto& item : candidate.objects)
        {
            ::std::uint16_t kind{};
            ::std::uint32_t zero{};
            ::std::uint64_t links{}, values{}, size{};
            if(!payload.get<16>(kind) || !payload.get<16>(item.flags) || !payload.get<32>(zero) || zero != 0u) { return error::malformed; }
            item.kind = static_cast<object_kind>(kind);
            if(!known(item.kind)) { return error::malformed; }
            for(auto& number : item.words) { if(!payload.get<64>(number)) { return error::truncated; } }
            if(!payload.get<64>(links) || !payload.get<64>(values) || !payload.get<64>(size)) { return error::truncated; }
            if(!state_details::charge(links, cap.max_links, links_used) || !state_details::charge(values, cap.max_values, values_used) ||
               !state_details::charge(size, cap.max_payload_bytes, bytes_used)) { return error::limit_exceeded; }
            if(links > payload.remaining() / 8u) { return error::truncated; }
            item.links.resize(static_cast<::std::size_t>(links));
            for(auto& id : item.links) { if(!payload.get<64>(id)) { return error::truncated; } }
            if(values > payload.remaining() / value_bytes) { return error::truncated; }
            item.values.resize(static_cast<::std::size_t>(values));
            for(auto& field : item.values) { if(!codec_details::decode_value(payload, field)) { return error::malformed; } }
            if(size > payload.remaining()) { return error::truncated; }
            item.bytes.resize(static_cast<::std::size_t>(size));
            if(!payload.bytes(item.bytes)) { return error::truncated; }
        }
        if(payload.remaining() != 0u) { return error::trailing_bytes; }
        if(auto const status{validate_graph(candidate, cap)}; status != error::none) { return status; }
        output = ::std::move(candidate);
        return error::none;
    }
    // A loader is a move-only RAII mapping owner. This overload does not open a
    // path, infer sealing from a filename, or grant guest access to host files.
    [[nodiscard]] inline error decode_database(::fast_io::native_file_loader const& mapping, state& output, limits const& cap = {})
    { return decode_database({reinterpret_cast<::std::byte const*>(mapping.data()), mapping.size()}, output, cap); }
}

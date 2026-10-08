// Management request DATA only. A request, source path or detached reply cannot
// authorize a write; the runtime repeats the complete current stop proof.
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <string>
# include <utility>
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include "wasm_state.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::wasm_mutation
{
    inline constexpr unsigned protocol_version{3u};
    enum class destination : unsigned char { global, table, member, member_path, memory };
    enum class source_kind : unsigned char { numeric_bits, null, i31, function, original_root, original_path, bytes };
    enum class refusal : unsigned char
    { none, immutable_global, type_mismatch, unavailable_source, retention_failed, immutable_member };
    struct request
    {
        destination target{destination::global};
        ::std::uint64_t participant{}, module{}, index{}, element{};
        source_kind source{source_kind::null};
        wasm_state::value_kind numeric_kind{wasm_state::value_kind::i32};
        ::std::array<::std::byte, 16u> bits{}; // LE numeric/vector bits; NEVER reference payload.
        ::std::uint32_t i31_bits{};
        inline static constexpr ::std::size_t maximum_memory_bytes{256u};
        ::std::array<::std::byte,maximum_memory_bytes> memory_bytes{};
        ::std::uint16_t memory_size{}; // Owned raw bytes, not a native pointer or numeric carrier.
        ::std::uint64_t function_module{}, function_index{};
        wasm_state::request target_original{}; // Actual root/path rematerialized under fresh N, never an object reply ID.
        ::std::uint64_t target_path_session{},target_path_handle{};
        ::std::array<::std::uint64_t,wasm_state::maximum_path> target_path_suffix{};
        unsigned char target_path_suffix_size{};
        wasm_state::request original{}; // Original typed root + immutable member indices only.
        // Session-local DATA only. Runtime API rejects an unresolved path label;
        // controller resolves immutable indices before its ordinary fresh borrow.
        ::std::uint64_t path_session{}, path_handle{};
        ::std::array<::std::uint64_t,wasm_state::maximum_path> path_suffix{};
        unsigned char path_suffix_size{};
    };
    struct result
    {
        wasm_state::status status{wasm_state::status::requires_current_cooperative_stop};
        refusal reason{refusal::none};
        ::std::uint64_t runtime_epoch{}, module{}, index{}, element{};
        destination target{destination::global};
        ::std::uint16_t memory_size{};
        unsigned char address_bytes{}; // Actual selected memory declaration, never supplied authority.
        bool applied{}; // Set only AFTER the one complete real slot commit.
    };
    [[nodiscard]] constexpr bool valid(request const& input) noexcept
    {
        if(input.participant==0u || static_cast<unsigned>(input.target)>static_cast<unsigned>(destination::memory) ||
           static_cast<unsigned>(input.source)>static_cast<unsigned>(source_kind::bytes) ||
           (input.target==destination::global && input.element!=0u)) { return false; }
        if(input.target==destination::memory)
        {
            if(input.source!=source_kind::bytes || input.memory_size==0u || input.memory_size>input.memory_bytes.size() ||
               !input.target_original.long_path.empty() || !input.original.long_path.empty() || input.target_original.path_size!=0u ||
               input.original.path_size!=0u || input.target_original.compressed_path() || input.original.compressed_path() ||
               input.target_path_session!=0u || input.target_path_handle!=0u || input.path_session!=0u || input.path_handle!=0u ||
               input.target_path_suffix_size!=0u || input.path_suffix_size!=0u) { return false; }
            for(::std::size_t i{input.memory_size};i!=input.memory_bytes.size();++i)
            { if(input.memory_bytes[i]!=::std::byte{}) { return false; } }
            return true; // Runtime repeats actual address type/extent/current owner proof.
        }
        if(input.source==source_kind::bytes || input.memory_size!=0u) { return false; }
        for(auto byte:input.memory_bytes) { if(byte!=::std::byte{}) { return false; } }
        // An inactive copied root must not hide an unbounded allocated path.
        // Controller label resolution copies this request only under catch;
        // these guards make that copy bounded even for native API callers.
        if(input.target!=destination::member && input.target!=destination::member_path &&
           !input.target_original.long_path.empty()) { return false; }
        if(input.source!=source_kind::original_root && input.source!=source_kind::original_path &&
           !input.original.long_path.empty()) { return false; }
        if(input.target==destination::member || input.target==destination::member_path)
        {
            if(input.module!=0u || input.index!=0u) { return false; }
            if(input.target==destination::member)
            {
                if(!wasm_state::valid(input.target_original) || !wasm_state::value_selection(input.target_original.selected) ||
                   input.target_original.participant!=input.participant || input.target_original.count!=1u ||
                   input.target_original.member_first!=0u || input.target_original.member_count!=1u) { return false; }
            }
            else
            {
                if(input.target_path_session==0u || input.target_path_handle==0u ||
                   input.target_path_suffix_size>input.target_path_suffix.size() || !input.target_original.long_path.empty() ||
                   input.target_original.path_size!=0u || input.target_original.compressed_path()) { return false; }
                for(::std::size_t i{input.target_path_suffix_size};i!=input.target_path_suffix.size();++i)
                { if(input.target_path_suffix[i]!=0u) { return false; } }
            }
        }
        if(input.source==source_kind::numeric_bits)
        {
            if(input.numeric_kind==wasm_state::value_kind::reference || input.target==destination::table ||
               static_cast<unsigned>(input.numeric_kind)>static_cast<unsigned>(wasm_state::value_kind::v128)) { return false; }
            auto const width{input.numeric_kind==wasm_state::value_kind::i32 || input.numeric_kind==wasm_state::value_kind::f32 ? 4u :
                input.numeric_kind==wasm_state::value_kind::v128 ? 16u : 8u};
            for(::std::size_t i{width};i!=input.bits.size();++i) { if(input.bits[i]!=::std::byte{}) { return false; } }
        }
        if(input.source==source_kind::i31 && input.i31_bits>0x7fffffffu) { return false; }
        if(input.source==source_kind::original_path)
        {
            if(input.path_session==0u || input.path_handle==0u || input.path_suffix_size>input.path_suffix.size() ||
               !input.original.long_path.empty() || input.original.path_size!=0u || input.original.compressed_path()) { return false; }
            for(::std::size_t i{input.path_suffix_size};i!=input.path_suffix.size();++i)
            { if(input.path_suffix[i]!=0u) { return false; } }
            return true;
        }
        if(input.source==source_kind::original_root)
        {
            return wasm_state::valid(input.original) && wasm_state::value_selection(input.original.selected) &&
                input.original.participant==input.participant && input.original.count==1u &&
                input.original.member_first==0u && input.original.member_count<=1u;
        }
        return true;
    }
    namespace details
    {
        template<bool Hex, typename Integer>
        [[nodiscard]] constexpr bool number(::fast_io::string_view text,Integer& out) noexcept
        {
            if(text.empty() || text.size()>512u) { return false; }
            Integer candidate{};
            // [owned token bytes0..size][one-past] end
            // [safe] complete string_view extent BEFORE forming end.
            auto const* end{text.data()+text.size()};
            auto parsed{[&] {
                if constexpr(Hex) { return ::fast_io::parse_by_scan(text.data(),end,::fast_io::mnp::hex_get<true,true>(candidate)); }
                else { return ::fast_io::parse_by_scan(text.data(),end,::fast_io::mnp::dec_get<true,true>(candidate)); }
            }()};
            if(parsed.code!=::fast_io::parse_code::ok || parsed.iter!=end) { return false; }
            out=candidate;return true;
        }
        template<unsigned Bits,typename Integer>
        constexpr void put(::std::array<::std::byte,16u>& bytes,::std::size_t offset,Integer value) noexcept
        {
            static_assert(Bits==32u || Bits==64u);
            if(offset>bytes.size() || Bits/8u>bytes.size()-offset) { return; }
            // Actual full subtraction bound, including offset0/8 callers.
            // [owned16 byte array][offset..offset+width][one-past] end
            // [safe] invariant is fixed at each call, BEFORE pointer advance.
            auto* first{reinterpret_cast<unsigned char*>(bytes.data())+offset};
            ::fast_io::basic_obuffer_view<unsigned char> output{first,first+Bits/8u};
            ::fast_io::io::print(output,::fast_io::mnp::le_put<Bits>(value));
        }
    }
    // `set wasm global M G T bits i32 HEX`; table inserts ELEMENT before T.
    // Reference sources are null, i31 U31, function M F, or
    // `from locals|operands|saved FRAME INDEX`, `from globals M INDEX`,
    // `from table M TABLE ELEMENT`, optionally `path` and <=16 original edges.
    // `from handle SESSION HANDLE [path EDGES...]` resolves original DATA anew.
    // `set wasm member T ROOT [path EDGES...] at MEMBER SOURCE` accepts the
    // same immutable original roots, or a current session-local path label.
    // Member reply index is the actual owner module's canonical type index;
    // it is DATA and never an object token/native-address selector.
    // `set wasm memory MODULE MEMORY THREAD OFFSET bytes HEX` selects the actual
    // Wasm memory index space; HEX owns1..256 bytes in address order.
    // No machine address, dense reply object ID, script evaluator or host call.
    [[nodiscard]] inline constexpr bool parse(::fast_io::string_view line,request& output) noexcept
    {
        if(line.size()>1024u) { return false; }
        ::std::array<::fast_io::string_view,64u> tokens{};::std::size_t cursor{},count{};
        while(cursor<line.size())
        {
            if(::fast_io::char_category::is_c_blank(line[cursor])) { ++cursor;continue; }
            auto const begin{cursor};
            while(cursor<line.size() && !::fast_io::char_category::is_c_blank(line[cursor])) { ++cursor; }
            if(count==tokens.size()) { return false; }
            tokens[count++]=line.subview(begin,cursor-begin); // bounded begin<=cursor<=size BEFORE view.
        }
        if(count<7u || tokens[0]!="set" || tokens[1]!="wasm") { return false; }
        request next{};
        // Each textual pair is parsed by FastIO. Preserve byte order exactly
        // on either host endian; a request contains no interpreted host address.
        if(tokens[2]=="memory")
        {
            next.target=destination::memory;next.source=source_kind::bytes;
            if(count!=9u || !details::number<false>(tokens[3],next.module) || !details::number<false>(tokens[4],next.index) ||
               !details::number<false>(tokens[5],next.participant) || !details::number<false>(tokens[6],next.element) || tokens[7]!="bytes") { return false; }
            auto const hex{tokens[8]};
            if(hex.empty() || hex.size()%2u!=0u || hex.size()/2u>next.memory_bytes.size()) { return false; }
            auto const size{hex.size()/2u};next.memory_size=static_cast<::std::uint16_t>(size);
            for(::std::size_t i{};i!=size;++i)
            {
                // [owned even hex token][2*i..2*i+2<=size] one-past
                // [safe] i<size/2 and size<=512 BEFORE subview/index.
                ::std::uint8_t byte{};
                if(!details::number<true>(hex.subview(i*2u,2u),byte)) { return false; }
                next.memory_bytes[i]=static_cast<::std::byte>(byte);
            }
            if(!valid(next)) { return false; }output=::std::move(next);return true;
        }
        if(line.size()>512u) { return false; } // Other mutation grammars retain their original legacy budget.
        ::std::size_t current{};
        if(tokens[2]=="member")
        {
            next.target=destination::member;
            if(!details::number<false>(tokens[3],next.participant)) { return false; }
            current=4u;if(current>=count) { return false; }
            auto const kind{tokens[current++]};auto& root{next.target_original};
            root.participant=next.participant;root.count=1u;root.member_count=1u;
            if(kind=="handle")
            {
                next.target=destination::member_path;
                if(count-current<2u || !details::number<false>(tokens[current++],next.target_path_session) ||
                   !details::number<false>(tokens[current++],next.target_path_handle)) { return false; }
            }
            else if(kind=="locals" || kind=="operands" || kind=="saved")
            {
                root.selected=kind=="locals" ? wasm_state::selection::locals : kind=="saved" ? wasm_state::selection::saved_parameters : wasm_state::selection::operands;
                if(count-current<2u || !details::number<false>(tokens[current++],root.frame) ||
                   !details::number<false>(tokens[current++],root.first)) { return false; }
            }
            else if(kind=="globals" || kind=="table")
            {
                root.selected=kind=="globals" ? wasm_state::selection::globals : wasm_state::selection::table;
                if(current>=count || !details::number<false>(tokens[current++],root.module)) { return false; }
                if(kind=="table")
                { if(current>=count || !details::number<false>(tokens[current++],root.index)) { return false; } }
                if(current>=count || !details::number<false>(tokens[current++],root.first)) { return false; }
            }
            else { return false; }
            if(current<count && tokens[current]=="path")
            {
                ++current;unsigned edges{};
                while(current<count && tokens[current]!="at")
                {
                    if(edges==wasm_state::maximum_path) { return false; }
                    auto& edge{next.target==destination::member_path ? next.target_path_suffix[edges] : root.path[edges]};
                    if(!details::number<false>(tokens[current++],edge)) { return false; }++edges;
                }
                if(edges==0u) { return false; }
                if(next.target==destination::member_path) { next.target_path_suffix_size=static_cast<unsigned char>(edges); }
                else { root.path_size=static_cast<unsigned char>(edges); }
            }
            if(current>=count || tokens[current++]!="at" || current>=count ||
               !details::number<false>(tokens[current++],next.element) || current>=count) { return false; }
        }
        else
        {
            if(tokens[2]=="global") { next.target=destination::global; }
            else if(tokens[2]=="table") { next.target=destination::table; }
            else { return false; }
            if(!details::number<false>(tokens[3],next.module) || !details::number<false>(tokens[4],next.index)) { return false; }
            current=5u;
            if(next.target==destination::table)
            { if(current>=count || !details::number<false>(tokens[current++],next.element)) { return false; } }
            if(current>=count || !details::number<false>(tokens[current++],next.participant) || current>=count) { return false; }
        }
        auto const source{tokens[current++]};
        if(source=="null") { next.source=source_kind::null; }
        else if(source=="i31")
        { next.source=source_kind::i31;if(current>=count || !details::number<false>(tokens[current++],next.i31_bits)) { return false; } }
        else if(source=="function")
        {
            next.source=source_kind::function;
            if(count-current<2u || !details::number<false>(tokens[current++],next.function_module) ||
               !details::number<false>(tokens[current++],next.function_index)) { return false; }
        }
        else if(source=="bits")
        {
            next.source=source_kind::numeric_bits;
            if(count-current<2u) { return false; }
            auto const kind{tokens[current++]};
            if(kind=="i32" || kind=="f32")
            {
                next.numeric_kind=kind=="i32" ? wasm_state::value_kind::i32 : wasm_state::value_kind::f32;
                ::std::uint32_t bits{};if(!details::number<true>(tokens[current++],bits)) { return false; }
                details::put<32u>(next.bits,0u,bits);
            }
            else if(kind=="i64" || kind=="f64" || kind=="v128")
            {
                next.numeric_kind=kind=="i64" ? wasm_state::value_kind::i64 : kind=="f64" ? wasm_state::value_kind::f64 : wasm_state::value_kind::v128;
                ::std::uint64_t bits{};if(!details::number<true>(tokens[current++],bits)) { return false; }
                details::put<64u>(next.bits,0u,bits);
                if(kind=="v128")
                { if(current>=count || !details::number<true>(tokens[current++],bits)) { return false; }details::put<64u>(next.bits,8u,bits); }
            }
            else { return false; }
        }
        else if(source=="from")
        {
            next.source=source_kind::original_root;auto& root{next.original};root.participant=next.participant;root.count=1u;
            if(count-current<3u) { return false; }auto const kind{tokens[current++]};
            if(kind=="handle")
            {
                next.source=source_kind::original_path;
                if(!details::number<false>(tokens[current++],next.path_session) ||
                   !details::number<false>(tokens[current++],next.path_handle)) { return false; }
                if(current<count)
                {
                    if(tokens[current++]!="path" || current==count || count-current>next.path_suffix.size()) { return false; }
                    next.path_suffix_size=static_cast<unsigned char>(count-current);
                    for(::std::size_t i{};i!=next.path_suffix_size;++i)
                    { if(!details::number<false>(tokens[current++],next.path_suffix[i])) { return false; } }
                }
                if(current!=count || !valid(next)) { return false; }
                output=::std::move(next);return true; // No allocated path in parser.
            }
            else if(kind=="locals" || kind=="operands" || kind=="saved")
            {
                root.selected=kind=="locals" ? wasm_state::selection::locals : kind=="saved" ? wasm_state::selection::saved_parameters : wasm_state::selection::operands;
                if(!details::number<false>(tokens[current++],root.frame) || !details::number<false>(tokens[current++],root.first)) { return false; }
            }
            else if(kind=="globals" || kind=="table")
            {
                root.selected=kind=="globals" ? wasm_state::selection::globals : wasm_state::selection::table;
                if(!details::number<false>(tokens[current++],root.module)) { return false; }
                if(kind=="table")
                { if(count-current<2u || !details::number<false>(tokens[current++],root.index)) { return false; } }
                if(current>=count || !details::number<false>(tokens[current++],root.first)) { return false; }
            }
            else { return false; }
            if(current<count)
            {
                if(tokens[current++]!="path" || current==count || count-current>wasm_state::maximum_path) { return false; }
                root.member_count=1u;root.path_size=static_cast<unsigned char>(count-current);
                for(::std::size_t i{};i!=root.path_size;++i)
                { if(!details::number<false>(tokens[current++],root.path[i])) { return false; } }
            }
        }
        else { return false; }
        if(current!=count || !valid(next)) { return false; }
        output=::std::move(next);return true; // Parser owns no allocated long path.
    }
    [[nodiscard]] constexpr ::fast_io::string_view refusal_text(refusal value) noexcept
    {
        switch(value)
        {
            case refusal::none:return "none";
            case refusal::immutable_global:return "global is immutable";
            case refusal::immutable_member:return "GC member is immutable";
            case refusal::type_mismatch:return "actual value does not match the canonical destination type";
            case refusal::unavailable_source:return "original current typed source is unavailable";
            case refusal::retention_failed:return "reference retention failed; destination retained its old value";
        }
        return "invalid refusal";
    }
    [[nodiscard]] inline ::std::string format(result const& out)
    {
        return ::fast_io::concat_std("Wasm mutation v=",protocol_version," status=",wasm_state::status_text(out.status),
            " applied=",out.applied ? 1u : 0u," reason=",refusal_text(out.reason)," runtime=",out.runtime_epoch,
            " module=",out.module," target=",out.target==destination::global ? ::fast_io::string_view{"global"} : out.target==destination::table ? ::fast_io::string_view{"table"} : out.target==destination::memory ? ::fast_io::string_view{"memory"} : ::fast_io::string_view{"member"},
            " index=",out.index," element=",out.element," bytes=",out.memory_size," address-bytes=",static_cast<unsigned>(out.address_bytes),"\n");
    }
}

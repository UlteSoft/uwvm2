/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_unit/string.h>
# include <fast_io_dsal/string.h>
# include <algorithm>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <string_view>
# include <utility>
# include <vector>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_map_v3
{
    // Source Map v3 describes display locations only. It cannot mint DWARF
    // types, variable locations, inline DIEs, native PCs or guest read leases.
    enum class error { none, malformed, unsupported, limit_exceeded };
    struct row
    {
        ::std::uint64_t begin{}, end{}, line{}, column{};
        ::std::size_t file{};
    };
    struct image
    {
        ::std::vector<::fast_io::string> sources{};
        ::fast_io::string source_root{};
        ::std::vector<row> rows{};
    };
    inline ::std::string_view view(::fast_io::string const& value) noexcept
    { return {value.data(),value.size()}; }
    namespace detail
    {
        inline bool utf8(::std::string_view text) noexcept
        {
            for(::std::size_t i{};i<text.size();)
            {
                auto const first{static_cast<unsigned char>(text[i++])};
                if(first<128u) { continue; }
                unsigned count{};::std::uint32_t scalar{},minimum{};
                if(first>=0xc2u && first<=0xdfu) { count=1u;scalar=first&31u;minimum=128u; }
                else if(first>=0xe0u && first<=0xefu) { count=2u;scalar=first&15u;minimum=2048u; }
                else if(first>=0xf0u && first<=0xf4u) { count=3u;scalar=first&7u;minimum=65536u; }
                else { return false; }
                if(count>text.size()-i) { return false; }
                while(count--)
                {
                    auto const c{static_cast<unsigned char>(text[i++])};
                    if((c&0xc0u)!=0x80u) { return false; }
                    scalar=(scalar<<6u)|(c&63u);
                }
                if(scalar<minimum || scalar>0x10ffffu || (scalar>=0xd800u && scalar<=0xdfffu)) { return false; }
            }
            return true;
        }
        struct json_reader
        {
            ::std::string_view text{};::std::size_t at{};
            void space() noexcept
            { while(at<text.size() && (text[at]==' ' || text[at]=='\t' || text[at]=='\n' || text[at]=='\r')) { ++at; } }
            bool take(char c) noexcept
            { space();if(at==text.size() || text[at]!=c) { return false; }++at;return true; }
            bool hex(::std::uint32_t& out) noexcept
            {
                if(text.size()-at<4u) { return false; }
                auto const first{text.data()+at};auto const last{first+4u};
                auto const result{::fast_io::parse_by_scan(first,last,::fast_io::mnp::hex_get<true,true>(out))};
                if(result.code!=::fast_io::parse_code::ok || result.iter!=last) { return false; }
                at+=4u;return true;
            }
            bool string(::fast_io::string& out,::std::size_t cap)
            {
                out.clear();if(!take('"')) { return false; }
                while(at<text.size())
                {
                    auto c{static_cast<unsigned char>(text[at++])};
                    if(c=='"') { return utf8(view(out)); }
                    if(c<32u || out.size()>=cap) { return false; }
                    if(c!='\\') { out.push_back(static_cast<char>(c));continue; }
                    if(at==text.size()) { return false; }c=static_cast<unsigned char>(text[at++]);
                    switch(c)
                    {
                        case '"':case '\\':case '/':out.push_back(static_cast<char>(c));break;
                        case 'b':out.push_back('\b');break;case 'f':out.push_back('\f');break;
                        case 'n':out.push_back('\n');break;case 'r':out.push_back('\r');break;case 't':out.push_back('\t');break;
                        case 'u':
                        {
                            ::std::uint32_t scalar{};if(!hex(scalar)) { return false; }
                            if(scalar>=0xd800u && scalar<=0xdbffu)
                            {
                                if(text.size()-at<2u || text[at]!='\\' || text[at+1u]!='u') { return false; }at+=2u;
                                ::std::uint32_t low{};if(!hex(low) || low<0xdc00u || low>0xdfffu) { return false; }
                                scalar=65536u+((scalar-0xd800u)<<10u)+(low-0xdc00u);
                            }
                            else if(scalar>=0xdc00u && scalar<=0xdfffu) { return false; }
                            if(scalar<128u) { out.push_back(static_cast<char>(scalar)); }
                            else if(scalar<2048u)
                            { out.push_back(static_cast<char>(0xc0u|(scalar>>6u)));out.push_back(static_cast<char>(0x80u|(scalar&63u))); }
                            else if(scalar<65536u)
                            { out.push_back(static_cast<char>(0xe0u|(scalar>>12u)));out.push_back(static_cast<char>(0x80u|((scalar>>6u)&63u)));out.push_back(static_cast<char>(0x80u|(scalar&63u))); }
                            else
                            { out.push_back(static_cast<char>(0xf0u|(scalar>>18u)));out.push_back(static_cast<char>(0x80u|((scalar>>12u)&63u)));out.push_back(static_cast<char>(0x80u|((scalar>>6u)&63u)));out.push_back(static_cast<char>(0x80u|(scalar&63u))); }
                            if(out.size()>cap) { return false; }break;
                        }
                        default:return false;
                    }
                }
                return false;
            }
            bool number(::std::string_view& out) noexcept
            {
                space();auto const begin{at};if(at<text.size() && text[at]=='-') { ++at; }
                if(at==text.size()) { return false; }
                if(text[at]=='0') { ++at; }
                else
                {
                    if(text[at]<'1' || text[at]>'9') { return false; }
                    do { ++at; }while(at<text.size() && text[at]>='0' && text[at]<='9');
                }
                for(char marker:{'.','e'})
                {
                    bool const found{at<text.size() && (text[at]==marker || (marker=='e' && text[at]=='E'))};
                    if(!found) { continue; }++at;
                    if(marker=='e' && at<text.size() && (text[at]=='+' || text[at]=='-')) { ++at; }
                    auto const digits{at};while(at<text.size() && text[at]>='0' && text[at]<='9') { ++at; }
                    if(at==digits) { return false; }
                }
                out=text.substr(begin,at-begin);return true;
            }
            bool skip(unsigned depth=0u)
            {
                if(depth>=32u) { return false; }space();if(at==text.size()) { return false; }
                if(text[at]=='"') { ::fast_io::string ignored{};return string(ignored,16u*1024u*1024u); }
                if(text[at]=='[' || text[at]=='{')
                {
                    bool const object{text[at++]=='{'};char const end{object?'}':']'};
                    if(take(end)) { return true; }
                    do
                    {
                        if(object) { ::fast_io::string key{};if(!string(key,4096u) || !take(':')) { return false; } }
                        if(!skip(depth+1u)) { return false; }
                        if(take(end)) { return true; }
                    }while(take(','));
                    return false;
                }
                for(auto literal:{::std::string_view{"true"},::std::string_view{"false"},::std::string_view{"null"}})
                { if(text.substr(at).starts_with(literal)) { at+=literal.size();return true; } }
                ::std::string_view value{};return number(value);
            }
            bool strings(::std::vector<::fast_io::string>& out,::std::size_t& total)
            {
                if(!take('[')) { return false; }if(take(']')) { return true; }
                do
                {
                    ::fast_io::string entry{};if(out.size()==65536u || !string(entry,4096u) || entry.size()>8u*1024u*1024u-total) { return false; }
                    total+=entry.size();out.push_back(::std::move(entry));
                    if(take(']')) { return true; }
                }while(take(','));
                return false;
            }
        };
        inline int base64(char c) noexcept
        {
            if(c>='A' && c<='Z') { return c-'A'; }if(c>='a' && c<='z') { return c-'a'+26; }
            if(c>='0' && c<='9') { return c-'0'+52; }if(c=='+') { return 62; }if(c=='/') { return 63; }return -1;
        }
        inline bool vlq(::std::string_view text,::std::size_t& at,::std::int64_t& out) noexcept
        {
            ::std::uint64_t encoded{};unsigned shift{};
            for(unsigned n{};n!=13u;++n)
            {
                if(at==text.size()) { return false; }int const value{base64(text[at++])};if(value<0) { return false; }
                auto const payload{static_cast<::std::uint64_t>(value&31)};
                if(payload>((::std::numeric_limits<::std::uint64_t>::max)()>>shift)) { return false; }
                encoded|=payload<<shift;
                if((value&32)==0)
                {
                    auto const magnitude{encoded>>1u};
                    if((encoded&1u) && magnitude==0u) { return false; }
                    out=(encoded&1u)?-static_cast<::std::int64_t>(magnitude):static_cast<::std::int64_t>(magnitude);return true;
                }
                shift+=5u;
            }
            return false;
        }
        inline bool add(::std::int64_t& value,::std::int64_t delta) noexcept
        {
            if(delta>0 && value>(::std::numeric_limits<::std::int64_t>::max)()-delta) { return false; }
            if(delta<0 && value<(::std::numeric_limits<::std::int64_t>::min)()-delta) { return false; }
            value+=delta;return value>=0;
        }
    }
    // Generated columns are offsets in the WHOLE Wasm binary. The caller's
    // checked loader-owned Code span supplies the sole coordinate conversion.
    // On every failure output remains unchanged. No files are opened here.
    inline error parse(::std::string_view text,::std::uint64_t code_file_begin,
        ::std::uint64_t code_size,::std::uint64_t file_size,image& output)
    {
        if(text.size()>16u*1024u*1024u) { return error::limit_exceeded; }
        if(code_file_begin>file_size || code_size>file_size-code_file_begin) { return error::malformed; }
        image candidate{};::fast_io::string mappings{};::std::vector<::fast_io::string> names{},keys{};
        detail::json_reader input{text};bool version{},sources{},mapped{};::std::size_t total{};
        if(!input.take('{') || input.take('}')) { return error::malformed; }
        do
        {
            ::fast_io::string key{};if(keys.size()==128u || !input.string(key,4096u) || !input.take(':')) { return error::malformed; }
            for(auto const& previous:keys) { if(view(previous)==view(key)) { return error::malformed; } }
            auto const field{view(key)};
            if(field=="version") { ::std::string_view number{};if(!input.number(number) || number!="3") { return error::unsupported; }version=true; }
            else if(field=="sources") { if(!input.strings(candidate.sources,total)) { return error::malformed; }sources=true; }
            else if(field=="names") { if(!input.strings(names,total)) { return error::malformed; } }
            else if(field=="sourceRoot") { if(!input.string(candidate.source_root,4096u)) { return error::malformed; } }
            else if(field=="mappings") { if(!input.string(mappings,16u*1024u*1024u)) { return error::malformed; }mapped=true; }
            else if(field=="sections") { return error::unsupported; }
            else if(!input.skip()) { return error::malformed; }
            keys.push_back(::std::move(key));
            if(input.take('}')) { break; }
            if(!input.take(',')) { return error::malformed; }
        }while(true);
        input.space();if(input.at!=text.size() || !version || !sources || !mapped) { return error::malformed; }
        for(auto const& path:candidate.sources)
        { if(path.empty()) { return error::malformed; }for(unsigned char c:view(path)) { if(c<32u || c==127u) { return error::malformed; } } }
        for(unsigned char c:view(candidate.source_root)) { if(c<32u || c==127u) { return error::malformed; } }
        auto const encoded{view(mappings)};::std::size_t at{},segments{};
        ::std::int64_t generated{},source{},line{},column{},name{};
        bool pending{};row previous{};
        auto const close{[&](::std::uint64_t end)
        {
            if(pending && previous.begin<end)
            {
                auto const first{::std::max(previous.begin,code_file_begin)};
                auto const last{::std::min(end,code_file_begin+code_size)};
                if(first<last) { previous.begin=first-code_file_begin;previous.end=last-code_file_begin;candidate.rows.push_back(previous); }
            }
        }};
        while(at<encoded.size())
        {
            if(++segments>1024u*1024u) { return error::limit_exceeded; }
            ::std::int64_t values[5]{};unsigned count{};
            do
            { if(count==5u || !detail::vlq(encoded,at,values[count++])) { return error::malformed; } }
            while(at<encoded.size() && encoded[at]!=',' && encoded[at]!=';');
            if(count!=1u && count!=4u && count!=5u) { return error::malformed; }
            if(values[0]<0 || !detail::add(generated,values[0]) || static_cast<::std::uint64_t>(generated)>file_size) { return error::malformed; }
            close(static_cast<::std::uint64_t>(generated));pending=count!=1u;
            if(pending)
            {
                if(!detail::add(source,values[1]) || static_cast<::std::uint64_t>(source)>=candidate.sources.size() ||
                   !detail::add(line,values[2]) || !detail::add(column,values[3]) ||
                   (count==5u && (!detail::add(name,values[4]) || static_cast<::std::uint64_t>(name)>=names.size()))) { return error::malformed; }
                previous={static_cast<::std::uint64_t>(generated),0u,static_cast<::std::uint64_t>(line)+1u,
                    static_cast<::std::uint64_t>(column)+1u,static_cast<::std::size_t>(source)};
            }
            if(at==encoded.size()) { break; }
            // WebAssembly has one generated line. Multiline/indexed maps need
            // a separate adapter rather than silently resetting byte offsets.
            if(encoded[at++]!=',' || at==encoded.size()) { return error::unsupported; }
        }
        close(file_size);output=::std::move(candidate);return error::none;
    }
}

// Formatting of copied Wasm memory only. No host address is dereferenced.
#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <fast_io_dsal/string_view.h>
namespace uwvm2::uwvm::debugger::console_memory
{
    using view=::fast_io::string_view;
    inline constexpr view types[]{"all","char","wc","utf8","utf16","utf32","i8","u8","i16","u16","i32","u32","i64","u64","f32","f64"};
    [[nodiscard]] inline bool valid_type(view type) noexcept
    { for(auto candidate:types) { if(candidate==type) { return true; } } return false; }
    [[nodiscard]] inline ::std::size_t row_bytes(::std::size_t columns) noexcept
    { return columns>=87u ? 16u : columns>=55u ? 8u : columns>=39u ? 4u : 1u; }
    [[nodiscard]] inline ::fast_io::string dump(::std::span<::std::byte const> bytes,::std::uint64_t base,::std::size_t columns=120u)
    {
        ::fast_io::string text{};auto const width{row_bytes(columns)};
        if(bytes.size()>UINT64_MAX-base) { return ::fast_io::concat_fast_io("invalid Wasm memory range\n"); }
        for(::std::size_t start{};start<bytes.size();start+=width)
        {
            text.append(::fast_io::concat_fast_io(::fast_io::mnp::hex0x<true>(base+start),": "));
            for(::std::size_t i{};i<width;++i)
            { if(i<bytes.size()-start) { text.append(::fast_io::concat_fast_io(::fast_io::mnp::hex<false,true>(::std::to_integer<unsigned char>(bytes[start+i]))," ")); }
              else { text.append("   "); } }
            text.append(" |");
            for(::std::size_t i{};i<width && i<bytes.size()-start;++i)
            { auto c{::std::to_integer<unsigned char>(bytes[start+i])};text.push_back(c>=32u && c<127u ? static_cast<char>(c) : '.'); }
            text.append("|\n");
        }
        return text;
    }
    [[nodiscard]] inline ::fast_io::string interpret(::std::span<::std::byte const> bytes,view type,bool compact=false)
    {
        if(!valid_type(type)) { return ::fast_io::concat_fast_io("unknown memory format\n"); }
        ::fast_io::string out{};
        auto integer{[&](::std::size_t width) { ::std::uint64_t n{};for(::std::size_t i{};i<width;++i) { n|=::std::uint64_t{::std::to_integer<unsigned char>(bytes[i])}<<(8u*i); }return n; }};
        for(auto name:types)
        {
            if(name=="all" || (type!="all" && name!=type)) { continue; }
            ::std::size_t width{name=="wc" || name=="utf32" || name=="i32" || name=="u32" || name=="f32" ? 4u :
                name=="utf16" || name=="i16" || name=="u16" ? 2u : name=="i64" || name=="u64" || name=="f64" ? 8u : 1u};
            if(compact && type=="all" && bytes.size()<width) { continue; }
            out.append(name);out.push_back('=');
            if(bytes.size()<width) { out.append("<needs ");out.append(::fast_io::concat_fast_io(width," bytes>\n"));continue; }
            if(name=="char" || name=="utf8")
            {
                out.push_back('"');bool valid{true};
                for(::std::size_t i{};i<bytes.size() && i<128u;)
                {
                    auto const c{::std::to_integer<unsigned char>(bytes[i])};
                    if(c<128u)
                    { if(c=='"' || c=='\\') { out.push_back('\\');out.push_back(static_cast<char>(c)); }
                      else if(c>=32u && c<127u) { out.push_back(static_cast<char>(c)); }
                      else { out.append(::fast_io::concat_fast_io("\\x",::fast_io::mnp::hex<false,true>(c))); }++i;continue; }
                    if(name=="char") { out.append(::fast_io::concat_fast_io("\\x",::fast_io::mnp::hex<false,true>(c)));++i;continue; }
                    auto const count{c>=0xc2u && c<=0xdfu ? 2u : c>=0xe0u && c<=0xefu ? 3u : c>=0xf0u && c<=0xf4u ? 4u : 0u};
                    ::std::uint32_t code{count ? static_cast<::std::uint32_t>(c & (0x7fu>>count)) : 0u};
                    bool correct{count!=0u && count<=bytes.size()-i};
                    for(unsigned j{1u};correct && j<count;++j)
                    { auto const b{::std::to_integer<unsigned char>(bytes[i+j])};correct=(b&0xc0u)==0x80u;code=(code<<6u)|(b&63u); }
                    correct=correct && (count!=3u || code>=0x800u) && (count!=4u || code>=0x10000u) && code<=0x10ffffu && !(code>=0xd800u && code<=0xdfffu);
                    if(!correct) { valid=false;out.append(::fast_io::concat_fast_io("\\x",::fast_io::mnp::hex<false,true>(c)));++i;continue; }
                    for(unsigned j{};j<count;++j) { out.push_back(static_cast<char>(::std::to_integer<unsigned char>(bytes[i+j]))); }i+=count;
                }
                out.push_back('"');if(!valid) { out.append(" (invalid UTF-8)"); }
            }
            else if(name=="wc" || name=="utf16" || name=="utf32")
            {
                auto unit_width{name=="utf16" ? 2u : 4u};bool valid{bytes.size()%unit_width==0u};out.push_back('"');
                for(::std::size_t i{};i+unit_width<=bytes.size() && i<128u;)
                {
                    ::std::uint32_t code{};for(unsigned j{};j<unit_width;++j) { code|=static_cast<::std::uint32_t>(::std::to_integer<unsigned char>(bytes[i+j]))<<(8u*j); }i+=unit_width;
                    if(unit_width==2u && code>=0xd800u && code<=0xdbffu && i+2u<=bytes.size())
                    { auto low{::std::uint32_t{::std::to_integer<unsigned char>(bytes[i])}|(::std::uint32_t{::std::to_integer<unsigned char>(bytes[i+1u])}<<8u)};
                      if(low>=0xdc00u && low<=0xdfffu) { code=0x10000u+((code-0xd800u)<<10u)+(low-0xdc00u);i+=2u; } }
                    if(code>0x10ffffu || (code>=0xd800u && code<=0xdfffu)) { valid=false;out.append(::fast_io::concat_fast_io("\\u{",::fast_io::mnp::hex(code),"}"));continue; }
                    if(code<32u || code==127u || (code>=128u&&code<=159u)) { out.append(::fast_io::concat_fast_io("\\u{",::fast_io::mnp::hex(code),"}")); }
                    else if(code<128u) { if(code=='"' || code=='\\') { out.push_back('\\'); }out.push_back(static_cast<char>(code)); }
                    else if(code<0x800u) { out.push_back(static_cast<char>(0xc0u|(code>>6u)));out.push_back(static_cast<char>(0x80u|(code&63u))); }
                    else if(code<0x10000u) { out.push_back(static_cast<char>(0xe0u|(code>>12u)));out.push_back(static_cast<char>(0x80u|((code>>6u)&63u)));out.push_back(static_cast<char>(0x80u|(code&63u))); }
                    else { out.push_back(static_cast<char>(0xf0u|(code>>18u)));out.push_back(static_cast<char>(0x80u|((code>>12u)&63u)));out.push_back(static_cast<char>(0x80u|((code>>6u)&63u)));out.push_back(static_cast<char>(0x80u|(code&63u))); }
                }
                out.push_back('"');if(!valid) { out.append(" (invalid Unicode/code-unit length)"); }else if(!compact) { out.append(" (little endian)"); }
            }
            else
            {
                auto const bits{integer(width)};
                if(name=="f32") { out.append(::fast_io::concat_fast_io(::std::bit_cast<float>(static_cast<::std::uint32_t>(bits)))); }
                else if(name=="f64") { out.append(::fast_io::concat_fast_io(::std::bit_cast<double>(bits))); }
                else if(name.front()=='i')
                { auto extended{bits};if(width<8u && (bits & (::std::uint64_t{1u}<<(width*8u-1u)))) { extended|=UINT64_MAX<<(width*8u); }
                  out.append(::fast_io::concat_fast_io(::std::bit_cast<::std::int64_t>(extended))); }
                else if(name=="wc" || name=="utf16" || name=="utf32")
                { out.append(::fast_io::concat_fast_io("U+",::fast_io::mnp::hex(bits)," (little endian)")); }
                else { out.append(::fast_io::concat_fast_io(bits)); }
            }
            out.push_back('\n');
        }
        return out;
    }
}

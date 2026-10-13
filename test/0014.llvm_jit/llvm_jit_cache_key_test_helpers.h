#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace cache_key_test
{
    struct field
    {
        bool valid{};
        ::std::size_t matches{};
        ::std::u8string_view value{};
    };
    // Independent decoder for the length-prefixed cache-key wire format.
    constexpr field lookup(::std::u8string_view bytes, ::std::u8string_view wanted) noexcept
    {
        field result{};
        ::std::size_t position{};
        auto read_length=[&](::std::size_t& length)
        {
            ::std::uint64_t value{};
            for(unsigned index{};index!=10u;++index)
            {
                if(position==bytes.size()) { return false; }
                auto const byte{static_cast<unsigned char>(bytes[position++])};
                if(index==9u && (byte&0xfeu)!=0u) { return false; }
                value|=static_cast<::std::uint64_t>(byte&0x7fu)<<(index*7u);
                if((byte&0x80u)==0u)
                {
                    if(index!=0u && byte==0u) { return false; }
                    if(value>bytes.size()-position) { return false; }
                    length=static_cast<::std::size_t>(value);return true;
                }
            }
            return false;
        };
        while(position!=bytes.size())
        {
            ::std::size_t length{};
            if(!read_length(length)) { return {}; }
            auto name{bytes.substr(position,length)};position+=length;
            if(!read_length(length)) { return {}; }
            auto value{bytes.substr(position,length)};position+=length;
            if(name==wanted)
            {
                if(++result.matches!=1uz) { return {}; }
                result.value=value;
            }
        }
        result.valid=true;return result;
    }
    constexpr bool equals(::std::u8string_view bytes,::std::u8string_view name,::std::u8string_view value) noexcept
    {
        auto const entry{lookup(bytes,name)};
        return entry.valid && entry.matches==1uz && entry.value==value;
    }
    static_assert(equals(u8"\x01" "a" "\x01" "b",u8"a",u8"b"));
    static_assert(!equals(u8"\x01" "a" "\x01" "c",u8"a",u8"b"));
    static_assert(!lookup(u8"\x01" "a" "\x02" "b",u8"a").valid);
    static_assert(!lookup(u8"\x01" "a" "\x01" "b" "\x01" "a" "\x01" "b",u8"a").valid);
    static_assert(!lookup(u8"\x81\x00" "a" "\x01" "b",u8"a").valid);
}

// Owned, display-only Wasm symbols. Malformed optional name maps are ignored;
// they never change source ownership, instruction boundaries or execution.
#pragma once
#ifndef UWVM_MODULE
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
#include <llvm/Demangle/Demangle.h>
#endif
#endif
namespace uwvm2::uwvm::debugger::wasm_symbols
{
    using names=::std::map<::std::uint64_t,::std::string>;
    struct table { names functions{},globals{}; };
    [[nodiscard]] inline ::std::string display(::std::string_view name)
    {
        if(name.empty() || name.size()>4096u) { return {}; }
        // Metadata can contain terminal controls. Sanitize before it enters a
        // styled pane, and never interpret a metadata ESC as a colour command.
        ::std::string value{name};
        for(auto& c:value) { if(static_cast<unsigned char>(c)<32u || c==127) { c='?'; } }
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
        value=::llvm::demangle(value);
#endif
        if(value.size()>4096u) { value.resize(4096u);value.append("…"); }
        return value;
    }
    struct reader
    {
        ::std::span<::std::byte const> data; ::std::size_t at{};
        [[nodiscard]] bool number(::std::uint32_t& value)
        {
            value=0u;
            for(unsigned shift{};shift<35u;shift+=7u)
            {
                if(at==data.size()) { return false; }
                auto byte{::std::to_integer<unsigned>(data[at++])};
                if(shift==28u && (byte&0xf0u)) { return false; }
                value|=(byte&127u)<<shift;if(!(byte&128u)) { return true; }
            }
            return false;
        }
        [[nodiscard]] bool text(::std::string_view& out)
        {
            ::std::uint32_t size{};if(!number(size) || size>data.size()-at || size>4096u) { return false; }
            out={reinterpret_cast<char const*>(data.data()+at),size};at+=size;return true;
        }
    };
    [[nodiscard]] inline bool name_map(::std::span<::std::byte const> payload,names& result)
    {
        reader input{payload};::std::uint32_t count{};
        if(!input.number(count) || count>65536u) { return false; }
        names parsed{};
        for(::std::uint32_t i{};i<count;++i)
        {
            ::std::uint32_t index{};::std::string_view name{};
            if(!input.number(index) || !input.text(name) || !parsed.emplace(index,display(name)).second) { return false; }
        }
        if(input.at!=input.data.size()) { return false; }
        result=::std::move(parsed);return true;
    }
    inline void load_name_section(::std::span<::std::byte const> payload,table& result)
    {
        reader input{payload};table parsed{};bool functions{},globals{};
        while(input.at<input.data.size())
        {
            auto id{::std::to_integer<unsigned>(input.data[input.at++])};::std::uint32_t size{};
            if(!input.number(size) || size>input.data.size()-input.at) { return; }
            auto part{input.data.subspan(input.at,size)};input.at+=size;
            if(id==1u) { if(functions || !name_map(part,parsed.functions)) { return; }functions=true; }
            else if(id==7u) { if(globals || !name_map(part,parsed.globals)) { return; }globals=true; }
        }
        for(auto& [index,name]:parsed.functions) { if(!name.empty()) { result.functions.insert_or_assign(index,::std::move(name)); } }
        for(auto& [index,name]:parsed.globals) { if(!name.empty()) { result.globals.insert_or_assign(index,::std::move(name)); } }
    }
    [[nodiscard]] inline ::std::string_view lookup(table const& symbols,bool global,::std::uint64_t index)
    {
        auto const& values{global?symbols.globals:symbols.functions};auto found{values.find(index)};
        return found==values.end()? ::std::string_view{} : ::std::string_view{found->second};
    }
    [[nodiscard]] inline ::std::string_view reference(table const& symbols,::std::span<::std::byte const> instruction)
    {
        if(instruction.empty()) { return {}; }
        auto op{::std::to_integer<unsigned>(instruction.front())};
        if(op!=0x10u && op!=0x12u && op!=0xd2u && op!=0x23u && op!=0x24u) { return {}; }
        reader input{instruction.subspan(1u)};::std::uint32_t index{};
        if(!input.number(index) || input.at!=input.data.size()) { return {}; }
        return lookup(symbols,op==0x23u || op==0x24u,index);
    }
}

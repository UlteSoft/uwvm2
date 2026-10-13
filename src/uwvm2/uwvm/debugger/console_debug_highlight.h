// Lexical colouring of detached Wasm/ASM display text. No evaluation or reads.
#pragma once
#ifndef UWVM_MODULE
#include "console_source_highlight.h"
#endif
namespace uwvm2::uwvm::debugger::console_debug_highlight
{
    using namespace console_source_highlight;
    [[nodiscard]] constexpr bool register_name(view word)
    {
        if(in(word,"sp fp lr pc nzcv xzr wzr ra zero gp tp hi lo eax ebx ecx edx esi edi esp ebp eip rax rbx rcx rdx rsi rdi rsp rbp rip rflags eflags")) { return true; }
        for(view prefix:{view{"xmm"},view{"ymm"},view{"zmm"},view{"st"},view{"x"},view{"w"},view{"r"},view{"v"},view{"q"},view{"d"},view{"s"},view{"b"},view{"h"},view{"a"},view{"t"},view{"f"}})
        {
            if(!word.starts_with(prefix) || word.size()==prefix.size() || !digit(word[prefix.size()])) { continue; }
            auto i{prefix.size()};while(i<word.size() && digit(word[i])) { ++i; }
            return i==word.size() || word.subview(i)=="d" || word.subview(i)=="w" || word.subview(i)=="b";
        }
        return false;
    }
    template<typename Emit> void scan(view text,bool assembly,Emit emit)
    {
        // Only rows with a real address/offset prefix are instruction rows.
        if(text.size()<4u || (!text.starts_with("=> ") && !text.starts_with("N> ") && !text.starts_with("   ")))
        { emit(0u,text.size(),style::comment);return; }
        auto offset{::std::size_t{3u}};while(offset<text.size() && text[offset]==' ') { ++offset; }
        if(offset==text.size() || !digit(text[offset])) { emit(0u,text.size(),style::comment);return; }
        emit(0u,3u,text.starts_with("=>")?style::string:text.starts_with("N>")?style::number:style::comment);
        auto at{offset};while(at<text.size() && text[at]!=' ') { ++at; }emit(3u,at,style::comment);
        auto start{at};while(at<text.size() && text[at]==' ') { ++at; }emit(start,at,style::plain);
        start=at;while(at<text.size() && text[at]!=' ' && text[at]!='\t') { ++at; }emit(start,at,style::keyword);
        while(at<text.size())
        {
            start=at;auto c{text[at]};style kind{style::plain};
            if(c==';') { emit(at,text.size(),style::comment);break; }
            if(c=='<') { emit(at,text.size(),style::string);break; }
            if(letter(c) || digit(c) || c=='%' || c=='$' || c=='-' || c=='#')
            {
                ++at;while(at<text.size() && (letter(text[at]) || digit(text[at]) || text[at]=='.')) { ++at; }
                auto word{text.subview(start,at-start)};
                auto reg{word.starts_with("%") || word.starts_with("$")?word.subview(1u):word};
                if(assembly && register_name(reg)) { kind=style::type; }
                else if(digit(c) || c=='-' || c=='#' || c=='$') { kind=style::number; }
            }
            else { ++at; }
            emit(start,at,kind);
        }
    }
}

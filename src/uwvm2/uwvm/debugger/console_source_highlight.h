/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <fast_io_dsal/array.h>
# include <fast_io_dsal/string_view.h>
# include <fast_io_dsal/vector.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::console_source_highlight
{
    using view = ::fast_io::string_view;
    enum class language { plain, cpp, rust, go, zig, script, wat };
    enum class style { plain, keyword, type, number, string, comment };
    // A bounded, forward-only lexical scanner. It never parses, evaluates or
    // resolves identifiers. Raw delimiters and block-comment state survive
    // the source window and scrolling; unknown file extensions stay plain.
    struct state
    {
        ::std::size_t block_depth{};
        char quote{};
        ::fast_io::array<char,16u> delimiter{};
        ::std::size_t delimiter_size{};
        bool raw{};
    };
    struct line_style { ::std::size_t row{}, prefix{}; language lang{}; state before{}; };
    using line_styles = ::fast_io::vector<line_style>;
    [[nodiscard]] constexpr language detect(view path) noexcept
    {
        auto const at{path.rfind(".")}; if(at == ::fast_io::containers::npos) { return language::plain; }
        auto const suffix{path.subview(at)};
        if(suffix == ".c" || suffix == ".h" || suffix == ".cc" || suffix == ".cpp" || suffix == ".cxx" ||
           suffix == ".hpp" || suffix == ".hh" || suffix == ".hxx" || suffix == ".cppm") { return language::cpp; }
        if(suffix == ".rs") { return language::rust; }
        if(suffix == ".go") { return language::go; }
        if(suffix == ".zig") { return language::zig; }
        if(suffix == ".js" || suffix == ".ts" || suffix == ".mjs" || suffix == ".tsx" || suffix == ".jsx") { return language::script; }
        if(suffix == ".wat" || suffix == ".wast") { return language::wat; }
        return language::plain;
    }
    [[nodiscard]] constexpr bool letter(char c) noexcept
    { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || static_cast<unsigned char>(c) >= 128u; }
    [[nodiscard]] constexpr bool digit(char c) noexcept { return c >= '0' && c <= '9'; }
    [[nodiscard]] constexpr bool in(view word, view words) noexcept
    {
        while(!words.empty())
        { auto const at{words.find(" ")}; auto const n{at == ::fast_io::containers::npos ? words.size() : at};
          if(words.subview(0u,n) == word) { return true; } words=words.subview(at == ::fast_io::containers::npos ? n : n+1u); }
        return false;
    }
    [[nodiscard]] constexpr style identifier(view word, language lang) noexcept
    {
        if(in(word,"bool char double float int long short signed unsigned void wchar_t size_t i8 i16 i32 i64 i128 u8 u16 u32 u64 u128 f32 f64 usize isize string byte rune any never")) { return style::type; }
        if(in(word,"if else for while do break continue return switch case default const static struct enum union true false null nullptr")) { return style::keyword; }
        if(lang == language::cpp && in(word,"auto class template typename namespace using public private protected virtual override final constexpr consteval constinit inline noexcept new delete this try catch throw co_await co_return co_yield concept requires import export module")) { return style::keyword; }
        if(lang == language::rust && in(word,"fn let mut pub impl trait use mod crate self Self super match loop move ref where async await dyn unsafe extern as in type macro_rules")) { return style::keyword; }
        if(lang == language::go && in(word,"func package import var type map chan go defer select range interface fallthrough nil")) { return style::keyword; }
        if(lang == language::zig && in(word,"fn var pub comptime defer errdefer orelse catch try error test opaque extern export packed usingnamespace undefined null")) { return style::keyword; }
        if(lang == language::script && in(word,"function let var class extends implements interface type async await new delete this try catch throw import export from typeof instanceof yield of in null undefined")) { return style::keyword; }
        if(lang == language::wat && (word.find(".") != ::fast_io::containers::npos ||
            in(word,"module func param result local global memory table import export start elem data block loop end br br_if br_table call call_indirect return unreachable drop select ref null externref funcref"))) { return style::keyword; }
        return style::plain;
    }
    template<typename Emit> void scan(view text, language lang, state& current, Emit emit)
    {
        if(lang == language::plain) { emit(0u,text.size(),style::plain); return; }
        ::std::size_t i{};
        while(i < text.size())
        {
            auto const begin{i}; style color{style::plain};
            auto starts=[&](view part) { return text.subview(i).starts_with(part); };
            auto const block_open{lang == language::wat ? view{"(;"} : view{"/*"}};
            auto const block_close{lang == language::wat ? view{";)"} : view{"*/"}};
            if(current.block_depth != 0u || (current.quote == 0 && starts(block_open)))
            {
                color=style::comment;
                if(current.block_depth == 0u) { current.block_depth=1u; i+=2u; }
                while(i < text.size())
                {
                    if(starts(block_close)) { i+=2u; if(--current.block_depth == 0u) { break; } }
                    else if((lang == language::rust || lang == language::wat) && starts(block_open)) { ++current.block_depth; i+=2u; }
                    else { ++i; }
                }
            }
            else if(current.quote == 0 && (starts(lang == language::wat ? view{";;"} : view{"//"})))
            { color=style::comment; i=text.size(); }
            else if(current.quote == 0 && lang == language::zig && starts("\\\\"))
            { color=style::string; i=text.size(); }
            else
            {
                ::std::size_t raw_prefix{};
                if(lang == language::cpp)
                { if(starts("u8R\"")) { raw_prefix=4u; }
                  else if(starts("uR\"") || starts("UR\"") || starts("LR\"")) { raw_prefix=3u; }
                  else if(starts("R\"")) { raw_prefix=2u; } }
                if(current.quote == 0 && raw_prefix != 0u)
                {
                    auto p{i+raw_prefix}; while(p < text.size() && p-i-raw_prefix <= 16u && text[p] != '(' && text[p] > ' ' && text[p] != '\\' && text[p] != ')') { ++p; }
                    if(p < text.size() && text[p] == '(' && p-i-raw_prefix <= 16u)
                    { current.raw=true; current.quote='"'; current.delimiter_size=p-i-raw_prefix;
                      for(::std::size_t n{}; n < current.delimiter_size; ++n) { current.delimiter[n]=text[i+raw_prefix+n]; } i=p+1u; }
                }
                if(current.quote == 0 && lang == language::rust && (starts("r\"") || starts("r#") || starts("br\"") || starts("br#")))
                {
                    auto p{i+(text[i] == 'b' ? 2u : 1u)}; auto const first{p}; while(p < text.size() && text[p] == '#') { ++p; }
                    if(p < text.size() && text[p] == '"' && p-first <= 255u)
                    { current.raw=true; current.quote='r'; current.delimiter_size=p-first;
                      i=p+1u; }
                }
                if(current.quote == 0 && i == begin && (text[i] == '"' || text[i] == '\'' ||
                    ((lang == language::go || lang == language::script) && text[i] == '`')))
                {
                    // Rust lifetimes are identifiers, not unterminated strings.
                    auto p{i+1u}; while(p < text.size() && letter(text[p])) { ++p; }
                    if(!(lang == language::rust && text[i] == '\'' && p > i+1u && (p == text.size() || text[p] != '\'')))
                    { current.quote=text[i++]; current.raw=current.quote == '`' && lang == language::go; current.delimiter_size=0u; }
                }
                if(current.quote != 0)
                {
                    color=style::string;
                    while(i < text.size())
                    {
                        if(current.raw && current.quote == '"')
                        { if(text[i] == ')' && text.size()-i >= current.delimiter_size+2u &&
                              text.subview(i+1u,current.delimiter_size) == view{current.delimiter.data(),current.delimiter_size} && text[i+1u+current.delimiter_size] == '"')
                          { i+=current.delimiter_size+2u; current={}; break; } ++i; }
                        else if(current.raw && current.quote == 'r')
                        { if(text[i] == '"' && text.size()-i >= current.delimiter_size+1u &&
                              [&] { for(::std::size_t n{}; n < current.delimiter_size; ++n) { if(text[i+1u+n] != '#') { return false; } } return true; }())
                          { i+=current.delimiter_size+1u; current={}; break; } ++i; }
                        else if(text[i] == current.quote) { ++i; current={}; break; }
                        else if(!current.raw && text[i] == '\\' && i+1u < text.size()) { i+=2u; }
                        else { ++i; }
                    }
                }
                else if(i != begin) { color=style::string; }
                else if(letter(text[i]) || (lang == language::wat && text[i] == '$'))
                { ++i; while(i < text.size() && (letter(text[i]) || digit(text[i]) || (lang == language::wat && (text[i] == '.' || text[i] == '-')))) { ++i; }
                  color=identifier(text.subview(begin,i-begin),lang); }
                else if(digit(text[i]))
                { ++i; while(i < text.size() && (letter(text[i]) || digit(text[i]) || text[i] == '.' ||
                    ((text[i] == '+' || text[i] == '-') && (text[i-1u] == 'e' || text[i-1u] == 'E' || text[i-1u] == 'p' || text[i-1u] == 'P')))) { ++i; } color=style::number; }
                else if(lang == language::cpp && text[i] == '#')
                { ++i; while(i < text.size() && (letter(text[i]) || text[i] == ' ')) { ++i; } color=style::keyword; }
                else { ++i; }
            }
            emit(begin,i,color);
        }
        if(!current.raw && (lang == language::cpp || lang == language::script) &&
           current.quote != '`' && (text.empty() || text.back() != '\\')) { current.quote=0; }
    }
}

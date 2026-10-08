/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_expression.h"
# include <bit>
# include <array>
# include <cmath>
# include <limits>
# include <memory>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_scalar_expression
{
    namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
    // Owned syntax and values only. The resolver must run inside one genuine
    // stopped guest transaction. Neither literals nor arithmetic mint pointers.
    enum class operation { literal, reference, size, type_size, cast, zig_coerce, positive, negative, invert, logical_not,
        add, subtract, multiply, divide, remainder, left, right, less, less_equal,
        greater, greater_equal, equal, unequal, bit_and, bit_clear, bit_xor, bit_or, logical_and, logical_or, conditional, expression_size };
    using narrow_builtin = dwarf::cxx_narrow_builtin;
    using wide_builtin = dwarf::c_wide_builtin;
    struct node
    {
        operation op{};
        ::std::size_t lhs{}, rhs{}, third{};
        ::std::uint64_t literal{}; bool hex_literal{};
        dwarf::source_expression reference{};
        unsigned literal_width{}; bool literal_unsigned{}, floating_literal{}, boolean_cast{}, guest_long_literal{}, boolean_literal{}, character_literal{};
        narrow_builtin builtin_identity{};
        wide_builtin wide_identity{};
        unsigned utf_character_width{}; // owned C++ char8/16/32 syntax, never a language grant
        bool rust_character{}, rust_cast_as{};
        bool rust_numeric_literal{}, rust_numeric_suffix{};
        ::fast_io::string rust_numeric_text{}; // owned normalized float digits, bounded by original 4096 bytes // copied Rust token grammar; actual CU still required // copied Rust scalar syntax, never CU authority
        bool logical_keyword{}; // owned Zig and/or spelling, never language authority
    };
    // Explicit producer/syntax type identity. The default preserves old
    // callbacks while refusing ambiguous unclassified eight-bit carriers.
    enum class value_category { unspecified, integer, boolean };
    enum class language_semantics { shared_numeric, c, cpp, c23, rust, go, zig };
    struct program
    {
        ::std::vector<node> nodes{}; ::std::size_t root{};
        ::fast_io::string original{}; // <=4096 owned syntax bytes, never a frame/location grant.
        language_semantics syntax_language{language_semantics::shared_numeric};
    };
    // Only an actual selected-frame CU supplies these metadata bits. This
    // classifier is not a frame, memory, pointer or language-runtime grant.
    inline constexpr language_semantics language_from_dwarf(::std::uint64_t language, bool tinygo, bool zig = false) noexcept
    {
        if(tinygo && zig) { return language_semantics::shared_numeric; }
        if(tinygo) { return language_semantics::go; }
        if(zig) { return language == 0x0cu ? language_semantics::zig : language_semantics::shared_numeric; }
        switch(language)
        {
            case 0x01u: case 0x02u: case 0x0cu: case 0x10u: case 0x1du: case 0x2cu:
                return language_semantics::c;
            case 0x3eu: return language_semantics::c23;
            case 0x1cu: return language_semantics::rust;
            case 0x16u: return language_semantics::go;
            case 0x27u: return language_semantics::zig;
            case 0x04u: case 0x11u: case 0x19u: case 0x1au: case 0x21u: case 0x2au: case 0x2bu: case 0x3au:
                return language_semantics::cpp;
            default: return language_semantics::shared_numeric;
        }
    }
    struct integer
    {
        ::std::uint64_t bits{}; unsigned width{32u}; bool unsigned_value{}; bool floating{};
        value_category category{};
        narrow_builtin builtin_identity{};
        dwarf::die_key declaration_identity{};
        bool declaration_identity_known{}; // type DATA, never a location/read token
        wide_builtin wide_identity{};
        unsigned utf_character_width{}; // distinct copied UTF primitive identity
        bool rust_character{}; // Unicode scalar DATA, distinct from u32
        bool rust_pointer_sized{}; // Rust isize/usize type DATA, distinct from equal-width i/u32 or i/u64
    };
    inline constexpr integer from_dwarf_numeric(dwarf::numeric_kind kind, ::std::uint64_t bits, unsigned width) noexcept
    {
        if(width != 8u && width != 16u && width != 32u && width != 64u) { return {0u,0u,false}; }
        switch(kind)
        {
            case dwarf::numeric_kind::signed_integer: return {bits,width,false,false,value_category::integer};
            case dwarf::numeric_kind::unsigned_integer: return {bits,width,true,false,value_category::integer};
            case dwarf::numeric_kind::boolean: return {bits,width,true,false,value_category::boolean};
            case dwarf::numeric_kind::f32_bits: return width == 32u ? integer{bits,width,false,true} : integer{0u,0u,false};
            case dwarf::numeric_kind::f64_bits: return width == 64u ? integer{bits,width,false,true} : integer{0u,0u,false};
            default: return {0u,0u,false}; // unavailable metadata never supplies numeric type evidence.
        }
    }
    enum class error { none, malformed, unsupported, limit_exceeded, unavailable, arithmetic, allocation_failure };
    namespace details
    {
        inline constexpr bool native_integer_language(language_semantics language) noexcept
        { return language == language_semantics::c || language == language_semantics::c23 || language == language_semantics::cpp; }
        inline constexpr bool native_boolean_language(language_semantics language) noexcept
        { return language == language_semantics::rust || language == language_semantics::go || language == language_semantics::zig; }
        inline constexpr bool boolean_value(integer const& value) noexcept
        { return !value.floating && value.category == value_category::boolean; }
        inline constexpr bool native_syntax(node const& n, language_semantics language) noexcept
        {
            if(n.utf_character_width && language != language_semantics::cpp) { return false; }
            if(n.rust_numeric_literal && language != language_semantics::rust) { return false; }
            if(language == language_semantics::rust && (n.op == operation::positive || n.op == operation::invert)) { return false; }
            if(n.rust_character && (language != language_semantics::rust || (n.op == operation::cast && !n.rust_cast_as))) { return false; }
            if(n.op == operation::logical_and || n.op == operation::logical_or)
            { return n.logical_keyword ? language == language_semantics::zig : language != language_semantics::zig; }
            return !native_boolean_language(language) || n.op != operation::conditional;
        }
        struct numeric_type { unsigned width{}; bool unsigned_value{}, floating{}, boolean{}; narrow_builtin identity{}; wide_builtin wide_identity{}; unsigned utf_character_width{}; bool rust_character{}; };
        inline wide_builtin standard_wide_identity(::std::string_view words) noexcept
        {
            unsigned signs{},ints{},longs{}; bool uns{};
            if(words.empty()) { return wide_builtin::unknown; }
            while(!words.empty())
            {
                auto const end{words.find(' ')};auto const word{words.substr(0u,end)};
                if(word == "signed") { ++signs; }
                else if(word == "unsigned") { ++signs;uns=true; }
                else if(word == "int") { ++ints; }
                else if(word == "long") { ++longs; }
                else { return wide_builtin::unknown; }
                if(end == words.npos) { break; }words.remove_prefix(end+1u);
            }
            if(signs>1u || ints>1u || longs>2u) { return wide_builtin::unknown; }
            return longs == 2u ? (uns ? wide_builtin::unsigned_long_long : wide_builtin::signed_long_long) :
                longs ? (uns ? wide_builtin::unsigned_long : wide_builtin::signed_long) :
                uns ? wide_builtin::unsigned_int : wide_builtin::signed_int;
        }
        inline narrow_builtin standard_narrow_identity(::std::string_view words) noexcept
        {
            unsigned signs{},chars{},shorts{},ints{}; bool uns{};
            while(!words.empty())
            {
                auto const end{words.find(' ')};auto const word{words.substr(0u,end)};
                if(word == "signed") { ++signs; }
                else if(word == "unsigned") { ++signs;uns=true; }
                else if(word == "char") { ++chars; }
                else if(word == "short") { ++shorts; }
                else if(word == "int") { ++ints; }
                else { return narrow_builtin::unknown; }
                if(end == words.npos) { break; }words.remove_prefix(end+1u);
            }
            if(signs>1u || chars>1u || shorts>1u || ints>1u || (chars && (shorts || ints))) { return narrow_builtin::unknown; }
            if(chars) { return uns ? narrow_builtin::unsigned_char : signs ? narrow_builtin::signed_char : narrow_builtin::plain_char; }
            if(shorts) { return uns ? narrow_builtin::unsigned_short : narrow_builtin::signed_short; }
            return narrow_builtin::unknown;
        }
        inline bool builtin_type(::std::string_view text, numeric_type& type) noexcept
        {
            while(!text.empty() && ::fast_io::char_category::is_c_space(text.front())) { text.remove_prefix(1u); }
            while(!text.empty() && ::fast_io::char_category::is_c_space(text.back())) { text.remove_suffix(1u); }
            // Type words remain separate tokens; repeated whitespace is not
            // an identifier join. No view of this owned bounded buffer escapes.
            char normalized[32u]{}; ::std::size_t count{}; bool separator{};
            for(auto const c : text)
            {
                if(::fast_io::char_category::is_c_space(c)) { separator = count != 0u; continue; }
                if(separator)
                { if(count == sizeof(normalized)) { return false; } normalized[count++] = ' '; separator = false; }
                if(count == sizeof(normalized)) { return false; }
                normalized[count++] = c;
            }
            text = {normalized,count};
            if(text == "char8_t" || text == "char16_t" || text == "char32_t")
            { type = {text == "char8_t" ? 8u : text == "char16_t" ? 16u : 32u,true,false,false}; type.utf_character_width = type.width; }
            else if(text == "float" || text == "f32" || text == "float32") { type = {32u,false,true,false}; }
            else if(text == "double" || text == "f64" || text == "float64") { type = {64u,false,true,false}; }
            else if(text == "bool" || text == "_Bool") { type = {8u,true,false,true}; }
            else if(text == "char" || text == "signed char" || text == "i8" || text == "int8") { type = {8u,false,false,false}; }
            else if(text == "unsigned char" || text == "u8" || text == "uint8" || text == "byte") { type = {8u,true,false,false}; }
            else if(text == "short" || text == "short int" || text == "i16" || text == "int16") { type = {16u,false,false,false}; }
            else if(text == "unsigned short" || text == "u16" || text == "uint16") { type = {16u,true,false,false}; }
            else if(text == "int" || text == "signed int" || text == "i32" || text == "int32" || text == "rune") { type = {32u,false,false,false}; }
            else if(text == "unsigned" || text == "unsigned int" || text == "u32" || text == "uint32") { type = {32u,true,false,false}; }
            else if(text == "long long" || text == "long long int" || text == "i64" || text == "int64") { type = {64u,false,false,false}; }
            else if(text == "unsigned long long" || text == "u64" || text == "uint64") { type = {64u,true,false,false}; }
            else if(text == "long" || text == "long int" || text == "isize" || text == "intptr_t") { type = {0u,false,false,false}; }
            else if(text == "unsigned long" || text == "usize" || text == "uintptr_t" || text == "size_t" || text == "uint") { type = {0u,true,false,false}; }
            else
            {
                // Standard integer specifiers are an unordered, bounded set.
                // Only long may occur twice; aliases never participate in it.
                unsigned signs{}, chars{}, shorts{}, ints{}, longs{}; bool uns{};
                auto words{text};
                if(words.empty()) { return false; }
                while(!words.empty())
                {
                    auto const end{words.find(' ')}; auto const word{words.substr(0u,end)};
                    if(word == "signed") { ++signs; }
                    else if(word == "unsigned") { ++signs; uns = true; }
                    else if(word == "char") { ++chars; }
                    else if(word == "short") { ++shorts; }
                    else if(word == "int") { ++ints; }
                    else if(word == "long") { ++longs; }
                    else { return false; }
                    if(end == words.npos) { break; }
                    words.remove_prefix(end+1u);
                }
                if(signs > 1u || chars > 1u || shorts > 1u || ints > 1u || longs > 2u ||
                    (chars && (shorts || ints || longs)) || (shorts && longs)) { return false; }
                type = {chars ? 8u : shorts ? 16u : longs == 2u ? 64u : longs ? 0u : 32u,uns,false,false};
            }
            type.identity=standard_narrow_identity(text);
            type.wide_identity=text == "size_t" ? wide_builtin::unsigned_long : standard_wide_identity(text);
            return true;
        }
        inline bool zig_builtin_type(::std::string_view text, numeric_type& type) noexcept
        {
            while(!text.empty() && ::fast_io::char_category::is_c_space(text.front())) { text.remove_prefix(1u); }
            while(!text.empty() && ::fast_io::char_category::is_c_space(text.back())) { text.remove_suffix(1u); }
            for(auto name : {::std::string_view{"i8"},::std::string_view{"u8"},::std::string_view{"i16"},::std::string_view{"u16"},::std::string_view{"i32"},::std::string_view{"u32"},::std::string_view{"i64"},::std::string_view{"u64"},::std::string_view{"isize"},::std::string_view{"usize"},::std::string_view{"f32"},::std::string_view{"f64"}})
            { if(text == name) { return builtin_type(text,type); } }
            return false;
        }
        inline constexpr bool unicode_scalar(::std::uint64_t c) noexcept
        { return c <= 0x10ffffu && (c < 0xd800u || c > 0xdfffu); }
        inline error rust_character_literal(::std::string_view text, ::std::size_t& cursor, ::std::uint64_t& value) noexcept
        {
            auto next{cursor};
            if(next >= text.size() || text[next++] != '\'') { return error::malformed; }
            if(next >= text.size()) { return error::malformed; }
            ::std::uint64_t c{static_cast<unsigned char>(text[next++])};
            if(c == '\\')
            {
                if(next >= text.size()) { return error::malformed; }
                auto const escaped{text[next++]};
                if(escaped == 'u')
                {
                    if(next >= text.size() || text[next++] != '{') { return error::malformed; }
                    ::fast_io::array<char,6u> digits{};unsigned count{};
                    while(next < text.size() && text[next] != '}')
                    {
                        auto const digit{text[next++]};
                        if(digit == '_' && count) { continue; }
                        if(count == digits.size() || (!::fast_io::char_category::is_c_digit(digit) &&
                            !(digit >= 'a' && digit <= 'f') && !(digit >= 'A' && digit <= 'F'))) { return error::unsupported; }
                        digits[count++] = digit;
                    }
                    if(!count || next >= text.size() || text[next++] != '}') { return error::malformed; }
                    auto const parsed{::fast_io::parse_by_scan(digits.data(),digits.data()+count,::fast_io::mnp::hex_get<true,true>(c))};
                    if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != digits.data()+count) { return error::unsupported; }
                }
                else if(escaped == 'x')
                {
                    if(text.size()-next < 2u || text[next] < '0' || text[next] > '7') { return error::unsupported; }
                    auto const second{text[next+1u]};
                    if(!::fast_io::char_category::is_c_digit(second) && !(second >= 'a' && second <= 'f') &&
                        !(second >= 'A' && second <= 'F')) { return error::unsupported; }
                    auto const parsed{::fast_io::parse_by_scan(text.data()+next,text.data()+next+2u,::fast_io::mnp::hex_get<true,true>(c))};
                    if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != text.data()+next+2u) { return error::unsupported; }
                    next += 2u;
                }
                else
                {
                    switch(escaped)
                    { case '0': c=0u;break;case 'n':c='\n';break;case 'r':c='\r';break;case 't':c='\t';break;
                      case '\\':c='\\';break;case '\'':c='\'';break;case '"':c='"';break;default:return error::unsupported; }
                }
            }
            else if(c >= 128u || c == '\'' || c == '\n' || c == '\r' || c == '\t') { return error::unsupported; }
            if(!unicode_scalar(c) || next >= text.size() || text[next] != '\'') { return error::unsupported; }
            cursor=next+1u;value=c;return error::none;
        }
        inline unsigned utf_character_prefix(::std::string_view text, ::std::size_t cursor) noexcept
        {
            auto const tail{text.substr(cursor)};
            return tail.starts_with("u8'") ? 8u : tail.starts_with("u'") ? 16u : tail.starts_with("U'") ? 32u : 0u;
        }
        inline error character_literal(::std::string_view text, ::std::size_t& cursor, ::std::uint64_t& value,
            language_semantics language = language_semantics::shared_numeric, unsigned* utf_width = nullptr) noexcept
        {
            if(language == language_semantics::rust) { return rust_character_literal(text,cursor,value); }
            auto next{cursor};
            auto const width{language == language_semantics::cpp ? utf_character_prefix(text,cursor) : 0u};
            if(width) { next += width == 8u ? 2u : 1u; }
            bool universal{};
            if(next >= text.size() || text[next++] != '\'') { return error::malformed; }
            if(next >= text.size()) { return error::malformed; }
            ::std::uint64_t c{static_cast<unsigned char>(text[next++])};
            if(c == '\\')
            {
                if(next >= text.size()) { return error::malformed; }
                auto const escaped{text[next++]};
                if(width && (escaped == 'u' || escaped == 'U'))
                {
                    universal = true;
                    auto const count{escaped == 'u' ? 4u : 8u};
                    if(text.size()-next < count) { return error::malformed; }
                    auto const first{text.data()+next};auto const last{first+count};
                    for(auto p{first};p!=last;++p)
                    { if(!::fast_io::char_category::is_c_digit(*p) && !(*p >= 'a' && *p <= 'f') && !(*p >= 'A' && *p <= 'F')) { return error::malformed; } }
                    auto const parsed{::fast_io::parse_by_scan(first,last,::fast_io::mnp::hex_get<true,true>(c))};
                    if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last) { return error::unsupported; }
                    next += count;
                }
                else if(escaped == 'x' || (escaped >= '0' && escaped <= '7'))
                {
                    auto const begin{escaped == 'x' ? next : next-1u};
                    next = begin;
                    while(next < text.size())
                    {
                        auto const digit{text[next]};
                        bool const accepted{escaped == 'x' ? ::fast_io::char_category::is_c_digit(digit) ||
                            (digit >= 'a' && digit <= 'f') || (digit >= 'A' && digit <= 'F') : digit >= '0' && digit <= '7'};
                        if(!accepted || (escaped != 'x' && next-begin == 3u)) { break; }
                        ++next;
                    }
                    if(next == begin) { return error::malformed; }
                    auto const first{text.data()+begin}; auto const last{text.data()+next};
                    auto const parsed{escaped == 'x' ? ::fast_io::parse_by_scan(first,last,::fast_io::mnp::hex_get<true,true>(c)) :
                        ::fast_io::parse_by_scan(first,last,::fast_io::mnp::oct_get<true,true>(c))};
                    if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last) { return error::unsupported; }
                }
                else
                {
                    switch(escaped)
                    { case 'a': c = '\a'; break; case 'b': c = '\b'; break; case 'f': c = '\f'; break;
                      case 'n': c = '\n'; break; case 'r': c = '\r'; break; case 't': c = '\t'; break; case 'v': c = '\v'; break;
                      case '\\': c = '\\'; break; case '\'': c = '\''; break; case '"': c = '"'; break; case '?': c = '?'; break;
                      default: return error::unsupported; }
                }
            }
            else if(c >= 128u || c == '\'' || c == '\n' || c == '\r') { return error::unsupported; }
            // Numeric escapes describe a single code unit, including lone
            // UTF-8/UTF-16 units. Universal names describe Unicode scalars and
            // must fit ONE unit of the specified literal encoding.
            auto const limit{width == 32u ? 0xffffffffull : width == 16u ? 0xffffull : width == 8u ? 0xffull : 0x7full};
            if(c > limit || (universal && (c > 0x10ffffu || (c >= 0xd800u && c <= 0xdfffu) || (width == 8u && c >= 128u))) ||
               next >= text.size() || text[next] != '\'') { return error::unsupported; }
            cursor = next+1u; value = c; if(utf_width) { *utf_width = width; } return error::none;
        }
        struct parser
        {
            ::std::string_view text{}; ::std::size_t cursor{}; program pending{}; error status{}; language_semantics language{language_semantics::shared_numeric};
            void space() noexcept
            { while(cursor < text.size() && ::fast_io::char_category::is_c_space(text[cursor])) { ++cursor; } }
            ::std::size_t append(node value)
            {
                if(pending.nodes.size() >= 128u) { status = error::limit_exceeded; return 0u; }
                pending.nodes.push_back(::std::move(value)); return pending.nodes.size() - 1u;
            }
            bool consume(::std::string_view token) noexcept
            {
                space(); if(text.substr(cursor).starts_with(token)) { cursor += token.size(); return true; } return false;
            }
            bool named_open(::std::string_view name, ::std::string_view opening) noexcept
            {
                auto const saved{cursor};
                if(consume(name) && consume(opening)) { return true; }
                cursor = saved; return false; // a partial keyword/punctuation probe consumes no producer name.
            }
            bool builtin(::std::string_view text, numeric_type& type) const noexcept
            {
                if(!builtin_type(text,type)) { return false; }
                if(language == language_semantics::rust && text == "char")
                { type={32u,true,false,false};type.rust_character=true; }
                return true;
            }
            ::std::size_t cast_node(numeric_type type, ::std::size_t child, operation op = operation::cast)
            {
                node n{}; n.op = op; n.lhs = child; n.literal_width = type.width;
                n.literal_unsigned = type.unsigned_value; n.floating_literal = type.floating; n.boolean_cast = type.boolean; n.builtin_identity = type.identity; n.wide_identity = type.wide_identity; n.utf_character_width = type.utf_character_width; n.rust_character = type.rust_character;
                return append(::std::move(n));
            }

            ::std::size_t rust_numeric_literal()
            {
                node value{}; value.op = operation::literal; value.rust_numeric_literal = true;
                unsigned base{10u};
                if(text.size()-cursor >= 2u && text[cursor] == '0')
                {
                    auto const prefix{text[cursor+1u]};
                    base = prefix == 'x' ? 16u : prefix == 'o' ? 8u : prefix == 'b' ? 2u : 10u;
                    if(base != 10u) { cursor += 2u; }
                }
                char normalized[4096u]{}; ::std::size_t count{};
                auto put{[&](char c) { if(count == sizeof(normalized)) { status=error::limit_exceeded; } else { normalized[count++]=c; } }};
                auto digits{[&](unsigned radix)
                {
                    bool any{};
                    while(cursor < text.size())
                    {
                        auto const c{text[cursor]};
                        if(c == '_') { ++cursor; continue; }
                        unsigned const digit{c >= '0' && c <= '9' ? static_cast<unsigned>(c-'0') :
                            c >= 'a' && c <= 'f' ? static_cast<unsigned>(c-'a'+10) :
                            c >= 'A' && c <= 'F' ? static_cast<unsigned>(c-'A'+10) : 16u};
                        if(digit >= radix) { break; }
                        any=true; put(c); ++cursor;
                    }
                    return any;
                }};
                if(!digits(base)) { status=error::malformed; return 0u; }
                bool real{};
                if(base == 10u && cursor < text.size() && text[cursor] == '.')
                {
                    // Rust 1. is a float; 1.f32 and 1._ are selector syntax,
                    // outside this finite scalar grammar. .5 is not a token.
                    auto const next{cursor+1u};
                    if(next == text.size() || (text[next] != '.' && text[next] != '_' &&
                        !::fast_io::char_category::is_c_alpha(text[next])))
                    { real=true; put(text[cursor++]); digits(10u); }
                }
                if(base == 10u && cursor < text.size() && (text[cursor] == 'e' || text[cursor] == 'E'))
                {
                    real=true; put(text[cursor++]);
                    if(cursor < text.size() && (text[cursor] == '+' || text[cursor] == '-')) { put(text[cursor++]); }
                    if(!digits(10u)) { status=error::malformed; return 0u; }
                }
                auto const suffix_begin{cursor};
                while(cursor < text.size() && (::fast_io::char_category::is_c_alnum(text[cursor]) || text[cursor] == '_')) { ++cursor; }
                auto const suffix{text.substr(suffix_begin,cursor-suffix_begin)};
                value.rust_numeric_suffix = !suffix.empty();
                if(status != error::none) { return 0u; }
                if(suffix == "f32" || suffix == "f64") { if(base != 10u) { status=error::unsupported; return 0u; } real=true; }
                auto const first{normalized}; auto const last{first+count};
                if(real)
                {
                    if(!suffix.empty() && suffix != "f32" && suffix != "f64") { status=error::unsupported; return 0u; }
                    if(suffix.empty()) { value.rust_numeric_text=::fast_io::concat_fast_io(::std::string_view{first,count}); }
                    value.floating_literal=true; value.literal_width=suffix == "f32" ? 32u : 64u;
                    if(value.literal_width == 32u)
                    {
                        float number{};auto const parsed{::fast_io::parse_by_scan(first,last,number)};
                        if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last || !::std::isfinite(number)) { status=error::malformed; return 0u; }
                        value.literal=::std::bit_cast<::std::uint32_t>(number);
                    }
                    else
                    {
                        double number{};auto const parsed{::fast_io::parse_by_scan(first,last,number)};
                        if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last || !::std::isfinite(number)) { status=error::malformed; return 0u; }
                        value.literal=::std::bit_cast<::std::uint64_t>(number);
                    }
                }
                else
                {
                    value.literal_width=32u; // finite unconstrained Rust default
                    if(!suffix.empty())
                    {
                        value.literal_unsigned=suffix.front() == 'u';
                        auto const type{suffix.size() >= 2u && (suffix.front() == 'i' || suffix.front() == 'u') ? suffix.substr(1u) : ::std::string_view{}};
                        value.literal_width=type == "8" ? 8u : type == "16" ? 16u : type == "32" ? 32u : type == "64" ? 64u : 0u;
                        value.guest_long_literal=type == "size";
                        if(value.literal_width == 0u && !value.guest_long_literal) { status=error::unsupported; return 0u; }
                    }
                    auto const parsed{base == 16u ? ::fast_io::parse_by_scan(first,last,::fast_io::mnp::hex_get<true,true>(value.literal)) :
                        base == 8u ? ::fast_io::parse_by_scan(first,last,::fast_io::mnp::oct_get<true,true>(value.literal)) :
                        base == 2u ? ::fast_io::parse_by_scan(first,last,::fast_io::mnp::bin_get<true,true>(value.literal)) :
                        ::fast_io::parse_by_scan(first,last,::fast_io::mnp::dec_get<true,true>(value.literal))};
                    if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last) { status=error::malformed; return 0u; }
                }
                return append(::std::move(value));
            }
            ::std::size_t primary(unsigned depth)
            {
                space(); if(depth >= 32u) { status = error::limit_exceeded; return 0u; }
                if(cursor >= text.size()) { status = error::malformed; return 0u; }
                if(text.substr(cursor).starts_with("sizeof") && (text.size()-cursor == 6u ||
                    (!::fast_io::char_category::is_c_alnum(text[cursor+6u]) && text[cursor+6u] != '_')) && consume("sizeof"))
                {
                    if(!consume("("))
                    {
                        // sizeof owns one unary operand. Binary/conditional
                        // operators remain outside its unevaluated type query.
                        auto const child{unary(depth+1u)};
                        if(status != error::none || child >= pending.nodes.size()) { return 0u; }
                        node value{}; value.op = operation::expression_size; value.lhs = child;
                        if(pending.nodes[child].op == operation::reference)
                        { value.op = operation::size; value.reference = pending.nodes[child].reference; }
                        return append(::std::move(value));
                    }
                    auto const begin{cursor}; unsigned nesting{1u};
                    while(cursor < text.size() && nesting != 0u)
                    {
                        auto const c{text[cursor++]};
                        // Probe the same complete bounded character token used
                        // by primary. Failed probes (including digit separators)
                        // consume nothing and the operand parser rejects errors.
                        if(c == '\'')
                        {
                            auto probe{cursor-1u}; ::std::uint64_t ignored{};
                            if(character_literal(text,probe,ignored,language) == error::none) { cursor = probe; continue; }
                        }
                        if(c == '(') { if(++nesting > 32u) { status = error::limit_exceeded; return 0u; } }
                        else if(c == ')') { --nesting; }
                    }
                    node value{}; value.op = operation::size; numeric_type type{};
                    if(nesting == 0u && builtin(text.substr(begin, cursor - begin - 1u),type))
                    { value.op = operation::type_size; value.literal_width = type.width; value.utf_character_width = type.utf_character_width; return append(::std::move(value)); }
                    if(nesting != 0u) { status = error::unsupported; return 0u; }
                    auto const end{cursor};
                    if(dwarf::parse_source_expression(text.substr(begin,end-begin-1u),value.reference) == dwarf::object_selector_error::none &&
                       !(value.reference.steps.empty() && (value.reference.root_name == "true" || value.reference.root_name == "false")))
                    { return append(::std::move(value)); } // retain aggregate/pointer declaration-only sizeof.
                    cursor = begin; value.op = operation::expression_size;
                    value.lhs = expression(1u,depth+1u);
                    if(status != error::none) { return 0u; }
                    if(!consume(")") || cursor != end) { status = error::unsupported; return 0u; }
                    return append(::std::move(value));
                }
                {
                    auto const saved{cursor};
                    if(named_open("len","(") || named_open("cap","("))
                    {
                        dwarf::expression_details::parser state{text,{},saved,32u,32u};
                        auto const result{state.primary(depth)};
                        if(result != dwarf::object_selector_error::none)
                        {
                            status = result == dwarf::object_selector_error::limit_exceeded ? error::limit_exceeded :
                                result == dwarf::object_selector_error::malformed ? error::malformed : error::unsupported;
                            return 0u;
                        }
                        cursor = state.cursor; node value{}; value.op = operation::reference;
                        value.reference = ::std::move(state.pending); return append(::std::move(value));
                    }
                }
                if(named_open("@as","("))
                {
                    // A finite Zig coercion, never a function call. Only the
                    // shared builtin numeric names are accepted; no pointers,
                    // bool coercion, CTFE or context-free @intCast is invented.
                    auto const begin{cursor}; auto const end{text.find(',',cursor)}; numeric_type type{};
                    if(end == text.npos || !zig_builtin_type(text.substr(begin,end-begin),type) || type.boolean)
                    { status = error::unsupported; return 0u; }
                    cursor = end+1u; auto const child{expression(1u,depth+1u)};
                    if(!consume(")")) { status = error::malformed; return 0u; }
                    return cast_node(type,child,operation::zig_coerce);
                }
                if(named_open("static_cast","<"))
                {
                    auto const begin{cursor}; auto const end{text.find('>',cursor)}; numeric_type type{};
                    if(end == text.npos || !builtin(text.substr(begin,end-begin),type)) { status = error::unsupported; return 0u; }
                    cursor = end + 1u; if(!consume("(")) { status = error::malformed; return 0u; }
                    auto const child{expression(1u,depth+1u)}; if(!consume(")")) { status = error::malformed; return 0u; }
                    return cast_node(type,child);
                }
                if(consume("("))
                {
                    auto const begin{cursor}; auto const end{text.find(')',cursor)}; numeric_type type{};
                    if(end != text.npos && builtin(text.substr(begin,end-begin),type))
                    { cursor = end + 1u; return cast_node(type,unary(depth+1u)); }
                    auto const result{expression(1u, depth + 1u)};
                    if(status != error::none || !consume(")"))
                    { if(status == error::none) { status = error::malformed; } return 0u; }
                    if(result < pending.nodes.size() && pending.nodes[result].op == operation::reference)
                    {
                        // Parentheses preserve this existing producer reference.
                        // Postfix syntax cannot attach to arithmetic, literals,
                        // sizeof, casts or conditional numeric results.
                        node value{pending.nodes[result]}; auto const old_steps{value.reference.steps.size()};
                        dwarf::expression_details::parser state{text,::std::move(value.reference),cursor,32u,32u};
                        auto const tail_status{state.postfix_tail(true)};
                        if(tail_status != dwarf::object_selector_error::none)
                        {
                            status = tail_status == dwarf::object_selector_error::limit_exceeded ? error::limit_exceeded :
                                tail_status == dwarf::object_selector_error::malformed ? error::malformed : error::unsupported;
                            return 0u;
                        }
                        cursor = state.cursor;
                        if(state.pending.steps.size() != old_steps)
                        { value.reference = ::std::move(state.pending); return append(::std::move(value)); }
                    }
                    return result;
                }
                // Numeric conversion syntax is read-only; no guest/host function is called.
                {
                    auto const begin{cursor}; auto end{cursor};
                    while(end < text.size() && (::fast_io::char_category::is_c_alnum(text[end]) || text[end] == '_')) { ++end; }
                    numeric_type type{};
                    if(end > begin && builtin(text.substr(begin,end-begin),type))
                    {
                        cursor = end;
                        if(consume("("))
                        { auto const child{expression(1u,depth+1u)}; if(!consume(")")) { status = error::malformed; return 0u; } return cast_node(type,child); }
                        cursor = begin;
                    }
                }
                for(auto const keyword : {::std::string_view{"true"},::std::string_view{"false"}})
                {
                    if(text.substr(cursor).starts_with(keyword) && (text.size()-cursor == keyword.size() ||
                       (!::fast_io::char_category::is_c_alnum(text[cursor+keyword.size()]) && text[cursor+keyword.size()] != '_')))
                    { cursor += keyword.size(); node value{}; value.op = operation::literal; value.literal = keyword == "true"; value.boolean_literal = true; return append(::std::move(value)); }
                }
                auto const begin{cursor};
                if(text[cursor] == '\'' || (language == language_semantics::cpp && utf_character_prefix(text,cursor)))
                {
                    ::std::uint64_t c{}; unsigned width{}; status = character_literal(text,cursor,c,language,&width);
                    if(status != error::none) { return 0u; }
                    node value{}; value.op = operation::literal; value.literal = c; value.character_literal = true; value.utf_character_width = width; value.rust_character = language == language_semantics::rust; return append(::std::move(value));
                }
                if(::fast_io::char_category::is_c_digit(text[cursor]) ||
                   (text[cursor] == '.' && cursor+1u < text.size() && ::fast_io::char_category::is_c_digit(text[cursor+1u])))
                {
                    if(language == language_semantics::rust) { return rust_numeric_literal(); }
                    node value{}; value.op = operation::literal;
                    value.hex_literal = text.size()-cursor >= 2u && text[cursor] == '0' && (text[cursor+1u] == 'x' || text[cursor+1u] == 'X');
                    if(!value.hex_literal)
                    {
                        // Decimal float separators belong between successive digits
                        // in each significand/exponent segment, never beside . or a sign.
                        // Normalize owned syntax for fast_io; the integer path below
                        // keeps its existing base-prefix and candidate-type rules.
                        char normalized[4096u]{}; ::std::size_t count{}; bool bad_separator{};
                        auto const put{[&](char c)
                        {
                            if(count == sizeof(normalized)) { status = error::limit_exceeded; }
                            else { normalized[count++] = c; }
                        }};
                        auto const digits{[&]()
                        {
                            auto const start{cursor}; bool any{};
                            while(cursor < text.size() && status == error::none)
                            {
                                auto const c{text[cursor]};
                                if(::fast_io::char_category::is_c_digit(c)) { put(c); any = true; }
                                else if(c == '\'' || c == '_')
                                {
                                    if(cursor == start || !::fast_io::char_category::is_c_digit(text[cursor-1u]) ||
                                       cursor+1u == text.size() || !::fast_io::char_category::is_c_digit(text[cursor+1u]))
                                    { bad_separator = true; }
                                }
                                else { break; }
                                ++cursor;
                            }
                            return any;
                        }};
                        digits(); bool real{};
                        if(cursor < text.size() && text[cursor] == '.')
                        { real = true; put(text[cursor++]); digits(); }
                        if(cursor < text.size() && (text[cursor] == 'e' || text[cursor] == 'E'))
                        {
                            real = true; put(text[cursor++]);
                            if(cursor < text.size() && (text[cursor] == '+' || text[cursor] == '-')) { put(text[cursor++]); }
                            if(!digits()) { bad_separator = true; }
                        }
                        if(status != error::none) { return 0u; }
                        if(real)
                        {
                            if(bad_separator) { status = error::malformed; return 0u; }
                            value.floating_literal = true; value.literal_width = 64u;
                            auto const first{normalized}; auto const last{first+count};
                            if(cursor < text.size() && (text[cursor] == 'f' || text[cursor] == 'F'))
                            {
                                ++cursor; float number{}; auto const parsed{::fast_io::parse_by_scan(first,last,number)};
                                if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last || !::std::isfinite(number)) { status = error::malformed; return 0u; }
                                value.literal_width = 32u; value.literal = ::std::bit_cast<::std::uint32_t>(number);
                            }
                            else
                            {
                                double number{}; auto const parsed{::fast_io::parse_by_scan(first,last,number)};
                                if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last || !::std::isfinite(number)) { status = error::malformed; return 0u; }
                                value.literal = ::std::bit_cast<::std::uint64_t>(number);
                            }
                            return append(::std::move(value));
                        }
                        cursor = begin;
                    }
                    unsigned base{value.hex_literal ? 16u : 10u};
                    if(text.size()-cursor >= 2u && text[cursor] == '0')
                    {
                        auto const prefix{text[cursor+1u]};
                        if(prefix == 'b' || prefix == 'B') { base = 2u; }
                        else if(prefix == 'o' || prefix == 'O') { base = 8u; }
                        else if(::fast_io::char_category::is_c_digit(prefix) || prefix == '\'' || prefix == '_') { base = 8u; }
                    }
                    value.hex_literal = base != 10u; // C integer candidate rules apply to every nondecimal base.
                    if(base == 16u || (base != 10u && text.size()-cursor >= 2u &&
                        !::fast_io::char_category::is_c_digit(text[cursor+1u]) && text[cursor+1u] != '\'' && text[cursor+1u] != '_'))
                    {
                        cursor += 2u;
                        if(cursor < text.size() && text[cursor] == '_') { ++cursor; }
                    }
                    char normalized[4096u]{}; ::std::size_t count{}; bool separator{};
                    while(cursor < text.size())
                    {
                        auto const c{text[cursor]};
                        if(c == '\'' || c == '_')
                        {
                            if(count == 0u || separator) { status = error::malformed; return 0u; }
                            separator = true; ++cursor; continue;
                        }
                        bool const digit{::fast_io::char_category::is_c_digit(c) ||
                            (base == 16u && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))};
                        if(!digit) { break; }
                        if(count == sizeof(normalized)) { status = error::limit_exceeded; return 0u; }
                        normalized[count++] = c; separator = false; ++cursor;
                    }
                    if(count == 0u || separator) { status = error::malformed; return 0u; }
                    auto const first{normalized}; auto const last{first + count};
                    auto const parsed{base == 16u ? ::fast_io::parse_by_scan(first,last,::fast_io::mnp::hex_get<true,true>(value.literal)) :
                        base == 8u ? ::fast_io::parse_by_scan(first,last,::fast_io::mnp::oct_get<true,true>(value.literal)) :
                        base == 2u ? ::fast_io::parse_by_scan(first,last,::fast_io::mnp::bin_get<true,true>(value.literal)) :
                        ::fast_io::parse_by_scan(first,last,::fast_io::mnp::dec_get<true,true>(value.literal))};
                    if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last) { status = error::malformed; return 0u; }
                    // A finite standard C suffix, not an arbitrary trailing identifier.
                    auto const suffix_begin{cursor};
                    while(cursor < text.size() && (text[cursor] == 'u' || text[cursor] == 'U' || text[cursor] == 'l' || text[cursor] == 'L')) { ++cursor; }
                    auto suffix{text.substr(suffix_begin,cursor-suffix_begin)};
                    if(!suffix.empty())
                    {
                        if(suffix.front() == 'u' || suffix.front() == 'U') { value.literal_unsigned = true; suffix.remove_prefix(1u); }
                        if(!suffix.empty() && (suffix.back() == 'u' || suffix.back() == 'U'))
                        { if(value.literal_unsigned) { status = error::malformed; return 0u; } value.literal_unsigned = true; suffix.remove_suffix(1u); }
                        if(suffix.size() > 2u || (suffix.size() == 2u && suffix[0u] != suffix[1u])) { status = error::malformed; return 0u; }
                        if(!suffix.empty()) { value.literal_width = suffix.size() == 2u ? 64u : 0u; value.guest_long_literal = suffix.size() == 1u; }
                    }
                    return append(::std::move(value));
                }
                // A leaf is precisely the existing bounded producer-name/selector
                // grammar. Operators outside [] are consumed by the scalar parser.
                unsigned brackets{};
                while(cursor < text.size())
                {
                    auto const c{text[cursor]};
                    if(brackets == 0u && ::fast_io::char_category::is_c_space(c))
                    { auto look{cursor}; while(look < text.size() && ::fast_io::char_category::is_c_space(text[look])) { ++look; }
                      if(text.substr(look).starts_with("as ")) { break; }
                      bool logical_word{};
                      for(auto word : {::std::string_view{"and"},::std::string_view{"or"}})
                      { auto const end{look+word.size()};
                        if(text.substr(look).starts_with(word) && (end == text.size() ||
                           (!::fast_io::char_category::is_c_alnum(text[end]) && text[end] != '_'))) { logical_word = true; } }
                      if(logical_word) { break; } }
                    if(brackets == 0u && c == '.' && text.size()-cursor >= 2u && text[cursor+1u] == '*')
                    { cursor += 2u; continue; } // A postfix selector belongs to this same bounded reference leaf.
                    if(c == '[') { ++brackets; }
                    else if(c == ']') { if(brackets == 0u) { break; } --brackets; }
                    if(brackets == 0u && c == ':' && text.substr(cursor).starts_with("::"))
                    { cursor += 2u; continue; } // preserve the existing producer selector boundary.
                    if(brackets == 0u && (c == ':' || c == ')' || c == '(' || c == '+' || c == '*' || c == '/' || c == '%' || c == '<' ||
                        c == '>' || c == '?' || c == ',' || c == '=' || c == '!' || c == '&' || c == '^' || c == '|' || c == '~')) { break; }
                    if(brackets == 0u && c == '-')
                    { if(cursor + 1u < text.size() && text[cursor + 1u] == '>') { cursor += 2u; continue; } break; }
                    ++cursor;
                }
                node value{}; value.op = operation::reference;
                if(begin == cursor || dwarf::parse_source_expression(text.substr(begin, cursor - begin), value.reference) != dwarf::object_selector_error::none)
                { status = error::unsupported; return 0u; }
                return append(::std::move(value));
            }
            ::std::size_t unary(unsigned depth)
            {
                if(depth >= 32u) { status = error::limit_exceeded; return 0u; }
                space();
                if(text.substr(cursor).starts_with("++") || text.substr(cursor).starts_with("--"))
                { status = error::unsupported; return 0u; }
                if(consume("*"))
                {
                    auto const child{unary(depth+1u)};
                    if(status != error::none || child >= pending.nodes.size() || pending.nodes[child].op != operation::reference)
                    { status = error::unsupported; return 0u; }
                    node value{pending.nodes[child]};
                    if(!value.reference.steps.empty() && (value.reference.steps.back().kind == dwarf::source_expression_step_kind::go_length ||
                        value.reference.steps.back().kind == dwarf::source_expression_step_kind::go_capacity))
                    { status = error::unsupported; return 0u; }
                    if(value.reference.steps.size() >= 32u) { status = error::limit_exceeded; return 0u; }
                    value.reference.steps.push_back({dwarf::source_expression_step_kind::dereference,{},{}}); return append(::std::move(value));
                }
                operation op{}; bool prefix{true};
                if(consume("+")) { op = operation::positive; }
                else if(consume("-")) { op = operation::negative; }
                else if(consume("~")) { op = operation::invert; }
                else if(consume("!")) { op = operation::logical_not; }
                else { prefix = false; }
                if(!prefix) { return primary(depth); }
                auto const child{unary(depth + 1u)}; node value{}; value.op = op; value.lhs = child; return append(::std::move(value));
            }
            struct binary { ::std::string_view token; operation op; unsigned precedence; bool keyword; };
            static constexpr binary operators[] = {
                {"or",operation::logical_or,1u,true},{"and",operation::logical_and,2u,true},
                {"||",operation::logical_or,1u,false},{"&&",operation::logical_and,2u,false},
                {"|",operation::bit_or,3u,false},{"^",operation::bit_xor,4u,false},{"&^",operation::bit_clear,10u,false},{"&",operation::bit_and,5u,false},
                {"==",operation::equal,6u,false},{"!=",operation::unequal,6u,false},
                {"<=",operation::less_equal,7u,false},{">=",operation::greater_equal,7u,false},
                {"<<",operation::left,8u,false},{">>",operation::right,8u,false},
                {"<",operation::less,7u,false},{">",operation::greater,7u,false},
                {"+",operation::add,9u,false},{"-",operation::subtract,9u,false},
                {"*",operation::multiply,10u,false},{"/",operation::divide,10u,false},{"%",operation::remainder,10u,false}};
            unsigned precedence(binary const& op) const noexcept
            {
                if(language == language_semantics::rust)
                {
                    switch(op.op)
                    {
                        case operation::bit_and: return 7u;
                        case operation::bit_xor: return 6u;
                        case operation::bit_or: return 5u;
                        case operation::less: case operation::less_equal: case operation::greater: case operation::greater_equal:
                        case operation::equal: case operation::unequal: return 4u;
                        default: break;
                    }
                }
                if(language == language_semantics::go)
                {
                    switch(op.op)
                    {
                        case operation::left: case operation::right: case operation::bit_and: case operation::bit_clear: return 10u;
                        case operation::bit_or: case operation::bit_xor: return 9u;
                        case operation::equal: case operation::unequal: return 7u;
                        default: break;
                    }
                }
                return op.precedence;
            }
            ::std::size_t expression(unsigned minimum, unsigned depth)
            {
                if(depth >= 32u) { status = error::limit_exceeded; return 0u; }
                auto lhs{unary(depth + 1u)};
                while(status == error::none)
                {
                    space();
                    if(text.substr(cursor).starts_with("++") || text.substr(cursor).starts_with("--"))
                    { status = error::unsupported; break; }
                    if(minimum <= 11u && text.substr(cursor).starts_with("as "))
                    {
                        cursor += 3u; space(); auto const begin{cursor};
                        while(cursor < text.size() && (::fast_io::char_category::is_c_alnum(text[cursor]) || text[cursor] == '_')) { ++cursor; }
                        numeric_type type{}; if(!builtin(text.substr(begin,cursor-begin),type)) { status = error::unsupported; break; }
                        lhs = cast_node(type,lhs); pending.nodes[lhs].rust_cast_as = true; continue;
                    }
                    binary const* next{};
                    for(auto const& candidate : operators)
                    { auto const end{cursor+candidate.token.size()};
                      if(text.substr(cursor).starts_with(candidate.token) && (!candidate.keyword || end == text.size() ||
                         (!::fast_io::char_category::is_c_alnum(text[end]) && text[end] != '_')))
                      { next = ::std::addressof(candidate); break; } }
                    if(next == nullptr || precedence(*next) < minimum) { break; }
                    cursor += next->token.size(); auto const rhs{expression(precedence(*next) + 1u, depth + 1u)};
                    node value{}; value.op = next->op; value.lhs = lhs; value.rhs = rhs; value.logical_keyword = next->keyword; lhs = append(::std::move(value));
                }
                if(status == error::none && minimum == 1u && consume("?"))
                {
                    // Lowest precedence and right associative. Both arms must
                    // be fully parsed, including the one not evaluated later.
                    auto const yes{expression(1u,depth+1u)};
                    if(status != error::none || !consume(":")) { if(status == error::none) { status = error::malformed; } return 0u; }
                    auto const no{expression(1u,depth+1u)};
                    node value{}; value.op = operation::conditional; value.lhs = lhs; value.rhs = yes; value.third = no;
                    lhs = append(::std::move(value));
                }
                return lhs;
            }
        };
        inline constexpr ::std::uint64_t mask(unsigned width) noexcept
        { return width == 64u ? ~::std::uint64_t{} : (::std::uint64_t{1u} << width) - 1u; }
        inline constexpr ::std::int64_t signed_bits(integer value) noexcept
        {
            auto bits{value.bits & mask(value.width)};
            if(value.width < 64u && (bits & (::std::uint64_t{1u} << (value.width - 1u)))) { bits |= ~mask(value.width); }
            return ::std::bit_cast<::std::int64_t>(bits);
        }
        inline constexpr integer promote(integer value, language_semantics language = language_semantics::shared_numeric) noexcept
        {
            if(language == language_semantics::rust) { value.bits &= mask(value.width); return value; }
            // A declared boolean promotes to C int regardless of producer
            // storage width; its copied nonzero bits become the value one.
            if(value.category == value_category::boolean)
            { integer out{value.bits != 0u,32u,false,false,value_category::integer};
              if(native_integer_language(language)) { out.wide_identity = wide_builtin::signed_int; } return out; }
            if(value.utf_character_width)
            {
                // Finite Wasm C++ ABI: unsigned 8/16 -> int; unsigned 32
                // -> unsigned int, even when this particular value is small.
                integer out{value.bits,32u,value.width == 32u,false,value_category::integer};
                if(native_integer_language(language)) { out.wide_identity = value.width == 32u ? wide_builtin::unsigned_int : wide_builtin::signed_int; }
                return out;
            }
            if(!value.floating && value.width < 32u)
            { value.bits = value.unsigned_value ? value.bits : static_cast<::std::uint64_t>(signed_bits(value)); value.width = 32u; value.unsigned_value = false; value.builtin_identity = {}; value.declaration_identity = {}; value.declaration_identity_known = false;
              if(native_integer_language(language)) { value.wide_identity = wide_builtin::signed_int; } }
            value.bits &= mask(value.width); return value;
        }
        inline double floating_number(integer value) noexcept
        {
            if(value.floating) { return value.width == 32u ? static_cast<double>(::std::bit_cast<float>(static_cast<::std::uint32_t>(value.bits))) :
                ::std::bit_cast<double>(value.bits); }
            return value.unsigned_value ? static_cast<double>(value.bits) : static_cast<double>(signed_bits(value));
        }
        inline bool truth(integer value) noexcept
        { return value.floating ? floating_number(value) != 0.0 : value.bits != 0u; }
        inline integer real_value(double value, unsigned width) noexcept
        {
            if(width != 32u) { return {::std::bit_cast<::std::uint64_t>(value),64u,false,true}; }
            // Avoid an out-of-range C++ floating conversion. The shared IEEE
            // numeric subset represents overflow explicitly; Zig @as rejects
            // an unrepresentable constant before entering this helper below.
            auto const narrowed{value > (::std::numeric_limits<float>::max)() ? ::std::numeric_limits<float>::infinity() :
                value < (::std::numeric_limits<float>::lowest)() ? -::std::numeric_limits<float>::infinity() : static_cast<float>(value)};
            return {::std::bit_cast<::std::uint32_t>(narrowed),32u,false,true};
        }
        inline error numeric_literal_value(node const& n, integer& out, unsigned guest_bits, language_semantics language) noexcept
        {
            if(n.boolean_literal && (language == language_semantics::cpp || language == language_semantics::c23 || native_boolean_language(language)))
            { out = {n.literal != 0u,8u,true,false,value_category::boolean}; return error::none; }
            if(n.rust_character)
            {
                if(language != language_semantics::rust || !unicode_scalar(n.literal)) { return error::unsupported; }
                out={n.literal,32u,true,false,value_category::integer};out.rust_character=true;return error::none;
            }
            if(n.utf_character_width)
            {
                if(language != language_semantics::cpp || (n.utf_character_width != 8u && n.utf_character_width != 16u && n.utf_character_width != 32u)) { return error::unsupported; }
                out = {n.literal,n.utf_character_width,true,false,value_category::integer};
                out.utf_character_width = n.utf_character_width; out.rust_character = n.rust_character; return error::none;
            }
            if(n.character_literal && language == language_semantics::cpp)
            { out = {n.literal,8u,false,false,value_category::integer,narrow_builtin::plain_char}; return error::none; }
            if(n.floating_literal) { out = {n.literal,n.literal_width,false,true}; return error::none; }
            if(n.rust_numeric_literal)
            {
                if(language != language_semantics::rust) { return error::unsupported; }
                auto const width{n.guest_long_literal ? guest_bits : n.literal_width};
                if(n.literal > (n.literal_unsigned ? mask(width) : mask(width)>>1u)) { return error::arithmetic; }
                out={n.literal,width,n.literal_unsigned,false,value_category::integer};out.rust_pointer_sized=n.guest_long_literal;return error::none;
            }
            if(n.literal_unsigned || n.literal_width || n.guest_long_literal)
            {
                // C long follows the authenticated guest ABI, never this host.
                auto width{n.literal_width ? n.literal_width : n.guest_long_literal ? guest_bits : n.literal <= 0xffffffffu ? 32u : 64u};
                if(width == 32u && ((!n.literal_unsigned && !n.hex_literal && n.literal > 0x7fffffffu) || n.literal > 0xffffffffu)) { width = 64u; }
                if(n.literal > mask(width) || (!n.literal_unsigned && !n.hex_literal && n.literal > (mask(width)>>1u))) { return error::arithmetic; }
                out = {n.literal,width,n.literal_unsigned || (n.hex_literal && n.literal > (mask(width)>>1u))}; return error::none;
            }
            if(n.hex_literal)
            { auto const width{n.literal <= 0xffffffffu ? 32u : 64u}; out = {n.literal,width,n.literal > (mask(width)>>1u)}; }
            else { out = {n.literal, n.literal <= 0x7fffffffu ? 32u : 64u, n.literal > 0x7fffffffffffffffull}; }
            return error::none;
        }
        inline error literal_value(node const& n, integer& out, unsigned guest_bits, language_semantics language = language_semantics::shared_numeric) noexcept
        {
            auto const status{numeric_literal_value(n,out,guest_bits,language)};
            if(status != error::none || !native_integer_language(language) || out.floating || out.category == value_category::boolean || out.width < 32u || out.utf_character_width)
            { return status; }
            // An unsuffixed decimal beyond signed 64 bits has no standard
            // candidate in this profile; preserve the old finite value without
            // fabricating an extended or unsigned native type.
            if(!n.hex_literal && !n.literal_unsigned && out.unsigned_value) { return status; }
            auto const rank{n.literal_width == 64u ? 5u : n.guest_long_literal ? (out.width == guest_bits ? 4u : 5u) :
                out.width == 32u ? 3u : guest_bits == 64u ? 4u : 5u};
            out.wide_identity = rank == 3u ? (out.unsigned_value ? wide_builtin::unsigned_int : wide_builtin::signed_int) :
                rank == 4u ? (out.unsigned_value ? wide_builtin::unsigned_long : wide_builtin::signed_long) :
                out.unsigned_value ? wide_builtin::unsigned_long_long : wide_builtin::signed_long_long;
            return status;
        }

        // Unary minus on a literal (including parentheses, erased by parsing)
        // admits exactly one extra signed magnitude: -128i8, etc. Never relax
        // the standalone literal or a cast/reference/compound operand.
        inline error rust_negative_literal(program const& code, node const& n, integer& out, unsigned guest_bits) noexcept
        {
            auto const& child{code.nodes[n.lhs]};
            auto const width{child.guest_long_literal ? guest_bits : child.literal_width};
            if(child.literal_unsigned) { return error::unsupported; }
            if(child.literal > (::std::uint64_t{1u} << (width-1u))) { return error::arithmetic; }
            out={ (0u-child.literal)&mask(width),width,false,false,value_category::integer };out.rust_pointer_sized=child.guest_long_literal;
            return error::none;
        }
        inline bool is_rust_negative_literal(program const& code, node const& n, language_semantics language) noexcept
        {
            return language == language_semantics::rust && n.op == operation::negative && n.lhs < code.nodes.size() &&
                code.nodes[n.lhs].op == operation::literal && code.nodes[n.lhs].rust_numeric_literal && !code.nodes[n.lhs].floating_literal;
        }

        inline node const* rust_untyped_literal(program const& code, ::std::size_t index) noexcept
        {
            if(index >= code.nodes.size()) { return nullptr; }
            auto const* literal{::std::addressof(code.nodes[index])};
            if(literal->op == operation::negative)
            { if(literal->lhs >= code.nodes.size()) { return nullptr; } literal=::std::addressof(code.nodes[literal->lhs]); }
            return literal->op == operation::literal && literal->rust_numeric_literal && !literal->rust_numeric_suffix ? literal : nullptr;
        }
        inline bool rust_context_binary(operation op, language_semantics language) noexcept
        {
            if(language != language_semantics::rust) { return false; }
            switch(op)
            {
                case operation::add:case operation::subtract:case operation::multiply:case operation::divide:
                case operation::remainder:case operation::bit_and:case operation::bit_xor:case operation::bit_or:
                case operation::less:case operation::less_equal:case operation::greater:case operation::greater_equal:
                case operation::equal:case operation::unequal:return true;
                default:return false;
            }
        }
        inline error rust_literal_as_type(program const& code, ::std::size_t index, integer const& type, integer& out) noexcept
        {
            auto const* literal{rust_untyped_literal(code,index)};
            if(literal == nullptr || type.rust_character || type.category == value_category::boolean ||
                type.width == 0u || type.width > 64u || literal->floating_literal != type.floating) { return error::unsupported; }
            bool const negative{code.nodes[index].op == operation::negative};
            if(type.floating)
            {
                auto const first{literal->rust_numeric_text.data()};auto const last{first+literal->rust_numeric_text.size()};
                if(first == last) { return error::unavailable; }
                if(type.width == 32u)
                {
                    float number{};auto const parsed{::fast_io::parse_by_scan(first,last,number)};
                    if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last || !::std::isfinite(number)) { return error::arithmetic; }
                    out={::std::bit_cast<::std::uint32_t>(negative ? -number : number),32u,false,true};
                }
                else
                {
                    double number{};auto const parsed{::fast_io::parse_by_scan(first,last,number)};
                    if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last || !::std::isfinite(number)) { return error::arithmetic; }
                    out={::std::bit_cast<::std::uint64_t>(negative ? -number : number),64u,false,true};
                }
                return error::none;
            }
            if(negative && type.unsigned_value) { return error::unsupported; }
            auto const maximum{type.unsigned_value ? mask(type.width) : (mask(type.width)>>1u)+(negative ? 1u : 0u)};
            if(literal->literal > maximum) { return error::arithmetic; }
            out={(negative ? 0u-literal->literal : literal->literal)&mask(type.width),type.width,type.unsigned_value,false,value_category::integer};out.rust_pointer_sized=type.rust_pointer_sized;
            return error::none;
        }
        inline bool rust_numeric_operands(operation op, integer const& a, integer const& b, language_semantics language) noexcept
        {
            if(language != language_semantics::rust) { return true; }
            if(op == operation::left || op == operation::right) { return !a.floating && !b.floating; }
            return a.width == b.width && a.unsigned_value == b.unsigned_value && a.floating == b.floating && a.rust_pointer_sized == b.rust_pointer_sized;
        }
        inline node const* zig_coercion_constant(program const& code, node const& child) noexcept
        {
            auto const* literal{::std::addressof(child)};
            if(child.op == operation::negative)
            {
                if(child.lhs >= code.nodes.size()) { return nullptr; }
                literal = ::std::addressof(code.nodes[child.lhs]);
            }
            return literal->op == operation::literal && !literal->boolean_literal &&
                !literal->literal_unsigned && !literal->guest_long_literal &&
                (literal->floating_literal ? literal->literal_width == 64u : literal->literal_width == 0u) ? literal : nullptr;
        }
        inline error zig_constant_value(node const& child, node const& literal, integer& out, unsigned guest_bits) noexcept
        {
            auto const status{literal_value(literal,out,guest_bits)};
            if(status != error::none || child.op != operation::negative) { return status; }
            if(out.floating) { out = real_value(-floating_number(out),out.width); }
            else
            {
                // Use owned magnitude before C literal promotion/negation.
                // INT64_MIN is representable; no host signed negation occurs.
                if(literal.literal > (::std::uint64_t{1u} << 63u)) { return error::arithmetic; }
                out = {0u-literal.literal,64u,false};
            }
            return error::none;
        }
        inline bool zig_coercion_source_supported(program const& code, node const& child) noexcept
        {
            // Compound CTFE, C casts and C-style boolean/arithmetic promotion
            // cannot supply Zig type evidence in this finite numeric subset.
            return child.op == operation::reference || child.op == operation::zig_coerce ||
                zig_coercion_constant(code,child) != nullptr;
        }
        inline error check_zig_coercion(program const& code, node const& n, node const& child, integer const& a, unsigned guest_bits) noexcept
        {
            if(!zig_coercion_source_supported(code,child) || a.category == value_category::boolean ||
                (a.width == 8u && a.category != value_category::integer)) { return error::unsupported; }
            auto const width{n.literal_width ? n.literal_width : guest_bits};
            auto const* constant{zig_coercion_constant(code,child)};
            if(n.floating_literal)
            {
                if(!a.floating || (!constant && width < a.width)) { return error::unsupported; }
                if(constant)
                {
                    auto const number{floating_number(a)};
                    if(!::std::isfinite(number) || (width == 32u &&
                        (number > (::std::numeric_limits<float>::max)() || number < (::std::numeric_limits<float>::lowest)())))
                    { return error::arithmetic; }
                    if(!::std::isfinite(floating_number(real_value(number,width)))) { return error::arithmetic; }
                }
                return error::none;
            }
            if(a.floating) { return error::unsupported; }
            if(constant)
            {
                auto const maximum{n.literal_unsigned ? mask(width) : mask(width)>>1u};
                if(child.op == operation::negative && constant->literal != 0u)
                {
                    if(n.literal_unsigned || constant->literal > (::std::uint64_t{1u} << (width-1u))) { return error::arithmetic; }
                }
                else if(a.bits > maximum) { return error::arithmetic; }
            }
            else if(n.literal_unsigned ? (!a.unsigned_value || width < a.width) :
                (a.unsigned_value ? width <= a.width : width < a.width))
            { return error::unsupported; }
            return error::none;
        }
        // Finite copied scalar result types. Language and explicit type DATA
        // select promotions; no type query evaluates locations or yields a
        // writable glvalue, pointer or native debugging capability.
        struct no_type_resolver
        {
            bool operator()(dwarf::source_expression const&, integer&) const noexcept { return false; }
        };
        inline constexpr bool type_resolver_supplied(no_type_resolver const&) noexcept { return false; }
        template<typename TypeResolver>
        inline constexpr bool type_resolver_supplied(TypeResolver const&) noexcept { return true; }
        inline bool valid_wide_identity(integer const& value, unsigned guest_bits = 0u) noexcept
        {
            if(value.wide_identity == wide_builtin::unknown) { return true; }
            if(value.floating || value.category == value_category::boolean || value.builtin_identity != narrow_builtin::unknown ||
               (value.width != 32u && value.width != 64u) || (value.declaration_identity_known && value.declaration_identity.offset == 0u)) { return false; }
            auto const address_bytes{guest_bits == 0u ? value.width/8u : guest_bits/8u};
            return dwarf::c_wide_extent(value.wide_identity,value.width/8u,address_bytes) &&
                value.unsigned_value == dwarf::c_wide_unsigned(value.wide_identity);
        }
        inline bool valid_narrow_identity(integer const& value) noexcept
        {
            if(value.wide_identity != wide_builtin::unknown) { return valid_wide_identity(value); }
            if(value.builtin_identity == narrow_builtin::unknown && !value.declaration_identity_known) { return true; }
            if(value.floating || value.category != value_category::integer || (value.width != 8u && value.width != 16u) ||
               (value.declaration_identity_known && value.declaration_identity.offset == 0u)) { return false; }
            switch(value.builtin_identity)
            {
                case narrow_builtin::unknown: return true;
                case narrow_builtin::plain_char: case narrow_builtin::signed_char: return value.width == 8u && !value.unsigned_value;
                case narrow_builtin::unsigned_char: return value.width == 8u && value.unsigned_value;
                case narrow_builtin::signed_short: return value.width == 16u && !value.unsigned_value;
                case narrow_builtin::unsigned_short: return value.width == 16u && value.unsigned_value;
            }
            return false;
        }
        inline bool same_narrow_identity(integer const& a, integer const& b) noexcept
        {
            if(a.category != value_category::integer || b.category != value_category::integer ||
               a.width != b.width || a.unsigned_value != b.unsigned_value) { return false; }
            // Conflicting standard identities cannot be overridden by a key.
            // Matching validated builtin DATA may span independent base DIEs.
            if(a.builtin_identity != narrow_builtin::unknown && b.builtin_identity != narrow_builtin::unknown)
            { return a.builtin_identity == b.builtin_identity; }
            return a.declaration_identity_known && b.declaration_identity_known && a.declaration_identity == b.declaration_identity;
        }
        inline bool valid_numeric(integer const& value, unsigned guest_bits = 0u) noexcept
        {
            if(value.rust_pointer_sized && (value.floating || value.rust_character || value.utf_character_width || value.category == value_category::boolean ||
                (guest_bits && value.width != guest_bits))) { return false; }
            if(value.rust_character && (value.floating || value.category != value_category::integer || !value.unsigned_value ||
                value.width != 32u || !unicode_scalar(value.bits) || value.utf_character_width ||
                value.wide_identity != wide_builtin::unknown || value.builtin_identity != narrow_builtin::unknown || value.declaration_identity_known)) { return false; }
            if(value.utf_character_width && (value.floating || value.category != value_category::integer || !value.unsigned_value ||
                value.width != value.utf_character_width || (value.width != 8u && value.width != 16u && value.width != 32u) ||
                value.wide_identity != wide_builtin::unknown || value.builtin_identity != narrow_builtin::unknown || value.declaration_identity_known)) { return false; }
            return valid_wide_identity(value,guest_bits) && valid_narrow_identity(value) && value.width != 0u && value.width <= 64u && (value.width & (value.width-1u)) == 0u &&
                (!value.floating || (value.width == 32u || value.width == 64u)) &&
                (value.category == value_category::unspecified || value.category == value_category::integer ||
                 value.category == value_category::boolean) &&
                (!value.floating || value.category == value_category::unspecified) &&
                (value.category != value_category::boolean || value.unsigned_value);
        }
        inline integer common_numeric_type(integer a, integer b, language_semantics language = language_semantics::shared_numeric) noexcept
        {
            if(language == language_semantics::rust)
            {
                if(a.width != b.width || a.unsigned_value != b.unsigned_value || a.floating != b.floating || a.rust_pointer_sized != b.rust_pointer_sized) { return {0u,0u,false}; }
                integer out{0u,a.width,a.unsigned_value,a.floating,a.floating ? value_category::unspecified : value_category::integer};out.rust_pointer_sized=a.rust_pointer_sized;return out;
            }
            a = promote(a,language); b = promote(b,language);
            if(native_integer_language(language) && a.wide_identity != wide_builtin::unknown && b.wide_identity != wide_builtin::unknown &&
               a.declaration_identity_known && b.declaration_identity_known && a.declaration_identity == b.declaration_identity &&
               a.wide_identity != b.wide_identity) { return {0u,0u,false}; }
            if(a.floating || b.floating)
            { return {0u,(a.floating && a.width == 64u) || (b.floating && b.width == 64u) ? 64u : 32u,false,true}; }
            integer out{0u,a.width > b.width ? a.width : b.width,
                (a.unsigned_value && a.width >= b.width) || (b.unsigned_value && b.width >= a.width)};
            if(native_integer_language(language) && a.wide_identity != wide_builtin::unknown && b.wide_identity != wide_builtin::unknown)
            {
                auto const ar{dwarf::c_wide_rank(a.wide_identity)},br{dwarf::c_wide_rank(b.wide_identity)};
                if(a.unsigned_value == b.unsigned_value) { out.wide_identity = ar >= br ? a.wide_identity : b.wide_identity; }
                else
                {
                    auto const& u{a.unsigned_value ? a : b};auto const& v{a.unsigned_value ? b : a};
                    out.wide_identity = dwarf::c_wide_rank(u.wide_identity) >= dwarf::c_wide_rank(v.wide_identity) ? u.wide_identity :
                        v.width > u.width ? v.wide_identity : dwarf::c_wide_unsigned_partner(v.wide_identity);
                }
                if(a.declaration_identity_known && b.declaration_identity_known && a.declaration_identity == b.declaration_identity &&
                   a.wide_identity == b.wide_identity && out.wide_identity == a.wide_identity)
                { out.declaration_identity = a.declaration_identity;out.declaration_identity_known = true; }
            }
            return out;
        }
        // C/C23 use arithmetic promotion. C++ same narrow copied results
        // require standard builtin identity or one canonical metadata DIE.
        // Distinct identities carried by explicit builtin casts prove the
        // arithmetic case even when representation is equal.
        inline error conditional_numeric_type(node const& yes, node const& no,
            integer a, integer b, integer& out, language_semantics language) noexcept
        {
            bool const a_bool{yes.boolean_literal || a.category == value_category::boolean};
            bool const b_bool{no.boolean_literal || b.category == value_category::boolean};
            if(language == language_semantics::c || language == language_semantics::c23)
            {
                // Unknown narrow carrier categories cannot become native type
                // evidence, even when a C frame selected these arithmetic rules.
                if((a.width < 32u && !a.floating && a.category == value_category::unspecified) ||
                   (b.width < 32u && !b.floating && b.category == value_category::unspecified))
                { return error::unsupported; }
                out = common_numeric_type(a,b,language); return valid_numeric(out) ? error::none : error::unavailable;
            }
            if(language == language_semantics::cpp && a.utf_character_width && a.utf_character_width == b.utf_character_width)
            { out = a; out.bits = 0u; return error::none; }
            if(a_bool && b_bool)
            {
                if(language == language_semantics::cpp && a.width == 8u && b.width == 8u &&
                   a.category == value_category::boolean && b.category == value_category::boolean)
                { out = {0u,8u,true,false,value_category::boolean}; return error::none; }
                return error::unsupported;
            }
            bool const narrow_pair{!a.floating && !b.floating && a.width < 32u && b.width < 32u};
            bool const literal_narrow{(yes.boolean_literal && !b.floating && b.width < 32u) ||
                                      (no.boolean_literal && !a.floating && a.width < 32u)};
            if(language == language_semantics::cpp && narrow_pair && a.declaration_identity_known && b.declaration_identity_known &&
               a.declaration_identity == b.declaration_identity && a.builtin_identity != narrow_builtin::unknown &&
               b.builtin_identity != narrow_builtin::unknown && a.builtin_identity != b.builtin_identity)
            { return error::unavailable; } // one base DIE cannot prove conflicting standard primitives
            if(language == language_semantics::cpp && narrow_pair && same_narrow_identity(a,b))
            { out = a; out.bits = 0u;
              if(out.builtin_identity == narrow_builtin::unknown) { out.builtin_identity = b.builtin_identity; }
              if(!a.declaration_identity_known || !b.declaration_identity_known || a.declaration_identity != b.declaration_identity)
              { out.declaration_identity = {}; out.declaration_identity_known = false; }
              return error::none; }
            if(narrow_pair || literal_narrow)
            {
                bool const distinct_boolean{a_bool != b_bool &&
                    (a_bool ? b.category : a.category) == value_category::integer};
                bool const distinct_integer{!a_bool && !b_bool &&
                    a.category == value_category::integer && b.category == value_category::integer &&
                    (a.width != b.width || a.unsigned_value != b.unsigned_value || a.utf_character_width != b.utf_character_width ||
                     (language == language_semantics::cpp && a.builtin_identity != narrow_builtin::unknown &&
                      b.builtin_identity != narrow_builtin::unknown && a.builtin_identity != b.builtin_identity))};
                if(!distinct_boolean && !distinct_integer) { return error::unsupported; }
            }
            out = common_numeric_type(a,b,language); return valid_numeric(out) ? error::none : error::unavailable;
        }
        inline constexpr integer predicate_value(bool value, language_semantics language) noexcept
        {
            if(language == language_semantics::cpp || native_boolean_language(language)) { return {value,8u,true,false,value_category::boolean}; }
            integer out{value,32u,false};if(native_integer_language(language)) { out.wide_identity = wide_builtin::signed_int; }return out;
        }
        // Primitive Boolean operators retain truth/category. Rust eager
        // bitwise Boolean operators and integer ! differ from Go/Zig/C.
        // This models copied primitive DATA, not overloaded/user-defined ops.
        inline error boolean_binary(operation op, integer a, integer b, integer& out, language_semantics language) noexcept
        {
            if(!boolean_value(a) || !boolean_value(b)) { return error::unsupported; }
            bool const av{truth(a)},bv{truth(b)}; bool value{};
            switch(op)
            {
                case operation::logical_and: value = av && bv; break;
                case operation::logical_or: value = av || bv; break;
                case operation::equal: value = av == bv; break;
                case operation::unequal: value = av != bv; break;
                case operation::bit_and: case operation::bit_or: case operation::bit_xor:
                case operation::less: case operation::less_equal: case operation::greater: case operation::greater_equal:
                    if(language != language_semantics::rust) { return error::unsupported; }
                    value = op == operation::bit_and ? av && bv : op == operation::bit_or ? av || bv :
                        op == operation::bit_xor ? av != bv : op == operation::less ? av < bv :
                        op == operation::less_equal ? av <= bv : op == operation::greater ? av > bv : av >= bv;
                    break;
                default: return error::unsupported;
            }
            out = predicate_value(value,language); return error::none;
        }
        inline error rust_character_binary(operation op, integer a, integer b, integer& out, language_semantics language) noexcept
        {
            if(language != language_semantics::rust || !a.rust_character || !b.rust_character) { return error::unsupported; }
            bool value{};
            switch(op)
            {
                case operation::equal:value=a.bits==b.bits;break;case operation::unequal:value=a.bits!=b.bits;break;
                case operation::less:value=a.bits<b.bits;break;case operation::less_equal:value=a.bits<=b.bits;break;
                case operation::greater:value=a.bits>b.bits;break;case operation::greater_equal:value=a.bits>=b.bits;break;
                default:return error::unsupported;
            }
            out=predicate_value(value,language);return error::none;
        }
        inline error boolean_not(integer a, integer& out, language_semantics language) noexcept
        {
            if(boolean_value(a)) { out = predicate_value(!truth(a),language); return error::none; }
            if(language != language_semantics::rust || a.rust_character || a.floating || a.category != value_category::integer)
            { return error::unsupported; }
            out = a; out.bits = (~a.bits) & mask(a.width); return error::none;
        }
        inline constexpr bool boolean_cast_allowed(node const& n, integer const& a, language_semantics language) noexcept
        {
            if(n.rust_character)
            { return language == language_semantics::rust && (a.rust_character || (!a.floating &&
                a.category == value_category::integer && a.unsigned_value && a.width == 8u)); }
            if(a.rust_character)
            { return language == language_semantics::rust && !n.floating_literal && !n.boolean_cast && !n.utf_character_width; }
            if(language == language_semantics::zig) { return false; } // Zig uses the bounded @as path.
            if(!native_boolean_language(language)) { return true; }
            if(n.boolean_cast) { return boolean_value(a); }
            if(!boolean_value(a)) { return true; }
            return language == language_semantics::rust && !n.floating_literal; // bool as an integer only.
        }
        inline constexpr bool agrees_with_declared_type(integer const& value, integer const& type) noexcept
        {
            if(value.width != type.width || value.unsigned_value != type.unsigned_value ||
               value.floating != type.floating || value.category != type.category) { return false; }
            if(value.utf_character_width != type.utf_character_width || value.rust_character != type.rust_character || value.rust_pointer_sized != type.rust_pointer_sized) { return false; }
            if(type.wide_identity != wide_builtin::unknown && value.wide_identity != type.wide_identity) { return false; }
            if(type.builtin_identity != narrow_builtin::unknown && value.builtin_identity != type.builtin_identity) { return false; }
            return !type.declaration_identity_known || (value.declaration_identity_known && value.declaration_identity == type.declaration_identity);
        }
        inline bool valid_size_extent(integer const& extent, unsigned guest_bits, language_semantics language) noexcept
        {
            // Both declaration-only and evaluated native sizeof callbacks
            // carry an extent, not the operand's numeric type. Keep the same
            // contract even when the operand occurs only in an unevaluated
            // or short-circuited subtree. Legacy two-argument type callbacks
            // still describe the operand type and do not use this predicate.
            return valid_numeric(extent,guest_bits) && !extent.floating && extent.unsigned_value &&
                extent.width == guest_bits && (!native_integer_language(language) ||
                    (extent.category != value_category::boolean && extent.bits <= mask(guest_bits)));
        }
        inline constexpr integer size_value(::std::uint64_t bytes, unsigned guest_bits, language_semantics language) noexcept
        {
            integer out{bytes,guest_bits,true};
            // The finite native profiles model the standard Wasm C ABI:
            // size_t is unsigned long on both Wasm32 and Wasm64. This is
            // independent of the host ABI and never copies the operand DIE.
            // WALI's different long/size_t ABI is outside these profiles.
            if(native_integer_language(language))
            { out.category = value_category::integer; out.wide_identity = wide_builtin::unsigned_long; }
            return out;
        }
        template<typename TypeResolver>
        error infer_type(program const& code, ::std::size_t index, unsigned depth, TypeResolver& resolve_type,
            integer& out, unsigned guest_bits, ::std::size_t& budget, language_semantics language)
        {
            if(depth >= 32u || index >= code.nodes.size() || budget == 0u) { return error::limit_exceeded; }
            --budget; auto const& n{code.nodes[index]};
            if(!native_syntax(n,language)) { return error::unsupported; }
            if(n.op == operation::literal)
            {
                auto const status{literal_value(n,out,guest_bits,language)}; out.bits = 0u; return status;
            }
            if(n.op == operation::size)
            {
                integer operand_type{};
                // A sizeof operand can be an aggregate or pointer. Query its
                // declared extent separately from its numeric value category.
                // The three-argument callback still has no value/read access.
                if constexpr(requires { resolve_type(n.reference,true,operand_type); })
                {
                    if(!resolve_type(n.reference,true,operand_type) || !valid_size_extent(operand_type,guest_bits,language))
                    { return error::unavailable; }
                }
                else if(!resolve_type(n.reference,operand_type) || !valid_numeric(operand_type,guest_bits)) { return error::unavailable; }
                out = size_value(0u,guest_bits,language); return error::none; // declaration/type only, no sizeof/value read.
            }
            if(n.op == operation::type_size)
            { out = size_value(0u,guest_bits,language); return error::none; }
            if(n.op == operation::reference)
            {
                if constexpr(requires { resolve_type(n.reference,false,out); })
                { if(!resolve_type(n.reference,false,out)) { return error::unavailable; } }
                else if(!resolve_type(n.reference,out)) { return error::unavailable; }
                if(!valid_numeric(out,guest_bits)) { return error::unavailable; }
                out.bits = 0u; return error::none; // type DATA only, never copied value bits.
            }
            if(is_rust_negative_literal(code,n,language))
            { auto const status{rust_negative_literal(code,n,out,guest_bits)};out.bits=0u;return status; }
            integer a{},b{};bool const contextual_lhs{rust_context_binary(n.op,language) &&
                rust_untyped_literal(code,n.lhs) && !rust_untyped_literal(code,n.rhs)};
            auto status{contextual_lhs ? infer_type(code,n.rhs,depth+1u,resolve_type,b,guest_bits,budget,language) :
                infer_type(code,n.lhs,depth+1u,resolve_type,a,guest_bits,budget,language)};
            if(status != error::none) { return status; }
            if(contextual_lhs) { status=rust_literal_as_type(code,n.lhs,b,a);if(status != error::none) { return status; } a.bits=0u; }
            if(n.op == operation::expression_size)
            {
                if(!valid_numeric(a,guest_bits) || (a.width & 7u) != 0u) { return error::unavailable; }
                out = size_value(0u,guest_bits,language); return error::none;
            }
            if(n.op == operation::cast)
            { if(!boolean_cast_allowed(n,a,language)) { return error::unsupported; }
              out = {0u,n.literal_width ? n.literal_width : guest_bits,n.literal_unsigned,n.floating_literal,
                n.boolean_cast ? value_category::boolean : n.floating_literal ? value_category::unspecified : value_category::integer,n.builtin_identity};
              out.utf_character_width = n.utf_character_width; out.rust_character = n.rust_character;out.rust_pointer_sized=language==language_semantics::rust && !n.literal_width;
              if(native_integer_language(language)) { out.wide_identity = n.wide_identity; } return error::none; }
            if(n.op == operation::zig_coerce)
            {
                auto const& child{code.nodes[n.lhs]}; auto source{a};
                // A literal or one negative literal is owned syntax DATA.
                // Check its range even when this branch has no value access.
                if(auto const* constant{zig_coercion_constant(code,child)})
                {
                    status = zig_constant_value(child,*constant,source,guest_bits);
                    if(status != error::none) { return status; }
                }
                status = check_zig_coercion(code,n,child,source,guest_bits);
                if(status != error::none) { return status; }
                out = {0u,n.literal_width ? n.literal_width : guest_bits,n.literal_unsigned,n.floating_literal,
                    n.floating_literal ? value_category::unspecified : value_category::integer};
                return error::none;
            }
            if(native_boolean_language(language) && n.op == operation::logical_not)
            { status = boolean_not(a,out,language); out.bits = 0u; return status; }
            if(native_boolean_language(language) && (n.op == operation::positive || n.op == operation::negative || n.op == operation::invert) &&
               (boolean_value(a) || (n.op == operation::invert && language != language_semantics::zig))) { return error::unsupported; }
            if(a.rust_character && (language != language_semantics::rust || n.op == operation::positive ||
                n.op == operation::negative || n.op == operation::invert)) { return error::unsupported; }
            if(language == language_semantics::rust && n.op == operation::negative && a.unsigned_value) { return error::unsupported; }
            auto const original_a{a};
            a = promote(a,language);
            if(n.op == operation::logical_not) { out = predicate_value(false,language); return error::none; }
            if(n.op == operation::positive || n.op == operation::negative || n.op == operation::invert)
            {
                if(n.op == operation::invert && a.floating) { return error::unsupported; }
                out = a; return error::none;
            }
            if(!contextual_lhs)
            {
                status = rust_context_binary(n.op,language) && rust_untyped_literal(code,n.rhs) && !rust_untyped_literal(code,n.lhs) ?
                    rust_literal_as_type(code,n.rhs,a,b) : infer_type(code,n.rhs,depth+1u,resolve_type,b,guest_bits,budget,language);
                if(status != error::none) { return status; }
                b.bits=0u;
            }
            if(n.op == operation::conditional)
            {
                integer c{}; status = infer_type(code,n.third,depth+1u,resolve_type,c,guest_bits,budget,language);
                if(status != error::none) { return status; }
                return conditional_numeric_type(code.nodes[n.rhs],code.nodes[n.third],b,c,out,language);
            }
            if(original_a.rust_character || b.rust_character)
            { status=rust_character_binary(n.op,original_a,b,out,language);out.bits=0u;return status; }
            if(native_boolean_language(language) && (boolean_value(original_a) || boolean_value(b)))
            { status = boolean_binary(n.op,original_a,b,out,language); out.bits = 0u; return status; }
            if(native_boolean_language(language) && (n.op == operation::logical_and || n.op == operation::logical_or))
            { return error::unsupported; }
            if(!rust_numeric_operands(n.op,a,b,language)) { return error::unsupported; }
            b = promote(b,language);
            switch(n.op)
            {
                case operation::logical_and: case operation::logical_or:
                case operation::less: case operation::less_equal: case operation::greater: case operation::greater_equal:
                case operation::equal: case operation::unequal: out = predicate_value(false,language); return error::none;
                case operation::left: case operation::right:
                    if(a.floating || b.floating) { return error::unsupported; }
                    out = a; return error::none;
                case operation::remainder: case operation::bit_and: case operation::bit_clear: case operation::bit_xor: case operation::bit_or:
                    if(a.floating || b.floating) { return error::unsupported; }
                    [[fallthrough]];
                case operation::add: case operation::subtract: case operation::multiply: case operation::divide:
                    out = common_numeric_type(a,b,language); return valid_numeric(out) ? error::none : error::unavailable;
                default: return error::unsupported;
            }
        }

        // One bounded type variable per syntax node. Constraints copy primitive
        // declaration DATA only; they cannot invoke the value/guest resolver.
        // Cast operands and shift counts have independent type variables.
        struct rust_type_constraints
        {
            struct group { ::std::size_t parent{}; unsigned hint{}; integer type{0u,0u,false}; };
            ::std::array<group,128u> groups{};
            ::std::size_t count{};
            ::std::size_t find(::std::size_t i) noexcept
            {
                auto root{i};while(groups[root].parent != root) { root=groups[root].parent; }
                while(groups[i].parent != i) { auto const next{groups[i].parent};groups[i].parent=root;i=next; }
                return root;
            }
            static bool same(integer const& a, integer const& b) noexcept
            {
                return a.width==b.width && a.unsigned_value==b.unsigned_value && a.floating==b.floating &&
                    boolean_value(a)==boolean_value(b) && a.rust_character==b.rust_character &&
                    a.rust_pointer_sized==b.rust_pointer_sized;
            }
            error bind(::std::size_t i, integer type) noexcept
            {
                i=find(i);type.bits=0u;
                if(groups[i].type.width && !same(groups[i].type,type)) { return error::unsupported; }
                groups[i].type=type;return error::none;
            }
            error join(::std::size_t a, ::std::size_t b) noexcept
            {
                a=find(a);b=find(b);if(a==b) { return error::none; }
                if(groups[a].type.width && groups[b].type.width && !same(groups[a].type,groups[b].type))
                { return error::unsupported; }
                if(!groups[a].type.width) { groups[a].type=groups[b].type; }
                groups[a].hint|=groups[b].hint;groups[b].parent=a;return error::none;
            }
        };
        template<typename TypeResolver>
        error prepare_rust_types(program const& code, program& prepared, integer& root_type, unsigned guest_bits,
            TypeResolver& resolve_type, ::std::size_t& budget)
        {
            if(code.nodes.empty() || code.nodes.size()>128u || code.root>=code.nodes.size()) { return error::limit_exceeded; }
            rust_type_constraints plan{};plan.count=code.nodes.size();
            ::std::array<unsigned,128u> heights{};
            for(::std::size_t i{};i!=plan.count;++i) { plan.groups[i].parent=i; }
            auto const boolean{predicate_value(false,language_semantics::rust)};
            for(::std::size_t i{};i!=plan.count;++i)
            {
                if(!budget) { return error::limit_exceeded; }--budget;
                auto const& n{code.nodes[i]};if(!native_syntax(n,language_semantics::rust)) { return error::unsupported; }
                error status{};
                if(n.op==operation::literal)
                {
                    if(n.rust_numeric_literal)
                    {
                        if(!n.rust_numeric_suffix) { plan.groups[i].hint=n.floating_literal?2u:1u;continue; }
                        integer type{0u,n.guest_long_literal?guest_bits:n.literal_width,n.literal_unsigned,n.floating_literal,
                            n.floating_literal?value_category::unspecified:value_category::integer};
                        type.rust_pointer_sized=n.guest_long_literal;status=plan.bind(i,type);
                    }
                    else { integer type{};status=literal_value(n,type,guest_bits,language_semantics::rust);if(status==error::none) { status=plan.bind(i,type); } }
                }
                else if(n.op==operation::reference)
                {
                    integer type{};
                    if constexpr(requires { resolve_type(n.reference,false,type); })
                    { if(!resolve_type(n.reference,false,type)) { return error::unavailable; } }
                    else if(!resolve_type(n.reference,type)) { return error::unavailable; }
                    if(!valid_numeric(type,guest_bits)) { return error::unavailable; }
                    status=plan.bind(i,type);
                }
                else if(n.op==operation::size || n.op==operation::type_size)
                { status=plan.bind(i,size_value(0u,guest_bits,language_semantics::rust)); }
                else
                {
                    if(n.lhs>=i) { return error::limit_exceeded; }
                    heights[i]=heights[n.lhs]+1u;
                    switch(n.op)
                    {
                        case operation::cast:
                        {
                            integer type{0u,n.literal_width?n.literal_width:guest_bits,n.literal_unsigned,n.floating_literal,
                                n.boolean_cast?value_category::boolean:n.floating_literal?value_category::unspecified:value_category::integer};
                            type.rust_character=n.rust_character;type.rust_pointer_sized=!n.literal_width;
                            status=plan.bind(i,type);break;
                        }
                        case operation::expression_size:status=plan.bind(i,size_value(0u,guest_bits,language_semantics::rust));break;
                        case operation::negative:case operation::logical_not:status=plan.join(i,n.lhs);break;
                        default:
                        {
                            if(n.rhs>=i) { return error::limit_exceeded; }
                            heights[i]=(::std::max)(heights[i],heights[n.rhs]+1u);
                            switch(n.op)
                            {
                                case operation::left:case operation::right:status=plan.join(i,n.lhs);break;
                                case operation::logical_and:case operation::logical_or:
                                    status=plan.bind(i,boolean);
                                    if(status==error::none) { status=plan.bind(n.lhs,boolean); }
                                    if(status==error::none) { status=plan.bind(n.rhs,boolean); }break;
                                case operation::less:case operation::less_equal:case operation::greater:case operation::greater_equal:
                                case operation::equal:case operation::unequal:
                                    status=plan.bind(i,boolean);
                                    if(status==error::none) { status=plan.join(n.lhs,n.rhs); }break;
                                case operation::add:case operation::subtract:case operation::multiply:case operation::divide:
                                case operation::remainder:case operation::bit_and:case operation::bit_xor:case operation::bit_or:
                                    status=plan.join(i,n.lhs);
                                    if(status==error::none) { status=plan.join(i,n.rhs); }break;
                                default:return error::unsupported;
                            }
                        }
                    }
                }
                if(status!=error::none) { return status; }
                if(heights[i]>=32u) { return error::limit_exceeded; }
            }
            for(::std::size_t i{};i!=plan.count;++i)
            {
                auto& g{plan.groups[plan.find(i)]};
                if(g.hint==3u) { return error::unsupported; } // integer literals cannot become floats
                if(!g.type.width)
                { if(!g.hint) { return error::unavailable; }g.type=g.hint==2u?integer{0u,64u,false,true}:integer{0u,32u,false,false,value_category::integer}; }
                if(g.hint && (boolean_value(g.type) || g.type.rust_character || ((g.hint==2u)!=g.type.floating)))
                { return error::unsupported; }
            }
            prepared=code;
            for(::std::size_t i{};i!=plan.count;++i)
            {
                auto& n{prepared.nodes[i]};if(n.op!=operation::literal || !n.rust_numeric_literal || n.rust_numeric_suffix) { continue; }
                auto const& type{plan.groups[plan.find(i)].type};
                if(n.floating_literal)
                {
                    integer value{};auto const status{rust_literal_as_type(code,i,type,value)};
                    if(status!=error::none) { return status; }n.literal=value.bits;
                }
                n.literal_width=type.rust_pointer_sized?0u:type.width;n.guest_long_literal=type.rust_pointer_sized;
                n.literal_unsigned=type.unsigned_value;n.rust_numeric_suffix=true;
            }
            // Complete type/range checking includes dead operands without their
            // arithmetic or value reads. The existing depth and visit caps stay.
            integer checked{};auto const status{infer_type(prepared,prepared.root,0u,resolve_type,checked,guest_bits,budget,language_semantics::rust)};
            if(status!=error::none) { return status; }
            root_type=plan.groups[plan.find(code.root)].type;
            return error::none;
        }

        template<typename Resolver, typename TypeResolver>
        error evaluate(program const& code, ::std::size_t index, unsigned depth, Resolver& resolve, integer& out, unsigned guest_bits, TypeResolver& resolve_type, ::std::size_t& type_budget, language_semantics language)
        {
            if(depth >= 32u || index >= code.nodes.size()) { return error::limit_exceeded; }
            auto const& n{code.nodes[index]};
            if(!native_syntax(n,language)) { return error::unsupported; }
            if(n.op == operation::literal) { return literal_value(n,out,guest_bits,language); }
            if(is_rust_negative_literal(code,n,language)) { return rust_negative_literal(code,n,out,guest_bits); }
            if(n.op == operation::type_size) { out = size_value((n.literal_width ? n.literal_width : guest_bits)/8u,guest_bits,language); return error::none; }
            if(n.op == operation::expression_size)
            {
                // The operand is never evaluated. Reuse bounded declaration
                // type inference; unavailable value/location bits are irrelevant.
                integer operand_type{};
                auto const status{infer_type(code,n.lhs,depth+1u,resolve_type,operand_type,guest_bits,type_budget,language)};
                if(status != error::none) { return status; }
                if(!valid_numeric(operand_type,guest_bits) || (operand_type.width & 7u) != 0u) { return error::unavailable; }
                out = size_value(operand_type.width/8u,guest_bits,language); return error::none;
            }
            if(n.op == operation::reference || n.op == operation::size)
            { if(!resolve(n.reference, n.op == operation::size, out)) { return error::unavailable; }
              if(!valid_numeric(out,guest_bits)) { return error::unavailable; }
              if(n.op == operation::size && native_integer_language(language))
              {
                  if(!valid_size_extent(out,guest_bits,language))
                  { return error::unavailable; }
                  out = size_value(out.bits,guest_bits,language);
              }
              out.bits &= mask(out.width); return error::none; }
            if(n.op == operation::conditional)
            {
                // Type-only metadata queries for both arms happen inside the
                // same stopped transaction. No dead-arm value/sizeof resolver
                // or arithmetic evaluation is allowed to supply its type.
                integer yes_type{},no_type{};
                auto status{infer_type(code,n.rhs,depth+1u,resolve_type,yes_type,guest_bits,type_budget,language)};
                if(status != error::none) { return status; }
                status = infer_type(code,n.third,depth+1u,resolve_type,no_type,guest_bits,type_budget,language);
                if(status != error::none) { return status; }
                integer common{};
                status = conditional_numeric_type(code.nodes[n.rhs],code.nodes[n.third],yes_type,no_type,common,language);
                if(status != error::none) { return status; }
                integer condition{},selected{};
                status = evaluate(code,n.lhs,depth+1u,resolve,condition,guest_bits,resolve_type,type_budget,language);
                if(status != error::none) { return status; }
                status = evaluate(code,truth(condition) ? n.rhs : n.third,depth+1u,resolve,selected,guest_bits,resolve_type,type_budget,language);
                if(status != error::none) { return status; }
                auto const& selected_type{truth(condition) ? yes_type : no_type};
                if(selected.width != selected_type.width || selected.unsigned_value != selected_type.unsigned_value ||
                   selected.floating != selected_type.floating || selected.category != selected_type.category) { return error::unavailable; }
                if(selected_type.wide_identity != wide_builtin::unknown &&
                   (selected.wide_identity != selected_type.wide_identity ||
                    (selected_type.declaration_identity_known && (!selected.declaration_identity_known || selected.declaration_identity != selected_type.declaration_identity))))
                { return error::unavailable; }
                if(common.utf_character_width)
                {
                    if(!agrees_with_declared_type(selected,selected_type)) { return error::unavailable; }
                    out = common;out.bits = selected.bits & mask(common.width);return error::none;
                }
                if(common.category == value_category::boolean)
                { out = predicate_value(truth(selected),language); return error::none; }
                if(language == language_semantics::cpp && common.width < 32u && common.category == value_category::integer)
                {
                    if(!same_narrow_identity(selected,selected_type) || (selected_type.declaration_identity_known &&
                        (!selected.declaration_identity_known || selected.declaration_identity != selected_type.declaration_identity)))
                    { return error::unavailable; }
                    out = common; out.bits = selected.bits & mask(common.width); return error::none;
                }
                selected = promote(selected,language);
                if(common.floating)
                { out = selected.floating && selected.width == common.width ? selected : real_value(floating_number(selected),common.width); }
                else
                { out = common; out.bits = (selected.unsigned_value ? selected.bits : static_cast<::std::uint64_t>(signed_bits(selected))) & mask(common.width); }
                return error::none;
            }
            integer source_type{}; bool source_type_checked{}; node const* constant{};
            if(n.op == operation::zig_coerce)
            {
                if(n.lhs >= code.nodes.size()) { return error::limit_exceeded; }
                auto const& child{code.nodes[n.lhs]};
                if(!zig_coercion_source_supported(code,child)) { return error::unsupported; }
                constant = zig_coercion_constant(code,child);
                if(type_resolver_supplied(resolve_type) && constant == nullptr)
                {
                    // The real stopped-frame callers supply declaration DATA.
                    // Reject unsafe/unknown source types before any value read.
                    auto status{infer_type(code,n.lhs,depth+1u,resolve_type,source_type,guest_bits,type_budget,language)};
                    if(status != error::none) { return status; }
                    status = check_zig_coercion(code,n,child,source_type,guest_bits);
                    if(status != error::none) { return status; }
                    source_type_checked = true;
                }
            }
            // Native logical operands must be well-typed even when a value
            // will be skipped. These callbacks expose declaration DATA only;
            // they cannot read a dead operand or execute its arithmetic.
            bool const native_logical{(native_integer_language(language) || native_boolean_language(language)) &&
                (n.op == operation::logical_and || n.op == operation::logical_or)};
            integer logical_lhs_type{}, logical_rhs_type{};
            if(native_logical)
            {
                auto status{infer_type(code,n.lhs,depth+1u,resolve_type,logical_lhs_type,guest_bits,type_budget,language)};
                if(status != error::none) { return status; }
                status = infer_type(code,n.rhs,depth+1u,resolve_type,logical_rhs_type,guest_bits,type_budget,language);
                if(status != error::none) { return status; }
                if(native_boolean_language(language) && (!boolean_value(logical_lhs_type) || !boolean_value(logical_rhs_type)))
                { return error::unsupported; }
            }
            integer a{},b{};
            bool const contextual_lhs{rust_context_binary(n.op,language) && rust_untyped_literal(code,n.lhs) && !rust_untyped_literal(code,n.rhs)};
            integer contextual_type{};
            if(contextual_lhs)
            {
                auto const status{infer_type(code,n.rhs,depth+1u,resolve_type,contextual_type,guest_bits,type_budget,language)};
                if(status != error::none) { return status; }
            }
            auto status{contextual_lhs ? rust_literal_as_type(code,n.lhs,contextual_type,a) :
                constant ? zig_constant_value(code.nodes[n.lhs],*constant,a,guest_bits) :
                evaluate(code,n.lhs,depth+1u,resolve,a,guest_bits,resolve_type,type_budget,language)};
            if(status != error::none) { return status; }
            if(native_logical && !agrees_with_declared_type(a,logical_lhs_type)) { return error::unavailable; }
            if(n.op == operation::zig_coerce)
            {
                auto const& child{code.nodes[n.lhs]};
                // A copied value cannot change the declaration used to prove
                // this conversion, even when both types fit the destination.
                if(source_type_checked && (a.width != source_type.width ||
                    a.unsigned_value != source_type.unsigned_value || a.floating != source_type.floating || a.category != source_type.category))
                { return error::unavailable; }
                status = check_zig_coercion(code,n,child,a,guest_bits);
                if(status != error::none) { return status; }
                auto const width{n.literal_width ? n.literal_width : guest_bits};
                if(n.floating_literal) { out = real_value(floating_number(a),width); return error::none; }
                out = {(a.unsigned_value ? a.bits : static_cast<::std::uint64_t>(signed_bits(a))) & mask(width),width,n.literal_unsigned,false,value_category::integer};
                return error::none;
            }
            if(n.op == operation::cast)
            {
                if(!boolean_cast_allowed(n,a,language)) { return error::unsupported; }
                // A copied Boolean carries truth, not its storage magnitude.
                // Normalize before every explicit numeric conversion, just as
                // promotion does; genuine/legacy integers retain their value.
                if(a.category == value_category::boolean) { a = promote(a,language); }
                auto const width{n.literal_width ? n.literal_width : guest_bits};
                if(n.boolean_cast) { out = {truth(a),8u,true,false,value_category::boolean}; return error::none; }
                if(n.floating_literal) { out = real_value(floating_number(a),width); return error::none; }
                if(a.floating)
                {
                    auto const number{::std::trunc(floating_number(a))};
                    auto const upper{::std::ldexp(1.0,static_cast<int>(width-(n.literal_unsigned ? 0u : 1u)))};
                    if(!::std::isfinite(number) || number >= upper || number < (n.literal_unsigned ? 0.0 : -upper)) { return error::arithmetic; }
                    out = {n.literal_unsigned ? static_cast<::std::uint64_t>(number) : static_cast<::std::uint64_t>(static_cast<::std::int64_t>(number)),width,n.literal_unsigned,false,value_category::integer};
                }
                else { out = {(a.unsigned_value ? a.bits : static_cast<::std::uint64_t>(signed_bits(a))) & mask(width),width,n.literal_unsigned,false,value_category::integer}; }
                out.builtin_identity = n.builtin_identity; out.utf_character_width = n.utf_character_width; out.rust_character = n.rust_character;out.rust_pointer_sized=language==language_semantics::rust && !n.literal_width; if(native_integer_language(language)) { out.wide_identity = n.wide_identity; } out.bits &= mask(width); return error::none;
            }
            if(native_boolean_language(language) && n.op == operation::logical_not) { return boolean_not(a,out,language); }
            if(native_boolean_language(language) && (n.op == operation::positive || n.op == operation::negative || n.op == operation::invert) &&
               (boolean_value(a) || (n.op == operation::invert && language != language_semantics::zig))) { return error::unsupported; }
            if(a.rust_character && (language != language_semantics::rust || n.op == operation::positive ||
                n.op == operation::negative || n.op == operation::invert)) { return error::unsupported; }
            if(language == language_semantics::rust && n.op == operation::negative && a.unsigned_value) { return error::unsupported; }
            auto const original_a{a};
            a = promote(a,language);
            if(n.op == operation::positive) { out = a; return error::none; }
            if(n.op == operation::logical_not) { out = predicate_value(!truth(a),language); return error::none; }
            if(n.op == operation::negative || n.op == operation::invert)
            {
                if(a.floating)
                { if(n.op != operation::negative) { return error::unsupported; } out = real_value(-floating_number(a),a.width); return error::none; }
                if(n.op == operation::negative && !a.unsigned_value && a.bits == (::std::uint64_t{1u} << (a.width-1u))) { return error::arithmetic; }
                a.bits = (n.op == operation::negative ? 0u-a.bits : ~a.bits) & mask(a.width); out = a; return error::none;
            }
            // Short circuit before invoking the guest resolver for the right side.
            if((n.op == operation::logical_and && !truth(a)) || (n.op == operation::logical_or && truth(a)))
            { out = predicate_value(truth(a),language); return error::none; }
            status = rust_context_binary(n.op,language) && rust_untyped_literal(code,n.rhs) && !rust_untyped_literal(code,n.lhs) ?
                rust_literal_as_type(code,n.rhs,a,b) : evaluate(code,n.rhs,depth+1u,resolve,b,guest_bits,resolve_type,type_budget,language);
            if(status != error::none) { return status; }
            if(contextual_lhs && !agrees_with_declared_type(b,contextual_type)) { return error::unavailable; }
            if(native_logical && !agrees_with_declared_type(b,logical_rhs_type)) { return error::unavailable; }
            if(original_a.rust_character || b.rust_character)
            { return rust_character_binary(n.op,original_a,b,out,language); }
            if(native_boolean_language(language) && (boolean_value(original_a) || boolean_value(b)))
            { return boolean_binary(n.op,original_a,b,out,language); }
            if(!rust_numeric_operands(n.op,a,b,language)) { return error::unsupported; }
            b = promote(b,language);
            if(n.op == operation::logical_and || n.op == operation::logical_or) { out = predicate_value(truth(b),language); return error::none; }
            if(a.floating || b.floating)
            {
                auto const width{(a.floating && a.width == 64u) || (b.floating && b.width == 64u) ? 64u : 32u};
                auto const av{width == 32u ? static_cast<double>(static_cast<float>(floating_number(a))) : floating_number(a)};
                auto const bv{width == 32u ? static_cast<double>(static_cast<float>(floating_number(b))) : floating_number(b)};
                bool comparison{}; double number{}; bool predicate{};
                switch(n.op)
                {
                    case operation::less: comparison = true; predicate = av < bv; break;
                    case operation::less_equal: comparison = true; predicate = av <= bv; break;
                    case operation::greater: comparison = true; predicate = av > bv; break;
                    case operation::greater_equal: comparison = true; predicate = av >= bv; break;
                    case operation::equal: comparison = true; predicate = av == bv; break;
                    case operation::unequal: comparison = true; predicate = av != bv; break;
                    case operation::add: number = width == 32u ? static_cast<float>(av)+static_cast<float>(bv) : av+bv; break;
                    case operation::subtract: number = width == 32u ? static_cast<float>(av)-static_cast<float>(bv) : av-bv; break;
                    case operation::multiply: number = width == 32u ? static_cast<float>(av)*static_cast<float>(bv) : av*bv; break;
                    case operation::divide: number = width == 32u ? static_cast<float>(av)/static_cast<float>(bv) : av/bv; break;
                    default: return error::unsupported;
                }
                out = comparison ? predicate_value(predicate,language) : real_value(number,width); return error::none;
            }
            if(n.op == operation::left || n.op == operation::right)
            {
                if((!b.unsigned_value && signed_bits(b) < 0) || b.bits >= a.width) { return error::arithmetic; }
                auto const count{static_cast<unsigned>(b.bits)};
                if(n.op == operation::left)
                {
                    // Rust shifts truncate the copied two's-complement bit pattern.
                    // Only the count can overflow; keep the existing C profile.
                    if(language != language_semantics::rust && !a.unsigned_value &&
                       (signed_bits(a) < 0 || a.bits > (mask(a.width)>>1u)>>count)) { return error::arithmetic; }
                    a.bits = (a.bits << count) & mask(a.width);
                }
                else if(language == language_semantics::rust)
                {
                    // Explicit guest-width sign fill, independent of the host's
                    // signed right-shift representation and target endianness.
                    bool const negative{!a.unsigned_value && (a.bits & (::std::uint64_t{1u} << (a.width-1u))) != 0u};
                    a.bits >>= count;
                    if(negative && count != 0u) { a.bits |= mask(a.width) ^ mask(a.width-count); }
                }
                else { a.bits = a.unsigned_value ? a.bits >> count : static_cast<::std::uint64_t>(signed_bits(a) >> count) & mask(a.width); }
                out = a; return error::none;
            }
            auto const common{common_numeric_type(a,b,language)};
            if(!valid_numeric(common,guest_bits)) { return error::unavailable; }
            auto const width{common.width}; bool const uns{common.unsigned_value};
            auto const av{(a.unsigned_value ? a.bits : static_cast<::std::uint64_t>(signed_bits(a))) & mask(width)};
            auto const bv{(b.unsigned_value ? b.bits : static_cast<::std::uint64_t>(signed_bits(b))) & mask(width)};
            auto const as{signed_bits({av,width,false})}; auto const bs{signed_bits({bv,width,false})};
            bool comparison{}; ::std::uint64_t result{};
            switch(n.op)
            {
                case operation::less: comparison = true; result = uns ? av<bv : as<bs; break;
                case operation::less_equal: comparison = true; result = uns ? av<=bv : as<=bs; break;
                case operation::greater: comparison = true; result = uns ? av>bv : as>bs; break;
                case operation::greater_equal: comparison = true; result = uns ? av>=bv : as>=bs; break;
                case operation::equal: comparison = true; result = av==bv; break;
                case operation::unequal: comparison = true; result = av!=bv; break;
                case operation::bit_and: result = av&bv; break;
                case operation::bit_clear: result = av&~bv; break;
                case operation::bit_xor: result = av^bv; break;
                case operation::bit_or: result = av|bv; break;
                case operation::add: case operation::subtract: case operation::multiply:
                {
                    // Unsigned operations are defined modulo width. Signed
                    // intermediates use a wider scalar, never overflowing C++.
                    if(uns)
                    {
                        if(language == language_semantics::rust &&
                           (n.op == operation::add ? av > mask(width)-bv :
                            n.op == operation::subtract ? av < bv : bv != 0u && av > mask(width)/bv)) { return error::arithmetic; }
                        result = n.op == operation::add ? av+bv : n.op == operation::subtract ? av-bv : av*bv;
                    }
                    else
                    {
                        auto const minimum{signed_bits({::std::uint64_t{1u}<<(width-1u),width,false})}; auto const maximum{-(minimum+1)};
                        if(n.op == operation::add)
                        { if((bs > 0 && as > maximum-bs) || (bs < 0 && as < minimum-bs)) { return error::arithmetic; } result = static_cast<::std::uint64_t>(as+bs); }
                        else if(n.op == operation::subtract)
                        { if((bs < 0 && as > maximum+bs) || (bs > 0 && as < minimum+bs)) { return error::arithmetic; } result = static_cast<::std::uint64_t>(as-bs); }
                        else
                        {
                            if(as > 0 ? (bs > 0 ? as > maximum/bs : bs < minimum/as) :
                               as < 0 ? (bs > 0 ? as < minimum/bs : bs < 0 && as < maximum/bs) : false) { return error::arithmetic; }
                            result = static_cast<::std::uint64_t>(as*bs);
                        }
                    }
                    break;
                }
                case operation::divide: case operation::remainder:
                    if(bv == 0u || (!uns && as == signed_bits({::std::uint64_t{1u}<<(width-1u),width,false}) && bs == -1)) { return error::arithmetic; }
                    result = uns ? (n.op == operation::divide ? av/bv : av%bv) : static_cast<::std::uint64_t>(n.op == operation::divide ? as/bs : as%bs); break;
                default: return error::unsupported;
            }
            out = comparison ? predicate_value(result != 0u,language) : common; if(!comparison) { out.bits = result & mask(width); } return error::none;
        }
    }
    inline error parse(::std::string_view text, program& out, language_semantics language = language_semantics::shared_numeric) noexcept
    {
        out = {}; if(text.empty()) { return error::malformed; } if(text.size() > 4096u) { return error::limit_exceeded; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try {
#endif
            details::parser p{text}; p.language = language; p.pending.root = p.expression(1u,0u); p.space();
            if(p.status != error::none) { return p.status; } if(p.cursor != text.size()) { return error::unsupported; }
            p.pending.original = ::fast_io::concat_fast_io(text); p.pending.syntax_language = language;
            out = ::std::move(p.pending); return error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        } catch(...) { return error::allocation_failure; }
#endif
    }
    // Syntax union only. Evaluation reparses with the selected original CU;
    // admitting a Rust escape never selects a frame, language or guest read.
    inline error parse_admitted(::std::string_view text, program& out) noexcept
    {
        auto const status{parse(text,out,language_semantics::cpp)};
        if(status == error::none || status == error::limit_exceeded || status == error::allocation_failure) { return status; }
        return parse(text,out,language_semantics::rust);
    }
    template<typename Resolver, typename TypeResolver = details::no_type_resolver>
    inline error evaluate(program const& code, Resolver&& resolve, integer& out, unsigned guest_address_bits = 32u,
        TypeResolver&& resolve_type = {}, language_semantics language = language_semantics::shared_numeric) noexcept
    {
        out = {}; if(code.nodes.empty() || code.nodes.size() > 128u) { return error::limit_exceeded; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try {
#endif
            if(guest_address_bits != 32u && guest_address_bits != 64u) { return error::unavailable; }
            if(language != language_semantics::shared_numeric && language != language_semantics::c && language != language_semantics::cpp && language != language_semantics::c23 && !details::native_boolean_language(language))
            { return error::unavailable; }
            program reparsed{}; program const* active{::std::addressof(code)};
            if(details::native_boolean_language(language) && code.syntax_language != language)
            {
                if(code.original.empty()) { return error::unavailable; }
                auto const syntax{parse({code.original.data(),code.original.size()},reparsed,language)};
                if(syntax != error::none) { return syntax; }
                active = ::std::addressof(reparsed);
            }
            if(language != language_semantics::cpp)
            { for(auto const& n : active->nodes) { if(n.utf_character_width) { return error::unsupported; } } }
            if(language != language_semantics::rust)
            { for(auto const& n : active->nodes) { if(n.rust_character || n.rust_numeric_literal) { return error::unsupported; } } }
            ::std::size_t type_budget{4096u};
            program rust_prepared{};integer rust_result_type{};bool rust_prepared_active{};
            if(language==language_semantics::rust)
            {
                bool legacy_reference{};
                if(!details::type_resolver_supplied(resolve_type))
                { for(auto const& n:active->nodes) { if(n.op==operation::reference) { legacy_reference=true;break; } } }
                if(!legacy_reference)
                {
                    auto const status{details::prepare_rust_types(*active,rust_prepared,rust_result_type,guest_address_bits,resolve_type,type_budget)};
                    if(status!=error::none) { return status; }active=::std::addressof(rust_prepared);rust_prepared_active=true;
                }
            }
            integer value{}; auto const status{details::evaluate(*active,active->root,0u,resolve,value,guest_address_bits,resolve_type,type_budget,language)};
            if(status==error::none && rust_prepared_active && !details::rust_type_constraints::same(value,rust_result_type)) { return error::unavailable; }
            if(status == error::none)
            {
                if(language == language_semantics::shared_numeric && value.wide_identity != wide_builtin::unknown)
                { value.wide_identity = {};value.declaration_identity = {};value.declaration_identity_known = false; }
                out = value;
            }
            return status;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        } catch(...) { return error::allocation_failure; }
#endif
    }
    // Actual controller callers provide immutable type DATA selected from the
    // same authenticated frame. This helper grants no location or read access.
    inline bool attach_narrow_type(dwarf::type_record const& type, integer& value) noexcept
    {
        if(!details::valid_numeric(value) || value.category != value_category::integer || value.floating ||
           value.width >= 32u || type.kind != dwarf::type_kind::scalar || type.atomic_scalar ||
           (type.display_qualifiers & 8u) != 0u || type.declaration_identity.offset == 0u || type.byte_count*8u != value.width ||
           (value.unsigned_value ? type.encoding != 0x07u && type.encoding != 0x08u : type.encoding != 0x05u && type.encoding != 0x06u))
        { return false; }
        value.declaration_identity = type.declaration_identity; value.declaration_identity_known = true;
        value.builtin_identity = dwarf::cxx_language(type.language) && !type.tinygo_producer && !type.zig_producer &&
            type.identity.unit == type.declaration_identity.unit && dwarf::cxx_narrow_encoding(type.narrow_builtin,type.encoding,type.byte_count) ?
            type.narrow_builtin : narrow_builtin::unknown;
        return true;
    }
    inline bool attach_standard_integer_type(dwarf::type_record const& type, integer& value, unsigned guest_bits) noexcept
    {
        if(guest_bits != 32u && guest_bits != 64u) { return false; }
        if(type.language == 0x1cu && !type.tinygo_producer && !type.zig_producer && type.encoding == 0x10u)
        {
            if(!details::valid_numeric(value,guest_bits) || value.floating || value.category != value_category::integer ||
                !value.unsigned_value || value.width != 32u || !details::unicode_scalar(value.bits) ||
                type.kind != dwarf::type_kind::scalar || type.atomic_scalar || (type.display_qualifiers & 8u) != 0u ||
                type.declaration_identity.offset == 0u || type.identity.unit != type.declaration_identity.unit || type.byte_count != 4u)
            { value={0u,0u,false};return false; }
            value.rust_character=true;return true;
        }
        if(type.language==0x1cu && !type.tinygo_producer && !type.zig_producer &&
           (type.name=="isize" || type.name=="usize"))
        {
            if(!details::valid_numeric(value,guest_bits) || value.floating || value.category!=value_category::integer ||
               value.width!=guest_bits || value.unsigned_value!=(type.name=="usize") || type.kind!=dwarf::type_kind::scalar ||
               type.atomic_scalar || (type.display_qualifiers&8u)!=0u || type.declaration_identity.offset==0u ||
               type.identity.unit!=type.declaration_identity.unit || type.byte_count*8u!=guest_bits ||
               type.encoding!=(value.unsigned_value?0x07u:0x05u)) { value={0u,0u,false};return false; }
            value.rust_pointer_sized=true;return true;
        }
        if(attach_narrow_type(type,value)) { return true; }
        if(!details::valid_numeric(value,guest_bits) || value.floating || value.category != value_category::integer || value.width < 32u ||
           type.kind != dwarf::type_kind::scalar || type.atomic_scalar || (type.display_qualifiers & 8u) != 0u ||
           type.declaration_identity.offset == 0u || type.identity.unit != type.declaration_identity.unit || type.byte_count*8u != value.width ||
           !dwarf::c_integer_language(type.language) || type.tinygo_producer || type.zig_producer || type.wide_builtin == wide_builtin::unknown ||
           !dwarf::c_wide_extent(type.wide_builtin,type.byte_count,guest_bits/8u) || value.unsigned_value != dwarf::c_wide_unsigned(type.wide_builtin) ||
           type.encoding != (value.unsigned_value ? 0x07u : 0x05u)) { return false; }
        value.wide_identity = type.wide_builtin; value.declaration_identity = type.declaration_identity; value.declaration_identity_known = true;
        return true;
    }
    // Canonical primitive spelling of a copied result. This is not full
    // glvalue/CV/typedef/overload reconstruction and creates no write access.
    inline ::std::string_view copied_type_name(integer const& value) noexcept
    {
        if(!details::valid_numeric(value)) { return {}; }
        if(value.rust_character) { return "Rust char"; }
        if(value.utf_character_width) { return value.width == 8u ? "char8_t" : value.width == 16u ? "char16_t" : "char32_t"; }
        if(value.category == value_category::boolean) { return "bool"; }
        if(value.floating) { return value.width == 32u ? "float" : "double"; }
        switch(value.wide_identity)
        {
            case wide_builtin::signed_int: return "int";
            case wide_builtin::unsigned_int: return "unsigned int";
            case wide_builtin::signed_long: return "long";
            case wide_builtin::unsigned_long: return "unsigned long";
            case wide_builtin::signed_long_long: return "long long";
            case wide_builtin::unsigned_long_long: return "unsigned long long";
            default: break;
        }
        switch(value.builtin_identity)
        {
            case narrow_builtin::plain_char: return "char";
            case narrow_builtin::signed_char: return "signed char";
            case narrow_builtin::unsigned_char: return "unsigned char";
            case narrow_builtin::signed_short: return "short";
            case narrow_builtin::unsigned_short: return "unsigned short";
            default: return value.unsigned_value ? "unsigned integer" : "signed integer";
        }
    }

}

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/array.h>
# include <fast_io_dsal/string.h>
# include <fast_io_dsal/string_view.h>
# include <algorithm>
# include <cstddef>
# include <cstdint>
# include "console_line_editor.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::console_completion
{
    using view = ::fast_io::string_view;
    enum class kind { none, path, symbol, function };
    struct request
    {
        kind type{};
        ::std::size_t begin{}, end{}, cursor{};
        view prefix{}, root{}, selectors{};
        ::std::uint64_t module{}; bool indirect{}, explicit_frame{};
    };
    [[nodiscard]] inline bool identifier(char c) noexcept
    { auto const u{static_cast<unsigned char>(c)}; return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') ||
        (u >= '0' && u <= '9') || u == '_' || u == '$' || u >= 128u; }
    [[nodiscard]] inline request context(view line, ::std::size_t cursor) noexcept
    {
        if(cursor > line.size()) { return {}; }
        ::std::size_t p{};
        auto word{[&]() noexcept
        { while(p < line.size() && line[p] == ' ') { ++p; } auto const b{p};
          while(p < line.size() && line[p] != ' ') { ++p; } return line.subview(b, p - b); }};
        auto command{word()};
        if(command == "condition")
        {
            auto const token{word()};::std::uint64_t id{};
            if(token.empty()) { return {}; }
            auto const scanned{::fast_io::parse_by_scan(token.cbegin(),token.cend(),::fast_io::mnp::dec_get<true,true>(id))};
            if(scanned.code!=::fast_io::parse_code::ok || scanned.iter!=token.cend() || id==0u) { return {}; }
            command="display"; // same actual current-frame expression completion
        }
        else if(command=="break-source" || command=="break" || command=="b")
        {
            ::std::size_t condition_begin{};
            for(auto i{p};i<=cursor && i<line.size();++i)
            {
                if(line.size()-i>=4u && line.subview(i,4u)==" if " && cursor>=i+4u)
                { condition_begin=i+4u; }
            }
            if(condition_begin!=0u) { p=condition_begin;command="display"; }
        }
        if(command == "until" || command == "advance" || command == "u" || command == "adv")
        {
            auto path_begin{p};
            while(path_begin < line.size() && line[path_begin] == ' ') { ++path_begin; }
            auto const thread{word()}; ::std::uint64_t id{};
            if(!thread.empty())
            {
                auto const scan{::fast_io::parse_by_scan(thread.cbegin(),thread.cend(),::fast_io::mnp::dec_get<true,true>(id))};
                if(scan.code == ::fast_io::parse_code::ok && scan.iter == thread.cend())
                {
                    auto const stop{word()}; ::std::uint64_t stop_id{};
                    if(stop.empty()) { return {}; }
                    auto const checked{::fast_io::parse_by_scan(stop.cbegin(),stop.cend(),::fast_io::mnp::dec_get<true,true>(stop_id))};
                    if(id == 0u || stop_id == 0u || checked.code != ::fast_io::parse_code::ok || checked.iter != stop.cend()) { return {}; }
                    path_begin = p; while(path_begin < line.size() && line[path_begin] == ' ') { ++path_begin; }
                }
            }
            auto end{line.size()}; auto const colon{line.rfind_character(':')};
            if(colon != ::fast_io::containers::npos && colon >= path_begin) { end = colon; }
            if(colon == ::fast_io::containers::npos && path_begin < line.size())
            {
                auto const target{line.subview(path_begin)}; ::std::uint64_t line_number{};
                auto const scanned{::fast_io::parse_by_scan(target.cbegin(),target.cend(),::fast_io::mnp::dec_get<true,true>(line_number))};
                if(scanned.code == ::fast_io::parse_code::ok && scanned.iter == target.cend()) { return {}; }
            }
            if(cursor < path_begin || cursor > end) { return {}; }
            return {kind::path,path_begin,end,cursor,line.subview(path_begin,cursor-path_begin),{},{}};
        }
        if(command == "frame") { if(word() != "variable") { return {}; } command="print"; }
        if(command == "replace" || command == "tui" || command == "break-source")
        {
            if(command == "tui") { if(word() != "source") { return {}; } }
            else if(command == "break-source")
            {
                auto const token{word()}; ::std::uint64_t id{};
                if(token.empty()) { return {}; }
                auto const scanned{::fast_io::parse_by_scan(token.cbegin(),token.cend(),::fast_io::mnp::dec_get<true,true>(id))};
                if(scanned.code != ::fast_io::parse_code::ok || scanned.iter != token.cend()) { return {}; }
            }
            else
            {
                for(unsigned i{}; i != 3u; ++i)
                { auto const token{word()}; if(token.empty()) { return {}; }
                  for(char c : token) { if(c < '0' || c > '9') { return {}; } } }
            }
            if(p == line.size() || line[p] != ' ') { return {}; }
            ++p; if(cursor < p) { return {}; }
            auto end{line.size()};
            if(command == "break-source")
            {
                auto const colon{line.rfind_character(':')};
                if(colon != ::fast_io::containers::npos && colon >= p)
                { if(cursor > colon) { return {}; } end=colon; }
            }
            return {kind::path, p, end, cursor, line.subview(p, cursor - p), {}, {}};
        }
        if(command == "break" || command == "b" || command == "break-name")
        {
            auto const module{word()}; ::std::uint64_t id{};
            if(module.empty()) { return {}; }
            auto const parsed{::fast_io::parse_by_scan(module.cbegin(),module.cend(),::fast_io::mnp::dec_get<true,true>(id))};
            if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != module.cend()) { return {}; }
            if(p >= line.size() || line[p] != ' ') { return {}; }
            ++p; if(cursor < p) { return {}; }
            return {kind::function,p,line.size(),cursor,line.subview(p,cursor-p),{},{},id};
        }
        bool const display{command == "display"};
        bool const explicit_frame{command == "print-frame" || command == "ptype-frame"};
        if(!display && !explicit_frame && command != "print" && command != "p" && command != "ptype" && command != "source-value" && command != "source-type")
        { return {}; }
        auto expression{p}; while(expression < cursor && line[expression] == ' ') { ++expression; }
        // Preserve explicit THREAD STOP [FRAME] selectors in the actual query.
        auto const selector_begin{expression}; unsigned numeric{};
        while(!display && numeric < 3u)
        {
            auto end{expression}; while(end < line.size() && line[end] >= '0' && line[end] <= '9') { ++end; }
            if(end == expression || end >= line.size() || line[end] != ' ') { break; }
            ++numeric; expression = end; while(expression < line.size() && line[expression] == ' ') { ++expression; }
        }
        if(numeric == 1u) { expression = selector_begin; numeric = 0u; }
        if(explicit_frame && numeric != 3u) { return {}; }
        if(cursor < expression) { return {}; }
        auto begin{cursor}, end{cursor};
        while(begin > expression && identifier(line[begin - 1u])) { --begin; }
        while(end < line.size() && identifier(line[end])) { ++end; }
        view root{};
        if(begin > expression && line[begin - 1u] == '.') { root = line.subview(expression, begin - expression - 1u); }
        else if(begin >= expression + 2u && line[begin - 2u] == '-' && line[begin - 1u] == '>')
        { root = line.subview(expression, begin - expression - 2u); }
        return {kind::symbol, begin, end, cursor, line.subview(begin, cursor - begin), root,
            numeric >= 2u ? line.subview(selector_begin, expression - selector_begin) : view{},0u,
            begin >= expression + 2u && line[begin - 2u] == '-' && line[begin - 1u] == '>',explicit_frame};
    }
    inline constexpr ::std::size_t maximum_candidates{128u}, maximum_candidate_bytes{65536u};
    struct candidates
    {
        ::fast_io::array<::fast_io::string, maximum_candidates> words{};
        ::std::size_t size{}, bytes{};
        bool truncated{}, unavailable{};
        [[nodiscard]] bool add(view value, view prefix)
        {
            if(!value.starts_with(prefix) || value.empty() || value.size() > console_editing::maximum_bytes) { return true; }
            if(!console_editing::safe_completion_text(value)) { return true; }
            for(::std::size_t i{}; i < size; ++i) { if(words[i] == value) { return true; } }
            if(size == maximum_candidates || value.size() > maximum_candidate_bytes - bytes)
            { truncated = true; return false; }
            words[size++] = ::fast_io::concat_fast_io(value); bytes += value.size(); return true;
        }
        void sort() { ::std::sort(words.begin(), words.begin() + size); }
        [[nodiscard]] console_editing::action apply(console_editing::editor& editor, request const& query)
        {
            if(size == 0u) { return console_editing::action::unchanged; }
            sort(); auto common{words[0].size()};
            for(::std::size_t i{1u}; i < size; ++i)
            { ::std::size_t n{}; while(n < common && n < words[i].size() && words[0][n] == words[i][n]) { ++n; } common = n; }
            // A capped directory/query is not a complete uniqueness proof.
            if(!truncated && common > query.prefix.size())
            {
                // Do not split a UTF-8 character at an ambiguous common prefix.
                if(common < words[0].size())
                { while(common > query.prefix.size() && (static_cast<unsigned char>(words[0][common]) & 0xc0u) == 0x80u) { --common; } }
                auto const changed{editor.replace_completion(query.begin, query.end, words[0].subview(0u, common))};
                if(changed == console_editing::action::changed) { return changed; }
            }
            return console_editing::action::candidates;
        }
    };
    inline void paths(request const& query, candidates& output)
    {
        if(query.type != kind::path) { return; }
        for(unsigned char c : query.prefix) { if(c < 32u || c == 127u) { output.unavailable = true; return; } }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
            auto const slash{query.prefix.rfind_character('/')};
            auto const parent{slash == ::fast_io::containers::npos ? view{} : query.prefix.subview(0u, slash + 1u)};
            auto const leaf{query.prefix.subview(parent.size())};
            auto const directory{::fast_io::concat_fast_io(parent.empty() ? view{"."} : parent)};
            ::fast_io::native_file opened{directory, ::fast_io::open_mode::in | ::fast_io::open_mode::directory};
            auto entries{::fast_io::current(::fast_io::at(opened))}; ::std::size_t visited{};
            for(auto entry : entries)
            {
                if(++visited > 4096u) { output.truncated = true; break; }
                auto const name{::fast_io::concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::u8filename(entry)))};
                if(name == "." || name == ".." || (!leaf.starts_with(".") && name.starts_with(".")) || !name.starts_with(leaf)) { continue; }
                auto full{::fast_io::concat_fast_io(parent, name,
                    ::fast_io::type(entry) == ::fast_io::file_type::directory ? view{"/"} : view{})};
                if(!output.add(full.subview(0u), query.prefix)) { break; }
            }
        }
        catch(...) { output.unavailable = true; }
#else
        output.unavailable = true;
#endif
    }
}

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <fast_io.h>
# include <fast_io_dsal/array.h>
# include <fast_io_dsal/string.h>
# include <fast_io_dsal/string_view.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::console_displays
{
    // Owned HOST expression text only. A record is never a captured value,
    // activation, pointer, stop ticket or permission to evaluate a guest.
    using view = ::fast_io::string_view;
    inline constexpr ::std::size_t maximum_displays{32u}, maximum_expression_bytes{256u};
    enum class action { none, invalid, add, list, remove, enable, disable };
    struct request { action operation{}; view expression{}; ::std::uint64_t identifier{}; };
    inline view trim(view text) noexcept
    {
        while(!text.empty() && (text.front() == ' ' || text.front() == '\t')) { text = text.subview(1u); }
        while(!text.empty() && (text.back() == ' ' || text.back() == '\t')) { text = text.subview(0u,text.size()-1u); }
        return text;
    }
    inline request parse(view text) noexcept
    {
        auto const word_end{[](view value) noexcept
        { ::std::size_t n{}; while(n < value.size() && value[n] != ' ' && value[n] != '\t') { ++n; } return n; }};
        text = trim(text); auto const end{word_end(text)};
        auto const name{text.subview(0u,end)};
        auto tail{trim(text.subview(end))};
        action op{};
        if(name == "display") { return tail.empty() ? request{action::list} : request{action::add,tail}; }
        if(name == "info" && tail == "display") { return {action::list}; }
        if(name == "undisplay") { op = action::remove; }
        else if(name == "enable" || name == "disable" || name == "delete")
        {
            auto const sub_end{word_end(tail)};
            auto const sub{tail.subview(0u,sub_end)};
            if(sub != "display") { return {}; }
            op = name == "enable" ? action::enable : name == "disable" ? action::disable : action::remove;
            tail = trim(tail.subview(sub_end));
        }
        else { return {}; }
        if(tail.empty()) { return {op}; } // No ID means all currently registered expressions.
        ::std::uint64_t id{};
        auto const scanned{::fast_io::parse_by_scan(tail.cbegin(),tail.cend(),::fast_io::mnp::dec_get<true,true>(id))};
        if(scanned.code != ::fast_io::parse_code::ok || scanned.iter != tail.cend() || id == 0u) { return {action::invalid}; }
        return {op,{},id};
    }
    struct record { ::std::uint64_t identifier{}; bool enabled{true}; ::fast_io::string expression{}; };
    enum class error { none, invalid_expression, exhausted, unknown_identifier };
    class session
    {
        ::fast_io::array<record,maximum_displays> records_{};
        ::std::uint64_t next_identifier_{1u}, observed_stop_{};
    public:
        [[nodiscard]] auto const& records() const noexcept { return records_; }
        [[nodiscard]] bool new_stop(::std::uint64_t stop) noexcept
        {
            if(stop == 0u || stop == observed_stop_) { return false; }
            observed_stop_ = stop; return true;
        }
        [[nodiscard]] error add(view expression, ::std::uint64_t& identifier)
        {
            identifier = 0u;
            if(expression.empty() || expression.size() > maximum_expression_bytes) { return error::invalid_expression; }
            for(unsigned char c : expression) { if(c < 32u || c >= 127u) { return error::invalid_expression; } }
            if(next_identifier_ == (::std::numeric_limits<::std::uint64_t>::max)()) { return error::exhausted; }
            for(auto& entry : records_)
            {
                if(entry.identifier != 0u) { continue; }
                entry.expression = ::fast_io::concat_fast_io(expression);
                entry.enabled = true; entry.identifier = next_identifier_++; identifier = entry.identifier; return error::none;
            }
            return error::exhausted;
        }
        [[nodiscard]] error change(action operation, ::std::uint64_t identifier) noexcept
        {
            if(operation != action::remove && operation != action::enable && operation != action::disable) { return error::invalid_expression; }
            bool found{identifier == 0u};
            for(auto& entry : records_)
            {
                if(entry.identifier == 0u || (identifier != 0u && entry.identifier != identifier)) { continue; }
                found = true;
                if(operation == action::remove) { entry.identifier = 0u; entry.expression.clear(); }
                else { entry.enabled = operation == action::enable; }
            }
            return found ? error::none : error::unknown_identifier;
        }
    };
}

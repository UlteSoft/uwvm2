// Human CLI discovery of copied module metadata. Names are display data only;
// they never select a module or admit a runtime read/breakpoint operation.
#pragma once
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/push_macros.h>
# include <algorithm>
# include <fast_io.h>
# include <fast_io_dsal/string.h>
# include "controller.h"
# include "console_aliases.h"
# include "console_tui.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::console_module_help
{
    using view = ::fast_io::string_view;
    struct request { bool handled{}, valid{}, selected{}, source{}; ::std::uint64_t module{}, page{1u}; };
    [[nodiscard]] inline request parse(view input)
    {
        auto const normalized{console_aliases::normalize(input)};
        if(!normalized.valid || normalized.ambiguous) { return {}; }
        auto const words{console_aliases::split(normalized.view())};
        if(words.word != "breaksource" && words.word != "breakwasm") { return {}; }
        auto const args{console_aliases::split(words.tail)};
        if(!args.tail.empty()) { return {}; } // Complete breakpoint commands use the real parser/controller.
        request result{}; result.handled = true; result.valid = true; result.source = words.word == "breaksource";
        if(args.word.empty()) { return result; }
        for(char c : args.word)
        { if(c < '0' || c > '9') { result.valid = false; return result; } }
        auto const scanned{::fast_io::parse_by_scan(args.word.cbegin(), args.word.cend(), ::fast_io::mnp::dec_get<true, true>(result.module))};
        result.valid = scanned.code == ::fast_io::parse_code::ok && scanned.iter == args.word.cend();
        result.selected = result.valid; return result;
    }
    [[nodiscard]] inline constexpr view source_status(module_source_status status) noexcept
    {
        switch(status)
        {
            case module_source_status::available: return "source breakpoints available";
            case module_source_status::missing: return "no source debug mapping; may need -g";
            case module_source_status::invalid: return "source debug mapping invalid or unsupported";
            case module_source_status::no_points: return "no executable source breakpoint locations";
            case module_source_status::replaced: return "original source locations unavailable after replacement";
            default: return "source breakpoint availability not fully checked";
        }
    }
    [[nodiscard]] inline ::fast_io::string no_points_note(module_source_status status)
    {
        if(status == module_source_status::available || status == module_source_status::unchecked) { return {}; }
        if(status == module_source_status::replaced)
        { return ::fast_io::concat_fast_io("Source mapping was retired by replacement. Use Wasm breakpoints, or reload with run.\n"); }
        if(status == module_source_status::invalid)
        { return ::fast_io::concat_fast_io("Check that embedded DWARF or the adjacent source map is valid and supported.\n"); }
        return ::fast_io::concat_fast_io("No usable source breakpoint locations in this module. It may have been compiled\nwithout -g (or equivalent source debug information), or its symbols were stripped.\nOptimization or a module without executable mapped statements can also cause this.\nRebuild the Wasm with debug information; numeric Wasm breakpoints use bw.\n");
    }
    [[nodiscard]] inline ::fast_io::string format(module_help_page const& page, bool color = false,
        ::std::size_t columns = 80u, bool selected = false, bool source = true)
    {
        if(!page.valid)
        { return ::fast_io::concat_fast_io("error: ", selected ? view{"Module ID is out of range."} : view{"Module page is out of range."},
            " Use \"info modules\" to list numeric IDs.\n"); }
        if(page.total == 0u) { return ::fast_io::concat_fast_io("No Wasm module metadata is available in this session.\n"); }
        ::fast_io::string text{::fast_io::concat_fast_io(color ? view{"\x1b[96m"} : view{}, "Wasm modules", color ? view{"\x1b[0m"} : view{}, "\n")};
        auto const width{::std::clamp(columns, ::std::size_t{24u}, ::std::size_t{240u})};
        for(::std::size_t i{}; i < page.size; ++i)
        {
            auto const& row{page.rows[i]};
            auto const raw_file{row.file.empty() ? view{"(file unavailable)"} : row.file.subview(0u)};
            auto basename{raw_file};
            for(::std::size_t at{}; at < raw_file.size(); ++at)
            { if(raw_file[at] == '/' || raw_file[at] == '\\') { basename = raw_file.subview(at + 1u); } }
            auto const file{console_tui::clean(raw_file, 8192u)};
            auto const label{::fast_io::concat_fast_io("  ", ::fast_io::mnp::dec(row.identifier), " = ", console_tui::clean(basename, 8192u))};
            text.append(console_tui::clip_cells(label.subview(0u), width)); text.push_back('\n');
            auto const name{console_tui::clean(row.name.subview(0u), 8192u)};
            auto const detail{::fast_io::concat_fast_io("    name=\"", name, "\"; ", source_status(row.source))};
            text.append(console_tui::clip_cells(detail.subview(0u), width)); text.push_back('\n');
            if(selected)
            { text.append(::fast_io::concat_fast_io("Loaded file: ", file, "\nSource: ", source_status(row.source), "\n"));
              text.append(no_points_note(row.source)); }
        }
        if(!selected)
        {
            text.append(::fast_io::concat_fast_io("Page ", ::fast_io::mnp::dec(page.page), "/", ::fast_io::mnp::dec(page.pages),
                " (", ::fast_io::mnp::dec(page.total), " modules)\n"));
            if(page.page < page.pages)
            { text.append(::fast_io::concat_fast_io("Next: info modules ", ::fast_io::mnp::dec(page.page + 1u), "\n")); }
        }
        text.append("Use the numeric ID before '='; module names, even numeric names, are display only.\n");
        auto const selector{selected && page.size == 1u ? ::fast_io::concat_fast_io(::fast_io::mnp::dec(page.rows[0].identifier)) :
            ::fast_io::concat_fast_io("MODULE_ID")};
        if(source)
        { text.append(::fast_io::concat_fast_io("Usage: bs ", selector, " FILE:LINE\n")); text.append("bs = breaksource. Use bs MODULE_ID for that module's details; help bs explains source breakpoints.\n"); }
        else
        { text.append(::fast_io::concat_fast_io("Usage: bw ", selector, " FUNCTION BYTE_OFFSET | bw ", selector, " SYMBOL\n"));
          text.append("bw = breakwasm. For a source line, use bs MODULE_ID FILE:LINE; type help bw or help bs.\n"); }
        return text;
    }
}
#endif
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif

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
# if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#  include <sys/ioctl.h>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::console_tui
{
    using view = ::fast_io::string_view;
    enum class layout { src, assembly, split, registers, wasm, wasip1 };
    inline constexpr ::fast_io::array<view, 6u> layout_names{{"src", "asm", "split", "regs", "wasm", "wasip1"}};
    struct dimensions { ::std::size_t columns{80u}, rows{24u}; };
    [[nodiscard]] inline dimensions terminal_dimensions() noexcept
    {
        dimensions result{};
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
        ::winsize size{};
        if(::fast_io::posix::libc_ioctl(::fast_io::out().native_handle(), TIOCGWINSZ, ::std::addressof(size)) == 0)
        { result = {size.ws_col, size.ws_row}; }
#elif defined(_WIN32) && !defined(__CYGWIN__)
        ::fast_io::win32::console_screen_buffer_info info{};
        if(::fast_io::win32::GetConsoleScreenBufferInfo(::fast_io::out().native_handle(), ::std::addressof(info)) != 0)
        { result = {static_cast<::std::size_t>(info.Window.Right-info.Window.Left+1),
                    static_cast<::std::size_t>(info.Window.Bottom-info.Window.Top+1)}; }
#endif
        result.columns = ::std::clamp(result.columns, ::std::size_t{1u}, ::std::size_t{240u});
        result.rows = ::std::clamp(result.rows, ::std::size_t{1u}, ::std::size_t{100u}); return result;
    }
    struct character { ::std::uint32_t scalar{}; ::std::size_t bytes{1u}, cells{1u}; bool valid{}; };
    [[nodiscard]] inline character decode(view text) noexcept
    {
        if(text.empty()) { return {}; }
        auto const first{static_cast<unsigned char>(text[0])};
        if(first < 128u) { return {first,1u,1u,true}; }
        ::std::size_t bytes{first>=194u && first<=223u ? 2u : first>=224u && first<=239u ? 3u : first>=240u && first<=244u ? 4u : 0u};
        if(bytes == 0u || bytes > text.size()) { return {first,1u,1u,false}; }
        ::std::uint32_t value{static_cast<::std::uint32_t>(first & (0x7fu >> bytes))};
        for(::std::size_t i{1u};i<bytes;++i)
        { auto const c{static_cast<unsigned char>(text[i])}; if((c&0xc0u)!=0x80u) { return {first,1u,1u,false}; } value=(value<<6u)|(c&0x3fu); }
        if((bytes==2u && value<128u)||(bytes==3u && value<2048u)||(bytes==4u && value<65536u)||
           value>0x10ffffu||(value>=0xd800u&&value<=0xdfffu)) { return {first,1u,1u,false}; }
        ::std::size_t cells{1u};
        if((value>=0x300u&&value<=0x36fu)||(value>=0x1ab0u&&value<=0x1affu)||
           (value>=0x1dc0u&&value<=0x1dffu)||(value>=0xfe00u&&value<=0xfe0fu)||
           (value>=0xfe20u&&value<=0xfe2fu)||value==0x200bu||value==0x200cu||value==0x200du||value==0xfeffu) { cells=0u; }
        else if((value>=0x1100u&&value<=0x115fu)||(value>=0x2329u&&value<=0x232au)||
                (value>=0x2e80u&&value<=0xa4cfu)||(value>=0xac00u&&value<=0xd7a3u)||
                (value>=0xf900u&&value<=0xfaffu)||(value>=0xfe10u&&value<=0xfe19u)||
                (value>=0xfe30u&&value<=0xfe6fu)||(value>=0xff00u&&value<=0xff60u)||
                (value>=0xffe0u&&value<=0xffe6u)||(value>=0x1f300u&&value<=0x1faffu)||
                (value>=0x20000u&&value<=0x3fffdu)) { cells=2u; }
        return {value,bytes,cells,true};
    }
    [[nodiscard]] inline ::std::size_t cells(view text) noexcept
    { ::std::size_t result{}; while(!text.empty()) { auto const c{decode(text)};result+=c.cells;text=text.subview(c.bytes); } return result; }
    [[nodiscard]] inline ::fast_io::string clean(view text, ::std::size_t cap = 32768u)
    {
        ::fast_io::string result{};
        while(!text.empty())
        {
            auto const c{decode(text)};
            if(c.valid && c.scalar>=32u && c.scalar!=127u && !(c.scalar>=128u&&c.scalar<=159u))
            { if(c.bytes>cap-result.size()) { break; } result.append(text.subview(0u,c.bytes));text=text.subview(c.bytes);continue; }
            ::fast_io::string escaped{};
            if(c.scalar=='\n' && c.valid) { escaped.push_back('\n'); }
            else if(c.scalar=='\t' && c.valid) { escaped.append("    "); }
            else if(!c.valid || c.scalar<32u || c.scalar==127u || (c.scalar>=128u&&c.scalar<=159u))
            { for(::std::size_t i{};i<c.bytes;++i) { escaped.append(::fast_io::concat_fast_io("\\x",::fast_io::mnp::hex<false,true>(static_cast<unsigned char>(text[i])))); } }
            else { escaped.append(text.subview(0u,c.bytes)); }
            if(escaped.size()>cap-result.size()) { break; }
            result.append(escaped);text=text.subview(c.bytes);
        }
        return result;
    }
    // Bounded ordinary source-file I/O, never a guest memory/native-code read.
    // Metadata alone cannot make us block on a device, FIFO or socket.
    [[nodiscard]] inline ::fast_io::string source_text(view path, ::std::uint64_t line)
    {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
            if(path.empty() || path.size() > 4096u || line == 0u) { return ::fast_io::concat_fast_io("source text unavailable\n"); }
            for(unsigned char c : path) { if(c < 32u || c == 127u) { return ::fast_io::concat_fast_io("source path unavailable\n"); } }
            auto const name{::fast_io::concat_fast_io(path)};
            ::fast_io::native_file file{name, ::fast_io::open_mode::in | ::fast_io::open_mode::no_block};
            auto const before{::fast_io::status(file)};
            if(before.type != ::fast_io::file_type::regular || before.size > 1024u * 1024u)
            { return ::fast_io::concat_fast_io("source text unavailable: regular files up to 1 MiB\n"); }
            if(before.size == 0u) { return ::fast_io::concat_fast_io("source file is empty\n"); }
            ::fast_io::string bytes{}; bytes.resize(static_cast<::std::size_t>(before.size));
            ::fast_io::operations::read_all(file, bytes.data(), bytes.data() + bytes.size());
            auto const after{::fast_io::status(file)};
            if(before.dev != after.dev || before.ino != after.ino || before.size != after.size ||
               before.mtim != after.mtim || before.ctim != after.ctim)
            { return ::fast_io::concat_fast_io("source file changed during read\n"); }
            ::fast_io::string result{}; ::std::uint64_t number{1u}; ::std::size_t start{};
            auto const first{line > 1u ? line - 1u : 1u};
            for(::std::size_t end{}; end <= bytes.size(); ++end)
            {
                if(end != bytes.size() && bytes[end] != '\n') { continue; }
                if(number >= first && number <= line + 32u)
                {
                    result.append(::fast_io::concat_fast_io(number == line ? view{"=> "} : view{"   "},
                        ::fast_io::mnp::dec(number), " ", clean(bytes.subview(start, ::std::min(end-start, ::std::size_t{2048u})), 2048u), "\n"));
                }
                if(number > line + 32u || result.size() > 16384u) { break; }
                ++number; start = end + 1u;
            }
            return result.empty() ? ::fast_io::concat_fast_io("source line is outside the file\n") : ::std::move(result);
        }
        catch(...) { return ::fast_io::concat_fast_io("source text unavailable; use tui source PATH\n"); }
#else
        return ::fast_io::concat_fast_io("source text unavailable on this build\n");
#endif
    }
    [[nodiscard]] inline ::fast_io::string source_text(::fast_io::string const& path, ::std::uint64_t line)
    { return source_text(path.subview(0u),line); }
    class screen final
    {
        void* context_{};
        void (*write_)(void*, view) noexcept{};
        bool terminal_{}, enabled_{}, code_focus_{true};
        layout layout_{layout::src};
        ::std::size_t scroll_{}, log_scroll_{};
        ::fast_io::string log_{}, status_{};
        ::fast_io::array<::fast_io::string, 6u> panels_{};
        ::fast_io::string source_override_{};
        static constexpr ::std::size_t log_limit{32768u};
        void emit(view text) const noexcept { write_(context_, text); }
        void emit(::fast_io::string const& text) const noexcept { emit(text.subview(0u)); }
        [[nodiscard]] static view row(view text, ::std::size_t index) noexcept
        {
            ::std::size_t b{};
            while(index != 0u)
            { auto const n{text.find_character('\n', b)}; if(n == ::fast_io::containers::npos) { return {}; } b=n+1u; --index; }
            auto const n{text.find_character('\n', b)}; return text.subview(b, n == ::fast_io::containers::npos ? text.size()-b : n-b);
        }
        [[nodiscard]] static view row(::fast_io::string const& text, ::std::size_t index) noexcept { return row(text.subview(0u),index); }
        [[nodiscard]] static ::std::size_t rows(view text) noexcept
        { ::std::size_t n{1u}; for(char c : text) { if(c == '\n') { ++n; } } return n; }
        [[nodiscard]] static ::std::size_t rows(::fast_io::string const& text) noexcept { return rows(text.subview(0u)); }
        void draw_row(::std::size_t number, view text, ::std::size_t columns) const
        {
            ::std::size_t size{}, width{}; auto const budget{columns>1u ? columns-1u : 0u};
            while(size<text.size())
            { auto const c{decode(text.subview(size))}; if(c.scalar=='\n' || c.cells>budget-width) { break; }
              size+=c.bytes;width+=c.cells; }
            emit(::fast_io::concat_fast_io("\x1b[", ::fast_io::mnp::dec(number), ";1H\x1b[2K", text.subview(0u,size)));
        }
        void draw_row(::std::size_t number, ::fast_io::string const& text, ::std::size_t columns) const { draw_row(number,text.subview(0u),columns); }
        void panel(::std::size_t start, ::std::size_t height, view title, view text, dimensions size) const
        {
            if(height == 0u) { return; }
            draw_row(start, ::fast_io::concat_fast_io("[", title, code_focus_ ? view{" *]"} : view{"]"}), size.columns);
            for(::std::size_t i{1u}; i < height; ++i) { draw_row(start+i, row(text, scroll_+i-1u), size.columns); }
        }
        void panel(::std::size_t start, ::std::size_t height, view title, ::fast_io::string const& text, dimensions size) const
        { panel(start,height,title,text.subview(0u),size); }
    public:
        screen(void* context, void (*write)(void*,view) noexcept, bool terminal) noexcept
            : context_{context}, write_{write}, terminal_{terminal} {}
        screen(screen const&) = delete;
        screen& operator=(screen const&) = delete;
        ~screen() { disable(); }
        [[nodiscard]] bool enabled() const noexcept { return enabled_; }
        [[nodiscard]] layout current_layout() const noexcept { return layout_; }
        [[nodiscard]] view source_override() const noexcept { return source_override_.subview(0u); }
        void enable() { if(terminal_ && !enabled_) { enabled_=true; emit("\x1b[?1049h\x1b[2J\x1b[H"); } }
        void disable() noexcept { if(enabled_) { enabled_=false; emit("\x1b[r\x1b[?25h\x1b[?1049l"); } }
        void record(view text)
        {
            auto safe{clean(text)};
            if(safe.size() >= log_limit) { log_=::fast_io::concat_fast_io(safe.subview(safe.size()-log_limit)); }
            else
            { if(log_.size() > log_limit-safe.size()) { log_=::fast_io::concat_fast_io(log_.subview(log_.size()-(log_limit-safe.size()))); }
              log_.append(safe); }
            log_scroll_=0u;
        }
        void output(view text) { record(text); if(!enabled_) { emit(text); } }
        void record(::fast_io::string const& text) { record(text.subview(0u)); }
        void output(::fast_io::string const& text) { output(text.subview(0u)); }
        void set_status(::fast_io::string const& text) { set_status(text.subview(0u)); }
        void set_panel(layout which, ::fast_io::string const& text) { set_panel(which,text.subview(0u)); }
        void set_status(view text) { status_=clean(text, 2048u); }
        void set_panel(layout which, view text) { panels_[static_cast<::std::size_t>(which)]=clean(text); }
        void clear_panels() { for(auto& value : panels_) { value.clear(); } scroll_=0u; }
        [[nodiscard]] bool command(view command)
        {
            if(command == "tui disable") { disable(); return true; }
            if(command == "tui enable")
            { if(!terminal_) { output("error: TUI requires an interactive terminal\n"); } else { enable(); } return true; }
            if(command.starts_with("tui source "))
            { auto const path{command.subview(11u)}; if(path.size() > 4096u) { output("error: source path exceeds 4096 bytes\n"); }
              else { source_override_=::fast_io::concat_fast_io(path); } return true; }
            if(command.starts_with("layout "))
            {
                auto const requested{command.subview(7u)};
                if(requested == "next") { layout_=static_cast<layout>((static_cast<unsigned>(layout_)+1u)%6u); }
                else
                {
                    ::std::size_t i{}; while(i < layout_names.size() && requested != layout_names[i]) { ++i; }
                    if(i == layout_names.size()) { output("error: layout src|asm|split|regs|wasm|wasip1|next\n"); return true; }
                    layout_=static_cast<layout>(i);
                }
                scroll_=0u; if(terminal_) { enable(); } else { output("error: TUI requires an interactive terminal\n"); } return true;
            }
            if(command.starts_with("focus "))
            {
                auto const focus{command.subview(6u)};
                if(focus == "next" || focus == "prev") { code_focus_=!code_focus_; }
                else if(focus == "cmd") { code_focus_=false; }
                else if(focus == "code" || focus == "src" || focus == "asm" || focus == "regs" || focus == "wasm" || focus == "wasip1") { code_focus_=true; }
                else { output("error: focus next|prev|cmd|code\n"); }
                return true;
            }
            if(command.starts_with("tui")) { output("error: tui enable|disable|source PATH\n"); return true; }
            return false;
        }
        void key(console_editing::action key)
        {
            using action=console_editing::action;
            if(key == action::tui_toggle) { if(enabled_) { disable(); } else { enable(); } }
            else if(key == action::tui_single) { layout_=layout::src; scroll_=0u; enable(); }
            else if(key == action::tui_split) { layout_=layout::split; scroll_=0u; enable(); }
            else if(enabled_ && (key == action::page_up || key == action::page_down))
            { auto& value{code_focus_ ? scroll_ : log_scroll_};
              bool const increment{code_focus_ ? key == action::page_down : key == action::page_up};
              value=increment ? ::std::min(value+5u, ::std::size_t{4096u}) : value>5u ? value-5u : 0u; }
        }
        void render(console_editing::editor const& editor, dimensions size = terminal_dimensions()) const
        {
            if(!enabled_) { return; }
            emit("\x1b[?25l\x1b[r");
            if(size.rows < 12u || size.columns < 30u)
            { draw_row(1u, "TUI: enlarge terminal or Ctrl+X A", size.columns); }
            else
            {
                draw_row(1u, ::fast_io::concat_fast_io("UWVM debugger | layout ", layout_names[static_cast<unsigned>(layout_)],
                    " | Ctrl+X A toggle | PgUp/PgDn"), size.columns);
                draw_row(2u, status_, size.columns);
                auto const code_height{(size.rows-4u)*2u/3u};
                if(layout_ == layout::split || layout_ == layout::registers)
                {
                    auto const half{code_height/2u};
                    panel(3u,half,layout_ == layout::split ? view{"source"} : view{"asm"},
                        panels_[static_cast<unsigned>(layout_ == layout::split ? layout::src : layout::assembly)],size);
                    panel(3u+half,code_height-half,layout_ == layout::split ? view{"asm"} : view{"registers"},
                        panels_[static_cast<unsigned>(layout_ == layout::split ? layout::assembly : layout::registers)],size);
                }
                else { panel(3u,code_height,layout_names[static_cast<unsigned>(layout_)],panels_[static_cast<unsigned>(layout_)],size); }
                auto const log_start{3u+code_height}, height{size.rows-log_start};
                draw_row(log_start, code_focus_ ? view{"[commands]"} : view{"[commands *]"},size.columns);
                auto const count{rows(log_)}; auto const offset{count>height+log_scroll_ ? count-height-log_scroll_ : 0u};
                for(::std::size_t i{1u}; i<height; ++i) { draw_row(log_start+i,row(log_,offset+i-1u),size.columns); }
            }
            auto const query{clean(editor.search_query(),32u)};
            auto const prompt_text{editor.searching() ? ::fast_io::concat_fast_io(
                editor.search_matched() ? view{"(search)`"} : view{"(failed search)`"},query,"': ") : ::fast_io::concat_fast_io("(uwvm-debug) ")};
            auto const prompt{prompt_text.subview(0u)};
            auto const edited{clean(editor.line().view(),console_editing::maximum_bytes*4u)};
            auto const before{clean(editor.line().view().subview(0u,editor.cursor()),console_editing::maximum_bytes*4u)};
            auto const available{size.columns > prompt.size()+1u ? size.columns-prompt.size()-1u : 0u};
            auto const cursor{::std::min(before.size(),edited.size())};
            ::std::size_t first{}, width{cells(edited.subview(0u,cursor))};
            while(first<cursor && width>available)
            { auto const c{decode(edited.subview(first))};first+=c.bytes;width-=c.cells; }
            draw_row(size.rows,::fast_io::concat_fast_io(prompt,edited.subview(first)),size.columns);
            auto const column{::std::min(size.columns,prompt.size()+width+1u)};
            emit(::fast_io::concat_fast_io("\x1b[",::fast_io::mnp::dec(size.rows),";",::fast_io::mnp::dec(column),"H\x1b[?25h"));
        }
    };
}

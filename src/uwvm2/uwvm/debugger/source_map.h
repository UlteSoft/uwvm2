/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <fast_io.h>
# include <algorithm>
# include <bit>
# include <climits>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <optional>
# include <span>
# include <string>
# include <string_view>
# include <utility>
# include <vector>
# include <fast_io_unit/string.h>
# include <cerrno>
# if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#  include <fcntl.h>
# endif
#endif
#include "source_map_v3.h"
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger
{
    // The caller supplies the payloads AFTER the Wasm custom-section names. A
    // DWARF address is relative to the start of the Code section CONTENT, not
    // to the module file, the section header, a function body, or native code.
    // See https://github.com/WebAssembly/tool-conventions/blob/main/Dwarf.md .
    struct source_line_compilation_directory
    {
        ::std::uint64_t line_unit_offset{};
        ::std::string_view directory{};
    };
    struct source_map_sections
    {
        ::std::span<::std::byte const> debug_line{};
        ::std::span<::std::byte const> debug_line_str{};
        ::std::span<::std::byte const> debug_str{};
        ::std::uint64_t code_section_content_size{};
        // Optional bounded local Source Map v3 fallback, used only without
        // embedded DWARF line data. Coordinates refer to the whole module.
        ::std::string_view source_map_json{}, source_map_directory{};
        ::std::uint64_t code_section_file_offset{}, module_file_size{};
        bool source_map_declared{};
        // Embedded CU metadata, keyed by DW_AT_stmt_list. Borrowed only during
        // parse; no working directory or source file is queried.
        ::std::span<source_line_compilation_directory const> compilation_directories{};
    };

    struct source_map_limits
    {
        ::std::size_t max_section_bytes{16u * 1024u * 1024u};
        ::std::size_t max_units{4096u};
        ::std::size_t max_rows{1u * 1024u * 1024u};
        ::std::size_t max_files{65536u};
        ::std::size_t max_path_bytes{4096u};
        ::std::size_t max_total_path_bytes{8u * 1024u * 1024u};
    };

    enum class source_map_error
    {
        none,
        missing_debug_line,
        limit_exceeded,
        malformed,
        unsupported_dwarf,
        ambiguous_ranges
    };

    struct source_location
    {
        ::std::uint64_t module{}, code_offset{};
        // The view remains valid while the owning source_map is alive and is
        // not moved or reparsed. Source text is never opened by this parser.
        ::std::string_view file{};
        ::std::uint64_t line{}, column{};
        bool is_statement{};
        // Distinguishes source blocks on the same line/column. This is owned
        // scalar line-table metadata, not a native address or a stop authority.
        ::std::uint64_t discriminator{};
        bool prologue_end{}, epilogue_begin{};
    };

    namespace source_map_details
    {
        struct reader
        {
            ::std::span<::std::byte const> bytes{};
            ::std::size_t cursor{};

            [[nodiscard]] bool take(::std::size_t n, ::std::span<::std::byte const>& out) noexcept
            {
                if(cursor > bytes.size() || n > bytes.size() - cursor) { return false; }
                // [safe] cursor+n <= bytes.size(); form the subspan before
                // advancing the cursor. No pointer can cross section_end.
                out = bytes.subspan(cursor, n);
                cursor += n;
                return true;
            }
            [[nodiscard]] bool skip(::std::size_t n) noexcept
            {
                if(cursor > bytes.size() || n > bytes.size() - cursor) { return false; }
                // [safe] checked byte count; cursor may equal section_end.
                cursor += n;
                return true;
            }
            [[nodiscard]] bool byte(::std::uint8_t& out) noexcept
            {
                if(cursor >= bytes.size()) { return false; }
                // [safe bytes] unsafe (one-past section_end)
                //              ^^ cursor is proven strictly inside bytes.
                out = ::std::to_integer<::std::uint8_t>(bytes[cursor]);
                // [consumed byte] [remaining safe bytes] unsafe (section_end)
                //                 ^^ advancing one checked byte may reach end.
                ++cursor;
                return true;
            }
            [[nodiscard]] bool fixed(::std::size_t n, ::std::uint64_t& out) noexcept
            {
                if(cursor > bytes.size() || n == 0u || n > 8u || n > bytes.size() - cursor) { return false; }
#if CHAR_BIT == 8
                ::std::byte encoded[8]{};
                // section_begin ... cursor [n checked bytes] ... section_end
                // [safe                ^^^^^^^^^^^^^^^^^^^^] unsafe (one-past)
                //                      ^^ n>0 proves cursor<size before +cursor.
                // The owned eight-byte destination is zero-initialized; n<=8
                // bounds the copy and zero-extends widths 3/5/6/7 without a
                // misaligned native integer dereference or a manual decoder.
                ::std::copy_n(bytes.data() + cursor, n, encoded);
                // [eight initialized owned bytes] unsafe (one-past encoded)
                // ^^ first                     ^^ last, proven exact extent.
                auto const first{reinterpret_cast<char const*>(encoded)};
                auto const last{first + sizeof(encoded)};
                ::std::uint64_t decoded{};
                auto const parsed{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<64u>(decoded))};
                if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last) { return false; }
                // Publish neither destination nor cursor until full consumption
                // of the owned complete field succeeds.
                out = decoded;
#else
                // Preserve the existing wide-native-byte fallback. Its SDK
                // and octet representation remain independently unqualified;
                // no 8-bit profile gains a branch or wider-character promise.
                out = 0u;
                for(::std::size_t i{}; i != n; ++i)
                { out |= static_cast<::std::uint64_t>(::std::to_integer<::std::uint8_t>(bytes[cursor + i])) << (8u * i); }
#endif
                // [consumed n safe bytes] [remaining section bytes] unsafe
                //                        ^^ cursor+n<=size was proven before
                // copying/parsing; this update may reach section_end exactly.
                cursor += n;
                return true;
            }
            template<typename Integer>
            [[nodiscard]] bool leb(Integer& out) noexcept
            {
                if(cursor >= bytes.size()) { return false; }
                // section_begin ... cursor ... section_end
                // [safe bytes       ^^^^^^^^^^^^^^^^^^^^^] unsafe (one-past)
                //                    ^^ first is formed only after cursor < size.
                auto const first{reinterpret_cast<char const*>(bytes.data() + cursor)};
                auto const last{reinterpret_cast<char const*>(bytes.data() + bytes.size())};
                auto const result{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::leb128_get(out))};
                if(result.code != ::fast_io::parse_code::ok) { return false; }
                // [consumed LEB bytes] [remaining section bytes] unsafe (one-past)
                //                    ^^ parser returns next within [first,last].
                cursor += static_cast<::std::size_t>(result.iter - first);
                return true;
            }
            [[nodiscard]] bool uleb(::std::uint64_t& out) noexcept { return leb(out); }
            [[nodiscard]] bool sleb(::std::int64_t& out) noexcept
            {
                return leb(out);
            }
            [[nodiscard]] bool cstring(::std::string_view& out, ::std::size_t max_bytes) noexcept
            {
                if(cursor > bytes.size()) { return false; }
                auto const begin{cursor};
                while(cursor != bytes.size())
                {
                    auto const c{::std::to_integer<::std::uint8_t>(bytes[cursor])};
                    if(c == 0u)
                    {
                        if(cursor - begin > max_bytes) { return false; }
                        // [safe] begin..cursor lies within bytes; the NUL is
                        // consumed only after forming this bounded view.
                        out = {reinterpret_cast<char const*>(bytes.data() + begin), cursor - begin};
                        ++cursor;
                        return true;
                    }
                    if(c < 0x20u || c == 0x7fu || cursor - begin == max_bytes) { return false; }
                    // [safe] cursor < section_end, and the path length is bounded.
                    ++cursor;
                }
                return false;
            }
        };

        [[nodiscard]] inline bool section_cstring(::std::span<::std::byte const> section,
                                                     ::std::uint64_t offset, ::std::size_t limit,
                                                     ::std::string_view& value) noexcept
        {
            if(offset > section.size()) { return false; }
            reader r{section.subspan(static_cast<::std::size_t>(offset))};
            return r.cstring(value, limit);
        }
        [[nodiscard]] inline bool add_u64(::std::uint64_t a, ::std::uint64_t b, ::std::uint64_t& out) noexcept
        {
            if(b > ::std::numeric_limits<::std::uint64_t>::max() - a) { return false; }
            out = a + b;
            return true;
        }
        [[nodiscard]] inline bool add_i64(::std::int64_t a, ::std::int64_t b, ::std::int64_t& out) noexcept
        {
            auto constexpr max{::std::numeric_limits<::std::int64_t>::max()};
            auto constexpr min{::std::numeric_limits<::std::int64_t>::min()};
            if((b > 0 && a > max - b) || (b < 0 && a < min - b)) { return false; }
            out = a + b;
            return true;
        }
        [[nodiscard]] inline bool absolute_path(::std::string_view path) noexcept
        {
            return !path.empty() && (path.front() == '/' || path.front() == '\\' ||
                (path.size() >= 2u && ((path[0] >= 'A' && path[0] <= 'Z') ||
                                        (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':'));
        }
        // Lexical comparison of producer paths only: no filesystem lookup,
        // symlink resolution, cwd dependence or source-file read authority.
        [[nodiscard]] inline ::std::string normalized_path(::std::string_view path)
        {
            ::std::vector<::std::string_view> parts{}; ::std::size_t cursor{};
            ::std::string prefix{};
            if(path.size() >= 2u && path[1u] == ':')
            { prefix = ::fast_io::concat_std(path.substr(0u,2u)); cursor = 2u; }
            bool const rooted{cursor < path.size() && (path[cursor] == '/' || path[cursor] == '\\')};
            if(rooted) { prefix = ::fast_io::concat_std(::std::string_view{prefix}, "/"); }
            while(cursor < path.size())
            {
                while(cursor < path.size() && (path[cursor] == '/' || path[cursor] == '\\')) { ++cursor; }
                auto const begin{cursor};
                while(cursor < path.size() && path[cursor] != '/' && path[cursor] != '\\') { ++cursor; }
                auto const component{path.substr(begin,cursor-begin)};
                if(component.empty() || component == ".") { continue; }
                if(component == ".." && !parts.empty() && parts.back() != "..") { parts.pop_back(); }
                else if(component != ".." || !rooted) { parts.push_back(component); }
            }
            auto result{::std::move(prefix)};
            for(auto component : parts)
            { result = ::fast_io::concat_std(::std::string_view{result}, !result.empty() && result.back() != '/' && result.back() != ':' ? ::std::string_view{"/"} : ::std::string_view{}, component); }
            return result;
        }
        struct file_entry
        {
            ::std::string_view name{};
            ::std::uint64_t directory{};
        };
        struct entry_format
        {
            ::std::uint64_t content{}, form{};
        };
        struct form_value
        {
            ::std::string_view text{};
            ::std::uint64_t number{};
            bool is_text{};
            bool is_number{};
        };

        [[nodiscard]] inline source_map_error read_form(reader& r, entry_format format,
            source_map_sections const& sections, source_map_limits const& limits,
            ::std::size_t offset_width, ::std::size_t address_width, form_value& value)
        {
            auto const form{format.form};
            ::std::uint64_t n{};
            switch(form)
            {
                case 0x08u: // DW_FORM_string
                    if(!r.cstring(value.text, limits.max_path_bytes)) { return source_map_error::malformed; }
                    value.is_text = true; return source_map_error::none;
                case 0x1fu: // DW_FORM_line_strp
                case 0x0eu: // DW_FORM_strp
                    if(!r.fixed(offset_width, n)) { return source_map_error::malformed; }
                    if(!section_cstring(form == 0x1fu ? sections.debug_line_str : sections.debug_str,
                                        n, limits.max_path_bytes, value.text)) { return source_map_error::malformed; }
                    value.is_text = true; return source_map_error::none;
                case 0x01u: // DW_FORM_addr
                    if(!r.fixed(address_width, value.number)) { return source_map_error::malformed; }
                    value.is_number = true;
                    return source_map_error::none;
                case 0x0bu: n = 1u; break; // DW_FORM_data1
                case 0x05u: n = 2u; break; // DW_FORM_data2
                case 0x06u: n = 4u; break; // DW_FORM_data4
                case 0x07u: n = 8u; break; // DW_FORM_data8
                case 0x17u: n = offset_width; break; // DW_FORM_sec_offset
                case 0x0cu: n = 1u; break; // DW_FORM_flag
                case 0x0fu: // DW_FORM_udata
                    if(!r.uleb(value.number)) { return source_map_error::malformed; }
                    value.is_number = true;
                    return source_map_error::none;
                case 0x0du: // DW_FORM_sdata; only nonnegative values can be used as indexes
                {
                    ::std::int64_t signed_value{};
                    if(!r.sleb(signed_value) || signed_value < 0) { return source_map_error::malformed; }
                    value.number = static_cast<::std::uint64_t>(signed_value);
                    value.is_number = true;
                    return source_map_error::none;
                }
                case 0x19u: value.number = 1u; value.is_number = true;
                    return source_map_error::none; // flag_present
                case 0x1eu: // data16 (usually file MD5)
                    return r.skip(16u) ? source_map_error::none : source_map_error::malformed;
                case 0x09u: // block
                case 0x18u: // exprloc
                    if(!r.uleb(n)) { return source_map_error::malformed; }
                    return n <= r.bytes.size() - r.cursor && r.skip(static_cast<::std::size_t>(n))
                        ? source_map_error::none : source_map_error::malformed;
                case 0x0au: // block1
                {
                    ::std::uint8_t size{};
                    return r.byte(size) && r.skip(size) ? source_map_error::none : source_map_error::malformed;
                }
                case 0x03u: // block2
                case 0x04u: // block4
                    if(!r.fixed(form == 0x03u ? 2u : 4u, n)) { return source_map_error::malformed; }
                    return n <= r.bytes.size() - r.cursor && r.skip(static_cast<::std::size_t>(n))
                        ? source_map_error::none : source_map_error::malformed;
                default: return source_map_error::unsupported_dwarf;
            }
            if(!r.fixed(static_cast<::std::size_t>(n), value.number)) { return source_map_error::malformed; }
            value.is_number = true;
            return source_map_error::none;
        }

        [[nodiscard]] inline source_map_error read_formats(reader& r,
            ::std::vector<entry_format>& formats)
        {
            ::std::uint8_t count{};
            if(!r.byte(count) || count > 64u) { return source_map_error::malformed; }
            for(unsigned i{}; i != count; ++i)
            {
                entry_format f{};
                if(!r.uleb(f.content) || !r.uleb(f.form)) { return source_map_error::malformed; }
                formats.push_back(f);
            }
            return source_map_error::none;
        }
        [[nodiscard]] inline source_map_error read_v5_entry(reader& r,
            ::std::vector<entry_format> const& formats, source_map_sections const& sections,
            source_map_limits const& limits, ::std::size_t offset_width,
            ::std::size_t address_width, file_entry& out, bool allow_empty_path = false)
        {
            bool path_seen{}, dir_seen{};
            for(auto const f : formats)
            {
                form_value v{};
                auto const error{read_form(r, f, sections, limits, offset_width, address_width, v)};
                if(error != source_map_error::none) { return error; }
                if(f.content == 1u) // DW_LNCT_path
                {
                    if(path_seen || !v.is_text || (v.text.empty() && !allow_empty_path))
                    { return source_map_error::malformed; }
                    out.name = v.text;
                    path_seen = true;
                }
                else if(f.content == 2u) // DW_LNCT_directory_index
                {
                    if(dir_seen || !v.is_number) { return source_map_error::malformed; }
                    out.directory = v.number;
                    dir_seen = true;
                }
            }
            return path_seen ? source_map_error::none : source_map_error::malformed;
        }
    } // namespace source_map_details

    // Only an adjacent basename is accepted for implicit sidecar loading.
    // URL fetches, path traversal, symlinks and non-regular files cannot make
    // a debugger metadata request open arbitrary endpoints or block on a FIFO.
    [[nodiscard]] inline bool read_source_map_sidecar(::std::span<::std::byte const> payload,
        ::std::string_view module_path,::fast_io::string& bytes,::fast_io::string& directory)
    {
        bytes.clear();directory.clear();
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
        source_map_details::reader input{payload};::std::uint32_t length{};
        if(!input.leb(length) || length==0u || length>4096u || length!=payload.size()-input.cursor ||
           module_path.empty() || module_path.size()>4096u) { return false; }
        // reader proved an entire nonempty URL field inside the custom payload.
        auto name{::std::string_view{reinterpret_cast<char const*>(payload.data()+input.cursor),length}};
        if(name.starts_with("./")) { name.remove_prefix(2u); }
        if(name.empty()) { return false; }
        if(name=="." || name==".." || !source_map_v3::detail::utf8(name)) { return false; }
        for(unsigned char c:name)
        { if(c<32u || c==127u || c=='/' || c=='\\' || c==':' || c=='?' || c=='#' || c=='%') { return false; } }
        for(unsigned char c:module_path) { if(c<32u || c==127u) { return false; } }
        auto const slash{module_path.find_last_of('/')};
        auto const prefix{slash==module_path.npos?::std::string_view{}:module_path.substr(0u,slash+1u)};
        if(prefix.size()>4096u-name.size()) { return false; }
        auto const path{::fast_io::concat_fast_io(prefix,name)};
        auto opened{::fast_io::posix_openat_nothrow(::fast_io::posix_at_entry{AT_FDCWD},path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK)};
        if(!opened) { return false; }auto& file{opened.file};
        auto const before{::fast_io::posix_status_nothrow(file)};
        bool valid{before && before.value.type==::fast_io::file_type::regular && before.value.size!=0u && before.value.size<=16u*1024u*1024u};
        if(valid)
        {
            bytes.resize(static_cast<::std::size_t>(before.value.size));::std::size_t consumed{};
            while(consumed<bytes.size())
            {
                auto const result{::fast_io::posix_read_nothrow(file,bytes.data()+consumed,bytes.size()-consumed)};
                if(result.error==EINTR) { continue; }
                if(!result || result.transferred==0u || result.transferred>bytes.size()-consumed) { valid=false;break; }
                consumed+=result.transferred;
            }
            if(valid)
            {
                char extra{};::fast_io::posix_read_result result{};
                do { result=::fast_io::posix_read_nothrow(file,::std::addressof(extra),1u); }while(result.error==EINTR);
                auto const after{::fast_io::posix_status_nothrow(file)};
                valid=result && result.transferred==0u && after && before.value.dev==after.value.dev && before.value.ino==after.value.ino &&
                    before.value.size==after.value.size && before.value.mtim==after.value.mtim && before.value.ctim==after.value.ctim;
            }
        }
        if(!::fast_io::posix_close_nothrow(file)) { valid=false; }
        if(!valid) { bytes.clear();return false; }
        directory=::fast_io::concat_fast_io(prefix.empty()?::std::string_view{"."}:prefix);return true;
#else
        (void)payload;(void)module_path;return false;
#endif
    }

    class source_map
    {
        struct range
        {
            ::std::uint64_t begin{}, end{}, line{}, column{};
            ::std::size_t file{};
            bool is_statement{};
            ::std::uint64_t discriminator{};
            bool prologue_end{}, epilogue_begin{};
        };
        struct state
        {
            ::std::uint64_t address{}, op_index{}, file{1u}, column{};
            ::std::int64_t line{1};
            bool is_statement{};
            ::std::uint64_t discriminator{};
            bool prologue_end{}, epilogue_begin{};
        };
        ::std::uint64_t module_{}, code_size_{};
        ::std::vector<::std::string> files_{};
        ::std::vector<range> ranges_{};

        [[nodiscard]] static source_map_error parse_unit(
            source_map& result, source_map_details::reader& unit,
            source_map_sections const& sections, source_map_limits const& limits,
            ::std::size_t offset_width, ::std::string_view compilation_directory,
            ::std::size_t& path_bytes, ::std::size_t& rows);

    public:
        // Atomic result: on failure `output` is unchanged. No files or network
        // endpoints are opened. Allocations belong to this cold debugger path.
        [[nodiscard]] static source_map_error parse(::std::uint64_t module,
            source_map_sections sections, source_map& output,
            source_map_limits limits = {});

        [[nodiscard]] ::std::optional<source_location> lookup(::std::uint64_t module,
            ::std::uint64_t code_section_content_offset) const noexcept;
        [[nodiscard]] ::std::size_t range_count() const noexcept { return ranges_.size(); }
        [[nodiscard]] ::std::uint64_t module() const noexcept { return module_; }
    };

    inline source_map_error source_map::parse_unit(source_map& result,
        source_map_details::reader& unit, source_map_sections const& sections,
        source_map_limits const& limits, ::std::size_t offset_width, ::std::string_view compilation_directory,
        ::std::size_t& path_bytes, ::std::size_t& rows)
    {
        using source_map_details::reader;
        using source_map_details::file_entry;
        using source_map_details::entry_format;
        ::std::uint64_t raw{};
        if(!unit.fixed(2u, raw)) { return source_map_error::malformed; }
        auto const version{raw};
        if(version != 4u && version != 5u) { return source_map_error::unsupported_dwarf; }
        ::std::size_t address_width{};
        if(version == 5u)
        {
            ::std::uint8_t segment_width{};
            ::std::uint8_t address_byte{};
            if(!unit.byte(address_byte) || !unit.byte(segment_width) || segment_width != 0u ||
               (address_byte != 4u && address_byte != 8u)) { return source_map_error::unsupported_dwarf; }
            address_width = address_byte;
        }
        if(!unit.fixed(offset_width, raw) || raw > unit.bytes.size() - unit.cursor)
        { return source_map_error::malformed; }
        // [safe] header_length <= remaining unit bytes. `header` and `program`
        // are disjoint bounded views; advancing unit to the program start is
        // legal even when the prologue ends exactly at unit_end.
        reader header{unit.bytes.subspan(unit.cursor, static_cast<::std::size_t>(raw))};
        unit.cursor += static_cast<::std::size_t>(raw);
        reader program{unit.bytes.subspan(unit.cursor)};

        ::std::uint8_t min_instruction{}, max_operations{1u}, default_stmt{},
            line_base_byte{}, line_range{}, opcode_base{};
        if(!header.byte(min_instruction) || !header.byte(max_operations) ||
           !header.byte(default_stmt) || !header.byte(line_base_byte) ||
           !header.byte(line_range) || !header.byte(opcode_base) ||
           min_instruction == 0u || max_operations == 0u || line_range == 0u ||
           opcode_base == 0u || default_stmt > 1u)
        { return source_map_error::malformed; }
        auto const line_base{static_cast<::std::int64_t>(static_cast<::std::int8_t>(line_base_byte))};
        ::std::vector<::std::uint8_t> standard_lengths;
        standard_lengths.reserve(opcode_base - 1u);
        for(unsigned i{1u}; i != opcode_base; ++i)
        {
            ::std::uint8_t operands{};
            if(!header.byte(operands)) { return source_map_error::malformed; }
            standard_lengths.push_back(operands);
        }
        ::std::vector<::std::string_view> directories;
        ::std::vector<file_entry> file_entries;
        ::std::vector<entry_format> file_formats;
        if(version == 4u)
        {
            for(;;)
            {
                ::std::string_view directory;
                if(!header.cstring(directory, limits.max_path_bytes)) { return source_map_error::malformed; }
                if(directory.empty()) { break; }
                if(directories.size() == limits.max_files) { return source_map_error::limit_exceeded; }
                directories.push_back(directory);
            }
            for(;;)
            {
                file_entry entry;
                if(!header.cstring(entry.name, limits.max_path_bytes)) { return source_map_error::malformed; }
                if(entry.name.empty()) { break; }
                ::std::uint64_t ignored{};
                if(!header.uleb(entry.directory) || !header.uleb(ignored) || !header.uleb(ignored))
                { return source_map_error::malformed; }
                if(file_entries.size() == limits.max_files) { return source_map_error::limit_exceeded; }
                file_entries.push_back(entry);
            }
        }
        else
        {
            ::std::vector<entry_format> directory_formats;
            auto error{source_map_details::read_formats(header, directory_formats)};
            if(error != source_map_error::none) { return error; }
            ::std::uint64_t count{};
            if(!header.uleb(count)) { return source_map_error::malformed; }
            if(count > limits.max_files) { return source_map_error::limit_exceeded; }
            for(::std::uint64_t i{}; i != count; ++i)
            {
                file_entry entry;
                error = source_map_details::read_v5_entry(header, directory_formats,
                    sections, limits, offset_width, address_width, entry, true);
                if(error != source_map_error::none) { return error; }
                directories.push_back(entry.name);
            }
            error = source_map_details::read_formats(header, file_formats);
            if(error != source_map_error::none) { return error; }
            if(!header.uleb(count)) { return source_map_error::malformed; }
            if(count > limits.max_files) { return source_map_error::limit_exceeded; }
            for(::std::uint64_t i{}; i != count; ++i)
            {
                file_entry entry;
                error = source_map_details::read_v5_entry(header, file_formats,
                    sections, limits, offset_width, address_width, entry);
                if(error != source_map_error::none) { return error; }
                file_entries.push_back(entry);
            }
        }
        if(header.cursor != header.bytes.size()) { return source_map_error::malformed; }

        auto const file_base{result.files_.size()};
        auto append_file = [&](file_entry entry) -> source_map_error
        {
            if(result.files_.size() == limits.max_files) { return source_map_error::limit_exceeded; }
            ::std::string_view directory;
            if(version == 4u)
            {
                if(entry.directory > directories.size()) { return source_map_error::malformed; }
                if(entry.directory != 0u) { directory = directories[static_cast<::std::size_t>(entry.directory - 1u)]; }
            }
            else if(!directories.empty())
            {
                if(entry.directory >= directories.size()) { return source_map_error::malformed; }
                directory = directories[static_cast<::std::size_t>(entry.directory)];
            }
            else if(entry.directory != 0u) { return source_map_error::malformed; }
            ::std::string path;
            if(!source_map_details::absolute_path(entry.name) && !directory.empty())
            {
                path.assign(directory);
                if(path.back() != '/' && path.back() != '\\') { path.push_back('/'); }
            }
            path.append(entry.name);
            if(!source_map_details::absolute_path(path) && !compilation_directory.empty())
            { path = ::fast_io::concat_std(compilation_directory, "/", path); }
            path = source_map_details::normalized_path(path);
            if(path.size() > limits.max_path_bytes || path_bytes > limits.max_total_path_bytes ||
               path.size() > limits.max_total_path_bytes - path_bytes)
            { return source_map_error::limit_exceeded; }
            path_bytes += path.size();
            result.files_.push_back(::std::move(path));
            return source_map_error::none;
        };
        for(auto const entry : file_entries)
        {
            auto const error{append_file(entry)};
            if(error != source_map_error::none) { return error; }
        }

        state current;
        current.is_statement = default_stmt != 0u;
        ::std::optional<range> pending;
        ::std::size_t sequence_ranges_begin{result.ranges_.size()};
        bool ignored_tombstone_sequence{};
        auto advance = [&](::std::uint64_t operation_advance) -> bool
        {
            // The linker may retain line programs for removed functions with
            // an all-ones address tombstone. Consume their encoded operands
            // without doing address arithmetic or publishing source ranges.
            if(ignored_tombstone_sequence) { return true; }
            if(operation_advance > ::std::numeric_limits<::std::uint64_t>::max() - current.op_index)
            { return false; }
            auto const total{current.op_index + operation_advance};
            auto const instruction_advance{total / max_operations};
            if(instruction_advance > (::std::numeric_limits<::std::uint64_t>::max() - current.address) / min_instruction)
            { return false; }
            current.address += instruction_advance * min_instruction;
            current.op_index = total % max_operations;
            return current.address <= result.code_size_;
        };
        auto close_pending = [&](::std::uint64_t next_address) -> source_map_error
        {
            if(!pending) { return source_map_error::none; }
            if(next_address < pending->begin || next_address > result.code_size_)
            { return source_map_error::malformed; }
            if(next_address != pending->begin)
            {
                pending->end = next_address;
                result.ranges_.push_back(*pending);
            }
            pending.reset();
            return source_map_error::none;
        };
        auto emit_row = [&](bool end_sequence) -> source_map_error
        {
            if(rows == limits.max_rows) { return source_map_error::limit_exceeded; }
            ++rows;
            if(ignored_tombstone_sequence)
            {
                if(end_sequence)
                {
                    current = {};
                    current.is_statement = default_stmt != 0u;
                    ignored_tombstone_sequence = false;
                    sequence_ranges_begin = result.ranges_.size();
                }
                // DWARF row emission resets this transient register even for
                // ignored dead-code rows; a later sequence cannot inherit it.
                current.discriminator = 0u; current.prologue_end = current.epilogue_begin = false;
                return source_map_error::none;
            }
            if(current.address > result.code_size_ || current.line < 0)
            { return source_map_error::malformed; }
            auto error{close_pending(current.address)};
            if(error != source_map_error::none) { return error; }
            if(!end_sequence)
            {
                auto const local_file{version == 4u ? current.file - 1u : current.file};
                if((version == 4u && current.file == 0u) || local_file >= file_entries.size())
                { return source_map_error::malformed; }
                pending = range{current.address, current.address,
                    static_cast<::std::uint64_t>(current.line), current.column,
                    file_base + static_cast<::std::size_t>(local_file), current.is_statement,
                    current.discriminator, current.prologue_end, current.epilogue_begin};
            }
            else
            {
                current = {};
                current.is_statement = default_stmt != 0u;
                sequence_ranges_begin = result.ranges_.size();
            }
            // DWARF4/5 row semantics: copy/special/end_sequence all reset the
            // discriminator after recording the row. Column remains persistent.
            // https://dwarfstd.org/issues/090128.1.html
            current.discriminator = 0u; current.prologue_end = current.epilogue_begin = false;
            return source_map_error::none;
        };

        bool ended{program.bytes.empty()};
        while(program.cursor != program.bytes.size())
        {
            ::std::uint8_t opcode{};
            if(!program.byte(opcode)) { return source_map_error::malformed; }
            ended = false;
            if(opcode == 0u)
            {
                ::std::uint64_t length{};
                if(!program.uleb(length) || length == 0u || length > program.bytes.size() - program.cursor)
                { return source_map_error::malformed; }
                // [safe] length lies wholly in the line program; the cursor
                // advances past precisely this extended opcode, including its
                // opcode byte, after a bounded subreader has been formed.
                reader extended{program.bytes.subspan(program.cursor, static_cast<::std::size_t>(length))};
                program.cursor += static_cast<::std::size_t>(length);
                ::std::uint8_t kind{};
                if(!extended.byte(kind)) { return source_map_error::malformed; }
                switch(kind)
                {
                    case 1u: // DW_LNE_end_sequence
                    {
                        auto const error{emit_row(true)};
                        if(error != source_map_error::none) { return error; }
                        ended = true;
                        break;
                    }
                    case 2u: // DW_LNE_set_address
                    {
                        auto const bytes{extended.bytes.size() - extended.cursor};
                        if(version == 5u ? bytes != address_width : bytes != 4u && bytes != 8u)
                        { return source_map_error::malformed; }
                        ::std::uint64_t address{};
                        if(!extended.fixed(bytes, address)) { return source_map_error::malformed; }
                        auto const tombstone{bytes == 4u ?
                            static_cast<::std::uint64_t>((::std::numeric_limits<::std::uint32_t>::max)()) :
                            (::std::numeric_limits<::std::uint64_t>::max)()};
                        if(address == tombstone)
                        {
                            // lld relocates dead .debug_line functions to -1.
                            // Remove every row from this sequence, including
                            // rows seen before its tombstone, and never expose
                            // the sentinel as a real Code-section address.
                            result.ranges_.resize(sequence_ranges_begin);
                            pending.reset();
                            ignored_tombstone_sequence = true;
                            current.address = 0u;
                        }
                        else if(address > result.code_size_) { return source_map_error::malformed; }
                        else if(!ignored_tombstone_sequence) { current.address = address; }
                        current.op_index = 0u;
                        break;
                    }
                    case 3u: // DW_LNE_define_file
                    {
                        file_entry entry;
                        if(version == 4u)
                        {
                            ::std::uint64_t ignored{};
                            if(!extended.cstring(entry.name, limits.max_path_bytes) || entry.name.empty() ||
                               !extended.uleb(entry.directory) || !extended.uleb(ignored) || !extended.uleb(ignored))
                            { return source_map_error::malformed; }
                        }
                        else
                        {
                            auto const error{source_map_details::read_v5_entry(extended, file_formats,
                                sections, limits, offset_width, address_width, entry)};
                            if(error != source_map_error::none) { return error; }
                        }
                        auto const error{append_file(entry)};
                        if(error != source_map_error::none) { return error; }
                        file_entries.push_back(entry);
                        break;
                    }
                    case 4u: // DW_LNE_set_discriminator
                    {
                        // [bounded extended-op payload] payload_end
                        // [safe                       ] reader uses fast_io LEB
                        // and advances only within this checked payload. Keep
                        // the full scalar value; it is never used as an index.
                        if(!extended.uleb(current.discriminator)) { return source_map_error::malformed; }
                        break;
                    }
                    default: return source_map_error::unsupported_dwarf;
                }
                if(extended.cursor != extended.bytes.size()) { return source_map_error::malformed; }
                continue;
            }
            if(opcode >= opcode_base)
            {
                auto const adjusted{static_cast<::std::uint64_t>(opcode - opcode_base)};
                if(!advance(adjusted / line_range)) { return source_map_error::malformed; }
                ::std::int64_t next_line{};
                if(!source_map_details::add_i64(current.line,
                    line_base + static_cast<::std::int64_t>(adjusted % line_range), next_line))
                { return source_map_error::malformed; }
                current.line = next_line;
                auto const error{emit_row(false)};
                if(error != source_map_error::none) { return error; }
                continue;
            }
            // The header's operand counts describe unknown standard opcodes,
            // but not their effects on the state machine. Never guess a row.
            if(opcode > 12u) { return source_map_error::unsupported_dwarf; }
            ::std::uint64_t operand{};
            switch(opcode)
            {
                case 1u: // DW_LNS_copy
                {
                    auto const error{emit_row(false)};
                    if(error != source_map_error::none) { return error; }
                    break;
                }
                case 2u: // DW_LNS_advance_pc
                    if(!program.uleb(operand) || !advance(operand)) { return source_map_error::malformed; }
                    break;
                case 3u: // DW_LNS_advance_line
                {
                    ::std::int64_t delta{}, next{};
                    if(!program.sleb(delta) || !source_map_details::add_i64(current.line, delta, next))
                    { return source_map_error::malformed; }
                    current.line = next;
                    break;
                }
                case 4u: // DW_LNS_set_file
                    if(!program.uleb(current.file)) { return source_map_error::malformed; }
                    break;
                case 5u: // DW_LNS_set_column
                    if(!program.uleb(current.column)) { return source_map_error::malformed; }
                    break;
                case 6u: current.is_statement = !current.is_statement; break; // negate_stmt
                case 7u: break; // set_basic_block
                case 8u: // const_add_pc
                    if(!advance((255u - opcode_base) / line_range)) { return source_map_error::malformed; }
                    break;
                case 9u: // fixed_advance_pc
                    if(!program.fixed(2u, operand)) { return source_map_error::malformed; }
                    if(!ignored_tombstone_sequence &&
                       (!source_map_details::add_u64(current.address, operand, current.address) ||
                        current.address > result.code_size_)) { return source_map_error::malformed; }
                    current.op_index = 0u;
                    break;
                case 10u: current.prologue_end = true; break; // set_prologue_end
                case 11u: current.epilogue_begin = true; break; // set_epilogue_begin
                case 12u: // set_isa
                    if(!program.uleb(operand)) { return source_map_error::malformed; }
                    break;
                default: return source_map_error::unsupported_dwarf;
            }
        }
        // A terminal DW_LNE_end_sequence is essential: the last row has no
        // valid address range without one, even if the bytes otherwise parse.
        return pending || !ended ? source_map_error::malformed : source_map_error::none;
    }

    inline source_map_error source_map::parse(::std::uint64_t module,
        source_map_sections sections, source_map& output, source_map_limits limits)
    {
        if(sections.debug_line.empty() && sections.source_map_declared)
        {
            if(sections.source_map_json.size()>limits.max_section_bytes || limits.max_section_bytes>64u*1024u*1024u ||
               limits.max_rows>4u*1024u*1024u || limits.max_files>262144u || limits.max_path_bytes>16384u ||
               limits.max_total_path_bytes>64u*1024u*1024u || limits.max_rows==0u || limits.max_files==0u || limits.max_path_bytes==0u)
            { return source_map_error::limit_exceeded; }
            source_map_v3::image mapped{};
            auto const status{source_map_v3::parse(sections.source_map_json,sections.code_section_file_offset,
                sections.code_section_content_size,sections.module_file_size,mapped)};
            if(status!=source_map_v3::error::none)
            { return status==source_map_v3::error::limit_exceeded?source_map_error::limit_exceeded:source_map_error::malformed; }
            if(mapped.sources.size()>limits.max_files || mapped.rows.size()>limits.max_rows) { return source_map_error::limit_exceeded; }
            source_map candidate{};candidate.module_=module;candidate.code_size_=sections.code_section_content_size;
            ::std::size_t paths{};
            for(auto const& entry:mapped.sources)
            {
                auto const name{source_map_v3::view(entry)};auto const root{source_map_v3::view(mapped.source_root)};
                auto const absolute{[](auto text) { return !text.empty() && (text[0]=='/' || (text.size()>1u && text[1]==':')); }};
                auto path{absolute(name)?::fast_io::concat_std(name):absolute(root)?::fast_io::concat_std(root,"/",name):
                    ::fast_io::concat_std(sections.source_map_directory,"/",root,"/",name)};
                path=source_map_details::normalized_path(path);
                if(path.size()>limits.max_path_bytes || paths>limits.max_total_path_bytes || path.size()>limits.max_total_path_bytes-paths)
                { return source_map_error::limit_exceeded; }paths+=path.size();candidate.files_.push_back(::std::move(path));
            }
            for(auto const& row:mapped.rows)
            { candidate.ranges_.push_back({row.begin,row.end,row.line,row.column,row.file,true}); }
            output=::std::move(candidate);return source_map_error::none;
        }
        if(sections.debug_line.empty()) { return source_map_error::missing_debug_line; }
        if(sections.debug_line.size() > limits.max_section_bytes ||
           sections.debug_line_str.size() > limits.max_section_bytes ||
           sections.debug_str.size() > limits.max_section_bytes ||
           limits.max_section_bytes > 64u * 1024u * 1024u ||
           limits.max_units > 16384u || limits.max_rows > 4u * 1024u * 1024u ||
           limits.max_files > 262144u || limits.max_path_bytes > 16384u ||
           limits.max_total_path_bytes > 64u * 1024u * 1024u ||
           limits.max_path_bytes == 0u || limits.max_files == 0u || limits.max_rows == 0u)
        { return source_map_error::limit_exceeded; }
        if(sections.compilation_directories.size() > limits.max_units) { return source_map_error::limit_exceeded; }
        for(::std::size_t i{}; i != sections.compilation_directories.size(); ++i)
        {
            auto const& entry{sections.compilation_directories[i]};
            if(entry.directory.size() > limits.max_path_bytes) { return source_map_error::limit_exceeded; }
            if(entry.directory.find(char{}) != ::std::string_view::npos ||
               (i != 0u && sections.compilation_directories[i - 1u].line_unit_offset >= entry.line_unit_offset))
            { return source_map_error::malformed; }
        }
        ::std::size_t directory_index{};
        source_map result;
        result.module_ = module;
        result.code_size_ = sections.code_section_content_size;
        source_map_details::reader input{sections.debug_line};
        ::std::size_t units{}, rows{}, path_bytes{};
        while(input.cursor != input.bytes.size())
        {
            if(units == limits.max_units) { return source_map_error::limit_exceeded; }
            ++units;
            auto const line_unit_offset{input.cursor};
            ::std::string_view compilation_directory{};
            if(directory_index != sections.compilation_directories.size())
            {
                auto const& entry{sections.compilation_directories[directory_index]};
                if(entry.line_unit_offset < line_unit_offset) { return source_map_error::malformed; }
                if(entry.line_unit_offset == line_unit_offset)
                { compilation_directory = entry.directory; ++directory_index; }
            }
            ::std::uint64_t length{};
            if(!input.fixed(4u, length)) { return source_map_error::malformed; }
            ::std::size_t offset_width{4u};
            if(length == 0xffffffffu)
            {
                offset_width = 8u;
                if(!input.fixed(8u, length)) { return source_map_error::malformed; }
            }
            else if(length >= 0xfffffff0u) { return source_map_error::malformed; }
            if(length == 0u || length > input.bytes.size() - input.cursor)
            { return source_map_error::malformed; }
            // [safe] validated unit length; one unit cannot borrow bytes from
            // the next, and advancing to the next unit may reach section_end.
            source_map_details::reader unit{input.bytes.subspan(input.cursor,
                static_cast<::std::size_t>(length))};
            input.cursor += static_cast<::std::size_t>(length);
            auto const error{parse_unit(result, unit, sections, limits,
                offset_width, compilation_directory, path_bytes, rows)};
            if(error != source_map_error::none) { return error; }
        }
        if(directory_index != sections.compilation_directories.size()) { return source_map_error::malformed; }
        ::std::sort(result.ranges_.begin(), result.ranges_.end(),
            [](range const& a, range const& b) { return a.begin < b.begin; });
        for(::std::size_t i{1u}; i < result.ranges_.size(); ++i)
        {
            if(result.ranges_[i].begin < result.ranges_[i - 1u].end)
            { return source_map_error::ambiguous_ranges; }
        }
        output = ::std::move(result);
        return source_map_error::none;
    }

    inline ::std::optional<source_location> source_map::lookup(::std::uint64_t module,
        ::std::uint64_t code_section_content_offset) const noexcept
    {
        if(module != module_ || code_section_content_offset >= code_size_) { return ::std::nullopt; }
        auto const it{::std::upper_bound(ranges_.begin(), ranges_.end(),
            code_section_content_offset, [](auto offset, range const& item) { return offset < item.begin; })};
        if(it == ranges_.begin()) { return ::std::nullopt; }
        auto const& found{*(it - 1u)};
        if(code_section_content_offset >= found.end || found.line == 0u) { return ::std::nullopt; }
        return source_location{module_, code_section_content_offset,
            files_[found.file], found.line, found.column, found.is_statement, found.discriminator, found.prologue_end, found.epilogue_begin};
    }
}

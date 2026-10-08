/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)           *
 * Copyright (c) 2025-present UlteSoft. All rights reserved.   *
 * Licensed under the APL-2.0 License (see LICENSE file).      *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <utility>
# include <fast_io.h>
# include <fast_io_dsal/array.h>
# include <fast_io_dsal/string.h>
# include <fast_io_dsal/string_view.h>
# include <fast_io_dsal/vector.h>
# include <uwvm2/utils/utf/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::wasip1_state
{
    // These are request/result DATA. None can authorize entry, FD lookup, a
    // current pause, native memory, a checkpoint, a host import or a restore.
    inline constexpr ::std::size_t maximum_rows{64u}, maximum_text_bytes{4096u};
    inline constexpr ::std::size_t maximum_command_bytes{maximum_text_bytes * 2u + 256u};
    inline constexpr ::std::size_t maximum_page_text_bytes{4096u};
    inline constexpr ::std::size_t maximum_entries{4096u}, maximum_environment_bytes{1048576u};
    inline constexpr ::std::size_t maximum_fd_scan{65536u};
    using text = ::fast_io::u8string;
    using text_view = ::fast_io::u8string_view;
    enum class action : unsigned char
    { arguments, environment, descriptors, preopens, replace_argument, set_environment, remove_environment, reduce_rights, insert_argument, remove_argument, trace_enable, trace_disable, trace_clear, trace_read, create_file, duplicate_descriptor, close_descriptor, checkpoint_save, checkpoint_restore, checkpoint_drop, portable_export, portable_import, portable_export_group, portable_import_group };
    enum class status : unsigned char
    {
        ok, invalid_request, requires_llvm_jit_full, requires_current_cooperative_stop,
        not_selected, stale_stop_or_generation, incomplete_cohort, unavailable_foreign_host_state,
        unavailable_environment, unavailable_owned_text, bad_descriptor, changed_descriptor,
        capability_increase, entry_not_found, resource_limit, allocation_failed, unavailable_resource_rollback, native_operation_failed
    };
    enum class descriptor_kind : unsigned char { native_file, native_file_observer, directory, socket, socket_observer };
    struct environment_selection { ::std::uint64_t module{};text rebindings{}; };
    struct request
    {
        action operation{action::arguments};
        ::std::uint64_t module{}, first{}, count{maximum_rows}, descriptor{}, index{};
        ::std::uint64_t expected_base{}, expected_inheriting{}, new_base{}, new_inheriting{};
        text name{}, value{};
        bool strict_resources{};
        ::fast_io::vector<environment_selection> environments{};
    };
    struct text_entry { ::std::uint64_t index{}; text bytes{}; };
    struct descriptor_entry
    {
        ::std::uint64_t descriptor{}, base_rights{}, inheriting_rights{};
        descriptor_kind kind{};
        bool preopened{};
        text guest_preopen_name{};
        ::std::uint64_t managed_resource{};
        // Storage kind is not an OS file type: no fstat/native handle is exposed.
    };
    struct view
    {
        status result{status::requires_current_cooperative_stop};
        action operation{};
        ::std::uint64_t module{}, total_entries{}, next{}, observed_runtime_epoch{};
        bool more{}, mutation_applied{}, shared_environment{};
        ::std::uint64_t affected_descriptor{}, checkpoint_slot{}, managed_resources{}, retained_external_resources{};
        bool checkpoint_operation{}, portable_operation{}, portable_group_operation{};
        ::std::uint64_t environment_count{};
        text diagnostic{};
        ::fast_io::vector<text_entry> strings{};
        ::fast_io::vector<descriptor_entry> descriptors{};
        // Environment identity and capability metadata are detached DATA only.
        // No host pointers, native FD/handle, filesystem root path or ticket.
    };
    [[nodiscard]] inline constexpr ::fast_io::string_view status_text(status value) noexcept
    {
        switch(value)
        {
            case status::ok: return "ok";
            case status::invalid_request: return "invalid request";
            case status::requires_llvm_jit_full: return "requires LLVM JIT full";
            case status::requires_current_cooperative_stop: return "requires current cooperative stop";
            case status::not_selected: return "state observation not selected";
            case status::stale_stop_or_generation: return "stale stop or generation";
            case status::incomplete_cohort: return "incomplete or unauthenticated cohort";
            case status::unavailable_foreign_host_state: return "host activity is busy or untracked";
            case status::unavailable_environment: return "WASIp1 environment unavailable";
            case status::unavailable_owned_text: return "text storage is unowned, ambiguous or over limit";
            case status::bad_descriptor: return "guest descriptor unavailable";
            case status::changed_descriptor: return "descriptor rights changed";
            case status::capability_increase: return "cannot increase descriptor capabilities";
            case status::entry_not_found: return "entry not found";
            case status::resource_limit: return "debug resource limit";
            case status::allocation_failed: return "allocation failed";
            case status::unavailable_resource_rollback: return "resource rollback unavailable";
            case status::native_operation_failed: return "native resource operation failed";
        }
        return "invalid result";
    }
    [[nodiscard]] inline constexpr ::fast_io::string_view kind_text(descriptor_kind value) noexcept
    {
        switch(value)
        {
            case descriptor_kind::native_file: return "file";
            case descriptor_kind::native_file_observer: return "file observer";
            case descriptor_kind::directory: return "directory";
            case descriptor_kind::socket: return "socket";
            case descriptor_kind::socket_observer: return "socket observer";
        }
        return "invalid kind";
    }
    [[nodiscard]] inline constexpr bool is_mutation(action selected) noexcept
    {
        return selected == action::replace_argument || selected == action::set_environment ||
            selected == action::remove_environment || selected == action::reduce_rights ||
            selected == action::insert_argument || selected == action::remove_argument ||
            selected == action::create_file || selected == action::duplicate_descriptor || selected == action::close_descriptor ||
            selected == action::checkpoint_save || selected == action::checkpoint_restore || selected == action::checkpoint_drop || selected == action::portable_export || selected == action::portable_import || selected == action::portable_export_group || selected == action::portable_import_group;
    }
    [[nodiscard]] inline constexpr bool valid_text(text_view bytes, bool key) noexcept
    {
        if(bytes.size() > maximum_text_bytes || (key && bytes.empty())) { return false; }
        for(auto c : bytes) { if(c == u8'\0' || (key && c == u8'=')) { return false; } }
        return true; // Existing WASIp1 UTF-8 policy is unchanged, byte values are preserved.
    }
    // Host checkpoint filenames share one UTF-8 meaning on every OS. Admit
    // them before entropy generation, environment capture or native file IO;
    // argument/environment bytes continue to use valid_text independently.
    [[nodiscard]] inline constexpr bool valid_portable_path(text_view path) noexcept
    {
        if(path.empty() || path.size()>maximum_text_bytes) { return false; }
        return ::uwvm2::utils::utf::check_legal_utf8<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(
            path.cbegin(),path.cend()).err==::uwvm2::utils::utf::utf_error_code::success;
    }
    namespace details
    {
        // One bounded grammar for command admission and detached decoding.
        // Emit only into private candidates: a later error must not publish
        // an already scanned prefix. This helper performs no native IO.
        template<typename Emit>
        [[nodiscard]] inline constexpr bool scan_rebindings(text_view bytes,Emit&& emit)
            noexcept(noexcept(emit(::std::uint32_t{},::std::uint32_t{})))
        {
            if(bytes.size()>maximum_text_bytes) { return false; }
            ::fast_io::array<::std::uint32_t,maximum_rows> seen{};
            ::std::size_t position{},count{};
            while(position<bytes.size())
            {
                ::std::uint32_t values[2u]{};
                for(unsigned field{};field!=2u;++field)
                {
                    auto start=position;
                    while(position<bytes.size() && bytes[position]>=u8'0' && bytes[position]<=u8'9') { ++position; }
                    if(start==position) { return false; }
                    auto parsed=::fast_io::parse_by_scan(bytes.data()+start,bytes.data()+position,
                        ::fast_io::mnp::dec_get<true,true>(values[field]));
                    if(parsed.code!=::fast_io::parse_code::ok || parsed.iter!=bytes.data()+position) { return false; }
                    if(field==0u && (position==bytes.size() || bytes[position++]!=u8'=')) { return false; }
                }
                if(values[0]>=maximum_fd_scan || values[1]>(::std::numeric_limits<::std::int32_t>::max)() || count==maximum_rows)
                { return false; }
                for(::std::size_t i{};i!=count;++i) { if(seen[i]==values[0]) { return false; } }
                seen[count++]=values[0];emit(values[0],values[1]);
                if(position<bytes.size() && (bytes[position++]!=u8',' || position==bytes.size())) { return false; }
            }
            return true;
        }
    }
    [[nodiscard]] inline constexpr bool valid_rebindings(text_view bytes) noexcept
    { return details::scan_rebindings(bytes,[](::std::uint32_t,::std::uint32_t) noexcept {}); }
    [[nodiscard]] inline constexpr bool valid(request const& requested) noexcept
    {
        if(requested.count == 0u || requested.count > maximum_rows) { return false; }
        bool const group{requested.operation==action::portable_export_group || requested.operation==action::portable_import_group};
        if(!group && !requested.environments.empty()) { return false; }
        if(group)
        {
            if(requested.environments.empty() || requested.environments.size()>16u || requested.module!=requested.environments.front().module ||
               !valid_portable_path(text_view{requested.name.data(),requested.name.size()}) || !requested.value.empty()) { return false; }
            ::std::size_t text_bytes{requested.name.size()};
            for(::std::size_t i{};i!=requested.environments.size();++i)
            {
                auto const& row=requested.environments[i];
                if(row.rebindings.size()>maximum_text_bytes-text_bytes || (requested.operation==action::portable_export_group && !row.rebindings.empty())) { return false; }
                text_bytes+=row.rebindings.size();
                if(!valid_rebindings(text_view{row.rebindings.data(),row.rebindings.size()})) { return false; }
                for(::std::size_t j{};j!=i;++j) { if(row.module==requested.environments[j].module) { return false; } }
            }
            return true;
        }
        switch(requested.operation)
        {
            case action::trace_enable:
                if(requested.name.size() > 63u || !requested.value.empty()) { return false; }
                for(char8_t c : requested.name)
                { if(!((c >= u8'a' && c <= u8'z') || (c >= u8'0' && c <= u8'9') || c == u8'_')) { return false; } }
                return true;
            case action::trace_disable: case action::trace_clear: case action::trace_read:
                return requested.name.empty() && requested.value.empty();
            case action::arguments: case action::environment: case action::descriptors: case action::preopens:
                return requested.name.empty() && requested.value.empty();
            case action::replace_argument: case action::insert_argument:
                return requested.index < maximum_entries && requested.name.empty() && valid_text(text_view{requested.value.data(), requested.value.size()}, false);
            case action::remove_argument:
                return requested.index < maximum_entries && requested.name.empty() && requested.value.empty();
            case action::set_environment:
                return valid_text(text_view{requested.name.data(), requested.name.size()}, true) && valid_text(text_view{requested.value.data(), requested.value.size()}, false) &&
                    requested.name.size() < maximum_text_bytes && requested.value.size() <= maximum_text_bytes - 1u - requested.name.size();
            case action::remove_environment:
                return valid_text(text_view{requested.name.data(), requested.name.size()}, true) && requested.value.empty();
            case action::create_file:
                return requested.name.empty() && requested.value.size() <= maximum_text_bytes; // binary bytes, including NUL
            case action::portable_export_group: case action::portable_import_group: return false; // handled above
            case action::portable_export: case action::portable_import:
                return valid_portable_path(text_view{requested.name.data(),requested.name.size()}) &&
                    (requested.operation==action::portable_export ? requested.value.empty() :
                     valid_rebindings(text_view{requested.value.data(),requested.value.size()}));
            case action::checkpoint_save: case action::checkpoint_restore: case action::checkpoint_drop:
                return requested.index < 8u && requested.name.empty() && requested.value.empty();
            case action::duplicate_descriptor: case action::close_descriptor:
            case action::reduce_rights:
                return requested.descriptor <= static_cast<::std::uint64_t>((::std::numeric_limits<::std::int32_t>::max)()) &&
                    requested.name.empty() && requested.value.empty();
        }
        return false;
    }
    [[nodiscard]] inline constexpr bool rights_subset(::std::uint64_t base, ::std::uint64_t inheriting,
        ::std::uint64_t previous_base, ::std::uint64_t previous_inheriting) noexcept
    { return (base & ~previous_base) == 0u && (inheriting & ~previous_inheriting) == 0u; }
    [[nodiscard]] inline constexpr bool environment_key_matches(text_view entry, text_view key) noexcept
    {
        if(entry.size() <= key.size() || entry[key.size()] != u8'=') { return false; }
        for(::std::size_t i{}; i != key.size(); ++i)
        {
            // [bounded key][existing entry with size>key.size()] end
            // [safe] i<key.size() and i<entry.size() before both byte reads.
            if(entry[i] != key[i]) { return false; }
        }
        return true;
    }
    namespace details
    {
        [[nodiscard]] inline bool decimal(::fast_io::string_view bytes, ::std::uint64_t& value) noexcept
        {
            if(bytes.empty()) { return false; }
            ::std::uint64_t candidate{};
            // [input view bytes ...][one-past]
            // [safe] form end only from the bounded string_view extent.
            auto const end{bytes.data() + bytes.size()};
            auto const parsed{::fast_io::parse_by_scan(bytes.data(), end, ::fast_io::mnp::dec_get<true, true>(candidate))};
            if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != end) { return false; }
            value = candidate; return true;
        }
        [[nodiscard]] inline bool rights_mask(::fast_io::string_view bytes, ::std::uint64_t& value) noexcept
        {
            if(bytes.size() < 2u || bytes[0] != '0' || (bytes[1] != 'x' && bytes[1] != 'X')) { return decimal(bytes, value); }
            if(bytes.size() == 2u) { return false; }
            // Prefix extent is proven before subview; the scanner must consume
            // the entire unsigned mask, rejecting signs, overflow and suffixes.
            auto const digits{bytes.subview(2u, bytes.size() - 2u)};
            for(char c : digits) { if(!::fast_io::char_category::is_c_xdigit(c)) { return false; } }
            ::std::uint64_t candidate{};
            auto const end{digits.data() + digits.size()};
            auto const parsed{::fast_io::parse_by_scan(digits.data(), end, ::fast_io::mnp::hex_get<true, true>(candidate))};
            if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != end) { return false; }
            value = candidate; return true;
        }
        [[nodiscard]] inline bool hexadecimal_text(::fast_io::string_view bytes, text& out, bool binary = false) noexcept
        {
            // '-' means empty. Two hex characters represent one opaque byte;
            // syntax cannot inject spaces, escapes or terminal control sequences.
            if(bytes == "-") { out.clear(); return true; }
            if(bytes.empty() || (bytes.size() & 1u) != 0u || bytes.size() > maximum_text_bytes * 2u) { return false; }
            text candidate{}; candidate.reserve(bytes.size() / 2u);
            for(::std::size_t i{}; i != bytes.size(); i += 2u)
            {
                ::std::uint_least16_t byte{};
                // [hex pairs ... i,i+1][one-past]
                // [safe] even size and i<size prove a complete pair; no pointer
                // advance occurs before the remaining-size check above.
                if(!::fast_io::char_category::is_c_xdigit(bytes[i]) || !::fast_io::char_category::is_c_xdigit(bytes[i + 1u])) { return false; }
                auto const begin{bytes.data() + i}; auto const end{begin + 2u};
                auto const parsed{::fast_io::parse_by_scan(begin, end, ::fast_io::mnp::hex_get<true, true>(byte))};
                if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != end || (!binary && byte == 0u) || byte > 255u) { return false; }
                candidate.push_back(static_cast<char8_t>(byte));
            }
            out = ::std::move(candidate); return true;
        }
    }
    // Standalone console grammar; parent command routes only an authenticated
    // management request. Parsing itself cannot read or change a WASIp1 object.
    [[nodiscard]] inline bool parse(::fast_io::string_view line, request& out) noexcept
    {
        if(line.size() > maximum_command_bytes) { return false; }
        ::fast_io::array<::fast_io::string_view, 20u> words{};
        ::std::size_t count{}, cursor{};
        while(cursor < line.size())
        {
            if(::fast_io::char_category::is_c_blank(line[cursor]) || line[cursor] == '\r') { ++cursor; continue; }
            if(count == words.size()) { return false; }
            auto const begin{cursor};
            while(cursor < line.size() && !::fast_io::char_category::is_c_blank(line[cursor]) && line[cursor] != '\r')
            {
                if(!::fast_io::char_category::is_c_graph(static_cast<unsigned char>(line[cursor]))) { return false; }
                ++cursor; // cursor<=size after the bounded read; never an unchecked pointer jump.
            }
            words[count++] = line.subview(begin, cursor - begin);
        }
        request candidate{};
        if(count >= 3u && count <= 5u && words[0] == "trace" && words[1] == "wasip1")
        {
            if(words[2] == "on" && count <= 4u)
            {
                candidate.operation = action::trace_enable;
                if(count == 4u && words[3] != "all") { for(char c : words[3]) { candidate.name.push_back(static_cast<char8_t>(c)); } }
            }
            else if(words[2] == "off" && count == 3u) { candidate.operation = action::trace_disable; }
            else if(words[2] == "clear" && count == 3u) { candidate.operation = action::trace_clear; }
            else if(words[2] == "read")
            {
                candidate.operation = action::trace_read;
                if((count >= 4u && !details::decimal(words[3], candidate.first)) || (count == 5u && !details::decimal(words[4], candidate.count))) { return false; }
            }
            else { return false; }
        }
        else if(count >= 4u && count <= 6u && words[0] == "info" && words[1] == "wasip1")
        {
            if(words[2] == "args") { candidate.operation = action::arguments; }
            else if(words[2] == "env") { candidate.operation = action::environment; }
            else if(words[2] == "fds") { candidate.operation = action::descriptors; }
            else if(words[2] == "preopens") { candidate.operation = action::preopens; }
            else { return false; }
            if(!details::decimal(words[3], candidate.module) || (count >= 5u && !details::decimal(words[4], candidate.first)) ||
                (count == 6u && !details::decimal(words[5], candidate.count))) { return false; }
        }
        else if(count == 6u && words[0] == "set" && words[1] == "wasip1" && (words[2] == "arg" || words[2] == "arg-insert"))
        {
            candidate.operation = words[2] == "arg" ? action::replace_argument : action::insert_argument;
            if(!details::decimal(words[3], candidate.module) || !details::decimal(words[4], candidate.index) ||
                !details::hexadecimal_text(words[5], candidate.value)) { return false; }
        }
        else if(count == 5u && words[0] == "unset" && words[1] == "wasip1" && words[2] == "arg")
        {
            candidate.operation = action::remove_argument;
            if(!details::decimal(words[3], candidate.module) || !details::decimal(words[4], candidate.index)) { return false; }
        }
        else if(count == 6u && words[0] == "set" && words[1] == "wasip1" && words[2] == "env")
        {
            candidate.operation = action::set_environment;
            if(!details::decimal(words[3], candidate.module) || !details::hexadecimal_text(words[4], candidate.name) ||
                !details::hexadecimal_text(words[5], candidate.value)) { return false; }
        }
        else if(count == 5u && words[0] == "unset" && words[1] == "wasip1" && words[2] == "env")
        {
            candidate.operation = action::remove_environment;
            if(!details::decimal(words[3], candidate.module) || !details::hexadecimal_text(words[4], candidate.name)) { return false; }
        }
        else if(count == 5u && words[0] == "set" && words[1] == "wasip1" && words[2] == "file")
        {
            candidate.operation = action::create_file;
            if(!details::decimal(words[3], candidate.module) || !details::hexadecimal_text(words[4], candidate.value, true)) { return false; }
        }
        else if(count == 7u && words[1] == "wasip1" &&
            ((words[0] == "set" && words[2] == "fd-dup") || (words[0] == "unset" && words[2] == "fd")))
        {
            candidate.operation = words[0] == "set" ? action::duplicate_descriptor : action::close_descriptor;
            if(!details::decimal(words[3], candidate.module) || !details::decimal(words[4], candidate.descriptor) ||
               !details::rights_mask(words[5], candidate.expected_base) || !details::rights_mask(words[6], candidate.expected_inheriting)) { return false; }
        }
        else if(count>=5u && count<=20u && words[0]=="set" && words[1]=="wasip1" && (words[2]=="export-group" || words[2]=="import-group"))
        {
            candidate.operation=words[2]=="export-group" ? action::portable_export_group : action::portable_import_group;
            if(!details::hexadecimal_text(words[3],candidate.name)) { return false; }
            for(::std::size_t i{4u};i!=count;++i)
            {
                auto word=words[i];::std::size_t split{};while(split!=word.size() && word[split]!=':') { ++split; }
                environment_selection row{};if(!details::decimal(word.subview(0u,split),row.module)) { return false; }
                if(split!=word.size())
                {
                    if(candidate.operation==action::portable_export_group || split+1u==word.size()) { return false; }
                    for(auto c:word.subview(split+1u,word.size()-split-1u)) { row.rebindings.push_back(static_cast<char8_t>(c)); }
                }
                candidate.environments.push_back(::std::move(row));
            }
            candidate.module=candidate.environments.front().module;
        }
        else if(count >= 5u && count <= 6u && words[0] == "set" && words[1] == "wasip1" && (words[2] == "export" || words[2] == "import"))
        {
            candidate.operation=words[2]=="export" ? action::portable_export : action::portable_import;
            if(!details::decimal(words[3],candidate.module) || !details::hexadecimal_text(words[4],candidate.name)) { return false; }
            if(count==6u)
            { if(candidate.operation==action::portable_export) { return false; }for(auto c:words[5]) { candidate.value.push_back(static_cast<char8_t>(c)); } }
        }
        else if((count == 5u || count == 6u) && words[1] == "wasip1" &&
            ((words[0] == "set" && (words[2] == "checkpoint" || words[2] == "restore")) ||
             (words[0] == "unset" && words[2] == "checkpoint")))
        {
            candidate.operation = words[0] == "unset" ? action::checkpoint_drop :
                words[2] == "restore" ? action::checkpoint_restore : action::checkpoint_save;
            if(!details::decimal(words[3], candidate.module) || !details::decimal(words[4], candidate.index)) { return false; }
            if(candidate.operation == action::checkpoint_restore)
            {
                if(count != 6u || (words[5] != "bindings" && words[5] != "strict")) { return false; }
                candidate.strict_resources = words[5] == "strict";
            }
            else if(count != 5u) { return false; }
        }
        else if(count == 9u && words[0] == "set" && words[1] == "wasip1" && words[2] == "rights")
        {
            candidate.operation = action::reduce_rights;
            if(!details::decimal(words[3], candidate.module) || !details::decimal(words[4], candidate.descriptor) ||
                !details::rights_mask(words[5], candidate.expected_base) || !details::rights_mask(words[6], candidate.expected_inheriting) ||
                !details::rights_mask(words[7], candidate.new_base) || !details::rights_mask(words[8], candidate.new_inheriting)) { return false; }
        }
        else { return false; }
        if(!valid(candidate)) { return false; }
        out = ::std::move(candidate); return true;
    }
    template<typename Output>
    inline void print_escaped(Output output, text_view bytes)
    {
        for(char8_t byte : bytes)
        {
            auto const c{static_cast<unsigned char>(byte)};
            if(::fast_io::char_category::is_c_graph(c) && c != '\\' && c != '"') { ::fast_io::io::print(output, ::fast_io::mnp::chvw(static_cast<char>(c))); }
            else { ::fast_io::io::print(output, "\\x", ::fast_io::mnp::hex<false, true>(c)); }
        }
    }
    template<typename Output>
    inline void print(Output output, view const& copied)
    {
        // Detached view bounds are checked before formatting. Worst expansion is
        // 4 ASCII bytes per raw byte; <=4096 raw bytes and <=64 row labels keep
        // the whole reply below the parent's actual 65536-byte broker cap.
        if(copied.strings.size() > maximum_rows || copied.descriptors.size() > maximum_rows ||
            copied.strings.size() + copied.descriptors.size() > maximum_rows || copied.diagnostic.size()>maximum_text_bytes)
        { ::fast_io::io::print(output, "error: WASIp1 reply resource limit\n"); return; }
        ::std::size_t page_bytes{};
        for(auto const& entry : copied.strings)
        {
            if(entry.bytes.size() > maximum_text_bytes || entry.bytes.size() > maximum_page_text_bytes - page_bytes)
            { ::fast_io::io::print(output, "error: WASIp1 reply resource limit\n"); return; }
            page_bytes += entry.bytes.size(); // Bounded BEFORE addition.
        }
        for(auto const& descriptor : copied.descriptors)
        {
            if(descriptor.guest_preopen_name.size() > maximum_text_bytes || descriptor.guest_preopen_name.size() > maximum_page_text_bytes - page_bytes)
            { ::fast_io::io::print(output, "error: WASIp1 reply resource limit\n"); return; }
            page_bytes += descriptor.guest_preopen_name.size();
        }
        ::fast_io::io::print(output, "wasip1 module=", ::fast_io::mnp::dec(copied.module), " status=", status_text(copied.result),
            " epoch=", ::fast_io::mnp::dec(copied.observed_runtime_epoch), " applied=", copied.mutation_applied,
            " shared-environment=", copied.shared_environment, " total=", ::fast_io::mnp::dec(copied.total_entries), "\n");
        if(copied.portable_group_operation)
        {
            ::fast_io::io::print(output,"wasip1-portable-group environments=",copied.environment_count," resources=",copied.total_entries," format=1 content=external atomic=true\n",
                "reminder: checkpoint Wasm and WASIp1 together at the same cooperative stop; WASIp1-only restore does not restore Wasm state.\n");
            if(!copied.diagnostic.empty()) { ::fast_io::io::print(output,"diagnostic=\"");print_escaped(output,text_view{copied.diagnostic.data(),copied.diagnostic.size()});::fast_io::io::print(output,"\"\n"); }
        }
        if(copied.portable_operation)
        {
            ::fast_io::io::print(output,"wasip1-portable resources=",copied.total_entries," format=1 content=external\n",
                "reminder: checkpoint Wasm and WASIp1 together at the same cooperative stop; WASIp1-only restore does not restore Wasm state.\n");
            if(!copied.diagnostic.empty()) { ::fast_io::io::print(output,"diagnostic=\"");print_escaped(output,text_view{copied.diagnostic.data(),copied.diagnostic.size()});::fast_io::io::print(output,"\"\n"); }
        }
        if(copied.checkpoint_operation)
        {
            ::fast_io::io::print(output, "wasip1-checkpoint slot=", copied.checkpoint_slot, " managed=", copied.managed_resources,
                " retained-external=", copied.retained_external_resources, " external-io-rollback=false\n",
                "reminder: checkpoint Wasm and WASIp1 together at the same cooperative stop; WASIp1-only restore does not restore Wasm state.\n");
        }
        if(copied.mutation_applied && (copied.operation == action::create_file || copied.operation == action::duplicate_descriptor || copied.operation == action::close_descriptor))
        { ::fast_io::io::print(output, "wasip1-fd affected=", copied.affected_descriptor, "\n"); }
        for(auto const& entry : copied.strings)
        {
            ::fast_io::io::print(output, "  [", ::fast_io::mnp::dec(entry.index), "] \"");
            print_escaped(output, text_view{entry.bytes.data(), entry.bytes.size()}); ::fast_io::io::print(output, "\"\n");
        }
        for(auto const& fd : copied.descriptors)
        {
            ::fast_io::io::print(output, "  fd=", ::fast_io::mnp::dec(fd.descriptor), " storage-kind=", kind_text(fd.kind),
                " rights-base=", ::fast_io::mnp::hex0x(fd.base_rights), " rights-inheriting=", ::fast_io::mnp::hex0x(fd.inheriting_rights),
                " preopened=", fd.preopened);
            if(fd.preopened) { ::fast_io::io::print(output, " guest-name=\""); print_escaped(output, text_view{fd.guest_preopen_name.data(), fd.guest_preopen_name.size()}); ::fast_io::io::print(output, "\""); }
            ::fast_io::io::print(output, " managed-resource=", fd.managed_resource, "\n");
        }
        if(copied.more) { ::fast_io::io::print(output, "  more next=", ::fast_io::mnp::dec(copied.next), "\n"); }
    }
}

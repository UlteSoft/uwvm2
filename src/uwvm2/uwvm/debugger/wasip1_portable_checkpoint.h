// Portable WASIp1 metadata. These bytes never grant host resource authority.
#pragma once
#ifndef UWVM_MODULE
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cerrno>
#include <span>
#include <vector>
#include <fast_io.h>
#include <fast_io_crypto.h>
#include <fast_io_dsal/string.h>
#include <fast_io_dsal/string_view.h>
#include <uwvm2/utils/utf/impl.h>
#include "wasip1_state.h"
#endif
#ifndef UWVM_MODULE_EXPORT
#define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::wasip1_portable
{
    using text=::fast_io::u8string;
    using text_view=::fast_io::u8string_view;
    inline constexpr ::std::size_t max_rows{65536u},max_text{4096u},max_strings{4096u},max_bytes{8388608u};
    enum class kind : ::std::uint8_t { directory, file, stdio, external, external_stream };
    struct resource
    {
        kind type{};
        text mount{},path{};
        ::std::uint64_t offset{};
        ::std::uint16_t flags{}; // WASI bits, never native O_* values.
        ::std::uint8_t stdio_index{};
        bool follow{};
        ::std::uint8_t stream_type{};
    };
    struct binding { ::std::uint32_t descriptor{},resource_index{}; ::std::uint64_t base{},inheriting{}; };
    struct empty_binding { ::std::uint32_t descriptor{}; ::std::uint64_t base{},inheriting{}; bool null_resource{}; };
    struct rebind { ::std::uint32_t resource_index{},target_descriptor{}; };
    struct snapshot
    {
        ::std::array<::std::byte,16u> recording_label{};
        ::std::array<::std::byte,32u> original_wasm{},builtin_interface{};
        ::std::uint32_t opens_size{};
        ::std::vector<text> arguments{},environment{};
        ::std::vector<resource> resources{};
        ::std::vector<binding> bindings{};
        ::std::vector<empty_binding> reserved{};
        ::std::vector<::std::uint32_t> closed{};
    };
    [[nodiscard]] inline bool parse_rebindings(text_view text,::std::vector<rebind>& out)
    {
        ::std::vector<rebind> candidate{};
        if(!::uwvm2::uwvm::debugger::wasip1_state::details::scan_rebindings(text,
            [&](::std::uint32_t resource,::std::uint32_t descriptor) { candidate.push_back({resource,descriptor}); })) { return false; }
        out.swap(candidate);return true;
    }
    [[nodiscard]] inline bool valid_text(text const& s) noexcept
    { if(s.size()>max_text) { return false; } for(auto c:s) { if(c==u8'\0') { return false; } } return true; }
    [[nodiscard]] inline bool valid_file_path(text_view path) noexcept
    {
        return ::uwvm2::uwvm::debugger::wasip1_state::valid_portable_path(path);
    }
    [[nodiscard]] inline bool valid(snapshot const& s)
    {
        if(s.opens_size>max_rows || s.resources.size()>max_rows || s.bindings.size()>max_rows ||
           s.reserved.size()>max_rows || s.closed.size()>s.opens_size || s.arguments.size()>max_strings || s.environment.size()>max_strings)
        { return false; }
        // Closed and reserved slots also consume native descriptor scan cells.
        // Bound the whole table before allocating validation/import storage;
        // high renumbered FD values themselves remain valid up to INT32_MAX.
        if(s.bindings.size()+s.reserved.size()+s.closed.size()>max_rows) { return false; }
        bool label{};for(auto b:s.recording_label) { label=label || b!=::std::byte{}; } if(!label) { return false; }
        ::std::size_t bytes{};
        auto charge=[&](text const& t) { if(!valid_text(t) || t.size()+1u>1048576u-bytes) { return false; } bytes+=t.size()+1u;return true; };
        for(auto const& t:s.arguments) { if(!charge(t)) { return false; } }
        for(auto const& t:s.environment) { if(!charge(t)) { return false; } }
        for(auto const& r:s.resources)
        {
            if(static_cast<unsigned>(r.type)>4u || !charge(r.mount) || !charge(r.path) || r.flags>31u || r.offset>INT64_MAX) { return false; }
            if((r.type==kind::directory || r.type==kind::file) && r.mount.empty()) { return false; }
            if(r.type==kind::file && r.path.empty()) { return false; }
            // Guest mount names and reopen paths must keep one UTF-8 meaning
            // on every OS. Reject malformed metadata before encoding or any
            // target mount lookup; argv/environment remain opaque WASI bytes.
            if((r.type==kind::directory || r.type==kind::file) &&
               (!valid_file_path(text_view{r.mount.data(),r.mount.size()}) ||
                (!r.path.empty() && !valid_file_path(text_view{r.path.data(),r.path.size()})))) { return false; }
            if((r.type==kind::external_stream && (r.stream_type>7u || r.stream_type==3u || r.stream_type==4u)) || (r.type!=kind::external_stream && r.stream_type!=0u)) { return false; }
            if((r.type==kind::stdio && r.stdio_index>2u) || (r.type!=kind::stdio && r.stdio_index!=0u) ||
               (r.type!=kind::directory && r.type!=kind::file && r.follow)) { return false; }
            if((r.type==kind::stdio || r.type==kind::external || r.type==kind::external_stream) && (!r.mount.empty() || !r.path.empty())) { return false; }
            if(r.type!=kind::file && r.type!=kind::external && (r.offset!=0u || (r.type!=kind::external_stream && r.type!=kind::directory && r.flags!=0u))) { return false; }
            // Canonical guest relative paths. Reject Windows alternate path syntax
            // on every source OS, so importing cannot change its interpretation.
            if(!r.path.empty())
            {
                if(r.path.front()==u8'/' || r.path.back()==u8'/') { return false; }
                ::std::size_t start{};
                for(::std::size_t i{};i<=r.path.size();++i)
                {
                    if(i<r.path.size() && r.path[i]!=u8'/')
                    { if(r.path[i]==u8'\\' || r.path[i]==u8':') { return false; } continue; }
                    auto n=i-start;
                    if(n==0u || (n==1u && r.path[start]==u8'.') || (n==2u && r.path[start]==u8'.' && r.path[start+1u]==u8'.')) { return false; }
                    start=i+1u;
                }
            }
        }
        ::std::vector<::std::uint32_t> numbers{};numbers.reserve(s.bindings.size()+s.reserved.size()+s.closed.size());
        ::std::vector<bool> used(s.resources.size());
        for(auto const& b:s.bindings)
        {
            if(b.descriptor>INT32_MAX || b.resource_index>=s.resources.size() || ((b.base|b.inheriting)&~::std::uint64_t{0x3fffffffu})!=0u) { return false; }
            numbers.push_back(b.descriptor);used[b.resource_index]=true;
        }
        for(auto const& b:s.reserved)
        { if(b.descriptor>INT32_MAX || ((b.base|b.inheriting)&~::std::uint64_t{0x3fffffffu})!=0u) { return false; } numbers.push_back(b.descriptor); }
        for(auto n:s.closed) { if(n>=s.opens_size) { return false; } numbers.push_back(n); }
        for(bool b:used) { if(!b) { return false; } }
        ::std::sort(numbers.begin(),numbers.end());
        for(::std::size_t i{};i<numbers.size();++i)
        { if((i!=0u && numbers[i]==numbers[i-1u]) || (i<s.opens_size && numbers[i]!=i)) { return false; } }
        return numbers.size()>=s.opens_size;
    }
    namespace detail
    {
        struct writer
        {
            ::std::vector<::std::byte> bytes{};
            template<unsigned Bits,typename T> void put(T value)
            {
                ::std::array<unsigned char,Bits/8u> a{};
                ::fast_io::basic_obuffer_view<unsigned char> out{a.data(),a.data()+a.size()};
                ::fast_io::io::print(out,::fast_io::mnp::le_put<Bits>(static_cast<::std::uint64_t>(value)));
                auto p=reinterpret_cast<::std::byte const*>(a.data());bytes.insert(bytes.end(),p,p+a.size());
            }
            void raw(::std::span<::std::byte const> a) { bytes.insert(bytes.end(),a.begin(),a.end()); }
            void string(text const& t) { put<32u>(t.size());raw({reinterpret_cast<::std::byte const*>(t.data()),t.size()}); }
        };
        struct reader
        {
            ::std::span<::std::byte const> bytes{};::std::size_t pos{};
            template<unsigned Bits,typename T> bool get(T& value)
            {
                if(Bits/8u>bytes.size()-pos) { return false; }
                auto first=reinterpret_cast<unsigned char const*>(bytes.data()+pos);
                auto r=::fast_io::parse_by_scan(first,first+Bits/8u,::fast_io::mnp::le_get<Bits>(value));
                if(r.code!=::fast_io::parse_code::ok || r.iter!=first+Bits/8u) { return false; } pos+=Bits/8u;return true;
            }
            bool raw(::std::span<::std::byte> out)
            { if(out.size()>bytes.size()-pos) { return false; } ::std::copy_n(bytes.begin()+pos,out.size(),out.begin());pos+=out.size();return true; }
            bool string(text& t)
            {
                ::std::uint32_t n{};if(!get<32u>(n) || n>max_text || n>bytes.size()-pos) { return false; }
                t.assign(text_view{reinterpret_cast<char8_t const*>(bytes.data()+pos),n});pos+=n;return true;
            }
        };
        inline auto digest(::std::span<::std::byte const> b)
        {
            ::std::array<::std::byte,32u> result{};::fast_io::sha256_context hash{};
            hash.update(b.data(),b.data()+b.size());hash.do_final();hash.digest_to_byte_ptr(result.data());return result;
        }
    }
    [[nodiscard]] inline bool encode(snapshot const& s,::std::vector<::std::byte>& output)
    {
        if(!valid(s)) { return false; } detail::writer w{};
        w.put<64u>(::std::uint64_t{0x0031504953505755u}); // UWPSIP1\0
        w.put<32u>(1u);w.put<32u>(0u); // version, reserved; content is always external
        w.raw(s.recording_label);w.raw(s.original_wasm);w.raw(s.builtin_interface);w.put<32u>(s.opens_size);
        for(auto n:{s.arguments.size(),s.environment.size(),s.resources.size(),s.bindings.size(),s.reserved.size(),s.closed.size()}) { w.put<32u>(n); }
        for(auto const& t:s.arguments) { w.string(t); } for(auto const& t:s.environment) { w.string(t); }
        for(auto const& r:s.resources)
        { w.put<8u>(static_cast<::std::uint8_t>(r.type));w.put<8u>(r.stdio_index);w.put<8u>(r.follow);w.put<8u>(r.stream_type);w.put<16u>(r.flags);w.put<16u>(0u);w.put<64u>(r.offset);w.string(r.mount);w.string(r.path); }
        for(auto const& b:s.bindings) { w.put<32u>(b.descriptor);w.put<32u>(b.resource_index);w.put<64u>(b.base);w.put<64u>(b.inheriting); }
        for(auto const& b:s.reserved) { w.put<32u>(b.descriptor);w.put<64u>(b.base);w.put<64u>(b.inheriting);w.put<8u>(b.null_resource); }
        for(auto n:s.closed) { w.put<32u>(n); }
        w.raw(detail::digest(w.bytes));if(w.bytes.size()>max_bytes) { return false; } output.swap(w.bytes);return true;
    }
    [[nodiscard]] inline bool decode(::std::span<::std::byte const> bytes,snapshot& output)
    {
        if(bytes.size()<156u || bytes.size()>max_bytes) { return false; }
        auto body=bytes.first(bytes.size()-32u);auto hash=detail::digest(body);
        if(!::std::equal(hash.begin(),hash.end(),bytes.end()-32u)) { return false; }
        detail::reader r{body};::std::uint64_t magic{};::std::uint32_t version{},zero{};snapshot s{};
        if(!r.get<64u>(magic) || magic!=0x0031504953505755u || !r.get<32u>(version) || version!=1u || !r.get<32u>(zero) || zero!=0u ||
           !r.raw(s.recording_label) || !r.raw(s.original_wasm) || !r.raw(s.builtin_interface) || !r.get<32u>(s.opens_size)) { return false; }
        ::std::uint32_t counts[6u]{};for(auto& n:counts) { if(!r.get<32u>(n) || n>max_rows) { return false; } }
        if(s.opens_size>max_rows || counts[0]>max_strings || counts[1]>max_strings || counts[5]>s.opens_size ||
           counts[3]+counts[4]+counts[5]>max_rows) { return false; }
        // Each row has a minimum width. Reject impossible counts BEFORE allocation.
        ::std::uint64_t minimum=4ull*(counts[0]+counts[1])+24ull*counts[2]+24ull*counts[3]+21ull*counts[4]+4ull*counts[5];
        if(minimum>body.size()-r.pos) { return false; }
        s.arguments.resize(counts[0]);s.environment.resize(counts[1]);s.resources.resize(counts[2]);s.bindings.resize(counts[3]);s.reserved.resize(counts[4]);s.closed.resize(counts[5]);
        for(auto& t:s.arguments) { if(!r.string(t)) { return false; } } for(auto& t:s.environment) { if(!r.string(t)) { return false; } }
        for(auto& a:s.resources)
        {
            ::std::uint8_t type{},follow{};::std::uint16_t z16{};
            if(!r.get<8u>(type) || !r.get<8u>(a.stdio_index) || !r.get<8u>(follow) || follow>1u || !r.get<8u>(a.stream_type) ||
               !r.get<16u>(a.flags) || !r.get<16u>(z16) || z16!=0u || !r.get<64u>(a.offset) || !r.string(a.mount) || !r.string(a.path)) { return false; }
            a.type=static_cast<kind>(type);a.follow=follow!=0u;
        }
        for(auto& b:s.bindings) { if(!r.get<32u>(b.descriptor) || !r.get<32u>(b.resource_index) || !r.get<64u>(b.base) || !r.get<64u>(b.inheriting)) { return false; } }
        for(auto& b:s.reserved)
        { ::std::uint8_t n{};if(!r.get<32u>(b.descriptor) || !r.get<64u>(b.base) || !r.get<64u>(b.inheriting) || !r.get<8u>(n) || n>1u) { return false; } b.null_resource=n!=0u; }
        for(auto& n:s.closed) { if(!r.get<32u>(n)) { return false; } }
        if(r.pos!=body.size() || !valid(s)) { return false; } output=::std::move(s);return true;
    }
    // Publishes one completely written, checked-closed file without replacing
    // any target. Same-directory hard linking is the atomic commit point.
    // The destination filesystem must support native hard links.
    inline void write_exclusive_file(text const& path,::std::span<::std::byte const> bytes)
    {
        ::std::size_t split{};
        for(::std::size_t i{};i!=path.size();++i)
        {
            if(path[i]==u8'/'
#if defined(_WIN32) && !defined(__CYGWIN__)
                || path[i]==u8'\\'
#endif
            ) { split=i+1u; }
        }
#if defined(_WIN32) && !defined(__CYGWIN__)
        // Drive-relative C:filename keeps the drive prefix in the parent.
        if(split==0u && path.size()>=2u && path[1u]==u8':') { split=2u; }
#endif
        text_view const leaf_view{path.data()+split,path.size()-split};
        if(leaf_view.empty() || leaf_view==u8"." || leaf_view==u8"..") { ::fast_io::throw_posix_error(EINVAL); }
        text const leaf{leaf_view};
        text const parent_path{split==0u ? text_view{u8"."} : text_view{path.data(),split}};
        // Pin both publication and cleanup to the originally selected directory.
        ::fast_io::u8native_file parent{parent_path,::fast_io::open_mode::in|::fast_io::open_mode::directory|::fast_io::open_mode::follow|::fast_io::open_mode::shared_delete};
        ::fast_io::native_white_hole entropy{};::std::uint_least64_t random[2u]{};
        auto* first{reinterpret_cast<::std::byte*>(random)};
        ::fast_io::operations::read_all_bytes(entropy,first,first+sizeof(random));
        auto const name{::fast_io::u8concat_fast_io(u8".uwvm-checkpoint-",::fast_io::mnp::hex(random[0u]),u8"-",::fast_io::mnp::hex(random[1u]))};
        struct staging_owner
        {
            ::fast_io::u8native_file const& parent;
            text const& name;
            ::fast_io::u8native_file directory{};
            ::fast_io::posix_file_status identity{};
            bool identified{},payload_created{};
            ~staging_owner()
            {
                // This never looks up, removes or rewrites the final target.
                // Failure after publication cannot revoke a successful commit.
                if(static_cast<bool>(directory))
                {
                    if(payload_created)
                    { try { ::fast_io::native_unlinkat(::fast_io::at(directory),u8"payload"); } catch(...) {} }
                    try { directory.close(); } catch(...) {}
                }
                if(identified)
                {
                    try
                    {
                        auto const current=::fast_io::native_fstatat(::fast_io::at(parent),name);
                        if(current.type==::fast_io::file_type::directory && current.dev==identity.dev && current.ino==identity.ino)
                        { ::fast_io::native_unlinkat(::fast_io::at(parent),name,::fast_io::native_at_flags::removedir); }
                    }
                    catch(...) {} // Orphans stay private; preserve the original result/error.
                }
            }
        } stage{parent,name};
        // A failed mkdir acquires no ownership, including name collisions.
        ::fast_io::native_mkdirat(::fast_io::at(parent),name,static_cast<::fast_io::perms>(0700));
        stage.identity=::fast_io::native_fstatat(::fast_io::at(parent),name);
        stage.identified=stage.identity.type==::fast_io::file_type::directory && stage.identity.ino!=0u;
        stage.directory=::fast_io::u8native_file{::fast_io::at(parent),name,::fast_io::open_mode::in|::fast_io::open_mode::directory|::fast_io::open_mode::shared_delete};
        ::fast_io::native_file file{::fast_io::at(stage.directory),u8"payload",::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl,static_cast<::fast_io::perms>(0600)};
        stage.payload_created=true;
        try
        {
            ::fast_io::operations::write_all_bytes(file,bytes.data(),bytes.data()+bytes.size());
            file.close(); // Deferred payload I/O errors prevent publication.
        }
        catch(...)
        {
            try { if(static_cast<bool>(file)) { file.close(); } } catch(...) {}
            throw;
        }
        // FastIO POSIX linkat and NT FileLinkInformation both reject an
        // existing destination; no check-then-rename or overwrite fallback.
        ::fast_io::native_linkat(::fast_io::at(stage.directory),u8"payload",::fast_io::at(parent),leaf);
    }
    inline bool save_file(snapshot const& s,text_view path)
    {
        if(!valid_file_path(path)) { return false; }
        ::std::vector<::std::byte> bytes{};if(!encode(s,bytes)) { return false; }
        text owned_path{path};write_exclusive_file(owned_path,bytes);return true;
    }
    inline bool load_file(text_view path,snapshot& out)
    {
        if(!valid_file_path(path)) { return false; }
        text owned_path{path};
        auto mode=::fast_io::open_mode::in | ::fast_io::open_mode::no_write_attributes;
#if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__))
        // Reject FIFOs/devices without blocking before the type check.
        mode|=::fast_io::open_mode::no_block;
#endif
        ::fast_io::native_file file{owned_path,mode};auto stat=::fast_io::status(file);
        if(stat.type!=::fast_io::file_type::regular || stat.size<156u || stat.size>max_bytes) { return false; }
        ::std::vector<::std::byte> bytes(static_cast<::std::size_t>(stat.size));
        ::fast_io::operations::read_all_bytes(file,bytes.data(),bytes.data()+bytes.size());
        ::std::byte tail{};if(::fast_io::operations::read_some_bytes(file,&tail,&tail+1u)!=&tail) { return false; }
        // Finish fallible native I/O before publishing decoded output.
        file.close();return decode(bytes,out);
    }

    // Ordered, detached WASIp1 metadata only. Target modules and mount roots
    // are selected afresh on import; no source host identities are persisted.
    inline constexpr ::std::size_t max_environments{16u}, max_group_bytes{16777216u};
    struct group_snapshot { ::std::vector<snapshot> environments{}; };
    [[nodiscard]] inline bool valid_group(group_snapshot const& group)
    {
        if(group.environments.empty() || group.environments.size()>max_environments) { return false; }
        ::std::size_t descriptors{},text_bytes{};
        auto charge=[&](auto const& value)
        { if(value.size()>=1048576u-text_bytes) { return false; }text_bytes+=value.size()+1u;return true; };
        for(auto const& s:group.environments)
        {
            if(!valid(s) || s.recording_label!=group.environments.front().recording_label || s.bindings.size()>max_rows-descriptors) { return false; }
            descriptors+=s.bindings.size();if(s.reserved.size()>max_rows-descriptors) { return false; }descriptors+=s.reserved.size();
            for(auto const& t:s.arguments) { if(!charge(t)) { return false; } }
            for(auto const& t:s.environment) { if(!charge(t)) { return false; } }
            for(auto const& r:s.resources) { if(!charge(r.mount) || !charge(r.path)) { return false; } }
        }
        return true;
    }
    [[nodiscard]] inline bool encode_group(group_snapshot const& group,::std::vector<::std::byte>& output)
    {
        if(!valid_group(group)) { return false; }detail::writer w{};
        w.put<64u>(::std::uint64_t{0x0031504753575755u}); // UWWSGP1\0
        w.put<32u>(1u);w.put<32u>(0u);w.put<32u>(group.environments.size());
        for(auto const& s:group.environments)
        {
            ::std::vector<::std::byte> bytes{};if(!encode(s,bytes) || bytes.size()+4u>max_group_bytes-32u-w.bytes.size()) { return false; }
            w.put<32u>(bytes.size());w.raw(bytes);
        }
        w.raw(detail::digest(w.bytes));output.swap(w.bytes);return true;
    }
    [[nodiscard]] inline bool decode_group(::std::span<::std::byte const> bytes,group_snapshot& output)
    {
        if(bytes.size()<212u || bytes.size()>max_group_bytes) { return false; }
        auto body=bytes.first(bytes.size()-32u);auto hash=detail::digest(body);
        if(!::std::equal(hash.begin(),hash.end(),bytes.end()-32u)) { return false; }
        detail::reader r{body};::std::uint64_t magic{};::std::uint32_t version{},zero{},count{};
        if(!r.get<64u>(magic) || magic!=0x0031504753575755u || !r.get<32u>(version) || version!=1u ||
           !r.get<32u>(zero) || zero!=0u || !r.get<32u>(count) || count==0u || count>max_environments ||
           count>(body.size()-r.pos)/160u) { return false; }
        group_snapshot group{};group.environments.reserve(count);
        for(::std::uint32_t i{};i!=count;++i)
        {
            ::std::uint32_t size{};if(!r.get<32u>(size) || size<156u || size>max_bytes || size>body.size()-r.pos) { return false; }
            snapshot s{};if(!decode(body.subspan(r.pos,size),s)) { return false; }r.pos+=size;
            group.environments.push_back(::std::move(s));
            // Aggregate quotas are checked after EACH bounded nested snapshot,
            // before the next allocation; corrupt input never replaces output.
            if(!valid_group(group)) { return false; }
        }
        if(r.pos!=body.size()) { return false; }output=::std::move(group);return true;
    }
    inline bool save_group_file(group_snapshot const& group,text_view path)
    {
        if(!valid_file_path(path)) { return false; }::std::vector<::std::byte> bytes{};if(!encode_group(group,bytes)) { return false; }
        text owned_path{path};write_exclusive_file(owned_path,bytes);return true;
    }
    inline bool load_group_file(text_view path,group_snapshot& out)
    {
        if(!valid_file_path(path)) { return false; }text owned_path{path};auto mode=::fast_io::open_mode::in | ::fast_io::open_mode::no_write_attributes;
#if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__))
        mode|=::fast_io::open_mode::no_block;
#endif
        ::fast_io::native_file file{owned_path,mode};auto stat=::fast_io::status(file);
        if(stat.type!=::fast_io::file_type::regular || stat.size<212u || stat.size>max_group_bytes) { return false; }
        ::std::vector<::std::byte> bytes(static_cast<::std::size_t>(stat.size));::fast_io::operations::read_all_bytes(file,bytes.data(),bytes.data()+bytes.size());
        ::std::byte tail{};if(::fast_io::operations::read_some_bytes(file,&tail,&tail+1u)!=&tail) { return false; }
        file.close();return decode_group(bytes,out);
    }
}

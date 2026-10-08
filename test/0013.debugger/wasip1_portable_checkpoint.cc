#include <uwvm2/uwvm/debugger/wasip1_portable_checkpoint.h>
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <fast_io.h>
namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;
namespace ws=::uwvm2::uwvm::debugger::wasip1_state;
static unsigned checks{};
static void require(bool b,char const* message)
{ ++checks;if(!b) { ::fast_io::io::perrln("portable checkpoint: ",::fast_io::mnp::os_c_str(message));::fast_io::fast_terminate(); } }
int main(int argc,char** argv)
{
    pp::snapshot s{};s.recording_label[0]=::std::byte{0x12};s.original_wasm[0]=::std::byte{0x34};s.builtin_interface[0]=::std::byte{0x56};
    s.opens_size=8u;s.arguments.emplace_back(u8"guest");s.environment.emplace_back(u8"KEY=value");
    s.resources.push_back({pp::kind::stdio,{}, {},0u,0u,1u,false});
    s.resources.push_back({pp::kind::directory,pp::text{u8"/assets"}});
    s.resources.push_back({pp::kind::file,pp::text{u8"/assets"},pp::text{u8"save/state.bin"},128u});
    s.bindings={{1u,0u,64u,0u},{3u,1u,0x3fffffffu,0x3fffffffu},{7u,2u,0x60006eu,0u},{99u,2u,2u,0u}};
    s.reserved={{0u,0u,0u,true}};s.closed={2u,4u,5u,6u};
    require(pp::valid(s),"canonical graph, high renumber, shared file and non-sorted free list");
    ::std::vector<::std::byte> bytes{};require(pp::encode(s,bytes),"encode canonical LE");
    pp::snapshot decoded{};require(pp::decode(bytes,decoded),"decode metadata");
    ::std::vector<::std::byte> again{};require(pp::encode(decoded,again) && bytes==again,"canonical re-encoding");
    require(decoded.resources[2].offset==128u && decoded.bindings[3].resource_index==2u && decoded.closed==s.closed,"cursor, alias and allocator order");
    for(::std::size_t n{};n<bytes.size();++n)
    {
        auto broken=bytes;broken[n]^=::std::byte{0x80};pp::snapshot target=s;
        require(!pp::decode(broken,target) && target.opens_size==8u,"every wire byte authenticated and failed output untouched");
    }
    for(::std::size_t n{};n<bytes.size();++n) { pp::snapshot target{};require(!pp::decode({bytes.data(),n},target),"every truncation rejected"); }
    auto bad=s;bad.bindings[1].descriptor=1u;require(!pp::valid(bad),"duplicate FD rejected");
    bad=s;bad.closed[0]=7u;require(!pp::valid(bad),"live FD cannot be free");
    bad=s;bad.bindings[2].resource_index=100u;require(!pp::valid(bad),"resource index bounded");
    bad=s;bad.reserved.clear();require(!pp::valid(bad),"reserved hole cannot silently become allocatable");
    for(auto path:{u8"../outside",u8"/absolute",u8"a/../b",u8"a//b",u8"a/",u8"a\\b",u8"a:stream"})
    { bad=s;bad.resources[2].path=pp::text{pp::text_view{path,::fast_io::cstr_len(path)}};require(!pp::valid(bad),"non-portable path rejected"); }
    bad=s;bad.resources[2].flags=32u;require(!pp::valid(bad),"unknown WASI flags rejected");
    bad=s;bad.bindings[0].base=::std::uint64_t{1u}<<63u;require(!pp::valid(bad),"unknown rights rejected");
    // Resource names are portable UTF-8; argv/env are opaque WASI bytes.
    auto rejects_text=[&](pp::text_view invalid)
    {
        for(unsigned field{};field!=2u;++field)
        {
            auto invalid_snapshot=s;
            auto& value=field==0u ? invalid_snapshot.resources[2].mount : invalid_snapshot.resources[2].path;
            value.assign(invalid);
            require(!pp::valid(invalid_snapshot),"malformed resource UTF-8 refused before lookup");
            auto sentinel=bytes;
            require(!pp::encode(invalid_snapshot,sentinel) && sentinel==bytes,"refused encode leaves bytes unchanged");
            pp::group_snapshot invalid_group{{s,invalid_snapshot}};
            require(!pp::valid_group(invalid_group),"later invalid resource rejects whole group");
            require(!pp::encode_group(invalid_group,sentinel) && sentinel==bytes,"refused group encode leaves bytes unchanged");
        }
    };
    for(unsigned n{0x80u};n!=0x100u;++n)
    { char8_t invalid[]{static_cast<char8_t>(n)};rejects_text(pp::text_view{invalid,1u}); }
    for(auto invalid:{u8"\xc0\xaf",u8"\xc1\xbf",u8"\xe0\x80\xaf",u8"\xed\xa0\x80",u8"\xed\xbf\xbf",u8"\xf0\x80\x80\xaf",u8"\xf4\x90\x80\x80",u8"\xf5\x80\x80\x80",u8"\xe2\x82",u8"\xf0\x9f\x98"})
    { rejects_text(pp::text_view{invalid,::fast_io::cstr_len(invalid)}); }
    for(auto position:{0u,15u,16u,31u,32u,63u,64u,127u,128u,255u,256u,4095u})
    {
        pp::text invalid{};for(unsigned i{};i!=4096u;++i) { invalid.push_back(u8'a'); }
        invalid[position]=static_cast<char8_t>(0xffu);rejects_text(pp::text_view{invalid.data(),invalid.size()});
    }
    auto boundary_unicode=s;boundary_unicode.resources[2].path.clear();
    for(unsigned i{};i!=15u;++i) { boundary_unicode.resources[2].path.push_back(u8'a'); }
    boundary_unicode.resources[2].path.append(u8"€");
    while(boundary_unicode.resources[2].path.size()!=4096u) { boundary_unicode.resources[2].path.push_back(u8'a'); }
    require(pp::valid(boundary_unicode) && pp::encode(boundary_unicode,again) && pp::decode(again,decoded) &&
        decoded.resources[2].path==boundary_unicode.resources[2].path,"valid Unicode crossing SIMD boundary at maximum path length round-trips");
    auto unicode_snapshot=s;unicode_snapshot.resources[1].mount=pp::text{u8"/挂载-λ-😀"};
    unicode_snapshot.resources[2].mount=unicode_snapshot.resources[1].mount;
    unicode_snapshot.resources[2].path=pp::text{u8"目录/状态-λ-😀.bin"};
    require(pp::valid(unicode_snapshot) && pp::encode(unicode_snapshot,again) && pp::decode(again,decoded) &&
        decoded.resources[2].path==unicode_snapshot.resources[2].path && decoded.resources[1].mount==unicode_snapshot.resources[1].mount,
        "two three four byte Unicode resource names round-trip");
    auto raw_text=s;raw_text.arguments[0].push_back(static_cast<char8_t>(0xffu));raw_text.environment[0].push_back(static_cast<char8_t>(0x80u));
    require(pp::valid(raw_text) && pp::encode(raw_text,again) && pp::decode(again,decoded) &&
        decoded.arguments[0]==raw_text.arguments[0] && decoded.environment[0]==raw_text.environment[0],"argv/env retain opaque bytes");
    auto locate=[&](pp::text_view needle)
    {
        auto first=reinterpret_cast<::std::byte const*>(needle.data());
        auto found=::std::search(bytes.cbegin(),bytes.cend()-32u,first,first+needle.size());
        require(found!=bytes.cend()-32u,"resource text located in real wire format");return static_cast<::std::size_t>(found-bytes.cbegin());
    };
    auto resign=[](::std::vector<::std::byte>& wire)
    { auto hash=pp::detail::digest({wire.data(),wire.size()-32u});::std::copy(hash.begin(),hash.end(),wire.end()-32u); };
    auto unchanged=[&](pp::snapshot const& target)
    { ::std::vector<::std::byte> sentinel{};return pp::encode(target,sentinel) && sentinel==bytes; };
    auto invalid_wire=bytes;invalid_wire[locate(u8"save/state.bin")]=::std::byte{0xffu};resign(invalid_wire);
    pp::snapshot untouched=s;require(!pp::decode(invalid_wire,untouched) && unchanged(untouched),"valid checksum cannot authorize malformed path or replace output");
    auto invalid_mount_wire=bytes;invalid_mount_wire[locate(u8"/assets")]=::std::byte{0x80u};resign(invalid_mount_wire);
    require(!pp::decode(invalid_mount_wire,untouched) && unchanged(untouched),"valid checksum cannot authorize malformed mount or replace output");
    pp::group_snapshot group{{s,s}};::std::vector<::std::byte> group_bytes{};
    require(pp::encode_group(group,group_bytes),"canonical two-environment group encodes");
    auto invalid_group_wire=group_bytes;auto second=28u+bytes.size();
    require(invalid_group_wire.size()==second+bytes.size()+32u,"real nested group framing");
    invalid_group_wire[second+locate(u8"save/state.bin")]=::std::byte{0xffu};
    auto nested_hash=pp::detail::digest({invalid_group_wire.data()+second,bytes.size()-32u});
    ::std::copy(nested_hash.begin(),nested_hash.end(),invalid_group_wire.begin()+second+bytes.size()-32u);resign(invalid_group_wire);
    pp::group_snapshot untouched_group=group;
    require(!pp::decode_group(invalid_group_wire,untouched_group) && pp::encode_group(untouched_group,again) && again==group_bytes,
        "valid nested and outer hashes cannot partially decode invalid later environment");
    // A sparse high FD consumes one scan cell, rather than its numeric value.
    // The dense prefix includes closed and reserved cells even when few FDs live.
    pp::snapshot boundary{};boundary.recording_label=s.recording_label;boundary.opens_size=pp::max_rows-1u;
    boundary.resources.push_back({pp::kind::external});boundary.bindings.push_back({INT32_MAX,0u,0u,0u});
    for(::std::uint32_t n{};n!=boundary.opens_size;++n) { boundary.closed.push_back(n); }
    ::std::vector<::std::byte> oversized_wire{},oversized_group_wire{};
    auto put32=[](::std::vector<::std::byte>& wire,::std::size_t at,::std::uint32_t value)
    { pp::detail::writer word{};word.put<32u>(value);::std::copy(word.bytes.begin(),word.bytes.end(),wire.begin()+at); };
    for(unsigned with_reserved{};with_reserved!=2u;++with_reserved)
    {
        if(with_reserved) { boundary.closed.erase(boundary.closed.begin());boundary.reserved.push_back({0u,0u,0u,true}); }
        ::std::vector<::std::byte> boundary_wire{};
        require(pp::valid(boundary) && pp::encode(boundary,boundary_wire) && pp::decode(boundary_wire,decoded) &&
            decoded.bindings.front().descriptor==INT32_MAX && decoded.closed==boundary.closed && decoded.reserved.size()==boundary.reserved.size(),
            "exactly 65536 scan cells with sparse INT32_MAX FD and closed/reserved slots round-trip");
        auto oversized=boundary;++oversized.opens_size;oversized.closed.push_back(pp::max_rows-1u);
        require(!pp::valid(oversized),"closed live and reserved cells jointly exceed native scan budget");
        auto sentinel=bytes;require(!pp::encode(oversized,sentinel) && sentinel==bytes,"oversized table encode preserves output");
        pp::group_snapshot oversized_group{{s,oversized}};
        require(!pp::valid_group(oversized_group) && !pp::encode_group(oversized_group,sentinel) && sentinel==bytes,
            "oversized later environment rejects whole group encode");
        // Extend authentic boundary wire with one closed cell and correct hashes.
        oversized_wire=boundary_wire;put32(oversized_wire,112u,oversized.opens_size);
        put32(oversized_wire,136u,static_cast<::std::uint32_t>(oversized.closed.size()));
        pp::detail::writer extra{};extra.put<32u>(pp::max_rows-1u);
        oversized_wire.insert(oversized_wire.end()-32u,extra.bytes.begin(),extra.bytes.end());resign(oversized_wire);
        untouched=s;require(!pp::decode(oversized_wire,untouched) && unchanged(untouched),
            "authentic checksum cannot authorize oversized scan table or replace decoded output");
        pp::group_snapshot boundary_group{{s,boundary}};::std::vector<::std::byte> boundary_group_wire{};
        require(pp::encode_group(boundary_group,boundary_group_wire),"bounded sparse table also encodes in group");
        oversized_group_wire=boundary_group_wire;auto offset=28u+bytes.size();
        put32(oversized_group_wire,offset-4u,static_cast<::std::uint32_t>(oversized_wire.size()));
        oversized_group_wire.erase(oversized_group_wire.begin()+offset,oversized_group_wire.end()-32u);
        oversized_group_wire.insert(oversized_group_wire.end()-32u,oversized_wire.begin(),oversized_wire.end());resign(oversized_group_wire);
        untouched_group=group;
        require(!pp::decode_group(oversized_group_wire,untouched_group) && pp::encode_group(untouched_group,again) && again==group_bytes,
            "oversized later environment with authentic nested/outer checksums rejects group atomically");
    }
    auto all_closed=boundary;all_closed.resources.clear();all_closed.bindings.clear();all_closed.reserved.clear();
    all_closed.opens_size=pp::max_rows;all_closed.closed.clear();
    for(::std::uint32_t n{};n!=pp::max_rows;++n) { all_closed.closed.push_back(n); }
    require(pp::valid(all_closed) && pp::encode(all_closed,again) && pp::decode(again,decoded) && decoded.closed==all_closed.closed,
        "entire 65536-cell closed table remains valid");
    // A reserved null-resource FD can be moved by real WASI fd_renumber.
    // Its numeric value does not expand the dense prefix or the scan budget.
    pp::snapshot high_reserved{};high_reserved.recording_label=s.recording_label;
    high_reserved.opens_size=2u;high_reserved.closed={1u,0u};
    for(unsigned constructed{};constructed!=2u;++constructed)
    {
        high_reserved.reserved={{INT32_MAX,17u,19u,constructed!=0u}};
        ::std::vector<::std::byte> high_wire{},high_again{};
        require(pp::valid(high_reserved) && pp::encode(high_reserved,high_wire) && pp::decode(high_wire,decoded),
            "sparse INT32_MAX reserved cell encodes and decodes with either resource construction state");
        require(decoded.reserved.size()==1u && decoded.reserved.front().descriptor==INT32_MAX &&
            decoded.reserved.front().base==17u && decoded.reserved.front().inheriting==19u &&
            decoded.reserved.front().null_resource==(constructed!=0u) && decoded.closed==high_reserved.closed &&
            pp::encode(decoded,high_again) && high_again==high_wire,"sparse reserved rights null state and free-list order round-trip");
        auto invalid=high_reserved;invalid.reserved.front().descriptor=::std::uint32_t{INT32_MAX}+1u;
        require(!pp::valid(invalid),"reserved descriptor outside signed WASI FD range rejected");
        auto forged=high_wire;put32(forged,140u,::std::uint32_t{INT32_MAX}+1u);resign(forged);
        untouched=s;require(!pp::decode(forged,untouched) && unchanged(untouched),
            "authenticated out-of-range sparse reserved record cannot replace decoded output");
        invalid=high_reserved;invalid.reserved.front().descriptor=0u;
        require(!pp::valid(invalid),"reserved descriptor cannot overlap a closed dense cell");
        invalid=high_reserved;invalid.reserved.push_back(invalid.reserved.front());
        require(!pp::valid(invalid),"duplicate sparse reserved identity rejected");
        invalid=high_reserved;invalid.resources.push_back({pp::kind::external});invalid.bindings.push_back({INT32_MAX,0u,0u,0u});
        require(!pp::valid(invalid),"live binding cannot collide with sparse reserved cell");
        invalid=high_reserved;invalid.closed.pop_back();
        require(!pp::valid(invalid),"high reserved FD does not fill a missing dense-prefix hole");
        invalid=high_reserved;invalid.closed.front()=INT32_MAX;
        require(!pp::valid(invalid),"closed free-list entry remains restricted to dense prefix");
        pp::group_snapshot high_group{{s,high_reserved}},high_decoded{};::std::vector<::std::byte> high_group_wire{};
        require(pp::encode_group(high_group,high_group_wire) && pp::decode_group(high_group_wire,high_decoded) &&
            high_decoded.environments.back().reserved.front().descriptor==INT32_MAX,
            "sparse reserved metadata persists in actual two-environment group framing");
        if(argc>=2)
        {
            auto high_path=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])),u8"/sparse-reserved-",constructed,u8".uwp");
            require(pp::save_file(high_reserved,pp::text_view{high_path.data(),high_path.size()}),"FastIO persistent sparse reserved save");
            pp::snapshot high_loaded{};
            require(pp::load_file(pp::text_view{high_path.data(),high_path.size()},high_loaded) &&
                pp::encode(high_loaded,high_again) && high_again==high_wire,"FastIO persistent sparse reserved canonical round-trip");
        }
    }
    high_reserved.opens_size=pp::max_rows-1u;high_reserved.closed.clear();
    for(::std::uint32_t n{};n!=high_reserved.opens_size;++n) { high_reserved.closed.push_back(n); }
    require(pp::valid(high_reserved) && pp::encode(high_reserved,again) && pp::decode(again,decoded),
        "65536 scan cells allow one sparse reserved INT32_MAX and 65535 dense free cells");
    ++high_reserved.opens_size;high_reserved.closed.push_back(pp::max_rows-1u);
    auto high_sentinel=bytes;
    require(!pp::valid(high_reserved) && !pp::encode(high_reserved,high_sentinel) && high_sentinel==bytes,
        "sparse reserved slot remains charged to scan budget at 65537 cells");
    ::std::vector<pp::rebind> bindings{};
    require(pp::parse_rebindings(u8"2=7,3=101",bindings) && bindings.size()==2u,"explicit bindings parse");
    for(auto text:{u8"2=7,2=8",u8"2=-1",u8"2=2147483648",u8"65536=7",u8"2=7,",u8"2=7junk",u8"=7"})
    { require(!pp::parse_rebindings(pp::text_view{text,::fast_io::cstr_len(text)},bindings) && bindings.size()==2u,"malformed binding leaves output unchanged"); }
    ws::request command{};
    require(ws::parse("set wasip1 export 0 2f746d702f612062",command) && command.operation==ws::action::portable_export,"hex path supports space without interpolation");
    require(ws::parse("set wasip1 import 0 2f746d702f61 2=7,3=101",command) && command.operation==ws::action::portable_import,"import command");
    require(!ws::parse("set wasip1 export 0 2f746d70 2=7",command),"export has no binding arguments");
    bad=s;bad.resources[2].type=pp::kind::external_stream;bad.resources[2].mount.clear();bad.resources[2].path.clear();bad.resources[2].offset=0u;bad.resources[2].stream_type=6u;
    require(pp::valid(bad) && pp::encode(bad,again) && pp::decode(again,decoded) && decoded.resources[2].stream_type==6u,"socket metadata round-trip without connection state");
    bad.resources[2].stream_type=4u;require(!pp::valid(bad),"regular file cannot masquerade as external stream");
    bad=s;bad.resources[1].flags=4u;require(pp::valid(bad) && pp::encode(bad,again) && pp::decode(again,decoded) && decoded.resources[1].flags==4u,"portable directory flags retained");
    bad=s;bad.resources[2].stdio_index=1u;require(!pp::valid(bad),"unused stdio identity must be canonical");
    bad=s;bad.resources[0].follow=true;require(!pp::valid(bad),"unmounted resource cannot have lookup-follow state");
    bad=s;bad.resources[2].type=pp::kind::external_stream;bad.resources[2].mount.clear();bad.resources[2].path.clear();bad.resources[2].offset=0u;bad.resources[2].stream_type=3u;
    require(!pp::valid(bad),"directory cannot masquerade as external stream");
    if(argc>=2)
    {
        auto directory=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])));
        auto file=::fast_io::u8concat_fast_io(directory,u8"/metadata.uwp");
        require(pp::save_file(s,pp::text_view{file.data(),file.size()}),"exclusive persistent save");
        pp::snapshot actual{};require(pp::load_file(pp::text_view{file.data(),file.size()},actual) && pp::encode(actual,again) && again==bytes,"persistent canonical round-trip");
        bool refused{};try { (void)pp::save_file(s,pp::text_view{file.data(),file.size()}); }catch(::fast_io::error const&) { refused=true; }
        require(refused,"exclusive save never overwrites");
        require(pp::load_file(pp::text_view{file.data(),file.size()},actual) && pp::encode(actual,again) && again==bytes,"failed save preserves contents");
        auto malformed=::fast_io::u8concat_fast_io(directory,u8"/malformed-");malformed.push_back(static_cast<char8_t>(0xffu));malformed.append(u8".uwp");
        require(!pp::save_file(s,pp::text_view{malformed.data(),malformed.size()}),"checkpoint export filename must be UTF-8 before native IO");
        actual=s;require(!pp::load_file(pp::text_view{malformed.data(),malformed.size()},actual) && actual.opens_size==s.opens_size,"malformed checkpoint import filename refused without changing output");
        auto unicode=::fast_io::u8concat_fast_io(directory,u8"/metadata-λ-中.uwp");
        require(pp::save_file(s,pp::text_view{unicode.data(),unicode.size()}) && pp::load_file(pp::text_view{unicode.data(),unicode.size()},actual) && pp::encode(actual,again) && again==bytes,"valid Unicode checkpoint filename round-trips");
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),unicode,{});
        auto rejected_file=::fast_io::u8concat_fast_io(directory,u8"/invalid-snapshot.uwp");
        auto invalid_snapshot=s;invalid_snapshot.resources[2].path.push_back(static_cast<char8_t>(0xffu));
        require(!pp::save_file(invalid_snapshot,pp::text_view{rejected_file.data(),rejected_file.size()}),"bad resource text refused before persistent export");
        bool absent{};try { ::fast_io::native_file probe{rejected_file,::fast_io::open_mode::in}; }catch(::fast_io::error const&) { absent=true; }
        require(absent,"invalid snapshot export creates no file");
        auto signed_file=::fast_io::u8concat_fast_io(directory,u8"/signed-invalid.uwp");
        { ::fast_io::native_file output{signed_file,::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};
          ::fast_io::operations::write_all_bytes(output,invalid_wire.data(),invalid_wire.data()+invalid_wire.size()); }
        actual=s;require(!pp::load_file(pp::text_view{signed_file.data(),signed_file.size()},actual) && unchanged(actual),"FastIO import rejects signed malformed resource without replacing output");
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),signed_file,{});
        auto signed_group_file=::fast_io::u8concat_fast_io(directory,u8"/signed-invalid-group.uwpg");
        { ::fast_io::native_file output{signed_group_file,::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};
          ::fast_io::operations::write_all_bytes(output,invalid_group_wire.data(),invalid_group_wire.data()+invalid_group_wire.size()); }
        require(!pp::load_group_file(pp::text_view{signed_group_file.data(),signed_group_file.size()},untouched_group) &&
            pp::encode_group(untouched_group,again) && again==group_bytes,"FastIO group import rejects malformed later environment atomically");
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),signed_group_file,{});
        auto oversized_file=::fast_io::u8concat_fast_io(directory,u8"/oversized.uwp");
        auto oversized=boundary;++oversized.opens_size;oversized.closed.push_back(pp::max_rows-1u);
        require(!pp::save_file(oversized,pp::text_view{oversized_file.data(),oversized_file.size()}),"oversized export rejected before native file creation");
        bool no_oversized{};try { ::fast_io::native_file probe{oversized_file,::fast_io::open_mode::in}; }catch(::fast_io::error const&) { no_oversized=true; }
        require(no_oversized,"oversized export creates no native file");
        { ::fast_io::native_file output{oversized_file,::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};
          ::fast_io::operations::write_all_bytes(output,oversized_wire.data(),oversized_wire.data()+oversized_wire.size()); }
        actual=s;require(!pp::load_file(pp::text_view{oversized_file.data(),oversized_file.size()},actual) && unchanged(actual),
            "FastIO import rejects checksummed oversized table without replacing output");
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),oversized_file,{});
        auto oversized_group_file=::fast_io::u8concat_fast_io(directory,u8"/oversized.uwpg");
        pp::group_snapshot oversized_group{{s,oversized}};
        require(!pp::save_group_file(oversized_group,pp::text_view{oversized_group_file.data(),oversized_group_file.size()}),"oversized group rejected before native file creation");
        bool no_oversized_group{};try { ::fast_io::native_file probe{oversized_group_file,::fast_io::open_mode::in}; }catch(::fast_io::error const&) { no_oversized_group=true; }
        require(no_oversized_group,"oversized group export creates no native file");
        { ::fast_io::native_file output{oversized_group_file,::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};
          ::fast_io::operations::write_all_bytes(output,oversized_group_wire.data(),oversized_group_wire.data()+oversized_group_wire.size()); }
        require(!pp::load_group_file(pp::text_view{oversized_group_file.data(),oversized_group_file.size()},untouched_group) &&
            pp::encode_group(untouched_group,again) && again==group_bytes,"FastIO import rejects oversized later environment atomically");
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),oversized_group_file,{});
        if(argc>=3)
        {
            auto fifo=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[2])));
            actual=s;require(!pp::load_file(pp::text_view{fifo.data(),fifo.size()},actual) && actual.opens_size==s.opens_size,"nonregular FIFO refused without blocking or changing output");
        }
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),file,{});
    }
    ::fast_io::io::println("wasip1_portable_checkpoint ",checks," checks passed");
}

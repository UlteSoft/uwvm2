// Structural/schema/code-identity DATA, never real provider/stop/restore authority.
#include <uwvm2/uwvm/debugger/checkpoint_codec.h>
#include <uwvm2/uwvm/debugger/checkpoint_state_identity.h>
#include <uwvm2/uwvm/debugger/checkpoint_replay.h>
#include <bit>
#include <array>
#include <string_view>
namespace cp=::uwvm2::uwvm::debugger::checkpoint;
static unsigned checks{};
static void require(bool valid,unsigned line)
{ ++checks; if(!valid) { ::fast_io::io::perrln("builtin binding DATA failure line=",::fast_io::mnp::dec(line));::fast_io::fast_terminate(); } }
#define REQUIRE(x) require(bool(x),__LINE__)
template<unsigned Bits,typename T> static void put(::std::vector<::std::byte>& bytes,::std::size_t offset,T value)
{
    REQUIRE(offset<=bytes.size() && Bits/8u<=bytes.size()-offset);
    // [owned bytes ... offset][exact Bits/8 output] end
    // [safe] subtraction bounds BEFORE two pointer advances; fixed LE encoding.
    auto* first{reinterpret_cast<unsigned char*>(bytes.data())+offset};
    ::fast_io::basic_obuffer_view<unsigned char> output{first,first+Bits/8u};
    ::fast_io::io::print(output,::fast_io::mnp::le_put<Bits>(value));
}
static cp::object binding()
{
    cp::object out{};out.kind=cp::object_kind::host_resource;out.flags=3u;
    out.words={cp::builtin_wasip1_binding_kind,1u,4u};out.bytes.resize(cp::builtin_wasip1_binding_bytes);
    constexpr ::fast_io::string_view magic{"UWVMWASIP1B1"};
    for(::std::size_t i{};i!=magic.size();++i) { out.bytes[i]=static_cast<::std::byte>(magic[i]); }
    put<64>(out.bytes,12u,::std::uint64_t{3u});put<64>(out.bytes,20u,::std::uint64_t{1u});
    out.bytes[28u]=::std::byte{1u};put<64>(out.bytes,60u,::std::uint64_t{2u});put<64>(out.bytes,68u,::std::uint64_t{1u});
    out.bytes[76u]=::std::byte{0x7fu};out.bytes[77u]=::std::byte{0x7eu};out.bytes[92u]=::std::byte{0x7fu};return out;
}
static cp::state sample()
{
    cp::state out{};out.recording_id[0u]=::std::byte{1u};out.checkpoint_id=1u;out.objects.resize(7u);out.root_instances={2u,3u};
    auto& module{out.objects[0u]};module.kind=cp::object_kind::module;
    module.bytes={::std::byte{},::std::byte{'a'},::std::byte{'s'},::std::byte{'m'},::std::byte{1u},::std::byte{},::std::byte{},::std::byte{}};
    for(::std::size_t i{};i!=2u;++i)
    {
        auto& instance{out.objects[1u+i]};instance.kind=cp::object_kind::instance;instance.words[0u]=1u;instance.links={1u,4u+i};
        auto& function{out.objects[3u+i]};function.kind=cp::object_kind::function;function.flags=1u;
        function.words[1u]=1u;function.links={2u+i,6u+i};out.objects[5u+i]=binding();
    }
    return out;
}
static ::std::vector<::std::byte> encode(cp::state const& state)
{
    ::std::array<unsigned char,8192u> bytes{};
    // [fixed private8192-byte owner] end
    // [safe] exact owner extent BEFORE endpoint; synchronous codec only.
    ::fast_io::basic_obuffer_view<unsigned char> output{bytes.data(),bytes.data()+bytes.size()};
    REQUIRE(cp::encode_database(output,state)==cp::error::none);
    auto const count{static_cast<::std::size_t>(output.curr_ptr-bytes.data())};
    ::std::vector<::std::byte> out(count);for(::std::size_t i{};i!=count;++i) { out[i]=static_cast<::std::byte>(bytes[i]); }return out;
}
int main(int argc,char** argv)
{
    if(argc==2) { REQUIRE(::std::string_view{argv[1]}=="--require-big" && ::std::endian::native==::std::endian::big); }
    else { REQUIRE(argc==1); }
    static_assert(cp::database_format_version==6u && cp::binding::state_version==6u);
    auto original{sample()};REQUIRE(cp::validate_graph(original)==cp::error::none);
    REQUIRE(original.objects[3u].links[0u]!=original.objects[4u].links[0u] && original.objects[5u].bytes==original.objects[6u].bytes);
    auto wire{encode(original)};cp::state decoded{};
    REQUIRE(cp::decode_database(wire,decoded)==cp::error::none && decoded==original && encode(decoded)==wire);
    auto old{wire};put<64>(old,0u,cp::legacy_database_magic5);put<16>(old,8u,::std::uint16_t{5u});
    REQUIRE(cp::decode_database(old,decoded)==cp::error::unsupported_version && decoded==original);
    old=wire;put<64>(old,0u,cp::legacy_database_magic4);put<16>(old,8u,::std::uint16_t{4u});
    REQUIRE(cp::decode_database(old,decoded)==cp::error::unsupported_version && decoded==original);
    auto reject{[&](auto change)
    { auto invalid{original};change(invalid.objects[5u]);REQUIRE(cp::validate_graph(invalid)==cp::error::invalid_shape); }};
    reject([](auto& x){x.flags=4u;});reject([](auto& x){x.words[0u]^=1u;});reject([](auto& x){x.words[1u]=2u;});
    reject([](auto& x){x.words[2u]=0u;});reject([](auto& x){x.words[2u]=129u;});reject([](auto& x){x.words[3u]=1u;});
    reject([](auto& x){x.bytes.pop_back();});reject([](auto& x){x.bytes.push_back(::std::byte{});});reject([](auto& x){x.bytes[0u]^=::std::byte{1u};});
    reject([](auto& x){put<64>(x.bytes,12u,::std::uint64_t{128u});});reject([](auto& x){put<64>(x.bytes,12u,::std::uint64_t{2u});});
    reject([](auto& x){put<64>(x.bytes,20u,::std::uint64_t{2u});});reject([](auto& x){x.bytes[28u]=::std::byte{};});
    reject([](auto& x){put<64>(x.bytes,60u,::std::uint64_t{17u});});reject([](auto& x){put<64>(x.bytes,68u,::std::uint64_t{17u});});
    reject([](auto& x){x.bytes[76u]=::std::byte{0x7du};});reject([](auto& x){x.bytes[78u]=::std::byte{0x7fu};});
    reject([](auto& x){x.bytes[92u]=::std::byte{0x7du};});reject([](auto& x){x.bytes[93u]=::std::byte{0x7fu};});
    reject([](auto& x){x.links.push_back(1u);});reject([](auto& x){x.values.emplace_back();});
    auto bad{original};bad.objects[3u].words[1u]=2u;REQUIRE(cp::validate_graph(bad)==cp::error::invalid_shape);
    bad=original;cp::object host{};host.kind=cp::object_kind::host_reference;host.words[0u]=1u;host.links={6u};bad.objects.push_back(host);
    REQUIRE(cp::validate_graph(bad)==cp::error::invalid_shape);
    bad=original;cp::object event{};event.kind=cp::object_kind::event;event.words[0u]=1u;event.links={6u};bad.objects.push_back(event);
    REQUIRE(cp::validate_graph(bad)==cp::error::invalid_shape);
    cp::replay_log replay{};REQUIRE(replay.bind(original)==cp::error::non_replayable_import);
    auto identity{cp::state_identity::derive(original)};REQUIRE(identity.observation==cp::state_identity::status::derived_data && identity.functions.size()==1u);
    bad=original;bad.objects[5u].bytes[28u]=::std::byte{2u};auto altered{cp::state_identity::derive(bad)};
    REQUIRE(altered.observation==cp::state_identity::status::derived_data && altered.functions!=identity.functions);
    bad=original;bad.objects[5u].bytes[76u]=::std::byte{0x7eu};altered=cp::state_identity::derive(bad);
    REQUIRE(altered.observation==cp::state_identity::status::derived_data && altered.functions!=identity.functions);
    bad=original;++bad.objects[5u].words[2u];put<64>(bad.objects[5u].bytes,12u,::std::uint64_t{4u});altered=cp::state_identity::derive(bad);
    REQUIRE(altered.observation==cp::state_identity::status::derived_data && altered.functions!=identity.functions);
    ::fast_io::io::println("checkpoint builtin binding DATA PASS checks=",::fast_io::mnp::dec(checks),
        " endian=",::fast_io::mnp::os_c_str(::std::endian::native==::std::endian::big ? "big":"little"),
        " actual-provider=false capture=false externalrestore=false");
}

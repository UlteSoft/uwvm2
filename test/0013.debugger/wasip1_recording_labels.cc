// Portable recording labels are byte-preserving metadata, never live authority.
#include <uwvm2/uwvm/debugger/wasip1_portable_checkpoint.h>
#include <fast_io.h>
namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;
static unsigned checks{};
static void require(bool b,char const* why)
{ ++checks;if(!b) { ::fast_io::io::perrln("recording labels: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
int main(int argc,char** argv)
{
    if(argc!=2) { return 64; }
    pp::snapshot s{};s.recording_label[0u]=::std::byte{0x71u};s.original_wasm[0u]=::std::byte{3u};
    s.builtin_interface[0u]=::std::byte{7u};s.opens_size=1u;
    s.reserved.push_back({0u,0u,0u,true});s.arguments.emplace_back(u8"recording-label-λ");
    ::std::vector<::std::byte> baseline{},wire{};
    require(pp::valid(s) && pp::encode(s,baseline),"initial canonical metadata");
    auto unchanged=[&](pp::snapshot const& value)
    { wire.clear();return pp::encode(value,wire) && wire==baseline; };
    auto directory=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])));
    auto path=::fast_io::u8concat_fast_io(directory,u8"/recording-label-λ.uwp");
    require(pp::save_file(s,pp::text_view{path.data(),path.size()}),"FastIO exclusive metadata save");
    pp::snapshot loaded{};
    require(pp::load_file(pp::text_view{path.data(),path.size()},loaded) && unchanged(loaded),"native filename and wire preserve label exactly");
    pp::group_snapshot group{{s,s}};::std::vector<::std::byte> group_wire{};
    require(pp::encode_group(group,group_wire),"same-recording group encodes");
    for(unsigned n{};n!=16u;++n)
    {
        auto other=s;other.recording_label[n]^=::std::byte{0x80u};
        ::std::vector<::std::byte> independent{};pp::snapshot decoded{};
        require(pp::encode(other,independent) && pp::decode(independent,decoded) &&
            decoded.recording_label==other.recording_label && decoded.recording_label!=s.recording_label,
            "every label byte is retained by an independently valid checksummed snapshot");
        auto mixed=group;mixed.environments[1u]=other;wire=group_wire;
        require(!pp::valid_group(mixed) && !pp::encode_group(mixed,wire) && wire==group_wire,
            "mixed recordings cannot encode or replace the output");
        // Replace the later nested snapshot with valid independent bytes and
        // recompute the genuine outer checksum. Integrity is not consistency.
        auto signed_mixed=group_wire;auto second=28u+baseline.size();
        require(independent.size()==baseline.size() && second+independent.size()+32u==signed_mixed.size(),"actual nested envelope framing");
        ::std::copy(independent.begin(),independent.end(),signed_mixed.begin()+second);
        auto hash=pp::detail::digest({signed_mixed.data(),signed_mixed.size()-32u});
        ::std::copy(hash.begin(),hash.end(),signed_mixed.end()-32u);
        auto target=group;wire.clear();
        require(!pp::decode_group(signed_mixed,target) && pp::encode_group(target,wire) && wire==group_wire,
            "valid nested and outer hashes cannot authorize mixed labels or partially replace the group");
    }
    auto zero=s;zero.recording_label={};wire=baseline;
    require(!pp::valid(zero) && !pp::encode(zero,wire) && wire==baseline,"zero metadata label refused atomically");
    pp::group_snapshot decoded_group{};auto group_path=::fast_io::u8concat_fast_io(directory,u8"/recording-label.uwpg");
    require(pp::save_group_file(group,pp::text_view{group_path.data(),group_path.size()}) &&
        pp::load_group_file(pp::text_view{group_path.data(),group_path.size()},decoded_group) &&
        pp::encode_group(decoded_group,wire) && wire==group_wire,"FastIO persistent same-recording group round-trip");
    ::fast_io::io::println("wasip1_recording_labels ",checks," checks passed cases=1 unsupported=0");
}

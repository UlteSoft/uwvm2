// Command admission and detached rebinding decoding must share one grammar.
#include <uwvm2/uwvm/debugger/command.h>
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <uwvm2/uwvm/debugger/wasip1_portable_checkpoint.h>
#include <fast_io.h>
namespace dbg=::uwvm2::uwvm::debugger;
namespace ws=::uwvm2::uwvm::debugger::wasip1_state;
namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;
static unsigned checks{};
static void require(bool value,char const* why)
{ ++checks;if(!value) { ::fast_io::io::perrln("rebinding validation: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
static ::fast_io::string command(::fast_io::string_view prefix,pp::text_view tail)
{ return ::fast_io::concat_fast_io(prefix,::fast_io::mnp::code_cvt(tail)); }
int main(int argc,char** argv)
{
    if(argc<2 || argc>3) { return 64; }
    bool const before=argc==3;
    auto path=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])),u8"/rebinding-λ.uwp");
    ws::request sentinel{};sentinel.module=91u;sentinel.first=73u;
    sentinel.operation=ws::action::replace_argument;sentinel.value=ws::text{u8"sentinel"};
    auto unchanged=[&](ws::request const& value)
    { return value.operation==sentinel.operation && value.module==91u && value.first==73u && value.value==u8"sentinel" && value.name.empty() && value.environments.empty(); };
    auto input=[&](ws::action operation,pp::text_view bindings)
    {
        ws::request value{};value.operation=operation;value.name=ws::text{u8"/checkpoint-does-not-exist.uwp"};
        if(operation==ws::action::portable_import_group || operation==ws::action::portable_export_group)
        { value.environments.push_back({0u,ws::text{bindings}}); }
        else { value.value=ws::text{bindings}; }
        return value;
    };
    ::std::vector<pp::rebind> decoded{{9u,11u}};
    auto decoded_unchanged=[&] { return decoded.size()==1u && decoded.front().resource_index==9u && decoded.front().target_descriptor==11u; };
    for(auto raw:{u8"2=7,2=8",u8"2=7,0002=8",u8"2=2147483648",u8"65536=7",u8"4294967296=7",
                  u8"2=4294967296",u8"2=999999999999999999999999999",u8"2=7,",u8"2=7,,3=8",u8"2=73=8",
                  u8"=7",u8"2=",u8"2==7",u8",2=7",u8"2=-1",u8"-2=7",u8"2=+7",u8"+2=7",u8"2=7junk"})
    {
        pp::text_view bindings{raw,::fast_io::cstr_len(raw)};
        require(!pp::parse_rebindings(bindings,decoded) && decoded_unchanged(),"malformed detached list leaves output unchanged");
        auto single=input(ws::action::portable_import,bindings);
        require(ws::valid(single)==before,"single request rejects invalid list before native file IO");
        auto single_command=command("set wasip1 import 0 2f746d702f61 ",bindings);auto target=sentinel;
        bool accepted=ws::parse(::fast_io::string_view{single_command.data(),single_command.size()},target);
        require(accepted==before && (accepted || unchanged(target)),"single command rejection is atomic");
        require((dbg::parse_console_command(::fast_io::string_view{single_command.data(),single_command.size()}).kind==dbg::console_command_kind::wasip1_state)==before,"actual CLI parser rejects malformed single import");
        bool prior_group=true;
        for(auto c:bindings) { prior_group=prior_group && ((c>=u8'0' && c<=u8'9') || c==u8'=' || c==u8','); }
        auto group=input(ws::action::portable_import_group,bindings);
        require(ws::valid(group)==(before && prior_group),"group request uses the same numeric and uniqueness rules");
        auto group_command=command("set wasip1 import-group 2f746d702f61 0:",bindings);target=sentinel;
        accepted=ws::parse(::fast_io::string_view{group_command.data(),group_command.size()},target);
        require(accepted==(before && prior_group) && (accepted || unchanged(target)),"group command rejection is atomic");
        require((dbg::parse_console_command(::fast_io::string_view{group_command.data(),group_command.size()}).kind==dbg::console_command_kind::wasip1_state)==(before && prior_group),"actual CLI parser rejects malformed group import");
    }
    for(auto raw:{u8"",u8"0=0",u8"65535=2147483647",u8"0002=0007,3=7",u8"0=7,1=7"})
    {
        pp::text_view bindings{raw,::fast_io::cstr_len(raw)};
        require(pp::parse_rebindings(bindings,decoded),"valid boundaries and shared target FD numbers remain accepted");
        require(ws::valid(input(ws::action::portable_import,bindings)) && ws::valid(input(ws::action::portable_import_group,bindings)),"valid request agreement");
        if(!bindings.empty())
        {
            auto line=command("set wasip1 import 0 2f746d702f61 ",bindings);auto target=sentinel;
            require(ws::parse(::fast_io::string_view{line.data(),line.size()},target),"valid single script");
            line=command("set wasip1 import-group 2f746d702f61 0:",bindings);
            require(ws::parse(::fast_io::string_view{line.data(),line.size()},target),"valid group script");
        }
    }
    auto bad_export=input(ws::action::portable_export,pp::text_view{u8"2=7"});
    require(ws::valid(bad_export)==before,"export refuses irrelevant rebinding payload");
    require(!ws::valid(input(ws::action::portable_export_group,pp::text_view{u8"2=7"})),"group export has no rebinding payload");
    require(ws::valid(input(ws::action::portable_export,pp::text_view{u8""})) && ws::valid(input(ws::action::portable_export_group,pp::text_view{u8""})),"plain exports remain valid");
    pp::text full{};
    for(unsigned n{};n!=64u;++n)
    { if(n) { full.push_back(u8','); }full.append(::fast_io::u8concat_fast_io(n,u8"=2147483647")); }
    require(pp::parse_rebindings(pp::text_view{full.data(),full.size()},decoded) && decoded.size()==64u &&
        ws::valid(input(ws::action::portable_import,pp::text_view{full.data(),full.size()})) &&
        ws::valid(input(ws::action::portable_import_group,pp::text_view{full.data(),full.size()})),"exact 64-entry boundary");
    full.append(u8",64=0");decoded={{9u,11u}};
    require(!pp::parse_rebindings(pp::text_view{full.data(),full.size()},decoded) && decoded_unchanged(),"65-entry detached list rejected atomically");
    require(ws::valid(input(ws::action::portable_import,pp::text_view{full.data(),full.size()}))==before &&
        ws::valid(input(ws::action::portable_import_group,pp::text_view{full.data(),full.size()}))==before,"65 entries rejected at both request boundaries");
    auto line=command("set wasip1 import 0 2f746d702f61 ",pp::text_view{full.data(),full.size()});auto target=sentinel;
    bool accepted=ws::parse(::fast_io::string_view{line.data(),line.size()},target);
    require(accepted==before && (accepted || unchanged(target)),"65-entry script rejected atomically");
    pp::text padded{};padded.resize(4093u,u8'0');padded.append(u8"2=7");
    require(padded.size()==4096u && pp::parse_rebindings(pp::text_view{padded.data(),padded.size()},decoded) && decoded.size()==1u &&
        decoded.front().resource_index==2u && decoded.front().target_descriptor==7u,"4096-byte list retains legacy leading-zero behavior");
    require(ws::valid(input(ws::action::portable_import,pp::text_view{padded.data(),padded.size()})),"maximum single list length accepted");
    padded.push_back(u8'0');decoded={{9u,11u}};
    require(!pp::parse_rebindings(pp::text_view{padded.data(),padded.size()},decoded) && decoded_unchanged() &&
        !ws::valid(input(ws::action::portable_import,pp::text_view{padded.data(),padded.size()})),"4097-byte list rejected without publication");
    // The change only affects command admission; persistent version-1 bytes
    // and actual native FastIO file operations retain the original behavior.
    pp::snapshot s{};s.recording_label[0u]=::std::byte{9u};s.opens_size=1u;s.reserved.push_back({0u,0u,0u,true});
    ::std::vector<::std::byte> wire{},again{};
    require(pp::encode(s,wire) && pp::save_file(s,pp::text_view{path.data(),path.size()}),"FastIO exclusive version-1 save");
    pp::snapshot loaded{};
    require(pp::load_file(pp::text_view{path.data(),path.size()},loaded) && pp::encode(loaded,again) && again==wire,"Unicode filename and single wire remain unchanged");
    pp::group_snapshot group{{s,s}},loaded_group{};auto group_path=::fast_io::u8concat_fast_io(path,u8"g");
    require(pp::encode_group(group,wire) && pp::save_group_file(group,pp::text_view{group_path.data(),group_path.size()}) &&
        pp::load_group_file(pp::text_view{group_path.data(),group_path.size()},loaded_group) && pp::encode_group(loaded_group,again) && again==wire,"group file round-trip unchanged");
    ::fast_io::io::println("wasip1_rebinding_validation ",checks," checks passed cases=1 unsupported=0 phase=",::fast_io::mnp::os_c_str(before ? "before" : "post"));
}

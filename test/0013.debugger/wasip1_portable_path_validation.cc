// Portable request admission must validate filenames before native capture or IO.
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <uwvm2/uwvm/debugger/command.h>
#include <uwvm2/uwvm/debugger/wasip1_portable_checkpoint.h>
#include <fast_io.h>
namespace dbg=::uwvm2::uwvm::debugger;
namespace ws=::uwvm2::uwvm::debugger::wasip1_state;
namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;
static unsigned checks{};
static void require(bool value,char const* why)
{ ++checks;if(!value) { ::fast_io::io::perrln("portable path validation: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
static pp::text_view view(pp::text const& bytes) { return pp::text_view{bytes.data(),bytes.size()}; }
static ::fast_io::string hex(pp::text_view bytes)
{
    ::fast_io::string out{};constexpr char digits[]{"0123456789abcdef"};
    for(auto c:bytes) { unsigned v=static_cast<unsigned char>(c);out.push_back(digits[v>>4u]);out.push_back(digits[v&15u]); }
    return out;
}
int main(int argc,char** argv)
{
    if(argc<2 || argc>3) { return 64; }
    bool const before=argc==3;
    ws::request sentinel{};sentinel.operation=ws::action::replace_argument;
    sentinel.module=91u;sentinel.first=73u;sentinel.value=ws::text{u8"sentinel"};
    auto unchanged=[&](ws::request const& v)
    { return v.operation==sentinel.operation && v.module==91u && v.first==73u && v.value==u8"sentinel" && v.name.empty() && v.environments.empty(); };
    auto probe=[&](pp::text_view path,bool codec_valid,bool old_admission)
    {
        require(pp::valid_file_path(path)==codec_valid,"codec filename policy");
        for(auto operation:{ws::action::portable_export,ws::action::portable_import,ws::action::portable_export_group,ws::action::portable_import_group})
        {
            ws::request request{};request.operation=operation;request.name=ws::text{path};
            bool const group=operation==ws::action::portable_export_group || operation==ws::action::portable_import_group;
            bool const exporting=operation==ws::action::portable_export || operation==ws::action::portable_export_group;
            if(group) { request.environments.push_back({0u,{}}); }
            bool const expected=before ? old_admission : codec_valid;
            require(ws::valid(request)==expected,"single and group requests share filename admission");
            auto line=::fast_io::concat_fast_io("set wasip1 ",::fast_io::mnp::os_c_str(exporting ? "export" : "import"),::fast_io::mnp::os_c_str(group ? "-group " : " 0 "),hex(path),::fast_io::mnp::os_c_str(group ? " 0" : ""));
            auto target=sentinel;bool accepted=ws::parse(::fast_io::string_view{line.data(),line.size()},target);
            require(accepted==expected && (accepted || unchanged(target)),"console filename rejection is atomic");
            require((dbg::parse_console_command(::fast_io::string_view{line.data(),line.size()}).kind==dbg::console_command_kind::wasip1_state)==expected,"actual CLI parser rejects before controller dispatch");
        }
    };
    for(auto raw:{u8"/f\x80",u8"/f\xff",u8"/f\xc0\xaf",u8"/f\xe0\x80\xaf",u8"/f\xf0\x80\x80\xaf",
                  u8"/f\xc2",u8"/f\xe2\x82",u8"/f\xf0\x9f\x98",u8"/f\xed\xa0\x80",u8"/f\xf4\x90\x80\x80",u8"/f\xf5\x80\x80\x80"})
    { probe(pp::text_view{raw,::fast_io::cstr_len(raw)},false,true); }
    probe({},false,false);pp::text nul{u8"/f"};nul.push_back(char8_t{});nul.push_back(u8'x');probe(view(nul),false,false);
    for(auto raw:{u8"/checkpoint.uwp",u8"./有空 格-λ-😀.uwp",u8"/f\n\t",u8"/f\xef\xbf\xbf",u8"/f\xf4\x8f\xbf\xbf"})
    { probe(pp::text_view{raw,::fast_io::cstr_len(raw)},true,true); }
    pp::text boundary{};boundary.resize(4092u,u8'a');boundary.append(u8"😀");probe(view(boundary),true,true);
    pp::text truncated{};truncated.resize(4093u,u8'a');truncated.append(u8"\xf0\x9f\x98");probe(view(truncated),false,true);
    boundary.push_back(u8'x');probe(view(boundary),false,false);
    // WASI arguments and environment values are opaque bytes, not host filenames.
    ws::request argument{};argument.operation=ws::action::replace_argument;argument.value=ws::text{u8"\xff"};
    require(ws::valid(argument),"opaque argument bytes remain accepted");
    argument.operation=ws::action::set_environment;argument.name=ws::text{u8"KEY"};
    require(ws::valid(argument),"opaque environment bytes remain accepted");
    pp::snapshot saved{};saved.recording_label[0u]=::std::byte{9u};saved.opens_size=1u;saved.reserved.push_back({0u,0u,0u,true});
    saved.arguments.push_back(pp::text{u8"\xff"});saved.environment.push_back(pp::text{u8"KEY=\xff"});
    ::std::vector<::std::byte> wire{},again{};
    auto path=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])),u8"/portable-有空 格-λ-😀.uwp");
    require(pp::encode(saved,wire) && pp::save_file(saved,view(path)),"FastIO saves valid Unicode filename and opaque WASI bytes");
    pp::snapshot loaded{};require(pp::load_file(view(path),loaded) && pp::encode(loaded,again) && again==wire,"single native file round-trip");
    pp::group_snapshot group{{saved,saved}},loaded_group{};path.push_back(u8'g');
    require(pp::encode_group(group,wire) && pp::save_group_file(group,view(path)) && pp::load_group_file(view(path),loaded_group) && pp::encode_group(loaded_group,again) && again==wire,"group native file round-trip");
    pp::text bad{u8"/does-not-exist\xff"};auto previous=loaded.recording_label;
    require(!pp::save_file(saved,view(bad)) && !pp::load_file(view(bad),loaded) && loaded.recording_label==previous,"invalid filename does not publish single output");
    require(!pp::save_group_file(group,view(bad)) && !pp::load_group_file(view(bad),loaded_group) && loaded_group.environments.size()==2u,"invalid filename does not publish group output");
    ::fast_io::io::println("wasip1_portable_path_validation ",checks," checks passed cases=1 unsupported=0 phase=",::fast_io::mnp::os_c_str(before ? "before" : "post"));
}

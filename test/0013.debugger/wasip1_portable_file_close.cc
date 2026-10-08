// Real native FastIO files with an injected final-close error. Only the file
// owner used by the portable helper is substituted; reads, writes, status and
// native close still run on the target OS. No production test hook is needed.
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <cerrno>
#include <memory>
#include <fast_io.h>
#include <fast_io_crypto.h>
#include <fast_io_dsal/string.h>
#include <fast_io_dsal/string_view.h>
#include <uwvm2/utils/utf/impl.h>
#include <uwvm2/uwvm/debugger/wasip1_state.h>
namespace close_probe
{
    inline bool inject{};
    inline unsigned checked{}, unchecked{}, errors{};
    inline void reset(bool failing = false) noexcept { inject=failing;checked=unchecked=errors=0u; }
}
namespace fast_io
{
    class checkpoint_close_probe_file : public native_file
    {
    public:
        using native_file::native_file;
        void close()
        {
            ++close_probe::checked;
            native_file::close();
            if(static_cast<bool>(*this)) { ::fast_io::fast_terminate(); }
            if(close_probe::inject) { ++close_probe::errors;::fast_io::throw_posix_error(EIO); }
        }
        ~checkpoint_close_probe_file()
        { if(static_cast<bool>(*this)) { ++close_probe::unchecked; } }
    };
}
// All prerequisite headers are loaded before this bounded token substitution.
// It affects only portable payload/import file ownership, not directory owners.
#define native_file checkpoint_close_probe_file
#include <uwvm2/uwvm/debugger/wasip1_portable_checkpoint.h>
#undef native_file
namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;
namespace ws=::uwvm2::uwvm::debugger::wasip1_state;
namespace
{
    unsigned checks{};
    void require(bool value, ::fast_io::string_view label)
    { ++checks;if(!value) { ::fast_io::io::perrln("portable file close: ",label);::fast_io::fast_terminate(); } }
    pp::text_view view(pp::text const& s) { return pp::text_view{s.data(),s.size()}; }
    pp::snapshot saved(unsigned label)
    {
        pp::snapshot s{};s.recording_label[0]=static_cast<::std::byte>(label);
        s.original_wasm[0]=::std::byte{0x34};s.builtin_interface[0]=::std::byte{0x56};
        s.opens_size=1u;s.reserved.push_back({0u,17u,19u,true});
        s.arguments.emplace_back(u8"guest");s.arguments.emplace_back(u8"opaque-\xff");s.environment.emplace_back(u8"KEY=value");return s;
    }
    auto bytes(pp::snapshot const& s)
    { ::std::vector<::std::byte> out{};require(pp::encode(s,out),"single fixture canonical");return out; }
    auto bytes(pp::group_snapshot const& s)
    { ::std::vector<::std::byte> out{};require(pp::encode_group(s,out),"group fixture canonical");return out; }
    template<typename F> bool failed_io(F function)
    { try { (void)function(); } catch(::fast_io::error const&) { return true; } return false; }
}
int main(int argc,char const** argv)
{
    if(argc!=2 && argc!=3) { return 91; }
    bool const before{argc==3 && ::fast_io::string_view{argv[2],::fast_io::cstr_len(argv[2])}=="before"};
    if(argc==3 && !before) { return 92; }
    auto root{::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])),u8"/")};
    auto source{saved(0x12u)},sentinel{saved(0x99u)};
    pp::group_snapshot group{{source,source}},old_group{{sentinel,sentinel}};
    auto const single_wire{bytes(source)},group_wire{bytes(group)},old_single_wire{bytes(sentinel)},old_group_wire{bytes(old_group)};
    for(unsigned operation{};operation!=4u;++operation)
    {
        bool const grouped{operation>=2u},loading{(operation&1u)!=0u};
        auto path{::fast_io::u8concat_fast_io(root,u8"close-",operation,u8".uwp")};
        close_probe::reset();
        require(grouped ? pp::save_group_file(group,view(path)) : pp::save_file(source,view(path)),"real native baseline file saved");
        require(close_probe::checked==(before?0u:1u) && close_probe::unchecked==(before?1u:0u),"successful export reports checked final close");
        auto out{sentinel};auto out_group{old_group};close_probe::reset();
        require(grouped ? pp::load_group_file(view(path),out_group) : pp::load_file(view(path),out),"real native baseline file loaded");
        require((grouped?bytes(out_group):bytes(out))==(grouped?group_wire:single_wire),"normal native round-trip exact metadata");
        require(close_probe::checked==(before?0u:1u) && close_probe::unchecked==(before?1u:0u),"successful import closes before publication");
        if(!loading) { path.append(u8"-fault"); }
        out=sentinel;out_group=old_group;close_probe::reset(true);
        bool completed{};
        bool const failed{failed_io([&] {
            completed=loading ? (grouped?pp::load_group_file(view(path),out_group):pp::load_file(view(path),out)) :
                (grouped?pp::save_group_file(group,view(path)):pp::save_file(source,view(path)));return completed;
        })};
        require(failed!=before && completed==before,"close error cannot become a successful operation");
        require(close_probe::checked==(before?0u:1u) && close_probe::errors==(before?0u:1u) && close_probe::unchecked==(before?1u:0u),
            "native handle invalidated and no unchecked destructor close after error");
        if(loading)
        {
            require((grouped?bytes(out_group):bytes(out))==(before?(grouped?group_wire:single_wire):(grouped?old_group_wire:old_single_wire)),
                "final IO error preserves previously published single or group output");
        }
        close_probe::reset();
        if(!loading && !before)
        { require(grouped?pp::save_group_file(group,view(path)):pp::save_file(source,view(path)),"failed exclusive export is cleaned up for retry");close_probe::reset(); }
        require(grouped?pp::load_group_file(view(path),out_group):pp::load_file(view(path),out),"explicit inspection reads retried export or unchanged import source");
        require((grouped?bytes(out_group):bytes(out))==(grouped?group_wire:single_wire),"close error does not imply metadata file is absent");
        close_probe::reset();
        require(failed_io([&] { return grouped?pp::save_group_file(group,view(path)):pp::save_file(source,view(path)); }),"exclusive creation protects existing file on retry");
        require(close_probe::checked==1u && close_probe::unchecked==0u,"failed exclusive publication closes and retires its temporary owner");
    }
    close_probe::reset(true);pp::text invalid{u8"/invalid-\xff"};auto invalid_snapshot=source;invalid_snapshot.recording_label={};
    auto unused{::fast_io::u8concat_fast_io(root,u8"never-created.uwp")};
    require(!pp::save_file(source,view(invalid)) && !pp::load_file(view(invalid),sentinel) &&
        !pp::save_group_file(group,view(invalid)) && !pp::load_group_file(view(invalid),old_group),"filename admission occurs before file ownership");
    require(!pp::save_file(invalid_snapshot,view(unused)) && !pp::save_group_file(pp::group_snapshot{{invalid_snapshot}},view(unused)),"invalid graph does not open a file");
    require(close_probe::checked==0u && close_probe::unchecked==0u && close_probe::errors==0u,"no close action for pre-IO refusals");
    close_probe::reset();
    ::fast_io::io::println("wasip1_portable_file_close ",checks," checks passed cases=1 unsupported=0 phase=",::fast_io::mnp::os_c_str(before?"before":"post"));
}

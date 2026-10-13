// Real OS files and directory handles. Faults and observations are confined
// to this translation unit; production has no test callback or fault switch.
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
namespace probe
{
    inline ::fast_io::u8string target{},kept{},staging_name{};
    inline bool write_error{},close_error{},unlink_error{},open_error{},mkdir_collision{};
    inline int link_error{};
    inline unsigned checked{},unchecked{},writes{},links{},unlinks{},mkdirs{};
    inline bool visible_during_partial{},visible_before_close{};
    inline void (*after_close)(){};
    inline bool exists(::fast_io::u8string const& path)
    { try { (void)::fast_io::native_fstatat(::fast_io::at_fdcwd(),path);return true; } catch(::fast_io::error const&) { return false; } }
    inline void reset()
    { write_error=close_error=unlink_error=open_error=mkdir_collision=false;link_error=0;checked=unchecked=writes=links=unlinks=mkdirs=0;visible_during_partial=visible_before_close=false;after_close=nullptr;staging_name.clear(); }
}
namespace fast_io
{
    class atomic_export_probe_file:public native_file
    {
        template<typename A> static A checked_open(A arg)
        { if(probe::open_error) { ::fast_io::throw_posix_error(EIO); } return arg; }
    public:
        using native_file::native_file;
        template<::fast_io::constructible_to_os_c_str P> atomic_export_probe_file(P const& path,open_mode mode,perms permission=static_cast<perms>(0600)):native_file(checked_open(path),mode,permission) {}
        template<::fast_io::constructible_to_os_c_str P> atomic_export_probe_file(decltype(::fast_io::at_fdcwd()) at,P const& path,open_mode mode,perms permission=static_cast<perms>(0600)):native_file(checked_open(at),path,mode,permission) {}
        void close()
        {
            ++probe::checked;probe::visible_before_close=probe::exists(probe::target);native_file::close();
            if(probe::after_close) { auto callback=probe::after_close;probe::after_close=nullptr;callback(); }
            if(probe::close_error) { ::fast_io::throw_posix_error(EIO); }
        }
        ~atomic_export_probe_file() { if(static_cast<bool>(*this)) { ++probe::unchecked; } }
    };
    namespace operations
    {
        template<typename F> void atomic_export_probe_write(F&& file,::std::byte const* first,::std::byte const* last)
        {
            ++probe::writes;write_all_bytes(file,first,first+7u);
            probe::visible_during_partial=probe::exists(probe::target);
            if(probe::write_error) { ::fast_io::throw_posix_error(EIO); }
            write_all_bytes(file,first+7u,last);
        }
    }
    template<typename A,typename P,typename... Flags> void atomic_export_probe_unlink(A at,P const& path,Flags... flags)
    { ++probe::unlinks;if(probe::unlink_error) { ::fast_io::throw_posix_error(EACCES); } native_unlinkat(at,path,flags...); }
    template<typename A,typename P,typename B,typename Q,typename... Flags> void atomic_export_probe_link(A from,P const& oldpath,B to,Q const& newpath,Flags... flags)
    { ++probe::links;if(probe::link_error) { ::fast_io::throw_posix_error(probe::link_error); } native_linkat(from,oldpath,to,newpath,flags...); }
    template<typename A,typename P> void atomic_export_probe_mkdir(A at,P const& path,perms permission)
    {
        ++probe::mkdirs;probe::staging_name=::fast_io::u8concat_fast_io(path);
        native_mkdirat(at,path,permission);
        if(probe::mkdir_collision)
        {
            ::fast_io::basic_native_file<char> directory{at,path,open_mode::in|open_mode::directory|open_mode::shared_delete};
            ::fast_io::native_file file{::fast_io::at(directory),u8"foreign",open_mode::out|open_mode::creat|open_mode::excl};
            ::fast_io::io::print(file,"another-exporter");file.close();::fast_io::throw_posix_error(EEXIST);
        }
    }
}
#define native_file atomic_export_probe_file
#define write_all_bytes atomic_export_probe_write
#define native_unlinkat atomic_export_probe_unlink
#define native_linkat atomic_export_probe_link
#define native_mkdirat atomic_export_probe_mkdir
#include <uwvm2/uwvm/debugger/wasip1_portable_checkpoint.h>
#undef native_file
#undef write_all_bytes
#undef native_unlinkat
#undef native_linkat
#undef native_mkdirat
namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;
namespace
{
    unsigned checks{};
    void require(bool value,::fast_io::string_view label)
    { ++checks;if(!value) { ::fast_io::io::perrln("atomic export: ",label);::fast_io::fast_terminate(); } }
    auto view(pp::text const& s) { return pp::text_view{s.data(),s.size()}; }
    void competitor()
    {
        if(probe::exists(probe::target))
        { ::fast_io::native_renameat(::fast_io::at_fdcwd(),probe::target,::fast_io::at_fdcwd(),probe::kept); }
        ::fast_io::native_file file{probe::target,::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};
        ::fast_io::io::print(file,"competing-writer");file.close();
    }
    pp::text moving_parent{},moved_parent{};
    bool parent_rename_blocked{};
    void move_parent()
    {
        try { ::fast_io::native_renameat(::fast_io::at_fdcwd(),moving_parent,::fast_io::at_fdcwd(),moved_parent); }
        catch(::fast_io::error const& error)
        {
#if defined(_WIN32) && !defined(__CYGWIN__)
            // NT forbids renaming a directory with an open descendant handle.
            // The pinned private staging directory is such a descendant.
            if(error!=::fast_io::error{::fast_io::nt_domain_value,0xc0000022u}) { throw; }
            parent_rename_blocked=true;return;
#else
            throw;
#endif
        }
        ::fast_io::native_mkdirat(::fast_io::at_fdcwd(),moving_parent,static_cast<::fast_io::perms>(0700));
        ::fast_io::native_file file{probe::target,::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};
        ::fast_io::io::print(file,"new-parent-owner");file.close();
    }
    bool marker(pp::text const& path,::fast_io::string_view expected)
    {
        ::fast_io::native_file file{path,::fast_io::open_mode::in};::std::array<::std::byte,64> bytes{};
        auto* end=::fast_io::operations::read_some_bytes(file,bytes.data(),bytes.data()+bytes.size());file.close();
        return ::fast_io::string_view{reinterpret_cast<char const*>(bytes.data()),static_cast<::std::size_t>(end-bytes.data())}==expected;
    }
    void erase(pp::text const& path)
    { ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),path); }
    unsigned stage_count(pp::text const& root)
    {
        ::fast_io::basic_native_file<char> dir{root,::fast_io::open_mode::in|::fast_io::open_mode::directory|::fast_io::open_mode::shared_delete};unsigned n{};
        for(auto entry: ::fast_io::current(::fast_io::at(dir)))
        { auto name=::fast_io::u8concat_fast_io(::fast_io::u8filename(entry));if(name.starts_with(u8".uwvm-checkpoint-")) { ++n; } }
        return n;
    }
    void clear_owned_stage(pp::text const& root,pp::text const& name)
    {
        if(name.empty()) { return; }
        auto path=::fast_io::u8concat_fast_io(root,u8"/",name);
        ::fast_io::basic_native_file<char> dir{path,::fast_io::open_mode::in|::fast_io::open_mode::directory|::fast_io::open_mode::shared_delete};
        for(auto entry: ::fast_io::current(::fast_io::at(dir)))
        {
            auto child=::fast_io::u8concat_fast_io(::fast_io::u8filename(entry));
            if(child==u8"."||child==u8"..") { continue; }
            ::fast_io::native_unlinkat(::fast_io::at(dir),child);
        }
        dir.close();::fast_io::native_unlinkat(::fast_io::at_fdcwd(),path,::fast_io::native_at_flags::removedir);
    }
}
int exercise(int argc,char const** argv)
{
    if(argc!=2&&argc!=3) { return 91; }
    bool const before=argc==3;
    auto root=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])));
    pp::snapshot s{};s.recording_label[0]=::std::byte{0x24};s.opens_size=1;s.reserved.push_back({0,17,19,true});s.arguments.emplace_back(u8"guest");
    pp::group_snapshot group{{s,s}};::std::vector<::std::byte> single,whole;
    require(pp::encode(s,single)&&pp::encode_group(group,whole),"canonical fixture graphs");
    for(unsigned repeat{};repeat!=4u;++repeat)
    for(unsigned grouped{};grouped!=2u;++grouped)
    for(unsigned fault{};fault!=8u;++fault)
    {
        auto path=::fast_io::u8concat_fast_io(root,u8"/原子-",repeat,u8"-",grouped,u8"-",fault,u8".uwp");
        auto save=[&] { return grouped?pp::save_group_file(group,view(path)):pp::save_file(s,view(path)); };
        probe::reset();probe::target=path;probe::kept=::fast_io::u8concat_fast_io(path,u8".kept");
        probe::write_error=fault==1u||fault==3u;probe::close_error=fault==2u||fault==3u;
        probe::unlink_error=fault==3u||fault==6u;probe::link_error=fault==4u?EIO:fault==5u?ENOTSUP:0;
        if(fault==7u) { probe::after_close=competitor; }
        bool completed{},failed{};::fast_io::error failure{};
        try { completed=save(); } catch(::fast_io::error const& error) { failed=true;failure=error; }
        bool const success=before?(fault==0u||fault>=4u):(fault==0u||fault==6u);
        require(completed==success&&failed!=success,"acknowledgement matches publication boundary");
        require(probe::writes==1u&&probe::checked==1u&&probe::unchecked==0u,"real native payload written and checked-closed exactly once");
        require(probe::visible_during_partial==before,"target never exposes seven-byte partial payload");
        require(probe::visible_before_close==before,"target hidden until successful payload close");
        require(probe::links==(before||fault==1u||fault==2u||fault==3u?0u:1u),"publication occurs only after completed payload IO");
        require(probe::mkdirs==(before?0u:1u),"unique staging directory belongs to this export");
        if(fault==1u||fault==2u||fault==3u||(!before&&(fault==4u||fault==5u)))
        { require(failure==::fast_io::error{::fast_io::posix_domain_value,static_cast<::std::size_t>(fault==5u?ENOTSUP:EIO)},"cleanup preserves original write/close/publication error"); }
        bool const present=success||fault==7u||(before&&fault==3u);
        require(probe::exists(path)==present,"failed unpublished export cannot occupy final name");
        if(fault==7u)
        {
            require(marker(path,"competing-writer"),"competing target never overwritten or deleted");
            require(probe::exists(probe::kept)==before,"old direct write can be moved before acknowledgement");
        }
        else if(success)
        {
            probe::close_error=false;pp::snapshot decoded{};pp::group_snapshot decoded_group{};
            require(grouped?pp::load_group_file(view(path),decoded_group):pp::load_file(view(path),decoded),"committed file decodes on genuine native backend");
            ::std::vector<::std::byte> actual;require(grouped?pp::encode_group(decoded_group,actual):pp::encode(decoded,actual),"committed metadata valid");
            require(actual==(grouped?whole:single),"committed file contains exact original wire bytes");
        }
        require(stage_count(root)==((!before&&(fault==3u||fault==6u))?1u:0u),"cleanup remnants confined to owned temporary directory");
        auto stage=probe::staging_name;probe::reset();
        if(probe::exists(path)) { erase(path); }
        if(probe::exists(probe::kept)) { erase(probe::kept); }
        if(!stage.empty()&&stage_count(root)) { clear_owned_stage(root,stage); }
        require(stage_count(root)==0u,"only this test's authenticated staging remnants retired");
        probe::target=path;require(save(),"same final name can be retried after failed export");erase(path);
        require(stage_count(root)==0u,"normal success retires its staging directory");
    }
    for(unsigned grouped{};grouped!=2u;++grouped)
    for(unsigned directory{};directory!=2u;++directory)
    {
        auto path=::fast_io::u8concat_fast_io(root,u8"/existing-",grouped,u8"-",directory);
        if(directory) { ::fast_io::native_mkdirat(::fast_io::at_fdcwd(),path,static_cast<::fast_io::perms>(0700)); }
        else { ::fast_io::native_file file{path,::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};::fast_io::io::print(file,"existing-owner");file.close(); }
        probe::reset();probe::target=path;bool refused{};try { (void)(grouped?pp::save_group_file(group,view(path)):pp::save_file(s,view(path))); } catch(::fast_io::error const&) { refused=true; }
        require(refused,"pre-existing regular file or directory is preserved");
        require(directory? ::fast_io::native_fstatat(::fast_io::at_fdcwd(),path).type==::fast_io::file_type::directory:marker(path,"existing-owner"),"existing target identity/content unchanged");
        require(probe::unchecked==0u&&stage_count(root)==0u,"failed exclusive publication leaks no handle or stage");
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),path,directory?::fast_io::native_at_flags::removedir: ::fast_io::native_at_flags{});
    }
    for(unsigned grouped{};grouped!=2u;++grouped)
    {
        auto path=::fast_io::u8concat_fast_io(root,u8"/open-error-",grouped);probe::reset();probe::target=path;probe::open_error=true;
        bool refused{};try { (void)(grouped?pp::save_group_file(group,view(path)):pp::save_file(s,view(path))); } catch(::fast_io::error const& error) { require(error==::fast_io::error{::fast_io::posix_domain_value,EIO},"original payload-open error preserved");refused=true; }
        require(refused&&!probe::exists(path)&&probe::unchecked==0u,"payload-open failure never publishes a final name");require(stage_count(root)==0u,"payload-open failure removes empty owned staging directory");
        if(!before)
        {
            probe::reset();probe::target=path;probe::mkdir_collision=true;refused=false;
            try { (void)(grouped?pp::save_group_file(group,view(path)):pp::save_file(s,view(path))); } catch(::fast_io::error const& error) { require(error==::fast_io::error{::fast_io::posix_domain_value,EEXIST},"directory creation collision propagated");refused=true; }
            auto staged=::fast_io::u8concat_fast_io(root,u8"/",probe::staging_name,u8"/foreign");
            require(refused&&!probe::exists(path)&&marker(staged,"another-exporter"),"failed directory creation acquires no foreign ownership");
            require(probe::unlinks==0u,"foreign collision directory is not cleaned up");auto name=probe::staging_name;probe::reset();clear_owned_stage(root,name);
        }
    }
    for(unsigned grouped{};grouped!=2u;++grouped)
    {
        moving_parent=::fast_io::u8concat_fast_io(root,u8"/parent-",grouped);moved_parent=::fast_io::u8concat_fast_io(moving_parent,u8".moved");
        ::fast_io::native_mkdirat(::fast_io::at_fdcwd(),moving_parent,static_cast<::fast_io::perms>(0700));
        auto path=::fast_io::u8concat_fast_io(moving_parent,u8"/state.uwp"),published=::fast_io::u8concat_fast_io(moved_parent,u8"/state.uwp");
        probe::reset();probe::target=path;parent_rename_blocked=false;probe::after_close=move_parent;
        require(grouped?pp::save_group_file(group,view(path)):pp::save_file(s,view(path)),"original directory remains the publication authority across parent rename");
        if(parent_rename_blocked)
        { ::fast_io::io::println("atomic export: Windows parent rename blocked by STATUS_ACCESS_DENIED; export continues in original directory");published=path; }
        require(parent_rename_blocked?!probe::exists(moved_parent):marker(path,"new-parent-owner"),"parent is either natively pinned against rename or replacement remains untouched");
        pp::snapshot decoded{};pp::group_snapshot decoded_group{};require(grouped?pp::load_group_file(view(published),decoded_group):pp::load_file(view(published),decoded),"exact original parent contains committed checkpoint");
        ::std::vector<::std::byte> actual;require(grouped?pp::encode_group(decoded_group,actual):pp::encode(decoded,actual),"renamed parent checkpoint graph valid");require(actual==(grouped?whole:single),"renamed parent checkpoint bytes exact");
        require(stage_count(moving_parent)==0u&&(parent_rename_blocked||stage_count(moved_parent)==0u),"cleanup follows pinned directory rather than replacement pathname");
        erase(path);if(!parent_rename_blocked) { erase(published); }
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),moving_parent,::fast_io::native_at_flags::removedir);
        if(!parent_rename_blocked) { ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),moved_parent,::fast_io::native_at_flags::removedir); }
        pp::text long_name{};for(unsigned n{};n!=249u;++n) { long_name.push_back(u8'a'); }long_name.append(u8".uwp");
#if defined(_WIN32) && !defined(__CYGWIN__)
        // The full path exceeds DOS MAX_PATH; use native extended-length syntax
        // for both publication and the independent absolute-path reader.
        path=::fast_io::u8concat_fast_io(u8"\\\\?\\",root,u8"\\",long_name);
        for(auto& c:path) { if(c==u8'/') { c=u8'\\'; } }
#else
        path=::fast_io::u8concat_fast_io(root,u8"/",long_name);
#endif
        probe::reset();probe::target=path;
        require(grouped?pp::save_group_file(group,view(path)):pp::save_file(s,view(path)),"253-character final name does not get a temporary suffix");
        require(grouped?pp::load_group_file(view(path),decoded_group):pp::load_file(view(path),decoded),"long filename metadata round-trip");actual.clear();require((grouped?pp::encode_group(decoded_group,actual):pp::encode(decoded,actual))&&actual==(grouped?whole:single),"long filename exact wire contents");require(stage_count(root)==0u,"long filename temporary storage retired");erase(path);
    }
    probe::reset();auto path=::fast_io::u8concat_fast_io(root,u8"/invalid");pp::text invalid{u8"invalid-\xff"};require(!pp::save_file(s,view(invalid))&&!pp::save_group_file(group,view(invalid)),"invalid UTF8 rejected before acquisition");
    s.recording_label={};require(!pp::save_file(s,view(path))&&!pp::save_group_file(pp::group_snapshot{{s}},view(path)),"invalid graph rejected before acquisition");
    require(probe::mkdirs==0u&&probe::writes==0u&&probe::links==0u&&!probe::exists(path),"admission failure owns neither temp nor target");
    ::fast_io::io::println("wasip1_portable_atomic_export ",checks," checks passed cases=1 unsupported=0 phase=",::fast_io::mnp::os_c_str(before?"before":"post"));
    return 0;
}

int main(int argc,char const** argv)
{
    try { return exercise(argc,argv); }
    catch(::fast_io::error const& error)
    {
        ::fast_io::io::perrln("atomic export native error checks=",checks," domain=",::fast_io::mnp::hex(error.domain)," code=",::fast_io::mnp::hex(error.code)," target=",::fast_io::mnp::code_cvt(probe::target)," staging=",::fast_io::mnp::code_cvt(probe::staging_name));return 93;
    }
}

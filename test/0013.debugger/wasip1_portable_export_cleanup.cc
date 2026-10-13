// Faults occur around REAL FastIO native writes, closes, status and unlink.
// Token substitution is restricted to the portable file helper.
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
    inline bool close_error{},write_error{},unlink_error{};
    inline unsigned checked{},unchecked{},writes{},unlinks{};
    inline void (*after_close)(){};
    inline void reset() noexcept
    { close_error=write_error=unlink_error=false;checked=unchecked=writes=unlinks=0u;after_close=nullptr; }
}
namespace fast_io
{
    class export_probe_file:public native_file
    {
    public:
        using native_file::native_file;
        void close()
        {
            ++probe::checked;native_file::close();
            if(probe::after_close) { auto callback=probe::after_close;probe::after_close=nullptr;callback(); }
            if(probe::close_error) { ::fast_io::throw_posix_error(EIO); }
        }
        ~export_probe_file() { if(static_cast<bool>(*this)) { ++probe::unchecked; } }
    };
    namespace operations
    {
        template<typename F> void export_probe_write(F&& file,::std::byte const* first,::std::byte const* last)
        {
            ++probe::writes;
            if(probe::write_error)
            { write_all_bytes(file,first,first+7u);::fast_io::throw_posix_error(EIO); }
            write_all_bytes(file,first,last);
        }
    }
    template<typename A,typename P,typename... Flags> void export_probe_unlink(A at,P const& path,Flags... flags)
    { ++probe::unlinks;if(probe::unlink_error) { ::fast_io::throw_posix_error(EACCES); } native_unlinkat(at,path,flags...); }
}
#define native_file export_probe_file
#define write_all_bytes export_probe_write
#define native_unlinkat export_probe_unlink
#include <uwvm2/uwvm/debugger/wasip1_portable_checkpoint.h>
#undef native_file
#undef write_all_bytes
#undef native_unlinkat
namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;
namespace
{
    unsigned checks{};
    void require(bool value,::fast_io::string_view label)
    { ++checks;if(!value) { ::fast_io::io::perrln("export cleanup: ",label);::fast_io::fast_terminate(); } }
    auto view(pp::text const& s) { return pp::text_view{s.data(),s.size()}; }
    pp::text replacing{},kept{};
    void replace_path()
    {
        try { (void)::fast_io::native_fstatat(::fast_io::at_fdcwd(),replacing);
            ::fast_io::native_renameat(::fast_io::at_fdcwd(),replacing,::fast_io::at_fdcwd(),kept); }
        catch(::fast_io::error const&) {}
        ::fast_io::native_file file{replacing,::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};
        ::fast_io::io::print(file,"replacement-owned-by-another-writer");file.close();
    }
    bool exists(pp::text const& path)
    { try { (void)::fast_io::native_fstatat(::fast_io::at_fdcwd(),path);return true; } catch(::fast_io::error const&) { return false; } }
    template<typename F> bool io_failed(F&& f)
    { try { (void)f(); } catch(::fast_io::error const& error) { require(error==::fast_io::error{::fast_io::posix_domain_value,EIO},"original EIO preserved through cleanup");return true; } return false; }
}
int main(int argc,char const** argv)
{
    if(argc!=2 && argc!=3) { return 91; }
    bool const before{argc==3};
    auto root=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])),u8"/");
    pp::snapshot s{};s.recording_label[0]=::std::byte{0x23};s.opens_size=1;s.reserved.push_back({0,17,19,true});s.arguments.emplace_back(u8"guest");
    pp::group_snapshot group{{s,s}};
    for(unsigned repeat{};repeat!=8u;++repeat)
    for(unsigned grouped{};grouped!=2u;++grouped)
    for(unsigned fault{};fault!=5u;++fault)
    {
        auto path=::fast_io::u8concat_fast_io(root,u8"恢复-",repeat,u8"-",grouped,u8"-",fault,u8".uwp");
        auto save=[&] { return grouped?pp::save_group_file(group,view(path)):pp::save_file(s,view(path)); };
        probe::reset();probe::write_error=fault==0u;probe::close_error=fault!=0u;
        if(fault==2u) { probe::unlink_error=true; }
        if(fault==3u) { replacing=path;kept=::fast_io::u8concat_fast_io(path,u8".kept");probe::after_close=replace_path; }
        if(fault==4u) { probe::write_error=true; } // BOTH write and cleanup-close fail
        require(io_failed(save),"failed native export never acknowledges success");
        require(probe::writes==1u,"one real write sequence");
        require(probe::checked==((before && (fault==0u||fault==4u))?0u:1u),"failed writes receive a checked cleanup-close");
        require(probe::unchecked==((before && (fault==0u||fault==4u))?1u:0u),"no unchecked close remains after handled failure");
        bool const remains{fault==3u};
        require(exists(path)==remains,"only authenticated failed export is removed");
        require(probe::unlinks==2u,"replacement identity is not unlinked");
        if(fault==3u)
        {
            require(!exists(kept),"unpublished payload never occupied the public path");
            ::fast_io::native_file file{path,::fast_io::open_mode::in};::std::array<::std::byte,35> bytes{};
            auto end=::fast_io::operations::read_some_bytes(file,bytes.data(),bytes.data()+bytes.size());
            require(::fast_io::string_view{reinterpret_cast<char const*>(bytes.data()),static_cast<::std::size_t>(end-bytes.data())}=="replacement-owned-by-another-writer","replacement contents untouched");file.close();
        }
        probe::reset();
        if(remains)
        {
            bool refused{};try { (void)save(); } catch(::fast_io::error const&) { refused=true; }
            require(refused && probe::writes==1u && probe::checked==1u && probe::unchecked==0u,"existing target is never overwritten or claimed");
            ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),path);
        }
        else { require(save(),"same filename retry succeeds after cleanup");::fast_io::native_unlinkat(::fast_io::at_fdcwd(),path); }
        probe::reset();require(save(),"exclusive export succeeds after owner cleanup");
        require(probe::checked==1u && probe::unchecked==0u,"successful export checks final close");
        probe::reset();pp::snapshot out{};pp::group_snapshot out_group{};
        require(grouped?pp::load_group_file(view(path),out_group):pp::load_file(view(path),out),"successful single/group metadata round-trip");
        require(probe::checked==1u && probe::unlinks==0u,"import closes without deleting the source");
        ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),path);
        if(fault==3u && exists(kept)) { ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),kept); }
    }
    probe::reset();pp::text bad{u8"bad-\xff"};require(!pp::save_file(s,view(bad))&&!pp::save_group_file(group,view(bad)),"invalid UTF8 paths refused before IO");
    s.recording_label={};auto path=::fast_io::u8concat_fast_io(root,u8"invalid.uwp");
    require(!pp::save_file(s,view(path))&&!pp::save_group_file(pp::group_snapshot{{s}},view(path)),"invalid metadata refused before IO");
    require(probe::writes==0u&&probe::checked==0u&&probe::unlinks==0u&&!exists(path),"pre-IO refusal acquires no file ownership");
    ::fast_io::io::println("wasip1_portable_export_cleanup ",checks," checks passed cases=1 unsupported=0 phase=",::fast_io::mnp::os_c_str(before?"before":"post"));
}

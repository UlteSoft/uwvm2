// Exercise the production OS provider, independently of JIT admission. The
// companion debug_wasip1_checkpoint_runtime fixture verifies real host gates.
#include <uwvm2/runtime/lib/uwvm_runtime_wasip1_native_file.h>
#include <array>
namespace provider=::uwvm2::runtime::lib::wasip1_native_file;
inline unsigned checks{};
static void require(bool condition, char const* message)
{
    ++checks;
    if(!condition) { ::fast_io::io::perrln("FAIL ",::fast_io::mnp::os_c_str(message));::fast_io::fast_terminate(); }
}
static auto cursor(::fast_io::native_file const& file)
{ return ::fast_io::operations::io_stream_seek_bytes(file,0,::fast_io::seekdir::cur); }
static int run()
{
    require(provider::supported,"OS provider available");
    auto file{provider::create()};
    require(bool(file) && ::fast_io::status(file).size==0u && cursor(file)==0,"empty owned file");
    int const original_flags{provider::flags(file)};
#if defined(_WIN32) && !defined(__CYGWIN__)
    require(original_flags==0,"fixed Windows WASI flags");
    bool rejected{};
    try { provider::set_flags(file,1); } catch(::fast_io::error const&) { rejected=true; }
    require(rejected && cursor(file)==0,"unsupported Windows flags leave state unchanged");
#else
    auto status{::fast_io::status(file)};
    require(status.nlink==0u,"file already unlinked before publication");
    require((static_cast<unsigned>(status.perm)&0777u)==0600u,"private POSIX permissions");
    require((original_flags&O_APPEND)==0,"initial ordinary positioned writes");
#endif
    ::std::byte original[]{::std::byte{'A'},::std::byte{},::std::byte{'B'}};
    ::fast_io::operations::pwrite_all_bytes(file,original,original+3u,0);
    ::fast_io::operations::io_stream_seek_bytes(file,1,::fast_io::seekdir::beg);
    ::fast_io::native_file alias{::fast_io::io_dup,file};
    require(cursor(alias)==1,"genuine duplicate shares native cursor");
    std::array<::std::byte,3u> bytes{};
    provider::read_content(file,bytes.data(),bytes.data()+bytes.size());
    require(bytes[0]==original[0] && bytes[1]==original[1] && bytes[2]==original[2],"binary content including NUL");
    require(cursor(file)==1 && cursor(alias)==1,"capture preserves shared live cursor");
    bool short_read{};
    std::array<::std::byte,4u> too_long{};
    try { provider::read_content(file,too_long.data(),too_long.data()+too_long.size()); }
    catch(::fast_io::error const&) { short_read=true; }
    require(short_read && cursor(file)==1 && cursor(alias)==1,"failed capture preserves cursor");
    auto replacement{provider::create()};
    ::fast_io::operations::pwrite_all_bytes(replacement,bytes.data(),bytes.data()+bytes.size(),0);
    ::fast_io::operations::io_stream_seek_bytes(replacement,1,::fast_io::seekdir::beg);
    provider::set_flags(replacement,original_flags);
    ::std::byte mutation[]{::std::byte{'x'},::std::byte{'y'},::std::byte{'z'},::std::byte{'!'}};
    ::fast_io::operations::pwrite_all_bytes(file,mutation,mutation+4u,0);
    ::fast_io::operations::io_stream_seek_bytes(file,4,::fast_io::seekdir::beg);
    require(::fast_io::status(file).size==4u && ::fast_io::status(replacement).size==3u && cursor(replacement)==1,
        "fresh materialization is independent of changed original and duplicate");
    provider::read_content(replacement,bytes.data(),bytes.data()+bytes.size());
    require(bytes[0]==original[0] && bytes[1]==original[1] && bytes[2]==original[2] && cursor(replacement)==1,"snapshot restored exactly");
#if !defined(_WIN32) || defined(__CYGWIN__)
    provider::set_flags(replacement,original_flags|O_APPEND|O_NONBLOCK);
    int const captured_flags{provider::flags(replacement)};
    auto with_flags{provider::create()};
    ::fast_io::operations::pwrite_all_bytes(with_flags,bytes.data(),bytes.data()+bytes.size(),0);
    ::fast_io::operations::io_stream_seek_bytes(with_flags,1,::fast_io::seekdir::beg);
    provider::set_flags(with_flags,captured_flags);
    require(provider::flags(with_flags)==captured_flags && cursor(with_flags)==1,"append and nonblock flags restored after bytes and cursor");
    provider::set_flags(with_flags,original_flags);
    require(provider::flags(with_flags)==original_flags,"flags can be cleared");
#endif
    file.close();
    require(bool(alias) && ::fast_io::status(alias).size==4u && cursor(alias)==4,"closing original keeps owned alias alive");
    alias.close();
    require(::fast_io::status(replacement).size==3u,"retired original never closes restored backing");
    for(unsigned n{};n!=128u;++n)
    {
        auto transient{provider::create()};
        require(::fast_io::status(transient).size==0u,"repeated construction and retirement");
    }
    ::fast_io::io::println("PASS wasip1_native_file_platform checks=",checks);
    return 0;
}
int main()
{
    try { return run(); }
    catch(::fast_io::error const& error)
    { ::fast_io::io::perrln("native operation failed after checks=",checks," domain=",error.domain," code=",error.code);return 1; }
}


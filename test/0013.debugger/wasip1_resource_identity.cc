// Native resource identity and cursor behavior, without capture authority.
// The genuine LLVM-full test independently covers admission and publication.
#include <uwvm2/runtime/lib/uwvm_runtime_wasip1_native_file.h>
#include <fast_io.h>
#include <vector>
namespace fm=::uwvm2::imported::wasi::wasip1::fd_manager;
namespace native=::uwvm2::runtime::lib::wasip1_native_file;
namespace identity=::uwvm2::runtime::lib::wasip1_resource_identity;
static unsigned checks{};
static void require(bool good,char const* why)
{ ++checks;if(!good) { ::fast_io::io::perrln("wasip1_resource_identity: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
static auto& file(fm::wasi_fd_ref_t const& r)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    return r.ptr->wasi_fd_storage.storage.file_fd.file;
#else
    return r.ptr->wasi_fd_storage.storage.file_fd;
#endif
}
static fm::wasi_fd_ref_t create()
{
    fm::wasi_fd_ref_t r{};
    r.ptr->wasi_fd_storage=fm::wasi_fd_storage_t{fm::wasi_fd_type_e::file};
    file(r)=native::create();
    ::std::byte content[]{::std::byte{'A'},::std::byte{},::std::byte{'B'}};
    ::fast_io::operations::pwrite_all_bytes(file(r),content,content+3u,0);
    ::fast_io::operations::io_stream_seek_bytes(file(r),0,::fast_io::seekdir::beg);return r;
}
int main(int argc,char** argv)
{
    if(argc!=2) { return 64; }
    ::std::vector<fm::wasi_fd_ref_t> rows{};
    require(!identity::conflict(rows),"zero resources accepted");
    auto first=create(),second=create(),third=create();
    require(first.ptr!=second.ptr && second.ptr!=third.ptr && first.ptr!=third.ptr,"actual independent file owners");
    rows.push_back(first);require(!identity::conflict(rows),"one actual owner accepted");
    rows.push_back(second);rows.push_back(third);
    require(!identity::conflict(rows),"distinct native files with identical contents and cursors accepted");
    rows.push_back(first);require(identity::conflict(rows)==3u,"nonadjacent repeat of first owner rejected");
    rows.pop_back();rows.push_back(second);require(identity::conflict(rows)==3u,"nonadjacent repeat of middle owner rejected");
    rows.pop_back();rows.push_back(third);require(identity::conflict(rows)==3u,"adjacent duplicate rejected");
    rows.pop_back();
    fm::wasi_fd_unique_ptr_t fd_a{},fd_b{};
    fd_a.fd_p->wasi_fd=first;fd_b.fd_p->wasi_fd=first;
    require(fd_a.fd_p!=fd_b.fd_p && fd_a.fd_p->wasi_fd.ptr==fd_b.fd_p->wasi_fd.ptr,"different descriptor cells can share one actual resource");
    rows={fd_a.fd_p->wasi_fd,fd_b.fd_p->wasi_fd};
    require(identity::conflict(rows)==1u,"different FD cells do not excuse a resource alias collision");
    rows={first,second,first,second};require(identity::conflict(rows)==2u,"report first conflicting saved resource");
    rows={first,fm::wasi_fd_ref_t{fm::wasi_no_construct}};
    require(identity::conflict(rows)==1u,"null target is never a valid materialized resource");
    rows.clear();rows.push_back(first);rows.push_back(second);
    auto first_flags=native::flags(file(first)),second_flags=native::flags(file(second));
    ::fast_io::operations::io_stream_seek_bytes(file(first),7,::fast_io::seekdir::beg);
    require(::fast_io::operations::io_stream_seek_bytes(file(fd_b.fd_p->wasi_fd),0,::fast_io::seekdir::cur)==7 &&
        ::fast_io::operations::io_stream_seek_bytes(file(second),0,::fast_io::seekdir::cur)==0,"original alias shares cursor; independent owner does not");
    rows.push_back(fd_b.fd_p->wasi_fd);
    require(identity::conflict(rows)==2u,"conflict checks operate on identity even after cursors diverge");
    require(::fast_io::operations::io_stream_seek_bytes(file(first),0,::fast_io::seekdir::cur)==7 &&
        ::fast_io::operations::io_stream_seek_bytes(file(second),0,::fast_io::seekdir::cur)==0 &&
        native::flags(file(first))==first_flags && native::flags(file(second))==second_flags,
        "rejected identity comparison performs no cursor or flag mutation");
    rows={third,second,first};require(!identity::conflict(rows),"independent resources accepted in a different order");
    ::std::byte bytes[3u]{};
    native::read_content(file(second),bytes,bytes+3u);
    require(bytes[0]==::std::byte{'A'} && bytes[1]==::std::byte{} && bytes[2]==::std::byte{'B'} &&
        ::fast_io::operations::io_stream_seek_bytes(file(second),0,::fast_io::seekdir::cur)==0,"native binary payload and read cursor preserved");
    ::fast_io::dir_file directory{::fast_io::mnp::os_c_str(argv[1])};
    ::fast_io::native_file evidence{::fast_io::at(directory),u8"resource-identity.txt",
        ::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};
    ::fast_io::io::println(evidence,"actual native resource identity run checks=",checks);
    ::fast_io::io::println("wasip1_resource_identity ",checks," checks passed cases=1 unsupported=0");
}

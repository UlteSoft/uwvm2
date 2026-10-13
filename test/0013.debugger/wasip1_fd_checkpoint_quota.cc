// Exercises the real WASIp1 FD manager and FastIO native files on each OS.
// This allocator test has no debugger ticket/capsule and grants no VM authority.
#include <uwvm2/imported/wasi/wasip1/func/fd_close.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_renumber.h>
#include <fast_io.h>
#include <climits>
#include <cstddef>
namespace fm=::uwvm2::imported::wasi::wasip1::fd_manager;
namespace fn=::uwvm2::imported::wasi::wasip1::func;
namespace abi=::uwvm2::imported::wasi::wasip1::abi;
using environment=::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t>;
static unsigned checks{};
static void require(bool good,char const* message)
{
    ++checks;
    if(!good) { ::fast_io::io::perrln("wasip1_fd_checkpoint_quota: ",::fast_io::mnp::os_c_str(message));::fast_io::fast_terminate(); }
}
static auto& file(fm::wasi_fd_ref_t const& ref)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    return ref.ptr->wasi_fd_storage.storage.file_fd.file;
#else
    return ref.ptr->wasi_fd_storage.storage.file_fd;
#endif
}
static void quota(fm::wasm_fd_storage_t const& table,::std::size_t occupied)
{
    // All transitions are sequential; production callers hold fds_rwlock.
    require(table.fits_occupied_slot_limit(occupied),"exact occupied limit accepted");
    if(occupied) { require(!table.fits_occupied_slot_limit(occupied-1u),"one fewer slot rejected"); }
    require(table.fits_occupied_slot_limit(SIZE_MAX),"unbounded quota cannot overflow");
}
int main(int argc,char** argv)
{
    if(argc<2) { return 64; }
    auto path=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])),u8"/quota-state.bin");
    fm::wasi_fd_ref_t retained{};
    retained.ptr->wasi_fd_storage.reset_type(fm::wasi_fd_type_e::file);
    file(retained)=::fast_io::native_file{path,::fast_io::open_mode::in|::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::trunc};
    ::fast_io::io::print(file(retained),"0123456789");
    require(::fast_io::operations::io_stream_seek_bytes(file(retained),4,::fast_io::seekdir::beg)==4,"real native cursor initialized");
    environment env{};auto& table=env.fd_storage;table.fd_limit=65536u;
    quota(table,0u);
    table.opens.emplace_back(); // Allocated null-resource FD0 is reserved.
    quota(table,1u);
    table.opens.emplace_back();table.opens.back().fd_p->wasi_fd=retained;
    table.opens.emplace_back();table.opens.back().fd_p->wasi_fd=retained;
    table.opens[1u].fd_p->rights_base=abi::rights_t::right_fd_read;
    table.opens[2u].fd_p->rights_base=abi::rights_t::right_fd_write;
    quota(table,3u);
    require(table.opens[1u].fd_p->wasi_fd.ptr==table.opens[2u].fd_p->wasi_fd.ptr,"two live aliases share actual native RC");
    require(::fast_io::operations::io_stream_seek_bytes(file(retained),0,::fast_io::seekdir::cur)==4,"quota probes preserve shared native cursor");
    require(table.opens[0u].fd_p->wasi_fd.ptr->wasi_fd_storage.type==fm::wasi_fd_type_e::null &&
        table.opens[0u].fd_p->close_pos==SIZE_MAX,"reserved null resource was not made reusable");
    require(fn::fd_renumber_base(env,1,INT32_MAX)==abi::errno_t::esuccess,"real manager renumbers live alias to highest positive FD");
    quota(table,3u);
    require(table.opens.size()==3u && table.closes.size()==1u && table.renumber_map.size()==1u,"sparse high FD consumes one slot without dense expansion");
    require(table.renumber_map.find(INT32_MAX)->second.fd_p->wasi_fd.ptr==retained.ptr,"renumber preserves original file resource");
    require(fn::fd_close_base(env,INT32_MAX)==abi::errno_t::esuccess,"real manager closes sparse alias");
    quota(table,2u);
    require(fn::fd_close_base(env,2)==abi::errno_t::esuccess,"real manager closes remaining dense alias");
    quota(table,1u);
    require(table.opens[0u].fd_p->close_pos==SIZE_MAX && table.closes.size()==2u,"only reserved FD remains charged");
    require(fn::fd_renumber_base(env,0,INT32_MAX)==abi::errno_t::esuccess,"real manager can move reserved null resource");
    quota(table,1u);
    require(fn::fd_close_base(env,INT32_MAX)==abi::errno_t::esuccess,"real manager removes high reserved slot");
    quota(table,0u);
    require(fn::fd_close_base(env,INT32_MAX)==abi::errno_t::ebadf &&
        fn::fd_renumber_base(env,1,INT32_MAX)==abi::errno_t::ebadf,"invalid repeated operations do not create slots");
    quota(table,0u);
    require(::fast_io::operations::io_stream_seek_bytes(file(retained),0,::fast_io::seekdir::cur)==4,"close/renumber and quota checks preserve retained real file cursor");
    ::fast_io::operations::io_stream_seek_bytes(file(retained),0,::fast_io::seekdir::beg);
    char bytes[10]{};::fast_io::operations::read_all(file(retained),bytes,bytes+10);
    require(::fast_io::string_view{bytes,10}=="0123456789","native file content unchanged by all allocator transitions");
    {
        fm::wasm_fd_storage_t reserved{};
        for(::std::size_t n{};n!=65536u;++n) { reserved.opens.emplace_back(fm::wasi_no_construct); }
        quota(reserved,65536u); // Unconstructed reserved cells are still charged.
        reserved.renumber_map.emplace(INT32_MAX,fm::wasi_fd_unique_ptr_t{fm::wasi_no_construct});
        require(!reserved.fits_occupied_slot_limit(65536u) && reserved.fits_occupied_slot_limit(65537u),
            "sparse reserved cell crosses exact 65536 boundary");
        reserved.renumber_map.clear();
        reserved.closes.push_back(0u);quota(reserved,65535u);
    }
    {
        fm::wasm_fd_storage_t invalid{};
        invalid.closes.push_back(0u);
        require(!invalid.fits_occupied_slot_limit(0u) && !invalid.fits_occupied_slot_limit(SIZE_MAX),
            "malformed free list fails without unsigned underflow");
    }
    ::fast_io::io::println("wasip1_fd_checkpoint_quota ",checks," checks passed");
}

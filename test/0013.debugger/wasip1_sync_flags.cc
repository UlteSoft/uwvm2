// Original WASI opens, native FastIO status and both fdstat ABIs.
// Borrowed observers remain backed by a live FastIO owner throughout the query.
#define UWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64
#include <uwvm2/imported/wasi/wasip1/func/path_open.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close.h>
#include <fast_io.h>
namespace fm=::uwvm2::imported::wasi::wasip1::fd_manager;
namespace abi=::uwvm2::imported::wasi::wasip1::abi;
namespace fn=::uwvm2::imported::wasi::wasip1::func;
namespace mem=::uwvm2::imported::wasi::wasip1::memory;
static unsigned checks{},cases{},unsupported{},composite{};
static void require(bool value,char const* message)
{ ++checks;if(!value) { ::fast_io::io::perrln("wasip1_sync_flags: ",::fast_io::mnp::os_c_str(message));::fast_io::fast_terminate(); } }
static unsigned map_native(int flags,bool before)
{
    unsigned out{};
#if !defined(_WIN32) || defined(__CYGWIN__)
#ifdef O_APPEND
    if(flags&O_APPEND) { out|=1u; }
#endif
#ifdef O_NONBLOCK
    if(flags&O_NONBLOCK) { out|=4u; }
#endif
#if defined(O_DSYNC) && O_DSYNC != 0
    if(before ? bool(flags&O_DSYNC) : (flags&O_DSYNC)==O_DSYNC) { out|=2u; }
#endif
#if defined(O_RSYNC) && O_RSYNC != 0
    if(before ? bool(flags&O_RSYNC) : (flags&O_RSYNC)==O_RSYNC) { out|=8u; }
#endif
#if defined(O_SYNC) && O_SYNC != 0
    if(before ? bool(flags&O_SYNC) : (flags&O_SYNC)==O_SYNC) { out|=16u; }
#endif
#endif
    return out;
}
int main(int argc,char** argv)
{
    if(argc!=2 && argc!=3) { return 64; }
    bool before=argc==3 && ::fast_io::string_view{argv[2],::fast_io::cstr_len(argv[2])}=="before";
    ::fast_io::dir_file directory{::fast_io::mnp::os_c_str(argv[1])};
    ::fast_io::native_mkdirat(::fast_io::at(directory),u8"nested");
    { ::fast_io::native_file file{::fast_io::at(directory),u8"state.bin",::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};::fast_io::io::print(file,"DATA"); }
    fm::wasi_fd_ref_t root{};root.ptr->wasi_fd_storage=fm::wasi_fd_storage_t{fm::wasi_fd_type_e::dir};
    auto& entry=root.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.emplace_back().ptr->dir_stack;
    entry.name=u8"portable-root";entry.storage.file=::std::move(directory);
    ::uwvm2::object::memory::linear::native_memory_t memory{};memory.init_by_page_count(1u);
    ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<decltype(memory)> env{};
    env.wasip1_memory=::std::addressof(memory);env.fd_storage.fd_limit=8u;
    fm::wasi_fd_unique_ptr_t parent{};parent.fd_p->wasi_fd=root;
    parent.fd_p->rights_base=static_cast<abi::rights_t>(0x3fffffffu);parent.fd_p->rights_inherit=static_cast<abi::rights_t>(0x3fffffffu);
    env.fd_storage.opens.push_back(::std::move(parent));
    auto query=[&](unsigned width,unsigned number,unsigned expected,unsigned type)
    {
        auto error=width==0u ? static_cast<unsigned>(fn::fd_fdstat_get(env,static_cast<abi::wasi_posix_fd_t>(number),128u)) :
            static_cast<unsigned>(fn::fd_fdstat_get_wasm64(env,static_cast<abi::wasi_posix_fd_wasm64_t>(number),128u));
        require(error==0u,"original fd_fdstat_get succeeds");
        require(mem::get_basic_wasm_type_from_memory<::std::uint16_t>(memory,130u)==expected,"fdflags exactly match the actual native open state");
        require(mem::get_basic_wasm_type_from_memory<::std::uint8_t>(memory,128u)==type,"original fdstat retains native resource kind");
    };
    for(unsigned width{};width!=2u;++width)
    for(bool dir:{false,true})
    for(unsigned requested:{0u,1u,4u,5u,2u,3u,6u,7u,16u,18u,8u,24u})
    {
        if(dir && (requested&1u)) { continue; }
        ++cases;
        auto name=dir ? ::fast_io::u8string_view{u8"nested"} : ::fast_io::u8string_view{u8"state.bin"};
        auto first=reinterpret_cast<::std::byte const*>(name.data());mem::write_all_to_memory(memory,32u,first,first+name.size());
        unsigned rights=dir ? 2u : 0x60006eu;
        auto error=width==0u ? static_cast<unsigned>(fn::path_open(env,0,abi::lookupflags_t{},32u,
            static_cast<abi::wasi_size_t>(name.size()),static_cast<abi::oflags_t>(dir ? 2u : 0u),
            static_cast<abi::rights_t>(rights),abi::rights_t{},static_cast<abi::fdflags_t>(requested),16u)) :
            static_cast<unsigned>(fn::path_open_wasm64(env,0,abi::lookupflags_wasm64_t{},32u,name.size(),
                static_cast<abi::oflags_wasm64_t>(dir ? 2u : 0u),static_cast<abi::rights_wasm64_t>(rights),
                abi::rights_wasm64_t{},static_cast<abi::fdflags_wasm64_t>(requested),16u));
        bool supported=true;
#if defined(_WIN32) && !defined(__CYGWIN__)
        if(requested&26u) { supported=false; }
#else
#ifndef O_DSYNC
        if(requested&2u) { supported=false; }
#endif
#ifndef O_RSYNC
        if(requested&8u) { supported=false; }
#endif
#endif
        if(!supported) { require(error==static_cast<unsigned>(abi::errno_t::enotsup),"platform without requested sync mode explicitly rejects it");++unsupported;continue; }
        if(error) { ::fast_io::io::perrln("open error=",error," width=",width," dir=",dir," requested=",requested); }
        require(error==0u,"original WASI accepts supported native mode");
        auto number=mem::get_basic_wasm_type_from_memory<::std::uint32_t>(memory,16u);
        auto retained=env.fd_storage.opens.index_unchecked(number).fd_p->wasi_fd;
        unsigned expected{};
        ::fast_io::native_io_observer observed{};
        if(dir) { observed=retained.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.back_unchecked().ptr->dir_stack.storage.file; }
        else
        {
#if defined(_WIN32) && !defined(__CYGWIN__)
            observed=retained.ptr->wasi_fd_storage.storage.file_fd.file;
#else
            observed=retained.ptr->wasi_fd_storage.storage.file_fd;
#endif
        }
#if defined(_WIN32) && !defined(__CYGWIN__)
        expected=dir ? 0u : requested;
#else
        auto native=::fast_io::posix_getfl_nothrow(observed);require(native.error==0,"FastIO reads genuine native status flags");
        expected=map_native(native.flags,before);
        if(map_native(native.flags,false)!=map_native(native.flags,true)) { ++composite; }
#if defined(__linux__)
        if((requested&26u)==2u)
        {
            require((native.flags&O_SYNC)!=O_SYNC && (native.flags&O_DSYNC)==O_DSYNC,"DSYNC open carries no full native SYNC mask");
            require(expected==((requested&5u)|(before ? 26u : 2u)),"DSYNC remains a weaker sync capability in both fdstat ABIs");
        }
#endif
#endif
        query(width,number,expected,dir ? 3u : 4u);
        if(!dir)
        {
            fm::wasi_fd_unique_ptr_t borrowed{};borrowed.fd_p->rights_base=static_cast<abi::rights_t>(rights);
            borrowed.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(fm::wasi_fd_type_e::file_observer);
            borrowed.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.file_observer=observed;
            auto observer_number=static_cast<unsigned>(env.fd_storage.opens.size());env.fd_storage.opens.push_back(::std::move(borrowed));
            query(width,observer_number,
#if defined(_WIN32) && !defined(__CYGWIN__)
                0u,
#else
                expected,
#endif
                4u);
            require(fn::fd_close_base(env,static_cast<abi::wasi_posix_fd_t>(observer_number))==abi::errno_t::esuccess,"borrowed observer closes without closing retained native owner");
            // Drop the test-only observer cell, preserving the original allocator's close list.
            env.fd_storage.closes.pop_back();env.fd_storage.opens.pop_back();
            query(width,number,expected,4u);
        }
        require(fn::fd_close_base(env,static_cast<abi::wasi_posix_fd_t>(number))==abi::errno_t::esuccess,"original WASI retires owned opened resource");
    }
#if defined(__linux__)
    require(composite==12u,"all DSYNC-only file observer and directory composite-mask cases reproduced");
#endif
    ::fast_io::io::println("wasip1_sync_flags ",checks," checks passed cases=",cases," unsupported=",unsupported," composite=",composite," before=",before);
}

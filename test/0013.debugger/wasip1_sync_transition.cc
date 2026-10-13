// Exercise the original WASI ABI, including borrowed resources and shared aliases.
#define UWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64
#include <uwvm2/imported/wasi/wasip1/func/path_open.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_set_flags.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_set_flags_wasm64.h>
#include <fast_io.h>
namespace fm=::uwvm2::imported::wasi::wasip1::fd_manager;
namespace abi=::uwvm2::imported::wasi::wasip1::abi;
namespace fn=::uwvm2::imported::wasi::wasip1::func;
namespace mem=::uwvm2::imported::wasi::wasip1::memory;
static unsigned checks{},cases{},unsupported{},false_successes{},changed{},noops{};
static void require(bool value,char const* message)
{
    ++checks;
    if(!value) { ::fast_io::io::perrln("wasip1_sync_transition: ",::fast_io::mnp::os_c_str(message));::fast_io::fast_terminate(); }
}
static int desired_native(unsigned requested,int current,bool& supported)
{
#if !defined(_WIN32) || defined(__CYGWIN__)
    int mask{},wanted{};
#define UWVM_TRANSITION_BIT(os_bit,wasi_bit) mask|=os_bit; if(requested&wasi_bit) { wanted|=os_bit; }
#if defined(O_APPEND) && O_APPEND != 0
    UWVM_TRANSITION_BIT(O_APPEND,1u)
#else
    if(requested&1u) { supported=false; }
#endif
#if defined(O_DSYNC) && O_DSYNC != 0
    UWVM_TRANSITION_BIT(O_DSYNC,2u)
#else
    if(requested&2u) { supported=false; }
#endif
#if defined(O_NONBLOCK) && O_NONBLOCK != 0
    UWVM_TRANSITION_BIT(O_NONBLOCK,4u)
#else
    if(requested&4u) { supported=false; }
#endif
#if defined(O_RSYNC) && O_RSYNC != 0
    UWVM_TRANSITION_BIT(O_RSYNC,8u)
#else
    if(requested&8u) { supported=false; }
#endif
#if defined(O_SYNC) && O_SYNC != 0
    UWVM_TRANSITION_BIT(O_SYNC,16u)
#else
    if(requested&16u) { supported=false; }
#endif
#undef UWVM_TRANSITION_BIT
    return (current&~mask)|wanted;
#else
    return current;
#endif
}
int main(int argc,char** argv)
{
    if(argc!=2 && argc!=3) { return 64; }
    bool const before=argc==3 && ::fast_io::string_view{argv[2],::fast_io::cstr_len(argv[2])}=="before";
    ::fast_io::dir_file directory{::fast_io::mnp::os_c_str(argv[1])};
    ::fast_io::native_mkdirat(::fast_io::at(directory),u8"nested");
    { ::fast_io::native_file f{::fast_io::at(directory),u8"state.bin",::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};::fast_io::io::print(f,"DATA"); }
    fm::wasi_fd_ref_t root{};root.ptr->wasi_fd_storage=fm::wasi_fd_storage_t{fm::wasi_fd_type_e::dir};
    auto& entry=root.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.emplace_back().ptr->dir_stack;
    entry.name=u8"portable-root";entry.storage.file=::std::move(directory);
    for(unsigned width{};width!=2u;++width)
    for(unsigned type{};type!=4u;++type)
    for(unsigned initial:{0u,1u,2u,16u,18u,8u,24u})
    for(unsigned wanted:{0u,1u,4u,5u,2u,3u,6u,7u,16u,17u,20u,21u,18u,8u,24u,26u})
    {
        bool const dir=type>=2u,borrowed=(type&1u)!=0u;
        if(dir && (initial&1u)) { continue; }
#if defined(_WIN32) && !defined(__CYGWIN__)
        if(borrowed && (initial&1u)) { continue; }
#endif
        ++cases;
        ::uwvm2::object::memory::linear::native_memory_t memory{};memory.init_by_page_count(1u);
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<decltype(memory)> env{};
        env.wasip1_memory=::std::addressof(memory);env.fd_storage.fd_limit=8u;
        fm::wasi_fd_unique_ptr_t parent{};parent.fd_p->wasi_fd=root;
        parent.fd_p->rights_base=static_cast<abi::rights_t>(0x3fffffffu);parent.fd_p->rights_inherit=static_cast<abi::rights_t>(0x3fffffffu);
        env.fd_storage.opens.push_back(::std::move(parent));
        auto name=dir ? ::fast_io::u8string_view{u8"nested"} : ::fast_io::u8string_view{u8"state.bin"};
        auto bytes=reinterpret_cast<::std::byte const*>(name.data());mem::write_all_to_memory(memory,32u,bytes,bytes+name.size());
        auto error=width==0u ? static_cast<unsigned>(fn::path_open(env,0,abi::lookupflags_t{},32u,static_cast<abi::wasi_size_t>(name.size()),
            static_cast<abi::oflags_t>(dir ? 2u : 0u),static_cast<abi::rights_t>(dir ? 0x60002eu : 0x60006eu),abi::rights_t{},static_cast<abi::fdflags_t>(initial),16u)) :
            static_cast<unsigned>(fn::path_open_wasm64(env,0,abi::lookupflags_wasm64_t{},32u,name.size(),static_cast<abi::oflags_wasm64_t>(dir ? 2u : 0u),
            static_cast<abi::rights_wasm64_t>(dir ? 0x60002eu : 0x60006eu),abi::rights_wasm64_t{},static_cast<abi::fdflags_wasm64_t>(initial),16u));
        if(error==static_cast<unsigned>(abi::errno_t::enotsup)) { ++unsupported;continue; }
        if(error) { ::fast_io::io::perrln("open error=",error," width=",width," type=",type," initial=",initial," wanted=",wanted); }
        require(error==0u,"original path_open succeeds for supported initial flags");
        auto number=mem::get_basic_wasm_type_from_memory<::std::uint32_t>(memory,16u);
        auto retained=env.fd_storage.opens.index_unchecked(number).fd_p->wasi_fd;
        ::fast_io::native_io_observer observed{};
        if(dir) { observed=retained.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.back_unchecked().ptr->dir_stack.storage.file; }
        else
        {
#if defined(_WIN32) && !defined(__CYGWIN__)
            observed=retained.ptr->wasi_fd_storage.storage.file_fd.file;
#else
            observed=retained.ptr->wasi_fd_storage.storage.file_fd;
#endif
            require(::fast_io::operations::io_stream_seek_bytes(observed,3,::fast_io::seekdir::beg)==3,"test sets an actual native cursor");
        }
        if(borrowed)
        {
            fm::wasi_fd_unique_ptr_t cell{};cell.fd_p->rights_base=static_cast<abi::rights_t>(0x60006eu);
            if(dir)
            {
                cell.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(fm::wasi_fd_type_e::dir);
                auto& e=cell.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.emplace_back().ptr->dir_stack;
                e=fm::dir_stack_entry_t{true};e.name=u8"portable-root";e.storage.observer=::fast_io::dir_io_observer{observed.native_handle()};
            }
            else
            {
                cell.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(fm::wasi_fd_type_e::file_observer);
                cell.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.file_observer=observed;
            }
            number=static_cast<unsigned>(env.fd_storage.opens.size());env.fd_storage.opens.push_back(::std::move(cell));
        }
        auto* target=env.fd_storage.opens.index_unchecked(number).fd_p;
        fm::wasi_fd_unique_ptr_t alias{};alias.fd_p->wasi_fd=target->wasi_fd;alias.fd_p->rights_base=target->rights_base;
        auto alias_number=static_cast<unsigned>(env.fd_storage.opens.size());env.fd_storage.opens.push_back(::std::move(alias));
        auto query=[&](unsigned fd)
        {
            auto e=width==0u ? static_cast<unsigned>(fn::fd_fdstat_get(env,static_cast<abi::wasi_posix_fd_t>(fd),128u)) :
                static_cast<unsigned>(fn::fd_fdstat_get_wasm64(env,static_cast<abi::wasi_posix_fd_wasm64_t>(fd),128u));
            require(e==0u,"original fdstat succeeds on retained aliases");
            require(mem::get_basic_wasm_type_from_memory<::std::uint8_t>(memory,128u)==(dir ? 3u : 4u),"native resource type preserved");
            return mem::get_basic_wasm_type_from_memory<::std::uint16_t>(memory,130u);
        };
        auto set=[&]()
        {
            return width==0u ? static_cast<unsigned>(fn::fd_fdstat_set_flags(env,static_cast<abi::wasi_posix_fd_t>(number),static_cast<abi::fdflags_t>(wanted))) :
                static_cast<unsigned>(fn::fd_fdstat_set_flags_wasm64(env,static_cast<abi::wasi_posix_fd_wasm64_t>(number),static_cast<abi::fdflags_wasm64_t>(wanted)));
        };
        auto saved=query(number);require(query(alias_number)==saved,"aliases initially share status flags");
        target->rights_base&=~abi::rights_t::right_fd_fdstat_set_flags;
        require(set()==static_cast<unsigned>(abi::errno_t::enotcapable),"reduced FD rights forbid flag mutation");
        require(query(alias_number)==saved,"denied mutation leaves backing alias unchanged");
        target->rights_base|=abi::rights_t::right_fd_fdstat_set_flags;
        bool supported=true;int native_before{},expected{};
#if !defined(_WIN32) || defined(__CYGWIN__)
        auto native=::fast_io::posix_getfl_nothrow(observed);require(native.error==0,"FastIO reads original native flags");
        native_before=native.flags;expected=desired_native(wanted,native_before,supported);
#endif
        error=set();auto actual=query(number);require(query(alias_number)==actual,"aliases share resulting status flags");
#if !defined(_WIN32) || defined(__CYGWIN__)
        auto native_after=::fast_io::posix_getfl_nothrow(observed);require(native_after.error==0,"FastIO reads resulting native flags");
        if(error==0u)
        {
            bool exact=supported && native_after.flags==expected;
            if(before && !exact)
            {
                require((saved&26u)==26u && (wanted&26u)==2u,"baseline false success is specifically SYNC to DSYNC");++false_successes;
            }
            else { require(exact,"successful WASI mutation matches every requested native flag and preserves unrelated bits"); }
            if(native_after.flags==native_before) { ++noops; }else { ++changed; }
        }
        else
        {
            require(error==static_cast<unsigned>(abi::errno_t::enotsup),"unsupported native transition explicitly returns ENOTSUP");
            require(native_after.flags==native_before && actual==saved,"refused transition rolls back APPEND/NONBLOCK and sync flags for all aliases");
        }
#else
        require(error==0u || error==static_cast<unsigned>(abi::errno_t::enotsup),"Windows reports supported idempotence or explicit ENOTSUP");
        require(actual==saved,"Windows fixed native flags remain consistent on success or refusal");
        if(error==0u) { require(wanted==saved,"Windows succeeds only for matching current flags");++noops; }
#endif
        if(!dir) { require(::fast_io::operations::io_stream_seek_bytes(observed,0,::fast_io::seekdir::cur)==3,"mutation never rewinds the shared cursor"); }
    }
#if defined(__linux__)
    require(false_successes==(before ? 128u : 0u),"all composite SYNC downgrade false successes reproduced or eliminated");
#else
    require(false_successes==0u,"other OS providers report no false successes");
#endif
    { ::fast_io::native_file f{::fast_io::at(root.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.front_unchecked().ptr->dir_stack.storage.file),u8"state.bin",::fast_io::open_mode::in};
      char bytes[4]{};::fast_io::operations::read_all(f,bytes,bytes+4);require(::fast_io::string_view{bytes,4u}=="DATA","original file contents survive every transition"); }
    ::fast_io::io::println("wasip1_sync_transition ",checks," checks passed cases=",cases," unsupported=",unsupported," false_successes=",false_successes," changed=",changed," noops=",noops," before=",before);
}

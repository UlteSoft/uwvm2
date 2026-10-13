#define UWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64
#include <uwvm2/imported/wasi/wasip1/func/path_open.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_set_flags.h>
#include <fast_io.h>
namespace fm=::uwvm2::imported::wasi::wasip1::fd_manager;
namespace abi=::uwvm2::imported::wasi::wasip1::abi;
namespace fn=::uwvm2::imported::wasi::wasip1::func;
namespace mem=::uwvm2::imported::wasi::wasip1::memory;
static unsigned checks{},cases{},unsupported{},wrong_flags{},shared_handles{},mutated_parents{};
static void require(bool value,char const* message)
{ ++checks;if(!value) { ::fast_io::io::perrln("wasip1_directory_flags: ",::fast_io::mnp::os_c_str(message));::fast_io::fast_terminate(); } }
static unsigned expected_flags(unsigned requested)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    return requested;
#else
    int native{};unsigned result{};
#ifdef O_NONBLOCK
    if(requested&4u) { native|=O_NONBLOCK; }
    if(native&O_NONBLOCK) { result|=4u; }
#endif
#if defined(O_DSYNC) && O_DSYNC != 0
    if(requested&2u) { native|=O_DSYNC; }
#endif
#if defined(O_SYNC) && O_SYNC != 0
    if(requested&16u) { native|=O_SYNC; }
#endif
#if defined(O_RSYNC) && O_RSYNC != 0
    if(requested&8u) { native|=O_RSYNC; }
#endif
#if defined(O_DSYNC) && O_DSYNC != 0
    if((native&O_DSYNC)==O_DSYNC) { result|=2u; }
#endif
#if defined(O_SYNC) && O_SYNC != 0
    if((native&O_SYNC)==O_SYNC) { result|=16u; }
#endif
#if defined(O_RSYNC) && O_RSYNC != 0
    if((native&O_RSYNC)==O_RSYNC) { result|=8u; }
#endif
    return result;
#endif
}
int main(int argc,char** argv)
{
    if(argc!=2 && argc!=3) { return 64; }
    bool const before=argc==3 && ::fast_io::string_view{argv[2],::fast_io::cstr_len(argv[2])}=="before";
    { ::fast_io::dir_file root{::fast_io::mnp::os_c_str(argv[1])};::fast_io::native_mkdirat(::fast_io::at(root),u8"nested"); }
    for(unsigned width{};width!=2u;++width)
    for(bool borrowed:{false,true})
    for(unsigned parent_flags:{0u,4u})
    for(auto path:{u8".",u8"./.",u8"nested/.",u8"nested/..",u8"nested/../nested/.",u8"nested"})
    for(unsigned requested:{0u,4u,2u,16u,18u,8u,24u})
    {
#if defined(_WIN32) && !defined(__CYGWIN__)
        if(parent_flags!=0u) { continue; }
#endif
        ++cases;
        auto mode=::fast_io::open_mode::directory|::fast_io::open_mode::in;
        if(parent_flags&4u) { mode|=::fast_io::open_mode::no_block; }
        ::fast_io::dir_file owner{::fast_io::mnp::os_c_str(argv[1]),mode};
        fm::wasi_fd_ref_t root{};root.ptr->wasi_fd_storage.reset_type(fm::wasi_fd_type_e::dir);
        auto& entry=root.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.emplace_back().ptr->dir_stack;
        entry.name=u8"portable-root";
        if(borrowed) { entry=fm::dir_stack_entry_t{true};entry.name=u8"portable-root";entry.storage.observer=owner; }
        else { entry.storage.file=::std::move(owner); }
        ::fast_io::dir_io_observer native_parent=borrowed ? entry.storage.observer : ::fast_io::dir_io_observer{entry.storage.file};
        ::uwvm2::object::memory::linear::native_memory_t memory{};memory.init_by_page_count(1u);
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<decltype(memory)> env{};
        env.wasip1_memory=::std::addressof(memory);env.fd_storage.fd_limit=4u;
        fm::wasi_fd_unique_ptr_t parent{};parent.fd_p->wasi_fd=root;
        parent.fd_p->rights_base=abi::rights_t::right_path_open|abi::rights_t::right_fd_datasync|abi::rights_t::right_fd_sync;
        parent.fd_p->rights_inherit=abi::rights_t::right_fd_fdstat_set_flags;env.fd_storage.opens.push_back(::std::move(parent));
        auto query=[&](unsigned fd)
        {
            auto e=width==0u ? static_cast<unsigned>(fn::fd_fdstat_get(env,static_cast<abi::wasi_posix_fd_t>(fd),128u)) :
                static_cast<unsigned>(fn::fd_fdstat_get_wasm64(env,static_cast<abi::wasi_posix_fd_wasm64_t>(fd),128u));
            require(e==0u,"original fdstat reads directory");require(mem::get_basic_wasm_type_from_memory<::std::uint8_t>(memory,128u)==3u,"directory kind retained");
            return mem::get_basic_wasm_type_from_memory<::std::uint16_t>(memory,130u);
        };
        auto saved=query(0);require(saved==parent_flags,"actual parent flag fixture");
        auto name=::fast_io::u8string_view{path,::fast_io::cstr_len(path)};auto bytes=reinterpret_cast<::std::byte const*>(name.data());
        mem::write_all_to_memory(memory,32u,bytes,bytes+name.size());::std::byte sentinel[]{::std::byte{0xef},::std::byte{0xbe},::std::byte{0xad},::std::byte{0xde}};mem::write_all_to_memory(memory,16u,sentinel,sentinel+4u);
        auto open=[&]()
        {
            return width==0u ? static_cast<unsigned>(fn::path_open(env,0,abi::lookupflags_t{},32u,static_cast<abi::wasi_size_t>(name.size()),abi::oflags_t::o_directory,
                abi::rights_t::right_fd_fdstat_set_flags,abi::rights_t{},static_cast<abi::fdflags_t>(requested),16u)) :
                static_cast<unsigned>(fn::path_open_wasm64(env,0,abi::lookupflags_wasm64_t{},32u,name.size(),abi::oflags_wasm64_t::o_directory,
                abi::rights_wasm64_t::right_fd_fdstat_set_flags,abi::rights_wasm64_t{},static_cast<abi::fdflags_wasm64_t>(requested),16u));
        };
        auto error=open();
        if(error==static_cast<unsigned>(abi::errno_t::enotsup))
        {
            ++unsupported;require(env.fd_storage.opens.size()==1u && env.fd_storage.closes.empty() && env.fd_storage.renumber_map.empty(),"unsupported flags publish no FD or free slot");
            require(mem::get_basic_wasm_type_from_memory<::std::uint32_t>(memory,16u)==0xdeadbeefu && query(0)==saved,"refusal leaves output and parent unchanged");continue;
        }
        if(error) { ::fast_io::io::perrln("open errno=",error," path=",::fast_io::mnp::code_cvt(name)," flags=",requested); }
        require(error==0u,"original directory path_open accepts supported flags");
        auto number=mem::get_basic_wasm_type_from_memory<::std::uint32_t>(memory,16u);require(number==1u,"original allocator installs one child");
        auto actual=query(number);bool exact=actual==expected_flags(requested);
        if(before && !exact) { ++wrong_flags; }else { require(exact,"opened directory has requested supported flags"); }
        auto& leaf=env.fd_storage.opens.index_unchecked(number).fd_p->wasi_fd.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.back_unchecked().ptr->dir_stack;
        ::fast_io::dir_io_observer native_child=leaf.is_observer ? leaf.storage.observer : ::fast_io::dir_io_observer{leaf.storage.file};
        bool independent=native_child.native_handle()!=native_parent.native_handle();
        if(before && !independent) { ++shared_handles; }else { require(independent && !leaf.is_observer,"directory open owns an independent native description"); }
        require(query(0)==saved,"opening never changes parent flags");
        auto edited=fn::fd_fdstat_set_flags_base(env,static_cast<abi::wasi_posix_fd_t>(number),static_cast<abi::fdflags_t>(actual^4u));
        require(edited==abi::errno_t::esuccess || edited==abi::errno_t::enotsup,"child flag edit reports native support");
        auto after=query(0);
        if(before && after!=saved) { ++mutated_parents; }else { require(after==saved,"child flag edits cannot mutate original preopen"); }
    }
    if(before) { require(wrong_flags>0u && shared_handles>0u,"baseline reproduces ignored flags and shared root handles"); }
    else { require(wrong_flags==0u && shared_handles==0u && mutated_parents==0u,"all directory construction regressions eliminated"); }
    ::fast_io::io::println("wasip1_directory_flags ",checks," checks passed cases=",cases," unsupported=",unsupported," wrong_flags=",wrong_flags," shared_handles=",shared_handles," mutated_parents=",mutated_parents," before=",before);
}

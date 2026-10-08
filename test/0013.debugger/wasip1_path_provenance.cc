// Real WASI wasm32/wasm64 path opens and FastIO files. No debug-ticket substitute.
#define UWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64
#include <uwvm2/imported/wasi/wasip1/func/path_open.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close.h>
#include <fast_io.h>
namespace fm=::uwvm2::imported::wasi::wasip1::fd_manager;
namespace abi=::uwvm2::imported::wasi::wasip1::abi;
namespace fn=::uwvm2::imported::wasi::wasip1::func;
static unsigned checks{};
static void require(bool value,char const* message)
{ ++checks;if(!value) { ::fast_io::io::perrln("wasip1_path_provenance: ",::fast_io::mnp::os_c_str(message));::fast_io::fast_terminate(); } }
int main(int argc,char** argv)
{
    if(argc!=2 && argc!=3) { return 64; }
    bool const before=argc==3 && ::fast_io::string_view{argv[2],::fast_io::cstr_len(argv[2])}=="before";
    ::fast_io::dir_file directory{::fast_io::mnp::os_c_str(argv[1])};
    ::fast_io::native_mkdirat(::fast_io::at(directory),u8"nested");
    for(auto name:{u8"state.bin",u8"nested/state.bin"})
    { ::fast_io::native_file file{::fast_io::at(directory),::fast_io::mnp::os_c_str(name),::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};::fast_io::io::print(file,"DATA"); }
    fm::wasi_fd_ref_t root{};root.ptr->wasi_fd_storage=fm::wasi_fd_storage_t{fm::wasi_fd_type_e::dir};
    auto& entry=root.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.emplace_back().ptr->dir_stack;
    entry.name=u8"portable-root";entry.storage.file=::std::move(directory);
    ::uwvm2::object::memory::linear::native_memory_t memory{};memory.init_by_page_count(1u);
    ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<decltype(memory)> env{};
    env.wasip1_memory=::std::addressof(memory);env.fd_storage.fd_limit=8u;
    fm::wasi_fd_unique_ptr_t parent{};parent.fd_p->wasi_fd=root;
    parent.fd_p->rights_base=static_cast<abi::rights_t>(0x3fffffffu);parent.fd_p->rights_inherit=static_cast<abi::rights_t>(0x3fffffffu);
    env.fd_storage.opens.push_back(::std::move(parent));
    unsigned lost{};
    for(unsigned width{};width!=2u;++width)
    for(unsigned follow{};follow!=2u;++follow)
    for(auto raw:{u8"state.bin",u8"./state.bin",u8"././state.bin",u8"nested//./state.bin",u8"./nested/./state.bin",u8"nested/././state.bin",u8"nested///state.bin"})
    {
        auto size=::fast_io::cstr_len(raw);auto first=reinterpret_cast<::std::byte const*>(raw);
        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory(memory,32u,first,first+size);
        auto error=width==0u ? static_cast<unsigned>(fn::path_open(env,0,static_cast<abi::lookupflags_t>(follow),32u,
            static_cast<abi::wasi_size_t>(size),abi::oflags_t{},static_cast<abi::rights_t>(0x60006eu),abi::rights_t{},abi::fdflags_t{},16u))
            : static_cast<unsigned>(fn::path_open_wasm64(env,0,static_cast<abi::lookupflags_wasm64_t>(follow),32u,size,
                abi::oflags_wasm64_t{},static_cast<abi::rights_wasm64_t>(0x60006eu),abi::rights_wasm64_t{},abi::fdflags_wasm64_t{},16u));
        require(error==0u,"original WASI path_open accepts the valid spelling");
        auto number=::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory<::std::uint32_t>(memory,16u);
        auto retained=env.fd_storage.opens.index_unchecked(number).fd_p->wasi_fd;
        require(retained.ptr && retained.ptr->wasi_fd_storage.type==fm::wasi_fd_type_e::file,"original open issued a genuine owned regular file");
        bool const plain=::fast_io::u8string_view{raw,size}==u8"state.bin";
        if(before)
        { require(retained.ptr->checkpoint_reopenable==plain,"before fix only already-canonical spelling gets portable provenance");lost+=!plain; }
        else
        {
            require(retained.ptr->checkpoint_reopenable,"valid relative spelling retains mounted provenance");
            auto view=::fast_io::u8string_view{raw,size};
            auto expected=(view==u8"state.bin" || view==u8"./state.bin" || view==u8"././state.bin") ? ::fast_io::u8string_view{u8"state.bin"} : ::fast_io::u8string_view{u8"nested/state.bin"};
            require(retained.ptr->checkpoint_mount==u8"portable-root" && retained.ptr->checkpoint_path==expected &&
                retained.ptr->checkpoint_follow==(follow!=0u),"canonical path and saved follow retain original mount semantics");
        }
#if defined(_WIN32) && !defined(__CYGWIN__)
        auto& file=retained.ptr->wasi_fd_storage.storage.file_fd.file;
#else
        auto& file=retained.ptr->wasi_fd_storage.storage.file_fd;
#endif
        ::std::byte bytes[4u]{};::fast_io::operations::read_all_bytes(file,bytes,bytes+4u);
        require(bytes[0]==::std::byte{'D'} && bytes[3]==::std::byte{'A'},"actual file payload matches the same intended native resource");
        require(fn::fd_close_base(env,static_cast<abi::wasi_posix_fd_t>(number))==abi::errno_t::esuccess,"original WASI close retires the issued descriptor");
    }
    if(before)
    { require(lost==24u,"real wasm32/wasm64 opens lose provenance for all normalized spellings");::fast_io::io::println("wasip1_path_provenance_before ",checks," checks passed lost=",lost);return 0; }
    auto const& chain=root.ptr->wasi_fd_storage.storage.dir_stack.dir_stack;
    for(auto raw:{u8"../state.bin",u8"./../state.bin",u8"nested/../state.bin",u8"nested//../state.bin",u8"/state.bin",u8"//state.bin",u8"state.bin/",u8"a\\b",u8"a:stream",u8".",u8"././"})
    {
        fm::wasi_fd_ref_t value{};fm::record_checkpoint_path(*value.ptr,chain,::uwvm2::utils::container::u8string_view{raw,::fast_io::cstr_len(raw)},true);
        require(!value.ptr->checkpoint_reopenable && value.ptr->checkpoint_mount.empty() && value.ptr->checkpoint_path.empty() &&
            value.ptr->checkpoint_follow,"nonportable or empty normalized file path cannot mint reopen provenance");
    }
    char8_t nul[]{u8'a',u8'\0',u8'b'};fm::wasi_fd_ref_t value{};
    fm::record_checkpoint_path(*value.ptr,chain,::uwvm2::utils::container::u8string_view{nul,3u},false);require(!value.ptr->checkpoint_reopenable,"embedded NUL stays untracked");
    ::fast_io::u8string boundary{};boundary.append(u8"./");
    while(boundary.size()<4096u) { boundary.push_back(u8'a'); }
    fm::record_checkpoint_path(*value.ptr,chain,::uwvm2::utils::container::u8string_view{boundary.data(),boundary.size()},false);
    require(value.ptr->checkpoint_reopenable && value.ptr->checkpoint_path.size()==4094u,"maximum raw provenance size normalizes within the original allocation bound");
    boundary.push_back(u8'a');fm::record_checkpoint_path(*value.ptr,chain,::uwvm2::utils::container::u8string_view{boundary.data(),boundary.size()},true);
    require(!value.ptr->checkpoint_reopenable && value.ptr->checkpoint_path.empty(),"over-limit raw path does not allocate portable metadata even if shorter after normalization");
    require(value.ptr->checkpoint_follow,"failed provenance does not lose actual lookup mode");
    fm::record_checkpoint_path(*value.ptr,chain,u8"./目录//./状态.bin",false);
    require(value.ptr->checkpoint_reopenable && value.ptr->checkpoint_path==u8"目录/状态.bin","normalization preserves Unicode component bytes");
    ::fast_io::io::println("wasip1_path_provenance ",checks," checks passed");
}

// Real OS directory owners and original wasm32/wasm64 WASI calls.
// No native capture ticket, caller-injected handle or simulated filesystem.
#define UWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64
#include <uwvm2/runtime/lib/uwvm_runtime_wasip1_mount_identity.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get_wasm64.h>
#include <fast_io.h>
namespace fm=::uwvm2::imported::wasi::wasip1::fd_manager;
namespace abi=::uwvm2::imported::wasi::wasip1::abi;
namespace fn=::uwvm2::imported::wasi::wasip1::func;
namespace mem=::uwvm2::imported::wasi::wasip1::memory;
namespace mi=::uwvm2::runtime::lib::wasip1_mount_identity;
static unsigned checks{},cases{},unsupported{};
static void require(bool good,char const* why)
{ ++checks;if(!good) { ::fast_io::io::perrln("wasip1_root_selection: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
int main(int argc,char** argv)
{
    if(argc!=2) { return 64; }
    for(unsigned width{};width!=2u;++width)
    for(bool borrowed:{false,true})
    {
        ++cases;
        ::fast_io::dir_file owner{::fast_io::mnp::os_c_str(argv[1])};
        fm::wasi_fd_ref_t root{};root.ptr->wasi_fd_storage.reset_type(fm::wasi_fd_type_e::dir);
        auto& entry=root.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.emplace_back().ptr->dir_stack;
        entry.name=u8"portable-root";
        if(borrowed) { entry=fm::dir_stack_entry_t{true};entry.name=u8"portable-root";entry.storage.observer=owner; }
        else { entry.storage.file=::std::move(owner); }
        ::uwvm2::object::memory::linear::native_memory_t memory{};memory.init_by_page_count(1u);
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<decltype(memory)> env{};
        env.wasip1_memory=::std::addressof(memory);env.fd_storage.fd_limit=16u;
        fm::wasi_fd_unique_ptr_t parent{};parent.fd_p->wasi_fd=root;
        parent.fd_p->rights_base=abi::rights_t::right_path_open;
        parent.fd_p->rights_inherit=abi::rights_t::right_path_open|abi::rights_t::right_fd_fdstat_set_flags;
        env.fd_storage.opens.push_back(::std::move(parent));
        ::std::byte dot[]{::std::byte{'.'}};mem::write_all_to_memory(memory,32u,dot,dot+1u);
        auto open=[&](unsigned base,unsigned inherited,unsigned flags)
        {
            auto e=width==0u ? fn::path_open(env,0,abi::lookupflags_t{},32u,1u,abi::oflags_t::o_directory,
                static_cast<abi::rights_t>(base),static_cast<abi::rights_t>(inherited),static_cast<abi::fdflags_t>(flags),16u) :
                fn::path_open_wasm64(env,0,abi::lookupflags_wasm64_t{},32u,1u,abi::oflags_wasm64_t::o_directory,
                static_cast<abi::rights_wasm64_t>(base),static_cast<abi::rights_wasm64_t>(inherited),static_cast<abi::fdflags_wasm64_t>(flags),16u);
            return static_cast<unsigned>(e);
        };
        constexpr unsigned path=0x2000u,set=8u;
#if defined(_WIN32) && !defined(__CYGWIN__)
        unsigned constexpr nonblock{};
        require(open(path,0u,4u)==static_cast<unsigned>(abi::errno_t::enotsup) && env.fd_storage.opens.size()==1u,
            "Windows explicitly refuses native directory NONBLOCK without publishing a child");
        ++unsupported;
#else
        unsigned constexpr nonblock{4u};
#endif
        require(open(path,0u,0u)==0u,"original ABI issues wrong-state PATH_OPEN root");
        require(open(set,0u,0u)==0u,"original ABI issues independently reduced SET_FLAGS root");
        require(open(0u,path,nonblock)==0u,"original ABI issues inheriting-only root");
        require(open(path,0u,nonblock)==0u,"original ABI issues matching higher root");
        require(open(path|set,0u,0u)==0u,"original ABI issues one completely mutable root");
        auto fd=[&](unsigned n) { return env.fd_storage.opens.index_unchecked(n).fd_p; };
        unsigned observations{};
        auto observe=[&](fm::wasi_fd_t const* cell)->::std::optional<::std::uint16_t>
        {
            ++observations;
            unsigned n{};while(n<env.fd_storage.opens.size() && fd(n)!=cell) { ++n; }
            require(n<env.fd_storage.opens.size(),"only real live descriptor is inspected");
            auto e=width==0u ? static_cast<unsigned>(fn::fd_fdstat_get(env,n,128u)) :
                static_cast<unsigned>(fn::fd_fdstat_get_wasm64(env,n,128u));
            require(e==0u && mem::get_basic_wasm_type_from_memory<::std::uint8_t>(memory,128u)==3u,
                "original ABI supplies actual directory flags");
            return mem::get_basic_wasm_type_from_memory<::std::uint16_t>(memory,130u);
        };
        mi::descriptor_index index{};index.cell=fd(1);index.aliases={fd(1),fd(2),fd(3),fd(4)};
        require(index.select(path,0u)==fd(1),"ordinary path selection retains first sufficient descriptor");
        require(index.select_flags(path,0u,nonblock,observe)==(nonblock ? fd(4) : fd(1)),
            "complete matching higher root is selected despite earlier wrong state and separately reduced aliases");
        require(index.select_flags(0u,path,nonblock,observe)==fd(3),
            "inheriting rights are checked on the same actual descriptor");
        require(index.select_flags(path,path,nonblock,observe)==nullptr,
            "base and inheriting masks from different roots never combine");
        require(index.select_flags(path,0u,26u,observe)==nullptr,
            "synchronization difference never borrows another alias SET_FLAGS");
        auto read_count=observations;
        for(unsigned repeat{};repeat!=1024u;++repeat)
        {
            require(index.select_flags(path,0u,nonblock,observe)==(nonblock ? fd(4) : fd(1)),
                "cached selection retains the correct complete descriptor");
            require(index.select_flags(path,path,nonblock,observe)==nullptr,
                "cached missing authority stays denied");
        }
        require(observations==read_count && observations<=3u,
            "native observations and negative results cache within one immutable gated index");
        mi::descriptor_index wrong{};wrong.cell=fd(1);wrong.aliases={fd(1),fd(2),fd(3)};
        require(wrong.select_flags(path,0u,4u,observe)==nullptr,
            "wrong-state root and separate mutable root cannot manufacture complete authority");
        mi::descriptor_index mutable_root{};mutable_root.cell=fd(1);mutable_root.aliases={fd(1),fd(2),fd(3),fd(5)};
        require(mutable_root.select_flags(path,0u,4u,observe)==fd(5),
            "one higher descriptor holding both saved base rights and SET_FLAGS authorizes mutable flags");
        require(mutable_root.select_flags(path,0u,26u,observe)==nullptr,
            "even a complete mutable descriptor cannot change synchronization through SET_FLAGS");
        mi::descriptor_index reordered{};reordered.cell=fd(4);reordered.aliases={fd(4),fd(3),fd(2),fd(1)};
        require(reordered.select_flags(path,0u,nonblock,observe)==fd(4),
            "reordering preserves a sufficient exact-state selection");
        mi::descriptor_index failed{};failed.cell=fd(1);failed.aliases={fd(1),fd(4)};
        unsigned failed_reads{};
        auto unavailable=[&](fm::wasi_fd_t const* c)->::std::optional<::std::uint16_t>
        { if(c==fd(1)) { ++failed_reads;return ::std::nullopt; }return observe(c); };
        require(failed.select_flags(path,0u,nonblock,unavailable)==fd(4),
            "one unavailable observation does not hide a later authentic matching descriptor");
        require(failed.select_flags(path,0u,26u,unavailable)==nullptr && failed_reads==1u,
            "failed observations cache without gaining guessed flag authority");
        require(mi::same(fd(1)->wasi_fd,fd(4)->wasi_fd) &&
            fd(1)->wasi_fd.ptr!=fd(4)->wasi_fd.ptr,"independent actual roots share only genuine mount provenance");
        require(observe(fd(1))==0u && observe(fd(4))==nonblock,
            "selection leaves all target native flags unchanged");
        mi::descriptor_index empty{};
        require(empty.select(0u,0u)==nullptr && empty.select_flags(0u,0u,0u,observe)==nullptr,
            "empty index has no usable authority");
    }
    ::fast_io::io::println("wasip1_root_selection ",checks," checks passed cases=",cases," unsupported=",unsupported);
}

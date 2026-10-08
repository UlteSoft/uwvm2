// Actual WASI dot opens and FastIO directory owners; no native debug ticket.
#include <uwvm2/runtime/lib/uwvm_runtime_wasip1_mount_identity.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get.h>
#include <fast_io.h>
namespace fm=::uwvm2::imported::wasi::wasip1::fd_manager;
namespace abi=::uwvm2::imported::wasi::wasip1::abi;
namespace fn=::uwvm2::imported::wasi::wasip1::func;
namespace mi=::uwvm2::runtime::lib::wasip1_mount_identity;
static unsigned checks{};
static void require(bool value,char const* message)
{ ++checks;if(!value) { ::fast_io::io::perrln("wasip1_mount_identity: ",::fast_io::mnp::os_c_str(message));::fast_io::fast_terminate(); } }
static fm::wasi_fd_ref_t configured(char const* path)
{
    fm::wasi_fd_ref_t value{};
    value.ptr->wasi_fd_storage=fm::wasi_fd_storage_t{fm::wasi_fd_type_e::dir};
    auto& entry=value.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.emplace_back().ptr->dir_stack;
    entry.name=u8"same-guest-mount";entry.storage.file=::fast_io::dir_file{::fast_io::mnp::os_c_str(path)};
    return value;
}
int main(int argc,char** argv)
{
    if(argc<2) { return 64; }
    fm::wasi_fd_ref_t absent{fm::wasi_no_construct},empty{};
    require(mi::root(absent)==nullptr && mi::root(empty)==nullptr,"null and empty RC have no mount identity");
    auto root=configured(argv[1]),different=configured(argv[1]);
    require(mi::root(root)!=nullptr && mi::same(root,root),"actual configured directory is its own authority");
    require(!mi::same(root,different),"independently configured roots stay distinct even at the same host path");
    require(!mi::same(root,absent) && !mi::same(empty,root),"non-directory cannot impersonate mount");
    ::uwvm2::object::memory::linear::native_memory_t memory{};memory.init_by_page_count(1u);
    ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<decltype(memory)> env{};
    env.wasip1_memory=::std::addressof(memory);env.fd_storage.fd_limit=32u;
    fm::wasi_fd_unique_ptr_t issued{};issued.fd_p->wasi_fd=root;
    issued.fd_p->rights_base=static_cast<abi::rights_t>(0x3fffffffu);issued.fd_p->rights_inherit=static_cast<abi::rights_t>(0x3fffffffu);
    env.fd_storage.opens.push_back(::std::move(issued));
    ::std::byte dot[]{::std::byte{'.'}};::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory(memory,32u,dot,dot+1u);
    for(unsigned follow{};follow!=2u;++follow)
    {
        require(fn::path_open(env,0,static_cast<abi::lookupflags_t>(follow),32u,1u,abi::oflags_t::o_directory,
            abi::rights_t{},abi::rights_t{},abi::fdflags_t{},16u)==abi::errno_t::esuccess,"original path_open dot succeeds");
        auto n=::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory<::std::uint32_t>(memory,16u);
        auto& opened=env.fd_storage.opens.index_unchecked(n).fd_p->wasi_fd;
        require(opened.ptr!=root.ptr && mi::same(opened,root),"dot is a distinct WASI resource of the same actual mount");
        require(opened.ptr->checkpoint_follow==(follow!=0u),"original WASI records dot follow mode");
        auto copy=mi::copy_root(opened,follow!=0u);
        require(copy.ptr!=opened.ptr && copy.ptr!=root.ptr && mi::same(copy,root),"root restore retains authority without merging resource RCs");
        require(copy.ptr->checkpoint_follow==(follow!=0u) && copy.ptr->wasi_fd_storage.storage.dir_stack.checkpoint_mount_origin.has_value(),"restored follow and owned provenance retained");
        auto alias=copy;require(alias.ptr==copy.ptr,"descriptor aliases retain one actual resource RC");
        auto again=mi::copy_root(copy,follow==0u);
        require(again.ptr!=copy.ptr && mi::same(again,root) && again.ptr->checkpoint_follow==(follow==0u),"repeated restores keep original authority and each saved follow");
        // Simulate the independently opened root handle used for flag variants.
        auto independent=configured(argv[1]);mi::provenance(independent,copy,follow!=0u);
        require(independent.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.front_unchecked().ptr!=
            root.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.front_unchecked().ptr &&
            mi::same(independent,root),"independent native handle retains authentic origin");
        require(!mi::same(independent,different),"origin does not unify another configured same-name root");
        // The real path_open directory-chain copy must preserve a restored
        // origin even when its current native root entry is an independent one.
        env.fd_storage.opens.index_unchecked(0u).fd_p->wasi_fd=independent;
        auto opened_again=fn::path_open(env,0,abi::lookupflags_t::lookup_symlink_follow,32u,1u,
            abi::oflags_t::o_directory,abi::rights_t{},abi::rights_t{},abi::fdflags_t{},16u);
        auto child_number=::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory<::std::uint32_t>(memory,16u);
        require(opened_again==abi::errno_t::esuccess &&
            mi::same(env.fd_storage.opens.index_unchecked(child_number).fd_p->wasi_fd,root),
            "original guest dot open propagates authority after an independent-handle restore");
        require(fn::fd_close_base(env,static_cast<abi::wasi_posix_fd_t>(child_number))==abi::errno_t::esuccess,
            "original guest close retires derived restored root");
        env.fd_storage.opens.index_unchecked(0u).fd_p->wasi_fd=root;
        require(fn::fd_close_base(env,static_cast<abi::wasi_posix_fd_t>(n))==abi::errno_t::esuccess,"original WASI close retires dot FD");
        require(mi::same(copy,independent),"owned origin survives guest close");
        auto const before=root.ptr->checkpoint_follow;
        copy.ptr->checkpoint_follow=!before;require(root.ptr->checkpoint_follow==before,"staged metadata edits do not mutate live target resource");
    }
    auto restored=mi::copy_root(root,true);
    auto pin=restored.ptr->wasi_fd_storage.storage.dir_stack.checkpoint_mount_origin->ptr;
    root=fm::wasi_fd_ref_t{fm::wasi_no_construct};env.fd_storage.opens.clear();
    require(mi::root(restored)->ptr==pin && pin->refcount.load(::std::memory_order_relaxed)>=1u,"replacement owns mount origin after target retires");
    ::fast_io::native_file child{::fast_io::at(pin->dir_stack.storage.file),u8"origin-live.bin",
        ::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::excl};
    ::fast_io::io::print(child,"owned-origin");
    require(::fast_io::status(child).size==12u,"retained real directory capability remains usable");
    ::fast_io::io::println("wasip1_mount_identity ",checks," checks passed");
}

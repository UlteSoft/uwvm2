// The real stable table must reject a disabled target before any memory or FD effect.
#include <uwvm2/uwvm/wasm/type/wasip1_api.h>
#include <uwvm2/uwvm/wasm/storage/impl.h>
#include <uwvm2/uwvm/imported/wasi/wasip1/storage/env.h>
#include <fast_io.h>
#include <type_traits>
#include <cstring>
#include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
namespace st=::uwvm2::uwvm::imported::wasi::wasip1::storage;
namespace ty=::uwvm2::uwvm::wasm::type;
namespace abi=::uwvm2::imported::wasi::wasip1::abi;
static unsigned checked{};
template<class Result,class... Args>
static bool denies(Result (*function)(Args...))
{
    if(function==nullptr) { return false; }
    ++checked;
    if constexpr(::std::is_void_v<Result>) { function(Args{}...); return true; }
    else { return function(Args{}...)==static_cast<Result>(abi::errno_t::enotcapable); }
}
#define CHECK(v) do { if(!(v)) { ::fast_io::io::perrln("FAIL cached WASI denial line=",__LINE__);return 1; } } while(false)
int main()
{
    ::uwvm2::uwvm::wasm::storage::preload_expose_wasip1_host_api=true;
    auto const* api=ty::uwvm_get_wasip1_host_api_v1();CHECK(api!=nullptr);
    auto* restricted=st::try_create_targetless_wasip1_module_override(
        ::uwvm2::utils::container::u8string_view{u8"disabled-cache",14});
    CHECK(restricted!=nullptr);restricted->expose_host_api_is_set=true;restricted->expose_host_api=false;
    {
    st::scoped_current_wasip1_target_t target{st::wasip1_module_target_kind_t::preloaded_dl,u8"disabled-cache"};
    CHECK(ty::uwvm_get_wasip1_host_api_v1()==nullptr);
    CHECK(denies(api->args_get));
    CHECK(denies(api->args_sizes_get));
    CHECK(denies(api->clock_res_get));
    CHECK(denies(api->clock_time_get));
    CHECK(denies(api->environ_get));
    CHECK(denies(api->environ_sizes_get));
    CHECK(denies(api->fd_advise));
    CHECK(denies(api->fd_allocate));
    CHECK(denies(api->fd_close));
    CHECK(denies(api->fd_datasync));
    CHECK(denies(api->fd_fdstat_get));
    CHECK(denies(api->fd_fdstat_set_flags));
    CHECK(denies(api->fd_fdstat_set_rights));
    CHECK(denies(api->fd_filestat_get));
    CHECK(denies(api->fd_filestat_set_size));
    CHECK(denies(api->fd_filestat_set_times));
    CHECK(denies(api->fd_pread));
    CHECK(denies(api->fd_prestat_dir_name));
    CHECK(denies(api->fd_prestat_get));
    CHECK(denies(api->fd_pwrite));
    CHECK(denies(api->fd_read));
    CHECK(denies(api->fd_readdir));
    CHECK(denies(api->fd_renumber));
    CHECK(denies(api->fd_seek));
    CHECK(denies(api->fd_sync));
    CHECK(denies(api->fd_tell));
    CHECK(denies(api->fd_write));
    CHECK(denies(api->path_create_directory));
    CHECK(denies(api->path_filestat_get));
    CHECK(denies(api->path_filestat_set_times));
    CHECK(denies(api->path_link));
    CHECK(denies(api->path_open));
    CHECK(denies(api->path_readlink));
    CHECK(denies(api->path_remove_directory));
    CHECK(denies(api->path_rename));
    CHECK(denies(api->path_symlink));
    CHECK(denies(api->path_unlink_file));
    CHECK(denies(api->poll_oneoff));
    CHECK(denies(api->proc_exit));
    CHECK(denies(api->proc_raise));
    CHECK(denies(api->random_get));
    CHECK(denies(api->sched_yield));
#if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
    CHECK(denies(api->sock_accept));
    CHECK(denies(api->sock_recv));
    CHECK(denies(api->sock_send));
    CHECK(denies(api->sock_shutdown));
#endif
#if defined(UWVM_IMPORT_WASI_WASIP1_WASM64)
    CHECK(denies(api->args_get_wasm64));
    CHECK(denies(api->args_sizes_get_wasm64));
    CHECK(denies(api->clock_res_get_wasm64));
    CHECK(denies(api->clock_time_get_wasm64));
    CHECK(denies(api->environ_get_wasm64));
    CHECK(denies(api->environ_sizes_get_wasm64));
    CHECK(denies(api->fd_advise_wasm64));
    CHECK(denies(api->fd_allocate_wasm64));
    CHECK(denies(api->fd_close_wasm64));
    CHECK(denies(api->fd_datasync_wasm64));
    CHECK(denies(api->fd_fdstat_get_wasm64));
    CHECK(denies(api->fd_fdstat_set_flags_wasm64));
    CHECK(denies(api->fd_fdstat_set_rights_wasm64));
    CHECK(denies(api->fd_filestat_get_wasm64));
    CHECK(denies(api->fd_filestat_set_size_wasm64));
    CHECK(denies(api->fd_filestat_set_times_wasm64));
    CHECK(denies(api->fd_pread_wasm64));
    CHECK(denies(api->fd_prestat_dir_name_wasm64));
    CHECK(denies(api->fd_prestat_get_wasm64));
    CHECK(denies(api->fd_pwrite_wasm64));
    CHECK(denies(api->fd_read_wasm64));
    CHECK(denies(api->fd_readdir_wasm64));
    CHECK(denies(api->fd_renumber_wasm64));
    CHECK(denies(api->fd_seek_wasm64));
    CHECK(denies(api->fd_sync_wasm64));
    CHECK(denies(api->fd_tell_wasm64));
    CHECK(denies(api->fd_write_wasm64));
    CHECK(denies(api->path_create_directory_wasm64));
    CHECK(denies(api->path_filestat_get_wasm64));
    CHECK(denies(api->path_filestat_set_times_wasm64));
    CHECK(denies(api->path_link_wasm64));
    CHECK(denies(api->path_open_wasm64));
    CHECK(denies(api->path_readlink_wasm64));
    CHECK(denies(api->path_remove_directory_wasm64));
    CHECK(denies(api->path_rename_wasm64));
    CHECK(denies(api->path_symlink_wasm64));
    CHECK(denies(api->path_unlink_file_wasm64));
    CHECK(denies(api->poll_oneoff_wasm64));
    CHECK(denies(api->proc_exit_wasm64));
    CHECK(denies(api->proc_raise_wasm64));
    CHECK(denies(api->random_get_wasm64));
    CHECK(denies(api->sched_yield_wasm64));
# if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
    CHECK(denies(api->sock_accept_wasm64));
    CHECK(denies(api->sock_recv_wasm64));
    CHECK(denies(api->sock_send_wasm64));
    CHECK(denies(api->sock_shutdown_wasm64));
# endif
#endif
    }
    auto& environment=st::default_wasip1_env;
    environment.argv.emplace_back(u8"one");
    ::uwvm2::object::memory::linear::native_memory_t memory{};
    memory.init_by_page_count(1,4);
    st::scoped_current_wasip1_env_t selected{environment};
    st::scoped_current_wasip1_memory_t binding{environment,&memory};
    st::scoped_current_wasip1_target_t permitted{st::wasip1_module_target_kind_t::preloaded_dl,u8"permitted-cache"};
    CHECK(ty::uwvm_get_wasip1_host_api_v1()!=nullptr);
    CHECK(api->args_sizes_get(0,4)==abi::errno_t::esuccess);
    ::std::uint_least32_t count{},size{};
    ::std::memcpy(&count,memory.memory_begin,4);
    ::std::memcpy(&size,memory.memory_begin+4,4);
    CHECK(count==1 && size==4);
#if defined(UWVM_IMPORT_WASI_WASIP1_WASM64)
    CHECK(api->args_sizes_get_wasm64(8,16)==abi::errno_t::esuccess);
    ::std::uint_least64_t count64{},size64{};
    ::std::memcpy(&count64,memory.memory_begin+8,8);
    ::std::memcpy(&size64,memory.memory_begin+16,8);
    CHECK(count64==1 && size64==4);
#endif
    ::fast_io::io::println("PASS actual cached WASI table denial entries=",checked);
}
#include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>

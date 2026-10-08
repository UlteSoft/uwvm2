#include "posix_test_abi.h"
#include <uwvm2/utils/control/sealed_input.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close_wasm64.h>
#include <cstdlib>
#include <cstring>
#include <string>
namespace ctl=uwvm2::utils::control;
namespace abi=uwvm2::imported::wasi::wasip1::abi;
namespace wasi=uwvm2::imported::wasi::wasip1;
static unsigned checks{};
#define CHECK(x) do{++checks;if(!(x)){fast_io::io::perrln("FAIL ",__LINE__," ",fast_io::mnp::os_c_str(#x));fast_io::fast_terminate();}}while(false)
int main()
{
 using memory_type=uwvm2::object::memory::linear::native_memory_t;
 memory_type memory;memory.init_by_page_count(1);
 wasi::environment::wasip1_environment<memory_type> env{.wasip1_memory=&memory,.fd_storage={.fd_limit=64}};
 env.fd_storage.opens.resize(4);
 auto& entry=*env.fd_storage.opens[3].fd_p;entry.rights_base=static_cast<abi::rights_t>(-1);entry.rights_inherit=static_cast<abi::rights_t>(-1);
 entry.wasi_fd.ptr->wasi_fd_storage.reset_type(wasi::fd_manager::wasi_fd_type_e::dir);
 wasi::fd_manager::dir_stack_entry_ref_t directory;
 directory.ptr->dir_stack.storage.file=fast_io::dir_file{u8"."};
 entry.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.push_back(std::move(directory));
 int input=control_test::posix_abi::open_noexcept("commands",O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(input>=0);CHECK(control_test::posix_abi::write_noexcept(input,"help\n",5)==5);
 CHECK(control_test::posix_abi::link_noexcept("commands","hardlink")==0);CHECK(control_test::posix_abi::symlink_noexcept("commands","alias")==0);
 CHECK(ctl::seal_console_input_host_api(input)==ctl::sealed_input_status::ok);
 for(bool wasm64:{false,true})
 {
  auto invoke=[&](std::string const& name,abi::rights_t rights,abi::oflags_t flags)
  {
   wasi::memory::write_all_to_memory_wasm32(memory,1024,reinterpret_cast<std::byte const*>(name.data()),reinterpret_cast<std::byte const*>(name.data()+name.size()));
   unsigned status{};
   if(wasm64)status=static_cast<unsigned>(wasi::func::path_open_wasm64(env,3,abi::lookupflags_wasm64_t::lookup_symlink_follow,1024,name.size(),static_cast<abi::oflags_wasm64_t>(flags),static_cast<abi::rights_wasm64_t>(rights),{}, {},4096));
   else status=static_cast<unsigned>(wasi::func::path_open(env,3,abi::lookupflags_t::lookup_symlink_follow,1024,static_cast<abi::wasi_size_t>(name.size()),flags,rights,{}, {},4096));
   if(status==0)
   {
    auto fd=wasi::memory::get_basic_wasm_type_from_memory_wasm32<abi::wasi_posix_fd_t>(memory,4096);
    CHECK(wasi::func::fd_close(env,fd)==abi::errno_t::esuccess);
   }
   return status;
  };
  for(auto name:{"commands","hardlink","alias"})
  {
   CHECK(invoke(name,abi::rights_t::right_fd_read,{})==static_cast<unsigned>(abi::errno_t::eacces));
   CHECK(invoke(name,abi::rights_t::right_fd_write,{})==static_cast<unsigned>(abi::errno_t::eacces)); // implicit fast_io out truncation
   CHECK(invoke(name,abi::rights_t::right_fd_write,abi::oflags_t::o_trunc)==static_cast<unsigned>(abi::errno_t::eacces));
  }
  CHECK(invoke("benign",abi::rights_t::right_fd_write,abi::oflags_t::o_creat|abi::oflags_t::o_trunc)==0);
  struct stat st{};CHECK(control_test::posix_abi::fstat_noexcept(input,&st)==0&&st.st_size==5);
 }
 ctl::unseal_console_input_after_guest_drain_host_api();control_test::posix_abi::close_noexcept(input);
 CHECK(control_test::posix_abi::unlink_noexcept("commands")==0);CHECK(control_test::posix_abi::unlink_noexcept("hardlink")==0);CHECK(control_test::posix_abi::unlink_noexcept("alias")==0);CHECK(control_test::posix_abi::unlink_noexcept("benign")==0);
 fast_io::io::println("PASS sealed input actual WASI32/WASI64 path_open: ",checks," checks");
}

// Actual macOS / FreeBSD kernel identities; no guest transport capability.
#include <uwvm2/utils/control/sealed_input.h>
#include <fast_io_dsal/string.h>
#include <fast_io_device.h>
#if defined(__FreeBSD__)
# include <libutil.h>
#else
# include <util.h>
#endif
#include <stdexcept>
namespace ctl=uwvm2::utils::control;
static unsigned checks{};
static void require(bool v,char const* name)
{
 ++checks;fast_io::io::println("SEALED_CHECK ",checks," ",fast_io::mnp::os_c_str(name)," ",fast_io::mnp::os_c_str(v?"PASS":"FAIL"));
 if(!v) { throw std::runtime_error{name}; }
}
int main()
{
 try
 {
#if defined(__FreeBSD__)
  fast_io::native_pipe pipe;
  require(ctl::seal_console_input_host_api(pipe.in().native_handle())==ctl::sealed_input_status::unsupported_platform,"shared blocking pipe rejected");
  require(!ctl::console_input_sealed(),"failed pipe admission publishes nothing");
#endif
  int master{},slave{};require(fast_io::noexcept_call(::openpty,&master,&slave,nullptr,nullptr,nullptr)==0,"actual PTY");
  fast_io::native_file m{master},s{slave};
  require(ctl::seal_console_input_host_api(master)!=ctl::sealed_input_status::ok,"master cannot be command source");
  require(ctl::seal_console_input_host_api(slave)==ctl::sealed_input_status::ok,"slave protected");
  require(ctl::inspect_guest_file_host_api(master)==ctl::sealed_input_decision::denied,"master input alias denied");
  require(ctl::inspect_guest_output_host_api(master)==ctl::sealed_input_decision::denied,"master write injection denied");
  require(ctl::inspect_guest_file_host_api(slave)==ctl::sealed_input_decision::denied,"slave read denied");
  require(ctl::inspect_guest_output_host_api(slave)==ctl::sealed_input_decision::allow,"launch slave output permitted");
  fast_io::native_file duplicate{fast_io::io_dup,s};
  require(ctl::inspect_guest_file_host_api(duplicate.native_handle())==ctl::sealed_input_decision::denied,"duplicate slave denied");
  auto null_file=ctl::open_guest_path_sealed_host_api(AT_FDCWD,"/dev/null",O_WRONLY,0600);
  require(null_file.descriptor>=0,"exact kernel null sink allowed");fast_io::native_file null_owner{null_file.descriptor};
  ctl::unseal_console_input_after_guest_drain_host_api();
  fast_io::native_white_hole entropy;std::uint64_t token{};
  auto* bytes=reinterpret_cast<std::byte*>(&token);fast_io::operations::read_all_bytes(entropy,bytes,bytes+sizeof(token));
  auto name=fast_io::concat_fast_io("/tmp/uwvm-sealed-",fast_io::mnp::hex(token));auto alias=fast_io::concat_fast_io(name,".alias");
  fast_io::native_file f{name,fast_io::open_mode::in|fast_io::open_mode::out|fast_io::open_mode::creat|fast_io::open_mode::excl,static_cast<fast_io::perms>(0600)};
  struct cleanup { fast_io::string const& a;fast_io::string const& b;~cleanup(){try{fast_io::native_unlinkat(fast_io::posix_at_fdcwd(),b);}catch(...){}try{fast_io::native_unlinkat(fast_io::posix_at_fdcwd(),a);}catch(...){}} } remove{name,alias};
  fast_io::io::print(f,"sealed");fast_io::native_linkat(fast_io::posix_at_fdcwd(),name,fast_io::posix_at_fdcwd(),alias);
  require(ctl::seal_console_input_host_api(f.native_handle())==ctl::sealed_input_status::ok,"regular input protected");
  auto denied=ctl::open_guest_path_sealed_host_api(AT_FDCWD,alias.c_str(),O_RDWR|O_TRUNC,0600);
  require(denied.descriptor==-1 && denied.error==EACCES,"hard-link truncate rejected before effect");
  auto st=fast_io::posix_status_nothrow(f);
  require(bool(st) && st.value.size==6,"protected bytes remain intact");
  ctl::unseal_console_input_after_guest_drain_host_api();
  fast_io::io::println("sealed_console_bsd_platform PASS checks=",checks);
 }
 catch(std::exception const& e) { fast_io::io::perrln("sealed console failure ",fast_io::mnp::os_c_str(e.what()));return 1; }
 catch(...) { fast_io::io::perrln("sealed console platform IO failure");return 1; }
}

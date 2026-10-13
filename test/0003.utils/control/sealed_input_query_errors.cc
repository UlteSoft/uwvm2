#include "posix_test_abi.h"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <utility>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <fast_io.h>
static unsigned long injected_request{};
static int injected_error{}, injected_times{}, attempts{};
// posix_test_abi.h has already loaded the production ABI declarations. Replace
// only the later sealed-input call token, not the libc ABI declaration/symbol;
// libc, sanitizer and fast_io calls remain outside this isolated error hook.
// A macro for `ioctl` cannot intercept `ioctl_noexcept asm("ioctl")`.
namespace uwvm2::utils::control::posix_abi
{
 inline int test_query_ioctl_noexcept(int fd,unsigned long request,unsigned* out) noexcept
 {
  // [out,out+1) is the real caller's complete unsigned query cell;
  // the native borrow is forwarded unchanged and not dereferenced by the hook.
  if(request==injected_request && injected_times){--injected_times;++attempts;errno=injected_error;return -1;}
  return ioctl_noexcept(fd,request,out); // Real assembly-linked noexcept ABI.
 }
}
#define ioctl_noexcept test_query_ioctl_noexcept
#include <uwvm2/utils/control/sealed_input.h>
#undef ioctl_noexcept
namespace ctl=uwvm2::utils::control;
static unsigned checks{};
#define CHECK(x) do{++checks;if(!(x)){fast_io::io::perrln("FAIL ",__LINE__," ",fast_io::mnp::os_c_str(#x));fast_io::fast_terminate();}}while(false)
static void inject(unsigned long request,int error,int times=1){injected_request=request;injected_error=error;injected_times=times;attempts=0;}
int main()
{
 int master=control_test::posix_abi::posix_openpt_noexcept(O_RDWR|O_NOCTTY|O_CLOEXEC);CHECK(master>=0);CHECK(control_test::posix_abi::grantpt_noexcept(master)==0&&control_test::posix_abi::unlockpt_noexcept(master)==0);
 int slave=control_test::posix_abi::open_noexcept(control_test::posix_abi::ptsname_noexcept(master),O_RDWR|O_NOCTTY|O_CLOEXEC);CHECK(slave>=0);
 for(int error:{EPERM,EIO,EINVAL})
 {
  inject(TIOCGDEV,error);CHECK(ctl::seal_console_input_host_api(slave)==ctl::sealed_input_status::identity_unavailable);
  CHECK(attempts==1&&injected_times==0);CHECK(!ctl::console_input_sealed());
 }
 inject(TIOCGDEV,EINTR,3);CHECK(ctl::seal_console_input_host_api(slave)==ctl::sealed_input_status::ok);CHECK(attempts==3);
 for(auto request:{TIOCGDEV,TIOCGPTN})for(int error:{EPERM,EIO,EINVAL})
 {
  inject(request,error);CHECK(ctl::inspect_guest_output_host_api(slave)==ctl::sealed_input_decision::identity_unavailable);
  CHECK(attempts==1&&injected_times==0);
  inject(request,error);CHECK(ctl::inspect_guest_file_host_api(slave)==ctl::sealed_input_decision::identity_unavailable);
  CHECK(attempts==1&&injected_times==0);
 }
 inject(TIOCGPTN,EINTR,2);CHECK(ctl::inspect_guest_output_host_api(master)==ctl::sealed_input_decision::denied);CHECK(attempts==2);
 ctl::unseal_console_input_after_guest_drain_host_api();control_test::posix_abi::close_noexcept(slave);control_test::posix_abi::close_noexcept(master);
 fast_io::io::println("PASS sealed input injected ioctl errors: ",checks," checks");
}

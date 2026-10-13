#include "posix_test_abi.h"
#include <uwvm2/utils/control/sealed_input.h>
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <vector>
namespace ctl=uwvm2::utils::control;
static unsigned checks{};
#define CHECK(x) do{++checks;if(!(x)){fast_io::io::perrln("FAIL ",__LINE__," ",fast_io::mnp::os_c_str(#x)," errno=",errno);fast_io::fast_terminate();}}while(false)
static unsigned fd_count(){unsigned n{};auto* d=control_test::posix_abi::opendir_noexcept("/proc/self/fd");CHECK(d);while(control_test::posix_abi::readdir_noexcept(d))++n;control_test::posix_abi::closedir_noexcept(d);return n;}
static void deny(std::string const& path,int flags)
{auto opened=ctl::open_guest_path_sealed_host_api(AT_FDCWD,path.c_str(),flags,0600);CHECK(opened.descriptor==-1 && opened.error==EACCES);}
int main()
{
 CHECK(!ctl::console_input_sealed());CHECK(ctl::inspect_guest_file_host_api(-1)==ctl::sealed_input_decision::allow);
 CHECK(ctl::seal_console_input_host_api(-1)==ctl::sealed_input_status::invalid_descriptor);
 std::array<char,80> pattern{};std::strcpy(pattern.data(),"/dev/shm/uwvm-sealed-input-XXXXXX");CHECK(control_test::posix_abi::mkdtemp_noexcept(pattern.data()));
 std::string base=pattern.data(),input=base+"/commands",hard=base+"/alias",link=base+"/symlink",other=base+"/other";
 int fd=control_test::posix_abi::open_noexcept(input.c_str(),O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=0);CHECK(control_test::posix_abi::write_noexcept(fd,"help\n",5)==5);
 CHECK(control_test::posix_abi::link_noexcept(input.c_str(),hard.c_str())==0);CHECK(control_test::posix_abi::symlink_noexcept("commands",link.c_str())==0);
 CHECK(ctl::seal_console_input_host_api(fd)==ctl::sealed_input_status::ok);
 CHECK(ctl::seal_console_input_host_api(fd)==ctl::sealed_input_status::already_sealed);
 int output=control_test::posix_abi::open_noexcept(hard.c_str(),O_WRONLY|O_CLOEXEC);CHECK(output>=0);
 CHECK(ctl::inspect_guest_output_host_api(output)==ctl::sealed_input_decision::denied);control_test::posix_abi::close_noexcept(output);
 for(auto const& path:{input,hard,link,"/proc/self/fd/"+std::to_string(fd),"/dev/fd/"+std::to_string(fd)})
  for(int flags:{O_RDONLY,O_WRONLY,O_RDWR,O_RDWR|O_TRUNC,O_WRONLY|O_CREAT|O_TRUNC})deny(path,flags);
 struct stat status{};CHECK(control_test::posix_abi::fstat_noexcept(fd,&status)==0 && status.st_size==5);
 auto normal=ctl::open_guest_path_sealed_host_api(AT_FDCWD,other.c_str(),O_RDWR|O_CREAT|O_TRUNC,0600);CHECK(normal.descriptor>=0);
 CHECK(control_test::posix_abi::write_noexcept(normal.descriptor,"benign",6)==6);control_test::posix_abi::close_noexcept(normal.descriptor);
 normal=ctl::open_guest_path_sealed_host_api(AT_FDCWD,other.c_str(),O_RDWR|O_TRUNC,0600);CHECK(normal.descriptor>=0);
 CHECK(control_test::posix_abi::fstat_noexcept(normal.descriptor,&status)==0 && status.st_size==0);control_test::posix_abi::close_noexcept(normal.descriptor);
 auto const before=fd_count();std::atomic<unsigned> failures{};std::vector<std::thread> threads;
 for(unsigned t{};t!=8;++t)threads.emplace_back([&]{for(unsigned n{};n!=128;++n){auto r=ctl::open_guest_path_sealed_host_api(AT_FDCWD,hard.c_str(),O_WRONLY|O_TRUNC,0600);if(r.descriptor!=-1||r.error!=EACCES)++failures;}});
 for(auto& t:threads)t.join();CHECK(failures==0);CHECK(fd_count()==before);CHECK(control_test::posix_abi::fstat_noexcept(fd,&status)==0 && status.st_size==5);
 ctl::unseal_console_input_after_guest_drain_host_api();control_test::posix_abi::close_noexcept(fd);
 int pipefd[2];CHECK(control_test::posix_abi::pipe2_noexcept(pipefd,O_CLOEXEC)==0);int saved=control_test::posix_abi::dup_noexcept(0);CHECK(saved>=0);CHECK(control_test::posix_abi::dup2_noexcept(pipefd[0],0)==0);
 CHECK(ctl::seal_console_input_host_api(0)==ctl::sealed_input_status::ok);
 deny("/proc/self/fd/0",O_RDONLY);deny("/dev/fd/0",O_WRONLY);
 CHECK(ctl::inspect_guest_output_host_api(pipefd[1])==ctl::sealed_input_decision::denied);
 CHECK(control_test::posix_abi::write_noexcept(pipefd[1],"q",1)==1);char c{};CHECK(control_test::posix_abi::read_noexcept(0,&c,1)==1&&c=='q');
 ctl::unseal_console_input_after_guest_drain_host_api();CHECK(control_test::posix_abi::dup2_noexcept(saved,0)==0);control_test::posix_abi::close_noexcept(saved);control_test::posix_abi::close_noexcept(pipefd[0]);control_test::posix_abi::close_noexcept(pipefd[1]);
 int master=control_test::posix_abi::posix_openpt_noexcept(O_RDWR|O_NOCTTY|O_CLOEXEC);CHECK(master>=0);CHECK(control_test::posix_abi::grantpt_noexcept(master)==0&&control_test::posix_abi::unlockpt_noexcept(master)==0);
 auto* slave_name=control_test::posix_abi::ptsname_noexcept(master);CHECK(slave_name);std::string slave_path=slave_name;
 int slave=control_test::posix_abi::open_noexcept(slave_path.c_str(),O_RDWR|O_NOCTTY|O_CLOEXEC);CHECK(slave>=0);
 CHECK(ctl::seal_console_input_host_api(master)==ctl::sealed_input_status::identity_unavailable);
 auto child=control_test::posix_abi::fork_noexcept();CHECK(child>=0);
 if(child==0)
 {
  if(control_test::posix_abi::setsid_noexcept()<0||control_test::posix_abi::ioctl_noexcept(slave,TIOCSCTTY,0)!=0)std::abort();
  CHECK(ctl::seal_console_input_host_api(slave)==ctl::sealed_input_status::ok);
  deny(slave_path,O_RDWR);deny("/dev/tty",O_RDONLY);deny("/proc/self/fd/"+std::to_string(slave),O_RDWR);
  CHECK(ctl::inspect_guest_output_host_api(slave)==ctl::sealed_input_decision::allow);
  CHECK(ctl::inspect_guest_output_host_api(master)==ctl::sealed_input_decision::denied);
  deny("/proc/self/fd/"+std::to_string(master),O_RDWR);
  ctl::unseal_console_input_after_guest_drain_host_api();control_test::posix_abi::_exit_noexcept(0);
 }
 int child_status{};CHECK(control_test::posix_abi::waitpid_noexcept(child,&child_status,0)==child&&WIFEXITED(child_status)&&WEXITSTATUS(child_status)==0);
 control_test::posix_abi::close_noexcept(slave);control_test::posix_abi::close_noexcept(master);
 CHECK(control_test::posix_abi::unlink_noexcept(input.c_str())==0);CHECK(control_test::posix_abi::unlink_noexcept(hard.c_str())==0);CHECK(control_test::posix_abi::unlink_noexcept(link.c_str())==0);CHECK(control_test::posix_abi::unlink_noexcept(other.c_str())==0);CHECK(control_test::posix_abi::rmdir_noexcept(base.c_str())==0);
 fast_io::io::println("PASS sealed input: ",checks," parent checks, actual PTY child aliases, 1024 concurrent denied opens");
}

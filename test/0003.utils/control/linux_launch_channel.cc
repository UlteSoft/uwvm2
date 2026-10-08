#include "posix_test_abi.h"
#include "buffer_helpers.h"
using namespace control_test;
#include <uwvm2/utils/control/impl.h>
#include <cstdlib>
#include <dirent.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <string_view>
namespace ctl=uwvm2::utils::control;
static unsigned checks{};
static void check(bool yes,int line)
{++checks;if(!yes){fast_io::io::perrln("FAIL launch channel pid=",control_test::posix_abi::getpid_noexcept()," line=",line," errno=",errno);fast_io::fast_terminate();}}
#define CHECK(value) check(bool(value),__LINE__)
static ctl::launch_config config()
{ctl::launch_config c;c.debug_enabled=c.replacement_enabled=true;c.compiler=ctl::backend::llvm;c.instance[0]=42u;return c;}
static std::vector<ctl::wire_byte> frame(std::uint64_t id=1)
{
    std::vector<ctl::wire_byte> bytes(ctl::header_bytes);auto c=config();
    ctl::output_buffer output{bytes};
    auto r=ctl::encode_frame({ctl::operation::status,0,c.instance,1,id},{},output);CHECK(r.status==ctl::error::none);return bytes;
}
static unsigned descriptors()
{unsigned n{};auto* dir=control_test::posix_abi::opendir_noexcept("/proc/self/fd");CHECK(dir!=nullptr);while(control_test::posix_abi::readdir_noexcept(dir))++n;control_test::posix_abi::closedir_noexcept(dir);return n;}
static ctl::control_session::receive_result receive(ctl::linux_launch_channel& channel)
{
    for(unsigned i=0;i!=200;++i)
    {
        auto r=channel.receive();if(r.status!=ctl::error::would_block)return r;
        pollfd p{channel.native_handle(),POLLIN,0};CHECK(control_test::posix_abi::poll_noexcept(&p,1,50)>=0);
    }
    CHECK(false);return {ctl::error::transport_failure,0,{}};
}
static void child_ok(pid_t pid)
{int status{};CHECK(control_test::posix_abi::waitpid_noexcept(pid,&status,0)==pid);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==0);}
static void send_rights(int socket,ctl::input_buffer bytes,unsigned count=1)
{
    int fd=control_test::posix_abi::open_noexcept("/dev/null",O_RDONLY|O_CLOEXEC);CHECK(fd>=0);
    iovec io{const_cast<ctl::wire_byte*>(bytes.curr_ptr),ctl::remaining_bytes(bytes)};
    alignas(cmsghdr)std::array<ctl::wire_byte,CMSG_SPACE(64*sizeof(int))> control{};
    msghdr message{};message.msg_iov=&io;message.msg_iovlen=1;message.msg_control=control.data();message.msg_controllen=CMSG_SPACE(count*sizeof(int));
    auto* h=CMSG_FIRSTHDR(&message);h->cmsg_level=SOL_SOCKET;h->cmsg_type=SCM_RIGHTS;h->cmsg_len=CMSG_LEN(count*sizeof(int));
    for(unsigned i=0;i!=count;++i)std::memcpy(CMSG_DATA(h)+i*sizeof(int),&fd,sizeof(fd));
    CHECK(control_test::posix_abi::sendmsg_noexcept(socket,&message,MSG_NOSIGNAL)==ssize_t(ctl::remaining_bytes(bytes)));control_test::posix_abi::close_noexcept(fd);
}
int main(int argc,char** argv)
{
    if(argc==3&&std::strcmp(argv[1],"--check-closed")==0)
    {
        std::string_view const text{argv[2]};int descriptor{};
        // [text.data(),text.data()+text.size()) native argv bytes;
        // ^^ the scanner may advance at most to this one-past endpoint.
        auto const* const end{text.data()+text.size()};
        auto const parsed{fast_io::parse_by_scan(text.data(),end,fast_io::mnp::dec_get<true,true>(descriptor))};
        CHECK(parsed.code==fast_io::parse_code::ok&&parsed.iter==end&&descriptor>=0);
        CHECK(control_test::posix_abi::fcntl_noexcept(descriptor,F_GETFD)==-1&&errno==EBADF);return 0;
    }
    CHECK(control_test::posix_abi::prctl_noexcept(PR_SET_CHILD_SUBREAPER,1ul,0ul,0ul,0ul)==0);
    auto count=descriptors();ctl::error failure{};
    auto off=ctl::linux_launch_channel::create({},failure);CHECK(!off&&failure==ctl::error::disabled&&descriptors()==count);
    auto bad=config();bad.mode=ctl::compile_mode::lazy;
    off=ctl::linux_launch_channel::create(bad,failure);CHECK(!off&&failure==ctl::error::unsupported_mode&&descriptors()==count);
    {
        auto channel=ctl::linux_launch_channel::create(config(),failure);CHECK(channel&&failure==ctl::error::none);
        CHECK(channel->select_receiver({})==ctl::error::same_process);
    }
    {
        auto channel=ctl::linux_launch_channel::create(config(),failure);CHECK(channel);
        CHECK(channel->select_launcher()==ctl::error::none);auto pid=control_test::posix_abi::fork_noexcept();CHECK(pid>=0);
        if(pid==0)
        {
            char fd[32];fast_io::basic_obuffer_view<char> output{fd,fd+sizeof(fd)};
            // [32 writable bytes] accommodates one signed int and its NUL.
            // ^^ print advances the native output cursor within this array.
            fast_io::io::print(output,channel->native_handle(),'\0');
            control_test::posix_abi::execl_noexcept("/proc/self/exe","control-transport","--check-closed",fd,static_cast<char const*>(nullptr));
            control_test::posix_abi::_exit_noexcept(2);
        }
        child_ok(pid);
    }
    enum scenario{normal,fragment,replay,truncated,oversized,rights,rights_truncated,wrong_pid,own_process,empty_permit,concatenated};
    for(auto mode:{normal,fragment,replay,truncated,oversized,rights,rights_truncated,wrong_pid,own_process,empty_permit,concatenated})
    {
        auto channel=ctl::linux_launch_channel::create(config(),failure);CHECK(channel&&failure==ctl::error::none);
        int pipefd[2];CHECK(control_test::posix_abi::pipe2_noexcept(pipefd,O_CLOEXEC)==0);
        auto vm=control_test::posix_abi::fork_noexcept();CHECK(vm>=0);
        if(vm==0)
        {
            control_test::posix_abi::close_noexcept(pipefd[1]);int inherited_sender=-1;
            if(mode==own_process){CHECK(control_test::posix_abi::read_noexcept(pipefd[0],&inherited_sender,sizeof(inherited_sender))==sizeof(inherited_sender));inherited_sender=control_test::posix_abi::dup_noexcept(inherited_sender);CHECK(inherited_sender>=0);}
            control_test::posix_abi::close_noexcept(pipefd[0]);
            ctl::launch_authority authority(channel->receiver_config());CHECK(authority.status()==ctl::error::none);
            CHECK(channel->select_receiver(mode==empty_permit?ctl::launch_permit{}:authority.issue_permit())==ctl::error::none);
            CHECK((control_test::posix_abi::fcntl_noexcept(channel->native_handle(),F_GETFD)&FD_CLOEXEC)!=0);
            CHECK((control_test::posix_abi::fcntl_noexcept(channel->native_handle(),F_GETFL)&O_NONBLOCK)!=0);
            if(mode==own_process)
            {auto data=frame();CHECK(control_test::posix_abi::send_noexcept(inherited_sender,data.data(),data.size(),MSG_NOSIGNAL)==ssize_t(data.size()));control_test::posix_abi::close_noexcept(inherited_sender);}
            auto before=descriptors();auto r=receive(*channel);
            if(mode==normal||mode==fragment||mode==replay)
            {
                if(mode==fragment){CHECK(r.status==ctl::error::none&&!r.request);r=receive(*channel);}
                CHECK(r.status==ctl::error::none&&r.request);
                CHECK(channel->complete(*r.request,ctl::host_completion::inspected)==ctl::error::none);
                if(mode==replay){r=receive(*channel);CHECK(r.status==ctl::error::replay&&!r.request);}
            }
            else if(mode==truncated)
            {CHECK(r.status==ctl::error::none&&!r.request);r=receive(*channel);CHECK(r.status==ctl::error::truncated&&!r.request);}
            else
            {
                auto expected=mode==oversized?ctl::error::oversized:(mode==rights||mode==rights_truncated)?ctl::error::unexpected_ancillary:
                    mode==wrong_pid?ctl::error::wrong_peer:mode==own_process?ctl::error::same_process:
                    mode==empty_permit?ctl::error::unauthorized:ctl::error::malformed;
                CHECK(r.status==expected&&!r.request);
                if(mode==rights||mode==rights_truncated)CHECK(descriptors()==before-2); // Receiver + pidfd closed, no received FD leaked.
            }
            channel.reset();control_test::posix_abi::_exit_noexcept(0);
        }
        control_test::posix_abi::close_noexcept(pipefd[0]);CHECK(channel->select_launcher()==ctl::error::none);
        CHECK((control_test::posix_abi::fcntl_noexcept(channel->native_handle(),F_GETFD)&FD_CLOEXEC)!=0);
        auto data=frame();
        if(mode==own_process){auto fd=channel->native_handle();CHECK(control_test::posix_abi::write_noexcept(pipefd[1],&fd,sizeof(fd))==sizeof(fd));}
        else if(mode==rights||mode==rights_truncated)send_rights(channel->native_handle(),input(data),mode==rights?1:64);
        else if(mode==wrong_pid)
        {
            auto attacker=control_test::posix_abi::fork_noexcept();CHECK(attacker>=0);
            if(attacker==0)
            {
                // Same UID and a genuinely inherited FD do not authenticate.
                // The high-level sender refuses this fork, and the raw syscall
                // still reaches the receiver with the attacker's kernel PID.
                CHECK(channel->send_packet(input(data))==ctl::error::wrong_peer);
                iovec io{data.data(),data.size()};alignas(cmsghdr)std::array<ctl::wire_byte,CMSG_SPACE(sizeof(ucred))> control{};
                msghdr msg{};msg.msg_iov=&io;msg.msg_iovlen=1;msg.msg_control=control.data();msg.msg_controllen=control.size();
                auto* h=CMSG_FIRSTHDR(&msg);h->cmsg_level=SOL_SOCKET;h->cmsg_type=SCM_CREDENTIALS;h->cmsg_len=CMSG_LEN(sizeof(ucred));
                ucred forged{control_test::posix_abi::getppid_noexcept(),control_test::posix_abi::getuid_noexcept(),control_test::posix_abi::getgid_noexcept()};std::memcpy(CMSG_DATA(h),&forged,sizeof(forged));
                CHECK(control_test::posix_abi::sendmsg_noexcept(channel->native_handle(),&msg,MSG_NOSIGNAL)==-1&&errno==EPERM);
                CHECK(control_test::posix_abi::send_noexcept(channel->native_handle(),data.data(),data.size(),MSG_NOSIGNAL)==ssize_t(data.size()));control_test::posix_abi::_exit_noexcept(0);
            }
            child_ok(attacker);
        }
        else if(mode==oversized)
        {data.resize(ctl::max_frame_bytes+1);CHECK(control_test::posix_abi::send_noexcept(channel->native_handle(),data.data(),data.size(),MSG_NOSIGNAL)==ssize_t(data.size()));}
        else if(mode==concatenated)
        {auto second=frame(2);data.insert(data.end(),second.begin(),second.end());CHECK(channel->send_packet(input(data))==ctl::error::none);}
        else if(mode==fragment||mode==truncated)
        {
            CHECK(channel->send_packet(input(prefix(data,7)))==ctl::error::none);
            if(mode==fragment)CHECK(channel->send_packet(input(suffix(data,7)))==ctl::error::none);
            else channel->close();
        }
        else
        {CHECK(channel->send_packet(input(data))==ctl::error::none);if(mode==replay)CHECK(channel->send_packet(input(data))==ctl::error::none);}
        control_test::posix_abi::close_noexcept(pipefd[1]);child_ok(vm);
    }
    // A real creator death invalidates even a queued valid packet. The observer
    // is a subreaper only to reap the deliberately orphaned test VM afterwards.
    int death_pipe[2];CHECK(control_test::posix_abi::pipe2_noexcept(death_pipe,O_CLOEXEC)==0);
    auto launcher=control_test::posix_abi::fork_noexcept();CHECK(launcher>=0);
    if(launcher==0)
    {
        control_test::posix_abi::close_noexcept(death_pipe[0]);auto channel=ctl::linux_launch_channel::create(config(),failure);CHECK(channel);
        auto vm=control_test::posix_abi::fork_noexcept();CHECK(vm>=0);
        if(vm==0)
        {
            ctl::launch_authority authority(channel->receiver_config());CHECK(channel->select_receiver(authority.issue_permit())==ctl::error::none);
            while(control_test::posix_abi::getppid_noexcept()!=1&&control_test::posix_abi::getppid_noexcept()==pid_t(channel->receiver_config().launcher_process)){control_test::posix_abi::poll_noexcept(nullptr,0,1);}
            auto r=receive(*channel);CHECK(r.status==ctl::error::launcher_exited&&!r.request);
            char ok='Y';CHECK(control_test::posix_abi::write_noexcept(death_pipe[1],&ok,1)==1);channel.reset();control_test::posix_abi::_exit_noexcept(0);
        }
        CHECK(channel->select_launcher()==ctl::error::none);CHECK(channel->send_packet(input(frame()))==ctl::error::none);control_test::posix_abi::_exit_noexcept(0);
    }
    control_test::posix_abi::close_noexcept(death_pipe[1]);char ok{};CHECK(control_test::posix_abi::read_noexcept(death_pipe[0],&ok,1)==1&&ok=='Y');control_test::posix_abi::close_noexcept(death_pipe[0]);
    child_ok(launcher);int status{};CHECK(control_test::posix_abi::wait_noexcept(&status)>0&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    CHECK(descriptors()==count);
    fast_io::io::println("PASS Linux launch channel: 11 packet scenarios + creator death + actual exec closure; ",checks," parent checks, child checks also enforced");
}

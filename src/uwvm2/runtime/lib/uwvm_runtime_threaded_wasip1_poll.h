// Real bounded poll + owned timerfds + actual Group cancellation fd. No sleep
// loop, fake-ready event, borrowed-global O_NONBLOCK change or ordinary helper.
extern "C++"
{
    inline auto runtime_threaded_wasip1_adapter::poll_oneoff(context& call,::std::uint32_t input,
        ::std::uint32_t output,::std::uint32_t count,::std::uint32_t completed) noexcept -> error
    {
        if(count==0u)return error::einval;if(count>max_subscriptions)return error::e2big;
        if(!call.aligned(input,8u)||!call.aligned(output,8u)||!call.aligned(completed,4u))return error::efault;
        ::std::byte* unused{};
        if(!call.bounds(input,static_cast<::std::size_t>(count)*48uz,unused)||
           !call.bounds(output,static_cast<::std::size_t>(count)*32uz,unused)||!call.bounds(completed,4uz,unused))return error::efault;
        struct subscription
        {
            ::std::uint64_t userdata{},timeout{};::std::uint32_t id{},fd{};unsigned kind{};bool absolute{},immediate{},deferred{};
            error failure{error::esuccess};
        };
        ::std::array<subscription,max_subscriptions> subscriptions{};
        ::std::array<fd_pin,max_subscriptions> pins{};
        ::std::array<::pollfd,max_subscriptions+1uz> watched{};
        for(auto& descriptor:watched)descriptor.fd=-1;
        struct timers final
        {
            ::std::array<int,max_subscriptions> descriptors{};
            timers() noexcept{descriptors.fill(-1);}
            ~timers() noexcept{for(auto fd:descriptors)if(fd>=0)static_cast<void>(::close(fd));}
        } owned;
        // Snapshot every subscription before any output can overlap it. Each
        // byte is atomic; torn Guest writes cannot create an unchecked pointer.
        // Check all clock authority before native clocks/FD readiness are read.
        for(::std::uint32_t i{};i<count;++i)
        {
            ::std::array<::std::byte,48> raw{};
            if(!call.copy_in(static_cast<::std::uint64_t>(input)+i*48u,raw))return error::efault;
            auto& sub=subscriptions[i];sub.userdata=little(raw.data(),8);sub.kind=::std::to_integer<unsigned>(raw[8]);
            if(sub.kind==0u)
            {
                sub.id=static_cast<::std::uint32_t>(little(raw.data()+16,4));sub.timeout=little(raw.data()+24,8);
                auto const flags=little(raw.data()+40,2);if(flags&~1u)return error::einval;sub.absolute=(flags&1u)!=0u;
                if(sub.id>3u)return error::einval;::clockid_t native{};
                if(!clock_native(call,sub.id,native))return error::enotcapable;
                if(sub.id>1u)return error::enotsup; // timerfd cannot implement CPU-clock deadlines.
            }
            else if(sub.kind==1u||sub.kind==2u)sub.fd=static_cast<::std::uint32_t>(little(raw.data()+16,4));
            else return error::einval;
        }
        bool immediate{};
        for(::std::uint32_t i{};i<count;++i)
        {
            if(call.stopped())return error::ecanceled;
            auto& sub=subscriptions[i];auto& watch=watched[i+1u];
            if(sub.kind==0u)
            {
                ::clockid_t native{};if(!clock_native(call,sub.id,native))return error::enotcapable;
                ::timespec now{};if(::clock_gettime(native,&now)!=0)return native_error(errno);
                if(now.tv_sec<0||now.tv_nsec<0||now.tv_nsec>=1000000000l||
                   static_cast<::std::uint64_t>(now.tv_sec)>(UINT64_MAX-static_cast<::std::uint64_t>(now.tv_nsec))/1000000000u)return error::eoverflow;
                auto const current=static_cast<::std::uint64_t>(now.tv_sec)*1000000000u+static_cast<::std::uint64_t>(now.tv_nsec);
                if(sub.timeout==0u||(sub.absolute&&sub.timeout<=current))
                {sub.immediate=true;immediate=true;continue;}
                auto const seconds=sub.timeout/1000000000u;
                if(seconds>static_cast<::std::uint64_t>((::std::numeric_limits<::time_t>::max)()))return error::eoverflow;
                int fd=::timerfd_create(native,TFD_NONBLOCK|TFD_CLOEXEC);
                if(fd<0){sub.failure=native_error(errno);immediate=true;continue;}
                owned.descriptors[i]=fd;
                ::itimerspec timer{};timer.it_value.tv_sec=static_cast<::time_t>(seconds);timer.it_value.tv_nsec=static_cast<long>(sub.timeout%1000000000u);
                if(::timerfd_settime(fd,sub.absolute?TFD_TIMER_ABSTIME:0,&timer,nullptr)!=0)
                {sub.failure=native_error(errno);immediate=true;continue;}
                watch={fd,POLLIN,0};
            }
            else sub.deferred=true;
            // Establish ALL timers before attempting any resource pin. A
            // reader owning an FD mutex cannot hide an unrelated deadline.

        }
        auto const stop=call.actual.cancellation_fd();if(stop<0){static_cast<void>(call.native_failure());return error::eio;}
        watched[0]={stop,POLLIN,0};
        for(;;)
        {
            if(call.stopped())return error::ecanceled;
            bool pending{};
            for(::std::uint32_t i{};i<count;++i)
            {
                auto& sub=subscriptions[i];if(!sub.deferred)continue;
                auto const required=right::right_poll_fd_readwrite|(sub.kind==1u?right::right_fd_read:right::right_fd_write);
                auto const acquired=pins[i].acquire(call,sub.fd,required,false,true);
                // Even a failed attempt may own the actual descriptor mutex;
                // release it before any duplicate/cross subscription or wait.
                pins[i].unlock();
                if(acquired.deferred){pending=true;continue;}
                sub.deferred=false;sub.failure=acquired.failure;
                if(sub.failure!=error::esuccess){immediate=true;continue;}
                short interest=static_cast<short>(sub.kind==1u?POLLIN:POLLOUT);
                // Linux UAPI POLLRDHUP: a TCP peer FIN can be read-hung-up
                // without POLLHUP while this endpoint remains writable.
                constexpr short linux_pollrdhup{0x2000};
                if(pins[i].type==fd_pin::kind::stream_socket)interest|=linux_pollrdhup;
                watched[i+1u]={pins[i].descriptor,interest,0};
            }
            for(auto& descriptor:watched)descriptor.revents=0;
            // Retry contended native locks through the SAME real timer/stop
            // poll. 1ms bounds lock-retry latency; it creates no Guest event,
            // clock grant, errno or fictitious FD readiness.
            auto result=::poll(watched.data(),static_cast<::nfds_t>(count)+1u,immediate?0:(pending?1:-1));
            if(result<0){if(errno==EINTR)continue;return native_error(errno);}
            if(call.stopped())return error::ecanceled;
            if(watched[0].revents!=0){static_cast<void>(call.native_failure());return error::eio;}
            ::std::uint32_t written{};
            for(::std::uint32_t i{};i<count;++i)
            {
                auto const& sub=subscriptions[i];auto const revents=watched[i+1u].revents;
                if(!sub.immediate&&sub.failure==error::esuccess&&revents==0)continue;
                auto failure=sub.failure;
                if(revents&POLLNVAL)failure=error::ebadf;
                else if(revents&POLLERR)failure=error::eio;
                ::std::array<::std::byte,32> event{};
                little_put(event.data(),sub.userdata,8);little_put(event.data()+8,static_cast<::std::uint16_t>(failure),2);
                event[10]=static_cast<::std::byte>(sub.kind);
                if(sub.kind!=0u)
                {
                    // Snapshot availability for these authenticated kernel
                    // pipe/socket classes. It is advisory: another reader may
                    // win between poll/FIONREAD/the later actual I/O call.
                    ::std::uint64_t available{};
                    if(failure==error::esuccess&&sub.kind==1u)
                    {
                        int bytes{};
                        if(::ioctl(pins[i].descriptor,FIONREAD,&bytes)!=0)
                        {failure=native_error(errno);little_put(event.data()+8,static_cast<::std::uint16_t>(failure),2);}
                        else if(bytes>0)available=static_cast<unsigned>(bytes);
                    }
                    else if(failure==error::esuccess&&(revents&POLLOUT))available=1u;
                    // Write readiness guarantees progress at its observation,
                    // not an exact portable socket send-buffer byte count.
                    little_put(event.data()+16,available,8);
                    constexpr short linux_pollrdhup{0x2000};
                    little_put(event.data()+24,(revents&(POLLHUP|linux_pollrdhup))?1u:0u,2);
                }
                if(!call.copy_out(static_cast<::std::uint64_t>(output)+written*32u,event))return error::efault;++written;
            }
            if(written)return call.store<4>(completed,written)?error::esuccess:error::efault;
            // Native spurious wakeup: retry only through the actual stop fd.
            immediate=false;
        }
    }
}

// Included inside Runtime lib; only this selected cold adapter touches these paths.
extern "C++"
{
    struct runtime_threaded_wasip1_adapter::fd_pin final
    {
        enum class kind {unsupported,pipe,stream_socket};
        context* call{};native_fd* locked{};
        fd_ref retained{::uwvm2::imported::wasi::wasip1::fd_manager::wasi_no_construct};
        int descriptor{-1},flags{};kind type{kind::unsupported};
        fd_pin() noexcept=default;fd_pin(fd_pin const&)=delete;
        ~fd_pin() noexcept {if(descriptor>=0)static_cast<void>(::close(descriptor));unlock();}
        void unlock() noexcept
        {
            if(locked!=nullptr)
            {auto const result=::pthread_mutex_unlock(::std::addressof(locked->fd_mutex.mutex));locked=nullptr;if(result!=0&&call)static_cast<void>(call->native_failure());}
        }
        struct acquisition {error failure{error::esuccess};bool deferred{};};
        [[nodiscard]] acquisition acquire(context& actual,::std::uint32_t guest,right required,bool keep_lock,bool try_only=false) noexcept
        {
            call=&actual;if(guest>INT32_MAX)return {error::ebadf};
            auto& map=actual.env.fd_storage;
            // Same actual fair RW lock protocol; no blocking guard constructor
            // can hide cancellation. PENDING/WRITER block new readers.
            auto& bits=map.fds_rwlock.bits;
            constexpr auto increment=::uwvm2::utils::mutex::rwlock_reader_mask();
            constexpr auto excluded=::uwvm2::utils::mutex::rwlock_writer_mask()|::uwvm2::utils::mutex::rwlock_pending_mask();
            for(;;)
            {
                if(actual.stopped())return {error::ecanceled};
                auto old=bits.load(::std::memory_order_relaxed);
                if((old&excluded)==0u)
                {
                    if(old>UINT_MAX-increment)return {error::eoverflow};
                    if(bits.compare_exchange_weak(old,old+increment,::std::memory_order_acquire,::std::memory_order_relaxed))break;
                }
                if(try_only)return {error::esuccess,true};
                ::sched_yield();
            }
            struct release_map final {decltype(bits)& value;~release_map() noexcept{value.fetch_sub(::uwvm2::utils::mutex::rwlock_reader_mask(),::std::memory_order_release);}} read{bits};
            native_fd* selected{};
            if(guest<map.opens.size())selected=map.opens.index_unchecked(guest).fd_p;
            else
            {auto found=map.renumber_map.find(static_cast<::std::int32_t>(guest));if(found==map.renumber_map.end())return {error::ebadf};selected=found->second.fd_p;}
            if(selected==nullptr){static_cast<void>(actual.native_failure());return {error::eio};}
            // Manager pin -> actual descriptor mutex is the existing lifetime
            // order. Do not release the manager before fd mutex is owned.
            for(;;)
            {
                if(actual.stopped())return {error::ecanceled};
                auto const result=::pthread_mutex_trylock(::std::addressof(selected->fd_mutex.mutex));
                if(result==0){locked=selected;break;}
                if(result!=EBUSY){static_cast<void>(actual.native_failure());return {error::eio};}
                // Poll never waits for a descriptor held by a blocking read.
                // The map read RAII above releases before the caller waits on
                // its already-established real timers + Group stop fd.
                if(try_only)return {error::esuccess,true};
                ::sched_yield();
            }
            if(selected->close_pos!=SIZE_MAX)return {error::ebadf};
            if((selected->rights_base&required)!=required)return {error::enotcapable};
            if(selected->wasi_fd.ptr==nullptr){static_cast<void>(actual.native_failure());return {error::eio};}
            // The actual map reader and fd mutex keep this borrowed resource
            // alive while classifying it. Do NOT retain/duplicate an unknown
            // regular/FUSE file: even a non-last close can invoke f_op->flush.
            auto const& storage=selected->wasi_fd.ptr->wasi_fd_storage;
            using t=::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e;
            int borrowed{-1};
            if(storage.type==t::file)borrowed=storage.storage.file_fd.native_handle();
            else if(storage.type==t::file_observer)borrowed=storage.storage.file_observer.native_handle();
            else if(storage.type==t::dir)return {error::eisdir};
            else return {error::enotsup};
            // These class probes dispatch only through the kernel socket/pipe
            // identity checks, not an unknown filesystem/device operation.
            flags=::fcntl(borrowed,F_GETFL);if(flags<0)return {native_error(errno)};
            int socket_type{};::socklen_t extent{sizeof(socket_type)};
            if(::getsockopt(borrowed,SOL_SOCKET,SO_TYPE,&socket_type,&extent)==0)
            {
                // Datagram/seqpacket needs a separately reviewed full-message
                // bounce policy; truncating at our finite 4KiB cap would lose a
                // message that could have fit the Guest's larger vector.
                if(extent!=sizeof(socket_type)||socket_type!=SOCK_STREAM)return {error::enotsup};
                type=kind::stream_socket;
            }
            else
            {
                if(errno!=ENOTSOCK)return {native_error(errno)};
                // Linux F_GETPIPE_SZ identifies genuine pipe/FIFO operations
                // without calling unknown filesystem fstat/getattr/device IO.
                constexpr int linux_getpipe_size{1032};
                if(::fcntl(borrowed,linux_getpipe_size)<=0)return {error::enotsup};
                type=kind::pipe;
            }
            // Only authenticated kernel pipe/stream-socket classes reach RC
            // retention or duplication. SAME OFD; no flag changes or path open.
            retained=selected->wasi_fd;
            constexpr int linux_dupfd_cloexec{1030};
            descriptor=::fcntl(borrowed,linux_dupfd_cloexec,0);if(descriptor<0)return {native_error(errno)};
            auto const pinned_flags=::fcntl(descriptor,F_GETFL);
            if(pinned_flags<0)return {native_error(errno)};
            if(pinned_flags!=flags){static_cast<void>(actual.native_failure());return {error::eio};}
            if(!keep_lock)unlock();return {error::esuccess};
        }
    };

    inline auto runtime_threaded_wasip1_adapter::wait_ready(context& call,int descriptor,short events,short& returned) noexcept -> error
    {
        for(;;)
        {
            if(call.stopped())return error::ecanceled;
            auto const stop=call.actual.cancellation_fd();if(stop<0){static_cast<void>(call.native_failure());return error::eio;}
            ::pollfd watched[2]{{stop,POLLIN,0},{descriptor,events,0}};
            auto const result=::poll(watched,2,-1);
            if(result<0){if(errno==EINTR)continue;return native_error(errno);}
            if(call.stopped())return error::ecanceled;
            if(watched[0].revents!=0){static_cast<void>(call.native_failure());return error::eio;}
            if(watched[1].revents){returned=watched[1].revents;return error::esuccess;}
        }
    }

    inline auto runtime_threaded_wasip1_adapter::fd_io(context& call,bool writing,::std::uint32_t guest,
        ::std::uint32_t iovs,::std::uint32_t count,::std::uint32_t completed) noexcept -> error
    {
        fd_pin pinned{};auto acquired=pinned.acquire(call,guest,writing?right::right_fd_write:right::right_fd_read,true);
        if(acquired.failure!=error::esuccess)return acquired.failure;
        if(acquired.deferred){static_cast<void>(call.native_failure());return error::eio;}
        if(count>max_vectors)return error::e2big;
        if(!call.aligned(iovs,4u)||!call.aligned(completed,4u))return error::efault;
        ::std::byte* unused{};
        if(!call.bounds(iovs,static_cast<::std::size_t>(count)*8uz,unused)||!call.bounds(completed,4uz,unused))return error::efault;
        struct vector {::std::uint32_t base{},size{};};::std::array<vector,max_vectors> vectors{};
        ::std::uint64_t total{};
        for(::std::uint32_t i{};i<count;++i)
        {
            ::std::array<::std::byte,8> raw{};if(!call.copy_in(static_cast<::std::uint64_t>(iovs)+i*8u,raw))return error::efault;
            auto& v=vectors[i];v.base=static_cast<::std::uint32_t>(little(raw.data(),4));v.size=static_cast<::std::uint32_t>(little(raw.data()+4,4));
            if(!call.bounds(v.base,v.size,unused))return error::efault;
            if(v.size>UINT32_MAX-total)return error::einval;total+=v.size;
        }
        if(total==0u)return call.store<4>(completed,0u)?error::esuccess:error::efault;
        if(pinned.type==fd_pin::kind::pipe)
        {
            if(!writing)
            {
                // A pipe's READ-end flags cannot prove whether another writer
                // emitted packet-mode buffers. Only a REAL already-nonblocking
                // read preserves those native packet/drop semantics safely.
                if((pinned.flags&O_NONBLOCK)==0)return error::enotsup;
                // Never shorten a Guest capacity that could fit a larger
                // packet: reject before I/O instead of discarding its tail.
                if(total>transfer_bytes)return error::e2big;
            }
#ifdef O_DIRECT
            else if(pinned.flags&O_DIRECT)return error::enotsup;
#endif
        }
        auto const wanted=static_cast<::std::size_t>((::std::min)(total,static_cast<::std::uint64_t>(transfer_bytes)));
        ::std::array<::std::byte,transfer_bytes> bounce{};
        if(writing)
        {
            ::std::size_t at{};
            for(::std::uint32_t i{};i<count&&at<wanted;++i)
            {auto n=(::std::min)(static_cast<::std::size_t>(vectors[i].size),wanted-at);if(!call.copy_in(vectors[i].base,{bounce.data()+at,n}))return error::efault;at+=n;}
        }
        struct staging final
        {
            int ends[2]{-1,-1};
            ~staging() noexcept{for(auto fd:ends)if(fd>=0)static_cast<void>(::close(fd));}
        } pipe;
        if(pinned.type==fd_pin::kind::pipe&&writing)
        {
            if(::syscall(SYS_pipe2,pipe.ends,O_NONBLOCK|O_CLOEXEC)!=0)return native_error(errno);
            for(auto fd:pipe.ends){auto f=::fcntl(fd,F_GETFL);if(f<0||(f&O_NONBLOCK)==0){static_cast<void>(call.native_failure());return error::eio;}}
            if(writing)
            {
                // Only our NEW OFDs are nonblocking. The actual inherited / 
                // redirected descriptor and every user alias are unmodified.
                auto const n=::write(pipe.ends[1],bounce.data(),wanted);
                if(n<0)return native_error(errno);if(static_cast<::std::size_t>(n)!=wanted){static_cast<void>(call.native_failure());return error::eio;}
            }
        }
        struct sigpipe_guard final
        {
            context& call;::sigset_t set{},old{};bool active{},preexisting{};
            explicit sigpipe_guard(context& c) noexcept:call{c}
            {
                ::sigemptyset(&set);::sigaddset(&set,SIGPIPE);
                auto result=::pthread_sigmask(SIG_BLOCK,&set,&old);if(result!=0){static_cast<void>(call.native_failure());return;}
                active=true;::sigset_t pending{};if(::sigpending(&pending)!=0){static_cast<void>(call.native_failure());return;}
                preexisting=::sigismember(&pending,SIGPIPE)==1;
            }
            void consume_new_epipe() noexcept
            {
                if(preexisting||!active)return;
                ::timespec now{};int result{};do{result=::sigtimedwait(&set,nullptr,&now);}while(result<0&&errno==EINTR);
                if(result!=SIGPIPE&&!(result<0&&errno==EAGAIN))static_cast<void>(call.native_failure());
            }
            ~sigpipe_guard() noexcept{if(active&&::pthread_sigmask(SIG_SETMASK,&old,nullptr)!=0)static_cast<void>(call.native_failure());}
        } pipe_signals{call};
        if(call.stopped())return error::ecanceled;
        ::ssize_t transferred{};
        for(;;)
        {
            if(call.stopped())return error::ecanceled;
            auto const current=::fcntl(pinned.descriptor,F_GETFL);if(current<0)return native_error(errno);
            // Root excludes native policy mutation during the group lifetime.
            // Still fail on an observed flag drift rather than silently adapt.
            if(current!=pinned.flags){static_cast<void>(call.native_failure());return error::eio;}
            if(pinned.type==fd_pin::kind::stream_socket)
            {transferred=writing ? ::send(pinned.descriptor,bounce.data(),wanted,MSG_DONTWAIT|MSG_NOSIGNAL) : ::recv(pinned.descriptor,bounce.data(),wanted,MSG_DONTWAIT);}
            else if(!writing)
            {
                // Actual inherited O_NONBLOCK was checked, and its OFD flags
                // were just rechecked unchanged. No poll-then-blocking read,
                // dup-F_SETFL, staging splice or packet-class assumption.
                transferred=::read(pinned.descriptor,bounce.data(),wanted);
            }
            else
            {
                auto const owned_flags=::fcntl(pipe.ends[0],F_GETFL);
                if(owned_flags<0||(owned_flags&O_NONBLOCK)==0){static_cast<void>(call.native_failure());return error::eio;}
                constexpr unsigned splice_nonblock{2u}; // Linux UAPI SPLICE_F_NONBLOCK.
                transferred=static_cast<::ssize_t>(::syscall(SYS_splice,pipe.ends[0],nullptr,pinned.descriptor,nullptr,wanted,splice_nonblock));
            }
            if(transferred>=0)break;
            auto const failure=errno;if(failure==EPIPE)pipe_signals.consume_new_epipe();
            if(failure==EINTR)continue;
            if(failure!=EAGAIN&&failure!=EWOULDBLOCK)return native_error(failure);
            if(pinned.flags&O_NONBLOCK)return error::eagain; // preserve Guest's nonblocking policy.
            short ready{};auto waited=wait_ready(call,pinned.descriptor,writing?POLLOUT:POLLIN,ready);if(waited!=error::esuccess)return waited;
            if(ready&POLLNVAL)return error::ebadf;
        }
        if(call.stopped())return error::ecanceled; // actual external effect is not rolled back/replayed.
        auto const amount=static_cast<::std::size_t>(transferred);
        if(amount>wanted){static_cast<void>(call.native_failure());return error::eio;}
        if(!writing)
        {
            ::std::size_t at{};
            for(::std::uint32_t i{};i<count&&at<amount;++i)
            {auto n=(::std::min)(static_cast<::std::size_t>(vectors[i].size),amount-at);if(!call.copy_out(vectors[i].base,{bounce.data()+at,n}))return error::efault;at+=n;}
        }
        return call.store<4>(completed,amount)?error::esuccess:error::efault;
    }
}

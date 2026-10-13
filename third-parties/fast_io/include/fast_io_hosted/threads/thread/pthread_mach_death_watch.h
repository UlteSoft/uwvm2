#pragma once
#include <mach/mach.h>
#include <mach/notify.h>
#include <cstring>

namespace fast_io::posix::details
{
// Private HOST lifetime observation only. No port or thread-control operation
// is exposed to a guest or to the Wasm-context ASM debugger.
class pthread_mach_death_watch
{
    ::mach_port_t thread_{}, receiver_{};
    bool armed_{}, dead_{};
    ::std::uint_least32_t error_{};
public:
    constexpr pthread_mach_death_watch() noexcept = default;
    pthread_mach_death_watch(pthread_mach_death_watch const&) = delete;
    pthread_mach_death_watch& operator=(pthread_mach_death_watch const&) = delete;
    constexpr pthread_mach_death_watch(pthread_mach_death_watch&& other) noexcept
    { swap(other); }
    constexpr ~pthread_mach_death_watch() noexcept { release(); }
    constexpr void swap(pthread_mach_death_watch& other) noexcept
    {
        ::std::ranges::swap(thread_,other.thread_);
        ::std::ranges::swap(receiver_,other.receiver_);
        ::std::ranges::swap(armed_,other.armed_);
        ::std::ranges::swap(dead_,other.dead_);
        ::std::ranges::swap(error_,other.error_);
    }
    inline void arm(::pthread_t owner) noexcept
    {
        // The original pthread is still suspended. Retaining its actual SEND
        // right now prevents port-name reuse before any user body can finish.
        auto const task{mach_task_self()};
        auto const port{::fast_io::noexcept_call(::pthread_mach_thread_np,owner)};
        auto fail{[&](::kern_return_t value) noexcept { error_=static_cast<::std::uint_least32_t>(value); }};
        if(port==MACH_PORT_NULL || port==MACH_PORT_DEAD) { fail(KERN_INVALID_NAME);return; }
        auto result{::fast_io::noexcept_call(::mach_port_mod_refs,task,port,MACH_PORT_RIGHT_SEND,1)};
        if(result!=KERN_SUCCESS) { fail(result);return; }
        thread_=port;
        result=::fast_io::noexcept_call(::mach_port_allocate,task,MACH_PORT_RIGHT_RECEIVE,::std::addressof(receiver_));
        if(result!=KERN_SUCCESS) { fail(result);release();return; }
        ::mach_port_t previous{};
        result=::fast_io::noexcept_call(::mach_port_request_notification,task,thread_,MACH_NOTIFY_DEAD_NAME,
            1u,receiver_,MACH_MSG_TYPE_MAKE_SEND_ONCE,::std::addressof(previous));
        if(result!=KERN_SUCCESS) { fail(result);release();return; }
        armed_=true;
        if(previous!=MACH_PORT_NULL)
        {
            // Do not replace another HOST observer's notification. Restore its
            // genuine send-once right and retire only our own replaced request.
            ::mach_port_t ours{};
            result=::fast_io::noexcept_call(::mach_port_request_notification,task,thread_,MACH_NOTIFY_DEAD_NAME,
                1u,previous,MACH_MSG_TYPE_MOVE_SEND_ONCE,::std::addressof(ours));
            if(result!=KERN_SUCCESS) { ::fast_io::fast_terminate(); }
            armed_=false;
            if(ours!=MACH_PORT_NULL &&
               ::fast_io::noexcept_call(::mach_port_deallocate,task,ours)!=KERN_SUCCESS)
            { ::fast_io::fast_terminate(); }
            fail(KERN_FAILURE);release();
        }
    }
    [[nodiscard]] inline ::fast_io::thread_join_result poll() noexcept
    {
        if(error_ || !armed_) { return {::fast_io::thread_join_status::failed,error_}; }
        if(dead_) { return {::fast_io::thread_join_status::joined,0u}; }
        struct packet
        {
            ::mach_dead_name_notification_t message{};
            unsigned char extended_trailer[sizeof(::mach_msg_audit_trailer_t)-sizeof(::mach_msg_format_0_trailer_t)]{};
        } received{};
        auto const result{::fast_io::noexcept_call(::mach_msg,::std::addressof(received.message.not_header),
            MACH_RCV_MSG|MACH_RCV_TIMEOUT|MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0)|
                MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT),
            0u,static_cast<::mach_msg_size_t>(sizeof(received)),receiver_,0u,MACH_PORT_NULL)};
        if(result==MACH_RCV_TIMED_OUT || result==MACH_RCV_INTERRUPTED)
        { return {::fast_io::thread_join_status::pending,0u}; }
        if(result!=MACH_MSG_SUCCESS)
        { error_=static_cast<::std::uint_least32_t>(result);return {::fast_io::thread_join_status::failed,error_}; }
        auto const& header{received.message.not_header};
        ::mach_msg_audit_trailer_t trailer{};
        ::std::memcpy(::std::addressof(trailer),::std::addressof(received.message.trailer),sizeof(trailer));
        constexpr ::security_token_t kernel_sender KERNEL_SECURITY_TOKEN_VALUE;
        constexpr ::audit_token_t kernel_audit KERNEL_AUDIT_TOKEN_VALUE;
        bool audit_matches{true};
        for(unsigned i{};i!=8u;++i) { audit_matches &= trailer.msgh_audit.val[i]==kernel_audit.val[i]; }
        if(header.msgh_id!=MACH_NOTIFY_DEAD_NAME || header.msgh_size!=__builtin_offsetof(::mach_dead_name_notification_t,trailer) ||
           header.msgh_local_port!=receiver_ || (header.msgh_bits&MACH_MSGH_BITS_COMPLEX)!=0 ||
           received.message.not_port!=thread_ || trailer.msgh_trailer_type!=MACH_MSG_TRAILER_FORMAT_0 ||
           trailer.msgh_trailer_size!=sizeof(trailer) ||
           trailer.msgh_sender.val[0]!=kernel_sender.val[0] || trailer.msgh_sender.val[1]!=kernel_sender.val[1] || !audit_matches)
        { error_=KERN_INVALID_ARGUMENT;return {::fast_io::thread_join_status::failed,error_}; }
        // This is a genuine kernel death message on our private receive right,
        // naming our retained, non-reusable pthread port. It follows all TLS
        // destructors and kernel termination, not merely a body-return flag.
        dead_=true;
        return {::fast_io::thread_join_status::joined,0u};
    }
    constexpr void release() noexcept
    {
        if(!thread_ && !receiver_) { return; }
        auto const task{mach_task_self()};
        bool extra{dead_};
        if(armed_ && !dead_)
        {
            ::mach_port_t previous{};
            auto const result{::fast_io::noexcept_call(::mach_port_request_notification,task,thread_,MACH_NOTIFY_DEAD_NAME,
                0u,MACH_PORT_NULL,MACH_MSG_TYPE_MOVE_SEND_ONCE,::std::addressof(previous))};
            // XNU rejects cancellation of an already dead name with
            // KERN_INVALID_ARGUMENT. Our retained pin prevents name reuse;
            // verify its actual DEAD_NAME right before releasing the delivered
            // notification's additional uref. This is resource retirement,
            // never a substitute for poll()'s authenticated death message.
            if(result!=KERN_SUCCESS && result!=KERN_INVALID_ARGUMENT) { ::fast_io::fast_terminate(); }
            if(result==KERN_SUCCESS && previous!=MACH_PORT_NULL)
            {
                if(::fast_io::noexcept_call(::mach_port_deallocate,task,previous)!=KERN_SUCCESS)
                { ::fast_io::fast_terminate(); }
            }
            else
            {
                ::mach_port_type_t type{};
                if(::fast_io::noexcept_call(::mach_port_type,task,thread_,::std::addressof(type))!=KERN_SUCCESS ||
                   (type&MACH_PORT_TYPE_DEAD_NAME)==0u) { ::fast_io::fast_terminate(); }
                extra=true; // Delivered dead-name notification adds one uref.
            }
        }
        if(receiver_ &&
           ::fast_io::noexcept_call(::mach_port_mod_refs,task,receiver_,MACH_PORT_RIGHT_RECEIVE,-1)!=KERN_SUCCESS)
        { ::fast_io::fast_terminate(); }
        if(thread_)
        {
            auto result{extra ?
                ::fast_io::noexcept_call(::mach_port_mod_refs,task,thread_,MACH_PORT_RIGHT_DEAD_NAME,-2) :
                ::fast_io::noexcept_call(::mach_port_mod_refs,task,thread_,MACH_PORT_RIGHT_SEND,-1)};
            if(!extra && result==KERN_INVALID_RIGHT)
            {
                // Cancellation/death can race. Our own pin still prevents name
                // reuse; the request was cancelled, so remove only our pin.
                result=::fast_io::noexcept_call(::mach_port_mod_refs,task,thread_,MACH_PORT_RIGHT_DEAD_NAME,-1);
            }
            if(result!=KERN_SUCCESS) { ::fast_io::fast_terminate(); }
        }
        thread_=receiver_=MACH_PORT_NULL;armed_=dead_=false;
    }
};
}


// Actual public -Rdbg process integration. The caller supplies the OS memory /
// cgroup containment and deadline; this helper never calls private host APIs.
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <chrono>
#include <exception>
#include <string_view>
#include <stdexcept>
#if defined(__APPLE__) || defined(__FreeBSD__)
# include <termios.h>
# include <cstring>
# include <poll.h>
# include <sys/wait.h>
# if defined(__FreeBSD__)
#  include <libutil.h>
# else
#  include <util.h>
# endif
#endif

static unsigned checks{};
static char const* io_phase{"startup"};
#if defined(_WIN32) && !defined(__CYGWIN__)
static void* prompt_process{}; // borrowed only while the policy's owned child is alive
#elif defined(__APPLE__) || defined(__FreeBSD__)
static ::pid_t prompt_process{}; // own live child; waitid below never reaps it
#endif
static std::string_view view(fast_io::string const& text) { return {text.data(),text.size()}; }
static bool contains(fast_io::string const& text, std::string_view part) { return view(text).find(part)!=std::string_view::npos; }
static void require(bool condition, char const* name)
{
    ++checks;
    fast_io::io::println("CLI_CHECK ",checks," ",fast_io::mnp::os_c_str(name)," ",fast_io::mnp::os_c_str(condition?"PASS":"FAIL"));
    if(!condition) { throw std::runtime_error{name}; }
}
static fast_io::string prompt(fast_io::native_io_observer output)
{
    fast_io::string text;
#if (defined(_WIN32) && !defined(__CYGWIN__)) || defined(__APPLE__) || defined(__FreeBSD__)
    auto const deadline{std::chrono::steady_clock::now()+std::chrono::seconds{20}};
#endif
    constexpr std::string_view marker{"(uwvm-debug) "};
    while(!(view(text).ends_with(marker) && (text.size()==marker.size() || text[text.size()-marker.size()-1]=='\n')))
    {
#if defined(__APPLE__) || defined(__FreeBSD__)
        // The parent retains the slave for terminal-restoration checks, so a
        // crashed child need not produce PTY EOF. Poll the FastIO handle and
        // observe only our child without reaping its process-owner handle.
        ::pollfd ready{output.native_handle(), POLLIN, 0};
        auto const result{fast_io::noexcept_call(::poll, &ready, 1u, 100)};
        if(result < 0) { if(errno == EINTR) { continue; } fast_io::throw_posix_error(); }
        if(result == 0)
        {
            ::siginfo_t observed{};
            if(fast_io::noexcept_call(::waitid, P_PID, static_cast<::id_t>(prompt_process), &observed,
                                     WEXITED | WNOHANG | WNOWAIT) != 0) { fast_io::throw_posix_error(); }
            if(observed.si_pid == prompt_process || std::chrono::steady_clock::now() >= deadline)
            {
                fast_io::io::println("CLI_CHILD_EXIT pid=", observed.si_pid, " code=", observed.si_code,
                    " status=", observed.si_status, " prompt-deadline-expired=", std::chrono::steady_clock::now() >= deadline);
                fast_io::io::println(text);throw std::runtime_error{"debugger closed or timed out before prompt"};
            }
            continue;
        }
#endif
        char c{};
        if(fast_io::operations::read_some(output,&c,&c+1)==&c)
        {
#if defined(_WIN32) && !defined(__CYGWIN__)
            // Windows protects console input by rejecting unrelated pipe sinks
            // too. Read an independent regular-file handle while the child
            // writes its own cursor; reaching today's EOF is not child EOF.
            std::uint_least32_t status{};
            if(fast_io::win32::GetExitCodeProcess(prompt_process,&status)==0) { fast_io::throw_win32_error(); }
            if(status==259u && std::chrono::steady_clock::now()<deadline)
            { fast_io::this_thread::sleep_for(std::chrono::milliseconds{5});continue; }
            fast_io::io::println("CLI_CHILD_EXIT code=",status," prompt-deadline-expired=",std::chrono::steady_clock::now()>=deadline);
#endif
            fast_io::io::println(text);throw std::runtime_error{"debugger closed or timed out before prompt"};
        }
        text.push_back(c);
        if(text.size()>1u<<20) { throw std::runtime_error{"bounded prompt exceeded"}; }
    }
    fast_io::io::print(text);
    return text;
}
static std::uint64_t affected(fast_io::string const& text)
{
    auto const at{view(text).find("affected=")};
    if(at==std::string_view::npos) { throw std::runtime_error{"missing FD result"}; }
    std::uint64_t fd{};
    auto const result{fast_io::parse_by_scan(text.data()+at+9,text.data()+text.size(),fast_io::mnp::dec_get(fd))};
    if(result.code!=fast_io::parse_code::ok) { throw std::runtime_error{"invalid FD result"}; }
    return fd;
}
#if defined(_WIN32) && !defined(__CYGWIN__)
static void inherited_file_round(char const* directory)
{
    if(directory==nullptr) { throw std::runtime_error{"Windows test requires its private output directory"}; }
    for(bool nothrow:{false,true})
    {
        for(bool inherit:{false,true})
        {
            auto const path{fast_io::concat_fast_io(fast_io::mnp::os_c_str(directory),"\\nt-open-",nothrow,"-",inherit,".bin")};
            auto const mode{fast_io::open_mode::in|fast_io::open_mode::out|fast_io::open_mode::creat|fast_io::open_mode::excl|
                fast_io::open_mode::shared_delete|(inherit?fast_io::open_mode::inherit:fast_io::open_mode::none)};
            fast_io::native_file file;
            if(nothrow)
            {
                auto const nt_mode{fast_io::win32::nt::details::calculate_nt_open_mode({mode,fast_io::perms::owner_all})};
                auto const result{fast_io::win32::nt::details::nt_call_invoke_without_directory_handle(path.data(),path.size(),
                    fast_io::win32::nt::details::nt_create_nothrow_callback<false>{nt_mode})};
                require(result.status==0,"nonthrowing NT create succeeded");
                file=fast_io::native_file{result.handle};
            }
            else { file=fast_io::native_file{path,mode};require(bool(file),"throwing NT create succeeded"); }
            std::uint_least32_t flags{};
            require(fast_io::win32::GetHandleInformation(file.native_handle(),&flags)!=0 && ((flags&1u)!=0)==inherit,
                "actual NT handle inheritance matches open mode");
            char const content[]{'A','\0','B'};
            fast_io::operations::write_all(file,content,content+3);
            fast_io::native_file reader{path,fast_io::open_mode::in|fast_io::open_mode::shared_delete};
            char bytes[3]{};fast_io::operations::read_all(reader,bytes,bytes+3);
            require(bytes[0]=='A' && bytes[1]=='\0' && bytes[2]=='B',"independent NT file reader preserves binary content");
            require(fast_io::win32::GetHandleInformation(reader.native_handle(),&flags)!=0 && (flags&1u)==0,
                "independent reader does not inherit writer flag");
        }
    }
}
#endif
static void policy_round(char const* binary,char const* wasm,char const* policy,char const* directory)
{
    io_phase="input pipe";
    fast_io::native_pipe input;
#if defined(_WIN32) && !defined(__CYGWIN__)
    if(directory==nullptr) { throw std::runtime_error{"Windows test requires its private output directory"}; }
    auto const path{fast_io::concat_fast_io(fast_io::mnp::os_c_str(directory),"\\cli-output-",fast_io::mnp::os_c_str(policy),".log")};
    io_phase="output writer";
    fast_io::native_file output{path,fast_io::open_mode::out|fast_io::open_mode::creat|fast_io::open_mode::excl|
        fast_io::open_mode::inherit|fast_io::open_mode::shared_delete};
    io_phase="output reader";
    fast_io::native_file output_reader{path,fast_io::open_mode::in|fast_io::open_mode::shared_delete};
    fast_io::native_io_observer command_input{input.out()},reply_output{output_reader};
    fast_io::native_process_io redirects{.in=input,.out=output,.err=output};
    using test_process=fast_io::win32_process;
    using test_process_args=fast_io::win32_process_args;
    using test_wait_status=fast_io::win32_wait_status;
#else
    (void)directory;
    fast_io::native_pipe output;
    fast_io::native_io_observer command_input{input.out()},reply_output{output.in()};
    fast_io::native_process_io redirects{.in=input,.out=output,.err=output};
    using test_process=fast_io::native_process;
    using test_process_args=fast_io::native_process_args;
    using test_wait_status=fast_io::native_wait_status;
#endif
#if defined(__APPLE__) || defined(__FreeBSD__)
    int master{},slave{};
    if(fast_io::noexcept_call(::openpty,&master,&slave,nullptr,nullptr,nullptr)!=0) { fast_io::throw_posix_error(); }
    fast_io::native_file terminal_master{master},terminal_slave{slave};
    ::termios original{};
    if(fast_io::noexcept_call(::tcgetattr,slave,&original)!=0) { fast_io::throw_posix_error(); }
    redirects={.in=terminal_slave,.out=terminal_slave,.err=terminal_slave};
    command_input=reply_output=terminal_master;
#endif
    io_phase="process launch";
    test_process child{fast_io::containers::basic_cstring_view<char>{fast_io::mnp::os_c_str(binary)},test_process_args{
        "-Rdbg","-Rct","0","-Rllvm-call-stack",fast_io::mnp::os_c_str(policy),"-Rllvm-cache-path","disable",
        "-WFE-gc","-WFE-function-references","--wasip1-global-noinherit-system-environment",
        "--wasip1-global-force-args","2","argv0","before",
        "--wasip1-global-add-or-replace-environment","UWVM_DEBUG_KEY","old","--run",fast_io::mnp::os_c_str(wasm)},
        {},redirects};
    input.in().close();
#if defined(_WIN32) && !defined(__CYGWIN__)
    prompt_process=child.native_handle().hprocess;
#elif defined(__APPLE__) || defined(__FreeBSD__)
    prompt_process=child.native_handle();
    output.out().close();
#else
    output.out().close();
#endif
    try
    {
        auto send{[&](auto const& command)
        {
            fast_io::io::println("CLI_SEND ",command);
            fast_io::io::println(command_input,command);
            return prompt(reply_output);
        }};
        auto ok{[](fast_io::string const& text) { return contains(text,"status=ok "); }};
        io_phase="initial prompt";
        prompt(reply_output);
        require(contains(send("help"),"info wasip1 args|env|fds|preopens"),"public help");
        require(!ok(send("info wasip1 args 0")),"prepared refusal");
        require(contains(send("break 0 4 7"),"breakpoint"),"set actual Wasm break");
        send("continue");
        auto const deadline{std::chrono::steady_clock::now()+std::chrono::seconds{20}};
        fast_io::string stopped;
        do { fast_io::this_thread::sleep_for(std::chrono::milliseconds{5});stopped=send("status");if(std::chrono::steady_clock::now()>deadline) { throw std::runtime_error{"breakpoint stop timeout"}; } }
        while(!contains(stopped,"stopped: breakpoint"));
        require(true,"actual guest stopped");
        require(contains(send("info wasip1 args 0"),"\"before\""),"original argv");
        require(contains(send("info wasip1 env 0"),"UWVM_DEBUG_KEY=old"),"original environment");
        require(ok(send("set wasip1 arg 0 1 6166746572")),"edit argv");
        require(ok(send("set wasip1 env 0 5557564d5f44454255475f4b4559 6e6577")),"edit environment");
        require(contains(send("set wasip1 checkpoint 0 0"),"checkpoint Wasm and WASIp1 together"),"joint checkpoint reminder");
        if(directory!=nullptr)
        {
            auto checkpoint_path=fast_io::concat_fast_io(fast_io::mnp::os_c_str(directory),"/portable-",fast_io::mnp::os_c_str(policy),"-",std::chrono::steady_clock::now().time_since_epoch().count(),".uwp");
            fast_io::string path_hex{};constexpr char digits[]{"0123456789abcdef"};
            for(auto c:checkpoint_path) { auto value=static_cast<unsigned char>(c);path_hex.push_back(digits[value>>4u]);path_hex.push_back(digits[value&15u]); }
            auto exported=send(fast_io::concat_fast_io("set wasip1 export 0 ",path_hex));
            require(ok(exported) && contains(exported,"wasip1-portable resources=") && contains(exported,"checkpoint Wasm and WASIp1 together"),"portable export and paired reminder");
            { fast_io::native_file saved{checkpoint_path,fast_io::open_mode::in};require(fast_io::status(saved).type==fast_io::file_type::regular && fast_io::status(saved).size>=156u,"actual persistent metadata file"); }
            auto exclusive=send(fast_io::concat_fast_io("set wasip1 export 0 ",path_hex));
            require(!ok(exclusive) && contains(exclusive,"applied=0"),"portable exclusive save refusal");
            require(ok(send("set wasip1 arg 0 1 77726f6e67")),"edit before portable import");
            auto imported=send(fast_io::concat_fast_io("set wasip1 import 0 ",path_hex));
            require(ok(imported) && contains(imported,"content=external") && contains(imported,"applied=1"),"portable import under actual stop");
            require(contains(send("info wasip1 args 0"),"\"after\""),"portable owned argv restored");
            require(contains(send("info wasip1 env 0"),"UWVM_DEBUG_KEY=new"),"portable owned environment restored");
            fast_io::native_unlinkat(fast_io::at_fdcwd(),checkpoint_path,{});
        }
        auto created{send("set wasip1 file 0 410042")};require(ok(created),"binary managed file");
        auto const fd{affected(created)};
        auto alias_reply{send(fast_io::concat_fast_io("set wasip1 fd-dup 0 ",fd," 0x60006e 0"))};
        require(ok(alias_reply),"duplicate managed FD");auto const alias{affected(alias_reply)};
        require(contains(send("set wasip1 checkpoint 0 1"),"managed=1"),"capture aliased file");
        require(!ok(send("set wasip1 checkpoint 0 8")),"invalid slot refusal");
        require(ok(send("set wasip1 arg 0 1 77726f6e67")),"mutation after checkpoint");
        require(contains(send("set wasip1 restore 0 1 strict"),"resource rollback unavailable"),"strict external resource refusal");
        require(contains(send("info wasip1 args 0"),"\"wrong\""),"strict refusal leaves state intact");
        require(ok(send(fast_io::concat_fast_io("unset wasip1 fd 0 ",fd," 0x60006e 0"))),"close original FD");
        require(ok(send(fast_io::concat_fast_io("unset wasip1 fd 0 ",alias," 0x60006e 0"))),"close alias FD");
        require(ok(send("set wasip1 file 0 726575736564")),"reuse freed descriptor");
        require(contains(send("set wasip1 restore 0 1 bindings"),"managed=1"),"restore managed bytes and bindings");
        require(contains(send("info wasip1 args 0"),"\"after\""),"restore argv");
        auto fds{send("info wasip1 fds 0")};
        require(contains(fds,view(fast_io::concat_fast_io("fd=",fd," "))) && contains(fds,view(fast_io::concat_fast_io("fd=",alias," "))),"restore alias FD numbers");
        require(contains(send("set wasip1 restore 0 0 bindings"),"managed=0"),"restore baseline removes managed file");
        require(ok(send("unset wasip1 checkpoint 0 1")),"drop managed checkpoint");
        require(ok(send("unset wasip1 checkpoint 0 0")),"drop baseline checkpoint");
        require(contains(send("trace wasip1 on all"),"wasip1-trace enabled=1"),"enable dynamic syscall trace");
        send("delete 1");send("continue");
        fast_io::string exited;
        do { fast_io::this_thread::sleep_for(std::chrono::milliseconds{5});exited=send("status");if(std::chrono::steady_clock::now()>deadline+std::chrono::seconds{20}) { throw std::runtime_error{"guest exit timeout"}; } }
        while(!contains(exited,"guest exited:"));
        require(contains(exited,"guest exited: 0"),"real guest reads restored argv and env");
        auto trace{send("trace wasip1 read 0 64")};
        for(auto name:{"name=args_sizes_get","name=args_get","name=environ_sizes_get","name=environ_get"})
        { require(contains(trace,name),name); }
        require(contains(trace,"phase=entry") && contains(trace,"phase=return") && contains(trace,"errno-name=esuccess"),"trace entry return errno");
        require(!ok(send("info wasip1 env 0")),"exited refusal");
        fast_io::io::println(command_input,"quit");
#if defined(__APPLE__) || defined(__FreeBSD__)
        require(fast_io::wait_status_to_int(fast_io::wait(child))==0,"actual debugger process exited");
        ::termios restored{};
        require(fast_io::noexcept_call(::tcgetattr,slave,&restored)==0 &&
            original.c_iflag==restored.c_iflag && original.c_oflag==restored.c_oflag &&
            original.c_cflag==restored.c_cflag && (original.c_lflag & ~PENDIN)==(restored.c_lflag & ~PENDIN) &&
            std::memcmp(original.c_cc,restored.c_cc,sizeof(original.c_cc))==0 &&
            ::cfgetispeed(&original)==::cfgetispeed(&restored) && ::cfgetospeed(&original)==::cfgetospeed(&restored),
            "real terminal settings restored");
#else
        input.out().close();
#if defined(_WIN32) && !defined(__CYGWIN__)
        require(fast_io::wait_status_to_int(fast_io::wait(child))==0,"actual debugger process exited");
        prompt_process=nullptr;
#endif
        char bytes[1024];for(;;)
        { auto const end{fast_io::operations::read_some(reply_output,bytes,bytes+sizeof(bytes))};if(end==bytes) { break; }
          fast_io::io::print(fast_io::mnp::strvw(bytes,end)); }
#if !defined(_WIN32) || defined(__CYGWIN__)
        require(fast_io::wait_status_to_int(fast_io::wait(child))==0,"actual debugger process exited");
#endif
#endif
    }
    catch(...)
    {
        // Only this exact owned process handle is signalled; the caller records
        // failure and verifies its process/VM retirement. No retirement ACK.
        auto const failure{std::current_exception()};
        try { if(child) { fast_io::kill(child,test_wait_status{9});fast_io::wait(child); } } catch(...) {}
#if defined(_WIN32) && !defined(__CYGWIN__)
        prompt_process=nullptr;
#endif
        std::rethrow_exception(failure);
    }
    fast_io::io::println("CLI_POLICY_PASS ",fast_io::mnp::os_c_str(policy));
}
int main(int argc,char** argv)
{
    if(argc!=3 && argc!=4) { fast_io::io::perrln("usage: wasip1_debug_cli_platform UWVM CLI_WASM [PRIVATE_OUTPUT_DIRECTORY]");return 2; }
    try
    {
#if defined(_WIN32) && !defined(__CYGWIN__)
        io_phase="NT inheritance regression";
        inherited_file_round(argc==4?argv[3]:nullptr);
#endif
        policy_round(argv[1],argv[2],"instruction",argc==4?argv[3]:nullptr);
        policy_round(argv[1],argv[2],"unwind",argc==4?argv[3]:nullptr);
    }
    catch(std::exception const& e) { fast_io::io::perrln("CLI_PLATFORM_FAILURE ",fast_io::mnp::os_c_str(e.what()));return 1; }
    catch(fast_io::error const& e) { fast_io::io::perrln("CLI_PLATFORM_FAILURE phase=",fast_io::mnp::os_c_str(io_phase)," IO domain=",fast_io::mnp::hex(e.domain)," code=",fast_io::mnp::hex(e.code));return 1; }
    catch(...) { fast_io::io::perrln("CLI_PLATFORM_FAILURE platform IO exception");return 1; }
    fast_io::io::println("wasip1_debug_cli_platform PASS checks=",checks);
}

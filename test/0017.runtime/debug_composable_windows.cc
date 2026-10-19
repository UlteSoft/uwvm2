// Actual Windows ConPTY interaction; the host runs this inside its bounded QEMU.
#define _WIN32_WINNT 0x0a00
#include <windows.h>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <algorithm>
static void check(bool value,char const* why)
{ if(!value) { throw std::runtime_error(std::string(why)+" (Win32="+std::to_string(GetLastError())+")"); } }
static std::string plain(std::string const& s)
{
    std::string out;
    for(std::size_t i{};i<s.size();)
    {
        if(s[i]=='\x1b' && i+1<s.size() && s[i+1]=='[')
        { i+=2;while(i<s.size() && !(s[i]>='@' && s[i]<='~')) { ++i; } if(i<s.size()) { ++i; } }
        else { out+=s[i++]; }
    }
    return out;
}
static std::wstring wide(char const* s)
{ auto n=MultiByteToWideChar(CP_UTF8,0,s,-1,nullptr,0);std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,0,s,-1,w.data(),n);w.pop_back();return w; }
// ConPTY itself emits differences: an unchanged '[' need not occur in the new
// bytes when a pane title changes. Match pane assertions against painted cells.
static std::string painted(std::string const& input,int columns,int rows)
{
    std::vector<std::string> grid(rows,std::string(columns,' '));int x{},y{};
    auto linefeed=[&] { if(++y==rows) { grid.erase(grid.begin());grid.emplace_back(columns,' ');--y; } };
    for(std::size_t i{};i<input.size();)
    {
        auto ch=static_cast<unsigned char>(input[i++]);
        if(ch==27 && i<input.size())
        {
            auto kind=input[i++];
            if(kind==']') { while(i<input.size() && input[i]!='\a' && !(input[i]=='\x1b' && i+1<input.size() && input[i+1]=='\\')) { ++i; } if(i<input.size()) { i+=input[i]=='\a' ? 1u:2u; } continue; }
            if(kind!='[') { continue; }
            std::string params;while(i<input.size() && !(input[i]>='@' && input[i]<='~')) { params+=input[i++]; }
            if(i==input.size()) { break; }auto final=input[i++];
            if(params=="?1049" && final=='h') { for(auto& row:grid) { row.assign(columns,' '); } x=y=0;continue; }
            if(!params.empty() && params[0]=='?') { continue; }
            std::vector<int> values(1);for(auto c:params) { if(c==';') { values.push_back(0); } else if(c>='0' && c<='9') { values.back()=values.back()*10+c-'0'; } }
            auto a=values[0],n=a ? a:1;
            if(final=='H' || final=='f') { y=std::clamp(n-1,0,rows-1);x=std::clamp(values.size()>1 && values[1] ? values[1]-1:0,0,columns-1); }
            else if(final=='A') { y=std::max(0,y-n); } else if(final=='B') { y=std::min(rows-1,y+n); }
            else if(final=='C') { x=std::min(columns-1,x+n); } else if(final=='D') { x=std::max(0,x-n); }
            else if(final=='G') { x=std::clamp(n-1,0,columns-1); } else if(final=='d') { y=std::clamp(n-1,0,rows-1); }
            else if(final=='X') { grid[y].replace(x,std::min(n,columns-x),std::min(n,columns-x),' '); }
            else if(final=='K') { auto first=a==0 ? x:0,last=a==1 ? x+1:columns;grid[y].replace(first,last-first,last-first,' '); }
            else if(final=='J') { for(int row{};row<rows;++row) { for(int col{};col<columns;++col) { if(a==2 || (a==0 && row*columns+col>=y*columns+x) || (a==1 && row*columns+col<=y*columns+x)) { grid[row][col]=' '; } } } }
            continue;
        }
        if(ch=='\r') { x=0; } else if(ch=='\n') { linefeed(); } else if(ch=='\b') { x=std::max(0,x-1); }
        else if(ch>=32 && ch!=127 && (ch&0xc0u)!=0x80u)
        { if(x==columns) { x=0;linefeed(); }grid[y][x++]=ch<128 ? static_cast<char>(ch):'?'; }
    }
    std::string result;for(auto const& row:grid) { result+=row;result+='\n'; }return result;
}
struct console
{
    HANDLE input{},output{},process{},job{};HPCON pty{};
    std::thread reader;std::mutex mutex;std::string log,last_raw;bool overflow{};int columns{120},rows{40};
    explicit console(std::wstring command)
    {
        HANDLE in_read{},out_write{};
        check(CreatePipe(&in_read,&input,nullptr,0) && CreatePipe(&output,&out_write,nullptr,0),"ConPTY pipes");
        auto hr=CreatePseudoConsole({120,40},in_read,out_write,0,&pty);
        check(SUCCEEDED(hr),"actual ConPTY");
        SIZE_T size{};InitializeProcThreadAttributeList(nullptr,1,0,&size);
        auto attrs=static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(),0,size));
        check(attrs && InitializeProcThreadAttributeList(attrs,1,0,&size),"process attributes");
        check(UpdateProcThreadAttribute(attrs,0,PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,pty,sizeof(pty),nullptr,nullptr),"ConPTY attached");
        STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=attrs;PROCESS_INFORMATION pi{};
        // PowerShell may redirect this helper to pipes. Clear its standard
        // handle slots only during child creation so ConPTY installs all three
        // child console handles; restore the helper's own log pipes immediately.
        DWORD slots[]{STD_INPUT_HANDLE,STD_OUTPUT_HANDLE,STD_ERROR_HANDLE};
        HANDLE saved[3]{};
        for(unsigned i{};i!=3;++i) { saved[i]=GetStdHandle(slots[i]);SetStdHandle(slots[i],nullptr); }
        auto ok=CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,EXTENDED_STARTUPINFO_PRESENT|CREATE_SUSPENDED,nullptr,nullptr,&startup.StartupInfo,&pi);
        for(unsigned i{};i!=3;++i) { SetStdHandle(slots[i],saved[i]); }
        CloseHandle(in_read);CloseHandle(out_write);
        DeleteProcThreadAttributeList(attrs);HeapFree(GetProcessHeap(),0,attrs);check(ok,"product launch suspended");
        process=pi.hProcess;job=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        check(job && SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)) && AssignProcessToJobObject(job,process),"owned process job");
        reader=std::thread([this]
        {
            char bytes[16384];DWORD n{};
            while(ReadFile(output,bytes,sizeof(bytes),&n,nullptr) && n)
            { std::lock_guard lock(mutex);if(log.size()+n>16u*1024u*1024u) { overflow=true;break; } log.append(bytes,n); }
        });
        ResumeThread(pi.hThread);CloseHandle(pi.hThread);
    }
    std::string snapshot() { std::lock_guard lock(mutex);check(!overflow,"bounded transcript");return log; }
    std::string wait(std::size_t at,char const* needle)
    {
        for(unsigned i{};i<400;++i)
        { auto full=snapshot();auto raw=full.substr(at);if(plain(raw).find(needle)!=std::string::npos ||
              (!raw.empty() && needle[0]=='[' && painted(full,columns,rows).find(needle)!=std::string::npos)) { Sleep(200);return snapshot().substr(at); }
          if(WaitForSingleObject(process,0)!=WAIT_TIMEOUT) { DWORD status{};GetExitCodeProcess(process,&status);throw std::runtime_error("product exit "+std::to_string(status)); } Sleep(50); }
        throw std::runtime_error(std::string("missing reply ")+needle);
    }
    std::string send(std::string s,char const* expected=nullptr)
    {
        auto at=snapshot().size();std::printf("KEYS %s\n",s.c_str());std::fflush(stdout);
        s+='\r';for(char c:s) { DWORD n{};check(WriteFile(input,&c,1,&n,nullptr)&&n==1,"key delivered");Sleep(2); }
        if(expected) { last_raw=wait(at,expected); }
        else { Sleep(500);last_raw=snapshot().substr(at); }
        return plain(last_raw);
    }
    void finish()
    { send("quit");check(WaitForSingleObject(process,10000)==WAIT_OBJECT_0,"product retired");DWORD status{};GetExitCodeProcess(process,&status);check(status==0,"exit zero"); }
    ~console()
    {
        if(job) { CloseHandle(job); } if(process) { WaitForSingleObject(process,5000);CloseHandle(process); }
        if(input) { CloseHandle(input); } if(pty) { ClosePseudoConsole(pty); }
        if(reader.joinable()) { reader.join(); } if(output) { CloseHandle(output); }
    }
};
#ifndef UWVM_CONPTY_HELPER_ONLY
int main(int argc,char** argv)
{
    if(argc!=6) { return 2; }
    std::string transcript;
    try
    {
        auto binary=wide(argv[1]),wasm=wide(argv[2]),source=wide(argv[3]),root=wide(argv[4]);
        CreateDirectoryW(root.c_str(),nullptr);
        auto old=root+L"\\open.txt",moved=root+L"\\moved.txt";
        HANDLE file=CreateFileW(old.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_ALWAYS,0,nullptr);
        check(file!=INVALID_HANDLE_VALUE,"fixture file");CloseHandle(file);
        console c(L"\""+binary+L"\" -Rdbg -Rct 0 -Rllvm-call-stack instruction -Rllvm-cache-path disable --wasip1-global-mount-dir /sandbox \""+root+L"\" --run \""+wasm+L"\"");
        try
        {
            c.wait(0,"(uwvm-debug) ");
            c.send("help layout config save","layout config save <filePath>");
            c.send("help layout config load","layout config load <filePath>");
            c.send("layout config save   ","error: layout config save <filePath>");
            c.send("layout config load   ","error: layout config save <filePath>");
            auto s=c.send("  bw   \t");check(s.find("bw   0")!=std::string::npos,"module completion with spaces");
            std::string source_path{argv[3]};for(auto& ch:source_path) { if(ch=='\\') { ch='/'; } }
            c.send("bs 0 "+source_path+":19","breakpoint ");c.send("input WIN_STDIN","Guest stdin queued");
            c.send("run","stopped: breakpoint");s=c.send("print error_code","error_code = ");check(s.find("error_code = 0")!=std::string::npos,"WASI path_open");
            s=c.send("info wasip1 fds 0","current-path=");check(s.find("open.txt")!=std::string::npos,"live Windows file name");
            check(MoveFileExW(old.c_str(),moved.c_str(),0),"rename open WASI file");
            s=c.send("info wasip1 fds 0","current-path=");check(s.find("moved.txt")!=std::string::npos && s.find("open.txt")==std::string::npos,"renamed native handle path");
            c.send(std::string("set source ")+argv[3]);s=c.send("tui enable","layout src");check(s.find("=> 19")!=std::string::npos,"exact source marker");
            s=c.send("sw","N> ");c.send("layout wasm locals stack memory","[memory");c.send("focus stack","[operands *]");
            c.send("layout horizontal src wasm asm","[asm");
            auto config=std::string(argv[4])+"/saved layout.txt";
            c.send("layout config save "+config,"saved");
            check(std::ifstream(config).good(),"explicit layout file created");
            c.send("layout memory","[memory");
            c.send("layout config load "+config,"loaded");
            check(SUCCEEDED(ResizePseudoConsole(c.pty,{160,50})),"grow real terminal");c.columns=160;c.rows=50;Sleep(500);
            check(SUCCEEDED(ResizePseudoConsole(c.pty,{96,30})),"shrink real terminal");c.columns=96;c.rows=30;Sleep(500);
            check(SUCCEEDED(ResizePseudoConsole(c.pty,{120,40})),"restore real terminal");c.columns=120;c.rows=40;Sleep(500);
            c.send("layout io","[io");s=c.send("continue","Program exited normally.");
            check(s.find("TUI_STDOUT")!=std::string::npos && s.find("TUI_STDERR")!=std::string::npos && s.find("WIN_STDIN")!=std::string::npos,"three isolated guest streams");
            c.send("layout wasm");s=c.send("startw","stopped: first Wasm instruction");check(c.last_raw.find("\x1b[?1049l")==std::string::npos,"restart keeps TUI");
            c.send("layout asm");c.send("starti","native-pc=");c.send("tui disable");
            s=c.send("status","native-pc=");check(s.find("native-pc=")!=std::string::npos,"real Windows native trap");
            c.send("si","stopped: native instruction step");c.send("bi 65536","error:");
            c.send("layout horizontal src asm","[asm");c.send("tui disable");
            s=c.send("help step wasm","Usage:");check(s.find("wasm")!=std::string::npos,"level help");
            c.send("startw","stopped: first Wasm instruction");c.send("delete 1","breakpoint deleted");c.send("input PLAIN_WIN");
            s=c.send("continue","Program exited normally.");check(s.find("TUI_STDOUT")!=std::string::npos && s.find("PLAIN_WIN")!=std::string::npos,"plain mixed IO");
            c.finish();transcript=c.snapshot();
        }
        catch(...) { transcript=c.snapshot();throw; }
        std::ofstream(argv[5],std::ios::binary)<<transcript;std::puts("PASS actual Windows ConPTY layouts, source, Wasm, native stops, IO and renamed paths");return 0;
    }
    catch(std::exception const& error)
    { std::ofstream(argv[5],std::ios::binary)<<transcript;std::fprintf(stderr,"FAIL %s (Win32=%lu)\n",error.what(),GetLastError());return 1; }
}
#endif

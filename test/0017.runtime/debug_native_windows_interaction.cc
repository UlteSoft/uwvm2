// Reuse the actual ConPTY and owned job harness, without its layout test main.
#define UWVM_CONPTY_HELPER_ONLY
#include "debug_composable_windows.cc"
#include <regex>

int main(int argc,char** argv)
{
    if(argc!=4) { return 2; }
    std::string transcript;unsigned actions{};
    try
    {
        console c(L"\""+wide(argv[1])+L"\" -Rdbg -Rct 0 -Rllvm-cache-path disable --run \""+wide(argv[2])+L"\"");
        try
        {
            auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(6);
            auto command=[&](std::string const& text,bool allow_error=false)
            {
                check(std::chrono::steady_clock::now()<deadline,"native suite deadline");
                auto at=c.snapshot().size();DWORD n{};auto input=text+'\r';
                check(WriteFile(c.input,input.data(),static_cast<DWORD>(input.size()),&n,nullptr) && n==input.size(),"native command delivered");
                for(unsigned i{};i<400;++i)
                {
                    auto reply=plain(c.snapshot().substr(at));
                    // ConPTY can encode the final prompt space as CSI 1 C.
                    if((reply.ends_with("(uwvm-debug) ") || reply.ends_with("(uwvm-debug)")) && reply.find('\n')!=std::string::npos)
                    {
                        check(allow_error || reply.find("error:")==std::string::npos,reply.c_str());
                        ++actions;std::printf("PASS command %u %s\n",actions,text.c_str());std::fflush(stdout);return reply;
                    }
                    check(WaitForSingleObject(c.process,0)==WAIT_TIMEOUT,"native product alive");Sleep(25);
                }
                throw std::runtime_error("native reply deadline: "+text);
            };
            auto field=[](std::string const& reply,char const* pattern)
            { std::smatch match;check(std::regex_search(reply,match,std::regex(pattern)),reply.c_str());return match[1].str(); };
            auto function=[&](std::string const& reply) { return field(reply,"thread 1 module=0 function=([0-9]+)"); };
            auto native=[](std::string const& reply) { check(reply.find("native-pc=0x")!=std::string::npos,"actual owned native stop"); };
            auto start_call=[&]
            {
                command("start");auto point=field(command("bw 0 2 40"),"breakpoint ([0-9]+) registered");
                check(command("c").find("byte-offset=40")!=std::string::npos,"actual call breakpoint");command("delete "+point);
            };
            c.wait(0,"(uwvm-debug) ");native(command("starti"));
            auto before=command("status");command("jump asm *0x1",true);
            auto after=command("status");check(field(before,"native-pc=(0x[0-9a-f]+)")==field(after,"native-pc=(0x[0-9a-f]+)"),"outside owner retains PC");
            native(command("ni"));check(command("list wasm").find("N>")!=std::string::npos,"native stop has Wasm preview");
            start_call();std::string reply;unsigned steps{};
            for(;steps<1024;++steps) { reply=command("si");if(function(reply)=="1") { break; } }
            check(steps<1024,"SI entered guest leaf");native(reply);
            reply=command("fi");native(reply);check(function(reply)=="2","FI returned to caller");
            reply=command("nw");check(function(reply)=="2" && reply.find("native-pc=")==std::string::npos,"native to Wasm handoff");
            start_call();
            for(steps=0;steps<1024;++steps)
            {
                reply=command("ni");native(reply);check(function(reply)=="2","NI retains guest caller");
                if(std::regex_search(reply,std::regex("native-wasm provenance=exact .*byte-offset=46\\b"))) { break; }
            }
            check(steps<1024,"NI reached caller continuation");
            reply=command("fi");native(reply);check(function(reply)=="3","FI returned to root");
            before=command("status");command("fi",true);after=command("status");
            check(field(before,"stop-id ([0-9]+)")==field(after,"stop-id ([0-9]+)"),"root finish retains stop");
            command("c");c.finish();transcript=c.snapshot();
        }
        catch(...) { transcript=c.snapshot();throw; }
        std::ofstream(argv[3],std::ios::binary)<<transcript;
        std::printf("PASS Windows native SI/NI/finish, owned boundaries, Wasm handoff; %u actions\n",actions);return 0;
    }
    catch(std::exception const& error)
    { std::ofstream(argv[3],std::ios::binary)<<transcript;std::fprintf(stderr,"FAIL %s\n",error.what());return 1; }
}

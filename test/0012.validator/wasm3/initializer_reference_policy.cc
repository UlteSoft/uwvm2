// A changed initialization policy must not bypass the Core 3 encoding gate retained by the parser.
#include <uwvm2/uwvm/wasm/feature/impl.h>
#include <uwvm2/uwvm/runtime/initializer/impl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
namespace f=uwvm2::uwvm::wasm::feature;namespace init=uwvm2::uwvm::runtime::initializer::details;
using bytes=std::vector<std::byte>;unsigned checks{};
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
bytes raw(std::initializer_list<unsigned> v){bytes b;for(auto x:v)b.push_back(std::byte(x));return b;}
void leb(bytes& b,std::size_t n){do{auto x=n&127;n>>=7;b.push_back(std::byte(x|(n?128:0)));}while(n);}
bytes module(bytes const& type, unsigned id=0, bytes const& payload={}){auto b=raw({0,0x61,0x73,0x6d,1,0,0,0,1});leb(b,type.size());b.insert(b.end(),type.begin(),type.end());if(id){b.push_back(std::byte(id));leb(b,payload.size());b.insert(b.end(),payload.begin(),payload.end());}return b;}
void check(bytes const& binary,bool restricted,unsigned restriction=0){
 f::wasm_binfmt_ver1_feature_parameter_storage_t p{};auto& options=f::wasm_binfmt_ver1_wasm1p1_parameter(p);options.disable_function_references=false;options.disable_table_initializer=false;options.disable_exceptions=false;
 uwvm2::parser::wasm::base::error_impl err{};auto m=f::binfmt_ver1_handler(binary.data(),binary.data()+binary.size(),err,p);CHECK(err.err_code==uwvm2::parser::wasm::base::wasm_parse_error_code::ok);
 init::enforce_wasm1p1_initializer_feature_parameters(m,p);if(restriction==0)options.disable_function_references=true;else if(restriction==1)options.disable_reference_types=true;else if(restriction==2)options.disable_simd=true;else options.disable_exceptions=true;
 int pipes[2];CHECK(pipe(pipes)==0);auto pid=fork();CHECK(pid>=0);
 if(pid==0){close(pipes[0]);if(dup2(pipes[1],2)!=2)std::_Exit(55);close(pipes[1]);uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());init::enforce_wasm1p1_initializer_feature_parameters(m,p);std::_Exit(0);}
 close(pipes[1]);char buffer[2048];ssize_t n;std::string log;while((n=read(pipes[0],buffer,sizeof(buffer)))>0)log.append(buffer,n);close(pipes[0]);int status;CHECK(waitpid(pid,&status,0)==pid);CHECK((WIFEXITED(status)&&WEXITSTATUS(status)==0)==!restricted);
 if(restricted){CHECK(log.find("[fatal]")!=std::string::npos);if(restriction==0||restriction==3)CHECK(log.find("WebAssembly 3.0")!=std::string::npos);CHECK(log.find(restriction==0?"--wasm-feature-enable-function-references":restriction==1?"reference-types":restriction==2?"simd":"--wasm-feature-enable-exceptions")!=std::string::npos);}
}
int main(){rlimit lim{0,0};CHECK(setrlimit(RLIMIT_CORE,&lim)==0);
 check(module(raw({1,0x60,1,0x63,0x70,0})),true);check(module(raw({1,0x60,1,0x70,0})),false);check(module(raw({1,0x60,0,0})),false);
 auto type=raw({1,0x60,0,0});check(module(type,6,raw({1,0x70,0,0xd0,0,0x0b})),true);check(module(type,6,raw({1,0x70,0,0xd0,0x70,0x0b})),false);
 check(module(type,4,raw({1,0x40,0,0x70,0,1,0xd0,0,0x0b})),true);
 check(module(type,9,raw({1,5,0x70,1,0xd0,0,0x0b})),true);
 bytes many;leb(many,129);for(unsigned i=0;i<129;++i){auto t=raw({0x60,0,0});many.insert(many.end(),t.begin(),t.end());}
 auto binary=module(many,6,raw({1,0x70,0,0xd0,0x80,1,0x0b}));check(binary,true);
 for(auto local:{raw({0x63,0x70}),raw({0x63,0x6f}),raw({0x70}),raw({0x6f}),raw({0x7b}),raw({0x7f})})for(unsigned count:{0,1}){
  auto b=module(type,3,raw({1,0}));auto body=raw({1,count});body.insert(body.end(),local.begin(),local.end());body.push_back(std::byte{11});auto payload=raw({1});leb(payload,body.size());payload.insert(payload.end(),body.begin(),body.end());b.push_back(std::byte{10});leb(b,payload.size());b.insert(b.end(),payload.begin(),payload.end());
  check(b,local.size()==2);check(b,local.back()==std::byte{0x70}||local.back()==std::byte{0x6f},1);check(b,local.back()==std::byte{0x7b},2);
 }
 for(bool imported:{false,true})for(bool table:{false,true})for(unsigned heap:{0x70,0x6f}){
  auto payload=raw({1});if(imported){auto name=raw({1,'m',1,'x',table?1u:3u});payload.insert(payload.end(),name.begin(),name.end());}
  auto t=raw({0x63,heap,0});payload.insert(payload.end(),t.begin(),t.end());if(table)payload.push_back(std::byte{1});else if(!imported){auto init=raw({0xd0,heap,11});payload.insert(payload.end(),init.begin(),init.end());}
  auto b=module(type,imported?2:table?4:6,payload);check(b,true);check(b,true,1);check(b,false,2);
 }
 for(unsigned flag:{5,6,7})for(unsigned heap:{0x70,0x6f})for(unsigned count:{0,1}){
  auto b=flag==6?module(type,4,raw({1,heap,0,count})):module(type);auto payload=raw({1,flag});if(flag==6){auto offset=raw({0,0x41,0,11});payload.insert(payload.end(),offset.begin(),offset.end());}
  auto value=raw({0x63,heap,count});payload.insert(payload.end(),value.begin(),value.end());if(count){auto expr=raw({0xd0,heap,11});payload.insert(payload.end(),expr.begin(),expr.end());}
  b.push_back(std::byte{9});leb(b,payload.size());b.insert(b.end(),payload.begin(),payload.end());check(b,true);check(b,true,1);check(b,false,2);
 }
 for(unsigned count:{0,1,128}){bytes payload;leb(payload,count);for(unsigned i=0;i<count;++i){payload.push_back(std::byte{0});payload.push_back(std::byte{0});}auto b=module(type,13,payload);check(b,true,3);check(b,false,0);}
 check(module(type,2,raw({1,1,'m',1,'t',4,0,0})),true,3);
 std::printf("PASS Core 3 initializer policy: %u checks across signatures, locals, globals, tables and element expressions\n",checks);
}

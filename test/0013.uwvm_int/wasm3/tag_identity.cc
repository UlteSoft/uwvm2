// Tag identities are per instance; imports/reexports alias the concrete instance, not a structural signature.
#include "../strict/uwvm_int_translate_strict_common.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
using namespace uwvm2test::uwvm_int_strict;
namespace rt=uwvm2::uwvm::runtime::storage;
unsigned checks{};
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
void bytes(byte_vec& b,std::initializer_list<unsigned> v){for(auto x:v)append_u8(b,x);}
void section(byte_vec& b,unsigned id,byte_vec const& p){append_u8(b,id);append_u32_leb(b,p.size());b.insert(b.end(),p.begin(),p.end());}
byte_vec module(unsigned params=0,unsigned type=0x7f,char from=0,unsigned count=2,bool export_tag=true){
 byte_vec b;bytes(b,{0,0x61,0x73,0x6d,1,0,0,0});byte_vec t;bytes(t,{1,0x60,params});for(unsigned i=0;i<params;++i)append_u8(t,type);append_u8(t,0);section(b,1,t);
 byte_vec p;if(from){append_u32_leb(p,count);for(unsigned i=0;i<count;++i)bytes(p,{1,unsigned(from),1,'t',4,0,0});section(b,2,p);}else{append_u32_leb(p,count);for(unsigned i=0;i<count;++i)bytes(p,{0,0});section(b,13,p);}
 if(export_tag){byte_vec exp;bytes(exp,{1,1,'t',4,0});section(b,7,exp);}return b;
}
auto policy(){auto p=make_wasm1p1_feature_parameter();uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(p).disable_exceptions=false;return p;}
void success(){
 for(unsigned count:{1,2,128})for(unsigned params:{0,1,8}){
  auto p=policy();auto provider=module(params,0x7f,0,count);auto alias=module(params,0x7f,'p',2);auto consumer=module(params,0x7f,'a',count);
  auto prepared=prepare_runtime_from_wasm(consumer,u8"tag-identities",{{&alias,u8"a",&p},{&provider,u8"p",&p},{&provider,u8"q",&p}},p);
  auto pi=rt::wasm_module_runtime_storage.find(u8"p");auto ai=rt::wasm_module_runtime_storage.find(u8"a");CHECK(pi!=rt::wasm_module_runtime_storage.end());CHECK(ai!=rt::wasm_module_runtime_storage.end());
  CHECK(pi->second.local_defined_tag_vec_storage.size()==count);auto identity=std::addressof(pi->second.local_defined_tag_vec_storage.index_unchecked(0));
  auto qi=rt::wasm_module_runtime_storage.find(u8"q");CHECK(qi!=rt::wasm_module_runtime_storage.end());CHECK(identity!=std::addressof(qi->second.local_defined_tag_vec_storage.index_unchecked(0)));
  for(auto const& t:prepared.mod->imported_tag_vec_storage)CHECK(t.resolved_tag==identity);
  for(auto const& t:ai->second.imported_tag_vec_storage)CHECK(t.resolved_tag==identity);
  if(count>1)CHECK(identity!=std::addressof(pi->second.local_defined_tag_vec_storage.index_unchecked(1)));
 }
}
void reject(unsigned which){
 int pipes[2];CHECK(pipe(pipes)==0);auto child=fork();CHECK(child>=0);if(child==0){close(pipes[0]);if(dup2(pipes[1],2)!=2)std::_Exit(90);close(pipes[1]);uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());auto p=policy();auto provider=module(1,0x7f,which==2?'a':0);auto alias=module(1,0x7f,'p');auto consumer=module(which==0?2:1,which==1?0x7c:0x7f,which==3?'x':'a');auto prepared=prepare_runtime_from_wasm(consumer,u8"tag-rejected",{{&alias,u8"a",&p},{&provider,u8"p",&p}},p);std::_Exit(0);}
 close(pipes[1]);std::string log;char buf[1024];ssize_t n;while((n=read(pipes[0],buf,sizeof(buf)))>0)log.append(buf,n);close(pipes[0]);int status;CHECK(waitpid(child,&status,0)==child);CHECK(!(WIFEXITED(status)&&WEXITSTATUS(status)==0));if(which==3){CHECK(log.find("[error]")!=std::string::npos);CHECK(log.find("Missing module dependency")!=std::string::npos);}else{CHECK(log.find("[fatal]")!=std::string::npos);CHECK(log.find(which<2?"tag payload type mismatch":"cyclic tag alias")!=std::string::npos);}
}
int main(){rlimit limit{0,0};CHECK(setrlimit(RLIMIT_CORE,&limit)==0);success();for(unsigned i=0;i<4;++i)reject(i);std::printf("PASS %u Core 3 tag instance/alias/type rejection checks\n",checks);}

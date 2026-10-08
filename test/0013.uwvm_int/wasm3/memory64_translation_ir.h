#pragma once
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <llvm/Support/FileSystem.h>
#include <string>
inline bool memory64_fixture_unwind{};
inline void configure_memory64_jit_fixture_policy(int argc,char** argv)
{
 namespace mode=uwvm2::uwvm::runtime::runtime_mode;
 if(argc>2)std::abort();
 if(argc==2)
 {
  std::string policy=argv[1];if(policy!="instruction"&&policy!="unwind")std::abort();
  memory64_fixture_unwind=policy=="unwind";
 }
 mode::runtime_llvm_jit_call_stack_existed=true;
 mode::global_runtime_llvm_jit_call_stack=memory64_fixture_unwind?mode::runtime_llvm_jit_call_stack_t::unwind:mode::runtime_llvm_jit_call_stack_t::instruction;
 mode::runtime_llvm_jit_full_policy_existed=true;
 mode::global_runtime_llvm_jit_full_policy=mode::runtime_llvm_jit_full_policy_t::passbuilder_o3;
}
// Retain the module emitted by the real integrated translator for codegen review.
// The runtime execution test remains authoritative for actual host entry/publication.
inline void save_memory64_translation_ir(llvm::Module const& module,char const* name)
{
 // The strict runner disables caching during setup. Re-enable only a private
 // fixture directory after setup so the runtime's actual linked object can be
 // inspected; the offline IR dump is not substituted for native code evidence.
 if(auto directory=std::getenv("UWVM_MEMORY64_CACHE_DIR"))
 {
  namespace mode=uwvm2::uwvm::runtime::runtime_mode;
  std::string path=std::string(directory)+"/"+(memory64_fixture_unwind?"unwind":"instruction");
  auto first=reinterpret_cast<char8_t const*>(path.data());
  mode::global_runtime_llvm_jit_cache_path.assign(uwvm2::utils::container::u8string_view{first,path.size()});
  mode::global_runtime_llvm_jit_cache_path_mode=mode::runtime_llvm_jit_cache_path_mode_t::custom_path;
 }
 if(auto directory=std::getenv("UWVM_SHARED_IR_DIR"))
 {
  std::error_code error;llvm::raw_fd_ostream output(std::string(directory)+"/"+(memory64_fixture_unwind?"unwind-":"instruction-")+name,error);
  if(error)std::abort();module.print(output,nullptr);
 }
}
#endif

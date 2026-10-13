// Parse actual Core 3 modules and validate every function body with the production wasm3 validator.
#include <uwvm2/uwvm/wasm/feature/impl.h>
#include <uwvm2/validation/standard/wasm3/impl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
namespace f=uwvm2::uwvm::wasm::feature;namespace v=uwvm2::validation::standard::wasm3;namespace b=uwvm2::parser::wasm::base;
int main(int argc,char**argv){if(argc<2)return 2;std::ifstream input(argv[1],std::ios::binary);if(!input)return 2;std::vector<char> file((std::istreambuf_iterator<char>(input)),{});std::vector<std::byte> bytes(file.size());if(file.size())std::memcpy(bytes.data(),file.data(),file.size());
 f::wasm_binfmt_ver1_feature_parameter_storage_t features{};auto& flags=f::wasm_binfmt_ver1_wasm1p1_parameter(features);flags.disable_exceptions=false;flags.disable_function_references=false;
 if(argc>2){flags.disable_function_references=std::strcmp(argv[2],"no-function-references")==0;
  flags.disable_reference_types=std::strcmp(argv[2],"no-reference-types")==0;
  flags.disable_exceptions=std::strcmp(argv[2],"no-exceptions")==0;}
 b::error_impl parse{};f::wasm_binfmt_ver1_module_storage_t parsed{};try{parsed=f::binfmt_ver1_handler(bytes.data(),bytes.data()+bytes.size(),parse,features);}catch(fast_io::error const&){}if(parse.err_code!=b::wasm_parse_error_code::ok){std::printf("parser error %u\n",unsigned(parse.err_code));return 1;}
 using w1=uwvm2::parser::wasm::standard::wasm1::features::wasm1;using w11=uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
 auto const& codes=uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<w1,w11>>(parsed.sections).codes;
 auto const imports=uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<w1,w11>>(parsed.sections).importdesc.index_unchecked(0).size();
 if(argc>2&&std::strcmp(argv[2],"disabled")==0)flags.disable_exceptions=true;
 for(std::size_t i=0;i<codes.size();++i){auto const& code=codes.index_unchecked(i).body;uwvm2::validation::error::code_validation_error_impl err{};try{
  if(argc>2&&std::strcmp(argv[2],"default")==0)v::validate_code(v::wasm3_code_version{},parsed,imports+i,reinterpret_cast<std::byte const*>(code.expr_begin),reinterpret_cast<std::byte const*>(code.code_end),err);
  else v::validate_code(v::wasm3_code_version{},parsed,imports+i,reinterpret_cast<std::byte const*>(code.expr_begin),reinterpret_cast<std::byte const*>(code.code_end),err,features);
 }catch(fast_io::error const&){}if(err.err_code!=uwvm2::validation::error::code_validation_error_code::ok){std::printf("validation error %u\n",unsigned(err.err_code));return 1;}}
 std::puts("PASS production pure Core 3 exception validation");
}

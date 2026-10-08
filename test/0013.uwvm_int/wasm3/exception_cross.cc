// Actual compiler-generated protected calls and escaping throws, including EH-only forward joins.
#include "../strict/uwvm_int_translate_strict_common.h"
#undef UWVM2TEST_REQUIRE
#define UWVM2TEST_REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x); std::abort(); } } while(false)
namespace {
using namespace uwvm2test::uwvm_int_strict;
unsigned checks{};
void bytes(byte_vec& out, std::initializer_list<unsigned> values) { for(auto x: values) { append_u8(out,x); } }
byte_vec add_tag(byte_vec const& original) {
    byte_vec out; out.insert(out.end(),original.begin(),original.begin()+8); std::size_t cursor=8; bool inserted=false;
    while(cursor<original.size()) {
        auto begin=cursor; auto id=std::to_integer<unsigned>(original[cursor++]); unsigned size=0,shift=0,x;
        do { x=std::to_integer<unsigned>(original[cursor++]); size|=(x&127)<<shift; shift+=7; } while(x&128);
        if(!inserted && id>=6) { bytes(out,{13,3,1,0,0}); inserted=true; }
        cursor+=size; out.insert(out.end(),original.begin()+begin,original.begin()+cursor);
    }
    UWVM2TEST_REQUIRE(inserted); return out;
}
template<optable::uwvm_interpreter_translate_option_t Option>
std::byte* UWVM2TEST_WASM_ABI call(std::size_t module, std::size_t function, std::byte* top) UWVM_THROWS {
    UWVM2TEST_REQUIRE(module==SIZE_MAX);
    auto const& info=*reinterpret_cast<optable::compiled_defined_call_info const*>(function);
    auto const& runtime=*static_cast<runtime_local_func_t const*>(info.runtime_func);
    byte_vec params(info.param_bytes);
    if(!params.empty()) { std::memcpy(params.data(),top-params.size(),params.size()); }
    auto result=interpreter_runner<Option>::run(*info.compiled_func,runtime,params,nullptr,nullptr);
    auto dest=top-info.param_bytes;
    if(!result.results.empty()) { std::memcpy(dest,result.results.data(),result.results.size()); }
    return dest+result.results.size();
}
template<optable::uwvm_interpreter_translate_option_t Option> void check_mixed_escape() {
    module_builder b;
    func_type const signature{{0x7f,0x7e,0x7d,0x7c,0x7b},{}};
    b.types.push_back(signature);
    // Five typed arguments cross an actual compiled call before escaping.
    // The reference binary fixture uses the same throw/tuple syntax; payload
    // comparisons use raw native carrier bytes and never evaluate the sNaN.
    { func_body body; bytes(body.code,{0x20,0,0x20,1,0x20,2,0x20,3,0x20,4,0x08,0,0x0b});
      b.add_func(signature,std::move(body)); }
    { func_body body; bytes(body.code,{0x20,0,0x20,1,0x20,2,0x20,3,0x20,4,0x10,0,0x0b});
      b.add_func(signature,std::move(body)); }
    auto wasm=add_tag(b.build()); auto feature=make_wasm1p1_feature_parameter();
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(feature).disable_exceptions=false;
    auto prepared=prepare_runtime_from_wasm(wasm,u8"mixed-cross-exception",{},feature);
    uwvm2::validation::error::code_validation_error_impl error{}; optable::compile_option options;
    auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,error,&feature);
    UWVM2TEST_REQUIRE(error.err_code==uwvm2::validation::error::code_validation_error_code::ok);
    optable::call_func=call<Option>;
    std::array<byte_vec,5> fields{pack_i32(-7),pack_i64(-11),pack_i32(0x7fa00001),
        pack_i64(std::bit_cast<std::int64_t>(UINT64_C(0x8000000000000000))),byte_vec(16)};
    for(unsigned i{};i!=16;++i) { fields[4][i]=static_cast<std::byte>(i); }
    byte_vec arguments;
    for(auto const& field:fields) { arguments.insert(arguments.end(),field.begin(),field.end()); }
    bool caught_expected{};
    try {
        (void)interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(1),
            prepared.mod->local_defined_function_vec_storage.index_unchecked(1),arguments,nullptr,nullptr);
    } catch(uwvm2::runtime::exception::guest_exception const& caught) {
        using kind=uwvm2::runtime::exception::payload_kind;
        std::array const kinds{kind::i32,kind::i64,kind::f32,kind::f64,kind::v128};
        auto const actual=caught.instance()->fields(); UWVM2TEST_REQUIRE(actual.size()==fields.size());
        for(std::size_t i{};i!=fields.size();++i) {
            UWVM2TEST_REQUIRE(actual[i].kind()==kinds[i] && actual[i].bits().size()==fields[i].size());
            UWVM2TEST_REQUIRE(std::memcmp(actual[i].bits().data(),fields[i].data(),fields[i].size())==0);
        }
        caught_expected=true;
    }
    UWVM2TEST_REQUIRE(caught_expected); ++checks;
}
template<optable::uwvm_interpreter_translate_option_t Option> void check() {
    module_builder b; b.types.push_back({{0x7f},{}});
    // Function 0 escapes its own activation with an i32 tuple.
    { func_body body; bytes(body.code,{0x20,0,0x08,0,0x0b}); b.add_func({{0x7f},{}},std::move(body)); }
    // Function 1 keeps a caller prefix, discards an i64 temporary and receives a caught i32.
    // There is no ordinary executable edge to the block end after unreachable.
    { func_body body; bytes(body.code,{0x20,0,0x02,0x7f,0x1f,0x40,1,0,0,0,0x42,0x7f,0x20,0,0x10,0,0x1a,
        0x0b,0x00,0x0b,0x6a,0x41,7,0x6a,0x0b}); b.add_func({{0x7f},{0x7f}},std::move(body)); }
    // Function 2 catches all, discards the throw tuple, and preserves its caller prefix.
    { func_body body; bytes(body.code,{0x20,0,0x02,0x40,0x1f,0x40,1,2,0,0x20,0,0x10,0,
        0x0b,0x00,0x0b,0x41,7,0x6a,0x0b}); b.add_func({{0x7f},{0x7f}},std::move(body)); }
    auto wasm=add_tag(b.build()); auto feature=make_wasm1p1_feature_parameter();
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(feature).disable_exceptions=false;
    auto prepared=prepare_runtime_from_wasm(wasm,u8"cross-function-exception",{},feature);
    uwvm2::validation::error::code_validation_error_impl error{}; optable::compile_option options;
    compiled_module_t compiled;
    try { compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,error,&feature); }
    catch(...) { std::fprintf(stderr,"compile error code=%u\n",unsigned(error.err_code)); throw; }
    UWVM2TEST_REQUIRE(error.err_code==uwvm2::validation::error::code_validation_error_code::ok);
    optable::call_func=call<Option>;
    for(std::uint32_t input: {0u,1u,7u,0x7fffffffu,0x80000000u,0xfffffff9u,0xffffffffu}) {
        for(unsigned function=1;function!=3;++function) {
            auto result=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(function),
                prepared.mod->local_defined_function_vec_storage.index_unchecked(function),pack_i32(std::bit_cast<std::int32_t>(input)),nullptr,nullptr);
            UWVM2TEST_REQUIRE(result.results.size()==4);
            auto actual=std::bit_cast<std::uint32_t>(load_i32(result.results)); auto expected=function==1?input*2+7:input+7;
            if(actual!=expected) { std::fprintf(stderr,"tail=%u function=%u input=%u actual=%u expected=%u\n",unsigned(Option.is_tail_call),function,input,actual,expected); }
            UWVM2TEST_REQUIRE(actual==expected);
            ++checks;
        }
    }
    try {
        (void)interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(0),
            prepared.mod->local_defined_function_vec_storage.index_unchecked(0),pack_i32(123),nullptr,nullptr);
        UWVM2TEST_REQUIRE(false);
    } catch(uwvm2::runtime::exception::guest_exception const& caught) {
        UWVM2TEST_REQUIRE(caught.instance()->fields().size()==1);
        std::int32_t value{}; std::memcpy(&value,caught.instance()->fields()[0].bits().data(),4);
        UWVM2TEST_REQUIRE(value==123); ++checks;
    }
    std::printf("PASS compiler cross-function numeric EH, tail=%u\n",unsigned(Option.is_tail_call));
}
}
int main() {
    install_unexpected_traps();
    namespace mode=uwvm2::uwvm::runtime::runtime_mode;
    for(unsigned level=0;level!=4;++level) for(unsigned delay=0;delay!=2;++delay) {
        mode::global_runtime_uwvm_int_opcode_conbination_level=static_cast<mode::runtime_uwvm_int_opcode_conbination_level_t>(level);
        mode::runtime_uwvm_int_disable_delay_local=delay!=0;
        std::printf("START combine=%u disable_delay=%u\n",level,delay); std::fflush(stdout);
        check<k_test_byref_opt>(); check<make_tailcall_scalar4_merged_opt<1>()>(); check<make_tailcall_scalar4_merged_opt<2>()>();
        check_mixed_escape<k_test_byref_opt>(); check_mixed_escape<make_tailcall_scalar4_merged_opt<1>()>();
        check_mixed_escape<make_tailcall_scalar4_merged_opt<2>()>();
    }
    std::printf("PASS %u real compiler cross-function exception executions\n",checks);
}

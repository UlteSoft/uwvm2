#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"

#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace llvm_jit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;

    // Generated from wat/wasm2_multivalue_typed.wat. Keep the binary embedded so
    // this focused verifier test does not depend on a host wat2wasm installation.
    inline constexpr unsigned char wasm2_multivalue_typed_wasm[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x18u, 0x04u, 0x60u,
        0x02u, 0x7fu, 0x7fu, 0x02u, 0x7fu, 0x7fu, 0x60u, 0x03u, 0x7fu, 0x7fu, 0x7fu, 0x02u,
        0x7fu, 0x7fu, 0x60u, 0x00u, 0x02u, 0x70u, 0x6fu, 0x60u, 0x00u, 0x00u, 0x03u, 0x09u,
        0x08u, 0x00u, 0x02u, 0x00u, 0x00u, 0x01u, 0x01u, 0x00u, 0x03u, 0x04u, 0x04u, 0x01u,
        0x70u, 0x00u, 0x01u, 0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u,
        0x74u, 0x00u, 0x07u, 0x09u, 0x07u, 0x01u, 0x00u, 0x41u, 0x00u, 0x0bu, 0x01u, 0x00u,
        0x0au, 0x83u, 0x02u, 0x08u, 0x07u, 0x00u, 0x20u, 0x00u, 0x20u, 0x01u, 0x0fu, 0x0bu,
        0x07u, 0x00u, 0xd0u, 0x70u, 0xd0u, 0x6fu, 0x0fu, 0x0bu, 0x0bu, 0x00u, 0x20u, 0x00u,
        0x20u, 0x01u, 0x02u, 0x00u, 0x0cu, 0x00u, 0x0bu, 0x0bu, 0x17u, 0x01u, 0x01u, 0x7fu,
        0x20u, 0x00u, 0x20u, 0x01u, 0x03u, 0x00u, 0x20u, 0x02u, 0x41u, 0x01u, 0x6au, 0x22u,
        0x02u, 0x41u, 0x02u, 0x49u, 0x0du, 0x00u, 0x0bu, 0x0bu, 0x22u, 0x00u, 0x20u, 0x00u,
        0x20u, 0x01u, 0x20u, 0x02u, 0x04u, 0x00u, 0x21u, 0x01u, 0x21u, 0x00u, 0x20u, 0x00u,
        0x41u, 0x0au, 0x6au, 0x20u, 0x01u, 0x05u, 0x21u, 0x01u, 0x21u, 0x00u, 0x20u, 0x00u,
        0x41u, 0x14u, 0x6au, 0x20u, 0x01u, 0x0bu, 0x0bu, 0x12u, 0x00u, 0x20u, 0x00u, 0x20u,
        0x01u, 0x02u, 0x00u, 0x02u, 0x00u, 0x20u, 0x02u, 0x0eu, 0x01u, 0x00u, 0x01u, 0x0bu,
        0x0bu, 0x0bu, 0x0bu, 0x00u, 0x20u, 0x00u, 0x20u, 0x01u, 0x41u, 0x00u, 0x11u, 0x00u,
        0x00u, 0x0bu, 0x8au, 0x01u, 0x01u, 0x01u, 0x7fu, 0x41u, 0x03u, 0x41u, 0x04u, 0x10u,
        0x00u, 0x6au, 0x41u, 0x07u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x10u, 0x01u, 0xd1u,
        0x21u, 0x00u, 0xd1u, 0x20u, 0x00u, 0x71u, 0x45u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x41u,
        0x05u, 0x41u, 0x06u, 0x10u, 0x02u, 0x6au, 0x41u, 0x0bu, 0x47u, 0x04u, 0x40u, 0x00u,
        0x0bu, 0x41u, 0x07u, 0x41u, 0x08u, 0x10u, 0x03u, 0x6au, 0x41u, 0x0fu, 0x47u, 0x04u,
        0x40u, 0x00u, 0x0bu, 0x41u, 0x01u, 0x41u, 0x02u, 0x41u, 0x01u, 0x10u, 0x04u, 0x6au,
        0x41u, 0x0du, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x41u, 0x01u, 0x41u, 0x02u, 0x41u,
        0x00u, 0x10u, 0x04u, 0x6au, 0x41u, 0x17u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x41u,
        0x09u, 0x41u, 0x0au, 0x41u, 0x00u, 0x10u, 0x05u, 0x6au, 0x41u, 0x13u, 0x47u, 0x04u,
        0x40u, 0x00u, 0x0bu, 0x41u, 0x0bu, 0x41u, 0x0cu, 0x41u, 0x01u, 0x10u, 0x05u, 0x6au,
        0x41u, 0x17u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x41u, 0x0du, 0x41u, 0x0eu, 0x10u,
        0x06u, 0x6au, 0x41u, 0x1bu, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    enum local_function_index : ::std::uint_least32_t
    {
        return_pair_index,
        return_refs_index,
        block_br_index,
        loop_br_if_index,
        if_else_index,
        br_table_index,
        indirect_pair_index,
        start_index,
        local_function_count
    };

    [[nodiscard]] int fail(int line, ::std::string_view message)
    {
        ::std::cerr << "llvm_jit_multivalue_typed:" << line << ": " << message << '\n';
        return 1;
    }

#define LLVM_MV_REQUIRE(condition, message) \
    do                                      \
    {                                       \
        if(!(condition)) [[unlikely]]       \
        {                                   \
            return fail(__LINE__, message); \
        }                                   \
    } while(false)

    [[nodiscard]] strict::byte_vec make_fixture_bytes()
    {
        strict::byte_vec bytes{};
        bytes.reserve(::std::size(wasm2_multivalue_typed_wasm));
        for(auto const byte: wasm2_multivalue_typed_wasm) { bytes.push_back(static_cast<::std::byte>(byte)); }
        return bytes;
    }

    [[nodiscard]] strict::byte_vec make_invalid_legacy_select_t_arity_fixture(::std::uint32_t result_type_count)
    {
        strict::module_builder module{};
        strict::func_type type{{}, {strict::k_val_i32}};
        strict::func_body body{};
        auto& code{body.code};
        for(auto const value: {1, 2, 0})
        {
            strict::append_u8(code, strict::u8(strict::wasm_op::i32_const));
            strict::append_i32_leb(code, value);
        }
        strict::append_u8(code, strict::u8(strict::wasm1p1_op::select_t));
        strict::append_u32_leb(code, result_type_count);
        for(::std::uint32_t index{}; index != result_type_count; ++index) { strict::append_u8(code, strict::k_val_i32); }
        strict::append_u8(code, strict::u8(strict::wasm_op::end));
        static_cast<void>(module.add_func(::std::move(type), ::std::move(body)));
        return module.build();
    }

    [[nodiscard]] int inspect_rejected_legacy_select_t_arities()
    {
        for(auto const result_type_count: {0u, 2u})
        {
            auto wasm{make_invalid_legacy_select_t_arity_fixture(result_type_count)};
            auto features{strict::make_wasm1p1_feature_parameter()};
            auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"llvm_jit_legacy_select_t_arity", {}, features)};
            LLVM_MV_REQUIRE(prepared.mod != nullptr, "legacy typed-select fixture preparation failed");

            ::uwvm2::validation::error::code_validation_error_impl error{};
            llvm_jit::compile_option options{};
            options.validator_feature_parameter = ::std::addressof(features);
            try
            {
                static_cast<void>(llvm_jit::compile_all_from_uwvm(*prepared.mod, options, error, 0uz));
            }
            catch(::fast_io::error const&)
            {}
            catch(...)
            {
                return fail(__LINE__, "unexpected exception while rejecting legacy typed-select arity");
            }

            LLVM_MV_REQUIRE(error.err_code == ::uwvm2::validation::error::code_validation_error_code::invalid_const_immediate,
                            "LLVM full validation accepted a legacy typed-select vector whose arity is not one");
        }
        return 0;
    }

    [[nodiscard]] ::std::string_view as_string_view(::uwvm2::utils::container::u8string const& text) noexcept
    {
        return {reinterpret_cast<char const*>(text.data()), text.size()};
    }

    [[nodiscard]] ::std::string_view as_string_view(::llvm::StringRef text) noexcept
    { return {text.data(), text.size()}; }

    [[nodiscard]] bool starts_with(::std::string_view text, ::std::string_view prefix) noexcept
    { return text.size() >= prefix.size() && text.substr(0uz, prefix.size()) == prefix; }

    [[nodiscard]] ::llvm::Function* get_typed_function(::llvm::Module& module,
                                                       strict::runtime_module_t const& runtime_module,
                                                       ::std::uint_least32_t function_index) noexcept
    {
        auto const name{llvm_jit::details::get_llvm_wasm_function_name(runtime_module, function_index)};
        auto const name_view{as_string_view(name)};
        return module.getFunction(::llvm::StringRef{name_view.data(), name_view.size()});
    }

    [[nodiscard]] ::llvm::Function* get_raw_function(::llvm::Module& module,
                                                     strict::runtime_module_t const& runtime_module,
                                                     ::std::uint_least32_t function_index) noexcept
    {
        auto const name{llvm_jit::details::get_llvm_wasm_raw_function_name(runtime_module, function_index)};
        auto const name_view{as_string_view(name)};
        return module.getFunction(::llvm::StringRef{name_view.data(), name_view.size()});
    }

    [[nodiscard]] bool is_i32_pair_type(::llvm::Type* type) noexcept
    {
        auto const struct_type{::llvm::dyn_cast_or_null<::llvm::StructType>(type)};
        return struct_type != nullptr && struct_type->getNumElements() == 2u && struct_type->getElementType(0u)->isIntegerTy(32u) &&
               struct_type->getElementType(1u)->isIntegerTy(32u);
    }

    [[nodiscard]] bool is_reference_pair_type(::llvm::Type* type) noexcept
    {
        auto const struct_type{::llvm::dyn_cast_or_null<::llvm::StructType>(type)};
        constexpr auto reference_bit_width{static_cast<unsigned>(sizeof(::uwvm2::object::global::wasm_global_ref_t) * CHAR_BIT)};
        return struct_type != nullptr && struct_type->getNumElements() == 2u && struct_type->getElementType(0u)->isIntegerTy(reference_bit_width) &&
               struct_type->getElementType(1u)->isIntegerTy(reference_bit_width);
    }

    [[nodiscard]] bool has_direct_call(::llvm::Function const& caller, ::llvm::Function const& callee) noexcept
    {
        for(auto const& block: caller)
        {
            for(auto const& instruction: block)
            {
                auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                if(call != nullptr && call->getCalledFunction() == ::std::addressof(callee)) { return true; }
            }
        }
        return false;
    }

    [[nodiscard]] bool has_typed_indirect_pair_call(::llvm::Function const& function) noexcept
    {
        for(auto const& block: function)
        {
            for(auto const& instruction: block)
            {
                auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                if(call != nullptr && call->getCalledFunction() == nullptr && is_i32_pair_type(call->getFunctionType()->getReturnType())) { return true; }
            }
        }
        return false;
    }

    [[nodiscard]] bool has_i32_tuple_return(::llvm::Function const& function) noexcept
    {
        for(auto const& block: function)
        {
            for(auto const& instruction: block)
            {
                auto const return_instruction{::llvm::dyn_cast<::llvm::ReturnInst>(::std::addressof(instruction))};
                if(return_instruction != nullptr && return_instruction->getReturnValue() != nullptr &&
                   is_i32_pair_type(return_instruction->getReturnValue()->getType()))
                {
                    return true;
                }
            }
        }
        return false;
    }

    [[nodiscard]] bool has_reference_tuple_return(::llvm::Function const& function) noexcept
    {
        for(auto const& block: function)
        {
            for(auto const& instruction: block)
            {
                auto const return_instruction{::llvm::dyn_cast<::llvm::ReturnInst>(::std::addressof(instruction))};
                if(return_instruction != nullptr && return_instruction->getReturnValue() != nullptr &&
                   is_reference_pair_type(return_instruction->getReturnValue()->getType()))
                {
                    return true;
                }
            }
        }
        return false;
    }

    [[nodiscard]] ::std::size_t count_named_phis(::llvm::Function const& function, ::std::string_view name_prefix) noexcept
    {
        ::std::size_t count{};
        for(auto const& block: function)
        {
            for(auto const& instruction: block)
            {
                if(::llvm::isa<::llvm::PHINode>(instruction) && starts_with(as_string_view(instruction.getName()), name_prefix)) { ++count; }
            }
        }
        return count;
    }

    [[nodiscard]] bool has_named_block(::llvm::Function const& function, ::std::string_view name_prefix) noexcept
    {
        for(auto const& block: function)
        {
            if(starts_with(as_string_view(block.getName()), name_prefix)) { return true; }
        }
        return false;
    }

    [[nodiscard]] bool has_conditional_branch_to(::llvm::Function const& function, ::std::string_view block_name_prefix) noexcept
    {
        for(auto const& block: function)
        {
            auto const branch{::llvm::dyn_cast_or_null<::llvm::BranchInst>(block.getTerminator())};
            if(branch == nullptr || !branch->isConditional()) { continue; }
            for(unsigned successor_index{}; successor_index != branch->getNumSuccessors(); ++successor_index)
            {
                if(starts_with(as_string_view(branch->getSuccessor(successor_index)->getName()), block_name_prefix)) { return true; }
            }
        }
        return false;
    }

    [[nodiscard]] bool has_switch(::llvm::Function const& function) noexcept
    {
        for(auto const& block: function)
        {
            if(::llvm::dyn_cast_or_null<::llvm::SwitchInst>(block.getTerminator()) != nullptr) { return true; }
        }
        return false;
    }

    [[nodiscard]] int inspect_typed_ir(strict::byte_vec const& wasm_bytes)
    {
        auto feature_parameter{strict::make_wasm2_feature_parameter()};
        auto prepared{strict::prepare_runtime_from_wasm(wasm_bytes, u8"llvm_jit_multivalue_typed", {}, feature_parameter)};
        LLVM_MV_REQUIRE(prepared.mod != nullptr, "runtime module preparation failed");

        ::uwvm2::validation::error::code_validation_error_impl error{};
        llvm_jit::compile_option options{};
        options.validator_feature_parameter = ::std::addressof(feature_parameter);
        options.verify_llvm_jit_ir = true;
        options.emit_call_stack_frames = false;
        auto compiled{llvm_jit::compile_all_from_uwvm(*prepared.mod, options, error, 0uz)};

        LLVM_MV_REQUIRE(error.err_code == ::uwvm2::validation::error::code_validation_error_code::ok,
                        "LLVM internal validator rejected the multi-value fixture");
        LLVM_MV_REQUIRE(compiled.local_funcs.size() == local_function_count, "unexpected local function count");
        LLVM_MV_REQUIRE(compiled.llvm_jit_module.emitted, "LLVM module finalization did not succeed");
        LLVM_MV_REQUIRE(compiled.llvm_jit_module.llvm_module != nullptr, "LLVM module was not emitted");

        auto& module{*compiled.llvm_jit_module.llvm_module};
        ::std::string verifier_diagnostic{};
        ::llvm::raw_string_ostream verifier_stream{verifier_diagnostic};
        if(::llvm::verifyModule(module, ::std::addressof(verifier_stream)))
        {
            verifier_stream.flush();
            ::std::cerr << verifier_diagnostic;
            return fail(__LINE__, "LLVM IR verifier rejected the emitted module");
        }
        ::std::array<::llvm::Function*, local_function_count> typed_functions{};
        for(::std::uint_least32_t function_index{}; function_index != local_function_count; ++function_index)
        {
            auto typed_function{get_typed_function(module, *prepared.mod, function_index)};
            auto raw_function{get_raw_function(module, *prepared.mod, function_index)};
            LLVM_MV_REQUIRE(typed_function != nullptr && !typed_function->empty(), "missing typed LLVM function body");
            LLVM_MV_REQUIRE(raw_function != nullptr && !raw_function->empty(), "missing raw ABI wrapper");
            LLVM_MV_REQUIRE(has_direct_call(*raw_function, *typed_function), "raw ABI entry does not dispatch to the typed body");
            typed_functions[function_index] = typed_function;
        }

        auto const return_pair{typed_functions[return_pair_index]};
        auto const return_refs{typed_functions[return_refs_index]};
        auto const block_br{typed_functions[block_br_index]};
        auto const loop_br_if{typed_functions[loop_br_if_index]};
        auto const if_else{typed_functions[if_else_index]};
        auto const br_table{typed_functions[br_table_index]};
        auto const indirect_pair{typed_functions[indirect_pair_index]};
        auto const start{typed_functions[start_index]};

        LLVM_MV_REQUIRE(is_i32_pair_type(return_pair->getReturnType()), "typed multi-result ABI is not an {i32, i32} return");
        LLVM_MV_REQUIRE(has_i32_tuple_return(*return_pair), "explicit return did not feed a typed tuple return");
        LLVM_MV_REQUIRE(count_named_phis(*return_pair, "return.phi") == 2uz, "return tuple does not have two PHIs");
        LLVM_MV_REQUIRE(has_direct_call(*start, *return_pair), "multi-result direct call was not emitted as a typed call");

        LLVM_MV_REQUIRE(is_reference_pair_type(return_refs->getReturnType()),
                        "typed reference multi-result ABI is not a {funcref, externref} carrier struct");
        LLVM_MV_REQUIRE(has_reference_tuple_return(*return_refs), "reference tuple did not reach a typed return");
        LLVM_MV_REQUIRE(count_named_phis(*return_refs, "return.phi") == 2uz, "reference tuple does not have two return PHIs");
        LLVM_MV_REQUIRE(has_direct_call(*start, *return_refs), "reference multi-result direct call was not emitted as a typed call");

        LLVM_MV_REQUIRE(has_named_block(*block_br, "block.end"), "typeidx block body was not lowered");
        LLVM_MV_REQUIRE(count_named_phis(*block_br, "block.result") == 2uz, "typeidx block/br tuple does not have two result PHIs");

        LLVM_MV_REQUIRE(has_named_block(*loop_br_if, "loop.body"), "typeidx loop body was not lowered");
        LLVM_MV_REQUIRE(count_named_phis(*loop_br_if, "loop.param") == 2uz, "loop parameters do not have two PHIs");
        LLVM_MV_REQUIRE(count_named_phis(*loop_br_if, "loop.result") == 2uz, "loop results do not have two PHIs");
        LLVM_MV_REQUIRE(has_conditional_branch_to(*loop_br_if, "loop.body"), "br_if does not branch to the typed loop label");

        LLVM_MV_REQUIRE(has_named_block(*if_else, "if.then") && has_named_block(*if_else, "if.else") && has_named_block(*if_else, "if.end"),
                        "typeidx if/else CFG was not emitted");
        LLVM_MV_REQUIRE(count_named_phis(*if_else, "if.result") == 2uz, "if/else tuple does not have two result PHIs");

        LLVM_MV_REQUIRE(has_switch(*br_table), "br_table was not lowered to an LLVM switch");
        LLVM_MV_REQUIRE(count_named_phis(*br_table, "block.result") >= 4uz, "br_table targets do not carry both tuple fields");

        LLVM_MV_REQUIRE(is_i32_pair_type(indirect_pair->getReturnType()), "call_indirect caller lost its multi-result ABI");
        LLVM_MV_REQUIRE(has_typed_indirect_pair_call(*indirect_pair), "call_indirect has no typed multi-result call path");
        return 0;
    }

    [[nodiscard]] ::std::filesystem::path find_uwvm_binary(::std::filesystem::path directory)
    {
        for(;;)
        {
            auto const candidate{directory / "uwvm"};
            if(::std::filesystem::exists(candidate)) { return candidate; }
#ifdef _WIN32
            auto const windows_candidate{directory / "uwvm.exe"};
            if(::std::filesystem::exists(windows_candidate)) { return windows_candidate; }
#endif
            if(directory == directory.root_path()) { return {}; }
            directory = directory.parent_path();
        }
    }

    [[nodiscard]] ::std::string quote_argument(::std::filesystem::path const& path)
    { return ::std::string{"\""} + path.string() + "\""; }

    [[nodiscard]] int run_semantics_once(strict::byte_vec const& wasm_bytes, ::std::filesystem::path const& executable)
    {
        auto const artifact_directory{executable.parent_path() / "test-artifacts" / "0014.llvm_jit"};
        ::std::error_code ec{};
        ::std::filesystem::create_directories(artifact_directory, ec);
        LLVM_MV_REQUIRE(!ec, "failed to create the LLVM JIT artifact directory");

        auto const wasm_path{artifact_directory / "wasm2_multivalue_typed.wasm"};
        ::std::ofstream output(wasm_path, ::std::ios::binary | ::std::ios::trunc);
        LLVM_MV_REQUIRE(output.good(), "failed to open the multi-value wasm fixture");
        output.write(reinterpret_cast<char const*>(wasm_bytes.data()), static_cast<::std::streamsize>(wasm_bytes.size()));
        output.close();
        LLVM_MV_REQUIRE(output.good(), "failed to write the multi-value wasm fixture");

        auto const uwvm_path{find_uwvm_binary(executable.parent_path())};
        LLVM_MV_REQUIRE(!uwvm_path.empty(), "failed to locate uwvm next to the test executable");
        auto const command{quote_argument(uwvm_path) +
                           " -Rcm full -Rcc jit -Rllvm-cache-path disable --wasm-feature-wasm2 --run " + quote_argument(wasm_path)};
        ::std::cout << "[llvm_jit_multivalue_typed] " << command << '\n';
#ifdef _WIN32
        auto const wrapped{::std::string{"cmd.exe /S /C \""} + command + "\""};
        LLVM_MV_REQUIRE(::std::system(wrapped.c_str()) == 0, "full LLVM-JIT execution failed");
#else
        LLVM_MV_REQUIRE(::std::system(command.c_str()) == 0, "full LLVM-JIT execution failed");
#endif
        return 0;
    }
}  // namespace

int main(int argc, char** argv)
{
    if(argc <= 0 || argv == nullptr || argv[0] == nullptr) { return fail(__LINE__, "missing argv[0]"); }

    auto const wasm_bytes{make_fixture_bytes()};
    if(auto const result{inspect_rejected_legacy_select_t_arities()}; result != 0) { return result; }
    if(auto const result{inspect_typed_ir(wasm_bytes)}; result != 0) { return result; }
    return run_semantics_once(wasm_bytes, ::std::filesystem::absolute(argv[0]));
}

// Actual parsed/initialized/validated Core 3 Wasm-to-LLVM lowering. The linked
// SDK optimizes and executes it. A test-owned debug callback collects while
// the one native guest thread is synchronously stopped, using real static and
// generated frame roots. This is not automatic/multithreaded VM collection.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include <uwvm_int_translate_strict_common.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/gc/impl.h>
#include <uwvm2/runtime/exception/roots.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/lib/uwvm_runtime_native_exception_host.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <uwvm2/utils/macro/pop_macros.h>
#include <uwvm2/uwvm/runtime/storage/gc_static_roots.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <type_traits>
#include <span>

#if !defined(__cpp_exceptions) && !defined(__EXCEPTIONS)
#error This real native EH fixture requires the matched product C++ EH configuration.
#endif

namespace strict = ::uwvm2test::uwvm_int_strict;
namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
namespace details = compiler::details;
namespace frames = ::uwvm2::runtime::gc;
namespace storage = ::uwvm2::uwvm::runtime::storage;
namespace container = ::uwvm2::utils::container;
namespace eh = ::uwvm2::runtime::exception;
namespace types = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace bridges = ::uwvm2::runtime::lib::details;

namespace
{
    ::std::size_t checks{}, collections{}, reclaimed_total{}, max_depth{}, registry_rejections{};
    ::std::size_t fresh_throws{}, matches{}, tuple_copies{}, numeric_copies{}, ref_issues{}, ref_throws{};
    ::std::size_t wrong_handler_depth{}, outer_cleanup_witnesses{}, native_escapes{}, foreign_escapes{}, foreign_destructors{};
    ::std::vector<::std::shared_ptr<storage::gc_object_store>> cohort{};
    ::std::vector<storage::wasm_module_storage_t const*> modules{};
    storage::wasm_module_storage_t const* active_module{};
    bool force_collection{}, expect_empty_activation{}, exn_registry_live{}, inject_foreign{};
    unsigned current_case{};
    // A borrow only after make_exn_reference has registered an owning module
    // token. It is cleared before that module/store is retired; never a root.
    eh::value const* issued_identity{};

    void require(bool condition, char const* label) noexcept
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL Core 3 GC+EH LLVM roots: ", ::fast_io::mnp::os_c_str(label));
            ::fast_io::fast_terminate();
        }
    }

    struct root_buffer
    {
        ::std::array<storage::gc_reference, 256uz> values{};
        ::std::size_t size{};
        bool operator()(storage::gc_reference reference) noexcept
        {
            if(size == values.size()) { return false; }
            // [published 0,size)[one free complete carrier] ... end
            // [safe              ] Copy before advancing the bounded index.
            values[size++] = reference;
            return true;
        }
    };

    // This explicit, synchronous, SINGLE native mutator is stopped at an actual
    // generated instruction point or inside a known test adapter. No host/guest
    // callback runs during enumeration/sweep, and no other runtime exists.
    // Strong cohort pins retain all stores. A local payload_field.root is empty
    // by design and is NEVER treated as an independent liveness registration.
    [[nodiscard]] ::std::size_t collect_now(eh::value const* explicit_activation = nullptr) noexcept
    {
        if(!force_collection) { return 0uz; }
        root_buffer roots{};
        auto const active{frames::visit_quiescent_frame_roots(frames::current_root_frames(), roots)};
        require(active.status == frames::frame_root_status::ok, "complete genuine generated root chain validates");
        require(expect_empty_activation ? active.frames == 0uz : active.frames != 0uz || explicit_activation != nullptr,
            "reference-free frames elide records; escaped native owner is explicitly enumerated");
        max_depth = (::std::max)(max_depth, active.frames);
        auto const static_{storage::visit_quiescent_cohort_static_roots({modules.data(), modules.size()}, roots)};
        require(static_.status == storage::gc_static_root_status::ok, "real initialized module static roots validate");
        if(explicit_activation != nullptr)
        {
            // [actual strongly owned immutable native exception activation]
            // [safe ] Its C++ owner remains live until this synchronous call
            // returns. It is enumerated explicitly, never inferred from bits.
            auto const extra{eh::visit_immutable_exception_wasm_roots(*explicit_activation, roots)};
            require(extra.status == eh::payload_root_status::ok, "actual immutable exception payload roots validate");
        }
        ::std::size_t reclaimed{};
        auto const status{storage::gc_object_store::collect_exclusive_aggregate_domain(
            cohort.data(), cohort.size(), roots.values.data(), roots.size, reclaimed)};
        if(exn_registry_live)
        {
            require(status == storage::gc_object_status::invalid_reference && reclaimed == 0uz,
                "live exn registry is a real aggregate-collector rejection, not collection success");
            ++registry_rejections;
        }
        else
        {
            require(status == storage::gc_object_status::ok, "actual closed aggregate collector accepts exact roots");
            ++collections;
            reclaimed_total += reclaimed;
        }
        return active.frames;
    }

    // Exact five-uintptr_t ABI of the generated product instruction-point
    // helper. Engine-local symbol mapping replaces just this callback; it
    // does not activate the product debugger or a VM collection handshake.
    void collect_at_debug_point(::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t,
        ::std::uintptr_t, ::std::uintptr_t) noexcept { static_cast<void>(collect_now()); }
    static_assert(::std::is_same_v<decltype(&collect_at_debug_point),
        decltype(&bridges::llvm_jit_debug_safe_point_abi_bridge)>);

    [[nodiscard]] storage::local_defined_tag_storage_t const& actual_tag(
        ::std::uintptr_t module, ::std::uintptr_t index) noexcept
    {
        require(module == 0u && active_module != nullptr && active_module->gc_store &&
            active_module->gc_store->valid() && active_module->imported_tag_vec_storage.empty() &&
            active_module->local_defined_tag_vec_storage.size() == 3uz && index < 3u,
            "test-owned admission pins the actual initialized zero-import module and tag index");
        // [actual initialized tag records: 3] end
        // [safe                           ] index < 3; this is a native record
        // borrow, not an address supplied by guest linear memory.
        auto const& tag{active_module->local_defined_tag_vec_storage.index_unchecked(index)};
        require(tag.function_type_ptr != nullptr && tag.exception_identity &&
            tag.function_type_ptr->result.begin == tag.function_type_ptr->result.end,
            "real instantiated tag owns a validated parameter-only signature");
        auto const& parameters{tag.function_type_ptr->parameter};
        // [validated pinned signature endpoints] Both refer to its one owned
        // parameter allocation; form one-past only after proving non-null.
        // [safe ] No untrusted parameter buffer enters this native-only test.
        require(parameters.begin != nullptr && parameters.end == parameters.begin + 1uz,
            "pinned actual fixture tag signature contains exactly one parameter");
        auto const encoding{static_cast<::std::uint_least8_t>(*parameters.begin)};
        require(index == 2u ? encoding == 0x7fu : encoding == 0x69u,
            "real projected tag signatures match the selected numeric/typed-reference codec");
        return tag;
    }

    [[nodiscard]] eh::guest_exception make_guest_probe()
    {
        auto const& tag{actual_tag(0u, 2u)};
        ::std::uint32_t key{17u};
        auto field{eh::payload_field::numeric(eh::payload_kind::i32,
            ::std::as_bytes(::std::span{::std::addressof(key), 1uz}))};
        require(field.has_value(), "actual native ABI probe owns initialized numeric bits");
        ::std::array fields{::std::move(*field)};
        return eh::guest_exception{eh::value::make(tag.exception_identity, fields)};
    }

    [[nodiscard]] eh::guest_exception const& actual_caught(::std::uintptr_t object) noexcept
    {
        require(object != 0u, "actual typed Itanium catch supplies its guest object");
        // [actual __cxa_begin_catch result of the guest_exception type selector]
        // [safe ] The real personality/landingpad proved the dynamic type and
        // the C++ catch keeps it live. This test is not a transport pointer API.
        return *reinterpret_cast<eh::guest_exception const*>(object);
    }
    [[nodiscard]] storage::gc_reference payload_reference(eh::value const& value) noexcept
    {
        require(value.fields().size() == 1uz && value.fields()[0].kind() == eh::payload_kind::wasm_reference &&
            value.fields()[0].bits().size() == sizeof(storage::gc_reference), "actual exception has one full typed carrier");
        storage::gc_reference reference{};
        // [one owned initialized immutable full carrier][aligned local]
        // [safe ] Copy the complete kind+payload; never a pointer-width guess.
        ::std::memcpy(::std::addressof(reference), value.fields()[0].bits().data(), sizeof(reference));
        return reference;
    }
    void require_box(storage::gc_reference reference) noexcept
    {
        require(active_module->gc_store->reference_type_matches(reference,
            {types::value_kind::reference, {0}, false}), "actual GC codec proves live local non-null defined box type");
        require(active_module->gc_store->retain_gc_reference(reference) == storage::gc_object_status::ok,
            "actual GC codec admits the complete reference carrier");
    }

    struct foreign_probe final {};
    struct foreign_cleanup_witness final
    {
        ~foreign_cleanup_witness() noexcept
        {
            require(::std::uncaught_exceptions() != 0, "real native destructor runs during genuine foreign C++ unwinding");
            ++foreign_destructors;
        }
    };
    [[noreturn]] void test_throw_tuple(::std::uintptr_t module, ::std::uintptr_t tag_index,
        ::std::uintptr_t address, ::std::size_t bytes)
    {
        auto const& tag{actual_tag(module, tag_index)};
        require(tag_index < 2u && address != 0u && bytes == sizeof(storage::gc_reference),
            "tuple bridge borrows exactly one real generated private carrier");
        storage::gc_reference reference{};
        // [genuine generated throw alloca: sizeof(reference)][aligned local]
        // [safe ] The exact declaration/type/bytes and source-pinned lowering
        // provide this extent. No guessed guest/native address is followed.
        ::std::memcpy(::std::addressof(reference), reinterpret_cast<void const*>(address), sizeof(reference));
        require_box(reference);
        auto field{eh::payload_field::wasm_reference(
            ::std::as_bytes(::std::span{::std::addressof(reference), 1uz}), active_module->gc_store->root_reference(reference))};
        require(field.has_value(), "actual immutable tuple factory accepts the full validated carrier");
        ::std::vector<eh::payload_field> fields{};
        fields.push_back(::std::move(*field));
        // Retire all mutable aliases before transferring exclusive field storage.
        auto value{eh::value::make_owned(tag.exception_identity, ::std::move(fields))};
        require(static_cast<bool>(value), "genuine fresh immutable guest exception instance published");
        ++fresh_throws;
        static_cast<void>(collect_now(value.get())); // Actual activation owns the root across collection.
        if(inject_foreign)
        {
            foreign_cleanup_witness cleanup{};
            throw foreign_probe{}; // Genuine C++ RAII unwinding, never a guest catch_all.
        }
        eh::throw_value(::std::move(value)); // Creates a real C++ ABI exception; no SP/FP jumps.
    }
    [[noreturn]] void test_throw_numeric(::std::uintptr_t module, ::std::uintptr_t tag_index,
        ::std::uintptr_t address, ::std::size_t bytes)
    {
        auto const& tag{actual_tag(module, tag_index)};
        require(tag_index == 2u && address != 0u && bytes == sizeof(::std::uint32_t), "actual numeric throw tuple extent");
        ::std::uint32_t bits{};
        // [real generated numeric tuple: four initialized bytes][aligned local]
        // [safe ] Copy bits without evaluating/reinterpreting a floating value.
        ::std::memcpy(::std::addressof(bits), reinterpret_cast<void const*>(address), sizeof(bits));
        auto field{eh::payload_field::numeric(eh::payload_kind::i32, ::std::as_bytes(::std::span{::std::addressof(bits), 1uz}))};
        require(field.has_value(), "genuine numeric field codec");
        ::std::vector<eh::payload_field> fields{};
        fields.push_back(::std::move(*field));
        auto value{eh::value::make_owned(tag.exception_identity, ::std::move(fields))};
        ++fresh_throws;
        static_cast<void>(collect_now(value.get()));
        eh::throw_value(::std::move(value));
    }
    ::std::uintptr_t test_matches(::std::uintptr_t object, ::std::uintptr_t module, ::std::uintptr_t index) noexcept
    {
        auto const& tag{actual_tag(module, index)};
        auto const& activation{actual_caught(object)};
        ++matches;
        auto const depth{collect_now(activation.instance().get())};
        auto const matched{activation.instance()->tag_identity() == tag.exception_identity.get()};
        if(force_collection && !matched && index == 1u) { wrong_handler_depth = depth; }
        if(force_collection && matched && index == 0u && current_case == 3u)
        {
            require(wrong_handler_depth > depth, "unmatched inner native cleanup retires its actual generated record first");
            ++outer_cleanup_witnesses;
        }
        return matched ? 1u : 0u;
    }
    void test_copy_tuple(::std::uintptr_t object, ::std::uintptr_t module, ::std::uintptr_t index,
        ::std::uintptr_t destination, ::std::size_t bytes) noexcept
    {
        auto const& tag{actual_tag(module, index)};
        auto const& activation{actual_caught(object)};
        require(index < 2u && destination != 0u && bytes == sizeof(storage::gc_reference) &&
            activation.instance()->tag_identity() == tag.exception_identity.get(), "actual matched tuple/capacity");
        auto const reference{payload_reference(*activation.instance())};
        require_box(reference);
        static_cast<void>(collect_now(activation.instance().get()));
        // [validated immutable full carrier][genuine generated destination alloca]
        // [safe ] The selected real emitter declaration writes exactly bytes;
        // the actual C++ catch owns the source through this synchronous copy.
        ::std::memcpy(reinterpret_cast<void*>(destination), ::std::addressof(reference), sizeof(reference));
        ++tuple_copies;
    }
    void test_copy_numeric(::std::uintptr_t object, ::std::uintptr_t module, ::std::uintptr_t index,
        ::std::uintptr_t destination, ::std::size_t bytes) noexcept
    {
        auto const& tag{actual_tag(module, index)};
        auto const& activation{actual_caught(object)};
        require(index == 2u && destination != 0u && bytes == sizeof(::std::uint32_t) &&
            activation.instance()->tag_identity() == tag.exception_identity.get() &&
            activation.instance()->fields().size() == 1uz && activation.instance()->fields()[0].kind() == eh::payload_kind::i32,
            "matched genuine numeric instance and destination extent");
        static_cast<void>(collect_now(activation.instance().get()));
        auto const bits{activation.instance()->fields()[0].bits()};
        require(bits.size() == bytes, "complete owned numeric bytes");
        // [immutable four bytes][generated destination four bytes] Both live.
        // [safe ] The checked synchronous extent neither widens nor retains a borrow.
        ::std::memcpy(reinterpret_cast<void*>(destination), bits.data(), bytes);
        ++numeric_copies;
    }
    void test_make_ref(::std::uintptr_t object, ::std::uintptr_t module, ::std::uintptr_t destination) noexcept
    {
        static_cast<void>(actual_tag(module, 0u));
        auto const& activation{actual_caught(object)};
        require(destination != 0u && issued_identity == nullptr, "first real exnref publication into one generated carrier");
        storage::gc_reference reference{};
        require(active_module->gc_store->make_exn_reference(activation.instance(), reference) == storage::gc_object_status::ok,
            "actual module exn registry publishes an owning token");
        // [module-owned immutable value] The registry owns this borrow until
        // actual module teardown. Clear the observer before releasing its pin.
        issued_identity = activation.instance().get();
        exn_registry_live = true;
        // [real registered full exn carrier][generated complete destination slot]
        // [safe ] Publish kind+identity together; never an unwind-header pointer.
        ::std::memcpy(reinterpret_cast<void*>(destination), ::std::addressof(reference), sizeof(reference));
        ++ref_issues;
        static_cast<void>(collect_now(activation.instance().get()));
    }
    [[noreturn]] void test_throw_ref(::std::uintptr_t module, ::std::uintptr_t address)
    {
        static_cast<void>(actual_tag(module, 0u));
        require(address != 0u && exn_registry_live && issued_identity != nullptr, "throw_ref borrows an admitted generated exn carrier");
        storage::gc_reference reference{};
        // [genuine generated private ref slot: one full carrier][aligned local]
        // [safe ] Copy the complete representation before actual registry lookup.
        ::std::memcpy(::std::addressof(reference), reinterpret_cast<void const*>(address), sizeof(reference));
        auto value{storage::gc_object_store::lookup_exn_reference(reference)};
        require(value && value.get() == issued_identity, "throw_ref preserves the exact immutable catch_ref instance identity");
        ++ref_throws;
        static_cast<void>(collect_now(value.get()));
        eh::throw_value(::std::move(value));
    }
    static_assert(::std::is_same_v<decltype(&test_throw_tuple), decltype(&bridges::llvm_jit_throw_tuple_abi_bridge)>);
    static_assert(::std::is_same_v<decltype(&test_throw_numeric), decltype(&bridges::llvm_jit_throw_numeric_abi_bridge)>);
    static_assert(::std::is_same_v<decltype(&test_matches), decltype(&bridges::llvm_jit_exception_matches_tag_abi_bridge)>);
    static_assert(::std::is_same_v<decltype(&test_copy_tuple), decltype(&bridges::llvm_jit_exception_copy_tuple_abi_bridge)>);
    static_assert(::std::is_same_v<decltype(&test_copy_numeric), decltype(&bridges::llvm_jit_exception_copy_numeric_payload_abi_bridge)>);
    static_assert(::std::is_same_v<decltype(&test_make_ref), decltype(&bridges::llvm_jit_exception_make_ref_abi_bridge)>);
    static_assert(::std::is_same_v<decltype(&test_throw_ref), decltype(&bridges::llvm_jit_throw_ref_abi_bridge)>);

    [[nodiscard]] ::std::string name(container::u8string const& value)
    {
        // [complete bounded UTF-8 name] end
        // [safe                     ] copy its bytes; retain no borrowed view.
        return {reinterpret_cast<char const*>(value.data()), value.size()};
    }
    template<auto Bridge>
    [[nodiscard]] bool bridge(::llvm::Function const& function)
    {
        auto const prefix{details::get_llvm_runtime_bridge_function_symbol_name<Bridge>()};
        return function.getName().starts_with(::llvm::StringRef{
            reinterpret_cast<char const*>(prefix.data()), prefix.size()});
    }
    // This requires the actual x86-64 Linux external-declaration lowering.
    // RuntimeDyld's real MCJIT resolver first consults the per-engine map;
    // no process-global symbol replacement, SDK mocks or hand-written IR.
    template<auto Actual, auto Adapter, ::std::size_t Count, bool ReturnsWord = false>
    void map_exact_bridge(::llvm::ExecutionEngine& engine, ::llvm::Module& module)
    {
        static_assert(::std::is_same_v<decltype(Actual), decltype(Adapter)>);
        auto const intptr{::llvm::IntegerType::get(module.getContext(),
            static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
        ::std::array<::llvm::Type*, Count> arguments{};
        arguments.fill(intptr);
        auto const result{ReturnsWord ? static_cast<::llvm::Type*>(intptr) : ::llvm::Type::getVoidTy(module.getContext())};
        auto const type{::llvm::FunctionType::get(result, arguments, false)};
        auto const full_name{name(details::get_llvm_runtime_bridge_function_symbol_name<Actual>(type))};
        auto const declaration{module.getFunction(full_name)};
        require(declaration != nullptr && declaration->isDeclaration() && declaration->getFunctionType() == type &&
            declaration->getCallingConv() == ::llvm::CallingConv::C,
            "exact current emitter helper declaration and C calling convention are present");
        require(engine.getPointerToGlobalIfAvailable(declaration) == nullptr,
            "engine-local helper mapping has no conflicting existing owner");
        auto const address{reinterpret_cast<::std::uintptr_t>(Adapter)};
        engine.addGlobalMapping(declaration, reinterpret_cast<void*>(address));
        require(engine.getPointerToGlobalIfAvailable(declaration) == reinterpret_cast<void*>(address),
            "actual MCJIT mapping publishes the exact typed test-owned function");
        ::fast_io::io::println("EH test-owned engine mapping ", ::fast_io::mnp::os_c_str(full_name.c_str()),
            " args=", Count, " returns_word=", ReturnsWord ? 1 : 0);
    }

    void verify_fixed_gc_bridge_identity(::llvm::Module& module, bool enabled, bool optimized)
    {
        auto const i32{::llvm::Type::getInt32Ty(module.getContext())};
        auto const i64{::llvm::Type::getInt64Ty(module.getContext())};
        auto const intptr{::llvm::IntegerType::get(module.getContext(),
            static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
        auto const allocation_type{::llvm::FunctionType::get(i32, {intptr, i32, i32, intptr, intptr}, false)};
        auto const scalar_type{::llvm::FunctionType::get(i64, {intptr, i32, intptr, i32}, false)};
        constexpr char8_t new_tag[]{u8"gc_fixed_0_1"};
        constexpr char8_t unsigned_tag[]{u8"gc_struct_get32_unsigned"};
        constexpr char8_t signed_tag[]{u8"gc_struct_get32_signed"};
        auto const expected_new{details::get_llvm_runtime_bridge_function_symbol_name<
            details::llvm_jit_gc_aggregate_fixed_bridge<0u, 1uz>>(allocation_type,
                container::u8string_view{new_tag, sizeof(new_tag) / sizeof(char8_t) - 1uz})};
        auto const expected_get{details::get_llvm_runtime_bridge_function_symbol_name<
            details::llvm_jit_gc_struct_get32_bridge<false>>(scalar_type,
                container::u8string_view{unsigned_tag, sizeof(unsigned_tag) / sizeof(char8_t) - 1uz})};
        auto const expected_signed{details::get_llvm_runtime_bridge_function_symbol_name<
            details::llvm_jit_gc_struct_get32_bridge<true>>(scalar_type,
                container::u8string_view{signed_tag, sizeof(signed_tag) / sizeof(char8_t) - 1uz})};
        auto const new_name{name(expected_new)};
        auto const get_name{name(expected_get)};
        auto const signed_name{name(expected_signed)};
        require(new_name != get_name && get_name != signed_name && new_name != signed_name,
            "actual fixed allocation and four-argument scalar signed/unsigned semantic identities are distinct");
        auto const new_declaration{module.getFunction(new_name)};
        auto const get_declaration{module.getFunction(get_name)};
        // Both preserved WAT fixtures contain unpacked i32 struct.get, hence
        // the UNSIGNED helper is actually emitted. Merely computing the signed
        // name is not execution coverage of struct.get_s/packed fields.
        auto const signed_declaration{module.getFunction(signed_name)};
        require(new_declaration != nullptr && get_declaration != nullptr &&
            new_declaration != get_declaration && new_declaration->isDeclaration() &&
            get_declaration->isDeclaration() && new_declaration->getFunctionType() == allocation_type &&
            get_declaration->getFunctionType() == scalar_type && signed_declaration == nullptr &&
            get_declaration->getCallingConv() == ::llvm::CallingConv::C,
            "actual emitter uses five-arg allocation and i64-return four-arg scalar C ABI; unused signed declaration absent");
        ::std::size_t new_calls{}, get_calls{};
        for(auto const& function : module)
        {
            for(auto const& block : function) for(auto const& instruction : block)
            {
                auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                if(call == nullptr) { continue; }
                auto const callee{call->getCalledFunction()};
                new_calls += callee == new_declaration;
                if(callee == get_declaration)
                {
                    ++get_calls;
                    require(call->arg_size() == 4u && call->getType() == i64 &&
                        call->getArgOperand(0u)->getType() == intptr && call->getArgOperand(1u)->getType() == i32 &&
                        call->getArgOperand(2u)->getType() == intptr && call->getArgOperand(3u)->getType() == i32 &&
                        call->getCallingConv() == ::llvm::CallingConv::C && call->doesNotThrow(),
                        "actual scalar read transports module/kind/token/field in four registers and is nounwind");
                }
            }
        }
        require(new_calls != 0uz && get_calls != 0uz,
            "actual fixed allocation and scalar read declarations have real source-derived call sites");
        ::fast_io::io::println("PASS updated scalar32 GC bridge identities enabled=", enabled ? 1 : 0,
            " optimized=", optimized ? 1 : 0,
            " new=", ::fast_io::mnp::os_c_str(new_name.c_str()),
            " get=", ::fast_io::mnp::os_c_str(get_name.c_str()),
            " signed_not_exercised=", ::fast_io::mnp::os_c_str(signed_name.c_str()),
            " new_calls=", new_calls, " get_calls=", get_calls);
    }
    // Inspect the actual UNOPTIMIZED product LLVM instruction graph. The
    // old runtime collection callbacks remain the execution proof after O3;
    // this extra audit neither manufactures IR nor guesses pointer roots.
    void verify_precise_root_publication(::llvm::Module& module, ::llvm::Function const* numeric,
        bool enabled, bool has_real_eh)
    {
        ::std::size_t records{}, slot_stores{}, publications{}, exceptional_leaves{};
        constexpr auto live_offset{offsetof(frames::root_frame, live_count)};
        constexpr auto carrier_bits{static_cast<unsigned>(sizeof(frames::root_reference) * CHAR_BIT)};
        for(auto& function : module)
        {
            if(function.empty()) { continue; }
            ::llvm::CallBase const* enter{};
            ::llvm::AllocaInst const* record{};
            ::llvm::AllocaInst const* slots{};
            ::llvm::ConstantInt const* capacity{};
            for(auto const& block : function) for(auto const& instruction : block)
            {
                auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                if(call == nullptr || call->getCalledFunction() == nullptr ||
                   !bridge<frames::uwvm_gc_root_frame_enter_checked_abi>(*call->getCalledFunction())) { continue; }
                require(enabled && ::std::addressof(function) != numeric && enter == nullptr && call->arg_size() == 3u,
                    "one genuine nonempty record construction per reference-owning native function");
                enter = call;
                auto const frame_bits{::llvm::dyn_cast<::llvm::PtrToIntInst>(call->getArgOperand(0u))};
                auto const slot_bits{::llvm::dyn_cast<::llvm::PtrToIntInst>(call->getArgOperand(1u))};
                capacity = ::llvm::dyn_cast<::llvm::ConstantInt>(call->getArgOperand(2u));
                record = frame_bits == nullptr ? nullptr : ::llvm::dyn_cast<::llvm::AllocaInst>(frame_bits->getPointerOperand());
                slots = slot_bits == nullptr ? nullptr : ::llvm::dyn_cast<::llvm::AllocaInst>(slot_bits->getPointerOperand());
                require(record != nullptr && slots != nullptr && capacity != nullptr && !capacity->isZero() &&
                    capacity->getZExtValue() <= frames::frame_root_details::max_capacity &&
                    record->getAllocatedType()->isIntegerTy(8u) && slots->getAllocatedType()->isIntegerTy(carrier_bits) &&
                    record->getAlign().value() >= alignof(frames::root_frame) &&
                    slots->getAlign().value() >= alignof(frames::root_reference),
                    "actual entry owns fresh aligned byte-record and complete integer carrier slots with final positive capacity");
                auto const frame_extent{::llvm::dyn_cast<::llvm::ConstantInt>(record->getArraySize())};
                auto const slot_extent{::llvm::dyn_cast<::llvm::ConstantInt>(slots->getArraySize())};
                require(frame_extent != nullptr && slot_extent != nullptr &&
                    frame_extent->getZExtValue() == sizeof(frames::root_frame) &&
                    slot_extent->getValue() == capacity->getValue(),
                    "finalized real entry extents agree with native ABI record and maximum typed snapshot");
                ++records;
            }
            ::llvm::DominatorTree dominance{function};
            for(auto const& block : function) for(auto const& instruction : block)
            {
                bool const named_root{instruction.getName().starts_with("gc.root.")};
                require((enabled && ::std::addressof(function) != numeric) || !named_root,
                    "disabled and pure numeric lowering have no retained root allocations/addresses/loads");
                auto const store{::llvm::dyn_cast<::llvm::StoreInst>(::std::addressof(instruction))};
                if(store != nullptr && slots != nullptr &&
                   ::llvm::getUnderlyingObject(store->getPointerOperand()) == slots)
                {
                    auto const address{::llvm::dyn_cast<::llvm::GetElementPtrInst>(store->getPointerOperand())};
                    auto const index{address == nullptr || address->getNumIndices() != 1u ? nullptr :
                        ::llvm::dyn_cast<::llvm::ConstantInt>(address->getOperand(1u))};
                    require(store->getValueOperand()->getType()->isIntegerTy(carrier_bits) &&
                        address != nullptr && address->getPointerOperand() == slots && index != nullptr &&
                        index->getValue().ult(capacity->getValue()) &&
                        store->getAlign().value() >= alignof(frames::root_reference) && dominance.dominates(enter, store),
                        "actual typed root slot stores one full aligned carrier inside final native extent after construction");
                    ++slot_stores;
                }
                auto const gep{store == nullptr ? nullptr : ::llvm::dyn_cast<::llvm::GetElementPtrInst>(store->getPointerOperand())};
                if(gep != nullptr && record != nullptr && gep->getPointerOperand() == record && gep->getNumIndices() == 1u)
                {
                    auto const offset{::llvm::dyn_cast<::llvm::ConstantInt>(gep->getOperand(1u))};
                    if(offset != nullptr && offset->getZExtValue() == live_offset)
                    {
                        auto const count{::llvm::dyn_cast<::llvm::ConstantInt>(store->getValueOperand())};
                        require(count != nullptr && count->getValue().ule(capacity->getValue()) &&
                            dominance.dominates(enter, store),
                            "actual live-count publication is bounded by finalized capacity and dominated by construction");
                        ::std::vector<bool> written(static_cast<::std::size_t>(count->getZExtValue()));
                        // [same LLVM-owned block instructions ... publication]
                        // [safe                                            ]
                        // Each predecessor borrow is valid until this audit ends;
                        // getPrevNode reaches null only at this block's beginning.
                        for(auto const* previous{store->getPrevNode()}; previous != nullptr; previous = previous->getPrevNode())
                        {
                            auto const field{::llvm::dyn_cast<::llvm::StoreInst>(previous)};
                            if(field == nullptr) { continue; }
                            auto const destination{::llvm::dyn_cast<::llvm::GetElementPtrInst>(field->getPointerOperand())};
                            if(destination == nullptr || destination->getNumIndices() != 1u) { continue; }
                            auto const at{::llvm::dyn_cast<::llvm::ConstantInt>(destination->getOperand(1u))};
                            if(at == nullptr) { continue; }
                            if(destination->getPointerOperand() == record && at->getZExtValue() == live_offset) { break; }
                            if(destination->getPointerOperand() == slots && at->getZExtValue() < written.size())
                            { written[static_cast<::std::size_t>(at->getZExtValue())] = true; }
                        }
                        require(::std::all_of(written.begin(), written.end(), [](bool value) noexcept { return value; }),
                            "every actual published live slot was rewritten by this same-block typed snapshot before its count");
                        ++publications;
                    }
                }
                if(enter != nullptr && (::llvm::isa<::llvm::ResumeInst>(instruction) ||
                    ::llvm::isa<::llvm::ReturnInst>(instruction)))
                {
                    bool retired{};
                    // [same LLVM-owned block ... native terminator] block end
                    // [safe                                       ] walk only
                    // live predecessor handles; never cross a block or retain
                    // them through optimization/erasure of this real module.
                    for(auto const* previous{instruction.getPrevNode()}; previous != nullptr; previous = previous->getPrevNode())
                    {
                        auto const cleanup{::llvm::dyn_cast<::llvm::CallBase>(previous)};
                        if(cleanup != nullptr && cleanup->getCalledFunction() != nullptr &&
                           bridge<frames::uwvm_gc_root_frame_leave_checked_abi>(*cleanup->getCalledFunction()))
                        {
                            require(cleanup->arg_size() == 1u, "actual root leave has its exact one-argument ABI");
                            auto const frame_bits{::llvm::dyn_cast<::llvm::PtrToIntInst>(cleanup->getArgOperand(0u))};
                            require(frame_bits != nullptr && frame_bits->getPointerOperand() == record &&
                                cleanup->doesNotThrow(),
                                "actual native exit retires its own allocation despite distinct ptrtoint SSA handles");
                            retired = true; break;
                        }
                    }
                    require(retired, "real normal/tail/exceptional native exit contains checked root retirement");
                    exceptional_leaves += ::llvm::isa<::llvm::ResumeInst>(instruction);
                }
            }
        }
        require(enabled ? (records != 0uz && slot_stores != 0uz && publications != 0uz) :
            (records == 0uz && slot_stores == 0uz && publications == 0uz),
            "actual graph has complete typed stores/publication only for enabled nonempty records");
        require(!enabled || !has_real_eh || exceptional_leaves != 0uz,
            "actual GC+EH fixture has native exceptional cleanup witnesses");
    }
    void write_ir(::llvm::Module const& module, char const* directory, char const* file_name)
    {
        container::u8string text{};
        details::raw_uwvm_string_ostream stream{text};
        module.print(stream, nullptr); stream.flush();
        auto const path{container::concat_uwvm(::fast_io::mnp::os_c_str(directory), "/",
            ::fast_io::mnp::os_c_str(file_name))};
        ::fast_io::native_file file{::fast_io::mnp::os_c_str(path.c_str()), ::fast_io::open_mode::out};
        // [owned complete IR bytes] end
        // [safe                   ] form one-past only within that allocation.
        auto const begin{reinterpret_cast<::std::byte const*>(text.data())};
        ::fast_io::operations::write_all_bytes(file, begin, begin + text.size());
    }
    void optimize(::llvm::Module& module, ::llvm::TargetMachine* machine)
    {
        ::llvm::LoopAnalysisManager loop{};
        ::llvm::FunctionAnalysisManager function{};
        ::llvm::CGSCCAnalysisManager cgscc{};
        ::llvm::ModuleAnalysisManager analysis{};
        ::llvm::PassBuilder passes{machine};
        passes.registerLoopAnalyses(loop); passes.registerFunctionAnalyses(function);
        passes.registerCGSCCAnalyses(cgscc); passes.registerModuleAnalyses(analysis);
        passes.crossRegisterProxies(loop, function, cgscc, analysis);
        auto pipeline{passes.buildPerModuleDefaultPipeline(::llvm::OptimizationLevel::O3)};
        pipeline.run(module, analysis);
        require(!::llvm::verifyModule(module, ::std::addressof(::llvm::errs())), "same SDK verifies optimized IR");
    }

    // Observe the exact object MCJIT compiles and executes. Never satisfy an
    // engine request with a cached object, a separately generated object or a
    // hand-written IR stub; the runner disassembles these actual bytes.
    class native_object_witness final : public ::llvm::ObjectCache
    {
        container::string path_{};
    public:
        ::std::size_t objects{};
        native_object_witness(char const* directory, bool enabled)
            : path_{container::concat_uwvm(::fast_io::mnp::os_c_str(directory), "/",
                ::fast_io::mnp::os_c_str(enabled ? "roots.native.o" : "no-roots.native.o"))} {}
        void notifyObjectCompiled(::llvm::Module const*, ::llvm::MemoryBufferRef object) override
        {
            require(objects++ == 0uz && object.getBufferSize() != 0uz,
                "exactly one newly compiled actual native Wasm object is captured");
            ::fast_io::native_file file{::fast_io::mnp::os_c_str(path_.c_str()), ::fast_io::open_mode::out};
            // [complete MCJIT-owned object bytes] one-past
            // [safe                            ] copy before this callback's
            // borrowed memory buffer expires; do not retain its native address.
            auto const begin{reinterpret_cast<::std::byte const*>(object.getBufferStart())};
            ::fast_io::operations::write_all_bytes(file, begin, begin + object.getBufferSize());
        }
        ::std::unique_ptr<::llvm::MemoryBuffer> getObject(::llvm::Module const*) override { return {}; }
    };
}

int main(int argc, char** argv)
{
    require(argc == 3, "arguments are exact compiled Wasm fixture and output directory");
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(),
        "actual linked SDK native target initialized");
    ::llvm::EngineBuilder target{};
    target.setOptLevel(::llvm::CodeGenOptLevel::Aggressive).setCodeModel(::llvm::CodeModel::Large);
    ::std::unique_ptr<::llvm::TargetMachine> machine{target.selectTarget()};
    require(machine != nullptr, "actual SDK provides a native target");
    ::fast_io::native_file_loader file{::fast_io::mnp::os_c_str(argv[1]), ::fast_io::open_mode::in};
    strict::byte_vec wasm{};
    wasm.resize(file.size());
    // [complete file-loader bytes] -> [same-size owned Wasm bytes]
    // [safe                      ] parser/initializer borrows the owned copy
    // for this complete test; no pointer survives its final module teardown.
    if(!wasm.empty()) { ::std::memcpy(wasm.data(), file.data(), wasm.size()); }
    auto features{strict::make_wasm1p1_feature_parameter()};
    auto& core{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
    core.disable_gc = false;
    core.disable_tail_call = false;
    core.disable_function_references = false;
    core.disable_exceptions = false;
    compiler::compile_option options{};
    options.validator_feature_parameter = ::std::addressof(features);
    options.verify_llvm_jit_ir = true;
    options.emit_call_stack_frames = false;
    options.emit_unwind_call_stack_frames = true;
    options.native_exception_target_machine = machine.get();
    options.compilation_mode = compiler::llvm_jit_compilation_mode::full;
    options.emit_debug_safe_points = true;
    options.debug_safe_point_granularity = compiler::llvm_jit_debug_safe_point_granularity::instruction;
    for(bool enabled : {false, true})
    {
        require(cohort.empty() && modules.empty() && active_module == nullptr && frames::current_root_frames() == nullptr,
            "previous native engine/borrowed roots retired before fresh module preparation");
        exn_registry_live = false;
        issued_identity = nullptr;
        auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"gc-eh-actual-wasm", {}, features)};
        require(prepared.mod != nullptr && prepared.mod->gc_store && prepared.mod->gc_store->valid(),
            "actual parser and initializer provide a valid Core 3 module/store");
        for(auto const& item : storage::wasm_module_runtime_storage)
        {
            // [actual module map] prepared_runtime keeps every native record live;
            // cohort strong pins protect all stores, including empty-type modules.
            //  ^^ retain only native record addresses, never guest-provided pointers.
            modules.push_back(::std::addressof(item.second));
            if(item.second.gc_store) { cohort.push_back(item.second.gc_store); }
        }
        require(!cohort.empty(), "complete real store cohort retained");

        active_module = prepared.mod;
        require(actual_tag(0u, 0u).exception_identity != actual_tag(0u, 1u).exception_identity,
            "distinct actual instantiated tags retain distinct identity despite equal signatures");
        options.emit_precise_gc_root_frames = enabled;
        ::uwvm2::validation::error::code_validation_error_impl error{};
        auto compiled{compiler::compile_all_from_uwvm(*prepared.mod, options, error, 0uz)};
        require(error.err_code == ::uwvm2::validation::error::code_validation_error_code::ok &&
            compiled.llvm_jit_module.emitted && compiled.llvm_jit_module.llvm_module != nullptr,
            "real product validator/emitter accepts the actual Core 3 Wasm");
        auto const ir{compiled.llvm_jit_module.llvm_module.get()};
        require(!::llvm::verifyModule(*ir, ::std::addressof(::llvm::errs())), "actual unoptimized Wasm IR verifies");
        ::std::size_t enters{}, leaves{}, musttails{};
        ::std::size_t invokes{}, landingpads{}, musttail_leave_witnesses{};
        ::llvm::Function* callback{};
        auto const numeric_name{details::get_llvm_wasm_function_name(*prepared.mod, 11uz)};
        auto const numeric{ir->getFunction(name(numeric_name))};
        require(numeric != nullptr && !numeric->empty(), "actual pure-numeric Wasm typed function exists");
        auto const tail_thrower{ir->getFunction(name(details::get_llvm_wasm_function_name(*prepared.mod, 5uz)))};
        auto const tail_escape{ir->getFunction(name(details::get_llvm_wasm_function_name(*prepared.mod, 10uz)))};
        require(tail_thrower != nullptr && tail_escape != nullptr, "actual source-indexed tail functions exist");
        for(auto& function : *ir)
        {
            if(bridge<::uwvm2::runtime::lib::details::llvm_jit_debug_safe_point_abi_bridge>(function))
            {
                // [live LLVM module-owned declaration] engine retains its owner.
                //  ^^ callback is inspected only before the O3 pass; execution
                // obtains current GV handles again by exact semantic name.
                require(callback == nullptr, "exactly one real debug bridge declaration");
                callback = ::std::addressof(function);
            }
            for(auto& block : function) for(auto& instruction : block)
            {
                invokes += ::llvm::isa<::llvm::InvokeInst>(instruction);
                landingpads += ::llvm::isa<::llvm::LandingPadInst>(instruction);
                auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                if(call == nullptr) { continue; }
                if(auto const target_{call->getCalledFunction()}; target_ != nullptr)
                {
                    if(bridge<frames::uwvm_gc_root_frame_enter_checked_abi>(*target_))
                    {
                        ++enters;
                        require(::std::addressof(function) != numeric, "pure numeric entry owns no empty root ABI call");
                        require(::std::addressof(block) == ::std::addressof(function.getEntryBlock()),
                            "record placement construction is in true native entry only");
                        require(call->doesNotThrow(), "root entry cannot throw through an unconstructed frame");
                    }
                    if(bridge<frames::uwvm_gc_root_frame_leave_checked_abi>(*target_))
                    {
                        ++leaves;
                        require(::std::addressof(function) != numeric, "pure numeric return owns no empty root ABI call");
                    }
                }
                if(auto const direct{::llvm::dyn_cast<::llvm::CallInst>(call)};
                    direct != nullptr && direct->isMustTailCall())
                {
                    ++musttails;
                    require(::llvm::isa<::llvm::ReturnInst>(direct->getNextNode()), "actual tail calls remain musttail plus ret");
                    if(enabled && (::std::addressof(function) == tail_thrower || ::std::addressof(function) == tail_escape))
                    {
                        // [same live basic block instruction sequence] musttail, ret
                        // [safe ] getPrevNode/getNextNode do not cross block ends;
                        // borrowed IR nodes remain module-owned until O3 starts.
                        auto const preceding{::llvm::dyn_cast_or_null<::llvm::CallBase>(direct->getPrevNode())};
                        auto const previous_target{preceding == nullptr ? nullptr : preceding->getCalledFunction()};
                        require(previous_target != nullptr && bridge<frames::uwvm_gc_root_frame_leave_checked_abi>(*previous_target),
                            "typed-reference tail edge executes checked root leave immediately before musttail");
                        ++musttail_leave_witnesses;
                    }
                }
            }
        }
        require(callback != nullptr && musttails >= 2uz && invokes != 0uz && landingpads != 0uz,
            "actual new EH syntax emits native invokes/landingpads, instruction points and musttail edges");
        require(!enabled || musttail_leave_witnesses == 2uz, "both actual reference-bearing tail frames retire at the real tail edge");
        require(enabled ? (enters != 0uz && leaves >= enters) : (enters == 0uz && leaves == 0uz),
            "default lowering adds no root calls; enabled lowering owns and retires every record");
        verify_precise_root_publication(*ir, numeric, enabled, true);
        verify_fixed_gc_bridge_identity(*ir, enabled, false);
        write_ir(*ir, argv[2], enabled ? "roots.ll" : "no-roots.ll");
        optimize(*ir, machine.get());
        verify_fixed_gc_bridge_identity(*ir, enabled, true);
        write_ir(*ir, argv[2], enabled ? "roots.o3.ll" : "no-roots.o3.ll");
        // O3 may discard unused ABI declarations. Obtain genuine current
        // module handles after optimization; never retain a removed GV pointer.
        auto const imports{::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::declare_itanium_dwarf_symbols(
            *ir, *machine)};
        auto const catches{::uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad::declare_catch_runtime(*ir)};
        ::std::string engine_error{};
        // [exclusive actual generated LLVM module] -> [one native engine]
        // [safe                                  ] context holder remains alive
        // outside engine destruction; no instruction/module alias is retired.
        ::std::unique_ptr<::llvm::Module> execution{compiled.llvm_jit_module.llvm_module.release()};
        ::llvm::EngineBuilder builder{::std::move(execution)};
        builder.setErrorStr(::std::addressof(engine_error)).setEngineKind(::llvm::EngineKind::JIT)
            .setOptLevel(::llvm::CodeGenOptLevel::Aggressive).setCodeModel(::llvm::CodeModel::Large);
        builder.setMCJITMemoryManager(::std::make_unique<
            ::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>());
        native_object_witness object_witness{argv[2], enabled};
        ::std::unique_ptr<::llvm::ExecutionEngine> engine{builder.create()};
        require(engine != nullptr, "real same-SDK MCJIT engine created");
        require(::uwvm2::runtime::lib::details::native_exception_host::bind<
            ::uwvm2::runtime::exception::guest_exception>(*engine, imports, catches, make_guest_probe),
            "actual native exception ABI bindings match this generated module/engine");
        map_exact_bridge<&bridges::llvm_jit_debug_safe_point_abi_bridge, &collect_at_debug_point, 5uz>(*engine, *ir);
        map_exact_bridge<&bridges::llvm_jit_throw_tuple_abi_bridge, &test_throw_tuple, 4uz>(*engine, *ir);
        map_exact_bridge<&bridges::llvm_jit_throw_numeric_abi_bridge, &test_throw_numeric, 4uz>(*engine, *ir);
        map_exact_bridge<&bridges::llvm_jit_exception_matches_tag_abi_bridge, &test_matches, 3uz, true>(*engine, *ir);
        map_exact_bridge<&bridges::llvm_jit_exception_copy_tuple_abi_bridge, &test_copy_tuple, 5uz>(*engine, *ir);
        map_exact_bridge<&bridges::llvm_jit_exception_copy_numeric_payload_abi_bridge, &test_copy_numeric, 5uz>(*engine, *ir);
        map_exact_bridge<&bridges::llvm_jit_exception_make_ref_abi_bridge, &test_make_ref, 3uz>(*engine, *ir);
        map_exact_bridge<&bridges::llvm_jit_throw_ref_abi_bridge, &test_throw_ref, 2uz>(*engine, *ir);
        engine->setObjectCache(::std::addressof(object_witness));
        engine->finalizeObject();
        require(object_witness.objects == 1uz, "actual executed native machine object is retained for disassembly");
        force_collection = enabled;
        // Every reference starts in actual Wasm struct.new. The native raw
        // caller supplies no reference bytes and has no product public lease.
        auto const invoke_case{[&](unsigned index, unsigned expected, bool escape, bool foreign)
        {
            current_case = index;
            expect_empty_activation = index == 11u || index == 13u;
            wrong_handler_depth = 0uz;
            inject_foreign = foreign;
            auto const symbol{details::get_llvm_wasm_raw_function_name(*prepared.mod, index)};
            auto const address{engine->getFunctionAddress(name(symbol))};
            require(address != 0u, "actual MCJIT publishes the generated raw entry");
            using raw_entry = void(*)(::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t,
                ::std::uintptr_t, ::std::uintptr_t);
            static_assert(sizeof(raw_entry) == sizeof(address));
            raw_entry entry{};
            // [real native entry address][same-size callable representation]
            // [safe ] Copy the address obtained from the exact live engine;
            // no untrusted object/guest pointer is promoted to a function.
            ::std::memcpy(::std::addressof(entry), ::std::addressof(address), sizeof(entry));
            ::std::uint32_t result{};
            bool escaped{};
            require(frames::current_root_frames() == nullptr, "root TLS chain empty before every native guest entry");
            try
            {
                entry(0u, reinterpret_cast<::std::uintptr_t>(::std::addressof(result)), sizeof(result), 0u, 0u);
                require(!escape && !foreign && result == expected, "actual Core 3 guest result survives all forced collections");
            }
            catch(eh::guest_exception const& activation)
            {
                require(escape && !foreign, "only selected true guest exception escapes to this typed native owner");
                require(frames::current_root_frames() == nullptr, "all generated unmatched/escape/tail root records retired on C++ EH");
                auto const reclaimed_before{reclaimed_total};
                static_cast<void>(collect_now(activation.instance().get()));
                require(!enabled || index != 8u || reclaimed_total > reclaimed_before,
                    "escaped native payload survives while exited frame's unrelated box is really reclaimed");
                auto const reference{payload_reference(*activation.instance())};
                storage::gc_object_value observed{};
                require(active_module->gc_store->struct_get(reference, 0uz, false, observed) == storage::gc_object_status::ok &&
                    observed.as<::std::uint32_t>() == expected, "explicitly enumerated escaped native immutable payload remains live");
                ++native_escapes;
                escaped = true;
            }
            catch(foreign_probe const&)
            {
                require(foreign && !escape && frames::current_root_frames() == nullptr,
                    "guest catch_all does not absorb foreign native EH; every actual root record retires");
                expect_empty_activation = true;
                static_cast<void>(collect_now()); // No guest/native exception payload owner survives.
                ++foreign_escapes;
                escaped = true;
            }
            require(escaped == (escape || foreign), "escape classification matches the actual native control edge");
            require(frames::current_root_frames() == nullptr, "normal/catch/unmatched/foreign/tail paths leave no stale record");
            inject_foreign = false;
        }};
        for(auto const [index, expected] : ::std::array<::std::array<unsigned, 2uz>, 6uz>{{
            {1u, 37u}, {3u, 41u}, {4u, 43u}, {6u, 47u}, {11u, 17u}, {13u, 71u}}})
        { invoke_case(index, expected, false, false); }
        invoke_case(8u, 59u, true, false);
        invoke_case(10u, 67u, true, false);
        invoke_case(4u, 0u, false, true);
        // LAST: actual catch_ref permanently roots an exn token in this module.
        // From this point every collection attempt must explicitly reject until
        // the fresh profile's real module teardown, including its throw_ref.
        auto const rejections_before{registry_rejections};
        invoke_case(9u, 61u, false, false);
        require(issued_identity != nullptr && exn_registry_live &&
            (!enabled || registry_rejections > rejections_before),
            "actual catch_ref registry and throw_ref identity are tested without claiming successful exn collection");
        issued_identity = nullptr; // Retire this non-owning observer before store teardown.
        active_module = nullptr;
        modules.clear();
        cohort.clear();
        force_collection = false;
        expect_empty_activation = false;
    }
    // All engines and their actual prepared_runtime leases are now gone. This
    // is explicit host-controlled final module teardown, not runtime_reset GC.
    storage::wasm_module_runtime_storage.clear();
    ::std::size_t teardown_reclaimed{};
    require(storage::gc_object_store::collect_exclusive_aggregate_domain(nullptr, 0uz, nullptr, 0uz,
        teardown_reclaimed) == storage::gc_object_status::ok && teardown_reclaimed == 0uz,
        "real module teardown retired the last exn registry, admitted stores and owned aggregate tokens");
    require(collections > 20uz && reclaimed_total != 0uz && max_depth >= 2uz &&
        outer_cleanup_witnesses != 0uz && native_escapes == 4uz && foreign_escapes == 2uz && foreign_destructors == 2uz &&
        tuple_copies != 0uz && numeric_copies == 2uz && matches != 0uz && ref_issues == 2uz && ref_throws == 2uz &&
        registry_rejections != 0uz && frames::current_root_frames() == nullptr,
        "real repeated collections, cross-frame native EH, exact tag/ref identity and retirement have genuine witnesses");
    ::fast_io::io::println("PASS actual Core 3 GC+EH LLVM roots: checks=", checks,
        " collections=", collections, " reclaimed=", reclaimed_total, " max_native_depth=", max_depth,
        " unmatched_cleanup=", outer_cleanup_witnesses, " native_escapes=", native_escapes,
        " foreign_escapes=", foreign_escapes, " foreign_destructors=", foreign_destructors, " fresh_throws=", fresh_throws,
        " ref_issues=", ref_issues, " ref_throws=", ref_throws, " expected_exn_registry_rejections=", registry_rejections,
        "; actual parser/initializer/validator/O3/MCJIT/SMM/nativeEH; testOwnedEHAdapters=true; singleMutator=true; autoVMGC=false");
}

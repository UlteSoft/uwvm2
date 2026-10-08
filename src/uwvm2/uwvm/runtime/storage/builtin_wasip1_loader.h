// Cold native loader provenance only. The identity owns this provenance record,
// NOT the unique type-erased native implementation. Consumers must retain the
// actual maintenance/closed host/cohort/source lease for every lexical borrow.
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <type_traits>
# include <utility>
# include <uwvm2/utils/macro/push_macros.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
# endif
# include <fast_io.h>
# include <fast_io_crypto.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/local_imported/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::full
{
    class full_source_instance;
    // Pointer-free DATA only. Even a genuine copy does not authorize a host
    // call, restore external state, issue a VM world or extend provider life.
    struct builtin_wasip1_function_data
    {
        static constexpr unsigned interface_revision{1u};
        ::std::uint64_t function_index{};
        ::std::array<::std::byte, 32u> interface_sha256{};
        unsigned parameter_count{}, result_count{};
        bool exits_guest{}; // derived only from the canonical builtin tuple
        bool exit_wasm64{};
        ::std::array<unsigned char, 16u> parameters{}, results{};
    };
    class builtin_wasip1_loader_identity final
    {
        using module = ::uwvm2::uwvm::wasm::type::local_imported_t;
        using native_implementation = decltype(::std::declval<module>().ptr);
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_CPP_EXCEPTIONS)
        using builtin = ::uwvm2::uwvm::imported::wasi::wasip1::local_imported::wasip1_local_imported_module_t;
        using tuple = builtin::local_function_tuple;
#endif
        native_implementation implementation_{};
        ::std::size_t ordinal_{};
        ::std::weak_ptr<builtin_wasip1_loader_identity const> canonical_owner_{};
        inline static ::std::shared_ptr<builtin_wasip1_loader_identity const> selected_{};
        explicit builtin_wasip1_loader_identity(native_implementation implementation, ::std::size_t ordinal) noexcept
            : implementation_{implementation}, ordinal_{ordinal} {}
        friend class full_source_instance;
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_CPP_EXCEPTIONS)
        template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        [[nodiscard]] static constexpr auto const& known_functions(::uwvm2::uwvm::wasm::type::feature_list<Fs...>) noexcept
        {
            return ::uwvm2::uwvm::wasm::type::details::all_function_information_cache<tuple, Fs...>::function_information;
        }
#endif
        [[nodiscard]] static bool canonical(::std::shared_ptr<builtin_wasip1_loader_identity const> const& value) noexcept
        {
            return value && !value->canonical_owner_.owner_before(value) && !value.owner_before(value->canonical_owner_);
        }
        [[nodiscard]] module const* current_member() const noexcept
        {
            auto const& actual{::uwvm2::uwvm::wasm::storage::preload_local_imported};
            if(ordinal_ >= actual.size()) { return nullptr; }
            // [actual native vector0..size][ordinal<size] end
            // [safe] real native extent BEFORE indexing; never dereference a
            // supplied wrapper pointer or an old recorded implementation.
            auto const& member{actual.index_unchecked(ordinal_)};
            if(member.ptr == nullptr || member.ptr != implementation_) { return nullptr; }
            ::std::size_t memberships{};
            for(auto const& entry : ::uwvm2::uwvm::wasm::storage::all_module)
            {
                if(entry.second.type == ::uwvm2::uwvm::wasm::type::module_type_t::local_import &&
                   entry.second.module_storage_ptr.li == ::std::addressof(member))
                { if(++memberships != 1u) { return nullptr; } }
            }
            // No name, caller ID, exported interface shape or public ptr is
            // sufficient: this also needs the concrete typed factory record.
            return memberships == 1u ? ::std::addressof(member) : nullptr;
        }
        [[nodiscard]] bool copy_function(module const* actual, ::std::size_t index,
            builtin_wasip1_function_data& output) const noexcept
        {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_CPP_EXCEPTIONS)
            if(actual == nullptr || current_member() != actual) { return false; }
            auto const& known{known_functions(::uwvm2::uwvm::wasm::type::binfmt_ver1_feature_list_t{})};
            if(index >= known.size()) { return false; }
            // [known compile-time WASIp1 tuple0..size][index<size] end
            // [safe] bound BEFORE index, and read no user-provided signature.
            auto const& signature{known[index].function_type};
            builtin_wasip1_function_data result{};result.function_index=index;
            auto const name{known[index].function_name};
            result.exit_wasm64 = name == u8"proc_exit_wasm64";
            result.exits_guest = name == u8"proc_exit" || result.exit_wasm64;
            auto const copy{[](auto const& native, auto& destination, unsigned& count) noexcept -> bool
            {
                // These pointers originate ONLY in our known compile-time
                // tuple signature cache. Validate empty/pair/extent BEFORE
                // subtracting or advancing; never consume a host request view.
                if(native.begin == nullptr || native.end == nullptr)
                { return native.begin == nullptr && native.end == nullptr; }
                if(native.end < native.begin) { return false; }
                auto const extent{static_cast<::std::size_t>(native.end-native.begin)};
                if(extent > destination.size()) { return false; }
                for(::std::size_t position{}; position != extent; ++position)
                {
                    auto const code{static_cast<unsigned>(native.begin[position])};
                    if(code != 0x7fu && code != 0x7eu) { return false; }
                    destination[position]=static_cast<unsigned char>(code);
                }
                count=static_cast<unsigned>(extent);return true;
            }};
            if(!copy(signature.parameter,result.parameters,result.parameter_count) ||
               !copy(signature.result,result.results,result.result_count)) { return false; }
            // Bind the ENTIRE ordered known interface, including names and
            // signature types. Equal ABI shapes or a reordered optional tuple
            // must not accidentally mean the same persisted host function.
            if(known.size() > 128u) { return false; }
            ::fast_io::sha256_context hash{};
            auto const hash_number{[&](::std::uint64_t value) noexcept
            {
                ::std::array<unsigned char,8u> bytes{};
                // [owned8 LE output bytes][one-past] end
                // [safe] fixed extent BEFORE constructing/advancing cursors.
                ::fast_io::basic_obuffer_view<unsigned char> out{bytes.data(),bytes.data()+bytes.size()};
                ::fast_io::io::print(out,::fast_io::mnp::le_put<64u>(value));
                hash.update(reinterpret_cast<::std::byte const*>(bytes.data()),
                    reinterpret_cast<::std::byte const*>(bytes.data()+bytes.size()));
            }};
            hash_number(builtin_wasip1_function_data::interface_revision);hash_number(known.size());
            for(::std::size_t position{};position != known.size();++position)
            {
                auto const& entry{known[position]};auto const name{entry.function_name};
                if(entry.index != position || name.empty() || name.size() > 1024u || name.data() == nullptr) { return false; }
                // [known compile-time UTF8 name0..size<=1024][one-past] end
                // [safe] owned literal extent BEFORE any pointer advance.
                hash_number(position);hash_number(name.size());
                auto const* first{reinterpret_cast<::std::byte const*>(name.data())};hash.update(first,first+name.size());
                ::std::array<unsigned char,16u> parameters{},results{};unsigned parameter_count{},result_count{};
                if(!copy(entry.function_type.parameter,parameters,parameter_count) ||
                   !copy(entry.function_type.result,results,result_count)) { return false; }
                hash_number(parameter_count);hash_number(result_count);
                // copy() proves each count<=16 before these fixed array end
                // pointers are formed, including zero-arity signatures.
                auto const* p{reinterpret_cast<::std::byte const*>(parameters.data())};hash.update(p,p+parameter_count);
                auto const* r{reinterpret_cast<::std::byte const*>(results.data())};hash.update(r,r+result_count);
            }
            hash.do_final();hash.digest_to_byte_ptr(result.interface_sha256.data());
            output=result;return true;
#else
            (void)actual;(void)index;(void)output;return false;
#endif
        }
    public:
        builtin_wasip1_loader_identity(builtin_wasip1_loader_identity const&) = delete;
        builtin_wasip1_loader_identity& operator=(builtin_wasip1_loader_identity const&) = delete;
        ~builtin_wasip1_loader_identity() = default;
        // Sole issuer performs the real original insertion itself from the
        // exact builtin concrete type. No module pointer, type name, caller
        // ready bool, imported target or external registration is accepted.
        // Native setup must be serialized/drained just as the original loader.
        [[nodiscard]] static bool append_actual_builtin_for_native_loader() noexcept
        {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            auto& actual{::uwvm2::uwvm::wasm::storage::preload_local_imported};
            auto const ordinal{actual.size()};
            selected_.reset(); // A previous setup identity never survives reuse.
            actual.emplace_back(::uwvm2::uwvm::imported::wasi::wasip1::local_imported::wasip1_local_imported_module);
            // Only debugger-owned immutable sources need the provenance record.
            // Ordinary loaders retain their original allocation/call behavior.
            if(!::uwvm2::uwvm::wasm::storage::active_execute_wasm().has_owned_source_image()) { return true; }
            if(ordinal >= actual.size() || actual.size()-ordinal != 1u) { return false; }
# if defined(UWVM_CPP_EXCEPTIONS)
            try
            {
                auto const implementation{actual.index_unchecked(ordinal).ptr};
                if(implementation == nullptr) { return false; }
                ::std::shared_ptr<builtin_wasip1_loader_identity> result{
                    new builtin_wasip1_loader_identity{implementation,ordinal}};
                result->canonical_owner_=result;selected_=::std::move(result);return true;
            }
            catch(...) { return false; } // No source/publication can seal this failed setup.
# else
            // Original no-EH native loading still succeeds. These builds cannot
            // issue an executable/debug capture or a provider identity: leaving
            // selected_ empty preserves that denial without changing WASIp1.
            return true;
# endif
#else
            return false;
#endif
        }
    };
    static_assert(!::std::is_copy_constructible_v<builtin_wasip1_loader_identity>);
}
#ifndef UWVM_MODULE
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# endif
# include <uwvm2/utils/macro/pop_macros.h>
#endif

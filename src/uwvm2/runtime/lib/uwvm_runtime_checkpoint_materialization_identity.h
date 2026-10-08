// PRIVATE cold actual materialization DATA. Included in the runtime anonymous
// namespace after compiled_module_record's forward declaration, before its full
// code owner. Only the REAL materializer can construct it after finalization and
// complete symbol resolution. No serialized/request owner can issue this DATA.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
        class checkpoint_materialization_identity_data final
        {
            friend inline constexpr bool try_materialize_runtime_module_llvm_jit(compiled_module_record&,
                bool, ::llvm::CodeGenOptLevel, ::std::size_t) noexcept;
            using source_owner = ::uwvm2::uwvm::runtime::full::full_source_instance::owner;
            using profile_owner = ::uwvm2::runtime::checkpoint::compilation_profile::owner;
            using digest = ::std::array<::std::byte, 32u>;
        public:
            enum class status : unsigned char
            { source_and_compilation_data, unavailable_source_origin, invalid_actual_source,
                unavailable_effective_cache_policy, incomplete_initial_generation, quota_exceeded, allocation_failed };
            enum class cache_origin : unsigned char { unavailable, off_debug_process_binding };
            struct data
            {
                status observation{status::invalid_actual_source};
                cache_origin actual_cache_origin{cache_origin::unavailable};
                ::std::uint64_t actual_module{}, source_bytes{}, module_name_bytes{}, initial_defined_functions{}, imported_functions{};
                digest original_wasm{}, actual_module_name{}, initial_body_generation_closure{};
                digest compilation_key{}, compilation_isa{}, compilation_context{}, compilation_policy{};
                ::uwvm2::runtime::checkpoint::compilation_profile::cache_identity_type actual_profile{};
                bool imported_source_closure_incomplete{};
                // Those metadata hashes were consumed by THIS real compiler;
                // they are not an accepted cache entry/object or exact physical
                // loaded product/provider/build image attestation.
                inline static constexpr bool accepted_cache_object = false;
                inline static constexpr bool exact_loaded_build_provider_identity = false;
                inline static constexpr bool whole_binding_or_restore_authority = false;
            };
        private:
            source_owner source_{};
            profile_owner profile_{};
            data observed_{};
            checkpoint_materialization_identity_data() = default;
            template<typename A, typename B>
            [[nodiscard]] static bool same_owner(A const& a, B const& b) noexcept
            { return a && b && a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a); }
            [[nodiscard]] static digest hash_actual_extent(void const* pointer, ::std::size_t size) noexcept
            {
                ::fast_io::sha256_context hash{};
                if(size != 0u)
                {
                    // ALL callers below have checked actual owner, extent,
                    // nonnull and PTRDIFF bounds before this one-past cursor.
                    // [SAME private source/owned metadata: first...size] end
                    // [safe] caller-proven bounded owner BEFORE first+size.
                    auto const* first{static_cast<::std::byte const*>(pointer)};
                    hash.update(first, first + size);
                }
                hash.do_final(); digest value{}; hash.digest_to_byte_ptr(value.data()); return value;
            }
            template<unsigned Bits>
            static void hash_number(::fast_io::sha256_context& hash, ::std::uint64_t value) noexcept
            {
                static_assert(Bits == 64u);
                ::std::array<unsigned char, Bits / 8u> bytes{};
                // [actual fixed8 owned destination] end
                // [safe] full compile-time array extent BEFORE data()+size().
                ::fast_io::basic_obuffer_view<unsigned char> out{bytes.data(), bytes.data() + bytes.size()};
                ::fast_io::print(out, ::fast_io::mnp::le_put<Bits>(value));
                auto const* first{reinterpret_cast<::std::byte const*>(bytes.data())};
                // [same actual fixed8 local array] end; full bound BEFORE +8.
                hash.update(first, first + bytes.size());
            }
            static void hash_uleb(::fast_io::sha256_context& hash, ::std::uint64_t value) noexcept
            {
                // Unsigned64 LEB needs <=10 bytes, independent of host endian.
                ::std::array<unsigned char, 10u> bytes{};
                // [fixed10 owned scratch] end; full bound BEFORE +size().
                ::fast_io::basic_obuffer_view<unsigned char> out{bytes.data(), bytes.data() + bytes.size()};
                ::fast_io::print(out, ::fast_io::mnp::leb128_put(value));
                // [first...actual encoded<=10][unused] end
                // [safe] fast_io bounded reserve proves its current cursor;
                // reinterpretation retains SAME array, never reads unused bytes.
                hash.update(reinterpret_cast<::std::byte const*>(bytes.data()), reinterpret_cast<::std::byte const*>(out.curr_ptr));
            }
            template<typename ActualField>
            static void hash_pair(::fast_io::sha256_context& hash,
                ::uwvm2::utils::container::u8string_view key, ActualField const& field) noexcept
            {
                // Only privately current context owners / literal keys reach
                // here, AFTER whole-field <=1MiB/PTRDIFF preflight below. Same
                // canonical length+key+length+value order as cache format5.
                hash_uleb(hash, static_cast<::std::uint64_t>(key.size()));
                if(!key.empty())
                {
                    // [complete known key owner] end; fixed key bound BEFORE +.
                    auto const* first{reinterpret_cast<::std::byte const*>(key.data())}; hash.update(first, first + key.size());
                }
                hash_uleb(hash, static_cast<::std::uint64_t>(field.size()));
                if(!field.empty())
                {
                    // [actual privately preflighted value owner] end
                    // [safe] nonempty field data and bounded size BEFORE +.
                    auto const* first{reinterpret_cast<::std::byte const*>(field.data())}; hash.update(first, first + field.size());
                }
            }
            [[nodiscard]] static digest finalize_hash(::fast_io::sha256_context& hash) noexcept
            { hash.do_final(); digest result{}; hash.digest_to_byte_ptr(result.data()); return result; }
            [[nodiscard]] static ::std::unique_ptr<checkpoint_materialization_identity_data> retain(
                checkpoint_materialization_identity_data&& value) noexcept
            {
                try { return ::std::unique_ptr<checkpoint_materialization_identity_data>{new checkpoint_materialization_identity_data{::std::move(value)}}; }
                catch(...) { return {}; }
            }
            [[nodiscard]] static ::std::unique_ptr<checkpoint_materialization_identity_data> from_actual_finalized_materializer(
                source_owner const& source, profile_owner const& profile,
                ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* actual_module,
                ::std::uint64_t actual_id, ::std::uint_least64_t actual_epoch,
                ::uwvm2::utils::container::u8string_view actual_name,
                ::uwvm2::runtime::llvm_jit_cache::cache_context const& actual_context,
                ::uwvm2::runtime::llvm_jit_cache::cache_policy const& actual_policy,
                bool actual_debug_off_enforced, ::std::span<::std::uint_least64_t const> actual_generations) noexcept
            {
                // Sole friend executes this only AFTER real finalizeObject and
                // all typed/raw entry addresses succeeded in the actual code
                // publication. The caller cannot be a guest/API/decoder.
                if(!profile) { return {}; } // default full creates no hash/workspace/owner
                checkpoint_materialization_identity_data out{}; out.source_ = source; out.profile_ = profile;
                out.observed_.actual_profile = profile->cache_identity(); out.observed_.actual_module = actual_id;
                if(!actual_debug_off_enforced || actual_policy.enable)
                { out.observed_.observation = status::unavailable_effective_cache_policy; return retain(::std::move(out)); }
                out.observed_.actual_cache_origin = cache_origin::off_debug_process_binding;
                if(!source || !::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(source) ||
                    !source->initialized_from_actual_state() || actual_epoch == 0u || source->actual_full_validation_epoch() != actual_epoch)
                { return retain(::std::move(out)); }
                // source is the REAL actual code publication's owned source,
                // never a supplied native pointer. Resolve its actual registry
                // member/address BEFORE dereferencing actual_module.
                auto const member{source->registry().find(actual_name)};
                if(member == source->registry().end() || ::std::addressof(member->second) != actual_module ||
                    actual_module != source->initialized_main_module() || source->bound_initialized_main_module_id() != actual_id)
                { return retain(::std::move(out)); }
                // Actual private publisher source/member get/control lineage
                // is already qualified above BEFORE invoking this trusted-pointee
                // helper. Reuse its real initializer seal + preparse private
                // owned-image hash; never rehash a live map or reopen a path.
                auto const original{::uwvm2::uwvm::runtime::full::digest_actual_owned_source(source)};
                if(original.status != ::uwvm2::uwvm::runtime::full::source_digest_status::ok || !original.identity ||
                    !same_owner(original.identity->source_owner(), source))
                { out.observed_.observation = status::unavailable_source_origin; return retain(::std::move(out)); }
                auto const& module{member->second}; auto const source_size{original.identity->source_bytes()};
                if(actual_name.size() > 1048576u || actual_name.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) ||
                    (actual_name.size() != 0u && actual_name.data() == nullptr))
                { out.observed_.observation = status::quota_exceeded; return retain(::std::move(out)); }
                auto const defined{module.local_defined_function_vec_storage.size()};
                if(defined > 1048576u || actual_generations.size() != defined ||
                    defined > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(::std::uint_least64_t) ||
                    (defined != 0u && actual_generations.data() == nullptr))
                { out.observed_.observation = status::incomplete_initial_generation; return retain(::std::move(out)); }
                for(auto generation : actual_generations)
                { if(generation != 1u) { out.observed_.observation = status::incomplete_initial_generation; return retain(::std::move(out)); } }
                try
                {
                    // Bound ALL variable context fields before the actual
                    // metadata encoder allocates/copies its canonical buffers.
                    ::std::size_t remaining{1048576u - 256u};
                    for(auto const* field : {::std::addressof(actual_context.target_triple), ::std::addressof(actual_context.cpu_name),
                        ::std::addressof(actual_context.cpu_features), ::std::addressof(actual_context.llvm_version),
                        ::std::addressof(actual_context.uwvm_abi), ::std::addressof(actual_context.cache_key), ::std::addressof(actual_context.codegen_policy)})
                    {
                        if(field->size() > remaining || field->size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()))
                        { out.observed_.observation = status::quota_exceeded; return retain(::std::move(out)); }
                        remaining -= field->size(); // actual owner size checked BEFORE subtraction
                    }
                    // Existing cache metadata builders use noexcept fast_io
                    // vector allocation. This OPTIONAL observation must not add
                    // those unrecoverable allocations after a valid compilation.
                    // Hash the exact official format5 fields directly instead.
                    if(!actual_context.cache_key_is_complete || actual_context.cache_key.empty() || actual_context.codegen_policy.empty())
                    { out.observed_.observation = status::quota_exceeded; return retain(::std::move(out)); }
                    // Each field is a bounded real private owner; canonical
                    // length/key/value streaming uses only fixed10 LEB scratch.
                    out.observed_.source_bytes = source_size; out.observed_.module_name_bytes = actual_name.size();
                    out.observed_.initial_defined_functions = defined;
                    out.observed_.imported_functions = module.imported_function_vec_storage.size();
                    out.observed_.original_wasm = original.identity->original_sha256();
                    out.observed_.actual_module_name = hash_actual_extent(actual_name.data(), actual_name.size());
                    out.observed_.compilation_key = hash_actual_extent(actual_context.cache_key.data(), actual_context.cache_key.size());
                    ::fast_io::sha256_context isa{}, metadata{};
                    hash_pair(isa, u8"target", actual_context.target_triple); hash_pair(isa, u8"cpu", actual_context.cpu_name);
                    hash_pair(isa, u8"features", actual_context.cpu_features);
                    hash_pair(metadata, u8"product", ::uwvm2::runtime::llvm_jit_cache::cache_product_name);
                    hash_pair(metadata, u8"key", actual_context.cache_key); hash_pair(metadata, u8"llvm", actual_context.llvm_version);
                    hash_pair(metadata, u8"uwvm_abi", actual_context.uwvm_abi); hash_pair(metadata, u8"codegen", actual_context.codegen_policy);
                    out.observed_.compilation_isa = finalize_hash(isa); out.observed_.compilation_context = finalize_hash(metadata);
                    out.observed_.compilation_policy = hash_actual_extent(actual_context.codegen_policy.data(), actual_context.codegen_policy.size());
                    out.observed_.imported_source_closure_incomplete = !module.imported_function_vec_storage.empty() ||
                        !module.imported_memory_vec_storage.empty() || !module.imported_table_vec_storage.empty() ||
                        !module.imported_global_vec_storage.empty() || !module.imported_tag_vec_storage.empty();
                    ::fast_io::sha256_context generations{};
                    constexpr ::std::array<::std::byte, 8u> domain{::std::byte{'U'}, ::std::byte{'W'}, ::std::byte{'C'}, ::std::byte{'P'},
                        ::std::byte{'G'}, ::std::byte{'E'}, ::std::byte{'N'}, ::std::byte{1u}};
                    // [complete actual fixed8 domain / 32 source-hash arrays]
                    // [safe] each complete array bound BEFORE one-past update.
                    generations.update(domain.data(), domain.data() + domain.size());
                    generations.update(out.observed_.original_wasm.data(), out.observed_.original_wasm.data() + out.observed_.original_wasm.size());
                    hash_number<64>(generations, actual_id); hash_number<64>(generations, defined);
                    hash_number<64>(generations, out.observed_.imported_functions);
                    for(auto generation : actual_generations) { hash_number<64>(generations, generation); }
                    generations.do_final(); generations.digest_to_byte_ptr(out.observed_.initial_body_generation_closure.data());
                    out.observed_.observation = status::source_and_compilation_data;
                    return retain(::std::move(out));
                }
                catch(...) { out.observed_.observation = status::allocation_failed; return retain(::std::move(out)); }
            }
        public:
            checkpoint_materialization_identity_data(checkpoint_materialization_identity_data const&) = delete;
            checkpoint_materialization_identity_data& operator=(checkpoint_materialization_identity_data const&) = delete;
            checkpoint_materialization_identity_data(checkpoint_materialization_identity_data&&) noexcept = default;
            checkpoint_materialization_identity_data& operator=(checkpoint_materialization_identity_data&&) noexcept = default;
            ~checkpoint_materialization_identity_data() = default;
            [[nodiscard]] data const& get() const noexcept { return observed_; }
            [[nodiscard]] source_owner const& actual_source_owner() const noexcept { return source_; }
            [[nodiscard]] profile_owner const& actual_profile_owner() const noexcept { return profile_; }
        };
#endif

// Runtime-private uncaught Core 3 diagnostics. Include only in the actual
// runtime aggregation TU after its complete registry, exception and IO owners.
// No parser/storage definition is repeated here and no guest operation calls it.
#pragma once

#if (defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)) && defined(UWVM_CPP_EXCEPTIONS)
        template<typename Output>
        inline void print_guest_exception_prefix(Output& output, bool fatal) noexcept
        {
            ::fast_io::io::print(output,
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE), u8"uwvm: ",
                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color,
                                     ::fast_io::mnp::cond(fatal, UWVM_COLOR_U8_LT_RED, UWVM_COLOR_U8_LT_GREEN)),
                fatal ? u8"[fatal] " : u8"[info]  ",
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE));
        }

        template<typename Output, typename Name>
        inline void print_guest_exception_quoted_name(Output& output, Name const& name) noexcept
        {
            ::fast_io::io::print(output, u8"\"");
            for(char8_t ch: name)
            {
                // Custom-name data belongs to the guest. Escape terminal controls and quotes so an
                // embedded newline/escape cannot forge another diagnostic line or change its colors.
                if(ch == u8'"' || ch == u8'\\') { ::fast_io::io::print(output, u8"\\", ::fast_io::mnp::chvw(ch)); }
                else if(ch < u8' ' || ch == u8'\x7f')
                { ::fast_io::io::print(output, u8"\\x", ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least8_t>(ch))); }
                else { ::fast_io::io::print(output, ::fast_io::mnp::chvw(ch)); }
            }
            ::fast_io::io::print(output, u8"\"");
        }

        template<typename Output>
        inline void print_guest_exception_payload(Output& output, ::uwvm2::runtime::exception::payload_field const& field) noexcept
        {
            using kind = ::uwvm2::runtime::exception::payload_kind;
            auto const bits{field.bits()};
            // [immutable field-owned bytes] end; payload_field guarantees the exact width for its kind.
            // [safe                       ] memcpy avoids alignment and does not evaluate sNaN payloads.
            switch(field.kind())
            {
                case kind::i32: case kind::f32:
                {
                    ::std::uint32_t raw;
                    ::std::memcpy(::std::addressof(raw), bits.data(), sizeof(raw));
                    ::fast_io::io::print(output, field.kind() == kind::i32 ? u8"i32 bits=0x" : u8"f32 bits=0x",
                                        ::fast_io::mnp::hex<false, true>(raw));
                    if(field.kind() == kind::i32)
                    { ::fast_io::io::print(output, u8" signed=", ::std::bit_cast<::std::int32_t>(raw)); }
                    break;
                }
                case kind::i64: case kind::f64:
                {
                    ::std::uint64_t raw;
                    ::std::memcpy(::std::addressof(raw), bits.data(), sizeof(raw));
                    ::fast_io::io::print(output, field.kind() == kind::i64 ? u8"i64 bits=0x" : u8"f64 bits=0x",
                                        ::fast_io::mnp::hex<false, true>(raw));
                    if(field.kind() == kind::i64)
                    { ::fast_io::io::print(output, u8" signed=", ::std::bit_cast<::std::int64_t>(raw)); }
                    break;
                }
                case kind::v128:
                {
                    ::fast_io::io::print(output, u8"v128 storage_bytes=[");
                    for(::std::size_t i{}; i != bits.size(); ++i)
                    {
                        if(i != 0uz) { ::fast_io::io::print(output, u8" "); }
                        ::fast_io::io::print(output, ::fast_io::mnp::hex<false, true>(::std::to_integer<::std::uint_least8_t>(bits[i])));
                    }
                    ::fast_io::io::print(output, u8"]");
                    break;
                }
                case kind::reference: ::fast_io::io::print(output, u8"reference (opaque)"); break;
                case kind::wasm_reference:
                {
                    using carrier = ::uwvm2::object::global::wasm_global_ref_t;
                    // [field-owned complete Wasm carrier] end
                    // [safe                            ] read only the token and kind representations;
                    // no guest or host pointer is dereferenced while reporting an uncaught throw.
                    static_assert(offsetof(carrier, storage) == 0uz);
                    static_assert(sizeof(::std::uintptr_t) <= sizeof(carrier));
                    ::std::uintptr_t opaque_token{};
                    ::std::uint_least32_t raw_kind{};
                    static_assert(sizeof(raw_kind) == sizeof(::uwvm2::object::global::wasm_ref_kind));
                    static_assert(offsetof(carrier, kind) + sizeof(raw_kind) <= sizeof(carrier));
                    if(bits.size() != sizeof(carrier)) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::memcpy(::std::addressof(opaque_token), bits.data(), sizeof(opaque_token));
                    // [token bytes][kind bytes][carrier padding] end
                    // [safe                       ] offsetof(kind)+sizeof(raw_kind) <= sizeof(carrier).
                    //              ^^ address of the out-of-band kind; never treated as a pointer to guest data.
                    ::std::memcpy(::std::addressof(raw_kind),
                                  bits.data() + offsetof(carrier, kind),
                                  sizeof(raw_kind));
                    using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
                    auto const label{[&]() noexcept -> ::uwvm2::utils::container::u8string_view
                    {
                        switch(static_cast<ref_kind>(raw_kind))
                        {
                            case ref_kind::wasm_null: return u8"null";
                            case ref_kind::wasm_func: return u8"func";
                            case ref_kind::wasm_func_imported: return u8"func-imported";
                            case ref_kind::wasm_func_defined: return u8"func-defined";
                            case ref_kind::wasm_extern: return u8"extern";
                            case ref_kind::wasm_struct: return u8"struct";
                            case ref_kind::wasm_array: return u8"array";
                            case ref_kind::wasm_exn: return u8"exn";
                            case ref_kind::wasm_i31: return u8"i31";
                            default: return u8"unknown";
                        }
                    }()};
                    ::fast_io::io::print(output, u8"wasm_reference kind=", label,
                                        u8" opaque_token=0x", ::fast_io::mnp::hex<false, true>(opaque_token));
                    break;
                }
                default: ::fast_io::fast_terminate();
            }
        }

        // Defined after the actual entry-lease accessor in this same runtime
        // TU. This private check accepts no address, ID, file or metadata token.
        [[nodiscard]] inline bool guest_exception_registry_diagnostic_admitted() noexcept;

        struct guest_exception_tag_diagnostic
        {
            ::uwvm2::utils::container::u8string_view module_name{};
            ::std::size_t module_id{}, public_tag_index{}, type_index{};
            bool found{};
        };

        [[nodiscard]] inline guest_exception_tag_diagnostic resolve_guest_exception_tag_diagnostic(
            ::uwvm2::runtime::exception::value const& instance) noexcept
        {
            // The fatal host boundary still owns its REAL outer entry lease.
            // A retained tag identity is comparison DATA only; never cast or
            // dereference it, and never match a tag by its structural signature.
            if(!guest_exception_registry_diagnostic_admitted()) { return {}; }
            auto const identity{instance.tag_identity()};
            if(identity == nullptr) { return {}; }
            for(::std::size_t module_id{}; module_id != g_runtime.modules.size(); ++module_id)
            {
                // [actual generation-pinned complete module records ... end)
                // [safe] module_id<size BEFORE the complete record is borrowed.
                auto const& record{g_runtime.modules.index_unchecked(module_id)};
                auto const* module{record.runtime_module};
                if(module == nullptr) { continue; }
                auto const imports{module->imported_tag_vec_storage.size()};
                for(::std::size_t local{}; local != module->local_defined_tag_vec_storage.size(); ++local)
                {
                    // [actual module-owned complete local tags ... end)
                    // [safe] local<size BEFORE borrowing this real tag record;
                    // the execution lease pins module/name/identity lifetimes.
                    auto const& tag{module->local_defined_tag_vec_storage.index_unchecked(local)};
                    if(!tag.exception_identity || tag.exception_identity.get() != identity) { continue; }
                    if(local > ::std::numeric_limits<::std::size_t>::max() - imports) { return {}; }
                    // Exact genuine member equality precedes metadata reads.
                    // Imported aliases retain this same provider identity: show
                    // its actual defining module/public index, not a guessed alias.
                    return {record.module_name, module_id, imports + local, tag.type_index, true};
                }
            }
            // A retained exception may outlive its original registry instance.
            // Never substitute a new-world/cache index or dereference that token.
            return {};
        }

        [[noreturn]] UWVM_NOINLINE inline void uncaught_guest_exception_fatal(
            ::uwvm2::utils::container::u8string_view module_name, ::std::size_t entry_function_index,
            ::uwvm2::runtime::exception::guest_exception const& caught) noexcept
        {
            // This catch owns the immutable value/trace after all callee cleanup. Never reconstruct
            // the original throwing stack from the already-unwound TLS stack or reclassify it as a trap.
            // Serialize the entire report; simultaneous failing host threads cannot interleave its lines.
            {
                auto output_ref{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                ::fast_io::operations::decay::stream_ref_decay_lock_guard lock{
                    ::fast_io::operations::decay::output_stream_mutex_ref_decay(output_ref)};
                auto output{::fast_io::operations::decay::output_stream_unlocked_ref_decay(output_ref)};
                auto const& instance{*caught.instance()};
                print_guest_exception_prefix(output, true);
                ::fast_io::io::print(output, u8"Uncaught WebAssembly exception (module=",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW));
                print_guest_exception_quoted_name(output, module_name);
                ::fast_io::io::print(output,
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE), u8", entry_func_idx=",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW), entry_function_index,
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE), u8", payload_fields=",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW), instance.fields().size(),
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE), u8").\n",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                auto const tag{resolve_guest_exception_tag_diagnostic(instance)};
                print_guest_exception_prefix(output, false);
                if(tag.found)
                {
                    ::fast_io::io::print(output, u8"  tag module_id=", tag.module_id, u8" module=",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW));
                    print_guest_exception_quoted_name(output, tag.module_name);
                    ::fast_io::io::print(output,
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE), u8" tag_idx=",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW), tag.public_tag_index,
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE), u8" type_idx=",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW), tag.type_index);
                }
                else { ::fast_io::io::print(output, u8"  tag metadata unavailable (no current admitted defining instance)"); }
                ::fast_io::io::print(output,
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL), u8"\n");
                ::std::size_t payload_index{};
                for(auto const& field: instance.fields())
                {
                    print_guest_exception_prefix(output, false);
                    ::fast_io::io::print(output, u8"  payload[", payload_index++, u8"] ",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW));
                    print_guest_exception_payload(output, field);
                    ::fast_io::io::print(output,
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL), u8"\n");
                }
                auto const& trace{instance.diagnostic()};
                print_guest_exception_prefix(output, false);
                if(trace && trace->frame_count() != 0uz)
                {
                    ::fast_io::io::print(output,
                        u8"Wasm call stack captured at throw (most recent first; instruction/source locations unavailable):\n",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    auto const frame_count{trace->frame_count()};
                    for(::std::size_t frame_index{}; frame_index != frame_count; ++frame_index)
                    {
                        ::uwvm2::runtime::exception::diagnostic_frame_index frame{};
                        ::uwvm2::runtime::exception::diagnostic_frame_names names{};
                        // [retained immutable trace: ordinal<frame_count] end
                        // [safe] frame_at checks the real compact/expanded bound
                        // BEFORE returning borrowed names; this trace stays owned
                        // throughout synchronous output. No cold name copies.
                        if(!trace->frame_at(frame_index, frame, names))
                        {
                            print_guest_exception_prefix(output, false);
                            ::fast_io::io::print(output, u8"#", frame_index, u8" throw-site frame unavailable\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            continue;
                        }
                        print_guest_exception_prefix(output, false);
                        ::fast_io::io::print(output, u8"#", frame_index, u8" module_id=", frame.module_id, u8" module=",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW));
                        print_guest_exception_quoted_name(output, names.module_name);
                        ::fast_io::io::print(output,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE), u8" func_idx=",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW), frame.function_index);
                        if(!names.function_name.empty())
                        {
                            ::fast_io::io::print(output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE), u8" func_name=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW));
                            print_guest_exception_quoted_name(output, names.function_name);
                        }
                        ::fast_io::io::print(output,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL), u8"\n");
                    }
                }
                else
                {
                    ::fast_io::io::print(output, u8"Wasm call stack at throw unavailable (no snapshot captured).\n",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                }
                if(trace && trace->truncated())
                {
                    print_guest_exception_prefix(output, false);
                    ::fast_io::io::print(output, u8"Throw-site stack was truncated; older frames are unavailable.\n",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                }
                ::fast_io::io::print(output, u8"\n");
            }
            ::fast_io::fast_terminate();
        }
#endif

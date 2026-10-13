// SOURCE-only native component extension. No actual VM/GC/source admission.
    struct cache_registry
    {
        ::std::array<e::payload_kind, p::max_payload_fields> kinds{};
        ::std::array<::std::shared_ptr<s::tag_instance_identity const>, 4uz> identities{};
        p::prepared_registry::owner registry{};
        cache_registry(fixture_registry const& original)
        {
            for(::std::size_t index{}; index != kinds.size(); ++index)
            { kinds[index] = original.kinds[index % original.kinds.size()]; }
            for(auto& id : identities) { id = ::std::make_shared<s::tag_instance_identity>(); }
            identities[1] = identities[0];
            ::std::array<p::admitted_tag, 4uz> tags{{
                {identities[0], kinds}, {identities[1], kinds},
                {identities[2], {}}, {identities[3], {kinds.data(), 1uz}}}};
            registry = p::prepared_registry::prepare(tags, {});
            CHECK(p::prepared_registry::canonical(registry));
        }
    };
    struct cache_tuple
    {
        ::std::array<::std::byte, p::max_payload_fields * 16uz> bytes{};
        ::std::size_t used{};
        cache_tuple(cache_registry const& f, ::std::array<::std::byte, tuple_bytes> const& original)
        {
            ::std::size_t within{};
            for(::std::size_t index{}; index != f.kinds.size(); ++index)
            {
                if(index % 5uz == 0uz) { within = 0uz; }
                auto const width{e::payload_width(f.kinds[index])};
                CHECK(width <= bytes.size() - used && width <= original.size() - within);
                // [owned 1024-byte tuple][used,used+width) from original raw bits]
                // [safe ] both complete source and destination extents checked.
                ::fast_io::freestanding::my_memcpy(bytes.data() + used, original.data() + within, width);
                used += width;
                within += width;
            }
            ::std::size_t expected{}; for(auto kind : f.kinds) { expected += e::payload_width(kind); } CHECK(used == expected);
        }
    };
    void assert_cached_bits(p::pending_context const& context, cache_registry const& f, cache_tuple const& input)
    {
        p::numeric_leaf_state view{};
        CHECK(context.borrow_numeric_state(view) == p::status::ok);
        CHECK(view.signature.size() == p::max_payload_fields && view.fields.size() == p::max_payload_fields);
        CHECK(view.exact_numeric_bytes == input.used);
        e::instance_root const empty{};
        ::std::size_t copied{};
        for(::std::size_t index{}; index != view.fields.size(); ++index)
        {
            auto const& field{view.fields[index]};
            auto const bytes{field.bits()};
            CHECK(field.kind() == f.kinds[index]);
            CHECK(!field.root().owner_before(empty) && !empty.owner_before(field.root()));
            CHECK(bytes.size() <= input.used - copied);
            for(::std::size_t byte{}; byte != bytes.size(); ++byte) { CHECK(bytes[byte] == input.bytes[copied + byte]); }
            copied += bytes.size();
        }
        CHECK(copied == input.used);
    }
    void numeric_cache_lifecycle(fixture_registry const& original, ::std::array<::std::byte, tuple_bytes> const& source)
    {
        cache_registry f{original};
        cache_tuple input{f, source};
        p::native_island_chain chain{};
        p::pending_context context{f.registry};
        p::execution_island island{chain, context}; CHECK(island.admission() == p::status::ok);
        b::numeric_header header{context}; CHECK(header.activated_on_owner());
        auto const h{address(::std::addressof(header))};
        ::std::array<::std::byte, p::max_payload_fields * 16uz> output{};
        output.fill(::std::byte{0xae});
        auto const untouched{output};
        // Failed short/long source extents do not mint or consume pending proof.
        CHECK(b::uwvm2_pending_numeric_publish_r2(h, 0u, address(input.bytes.data()), input.used - 1uz) == b::result(p::status::invalid_signature));
        CHECK(!context.has_pending() && !header.pending_on_owner());
        CHECK(b::uwvm2_pending_numeric_publish_r2(h, 0u, address(input.bytes.data()), input.used + 1uz) == b::result(p::status::invalid_signature));
        CHECK(!context.has_pending());
        // Real generic publication must reject wrong kind before any move,
        // even though f32 and i32 have the same width and actual EMPTY owners.
        auto wrong{e::payload_field::numeric(e::payload_kind::f32, {source.data(), 4uz})};
        CHECK(wrong);
        auto validator{[](::std::size_t, ::std::size_t, p::reference) noexcept { return false; }};
        CHECK(context.publish_fresh(3uz, {::std::addressof(*wrong), 1uz}, validator) == p::status::invalid_signature);
        CHECK(wrong->kind() == e::payload_kind::f32 && !context.has_pending());
        CHECK(b::uwvm2_pending_numeric_take_r3(h, 99u, address(output.data()), input.used) == b::result(p::status::no_pending));
        CHECK(output == untouched);
        for(unsigned repetition{}; repetition != 3u; ++repetition)
        {
            CHECK(b::uwvm2_pending_numeric_publish_r2(h, 0u, address(input.bytes.data()), input.used) == b::result(p::status::ok));
            CHECK(b::uwvm2_pending_numeric_publish_r2(h, 3u, address(source.data()), 4u) == b::result(p::status::busy));
            for(::std::size_t frame{}; frame != 70uz; ++frame)
            { CHECK(b::uwvm2_pending_numeric_append_exit_r2(h, 5u, frame) == b::result(p::status::ok)); }
            assert_cached_bits(context, f, input);
            p::numeric_leaf_state view{}; CHECK(context.borrow_numeric_state(view) == p::status::ok);
            CHECK(view.retired_frames.size() == p::max_retired_frames && view.truncated);
            // Same native output sentinel and full pending/trace survive errors.
            CHECK(b::uwvm2_pending_numeric_take_r3(h, 3u, address(output.data()), input.used) == b::result(p::status::no_match));
            CHECK(b::uwvm2_pending_numeric_take_r3(h, 0u, address(output.data()), input.used - 1uz) == b::result(p::status::invalid_signature));
            CHECK(b::uwvm2_pending_numeric_take_r3(h, 0u, h, input.used) == b::result(p::status::invalid_reference));
            CHECK(output == untouched && header.pending_on_owner());
            assert_cached_bits(context, f, input);
            view = {}; // Retire the read borrow before the actual clear leaf.
            CHECK(b::uwvm2_pending_numeric_take_r3(h, 1u, address(output.data()), input.used) == b::result(p::status::ok));
            for(::std::size_t byte{}; byte != output.size(); ++byte)
            { CHECK(output[byte] == (byte < input.used ? input.bytes[byte] : ::std::byte{0xae})); }
            output = untouched;
            CHECK(!context.has_pending() && !header.pending_on_owner());
            CHECK(context.borrow_numeric_state(view) == p::status::no_pending);
            // New shorter prefix must hide all old 64-field bits and trace.
            CHECK(b::uwvm2_pending_numeric_publish_r2(h, 3u, address(source.data()), 4u) == b::result(p::status::ok));
            CHECK(context.borrow_numeric_state(view) == p::status::ok);
            CHECK(view.fields.size() == 1uz && view.exact_numeric_bytes == 4uz && view.retired_frames.empty() && !view.truncated);
            view = {}; // No prefix borrow crosses clear or next publication.
            CHECK(b::uwvm2_pending_numeric_take_r3(h, 3u, address(output.data()), 4u) == b::result(p::status::ok));
            output = untouched;
            CHECK(b::uwvm2_pending_numeric_publish_r2(h, 2u, 0u, 0u) == b::result(p::status::ok));
            CHECK(context.borrow_numeric_state(view) == p::status::ok && view.fields.empty() && view.exact_numeric_bytes == 0uz);
            view = {}; // Empty read view is also retired at the leaf boundary.
            CHECK(b::uwvm2_pending_numeric_take_r3(h, 2u, 0u, 0u) == b::result(p::status::ok));
        }
        // A real C++ builder throw leaves all pending fields and header state.
        CHECK(b::uwvm2_pending_numeric_publish_r2(h, 0u, address(input.bytes.data()), input.used) == b::result(p::status::ok));
        struct builder_failure {};
        {
            p::caught_payload target{f.registry};
            p::caught_root_scope caught{island, target}; CHECK(caught.admission() == p::status::ok);
            try
            {
                auto const result{header.materialize_in_registered(target,
                    [](auto, bool) -> e::diagnostic_trace_ref { throw builder_failure{}; })};
                (void)result;
                CHECK(false);
            }
            catch(builder_failure const&) { CHECK(true); }
            CHECK(header.pending_on_owner() && !target.observable());
            assert_cached_bits(context, f, input);
            CHECK(header.materialize_in_registered(target, [](auto, bool) { return e::diagnostic_trace_ref{}; }) == p::status::ok);
            CHECK(!context.has_pending() && !header.pending_on_owner());
            CHECK(target.observable() && target.observable()->fields().size() == p::max_payload_fields);
            ::std::size_t copied{};
            for(auto const& field : target.observable()->fields())
            {
                for(auto bit : field.bits()) { CHECK(bit == input.bytes[copied]); ++copied; }
            }
            CHECK(copied == input.used);
        }
        CHECK(p::prepared_registry::canonical(f.registry));
    }
    void numeric_cache_transfer_and_unwind(fixture_registry const& original, ::std::array<::std::byte, tuple_bytes> const& source)
    {
        cache_registry f{original};
        cache_tuple input{f, source};
        p::native_island_chain chain{};
        p::pending_context context{f.registry};
        // Direct context handoff is tested without a second hidden-header phase.
        {
            p::execution_island island{chain, context}; CHECK(island.admission() == p::status::ok);
            ::std::array<e::payload_field, p::max_payload_fields> fields{};
            ::std::size_t used{};
            for(::std::size_t index{}; index != fields.size(); ++index)
            {
                auto const width{e::payload_width(f.kinds[index])};
                CHECK(width <= input.used - used);
                // [actual owned 504-byte prefix][used,used+width) complete field]
                // [safe ] checked source extent before pointer advance/read.
                auto field{e::payload_field::numeric(f.kinds[index], {input.bytes.data() + used, width})};
                CHECK(field); fields[index] = ::std::move(*field); used += width;
            }
            p::caught_payload target{f.registry};
            p::caught_root_scope scope{island, target}; CHECK(scope.admission() == p::status::ok);
            CHECK(context.publish_fresh(0uz, fields,
                [](::std::size_t, ::std::size_t, p::reference) noexcept { return false; }) == p::status::ok);
            CHECK(context.transfer_matching(1uz, target) == p::status::ok);
            CHECK(!context.has_pending() && target.fields().size() == p::max_payload_fields);
            ::std::size_t copied{};
            for(auto const& field : target.fields())
            { for(auto bit : field.bits()) { CHECK(bit == input.bytes[copied]); ++copied; } }
            CHECK(copied == input.used);
        }
        // Genuine foreign C++ unwinding must retire pending/cache/trace before
        // unlinking the real island. Never clear private state from this test.
        try
        {
            p::execution_island island{chain, context}; CHECK(island.admission() == p::status::ok);
            b::numeric_header header{context}; CHECK(header.activated_on_owner());
            auto const h{address(::std::addressof(header))};
            CHECK(b::uwvm2_pending_numeric_publish_r2(h, 3u, address(source.data()), 4u) == b::result(p::status::ok));
            throw 7;
        }
        catch(int value) { CHECK(value == 7); }
        CHECK(chain.quiescent_head() == nullptr && !context.has_pending());
        p::numeric_leaf_state inactive{};
        CHECK(context.borrow_numeric_state(inactive) == p::status::inactive);
        {
            p::execution_island island{chain, context}; CHECK(island.admission() == p::status::ok);
            b::numeric_header header{context}; CHECK(header.activated_on_owner());
            auto const h{address(::std::addressof(header))};
            CHECK(b::uwvm2_pending_numeric_publish_r2(h, 0u, address(input.bytes.data()), input.used) == b::result(p::status::ok));
            assert_cached_bits(context, f, input);
            CHECK(b::uwvm2_pending_numeric_clear_r2(h) == b::result(p::status::ok));
            CHECK(!context.has_pending() && !header.pending_on_owner());
        }
    }

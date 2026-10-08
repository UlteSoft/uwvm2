        // Real canonical arrays under actual entry admission. Independent
        // expected integer bits include packed negatives and an f32 sNaN.
        {
            for(auto type : {0u, 1u, 2u, 4u, 7u})
            {
                auto const input{type == 7u ? value::i32(0x80000001u) : seed[type]};
                auto const expected{normalized(input, type).as<::std::uint32_t>()};
                ref scalar{};
                remember(store->array_new(type, input, 3uz, scalar), scalar);
                ARRAY_CHECK(store->template array_get32<false>(scalar, 1uz) == expected);
                auto const signed_expected{type == 0u ? 0xffffff80u :
                    type == 1u ? 0xffff80f1u : expected};
                ARRAY_CHECK(store->template array_get32<true>(scalar, 1uz) == signed_expected);
                ARRAY_CHECK(store->template array_get32<false>(scalar, 3uz) ==
                    (static_cast<::std::uint64_t>(status::out_of_bounds) << 32u));
                ARRAY_CHECK(store->template array_get32<false>(scalar, (~::std::size_t{})) ==
                    (static_cast<::std::uint64_t>(status::out_of_bounds) << 32u));
                if(type != 7u)
                {
                    auto const changed{value::i32(0x8000007fu)};
                    ARRAY_CHECK(store->array_set(scalar, 1uz, changed) == status::ok);
                    auto const raw{type == 0u || type == 1u ? 0x7fu : 0x8000007fu};
                    ARRAY_CHECK(store->template array_get32<false>(scalar, 1uz) == raw);
                    ARRAY_CHECK(observe(store, scalar, 1uz).as<::std::uint32_t>() == raw);
                }
                else
                {
                    ARRAY_CHECK(store->array_set(scalar, 1uz, value{}) == status::immutable_field);
                    ARRAY_CHECK(store->template array_get32<false>(scalar, 1uz) == expected);
                }
            }
            ARRAY_CHECK(store->template array_get32<false>(bytes, 0uz) == 128u);
            ARRAY_CHECK(store->template array_get32<true>(bytes, 0uz) == 0xffffff80u);
            ARRAY_CHECK(store->template array_get32<false>(halves, 0uz) == 0x80f1u);
            ARRAY_CHECK(store->template array_get32<true>(halves, 0uz) == 0xffff80f1u);
            // Actual unsupported scalar result layouts must decline rather
            // than read a reference/v128/f64/i64 prefix through this ABI.
            for(auto type : {3u, 5u, 6u, 8u})
            {
                ref other{};
                remember(store->array_new_default(type, 1uz, other), other);
                ARRAY_CHECK(store->template array_get32<false>(other, 0uz) ==
                    (static_cast<::std::uint64_t>(status::invalid_type) << 32u));
            }
            ref empty{}, structure{}, null{};
            remember(store->array_new_default(2u, 0uz, empty), empty);
            remember(store->struct_new_default(11u, structure), structure);
            null.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_null;
            ARRAY_CHECK(store->template array_get32<false>(empty, 0uz) ==
                (static_cast<::std::uint64_t>(status::out_of_bounds) << 32u));
            ARRAY_CHECK(store->template array_get32<false>(structure, 0uz) ==
                (static_cast<::std::uint64_t>(status::invalid_reference) << 32u));
            ARRAY_CHECK(store->template array_get32<false>(null, 0uz) ==
                (static_cast<::std::uint64_t>(status::null_reference) << 32u));
        }

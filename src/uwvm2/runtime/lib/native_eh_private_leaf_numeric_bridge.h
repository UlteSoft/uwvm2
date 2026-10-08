// Included only under the independent exact-1 private-leaf prototype gate,
// in the original runtime namespace. No public interpreter/CLI path selects it.
extern "C++" [[noreturn]] void details::llvm_jit_throw_numeric_private_leaf_no_trace_wide_r1(
    ::std::uintptr_t module_id, ::std::uintptr_t tag_index,
    ::std::uintptr_t buffer_address, ::std::uintptr_t byte_count) UWVM_THROWS
{
#ifdef UWVM_CPP_EXCEPTIONS
    // This synchronous ABI has the same checked native tuple contract as the
    // ordinary numeric bridge. Selection requires separate actual publication;
    // neither a guest bit nor an observed handler turns it into permission.
    static_assert(sizeof(::std::uintptr_t) == sizeof(::std::size_t));
    auto const bytes{static_cast<::std::size_t>(byte_count)};
    auto const& tag{native_exception_tag(module_id, tag_index)};
    if(bytes != native_exception_numeric_bytes(tag) || (bytes != 0uz && buffer_address == 0u)) [[unlikely]]
    { ::fast_io::fast_terminate(); }
    auto const& parameters{tag.function_type_ptr->parameter};
    auto const count{parameters.begin == parameters.end ? 0uz : static_cast<::std::size_t>(parameters.end - parameters.begin)};
    // [actual compiler-initialized native tuple: bytes] end
    // [safe] Borrow only the genuine synchronous bridge allocation; no guest
    // integer pointer is admitted by a public VM interface. This conversion
    // changes representation only; the complete checked extent is unchanged.
    ::std::span<::std::byte const> tuple{reinterpret_cast<::std::byte const*>(buffer_address), bytes};
    ::std::vector<::uwvm2::runtime::exception::payload_field> fields{};
    fields.reserve(count);
    ::std::size_t offset{};
    for(::std::size_t index{}; index != count; ++index)
    {
        // [retained actual tag parameters][index < count] end
        // [safe] Same original numeric signature determines every field width.
        auto const kind{native_exception_numeric_kind(parameters.begin[index])};
        auto const width{::uwvm2::runtime::exception::payload_width(kind)};
        // [copied prefix: offset][complete width][remaining bytes] end
        // [safe] The original full-signature sum was checked before any read.
        auto field{::uwvm2::runtime::exception::payload_field::numeric(kind, tuple.subspan(offset, width))};
        if(!field) [[unlikely]] { ::fast_io::fast_terminate(); }
        fields.push_back(::std::move(*field));
        offset += width; // Integer tuple cursor advances over this full field only.
    }
    // Only diagnostic capture differs. Payload/source ownership, native throw,
    // recipient routing, retirement and allocation-failure policy stay original.
    ::uwvm2::runtime::exception::diagnostic_trace_ref trace{};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && \
    defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
    auto value{source_exception_guest_bridge::try_publish_owned(module_id, tag_index, ::std::move(fields), trace)};
    if(!value) { value = ::uwvm2::runtime::exception::value::make_owned(tag.exception_identity, ::std::move(fields), ::std::move(trace)); }
#else
    auto value{::uwvm2::runtime::exception::value::make_owned(tag.exception_identity, ::std::move(fields), ::std::move(trace))};
#endif
    if(!value) [[unlikely]] { ::fast_io::fast_terminate(); }
    ::uwvm2::runtime::exception::throw_value(::std::move(value));
#else
    static_cast<void>(module_id); static_cast<void>(tag_index); static_cast<void>(buffer_address); static_cast<void>(byte_count);
    ::fast_io::fast_terminate();
#endif
}

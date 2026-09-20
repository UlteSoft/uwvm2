#pragma once

/**
 * @file
 * @brief Provides type-deducing factories for transcoder stream adapters.
 *
 * Factories select borrowed versus owned handle storage from the argument's
 * lifetime category. Public character inference uses an integral engine
 * endpoint when available; a std::byte endpoint inherits the underlying stream
 * character type unless the caller explicitly supplies PublicChar.
 */

namespace fast_io
{

namespace details
{

/** @brief Keeps an explicitly requested public output character type. */
template <typename requested_public_char, typename source_unit,
		  typename underlying_char>
struct otranscoder_public_char_selector
{
	using type = requested_public_char;
};

/** @brief Infers output characters from an integral source or the stream type. */
template <typename source_unit, typename underlying_char>
struct otranscoder_public_char_selector<void, source_unit, underlying_char>
{
	using type = ::std::conditional_t<::std::integral<source_unit>, source_unit,
									  underlying_char>;
};

/** @brief Keeps an explicitly requested public input character type. */
template <typename requested_public_char, typename transformed_unit,
		  typename underlying_char>
struct itranscoder_public_char_selector
{
	using type = requested_public_char;
};

/** @brief Infers input characters from an integral target or the stream type. */
template <typename transformed_unit, typename underlying_char>
struct itranscoder_public_char_selector<void, transformed_unit,
										underlying_char>
{
	using type = ::std::conditional_t<::std::integral<transformed_unit>,
									  transformed_unit, underlying_char>;
};

} // namespace details

/**
 * @brief Creates an output transcoder with lifetime-safe handle storage.
 *
 * The public domain is the engine source. Integral sources expose their unit
 * type directly; byte sources default to the underlying output character type.
 */
template <typename requested_public_char = void, typename handle,
		  typename engine>
	requires((::std::same_as<requested_public_char, void> ||
			  ::std::integral<requested_public_char>) &&
			 ::fast_io::transcoder<::std::remove_cvref_t<engine>> &&
			 ::std::constructible_from<::std::remove_cvref_t<engine>, engine &&>)
inline auto make_otranscoder(handle &&output, engine &&transcode_engine)
{
	// The engine source is the adapter's public output domain. The engine target
	// is independently checked against the normalized underlying output ref by
	// basic_otranscoder.
	using engine_type = ::std::remove_cvref_t<engine>;
	using engine_ref_type =
		::fast_io::operations::transcode_engine_ref_t<engine_type>;
	using handle_storage_type = decltype(::fast_io::details::make_otranscoder_handle_storage(
		::std::forward<handle>(output)));
	using normalized_output_ref_type = ::std::remove_cvref_t<decltype(::fast_io::operations::output_stream_ref(
		::std::declval<handle_storage_type &>().get()))>;
	using public_char_type = typename ::fast_io::details::
		otranscoder_public_char_selector<
			requested_public_char,
			::fast_io::transcode_from_value_t<engine_ref_type>,
			typename normalized_output_ref_type::output_char_type>::type;
	using adapter_type = ::fast_io::basic_otranscoder<
		public_char_type, handle_storage_type, engine_type>;
	return adapter_type{
		::fast_io::details::make_otranscoder_handle_storage(
			::std::forward<handle>(output)),
		engine_type(::std::forward<engine>(transcode_engine))};
}

/**
 * @brief Creates an input transcoder with lifetime-safe handle storage.
 *
 * The public domain is the engine target. Integral targets expose their unit
 * type directly; byte targets default to the underlying input character type.
 */
template <typename requested_public_char = void, typename handle,
		  typename engine>
	requires((::std::same_as<requested_public_char, void> ||
			  ::std::integral<requested_public_char>) &&
			 ::fast_io::transcoder<::std::remove_cvref_t<engine>> &&
			 ::std::constructible_from<::std::remove_cvref_t<engine>, engine &&>)
inline auto make_itranscoder(handle &&input, engine &&transcode_engine)
{
	// Input direction reverses that relation: the underlying stream feeds the
	// engine source and the engine target defines the public decoded domain.
	using engine_type = ::std::remove_cvref_t<engine>;
	using engine_ref_type =
		::fast_io::operations::transcode_engine_ref_t<engine_type>;
	using handle_storage_type = decltype(::fast_io::details::make_itranscoder_handle_storage(
		::std::forward<handle>(input)));
	using normalized_input_ref_type = ::std::remove_cvref_t<decltype(::fast_io::operations::input_stream_ref(
		::std::declval<handle_storage_type &>().get()))>;
	using public_char_type = typename ::fast_io::details::
		itranscoder_public_char_selector<
			requested_public_char,
			::fast_io::transcode_to_value_t<engine_ref_type>,
			typename normalized_input_ref_type::input_char_type>::type;
	using adapter_type = ::fast_io::basic_itranscoder<
		public_char_type, handle_storage_type, engine_type>;
	return adapter_type{
		::fast_io::details::make_itranscoder_handle_storage(
			::std::forward<handle>(input)),
		engine_type(::std::forward<engine>(transcode_engine))};
}

/**
 * @brief Creates a duplex transcoder with independent input and output engines.
 *
 * Both child adapters borrow one parent-owned handle. Input refill is tied to
 * output sync-flush, while terminal finish remains direction-specific.
 */
template <typename requested_input_char = void,
		  typename requested_output_char = void, typename handle,
		  typename input_engine, typename output_engine>
	requires((::std::same_as<requested_input_char, void> ||
			  ::std::integral<requested_input_char>) &&
			 (::std::same_as<requested_output_char, void> ||
			  ::std::integral<requested_output_char>) &&
			 ::fast_io::transcoder<::std::remove_cvref_t<input_engine>> &&
			 ::fast_io::transcoder<::std::remove_cvref_t<output_engine>> &&
			 ::std::constructible_from<::std::remove_cvref_t<input_engine>,
									   input_engine &&> &&
			 ::std::constructible_from<::std::remove_cvref_t<output_engine>,
									   output_engine &&>)
inline auto make_iotranscoder(handle &&io, input_engine &&input_transcode_engine,
							  output_engine &&output_transcode_engine)
{
	// Both directional character domains are inferred independently before the
	// parent constructs two child adapters over one shared handle storage.
	using input_engine_type = ::std::remove_cvref_t<input_engine>;
	using output_engine_type = ::std::remove_cvref_t<output_engine>;
	using input_engine_ref_type =
		::fast_io::operations::transcode_engine_ref_t<input_engine_type>;
	using output_engine_ref_type =
		::fast_io::operations::transcode_engine_ref_t<output_engine_type>;
	using handle_storage_type = decltype(::fast_io::details::make_itranscoder_handle_storage(
		::std::forward<handle>(io)));
	using normalized_input_ref_type = ::std::remove_cvref_t<decltype(::fast_io::operations::input_stream_ref(
		::std::declval<handle_storage_type &>().get()))>;
	using normalized_output_ref_type = ::std::remove_cvref_t<decltype(::fast_io::operations::output_stream_ref(
		::std::declval<handle_storage_type &>().get()))>;
	using input_char_type = typename ::fast_io::details::
		itranscoder_public_char_selector<
			requested_input_char,
			::fast_io::transcode_to_value_t<input_engine_ref_type>,
			typename normalized_input_ref_type::input_char_type>::type;
	using output_char_type = typename ::fast_io::details::
		otranscoder_public_char_selector<
			requested_output_char,
			::fast_io::transcode_from_value_t<output_engine_ref_type>,
			typename normalized_output_ref_type::output_char_type>::type;
	using adapter_type = ::fast_io::basic_iotranscoder<
		input_char_type, output_char_type, handle_storage_type,
		input_engine_type, output_engine_type>;
	return adapter_type{
		::fast_io::details::make_itranscoder_handle_storage(
			::std::forward<handle>(io)),
		input_engine_type(
			::std::forward<input_engine>(input_transcode_engine)),
		output_engine_type(
			::std::forward<output_engine>(output_transcode_engine))};
}

} // namespace fast_io

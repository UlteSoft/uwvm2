#pragma once

/**
 * @file
 * @brief Provides exact CPO recognition for the bounded transform protocol.
 *
 * The generic `transcoder_ref<T>` overloads forward to CPOs on `T&`. A provider
 * that needs a different observer can instead customize `transcode_ref_define`
 * and place the process/drain CPOs on that observer type.
 */

namespace fast_io
{

/** @brief Obtains the source unit type advertised by an engine observer. */
template <typename T>
using transcode_from_value_t = typename ::std::remove_cvref_t<T>::from_value_type;

/** @brief Obtains the destination unit type advertised by an engine observer. */
template <typename T>
using transcode_to_value_t = typename ::std::remove_cvref_t<T>::to_value_type;

/** @brief Forwards a bounded process step through the default engine observer. */
template <typename T>
	requires requires(T &engine, typename T::from_value_type const *from,
					  typename T::to_value_type *to) {
		{ transcode_process_define(engine, from, from, to, to) } -> ::std::same_as<::fast_io::basic_transcode_process_result<
			typename T::from_value_type, typename T::to_value_type>>;
	}
inline constexpr auto transcode_process_define(
	::fast_io::transcoder_ref<T> ref,
	typename T::from_value_type const *from_first,
	typename T::from_value_type const *from_last,
	typename T::to_value_type *to_first,
	typename T::to_value_type *to_last) noexcept(noexcept(transcode_process_define(*ref.ptr, from_first, from_last, to_first, to_last)))
{
	return transcode_process_define(
		*ref.ptr, from_first, from_last, to_first, to_last);
}

/** @brief Forwards a nonterminal sync-flush step through the default observer. */
template <typename T>
	requires requires(typename T::to_value_type *to) {
		{ transcode_sync_flush_define(::std::declval<T &>(), to, to) } -> ::std::same_as<::fast_io::basic_transcode_drain_result<
			typename T::to_value_type>>;
	}
inline constexpr auto transcode_sync_flush_define(
	::fast_io::transcoder_ref<T> ref,
	typename T::to_value_type *to_first,
	typename T::to_value_type *to_last) noexcept(noexcept(transcode_sync_flush_define(*ref.ptr, to_first, to_last)))
{
	return transcode_sync_flush_define(*ref.ptr, to_first, to_last);
}

/** @brief Forwards a terminal finish step through the default engine observer. */
template <typename T>
	requires requires(typename T::to_value_type *to) {
		{ transcode_finish_define(::std::declval<T &>(), to, to) } -> ::std::same_as<::fast_io::basic_transcode_drain_result<
			typename T::to_value_type>>;
	}
inline constexpr auto transcode_finish_define(
	::fast_io::transcoder_ref<T> ref,
	typename T::to_value_type *to_first,
	typename T::to_value_type *to_last) noexcept(noexcept(transcode_finish_define(*ref.ptr, to_first, to_last)))
{
	return transcode_finish_define(*ref.ptr, to_first, to_last);
}

/** @brief Forwards the minimum-capacity query from an observer to its engine. */
template <typename T>
	requires requires(::fast_io::transcode_phase phase) {
		{ transcode_min_output_size_define(
			::fast_io::transcode_reserve<T>, phase) } -> ::std::same_as<::std::size_t>;
	}
inline constexpr ::std::size_t transcode_min_output_size_define(
	::fast_io::transcode_reserve_t<::fast_io::transcoder_ref<T>>,
	::fast_io::transcode_phase phase) noexcept(noexcept(transcode_min_output_size_define(::fast_io::transcode_reserve<T>, phase)))
{
	return transcode_min_output_size_define(
		::fast_io::transcode_reserve<T>, phase);
}

/** @brief Forwards the maximum-output query from an observer to its engine. */
template <typename T>
	requires requires(::std::size_t input_units,
					  ::fast_io::transcode_phase phase) {
		{ transcode_max_output_size_define(
			::fast_io::transcode_reserve<T>, input_units, phase) } -> ::std::same_as<::std::size_t>;
	}
inline constexpr ::std::size_t transcode_max_output_size_define(
	::fast_io::transcode_reserve_t<::fast_io::transcoder_ref<T>>,
	::std::size_t input_units, ::fast_io::transcode_phase phase) noexcept(noexcept(transcode_max_output_size_define(::fast_io::transcode_reserve<T>, input_units, phase)))
{
	return transcode_max_output_size_define(
		::fast_io::transcode_reserve<T>, input_units, phase);
}

namespace operations::decay::defines
{

/** @brief Requires valid and supported source and destination endpoint units. */
template <typename T>
concept has_transcode_endpoint_types = requires {
	typename ::fast_io::transcode_from_value_t<T>;
	typename ::fast_io::transcode_to_value_t<T>;
	requires ::fast_io::transcode_unit<::fast_io::transcode_from_value_t<T>>;
	requires ::fast_io::transcode_unit<::fast_io::transcode_to_value_t<T>>;
};

/** @brief Detects an exact bounded process CPO with the required result type. */
template <typename T>
concept has_transcode_process_define =
	has_transcode_endpoint_types<T> &&
	requires(T &ref, ::fast_io::transcode_from_value_t<T> const *from,
			 ::fast_io::transcode_to_value_t<T> *to) {
		{ transcode_process_define(ref, from, from, to, to) } -> ::std::same_as<::fast_io::basic_transcode_process_result<
			::fast_io::transcode_from_value_t<T>,
			::fast_io::transcode_to_value_t<T>>>;
	};

/** @brief Detects an exact nonterminal sync-flush CPO. */
template <typename T>
concept has_transcode_sync_flush_define =
	has_transcode_endpoint_types<T> &&
	requires(T &ref, ::fast_io::transcode_to_value_t<T> *to) {
		{ transcode_sync_flush_define(ref, to, to) } -> ::std::same_as<::fast_io::basic_transcode_drain_result<
			::fast_io::transcode_to_value_t<T>>>;
	};

/** @brief Detects the mandatory terminal finish CPO. */
template <typename T>
concept has_transcode_finish_define =
	has_transcode_endpoint_types<T> &&
	requires(T &ref, ::fast_io::transcode_to_value_t<T> *to) {
		{ transcode_finish_define(ref, to, to) } -> ::std::same_as<::fast_io::basic_transcode_drain_result<
			::fast_io::transcode_to_value_t<T>>>;
	};

/** @brief Detects the mandatory minimum-output-capacity query CPO. */
template <typename T>
concept has_transcode_min_output_size_define =
	has_transcode_endpoint_types<T> &&
	requires(::fast_io::transcode_phase phase) {
		{ transcode_min_output_size_define(
			::fast_io::transcode_reserve<::std::remove_cvref_t<T>>, phase) } -> ::std::same_as<::std::size_t>;
	};

/** @brief Detects the optional per-input maximum-output-size query CPO. */
template <typename T>
concept has_transcode_max_output_size_define =
	has_transcode_endpoint_types<T> &&
	requires(::std::size_t input_units,
			 ::fast_io::transcode_phase phase) {
		{ transcode_max_output_size_define(
			::fast_io::transcode_reserve<::std::remove_cvref_t<T>>,
			input_units, phase) } -> ::std::same_as<::std::size_t>;
	};

} // namespace operations::decay::defines

/** @brief Requires the complete mandatory bounded transcoder observer protocol. */
template <typename T>
concept transcode_engine_ref =
	::fast_io::operations::decay::defines::has_transcode_process_define<T> &&
	::fast_io::operations::decay::defines::has_transcode_finish_define<T> &&
	::fast_io::operations::decay::defines::has_transcode_min_output_size_define<T>;

/** @brief Requires an engine whose normalized observer satisfies the protocol. */
template <typename T>
concept transcoder = requires(T &engine) {
	requires ::fast_io::transcode_engine_ref<
		decltype(::fast_io::transcode_ref(engine))>;
};

} // namespace fast_io

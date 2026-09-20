#pragma once

/*
 * Sensitive-buffer clearing policy query (CPO level).
 *
 * These directional markers let a normalized stream require secure clearing of
 * temporary IO storage. They are policy capabilities consumed by buffer-owning
 * algorithms, not clearing operations themselves. The actual allocation,
 * lifetime, and erasure point remain responsibilities of the operation that
 * owns the temporary storage.
 */

namespace fast_io
{

namespace operations::decay::defines
{

template <typename T>
concept has_input_stream_require_secure_clear_define = requires(T t) { input_stream_require_secure_clear_define(t); };

template <typename T>
concept has_output_stream_require_secure_clear_define = requires(T t) { output_stream_require_secure_clear_define(t); };

template <typename T>
concept has_io_stream_require_secure_clear_define = requires(T t) { io_stream_require_secure_clear_define(t); };

template <typename T>
concept has_input_or_io_stream_require_secure_clear_define =
	has_input_stream_require_secure_clear_define<T> || has_io_stream_require_secure_clear_define<T>;

template <typename T>
concept has_output_or_io_stream_require_secure_clear_define =
	has_output_stream_require_secure_clear_define<T> || has_io_stream_require_secure_clear_define<T>;

} // namespace operations::decay::defines

} // namespace fast_io

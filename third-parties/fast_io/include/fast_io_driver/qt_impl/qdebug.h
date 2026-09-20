#pragma once

namespace fast_io
{

template <::std::integral ch_type>
struct basic_qt_qdebug
{
	using char_type = ch_type;
	QDebug pqdbg;
};

using qt_qdebug = basic_qt_qdebug<char>;
using wqt_qdebug = basic_qt_qdebug<wchar_t>;
using u8qt_qdebug = basic_qt_qdebug<char8_t>;
using u16qt_qdebug = basic_qt_qdebug<char16_t>;
using u32qt_qdebug = basic_qt_qdebug<char32_t>;

namespace manipulators
{

/// @brief Wraps `QDebug` as a narrow-character fast_io output sink.
/// @details The `QDebug` handle is copied into the observer; fast_io writes are forwarded as Qt debug data.
inline ::fast_io::qt_qdebug qtdbg(QDebug qdbg)
{
	return {qdbg};
}

/// @brief Wraps `QDebug` as a `wchar_t` fast_io output sink.
/// @details Character conversion follows the adapter's Qt write implementation.
inline ::fast_io::wqt_qdebug wqtdbg(QDebug qdbg)
{
	return {qdbg};
}

/// @brief Wraps `QDebug` as a UTF-8-code-unit fast_io output sink.
/// @details The returned observer owns its copied `QDebug` handle.
inline ::fast_io::u8qt_qdebug u8qtdbg(QDebug qdbg)
{
	return {qdbg};
}

/// @brief Wraps `QDebug` as a UTF-16-code-unit fast_io output sink.
/// @details Writes are forwarded through the Qt adapter without exposing a file descriptor.
inline ::fast_io::u16qt_qdebug u16qtdbg(QDebug qdbg)
{
	return {qdbg};
}

/// @brief Wraps `QDebug` as a UTF-32-code-unit fast_io output sink.
/// @details The adapter performs the required Qt-side conversion during writes.
inline ::fast_io::u32qt_qdebug u32qtdbg(QDebug qdbg)
{
	return {qdbg};
}

} // namespace manipulators

namespace details
{

inline void qtdbg_write_impl(QDebug &qdb, char const *first, char const *last)
{
	qdb << QByteArrayView(first, last);
}

inline void qtdbg_scatter_write_impl(QDebug &qdb, io_scatter_t const *scatters, ::std::size_t n)
{
	for (auto i{scatters}, e{i + n}; i != e; ++i)
	{
		qtdbg_write_impl(qdb, reinterpret_cast<char const *>(i->base),
						 reinterpret_cast<char const *>(i->base) + i->len);
	}
}

} // namespace details

template <::std::integral char_type>
inline void write(basic_qt_qdebug<char_type> qdbg, char_type const *first, char_type const *last)
{
	::fast_io::details::qtdbg_write_impl(qdbg.pqdbg, reinterpret_cast<char const *>(first),
										 reinterpret_cast<char const *>(last));
}

template <::std::integral char_type>
inline void scatter_write(basic_qt_qdebug<char_type> qdbg, io_scatters_t scatters)
{
	::fast_io::details::qtdbg_scatter_write_impl(qdbg.pqdbg, scatters.base, scatters.len);
}

} // namespace fast_io

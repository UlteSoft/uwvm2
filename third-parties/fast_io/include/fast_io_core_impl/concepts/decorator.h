#pragma once

/*
 * Decorator/transformation object vocabulary (CPO level).
 *
 * This file recognizes the legacy byte-decorator shape used to transform
 * ranges incrementally. It describes a provider capability only; directional
 * decorator references, transcoding orchestration, buffer ownership, and
 * device transfer are defined elsewhere. The detailed contract below records
 * the cursor and lifetime obligations that callability cannot prove.
 */

namespace fast_io
{

/// @brief Recognizes the legacy byte-decorator object vocabulary.
/// @details A provider must expose both operations on a named mutable object. `deco_define` consumes a readable input
///          range, while both operations receive the exact output-pointer types shown below and return an exact mutable
///          byte cursor plus completion state. Any returned cursor must designate the supplied output range (including
///          its one-past endpoint), and no input/output pointer may be retained after the call. The concept is currently
///          recognition-only: active decorator filters use their directional transcode protocols instead. A future
///          consumer must prove output mutability and incremental-state ownership before using this legacy shape.
template <typename T>
concept decorator = requires(T t, ::std::byte const *fromfirst, ::std::byte const *fromlast, ::std::byte const *tofirst,
							 ::std::byte const *tolast) {
	{ t.deco_define(fromfirst, fromlast, tofirst, tolast) } -> ::std::same_as<context_print_result<::std::byte *>>;
	{ t.deco_unshift_define(tofirst, tolast) } -> ::std::same_as<context_print_result<::std::byte *>>;
};

} // namespace fast_io

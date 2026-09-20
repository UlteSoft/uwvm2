#pragma once

/*
 * Shared result vocabulary for incremental protocols (CPO/protocol level).
 *
 * `parse_code`, `parse_result`, and `context_print_result` communicate
 * progress between scanner/formatter providers and operation engines. They are
 * protocol results rather than public scenario policy: for example, the scan
 * operation decides when `partial` means refill, terminal failure, `false`, or
 * an exception.
 */

namespace fast_io
{

enum class parse_code : char unsigned
{
	ok = 0,
	end_of_file = 1,
	partial = 2,
	invalid = 3,
	overflow = 4
};

template <typename Iter>
struct parse_result
{
	using iterator = Iter;
	iterator iter;
	parse_code code;
};

template <typename Iter>
struct context_print_result
{
	using iterator = Iter;
	iterator iter;
	bool done;
};

} // namespace fast_io

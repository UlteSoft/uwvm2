#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace fast_io::i18n_module_v1
{

// This header is the sole binary contract shared by locale modules and their
// loader.  The module side contains only immutable standard-layout aggregates;
// no standard-library container, allocator, exception, or ownership operation
// crosses the DSO boundary.  The loader validates the versioned envelope and
// then converts these process-local pointers into its own relative-scatter
// representation while the module is still loaded.

template <typename value_type>
struct basic_scatter
{
	value_type const *base{};
	::std::size_t len{};
};

template <typename char_type>
struct basic_lc_identification
{
	basic_scatter<char_type> name{};
	basic_scatter<char_type> encoding{};
	basic_scatter<char_type> title{};
	basic_scatter<char_type> source{};
	basic_scatter<char_type> address{};
	basic_scatter<char_type> contact{};
	basic_scatter<char_type> email{};
	basic_scatter<char_type> tel{};
	basic_scatter<char_type> fax{};
	basic_scatter<char_type> language{};
	basic_scatter<char_type> territory{};
	basic_scatter<char_type> audience{};
	basic_scatter<char_type> application{};
	basic_scatter<char_type> abbreviation{};
	basic_scatter<char_type> revision{};
	basic_scatter<char_type> date{};
};

template <typename char_type>
struct basic_lc_monetary
{
	basic_scatter<char_type> int_curr_symbol{};
	basic_scatter<char_type> currency_symbol{};
	basic_scatter<char_type> mon_decimal_point{};
	basic_scatter<char_type> mon_thousands_sep{};
	basic_scatter<::std::size_t> mon_grouping{};
	basic_scatter<char_type> positive_sign{};
	basic_scatter<char_type> negative_sign{};
	::std::size_t int_frac_digits{};
	::std::size_t frac_digits{};
	::std::size_t p_cs_precedes{};
	::std::size_t p_sep_by_space{};
	::std::size_t n_cs_precedes{};
	::std::size_t n_sep_by_space{};
	::std::size_t int_p_cs_precedes{};
	::std::size_t int_p_sep_by_space{};
	::std::size_t int_n_cs_precedes{};
	::std::size_t int_n_sep_by_space{};
	::std::size_t p_sign_posn{};
	::std::size_t n_sign_posn{};
	::std::size_t int_p_sign_posn{};
	::std::size_t int_n_sign_posn{};
};

template <typename char_type>
struct basic_lc_numeric
{
	basic_scatter<char_type> decimal_point{};
	basic_scatter<char_type> thousands_sep{};
	basic_scatter<::std::size_t> grouping{};
};

template <typename char_type>
struct basic_lc_time_era
{
	bool direction{};
	::std::int_least64_t offset{};
	::std::int_least64_t start_date_year{};
	::std::uint_least8_t start_date_month{};
	::std::uint_least8_t start_date_day{};
	::std::int_least8_t end_date_special{};
	::std::int_least64_t end_date_year{};
	::std::uint_least8_t end_date_month{};
	::std::uint_least8_t end_date_day{};
	basic_scatter<char_type> era_name{};
	basic_scatter<char_type> era_format{};
	basic_scatter<char_type> era{};
};

template <typename char_type>
struct basic_lc_time
{
	basic_scatter<char_type> abday[7]{};
	basic_scatter<char_type> day[7]{};
	basic_scatter<char_type> abmon[12]{};
	basic_scatter<char_type> ab_alt_mon[12]{};
	basic_scatter<char_type> mon[12]{};
	basic_scatter<char_type> d_t_fmt{};
	basic_scatter<char_type> d_fmt{};
	basic_scatter<char_type> t_fmt{};
	basic_scatter<char_type> t_fmt_ampm{};
	basic_scatter<char_type> date_fmt{};
	basic_scatter<char_type> am_pm[2]{};
	basic_scatter<basic_lc_time_era<char_type>> era{};
	basic_scatter<char_type> era_d_fmt{};
	basic_scatter<char_type> era_d_t_fmt{};
	basic_scatter<char_type> era_t_fmt{};
	basic_scatter<basic_scatter<char_type>> alt_digits{};
	struct
	{
		::std::size_t ndays{7};
		::std::int_least64_t first_day{19971201};
		::std::size_t first_week{4};
	} week{};
	::std::size_t first_weekday{};
	::std::size_t first_workday{};
	::std::size_t cal_direction{};
	basic_scatter<basic_scatter<char_type>> timezone{};
};

template <typename char_type>
struct basic_lc_messages
{
	basic_scatter<char_type> yesexpr{};
	basic_scatter<char_type> noexpr{};
	basic_scatter<char_type> yesstr{};
	basic_scatter<char_type> nostr{};
};

template <typename char_type>
struct basic_lc_paper
{
	::std::uint_least64_t width{};
	::std::uint_least64_t height{};
};

template <typename char_type>
struct basic_lc_telephone
{
	basic_scatter<char_type> tel_int_fmt{};
	basic_scatter<char_type> tel_dom_fmt{};
	basic_scatter<char_type> int_select{};
	basic_scatter<char_type> int_prefix{};
};

template <typename char_type>
struct basic_lc_name
{
	basic_scatter<char_type> name_fmt{};
	basic_scatter<char_type> name_gen{};
	basic_scatter<char_type> name_miss{};
	basic_scatter<char_type> name_mr{};
	basic_scatter<char_type> name_mrs{};
	basic_scatter<char_type> name_ms{};
};

template <typename char_type>
struct basic_lc_address
{
	basic_scatter<char_type> postal_fmt{};
	basic_scatter<char_type> country_name{};
	basic_scatter<char_type> country_post{};
	basic_scatter<char_type> country_ab2{};
	basic_scatter<char_type> country_ab3{};
	::std::uint_least64_t country_num{};
	basic_scatter<char_type> country_car{};
	basic_scatter<char_type> country_isbn{};
	basic_scatter<char_type> lang_name{};
	basic_scatter<char_type> lang_ab{};
	basic_scatter<char_type> lang_term{};
	basic_scatter<char_type> lang_lib{};
};

template <typename char_type>
struct basic_lc_measurement
{
	::std::uint_least64_t measurement{};
};

template <typename char_type>
struct basic_lc_keyboard
{
	basic_scatter<basic_scatter<char_type>> keyboards{};
};

template <typename char_type>
struct basic_lc_all
{
	basic_lc_identification<char_type> identification{};
	basic_lc_monetary<char_type> monetary{};
	basic_lc_numeric<char_type> numeric{};
	basic_lc_time<char_type> time{};
	basic_lc_messages<char_type> messages{};
	basic_lc_paper<char_type> paper{};
	basic_lc_telephone<char_type> telephone{};
	basic_lc_name<char_type> name{};
	basic_lc_address<char_type> address{};
	basic_lc_measurement<char_type> measurement{};
	basic_lc_keyboard<char_type> keyboard{};
};

using lc_all = basic_lc_all<char>;
using wlc_all = basic_lc_all<wchar_t>;
using u8lc_all = basic_lc_all<char8_t>;
using u16lc_all = basic_lc_all<char16_t>;
using u32lc_all = basic_lc_all<char32_t>;

struct locale_data
{
	lc_all const *all{};
	wlc_all const *wall{};
	u8lc_all const *u8all{};
	u16lc_all const *u16all{};
	u32lc_all const *u32all{};
};

inline constexpr ::std::uint_least64_t export_magic{UINT64_C(0x46494f4c43303031)}; // "FIOLC001"
inline constexpr ::std::uint_least32_t export_version{1};

// The layout tag is intentionally derived from the platform ABI and from key
// nested offsets.  Equal structure sizes alone are insufficient evidence: two
// compilers can pad internal fields differently while producing the same final
// size.  A module is accepted only when both sides compiled the same v1 schema
// for the same data model.
[[nodiscard]] inline constexpr ::std::uint_least64_t layout_tag() noexcept
{
	::std::uint_least64_t value{UINT64_C(1469598103934665603)};
	auto mix{[&value](::std::size_t part) constexpr noexcept
	{
		value ^= static_cast<::std::uint_least64_t>(part);
		value *= UINT64_C(1099511628211);
	}};
	mix(sizeof(void *));
	mix(sizeof(::std::size_t));
	mix(sizeof(wchar_t));
	mix(sizeof(lc_all));
	mix(alignof(lc_all));
	mix(offsetof(lc_all, numeric));
	mix(offsetof(lc_all, time));
	mix(offsetof(lc_all, keyboard));
	mix(sizeof(basic_lc_time<char>));
	mix(offsetof(basic_lc_time<char>, era));
	mix(offsetof(basic_lc_time<char>, alt_digits));
	mix(sizeof(basic_lc_time_era<char>));
	mix(offsetof(basic_lc_time_era<char>, era_name));
	mix(sizeof(basic_lc_address<char>));
	mix(offsetof(basic_lc_address<char>, country_num));
	return value;
}

struct export_descriptor
{
	::std::uint_least64_t magic{};
	::std::uint_least32_t version{};
	::std::uint_least32_t descriptor_size{};
	::std::uint_least64_t abi_layout_tag{};
	locale_data locale{};
};

static_assert(::std::is_standard_layout_v<export_descriptor>);
static_assert(::std::is_trivially_copyable_v<export_descriptor>);
static_assert(::std::is_standard_layout_v<lc_all>);
static_assert(::std::is_trivially_copyable_v<lc_all>);

inline constexpr char8_t export_symbol[]{u8"fast_io_i18n_export_v1"};

} // namespace fast_io::i18n_module_v1

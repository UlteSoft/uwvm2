#pragma once

#include <span>

#include "locale/module.h"

namespace fast_io
{

using i18n_scatter_size_type = std::uint_least32_t;

template <typename T>
struct basic_scatter
{
	using size_type = i18n_scatter_size_type;
	inline static constexpr ::std::size_t relative_limit{
		static_cast<::std::size_t>(::std::numeric_limits<size_type>::max())};

	/// @brief Returns a representable locale-relative offset without silent narrowing.
	/// @details The public descriptor uses a 32-bit relative representation even when `size_t` is wider. Truncating an
	///          oversized vector length can turn a newly appended field into an apparently in-bounds descriptor naming
	///          unrelated earlier storage, so representability is checked at the only narrowing boundary.
	[[nodiscard]] inline static size_type buffer_size(std::vector<T> const &which) noexcept
	{
		if (which.size() > relative_limit) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return static_cast<size_type>(which.size());
	}

	template <std::ranges::range rg>
	[[nodiscard]] inline static basic_scatter append_range(std::vector<T> &which, rg &&r)
	{
		auto const first{which.size()};
		if (first > relative_limit) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		if constexpr (::std::ranges::sized_range<rg>)
		{
			auto const extent{::std::ranges::size(r)};
			if (!::std::in_range<::std::size_t>(extent) ||
				static_cast<::std::size_t>(extent) > relative_limit - first) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
		}

		struct rollback_guard
		{
			::std::vector<T> *destination;
			::std::size_t original_size;
			bool committed{};

			inline ~rollback_guard()
			{
				if (!committed)
				{
					// Popping only the appended suffix neither moves nor assigns a pre-existing element and imposes no
					// MoveAssignable requirement on T. This restores the logical locale table when allocation or element
					// construction throws partway through the range.
					while (destination->size() != original_size)
					{
						destination->pop_back();
					}
				}
			}
		} rollback{__builtin_addressof(which), first};

		// C++20 has no vector::append_range. Element-wise emplacement preserves the source range's reference category.
		// The per-element limit check is still required for an unsized or adversarial range whose true extent was not
		// available before iteration; it prevents every successful append from crossing the descriptor domain.
		for (auto &&element : r)
		{
			if (which.size() == relative_limit) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			which.emplace_back(::std::forward<decltype(element)>(element));
		}
		rollback.committed = true;
		auto const final_size{which.size()};
		return {static_cast<size_type>(first), static_cast<size_type>(final_size - first)};
	}

	[[nodiscard]] inline std::span<T> get_from(std::vector<T> &which) const noexcept
	{
		return get_from_impl(which);
	}
	[[nodiscard]] inline std::span<T const> get_from(std::vector<T> const &which) const noexcept
	{
		return get_from_impl(which);
	}
	size_type rva;
	size_type length;

	private:
	template <typename vector_type>
	[[nodiscard]] inline ::std::span<::std::conditional_t<
		::std::is_const_v<vector_type>, T const, T>>
	get_from_impl(vector_type &which) const noexcept
	{
		if (length == 0u)
		{
			// An empty vector may have a null data pointer. No pointer arithmetic is needed to represent an empty field.
			return {};
		}
		auto const first{static_cast<::std::size_t>(rva)};
		auto const count{static_cast<::std::size_t>(length)};
		if (which.size() < first || which.size() - first < count) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return {which.data() + first, count};
	}
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

using lc_identification = basic_lc_identification<char>;
using wlc_identification = basic_lc_identification<wchar_t>;
using u8lc_identification = basic_lc_identification<char8_t>;
using u16lc_identification = basic_lc_identification<char16_t>;
using u32lc_identification = basic_lc_identification<char32_t>;

template <typename char_type>
struct basic_lc_monetary
{
	basic_scatter<char_type> int_curr_symbol{};
	basic_scatter<char_type> currency_symbol{};
	basic_scatter<char_type> mon_decimal_point{};
	basic_scatter<char_type> mon_thousands_sep{};
	basic_scatter<std::size_t> mon_grouping{};
	basic_scatter<char_type> positive_sign{};
	basic_scatter<char_type> negative_sign{};
	std::size_t int_frac_digits{};
	std::size_t frac_digits{};
	std::size_t p_cs_precedes{};
	std::size_t p_sep_by_space{};
	std::size_t n_cs_precedes{};
	std::size_t n_sep_by_space{};
	std::size_t int_p_cs_precedes{};
	std::size_t int_p_sep_by_space{};
	std::size_t int_n_cs_precedes{};
	std::size_t int_n_sep_by_space{};
	std::size_t p_sign_posn{};
	std::size_t n_sign_posn{};
	std::size_t int_p_sign_posn{};
	std::size_t int_n_sign_posn{};
};

using lc_monetary = basic_lc_monetary<char>;
using wlc_monetary = basic_lc_monetary<wchar_t>;
using u8lc_monetary = basic_lc_monetary<char8_t>;
using u16lc_monetary = basic_lc_monetary<char16_t>;
using u32lc_monetary = basic_lc_monetary<char32_t>;

template <typename char_type>
struct basic_lc_numeric
{
	basic_scatter<char_type> decimal_point{};
	basic_scatter<char_type> thousands_sep{};
	basic_scatter<std::size_t> grouping{};
};

using lc_numeric = basic_lc_numeric<char>;
using wlc_numeric = basic_lc_numeric<wchar_t>;
using u8lc_numeric = basic_lc_numeric<char8_t>;
using u16lc_numeric = basic_lc_numeric<char16_t>;
using u32lc_numeric = basic_lc_numeric<char32_t>;

template <typename char_type>
struct basic_lc_time_era
{
	bool direction{}; //+ is true, - is false
	std::int_least64_t offset{};
	std::int_least64_t start_date_year{};
	std::uint_least8_t start_date_month{};
	std::uint_least8_t start_date_day{};
	std::int_least8_t end_date_special{}; //-1 is -*, 0 means end_date exist, 1 is +*
	std::int_least64_t end_date_year{};
	std::uint_least8_t end_date_month{};
	std::uint_least8_t end_date_day{};
	basic_scatter<char_type> era_name;
	basic_scatter<char_type> era_format;
	basic_scatter<char_type> era;
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
		std::size_t ndays{7};
		std::int_least64_t first_day{19971201};
		std::size_t first_week{4};
	} week{};
	std::size_t first_weekday{};
	std::size_t first_workday{};
	std::size_t cal_direction{};
	basic_scatter<basic_scatter<char_type>> timezone{};
};

using lc_time = basic_lc_time<char>;
using wlc_time = basic_lc_time<wchar_t>;
using u8lc_time = basic_lc_time<char8_t>;
using u16lc_time = basic_lc_time<char16_t>;
using u32lc_time = basic_lc_time<char32_t>;

template <typename char_type>
struct basic_lc_messages
{
	basic_scatter<char_type> yesexpr{};
	basic_scatter<char_type> noexpr{};
	basic_scatter<char_type> yesstr{};
	basic_scatter<char_type> nostr{};
};

using lc_messages = basic_lc_messages<char>;
using wlc_messages = basic_lc_messages<wchar_t>;
using u8lc_messages = basic_lc_messages<char8_t>;
using u16lc_messages = basic_lc_messages<char16_t>;
using u32lc_messages = basic_lc_messages<char32_t>;

template <typename char_type>
struct basic_lc_paper
{
	std::uint_least64_t width{};
	std::uint_least64_t height{};
};

using lc_paper = basic_lc_paper<char>;
using wlc_paper = basic_lc_paper<wchar_t>;
using u8lc_paper = basic_lc_paper<char8_t>;
using u16lc_paper = basic_lc_paper<char16_t>;
using u32lc_paper = basic_lc_paper<char32_t>;

template <typename char_type>
struct basic_lc_telephone
{
	basic_scatter<char_type> tel_int_fmt{};
	basic_scatter<char_type> tel_dom_fmt{};
	basic_scatter<char_type> int_select{};
	basic_scatter<char_type> int_prefix{};
};

using lc_telephone = basic_lc_telephone<char>;
using wlc_telephone = basic_lc_telephone<wchar_t>;
using u8lc_telephone = basic_lc_telephone<char8_t>;
using u16lc_telephone = basic_lc_telephone<char16_t>;
using u32lc_telephone = basic_lc_telephone<char32_t>;

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

using lc_name = basic_lc_name<char>;
using wlc_name = basic_lc_name<wchar_t>;
using u8lc_name = basic_lc_name<char8_t>;
using u16lc_name = basic_lc_name<char16_t>;
using u32lc_name = basic_lc_name<char32_t>;

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

using lc_address = basic_lc_address<char>;
using wlc_address = basic_lc_address<wchar_t>;
using u8lc_address = basic_lc_address<char8_t>;
using u16lc_address = basic_lc_address<char16_t>;
using u32lc_address = basic_lc_address<char32_t>;

template <typename char_type>
struct basic_lc_measurement
{
	std::uint_least64_t measurement{};
};

using lc_measurement = basic_lc_measurement<char>;
using wlc_measurement = basic_lc_measurement<wchar_t>;
using u8lc_measurement = basic_lc_measurement<char8_t>;
using u16lc_measurement = basic_lc_measurement<char16_t>;
using u32lc_measurement = basic_lc_measurement<char32_t>;

template <typename char_type>
struct basic_lc_keyboard
{
	basic_scatter<basic_scatter<char_type>> keyboards{};
};

using lc_keyboard = basic_lc_keyboard<char>;
using wlc_keyboard = basic_lc_keyboard<wchar_t>;
using u8lc_keyboard = basic_lc_keyboard<char8_t>;
using u16lc_keyboard = basic_lc_keyboard<char16_t>;
using u32lc_keyboard = basic_lc_keyboard<char32_t>;

template <typename char_type>
struct basic_lc_data_storage;

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
	// Relative facet scatters are meaningful only together with their owning tables. Keeping that association explicit
	// avoids non-portable container-of arithmetic and also lets a validated raw `basic_lc_all*` remain the historical
	// customization argument. A copied standalone `basic_lc_all` borrows this table; `basic_lc_object` below instead
	// rebinds the pointer after every construction and assignment so its complete-object copies remain self-contained.
	// Direct wholesale assignment to a public `object.all` intentionally has standalone-view semantics and can therefore
	// replace this link; code maintaining an owning object must update facet fields or assign the complete object instead.
	basic_lc_data_storage<char_type> const *data_storage{};
};

using lc_all = basic_lc_all<char>;
using wlc_all = basic_lc_all<wchar_t>;
using u8lc_all = basic_lc_all<char8_t>;
using u16lc_all = basic_lc_all<char16_t>;
using u32lc_all = basic_lc_all<char32_t>;

template <typename char_type>
struct basic_lc_data_storage
{
	std::vector<char_type> chars{};
	std::vector<std::size_t> integers{};
	std::vector<basic_lc_time_era<char_type>> eras{};
	std::vector<basic_scatter<char_type>> strings{};

private:
	template <typename T, typename Self>
	[[nodiscard]] inline static constexpr decltype(auto) get_storage_impl(Self &&self) noexcept
	{
		if constexpr (std::same_as<T, char_type>)
		{
			return (::std::forward<Self>(self).chars);
		}
		else if constexpr (std::same_as<T, std::size_t>)
		{
			return (::std::forward<Self>(self).integers);
		}
		else if constexpr (std::same_as<T, basic_lc_time_era<char_type>>)
		{
			return (::std::forward<Self>(self).eras);
		}
		else if constexpr (std::same_as<T, basic_scatter<char_type>>)
		{
			return (::std::forward<Self>(self).strings);
		}
		else
		{
			return;
		}
	}

public:
	template <typename T>
	[[nodiscard]] inline constexpr decltype(auto) get_storage() & noexcept
	{
		return get_storage_impl<T>(*this);
	}

	template <typename T>
	[[nodiscard]] inline constexpr decltype(auto) get_storage() const & noexcept
	{
		return get_storage_impl<T>(*this);
	}

	template <typename T>
	[[nodiscard]] inline constexpr decltype(auto) get_storage() && noexcept
	{
		return get_storage_impl<T>(::std::move(*this));
	}

	template <typename T>
	[[nodiscard]] inline constexpr decltype(auto) get_storage() const && noexcept
	{
		return get_storage_impl<T>(::std::move(*this));
	}
};

using lc_data_storage = basic_lc_data_storage<char>;
using wlc_data_storage = basic_lc_data_storage<wchar_t>;
using u8lc_data_storage = basic_lc_data_storage<char8_t>;
using u16lc_data_storage = basic_lc_data_storage<char16_t>;
using u32lc_data_storage = basic_lc_data_storage<char32_t>;

template <typename char_type>
struct basic_lc_object
{
	basic_lc_data_storage<char_type> data_storage{};
	basic_lc_all<char_type> all{};

	inline basic_lc_object() noexcept
	{
		bind_storage();
	}

	inline basic_lc_object(basic_lc_object const &other)
		: data_storage(other.data_storage), all(other.all)
	{
		bind_storage();
	}

	inline basic_lc_object(basic_lc_object &&other) noexcept(
		::std::is_nothrow_move_constructible_v<basic_lc_data_storage<char_type>> &&
		::std::is_nothrow_copy_constructible_v<basic_lc_all<char_type>> &&
		::std::is_nothrow_default_constructible_v<basic_lc_all<char_type>> &&
		::std::is_nothrow_move_assignable_v<basic_lc_all<char_type>>)
		: data_storage(::std::move(other.data_storage)), all(other.all)
	{
		bind_storage();
		// A moved-from complete locale remains a coherent empty locale rather than retaining nonzero RVAs into vectors
		// whose allocations now belong to this object.
		other.all = {};
		other.bind_storage();
	}

	inline basic_lc_object(basic_lc_data_storage<char_type> storage,
						   basic_lc_all<char_type> facets)
		: data_storage(::std::move(storage)), all(::std::move(facets))
	{
		// This constructor preserves the former two-member aggregate construction spelling while establishing the new
		// explicit owner link. Any link carried by `facets` is intentionally replaced by this complete object's storage.
		bind_storage();
	}

	inline basic_lc_object &operator=(basic_lc_object const &other)
	{
		if (__builtin_addressof(other) != this)
		{
			// Constructing the complete copy before mutation gives the object-level assignment a strong guarantee even
			// though copying one of the storage vectors may allocate and throw midway through its own member assignment.
			basic_lc_object temporary(other);
			swap(*this, temporary);
		}
		return *this;
	}

	inline basic_lc_object &operator=(basic_lc_object &&other) noexcept(
		::std::is_nothrow_move_assignable_v<basic_lc_data_storage<char_type>> &&
		::std::is_nothrow_copy_assignable_v<basic_lc_all<char_type>> &&
		::std::is_nothrow_default_constructible_v<basic_lc_all<char_type>> &&
		::std::is_nothrow_move_assignable_v<basic_lc_all<char_type>>)
	{
		if (__builtin_addressof(other) != this)
		{
			data_storage = ::std::move(other.data_storage);
			all = other.all;
			bind_storage();
			other.all = {};
			other.bind_storage();
		}
		return *this;
	}

	friend inline void swap(basic_lc_object &left, basic_lc_object &right) noexcept(
		::std::is_nothrow_swappable_v<basic_lc_data_storage<char_type>> &&
		::std::is_nothrow_swappable_v<basic_lc_all<char_type>>)
	{
		using ::std::swap;
		swap(left.data_storage, right.data_storage);
		swap(left.all, right.all);
		// Swapping the facet aggregates also swaps their old links. Rebind both sides after ownership changes so every
		// relative descriptor continues to resolve against the vectors that moved with its facet aggregate.
		left.bind_storage();
		right.bind_storage();
	}

private:
	inline void bind_storage() noexcept
	{
		all.data_storage = __builtin_addressof(data_storage);
	}
};

namespace details
{

/// @brief Resolves one compact locale-relative scatter against its explicit owning table.
/// @details `basic_lc_object` may contain implementation-defined standard-library containers, so neither its layout nor
///          `offsetof(all)` is a portable container-of proof. The embedded `basic_lc_all` instead carries the table
///          pointer established by the complete object's constructors. Copy and move operations rebind that pointer,
///          which prevents a copied locale from continuing to reference the source object's vectors. Bounds are
///          checked before pointer arithmetic: malformed locale data terminates at the ownership boundary rather than
///          manufacturing a descriptor outside the vector. An empty descriptor observes no storage and remains valid
///          even for a deliberately empty standalone `basic_lc_all`.
template <::std::integral char_type, typename value_type>
[[nodiscard]] inline ::fast_io::basic_io_scatter_t<value_type> lc_resolve_scatter(
	::fast_io::basic_lc_all<char_type> const *all,
	::fast_io::basic_scatter<value_type> scatter) noexcept
{
	if (!scatter.length)
	{
		// No element is observed for an empty facet. Avoid forming data()+0 when
		// the owning vector is also empty, because pointer arithmetic on a null
		// data pointer is not required to be valid.
		return {};
	}
	auto const storage_owner{all->data_storage};
	if (storage_owner == nullptr)
	{
		::fast_io::fast_terminate();
	}
	auto const &storage{storage_owner->template get_storage<value_type>()};
	// `basic_scatter::get_from` is the public checked resolution boundary. Reusing it here avoids maintaining a second
	// bounds formula and, on the formatting hot path, avoids validating the same descriptor twice.
	auto const view{scatter.get_from(storage)};
	return {view.data(), view.size()};
}

} // namespace details

using lc_object = basic_lc_object<char>;
using wlc_object = basic_lc_object<wchar_t>;
using u8lc_object = basic_lc_object<char8_t>;
using u16lc_object = basic_lc_object<char16_t>;
using u32lc_object = basic_lc_object<char32_t>;

struct lc_locale
{
	lc_object const *lc;
	wlc_object const *wlc;
	u8lc_object const *u8lc;
	u16lc_object const *u16lc;
	u32lc_object const *u32lc;
};

template <typename char_type>
concept lc_character_type = ::std::same_as<char_type, char> || ::std::same_as<char_type, wchar_t> ||
	::std::same_as<char_type, char8_t> || ::std::same_as<char_type, char16_t> ||
	::std::same_as<char_type, char32_t>;

template <lc_character_type char_type>
inline constexpr basic_lc_object<char_type> const *get_lc(lc_locale const &loc) noexcept
{
	if constexpr (std::same_as<char_type, char>)
	{
		return loc.lc;
	}
	else if constexpr (std::same_as<char_type, wchar_t>)
	{
		return loc.wlc;
	}
	else if constexpr (std::same_as<char_type, char8_t>)
	{
		return loc.u8lc;
	}
	else if constexpr (std::same_as<char_type, char16_t>)
	{
		return loc.u16lc;
	}
	else if constexpr (std::same_as<char_type, char32_t>)
	{
		return loc.u32lc;
	}
	else
	{
		return {};
	}
}

struct lc_locale_owner
{
	lc_object lc{};
	wlc_object wlc{};
	u8lc_object u8lc{};
	u16lc_object u16lc{};
	u32lc_object u32lc{};

	[[nodiscard]] inline constexpr lc_locale view() const noexcept
	{
		return {__builtin_addressof(lc), __builtin_addressof(wlc), __builtin_addressof(u8lc),
				__builtin_addressof(u16lc), __builtin_addressof(u32lc)};
	}
};

namespace details
{

[[nodiscard]] inline constexpr bool lc_module_descriptor_compatible(
	::fast_io::i18n_module_v1::export_descriptor const *descriptor) noexcept
{
	using namespace ::fast_io::i18n_module_v1;
	return descriptor != nullptr && descriptor->magic == export_magic && descriptor->version == export_version &&
		   descriptor->descriptor_size == sizeof(export_descriptor) &&
		   descriptor->abi_layout_tag == layout_tag() && descriptor->locale.all != nullptr &&
		   descriptor->locale.wall != nullptr && descriptor->locale.u8all != nullptr &&
		   descriptor->locale.u16all != nullptr && descriptor->locale.u32all != nullptr;
}

template <typename value_type, typename char_type>
[[nodiscard]] inline basic_scatter<value_type> lc_module_append_scatter(
	basic_lc_data_storage<char_type> &storage,
	::fast_io::i18n_module_v1::basic_scatter<value_type> source)
{
	auto &destination{storage.template get_storage<value_type>()};
	auto const first{destination.size()};
	constexpr auto relative_limit{static_cast<::std::size_t>(::std::numeric_limits<i18n_scatter_size_type>::max())};
	if ((source.len != 0u && source.base == nullptr) || first > relative_limit ||
		source.len > relative_limit - first)
	{
		// A loaded module already executes native code and is therefore a trust
		// boundary, not an untrusted file parser.  Termination nevertheless keeps
		// malformed lengths from being truncated into apparently valid RVAs.
		::fast_io::fast_terminate();
	}
	if (source.len != 0u)
	{
		destination.insert(destination.end(), source.base, source.base + source.len);
	}
	return {static_cast<i18n_scatter_size_type>(first), static_cast<i18n_scatter_size_type>(source.len)};
}

template <typename char_type>
[[nodiscard]] inline basic_scatter<basic_scatter<char_type>> lc_module_append_nested_scatters(
	basic_lc_data_storage<char_type> &storage,
	::fast_io::i18n_module_v1::basic_scatter<
		::fast_io::i18n_module_v1::basic_scatter<char_type>> source)
{
	if (source.len != 0u && source.base == nullptr)
	{
		::fast_io::fast_terminate();
	}
	auto &strings{storage.strings};
	auto const first{strings.size()};
	constexpr auto relative_limit{static_cast<::std::size_t>(::std::numeric_limits<i18n_scatter_size_type>::max())};
	if (first > relative_limit || source.len > relative_limit - first)
	{
		::fast_io::fast_terminate();
	}
	strings.reserve(first + source.len);
	for (::std::size_t index{}; index != source.len; ++index)
	{
		strings.emplace_back(lc_module_append_scatter(storage, source.base[index]));
	}
	return {static_cast<i18n_scatter_size_type>(first), static_cast<i18n_scatter_size_type>(source.len)};
}

template <typename char_type>
[[nodiscard]] inline basic_scatter<basic_lc_time_era<char_type>> lc_module_append_eras(
	basic_lc_data_storage<char_type> &storage,
	::fast_io::i18n_module_v1::basic_scatter<
		::fast_io::i18n_module_v1::basic_lc_time_era<char_type>> source)
{
	if (source.len != 0u && source.base == nullptr)
	{
		::fast_io::fast_terminate();
	}
	auto &eras{storage.eras};
	auto const first{eras.size()};
	constexpr auto relative_limit{static_cast<::std::size_t>(::std::numeric_limits<i18n_scatter_size_type>::max())};
	if (first > relative_limit || source.len > relative_limit - first)
	{
		::fast_io::fast_terminate();
	}
	eras.reserve(first + source.len);
	for (::std::size_t index{}; index != source.len; ++index)
	{
		auto const &input{source.base[index]};
		eras.emplace_back(basic_lc_time_era<char_type>{
			.direction = input.direction,
			.offset = input.offset,
			.start_date_year = input.start_date_year,
			.start_date_month = input.start_date_month,
			.start_date_day = input.start_date_day,
			.end_date_special = input.end_date_special,
			.end_date_year = input.end_date_year,
			.end_date_month = input.end_date_month,
			.end_date_day = input.end_date_day,
			.era_name = lc_module_append_scatter(storage, input.era_name),
			.era_format = lc_module_append_scatter(storage, input.era_format),
			.era = lc_module_append_scatter(storage, input.era)});
	}
	return {static_cast<i18n_scatter_size_type>(first), static_cast<i18n_scatter_size_type>(source.len)};
}

/// Converts one immutable module facet aggregate into the public owning form.
/// Every pointer is consumed before this function returns.  The resulting
/// object contains only 32-bit relative descriptors into its own vectors, so
/// neither later vector relocation nor unloading the module can invalidate a
/// locale print operation.
template <typename char_type>
inline void lc_module_import_one(
	::fast_io::i18n_module_v1::basic_lc_all<char_type> const &source,
	basic_lc_object<char_type> &destination)
{
	basic_lc_object<char_type> result;
	auto &storage{result.data_storage};
	auto string{[&storage](auto input) { return lc_module_append_scatter(storage, input); }};

	auto &identification{result.all.identification};
	identification.name = string(source.identification.name);
	identification.encoding = string(source.identification.encoding);
	identification.title = string(source.identification.title);
	identification.source = string(source.identification.source);
	identification.address = string(source.identification.address);
	identification.contact = string(source.identification.contact);
	identification.email = string(source.identification.email);
	identification.tel = string(source.identification.tel);
	identification.fax = string(source.identification.fax);
	identification.language = string(source.identification.language);
	identification.territory = string(source.identification.territory);
	identification.audience = string(source.identification.audience);
	identification.application = string(source.identification.application);
	identification.abbreviation = string(source.identification.abbreviation);
	identification.revision = string(source.identification.revision);
	identification.date = string(source.identification.date);

	auto &monetary{result.all.monetary};
	monetary.int_curr_symbol = string(source.monetary.int_curr_symbol);
	monetary.currency_symbol = string(source.monetary.currency_symbol);
	monetary.mon_decimal_point = string(source.monetary.mon_decimal_point);
	monetary.mon_thousands_sep = string(source.monetary.mon_thousands_sep);
	monetary.mon_grouping = lc_module_append_scatter(storage, source.monetary.mon_grouping);
	monetary.positive_sign = string(source.monetary.positive_sign);
	monetary.negative_sign = string(source.monetary.negative_sign);
	monetary.int_frac_digits = source.monetary.int_frac_digits;
	monetary.frac_digits = source.monetary.frac_digits;
	monetary.p_cs_precedes = source.monetary.p_cs_precedes;
	monetary.p_sep_by_space = source.monetary.p_sep_by_space;
	monetary.n_cs_precedes = source.monetary.n_cs_precedes;
	monetary.n_sep_by_space = source.monetary.n_sep_by_space;
	monetary.int_p_cs_precedes = source.monetary.int_p_cs_precedes;
	monetary.int_p_sep_by_space = source.monetary.int_p_sep_by_space;
	monetary.int_n_cs_precedes = source.monetary.int_n_cs_precedes;
	monetary.int_n_sep_by_space = source.monetary.int_n_sep_by_space;
	monetary.p_sign_posn = source.monetary.p_sign_posn;
	monetary.n_sign_posn = source.monetary.n_sign_posn;
	monetary.int_p_sign_posn = source.monetary.int_p_sign_posn;
	monetary.int_n_sign_posn = source.monetary.int_n_sign_posn;

	result.all.numeric.decimal_point = string(source.numeric.decimal_point);
	result.all.numeric.thousands_sep = string(source.numeric.thousands_sep);
	result.all.numeric.grouping = lc_module_append_scatter(storage, source.numeric.grouping);

	auto &time{result.all.time};
	for (::std::size_t index{}; index != 7u; ++index)
	{
		time.abday[index] = string(source.time.abday[index]);
		time.day[index] = string(source.time.day[index]);
	}
	for (::std::size_t index{}; index != 12u; ++index)
	{
		time.abmon[index] = string(source.time.abmon[index]);
		time.ab_alt_mon[index] = string(source.time.ab_alt_mon[index]);
		time.mon[index] = string(source.time.mon[index]);
	}
	time.d_t_fmt = string(source.time.d_t_fmt);
	time.d_fmt = string(source.time.d_fmt);
	time.t_fmt = string(source.time.t_fmt);
	time.t_fmt_ampm = string(source.time.t_fmt_ampm);
	time.date_fmt = string(source.time.date_fmt);
	time.am_pm[0] = string(source.time.am_pm[0]);
	time.am_pm[1] = string(source.time.am_pm[1]);
	time.era = lc_module_append_eras(storage, source.time.era);
	time.era_d_fmt = string(source.time.era_d_fmt);
	time.era_d_t_fmt = string(source.time.era_d_t_fmt);
	time.era_t_fmt = string(source.time.era_t_fmt);
	time.alt_digits = lc_module_append_nested_scatters(storage, source.time.alt_digits);
	time.week.ndays = source.time.week.ndays;
	time.week.first_day = source.time.week.first_day;
	time.week.first_week = source.time.week.first_week;
	time.first_weekday = source.time.first_weekday;
	time.first_workday = source.time.first_workday;
	time.cal_direction = source.time.cal_direction;
	time.timezone = lc_module_append_nested_scatters(storage, source.time.timezone);

	result.all.messages.yesexpr = string(source.messages.yesexpr);
	result.all.messages.noexpr = string(source.messages.noexpr);
	result.all.messages.yesstr = string(source.messages.yesstr);
	result.all.messages.nostr = string(source.messages.nostr);
	result.all.paper.width = source.paper.width;
	result.all.paper.height = source.paper.height;
	result.all.telephone.tel_int_fmt = string(source.telephone.tel_int_fmt);
	result.all.telephone.tel_dom_fmt = string(source.telephone.tel_dom_fmt);
	result.all.telephone.int_select = string(source.telephone.int_select);
	result.all.telephone.int_prefix = string(source.telephone.int_prefix);
	result.all.name.name_fmt = string(source.name.name_fmt);
	result.all.name.name_gen = string(source.name.name_gen);
	result.all.name.name_miss = string(source.name.name_miss);
	result.all.name.name_mr = string(source.name.name_mr);
	result.all.name.name_mrs = string(source.name.name_mrs);
	result.all.name.name_ms = string(source.name.name_ms);
	result.all.address.postal_fmt = string(source.address.postal_fmt);
	result.all.address.country_name = string(source.address.country_name);
	result.all.address.country_post = string(source.address.country_post);
	result.all.address.country_ab2 = string(source.address.country_ab2);
	result.all.address.country_ab3 = string(source.address.country_ab3);
	result.all.address.country_num = source.address.country_num;
	result.all.address.country_car = string(source.address.country_car);
	result.all.address.country_isbn = string(source.address.country_isbn);
	result.all.address.lang_name = string(source.address.lang_name);
	result.all.address.lang_ab = string(source.address.lang_ab);
	result.all.address.lang_term = string(source.address.lang_term);
	result.all.address.lang_lib = string(source.address.lang_lib);
	result.all.measurement.measurement = source.measurement.measurement;
	result.all.keyboard.keyboards = lc_module_append_nested_scatters(storage, source.keyboard.keyboards);

	destination = ::std::move(result);
}

inline void lc_module_import(
	::fast_io::i18n_module_v1::export_descriptor const &descriptor,
	lc_locale_owner &destination)
{
	// Build all five character domains off to the side.  If allocation throws,
	// the caller's previous locale remains unchanged and the DLL RAII guard is
	// still responsible for unloading the module.
	lc_locale_owner result;
	lc_module_import_one(*descriptor.locale.all, result.lc);
	lc_module_import_one(*descriptor.locale.wall, result.wlc);
	lc_module_import_one(*descriptor.locale.u8all, result.u8lc);
	lc_module_import_one(*descriptor.locale.u16all, result.u16lc);
	lc_module_import_one(*descriptor.locale.u32all, result.u32lc);
	destination = ::std::move(result);
}

} // namespace details

} // namespace fast_io

#pragma once
#include <ranges>

namespace mland {

	template <class T>
	template <class U, class V> requires std::is_base_of_v<_details::str_like_base, U>
	V str_like<T>::split(char delim) const {
		auto range = const_cast<const str_like&>(*this) | std::views::split(delim);
		V result;
		for (const auto& r : range) {
			result.emplace_back(r.begin(), r.end());
		}
		return result;
	}

	template <class T>
	std::ostream& operator<<(std::ostream& os, const vec<T>& v) {
		os << "[";
		bool first = true;
		for (const auto& e : v) {
			if (!first)
				os << ", ";

			first = false;
			os << e;
		}
		os << "]";
		return os;
	}
}
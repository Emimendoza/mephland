#pragma once
#include <atomic>
#include "../globals.h"
#ifdef MLAND_USE_REFLECTION
#warning "Reflection is not implemented"
#include <experimental/reflect>
#define MCLASS(NAME) \
static constexpr auto MName = mland::_details::pad_str<PAD_LENGTH>(mland::_details::nameof<NAME>());
#else
#define MCLASS(NAME) \
	static constexpr auto MName = mland::_details::pad_str<mland::_details::PAD_LENGTH>( #NAME ); \
	static constexpr auto MName_DEBUG = mland::_details::concat_str<mland::_details::PAD_LENGTH, mland::_details::LOG_LENGTH, mland::_details::DEBUG>(MName); \
	static constexpr auto MName_INFO = mland::_details::concat_str<mland::_details::PAD_LENGTH, mland::_details::LOG_LENGTH, mland::_details::INFO>(MName); \
	static constexpr auto MName_WARN = mland::_details::concat_str<mland::_details::PAD_LENGTH, mland::_details::LOG_LENGTH, mland::_details::WARN>(MName); \
	static constexpr auto MName_ERROR = mland::_details::concat_str<mland::_details::PAD_LENGTH, mland::_details::LOG_LENGTH, mland::_details::ERROR>(MName)
#endif

namespace mland::_details {
constexpr size_t PAD_LENGTH = 15;
constexpr char DEBUG[] = "[DEB]";
constexpr char INFO[] = "[INF]";
constexpr char WARN[] = "[WRN]";
constexpr char ERROR[] = "[ERR]";

static_assert(sizeof(DEBUG) == sizeof(INFO) && sizeof(INFO) == sizeof(WARN) && sizeof(WARN) == sizeof(ERROR), "All log levels must have the same length");

constexpr size_t LOG_LENGTH = sizeof(DEBUG) - 1;
template <size_t N>
constexpr std::array<char, N + 1> pad_str(std::string_view str) {
	std::array<char, N + 1> result{};
	size_t i = 1;
	result[0] = '[';
	for (; i < str.size() + 1; ++i) {
		result[i] = str[i - 1];
	}
	result[i] = ']';
	for (i++; i < N; ++i) {
		result[i] = ' ';
	}
	result[N] = '\0';
	return result;
}

template <size_t N2, size_t N1, const char pre[N1 + 1]>
constexpr std::array<char, N2 + N1 + 1> concat_str(std::array<char, N2 + 1> str) {
	std::array<char, N2 + N1 + 1> result{};
	size_t i = 0;
	for (; i < N1; ++i) {
		result[i] = pre[i];
	}
	for (size_t j = 0; j < N2; ++j) {
		result[i + j] = str[j];
	}
	result[i + N2] = '\0';
	return result;
}

struct str_like_base {};
struct endl_t {};
struct start_t{};

constexpr start_t start;

class nullStream {
public:
	template <typename T>
	constexpr const nullStream& operator<<(const T&) const { return *this; }
};

constexpr nullStream nullStream{};
extern std::atomic_flag msgMutex;
constexpr std::ostream& operator<<(std::ostream& os, const start_t&) {
	while (msgMutex.test_and_set()){}
	return os;
}

constexpr std::ostream& operator<<(std::ostream& os, const endl_t&) {
	os << '\n';
	msgMutex.clear();
	return os;
}


#ifdef MLAND_USE_REFLECTION
	template<typename T>
	constexpr std::string_view nameof() {
		using tInf = reflexpr(T);
		using aliased_Info = std::experimental::reflect::get_aliased_t<tInf>;
		return std::experimental::reflect::get_name_v<aliased_Info>;
	}
#endif
}
#ifdef MLAND_NO_DEBUG
#define MDEBUG mland::_details::nullStream
#else
#define MDEBUG mland::globals::debug << mland::_details::start << "\033[0m" << MName_DEBUG.data()
#endif

#ifdef MLAND_NO_INFO
#define MINFO mland::_details::nullStream
#else
#define MINFO mland::globals::info << mland::_details::start << "\033[0m" << MName_INFO.data()
#endif

#ifdef MLAND_NO_WARN
#define MWARN mland::_details::nullStream
#else
#define MWARN mland::globals::warn << mland::_details::start << "\033[31m" << MName_WARN.data()
#endif

#ifdef MLAND_NO_ERROR
#define MERROR mland::_details::nullStream
#else
#define MERROR mland::globals::error << mland::_details::start << "\033[31m" << MName_ERROR.data()
#endif

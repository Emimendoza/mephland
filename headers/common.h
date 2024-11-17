#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <ostream>

#include "templates/common_details.tcc"

namespace mland {
	// Since vulkan only uses uint32_t it is convenient to have a vector that uses it
	template <typename T>
	class vec : public std::vector<T> {
	public:
		MCLASS(vec);
		using std::vector<T>::vector;

		vec(std::vector<T>&& other) noexcept : std::vector<T>(std::move(other)) {}

		uint32_t size32() const {return static_cast<uint32_t>(this->size());}
	};
	typedef const char* cstr;
	// For when we have to pass a string to a vulkan function
	template <class T>
	class str_like : public T, public _details::str_like_base {
	public:
		MCLASS(str_like);
		using T::T;

		str_like() = default;
		str_like(const T& t) : T(t) {}
		str_like(T&& t) : T(std::move(t)) {}

		// ReSharper disable once CppNonExplicitConversionOperator
		operator cstr() const {return T::c_str();}

		template <class U> requires std::same_as<std::string, T>
		// ReSharper disable once CppNonExplicitConversionOperator
		operator str_like<U>() const {return str_like<U>(T::c_str());}

		template <class U> requires std::same_as<std::string_view, T>
		// ReSharper disable once CppNonExplicitConversionOperator
		operator str_like<U>() const {return str_like<U>(T::data());}

		template <class U = str_like, class V = vec<U>>
		requires std::is_base_of_v<str_like_base, U>
		V split (char delim) const;

	};

	typedef str_like<std::string> str;
	typedef str_like<std::string_view> str_view;

	template <class T>
	std::ostream& operator<<(std::ostream& os, const str_like<T>& s) {
		os << static_cast<const T&>(s);
		return os;
	}

	template <class T>
	std::ostream& operator<<(std::ostream& os, const vec<T>& v);



	constexpr char endl = '\n';

	template <class T>
	class u_ptr : public std::unique_ptr<T> {
	public:
		MCLASS(u_ptr);
		using std::unique_ptr<T>::unique_ptr;

		constexpr u_ptr(std::unique_ptr<T>&& other) noexcept : std::unique_ptr<T>(std::move(other)) {}

		// ReSharper disable once CppNonExplicitConversionOperator
		constexpr operator const T&() const { return *this->get(); }
	};

	template <class T>
	class s_ptr : public std::shared_ptr<T> {
	public:
		MCLASS(s_ptr);
		using std::shared_ptr<T>::shared_ptr;

		constexpr s_ptr(std::shared_ptr<T>&& other) noexcept : std::shared_ptr<T>(std::move(other)) {}

		// ReSharper disable once CppNonExplicitConversionOperator
		constexpr operator const T&() const { return *this->get(); }
	};

	template <typename T1, typename T2>
	using map = std::unordered_map<T1, T2>;
	template<typename T>
	using opt = std::optional<T>;

	// Forward declaration of all classes

	class DRM_Device;
	class DRM_Handler;
	class VDevice;
	class VShader;
	class VDisplay;
	class VInstance;
	class Controller;
}

// ReSharper disable once CppUnusedIncludeDirective
#include "templates/common_impl.tcc"
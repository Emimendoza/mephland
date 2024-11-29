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

	constexpr uint32_t size32() const { return static_cast<uint32_t>(this->size()); }
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
	str_like(const str_like& other) : T(other) {}
	str_like(str_like&& other) noexcept : T(std::move(other)) {}

	str_like& operator=(const str_like& other) {
		T::operator=(other);
		return *this;
	}

	str_like& operator=(str_like&& other) noexcept {
		T::operator=(std::move(other));
		return *this;
	}

	// ReSharper disable once CppNonExplicitConversionOperator
	template <class U = T> requires std::same_as<std::string, U>
	operator cstr() const { return T::c_str(); }
	// ReSharper disable once CppNonExplicitConversionOperator
	template <class U = T> requires std::same_as<std::string_view, U>
	operator cstr() const { return T::data(); }

	// ReSharper disable once CppNonExplicitConversionOperator
	constexpr operator T&() { return *this; }
	// ReSharper disable once CppNonExplicitConversionOperator
	constexpr operator const T&() const { return *this; }

	template <class U> requires std::same_as<std::string, T>
	// ReSharper disable once CppNonExplicitConversionOperator
	operator str_like<U>() const { return str_like<U>(T::c_str()); }

	template <class U> requires std::same_as<std::string_view, T>
	// ReSharper disable once CppNonExplicitConversionOperator
	operator str_like<U>() const { return str_like<U>(T::data()); }

	template <class U = str_like, class V = vec<U>>
		requires std::is_base_of_v<str_like_base, U>
	V split(char delim) const;
};

typedef str_like<std::string> str;
typedef str_like<std::string_view> str_view;

template <class T>
std::ostream& operator<<(std::ostream& os, const vec<T>& v);


constexpr _details::endl_t endl{};

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
template <typename T>
using opt = std::optional<T>;

// Forward declaration of all classes

class Backend {
public:
	virtual ~Backend() = default;
	MCLASS(Backend);

	class VDevice;
	class VDisplay;
	class VInstance;

	virtual const vec<cstr>& requiredInstanceExtensions() const = 0;
	virtual const vec<cstr>& requiredDeviceExtensions() const = 0;
	virtual u_ptr<VInstance> createInstance(bool validation_layers) = 0;


};
using VDisplay = Backend::VDisplay;
using VDevice = Backend::VDevice;
using VInstance = Backend::VInstance;
struct VShader;
class Controller;
class WLServer;
struct MState;

}

// ReSharper disable once CppUnusedIncludeDirective
#include "templates/common_impl.tcc"

#pragma once
#include <utility>

#include "common.h"

namespace mland {
	class DRM_Device{
	public:
		DRM_Device(std::nullptr_t){}
		struct id_t {
			uint64_t major{};
			uint64_t minor{};
			bool operator==(const id_t& other) const {return other.major == major && other.minor == minor;}
		};
		struct Connector {
			uint32_t id{};
			bool connected{};
			bool operator==(const Connector& other) const {return other.id == id;}
		};

	private:
		friend class DRM_Handler;
		DRM_Device(str  name, const id_t id) : name(std::move(name)), id(id) {}

		vec<Connector> connected;
		vec<Connector> disconnected;

		int fd{-1};
		str name;
	public:
		MCLASS(DRM_Device);
		~DRM_Device();
		DRM_Device(DRM_Device&& other) noexcept : fd(other.fd), name(std::move(other.name)), id(other.id) {other.fd = -1;}
		DRM_Device(const DRM_Device&) = delete;

		constexpr const vec<Connector>& getConnected() const {return connected;}
		constexpr const vec<Connector>& getDisconnected() const {return disconnected;}

		vec<Connector> refreshConnectors();

		const id_t id;

		constexpr int getFd() const { return fd; }
		constexpr const str& getName() const { return name; }
	};
}

template<>
struct std::hash<mland::DRM_Device::id_t> {
	std::size_t operator()(const mland::DRM_Device::id_t& id) const noexcept {
		return std::hash<uint64_t>{}(id.major) ^ std::hash<uint64_t>{}(id.minor);
	}
};

namespace mland {

	class DRM_Handler {
	public:
		struct DRM_Paths {
			vec<str> explicitInclude;
			vec<str> explicitExclude;
		};

		MCLASS(DRM_Handler);
		DRM_Handler(std::nullptr_t){}
		DRM_Handler(DRM_Paths drmPaths);
		DRM_Handler(const DRM_Handler&) = delete;
		DRM_Handler(DRM_Handler&&) = default;
		~DRM_Handler();

		vec<DRM_Device::id_t> refreshDevices();

		constexpr const map<DRM_Device::id_t, DRM_Device>& getDevices() const { return drmDevices; }

		static vec<str> listDrmDevices();

	private:
		friend class VInstance;
		DRM_Device takeDevice(const DRM_Device::id_t& id) {
			auto dev = std::move(drmDevices.at(id));
			drmDevices.erase(id);
			return dev;
		}

		const DRM_Paths drmPaths;
		map<DRM_Device::id_t, DRM_Device> drmDevices;
	};
}


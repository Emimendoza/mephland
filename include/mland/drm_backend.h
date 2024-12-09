#pragma once
#include <utility>

#include "common.h"
#include "vdevice.h"
#include "vdisplay.h"
#include "vinstance.h"

namespace mland::_details {
struct DrmId {
	int64_t major{};
	int64_t minor{};
	bool operator==(const DrmId& other) const { return other.major == major && other.minor == minor; }
};
}
template <>
struct std::hash<mland::_details::DrmId> {
	std::size_t operator()(const mland::_details::DrmId id) const noexcept {
		return std::hash<uint64_t>{}(id.major) ^ std::hash<uint64_t>{}(id.minor);
	}
};
namespace mland {
class DrmBackend final : public Backend {
public:
	MCLASS(DrmBackend);
	class DrmDevice;
	class DrmVInstance;
	class DrmVDevice;
	class DrmVDisplay;

	using DrmId = _details::DrmId;


	struct DrmPaths {
		vec<str> explicitInclude;
		vec<str> explicitExclude;
	};

	DrmBackend(DrmPaths& drmPaths);
	DrmBackend(const DrmBackend&) = delete;
	DrmBackend(DrmBackend&&) = delete;
	~DrmBackend() override;
	vec<DrmId> refreshDevices();
	constexpr const map<DrmId, DrmDevice>& getDevices() const { return drmDevices; }
	static vec<str> listDrmDevices();

	const vec<cstr>& requiredInstanceExtensions() const override;
	const vec<cstr>& requiredDeviceExtensions() const override;
	u_ptr<VInstance> createInstance(bool validation_layers) override;

	class DrmDevice {
	public:
		MCLASS(DrmDevice);
		typedef uint32_t Connector;

		~DrmDevice();
		DrmDevice(DrmDevice&& other) noexcept : id(other.id), fd(other.fd), name(std::move(other.name)) { other.fd = -1; }
		DrmDevice& operator=(DrmDevice&& other) noexcept{
			if (this == &other)
				return *this;
			id = other.id;
			fd = other.fd;
			name = std::move(other.name);
			other.fd = -1;
			return *this;
		}
		DrmDevice(const DrmDevice&) = delete;

		vec<Connector> refreshConnectors();
		DrmId id{};
		constexpr int getFd() const { return fd; }
		constexpr const str& getName() const { return name; }
		DrmDevice(std::nullptr_t) {}
	private:
		friend class DrmBackend;
		DrmDevice(str name, const DrmId id) : id(id), name(std::move(name)) {}

		int fd{-1};
		str name;
	};

private:
	friend class DrmVInstance;
	DrmDevice takeDevice(const DrmId& id) {
		auto dev = std::move(drmDevices.at(id));
		drmDevices.erase(id);
		return dev;
	}
	const DrmPaths drmPaths;
	map<DrmId, DrmDevice> drmDevices;
};

class DrmBackend::DrmVInstance final : public VInstance {
protected:
	friend class DrmBackend;
	DrmVInstance(const bool enableValidationLayers, DrmBackend& backend) : VInstance(enableValidationLayers, backend) {}
	opt<u_ptr<VDevice>> createDevice(vkr::PhysicalDevice&& physicalDevice, const vec<cstr>& extensions) override;
public:
	MCLASS(DrmVInstance);
	DrmVInstance(const DrmVInstance&) = delete;
	DrmVInstance(DrmVInstance&&) = delete;
	bool deviceGood(const vkr::PhysicalDevice& pDev) override;
};

class DrmBackend::DrmVDevice final : public VDevice {
public:
	MCLASS(DrmVDevice);
	DrmVDevice(const DrmVDevice&) = delete;
	DrmVDevice(DrmVDevice&&) = delete;

	opt<s_ptr<DrmVDisplay>> getDrmDisplay(DrmDevice::Connector con);
	constexpr const DrmDevice& getDRMDevice() const { return drmDev; }

	vec<s_ptr<VDisplay>> updateMonitors() override;

private:
	friend DrmVInstance;
	friend DrmVDisplay;
	DrmVDevice(vkr::PhysicalDevice&& physicalDevice, const vec<cstr>& extensions, DrmVInstance* parent);
	DrmDevice drmDev{nullptr};
	set<DrmDevice::Connector> connectors{};
};

class DrmBackend::DrmVDisplay final : public VDisplay {
public:
	MCLASS(DrmVDisplay);
	DrmVDisplay(const DrmVDisplay&) = delete;
	DrmVDisplay(DrmVDisplay&&) = delete;
	~DrmVDisplay() override;
protected:
	void createSurface() override;
private:
	friend class DrmVDevice;
	DrmVDisplay(str&&, DrmVDevice*, vkr::DisplayKHR&&, const vk::DisplayPropertiesKHR&, DrmDevice::Connector);
	uint32_t createModes();
	void createSurface(uint32_t mode_index);
	void deleteSurface() override;
	DrmDevice::Connector con{};
	vkr::SurfaceKHR vkrSurface{nullptr};
};
}

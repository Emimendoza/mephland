#pragma once
#include "vulk.h"
#include "vdevice.h"

namespace mland {
class Backend::VInstance {
public:
	MCLASS(VInstance);
	VInstance(const VInstance&) = delete;
	VInstance(VInstance&&) = delete;

	vec<VDevice::Id_t> refreshDevices();
	virtual bool deviceGood(const vkr::PhysicalDevice& pDev) = 0;

	constexpr VDevice& getDevice(const VDevice::Id_t& id) { return *devices.at(id); }
	constexpr vkr::Instance& getInstance() { return instance; }
	constexpr Backend* getBackend() { return backend; }

	virtual ~VInstance();
protected:
	VInstance(bool enableValidationLayers, Backend& backend);
	virtual opt<u_ptr<VDevice>> createDevice(vkr::PhysicalDevice&& physicalDevice, const vec<cstr>& extensions) = 0;
	static vkr::Context context;
	Backend* backend;
	vkr::Instance instance{nullptr};
	vkr::DebugUtilsMessengerEXT debugMessenger{nullptr};
	map<VDevice::Id_t, u_ptr<VDevice>> devices{};
};
}

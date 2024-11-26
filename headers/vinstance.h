#pragma once
#include "vulk.h"
#include "drm_backend.h"
#include "vdevice.h"

namespace mland {
class VInstance {
public:
	MCLASS(VInstance);
	VInstance(
		DRM_Handler::DRM_Paths drmDevs,
		bool enableValidationLayers = false
	);

	VInstance(std::nullptr_t) {}
	VInstance(const VInstance&) = delete;
	VInstance(VInstance&&) = delete;

	vec<VDevice::id_t> refreshDevices();

	constexpr VDevice& getDevice(const VDevice::id_t& id) { return *devices.at(id); }
	constexpr vkr::Instance& getInstance() { return instance; }

	~VInstance();

private:
	static vkr::Context context;
	DRM_Handler drmHandler{nullptr};
	vkr::Instance instance{nullptr};
	vkr::DebugUtilsMessengerEXT debugMessenger{nullptr};
	map<VDevice::id_t, u_ptr<VDevice>> devices{};
};
}

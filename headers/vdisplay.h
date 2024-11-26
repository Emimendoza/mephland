#pragma once
#include <condition_variable>
#include <thread>
#include "common.h"
#include "drm_backend.h"
#include "vulk.h"

namespace mland {
class VDisplay {
	friend VDevice;
	VDisplay(str&&, VDevice*, vkr::DisplayKHR&&, const vk::DisplayPropertiesKHR& displayProps,
	         DRM_Device::Connector con);

public:
	MCLASS(VDisplay);

	enum RenderingMode : uint32_t {
		eCompositing = 0 << 0,
		eDirect = 1 << 0,
		eHDR = 1 << 1, // TODO: IMPLEMENT
		eTearingFullscreen = 1 << 2,
		eTearingAllApps = 1 << 1
	};

	enum State : uint64_t {
		// Init sequence
		ePreInit,
		// main loop
		eIdle,
		eUpdateBackground,
		// Errors and cleanup
		eSubOptimal = std::numeric_limits<uint64_t>::max() / 2, // Potentially recoverable
		eError, // Unrecoverable
		eStop = std::numeric_limits<uint64_t>::max()
	};

	VDisplay(VDisplay&& other) noexcept = default;
	VDisplay& operator=(VDisplay&&) = default;
	VDisplay(const VDisplay&) = delete;
	~VDisplay();

	vec<vk::DisplayPlanePropertiesKHR> getDisplayPlaneProperties() const;
	typedef std::pair<vec<vk::SurfaceFormatKHR>, vec<vk::PresentModeKHR>> SurfaceInfo;
	SurfaceInfo getSurfaceInfo() const;
	static vk::SurfaceFormatKHR bestFormat(const vec<vk::SurfaceFormatKHR>& formatPool, bool HDR);
	static void requestRender();

	bool isGood();

	void setState(State state);

private:
	// <Rendered, Signal, Device>
	typedef std::tuple<vkr::Semaphore, vkr::Semaphore, VDevice*> DisplaySemaphore;
	typedef s_ptr<DisplaySemaphore> SharedDisplaySemaphore;
	typedef vec<SharedDisplaySemaphore> DisplaySemaphores;
	static std::mutex timelineMutex;
	static s_ptr<DisplaySemaphores> timelineSemaphores;
	struct impl;
	s_ptr<impl> p;
};
}

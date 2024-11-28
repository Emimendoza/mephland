#pragma once
#ifdef MLAND_SDL_BACKEND
#include "common.h"
#include "vdisplay.h"
#include "vinstance.h"
#include "vulk.h"

struct SDL_Window;

namespace mland {

class SdlBackend final : public Backend {
public:
	MCLASS(SdlBackend);
	SdlBackend(uint32_t maxWindows);
	~SdlBackend() override;
	class SdlVInstance;
	class SdlVDevice;
	class SdlVDisplay;
	const vec<cstr>& requiredInstanceExtensions() const override;
	const vec<cstr>& requiredDeviceExtensions() const override;
	u_ptr<VInstance> createInstance(bool validation_layers) override;

	friend SdlVInstance;
	friend SdlVDevice;
	friend SdlVDisplay;
private:
	vec<SDL_Window*> windows{};
};

class SdlBackend::SdlVInstance final : public VInstance {
public:
	MCLASS(SdlVInstance);
	bool deviceGood(const vkr::PhysicalDevice& pDev) override;
protected:
	opt<u_ptr<VDevice>> createDevice(vkr::PhysicalDevice&& physicalDevice, const vec<cstr>& extensions) override;
private:
	SdlVInstance(bool enableValidationLayers, SdlBackend& backend);
	std::unordered_set<SDL_Window*> taken{};
	friend SdlBackend;
	friend SdlVDevice;
	friend SdlVDisplay;
};

class SdlBackend::SdlVDevice final : public VDevice {
	friend SdlVDisplay;
public:
	MCLASS(SdlVDevice);
	vec<u_ptr<VDisplay>> updateMonitors() override;
protected:
	friend SdlVInstance;
	SdlVDevice(vkr::PhysicalDevice&& physicalDevice, const vec<cstr>& extensions, VInstance* parent);
};

class SdlBackend::SdlVDisplay final : public VDisplay {
public:
	MCLASS(SdlVDisplay);
	~SdlVDisplay() override;
protected:
	void createSurface() override;
	void deleteSurface() override;
private:
	struct SdlSurface {
		SDL_Window* window;
		SdlVDisplay* vDisplay;
		vk::SurfaceKHR surface;
		SdlSurface(SDL_Window*, SdlVDisplay*);
		~SdlSurface();
	};
	static std::atomic<uint32_t> displayCount;
	friend SdlVDevice;
	SDL_Window* window;
	SdlVDisplay(SDL_Window* window, SdlVDevice* sdlVDev);
	u_ptr<SdlSurface> sdlSurface;
	// ReSharper disable once CppRedundantCastExpression
	constexpr SdlVInstance& getInstance() const { return static_cast<SdlVInstance&>(*static_cast<SdlVDevice&>(*vDev).parent); }
};

}
#endif // MLAND_SDL_BACKEND
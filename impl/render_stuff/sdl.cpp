#ifdef MLAND_SDL_BACKEND
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include "mland/sdl_backend.h"
#include "mland/wayland_server.h"
using namespace mland;
using SdlVInstance = SdlBackend::SdlVInstance;
using SdlDevice = SdlBackend::SdlVDevice;
using SdlDisplay = SdlBackend::SdlVDisplay;

SdlBackend::SdlBackend(const uint32_t maxWindows, const s_ptr<WLServer>& server) : server(server) {
	MDEBUG << "Creating SDL Backend" << endl;
	static constexpr auto  windowName = "MephLand Compositor";
	static constexpr auto  windowWidth = 800;
	static constexpr auto  windowHeight = 600;
	static constexpr auto windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
	if (!SDL_Init(SDL_INIT_VIDEO) || !SDL_Vulkan_LoadLibrary(nullptr) ) {
		MERROR << "Failed to create SDL Backend: " << SDL_GetError() << endl;
		throw std::runtime_error("SDL Error");
	}
	for (uint32_t i = 0; i < maxWindows; i++) {
		SDL_Window* window = SDL_CreateWindow(windowName, windowWidth, windowHeight, windowFlags);
		if (!window) {
			MERROR << "Failed to create window: " << SDL_GetError() << endl;
			throw std::runtime_error("SDL Error");
		}
		windows.push_back(window);
	}
	thread = std::thread(&SdlBackend::run, this);
}

void SdlBackend::run() {
	MDEBUG << "Running SDL Backend" << endl;
	while (!stop.test()) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
				MINFO << "Received quit event" << endl;
				server->stop();
				return;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

SdlBackend::~SdlBackend() {
	stop.test_and_set();
	if (thread.joinable())
		thread.join();
	for (const auto window : windows) {
		SDL_DestroyWindow(window);
	}
	SDL_Vulkan_UnloadLibrary();
	SDL_Quit();
}

const vec<cstr>& SdlBackend::requiredInstanceExtensions() const {
	static uint32_t count;
	static char const* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);
	static vec<cstr> ext;
	if (ext.empty()) {
		for (uint32_t i = 0; i < count; i++) {
			ext.push_back(extensions[i]);
		}
	}
	return ext;
}

const vec<cstr>& SdlBackend::requiredDeviceExtensions() const {
	static constexpr vec<cstr> extensions{};
	return extensions;
}

u_ptr<VInstance> SdlBackend::createInstance(const bool validation_layers) {
	return u_ptr<VInstance>(new SdlVInstance(validation_layers, *this));
}

SdlVInstance::SdlVInstance(const bool enableValidationLayers, SdlBackend& backend) : VInstance(enableValidationLayers, backend) {
	MDEBUG << "Created SDL Vulkan instance" << endl;
}

bool SdlVInstance::deviceGood(const vkr::PhysicalDevice& pDev) {
	return true;
}

opt<u_ptr<VDevice>> SdlVInstance::createDevice(vkr::PhysicalDevice&& physicalDevice, const vec<cstr>& extensions) {
	return u_ptr<VDevice>(new SdlVDevice(std::move(physicalDevice), extensions, this));
}

SdlDevice::SdlVDevice(vkr::PhysicalDevice&& physicalDevice, const vec<cstr>& extensions, VInstance* parent) :
VDevice(std::move(physicalDevice), extensions, parent) {}

vec<s_ptr<VDisplay>> SdlDevice::updateMonitors() {
	MDEBUG << "Updating monitors for device " << name << endl;
	auto& instance = static_cast<SdlVInstance&>(*parent);
	auto& back = static_cast<SdlBackend&>(*instance.getBackend());

	if (back.windows.size() == instance.taken.size()) [[likely]]
		return {}; // No new windows

	vec<s_ptr<VDisplay>> ret;
	for (auto& window : back.windows) {
		if (instance.taken.contains(window)) [[likely]]
			continue;
		instance.taken.insert(window);
		ret.push_back(s_ptr<VDisplay>(new SdlDisplay(window, this)));
	}
	return ret;
}

SdlDisplay::SdlVDisplay(SDL_Window* window, SdlVDevice* sdlVDev) :  VDisplay("SdlDisplay " + std::to_string(displayCount++), sdlVDev), window(window) {
	MDEBUG << name << " Created display" << endl;
	start();
}

SdlDisplay::~SdlVDisplay() {
	MDEBUG << name << " Destroying display" << endl;
	stop();
	getInstance().taken.erase(window);
}

void SdlDisplay::createSurface() {
	MDEBUG << name << " Creating surface" << endl;
	deleteSurface();
	sdlSurface = u_ptr<SdlSurface>(new SdlSurface(window, this));
	surface = sdlSurface->surface;
}

void SdlDisplay::deleteSurface() {
	sdlSurface.reset();
	surface = nullptr;
}

SdlDisplay::SdlSurface::SdlSurface(SDL_Window* window, SdlVDisplay* vDisplay) : window(window), vDisplay(vDisplay) {
	const auto& instance = vDisplay->getInstance();
	VkSurfaceKHR surf;
	if (!SDL_Vulkan_CreateSurface(window, *instance.instance, nullptr, &surf)) {
		MERROR << "Failed to create surface: " << SDL_GetError() << endl;
		throw std::runtime_error("SDL Error");
	}
	surface = surf;
}
SdlDisplay::SdlSurface::~SdlSurface() {
	vkDestroySurfaceKHR(*vDisplay->getInstance().instance, surface, nullptr);
}

std::atomic<uint32_t> SdlDisplay::displayCount = 0;
#endif // MLAND_SDL_BACKEND
#include <mland/sdl3_backend.h>
#include <mland/renderer.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <vulkan/vulkan.hpp>

using namespace mland;

namespace {
void sdl_assert(const bool condition) {
	if (condition)
		return;
	std::cerr << SDL_GetError() << std::endl;
	exit(-1);
}

template<typename T>
T* sdl_ptr_assert(T* ptr) {
	sdl_assert(ptr);
	return ptr;
}

class sdl3_display final : public display {
public:
	~sdl3_display() override;
	[[nodiscard]]
	const vk::SurfaceKHR& get_surface() const noexcept override { return m_surface; }

	SDL_Window* m_window{};
	vk::Instance m_instance{};
	vk::SurfaceKHR m_surface{};
};

}

sdl3_display::~sdl3_display() {
	if (m_surface) {
        SDL_Vulkan_DestroySurface(m_instance, m_surface, nullptr);
    }

    // Destroy the SDL window
	if (m_window) {
        SDL_DestroyWindow(m_window);
    }
}

sdl3_backend::sdl3_backend(const uint32_t max_window_count) : m_max_window_count(max_window_count), m_window_count(0) {
	sdl_assert(SDL_Init(SDL_INIT_VIDEO));
	sdl_assert(SDL_Vulkan_LoadLibrary(nullptr));
}

sdl3_backend::~sdl3_backend() {
	SDL_Vulkan_UnloadLibrary();
	SDL_Quit();
}

void sdl3_backend::append_vulkan_instance_extensions(std::unordered_set<std::string>& extensions) {
	uint32_t sdl_extension_count = 0;
	const auto sdl_extensions = sdl_ptr_assert(SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count));
	for (uint32_t i = 0; i < sdl_extension_count; ++i) {
		extensions.insert(sdl_extensions[i]);
	}
}

void sdl3_backend::append_vulkan_device_extensions(std::unordered_set<std::string>& extensions) {

}

bool sdl3_backend::is_device_good(const vk::raii::PhysicalDevice& device) {
	// SDL doesn't require any specific device features or properties,
    // so we can assume all devices are good.
    return true;
}

std::optional<display_p> sdl3_backend::get_display(const renderer& renderer) {
	constexpr auto title = "mland SDL3 Window";
	if (m_window_count >= m_max_window_count) {
        return std::nullopt; // No more windows can be created
    }
	auto w = SDL_CreateWindow(title, 640, 480, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!w) {
		return std::nullopt; // TODO: log error
	}

	vk::SurfaceKHR surface;
	if (!SDL_Vulkan_CreateSurface(w, renderer.get_instance(), nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface))) {
        SDL_DestroyWindow(w);
        return std::nullopt; // TODO: log error
    }
	m_window_count++;
	const auto d = new sdl3_display();
	d->m_instance = renderer.get_instance();
	d->m_window = w;
	d->m_surface = surface;
	return std::unique_ptr<display>(d);
}



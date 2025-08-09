#pragma once
#include "backend.h"

namespace mland {
class sdl3_backend final : public backend {
public:
	sdl3_backend(uint32_t max_window_count = 1);
    ~sdl3_backend() override;

    void append_vulkan_instance_extensions(std::unordered_set<std::string>& extensions) override;
   	void append_vulkan_device_extensions(std::unordered_set<std::string>& extensions) override;
	bool is_device_good(const vk::raii::PhysicalDevice& device) override;
	std::optional<display_p> get_display(const renderer& renderer) override;
private:
	const uint32_t m_max_window_count;
	uint32_t m_window_count;
};
}
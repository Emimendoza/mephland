#pragma once
#include <unordered_set>
#include <memory>
#include <string>
#include <optional>

// Forward declarations
namespace vk::raii {
struct PhysicalDevice;
}
namespace vk {
struct SurfaceKHR;
}

namespace mland {
class renderer;
class backend;
class display;
using shared_backend = std::shared_ptr<backend>;
using display_p = std::unique_ptr<display>;
class backend {
public:
	virtual ~backend() = default;

	virtual void append_vulkan_instance_extensions(std::unordered_set<std::string>& extensions) = 0;
	virtual void append_vulkan_device_extensions(std::unordered_set<std::string>& extensions) = 0;
	// Check for backend specific device requirements
	virtual bool is_device_good(const vk::raii::PhysicalDevice& device) = 0;
	// Create a display surface (if available)
	virtual std::optional<display_p> get_display(const renderer& renderer) = 0;
protected:
	backend() = default;
};

class display {
public:
	virtual ~display() = default;

	[[nodiscard]]
	virtual const vk::SurfaceKHR& get_surface() const noexcept = 0;
protected:
	display() = default;
};

}

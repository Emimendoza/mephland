// ReSharper disable CppMemberFunctionMayBeConst
#include <mland/renderer.h>
#include <utility>
#include <vulkan/vulkan_raii.hpp>
#include <list>
#include <expected>
#include <ranges>


using namespace mland;
namespace vkr = vk::raii;

namespace {
std::vector<const char*> to_vector(const std::unordered_set<std::string>& set) {
	std::vector<const char*> result;
	result.reserve(set.size());
	for (const auto& str : set) {
		result.push_back(str.c_str());
	}
	return result;
}

constexpr vk::ApplicationInfo application_info = {
	"mland",
	VK_MAKE_VERSION(0, 1, 0),
	"mland",
	VK_MAKE_VERSION(0, 1, 0),
	VK_API_VERSION_1_3
};
class display_ctx {
public:
	display_ctx(display_p&& d) : display{std::move(d)} {}
private:
	display_p display;
	vkr::SwapchainKHR swapchain{nullptr};

	void create_swapchain();

};

struct Device {
	vkr::PhysicalDevice pdev{nullptr};
	vkr::Device ldev{nullptr};
	vkr::Queue queue{nullptr};
	Device() = delete;
	Device(const Device&) = delete;
	Device(Device&&) noexcept;
	Device(vkr::PhysicalDevice  p, const vk::DeviceCreateInfo&);
	~Device();
};

}

Device::Device(vkr::PhysicalDevice  p, const vk::DeviceCreateInfo& create_info) : pdev(std::move(p)) {
	assert(create_info.queueCreateInfoCount>0);
	ldev = vkr::Device(pdev, create_info);
	queue = ldev.getQueue(create_info.pQueueCreateInfos[0].queueFamilyIndex, 0);
}
Device::Device(Device&& other) noexcept{
	std::swap(pdev, other.pdev);
	std::swap(ldev, other.ldev);
	std::swap(queue, other.queue);
}

Device::~Device() {
	queue.clear();
	ldev.clear();
	pdev.clear();
}


struct renderer::impl {
	shared_backend backend;
	vkr::Context context{};
	vkr::Instance instance{nullptr};
	std::list<Device> devices;
	std::list<display_ctx> displays;
	impl(shared_backend b) : backend(std::move(b)) {}
	~impl();

	void create_instance(bool v);
	void create_devices();

	// helpers
private:
	[[nodiscard]]
	std::expected<Device, std::string> createDevice(const vkr::PhysicalDevice& p) const;
};



renderer::renderer(const shared_backend& backend, const bool validation_layers) : _p(std::make_unique<impl>(backend)) {
	if (!_p->backend) {
		throw std::runtime_error("Backend is not initialized");
	}
	_p->create_instance(validation_layers);
	_p->create_devices();
	if (_p->devices.empty()) {
		throw std::runtime_error("No valid Vulkan devices found");
	}
}

vk::Instance renderer::get_instance() const {
    return _p->instance;
}

void renderer::impl::create_instance(const bool v) {
	std::unordered_set<std::string> enabled_layers{};
	std::unordered_set<std::string> instance_extensions{vk::KHRSurfaceExtensionName};

	if (v) {
		enabled_layers.insert("VK_LAYER_KHRONOS_validation");
	}

	backend->append_vulkan_instance_extensions(instance_extensions);

	auto l = to_vector(enabled_layers);
	auto i = to_vector(instance_extensions);

	std::unordered_set<std::string> l_names;
	for (auto& lp : context.enumerateInstanceLayerProperties()) {
		l_names.insert(lp.layerName);
	}

	for (const auto& layer : l) {
		if (!l_names.contains(layer)) {
			throw std::runtime_error("Requested Vulkan layer not found: " + std::string(layer));
		}
	}

	std::unordered_set<std::string> e_names;
	for (auto& ep : context.enumerateInstanceExtensionProperties()) {
		e_names.insert(ep.extensionName);
	}

	for (const auto& extension : i) {
		if (!e_names.contains(extension)) {
			throw std::runtime_error("Requested Vulkan extension not found: " + std::string(extension));
		}
	}

	const vk::InstanceCreateInfo create_info = {
		{},
		&application_info,
		l,
		i,
	};

	instance = vkr::Instance(context, create_info);
}

void renderer::impl::create_devices() {
	for (const auto& physical_device : instance.enumeratePhysicalDevices()) {
		if (auto device = createDevice(physical_device)) {

        }
	}
}

// Helpers

std::expected<Device, std::string> renderer::impl::createDevice(const vkr::PhysicalDevice& p) const {
	static std::unordered_set<std::string> device_extensions{vk::KHRSwapchainExtensionName};
	static std::vector<const char*> de;
	static bool requirements_initialized = false;
	if (!requirements_initialized) {
		backend->append_vulkan_device_extensions(device_extensions);
		de = to_vector(device_extensions);
		requirements_initialized = true;
	}

	std::unordered_set<std::string> e_names;
	for (auto& ep : p.enumerateDeviceExtensionProperties()) {
		e_names.insert(ep.extensionName);
	}

	for (const auto& extension : de) {
		if (!e_names.contains(extension)) {
			return std::unexpected("Requested Vulkan device extension(s) not found");
		}
	}

	if (!backend->is_device_good(p)) {
        return std::unexpected("Device does not meet backend requirements");
    }

	static constexpr float queue_priority = 1.0f;
	std::optional<vk::DeviceQueueCreateInfo> queue_create_info;


	for (const auto [index, qfp] : std::views::enumerate(p.getQueueFamilyProperties())) {
		if ((qfp.queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlagBits::eGraphics) {
            continue; // Skip non-graphics queues
        }
		queue_create_info = vk::DeviceQueueCreateInfo{
            {},
            static_cast<uint32_t>(index),
            1,
            &queue_priority
        };
		break;
	}
	if (!queue_create_info) {
        return std::unexpected("No suitable queue found for device");
    }

	const vk::DeviceCreateInfo create_info{
		{},
		{queue_create_info.value()},
		{},
		de
	};


	return Device(p, create_info);
}

void renderer::refresh_displays() {
	while (true) {
		auto d = _p->backend->get_display(*this);
        if (!d) {
	        break; // No more displays can be created
        }
		_p->displays.emplace_back(std::move(d.value()));
	}
}


renderer::~renderer() = default;
renderer::impl::~impl() {
    displays.clear();
	devices.clear();
}
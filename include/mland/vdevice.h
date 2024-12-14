#pragma once

#include <unordered_set>
#include "common.h"
#include "vulk.h"

namespace mland {
class Backend::VDevice {
public:
	enum struct Id_t : int64_t { INVALID = -1 };
protected:
	VDevice(vkr::PhysicalDevice&& physicalDevice, const vec<cstr>& extensions, VInstance* parent);
	struct Queue {
		std::mutex mutex{};
		vkr::Queue queue;
		Queue(vkr::Queue&& queue) : queue(std::move(queue)) {}
	};

	VInstance* parent{nullptr};
	vkr::PhysicalDevice pDev{nullptr};
	vkr::Device dev{nullptr};
	map<uint32_t, Queue> queues{};
	vkr::ShaderModule vertShader{nullptr};
	vkr::ShaderModule fragShader{nullptr};
	friend class VInstance;
	friend class VDisplay;

	static constexpr vk::Fence nullFence{nullptr};
public:
	MCLASS(VDevice);

	friend std::ostream& operator<<(std::ostream& os, const Id_t& i) { return os << static_cast<int64_t>(i); }

	const Id_t id{};
	const str name{};
	const uint32_t graphicsIndex{0};
	const uint32_t transferIndex{0};
	const bool good;
	VDevice(const VDevice&) = delete;
	VDevice(VDevice&&) = delete;
	virtual ~VDevice() = default;

	vkr::CommandPool createCommandPool(uint32_t queueFamilyIndex);
	vkr::CommandBuffer createCommandBuffer(const vkr::CommandPool& pool);

	void submit(uint32_t queueFamilyIndex, const vk::SubmitInfo& submitInfo, const vk::Fence& fence = nullFence);
	vk::Result present(uint32_t queueFamilyIndex, const vk::PresentInfoKHR& presentInfo);

	void waitIdle(uint32_t queueFamilyIndex);

	opt<vkr::ShaderModule> createShaderModule(const VShader& shader);

	constexpr const vkr::ShaderModule& getVert() const { return vertShader; }
	constexpr const vkr::ShaderModule& getFrag() const { return fragShader; }

	virtual vec<s_ptr<VDisplay>> updateMonitors() = 0;
};
}

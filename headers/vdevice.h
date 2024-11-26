#pragma once

#include <unordered_set>
#include "common.h"
#include "drm_backend.h"
#include "vulk.h"

namespace mland {
class VDevice {
public:
	MCLASS(VDevice);

	enum struct id_t : int64_t { INVALID = -1 };

	friend std::ostream& operator<<(std::ostream& os, const id_t& id) { return os << static_cast<int64_t>(id); }

private:
	struct Queue {
		std::mutex mutex{};
		vkr::Queue queue;
		Queue(vkr::Queue&& queue) : queue(std::move(queue)) {}
	};

	VInstance* parent{nullptr};
	vkr::PhysicalDevice pDev{nullptr};
	vkr::Device dev{nullptr};
	DRM_Device drmDev;
	std::unordered_set<DRM_Device::Connector> connectors{};
	map<uint32_t, Queue> queues{};
	vkr::ShaderModule vertShader{nullptr};
	vkr::ShaderModule fragShader{nullptr};
	friend class VInstance;
	friend class VDisplay;

	VDevice(const id_t id, DRM_Device&& drmDev, str&& name) : drmDev(std::move(drmDev)), id(id),
	                                                          name(std::move(name)) {}
	static constexpr vk::Fence nullFence{nullptr};
public:
	const id_t id;
	const str name;
	const uint32_t graphicsQueueFamilyIndex{0};
	const uint32_t transferQueueFamilyIndex{0};
	VDevice(const VDevice&) = delete;
	VDevice(VDevice&&) = delete;
	~VDevice() = default;

	vkr::CommandPool createCommandPool(uint32_t queueFamilyIndex);
	vkr::CommandBuffer createCommandBuffer(const vkr::CommandPool& pool);

	void submit(uint32_t queueFamilyIndex, const vk::SubmitInfo& submitInfo, const vk::Fence& fence = nullFence);
	vk::Result present(uint32_t queueFamilyIndex, const vk::PresentInfoKHR& presentInfo);

	opt<VDisplay> getDRMDisplay(DRM_Device::Connector con);
	opt<vkr::ShaderModule> createShaderModule(const VShader& shader);
	constexpr const DRM_Device& getDRMDevice() const { return drmDev; }
	constexpr const vkr::ShaderModule& getVert() const { return vertShader; }
	constexpr const vkr::ShaderModule& getFrag() const { return fragShader; }


	vec<VDisplay> updateMonitors();
};
}

#pragma once

#include "common.h"
#include "drm_backend.h"
#include "vulk.h"

namespace mland {
	class VDevice {
	public:
		MCLASS(VDevice);
		enum struct id_t : int64_t {INVALID=-1};
		friend std::ostream& operator<<(std::ostream& os, const id_t& id) { return os << static_cast<int64_t>(id);}
	private:
		VInstance* parent{nullptr};
		vkr::PhysicalDevice pDev{nullptr};
		vkr::Device dev{nullptr};
		DRM_Device drmDev;
		map<uint32_t, std::mutex> queueMutexes{};
		map<uint32_t, vkr::Queue> queues{};
		vkr::ShaderModule vertShader{nullptr};
		vkr::ShaderModule fragShader{nullptr};
		friend class VInstance;
		friend class VDisplay;
		VDevice(const id_t id, DRM_Device&& drmDev, str&& name) : drmDev(std::move(drmDev)), id(id), name(std::move(name)) {}
	public:
		const id_t id;
		const str name;
		const uint32_t graphicsQueueFamilyIndex{0};
		const uint32_t transferQueueFamilyIndex{0};
		VDevice(VDevice&& other) noexcept = default;
		VDevice(const VDevice&) = delete;
		~VDevice() = default;

		vkr::CommandPool createCommandPool(uint32_t queueFamilyIndex);
		vkr::CommandBuffer createCommandBuffer(uint32_t queueFamilyIndex, const vkr::CommandPool& pool);

		void submit(uint32_t queueFamilyIndex, const vk::SubmitInfo& submitInfo, const vkr::Fence& fence);
		vk::Result present(uint32_t queueFamilyIndex, const vk::PresentInfoKHR& presentInfo);

		opt<VDisplay> getDRMDisplay(uint32_t connectorId);
		opt<vkr::ShaderModule> createShaderModule(const VShader& shader);
		constexpr const DRM_Device& getDRMDevice() const { return drmDev; }
		constexpr const vkr::ShaderModule& getVert() const { return vertShader; }
		constexpr const vkr::ShaderModule& getFrag() const { return fragShader; }


		vec<VDisplay> updateMonitors();
	};
}
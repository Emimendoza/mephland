#include "vdevice.h"
#include "vdisplay.h"
#include "drm_backend.h"
#include "vshaders.h"
using namespace mland;

opt<VDisplay> VDevice::getDRMDisplay(const DRM_Device::Connector con) {
	auto res = pDev.getDrmDisplayEXT(drmDev.getFd(), con);
	if (!res.has_value()) {
		MERROR << "Failed to get display for connector: " << to_str(res.error()) << endl;
		return std::nullopt;
	}
	pDev.acquireDrmDisplayEXT(drmDev.getFd(), res.value());
	auto allDisplayProps = pDev.getDisplayPropertiesKHR();
	vk::DisplayPropertiesKHR displayProps{nullptr};
	for (const auto& prop : allDisplayProps) {
		if (prop.display == res.value()) {
			displayProps = prop;
			break;
		}
	}
	if (displayProps.display == VK_NULL_HANDLE) {
		MERROR << name << " Failed to get display properties" << endl;
		return std::nullopt;
	}
	VDisplay display(name + ' ' + displayProps.displayName, this, std::move(res.value()), displayProps, con);
	if (!display.isGood()) {
		MERROR << name << " Display is not good" << endl;
		return std::nullopt;
	}

	MDEBUG << name << " Created display for connector " << con << endl;
	return std::make_optional(std::move(display));
}

vkr::CommandBuffer VDevice::createCommandBuffer(const vkr::CommandPool& pool) {
	const vk::CommandBufferAllocateInfo cmdBufferAllocInfo{
		.commandPool = pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	};

	auto res = dev.allocateCommandBuffers(cmdBufferAllocInfo);
	if (!res.has_value()) {
		throw std::runtime_error(name + " Failed to allocate command buffer: " + to_str(res.error()));
	}
	assert(res.value().size() == 1);
	MDEBUG << name << " Created command buffer" << endl;
	return std::move(res.value().front());
}

vec<VDisplay> VDevice::updateMonitors() {
	vec<VDisplay> ret;
	const auto cons = drmDev.refreshConnectors();
	MDEBUG << "Updating monitors for device " << name << endl;
	for (const auto& con : cons) {
		if (connectors.contains(con)) {
			MDEBUG << name << " Already have display for connector " << con << endl;
			continue;
		}
		auto res = getDRMDisplay(con);
		if (!res.has_value()) {
			MERROR << name << " Failed to get display for connector " << con << endl;
			continue;
		}
		MINFO << name << " Found display for connector " << con << endl;
		auto& display = res.value();
		ret.emplace_back(std::move(display));
	}
	return ret;
}

opt<vkr::ShaderModule> VDevice::createShaderModule(const VShader& shader) {
	const vk::ShaderModuleCreateInfo shaderCreateInfo{
		.codeSize = shader.len,
		.pCode = static_cast<const uint32_t*>(shader.bytes)
	};
	auto res = dev.createShaderModule(shaderCreateInfo);
	if (!res.has_value()) {
		MERROR << name << " Failed to create shader module: " << to_str(res.error()) << endl;
		return std::nullopt;
	}
	MDEBUG << name << " Created shader module" << endl;
	return std::make_optional(std::move(res.value()));
}

vkr::CommandPool VDevice::createCommandPool(const uint32_t queueFamilyIndex) {
	const vk::CommandPoolCreateInfo cmdPoolCreateInfo{
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = queueFamilyIndex
	};
	auto cmdRes = dev.createCommandPool(cmdPoolCreateInfo);
	if (!cmdRes.has_value()) [[unlikely]]{
		throw std::runtime_error(name + " Failed to create command pool: " + to_str(cmdRes.error()));
	}
	MDEBUG << name << " Created command pool" << endl;
	return std::move(cmdRes.value());
}

void VDevice::submit(const uint32_t queueFamilyIndex, const vk::SubmitInfo& submitInfo, const vk::Fence& fence) {
	auto& [mutex, queue] = queues.at(queueFamilyIndex);
	std::lock_guard lock(mutex);
	queue.submit(submitInfo, fence);
}

vk::Result VDevice::present(const uint32_t queueFamilyIndex, const vk::PresentInfoKHR& presentInfo) {
	auto& [mutex, queue] = queues.at(queueFamilyIndex);
	std::lock_guard lock(mutex);
	return queue.presentKHR(presentInfo);
}


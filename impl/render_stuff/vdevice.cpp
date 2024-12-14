#include "mland/vdevice.h"
#include "mland/vdisplay.h"
#include "mland/vshaders.h"
using namespace mland;

template <typename T>
static constexpr uint8_t countBits(T bits) {
	static_assert(std::numeric_limits<uint8_t>::max() > sizeof(T)*8);
	uint8_t count = 0;
	for(uint8_t i = 0; i < sizeof(T) * 8; i++) { // NOLINT(*-too-small-loop-variable)
		if(bits & 1)
			count++;
		bits <<= 1;
	}
	return count;
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

void VDevice::waitIdle(const uint32_t queueFamilyIndex) {
	auto& [mutex, queue] = queues.at(queueFamilyIndex);
	queue.waitIdle();
}

VDevice::VDevice(vkr::PhysicalDevice&& physicalDevice, const vec<cstr>& extensions, VInstance* parent) :
parent(parent),
pDev(std::move(physicalDevice)),
good(false) {
	const_cast<str&> (name) = pDev.getProperties().deviceName.data();
	static constexpr auto max = std::numeric_limits<uint32_t>::max();
	auto graphicsFamilyQueueIndex{max};
	auto transferFamilyQueueIndex{max};
	uint8_t transferBits{std::numeric_limits<uint8_t>::max()};
	const auto queueFamilyProperties = pDev.getQueueFamilyProperties();

	for (size_t j = 0 ; j < queueFamilyProperties.size(); j++) {
		constexpr auto graphicsFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eTransfer;
		constexpr auto transferFlags = vk::QueueFlagBits::eTransfer;
		const auto& queueFamily = queueFamilyProperties[j];
		if (graphicsFamilyQueueIndex == max && queueFamily.queueFlags & graphicsFlags ) {
			MDEBUG << name  << " Found graphics queue family " << to_str(queueFamily.queueFlags) << " index: " << j  << endl;
			graphicsFamilyQueueIndex = j;
		}
		if (queueFamily.queueFlags & transferFlags && countBits(static_cast<uint32_t>(queueFamily.queueFlags)) < transferBits) {
			transferFamilyQueueIndex = j;
			transferBits = countBits(static_cast<uint32_t>(queueFamily.queueFlags));
		}
	}
	if (graphicsFamilyQueueIndex == max) {
		MWARN << "Could not find a queue family with graphics capabilities for device " << name << endl;
		return;
	}
	const_cast<uint32_t&>(graphicsIndex) = graphicsFamilyQueueIndex;
	const_cast<uint32_t&>(transferIndex) = transferFamilyQueueIndex;
	MDEBUG << name << " Found transfer queue family " <<
		to_str(queueFamilyProperties[transferFamilyQueueIndex].queueFlags) << " index: " << transferFamilyQueueIndex << endl;
	set graphicsIndex{graphicsFamilyQueueIndex, transferFamilyQueueIndex};

	vec<vk::DeviceQueueCreateInfo> queueCreateInfos;
	constexpr float queuePriority = 1.0f;
	for (const auto& queueFamilyIndex : graphicsIndex) {
		queueCreateInfos.push_back({
			.queueFamilyIndex = queueFamilyIndex,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority
		});
	}

	static constexpr  vk::PhysicalDeviceVulkan12Features deviceFeatures{
		.timelineSemaphore = vk::True
	};

	const vk::DeviceCreateInfo deviceCreateInfo{
		.pNext = &deviceFeatures,
		.queueCreateInfoCount = queueCreateInfos.size32(),
		.pQueueCreateInfos = queueCreateInfos.data(),
		.enabledExtensionCount = extensions.size32(),
		.ppEnabledExtensionNames = extensions.data()
	};

	auto result = pDev.createDevice(deviceCreateInfo);
	if (!result.has_value()) {
		MERROR << name << " Could not create device: " << to_str(result.error()) << endl;
		return;
	}
	const_cast<Id_t&>(id) = static_cast<Id_t>(pDev.getProperties().deviceID);
	dev = std::move(result.value());

	for (const auto& j : graphicsIndex) {
		auto queue = dev.getQueue(j, 0);
		if (!queue.has_value()) {
			MERROR << name << " Could not get queue " << to_str(queue.error()) << endl;
			return;
		}
		MDEBUG << name << " Got queue " << j << endl;
		queues.emplace(j, std::move(queue.value()));
	}
	auto vertShader = createShaderModule(VERT_SHADER);
	auto fragShader = createShaderModule(FRAG_SHADER);
	if (!vertShader.has_value() || !fragShader.has_value()) {
		MERROR << name << " Could not create shader modules" << endl;
		return;
	}
	this->vertShader = std::move(vertShader.value());
	this->fragShader = std::move(fragShader.value());
	const_cast<bool&>(good) = true;
	MDEBUG << name << " Created device " << endl;
}

#define MLAND_VINSTANCE_IMPL
#include <unordered_set>
#include "vulk.h"
#include "vinstance.h"
#include "vshaders.h"
#include "globals.h"


using namespace mland;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData
) {
	MCLASS(ValLayers);
	switch (messageSeverity) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			MINFO << pCallbackData->pMessage << endl;
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			MERROR << pCallbackData->pMessage << endl;
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			MDEBUG << pCallbackData->pMessage << endl;
			break;
		default:
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			MWARN << pCallbackData->pMessage << endl;
		break;
	}

	return VK_FALSE;

}

template <typename T>
static constexpr uint8_t countBits(T bits) {
	static_assert(std::numeric_limits<uint8_t>::max() > sizeof(T)*8);
	uint8_t count = 0;
	for(uint8_t i = 0; i < sizeof(T) * 8; i++) {
		if(bits & 1)
			count++;
		bits <<= 1;
	}
	return count;
}

VInstance::VInstance(DRM_Handler::DRM_Paths drmDevs, const bool enableValidationLayers) :
drmHandler(std::move(drmDevs)) {
	MDEBUG << "Creating Vulkan instance" << endl;
	constexpr vk::ApplicationInfo appInfo{
		.pApplicationName = "MephLand Compositor",
		.applicationVersion = VK_MAKE_VERSION(0, 0, 0),
		.pEngineName = "MephLand",
		.engineVersion = VK_MAKE_VERSION(0, 0, 0),
		.apiVersion = VK_API_VERSION_1_3
	};

	vec<cstr> enabledLayerNames;
	vec<cstr> enabledExtensionNames{
		vk::KHRSurfaceExtensionName,
		vk::KHRDisplayExtensionName,
		vk::EXTDirectModeDisplayExtensionName,
		vk::EXTAcquireDrmDisplayExtensionName
	};

	// Optional stuff

	if (enableValidationLayers) {
		MINFO << "Enabling validation layers" << endl;
		enabledLayerNames.push_back(VULKAN_VALIDATION_LAYER_NAME);
		enabledExtensionNames.push_back(vk::EXTDebugUtilsExtensionName);
	}

	const vk::InstanceCreateInfo createInfo{
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = enabledLayerNames.size32(),
		.ppEnabledLayerNames = enabledLayerNames.data(),
		.enabledExtensionCount = enabledExtensionNames.size32(),
		.ppEnabledExtensionNames = enabledExtensionNames.data()
	};

	auto inst = createInstance(createInfo);
	if (inst.result != vk::Result::eSuccess) {
		MERROR << "Failed to create Vulkan instance:" << to_str(inst.result) << endl;
		throw std::runtime_error("Failed to create Vulkan instance");
	}

	instance = {context, inst.value};
	MDEBUG << "Created Vulkan instance" << endl;
	if (enableValidationLayers) {
		vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{
			.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
			.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
				vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
				vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
			.pfnUserCallback = &debugCallback
		};


		auto res = instance.createDebugUtilsMessengerEXT(debugCreateInfo);
		if (!res.has_value()) {
			MERROR << "Failed to create debug messenger: " << to_str(res.error()) << endl;
			throw std::runtime_error("Failed to create debug messenger");
		}
		debugMessenger = std::move(res.value());
		MDEBUG << "Created debug messenger" << endl;
	}
}

vec<VDevice::id_t> VInstance::refreshDevices() {
	MDEBUG << "Reloading devices" << endl;
	vec<VDevice::id_t> ret;

	const vec<cstr> deviceExtensions{
		vk::EXTPhysicalDeviceDrmExtensionName,
		vk::KHRSwapchainExtensionName
	};

	auto res = instance.enumeratePhysicalDevices();
	if (!res.has_value()) {
		MERROR << "Failed to enumerate devices" << to_str(res.error()) << endl;
		return ret;
	}

	for (auto& pDev : res.value()) {
		const auto id = static_cast<VDevice::id_t>(pDev.getProperties().deviceID);
		if (devices.contains(id)) {
			MDEBUG << "Device " << pDev.getProperties().deviceName << " already exists" << endl;
			continue;
		}
		auto props = pDev.enumerateDeviceExtensionProperties();
		vec<str> availableExtensions;
		for (const auto& [extensionName, specVersion] : props) {
			availableExtensions.push_back(extensionName.data());
		}

		bool hasAllExtensions = true;
		for (const auto& ext : deviceExtensions) {
			if (std::ranges::find(availableExtensions, str(ext)) == availableExtensions.end()) {
				MWARN << "Device " << pDev.getProperties().deviceName << " does not support extension " << ext << endl;
				hasAllExtensions = false;
				break;
			}
		}
		if (!hasAllExtensions)
			continue;

		const auto devProps = pDev.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDrmPropertiesEXT>();
		const auto drmProps = devProps.get<vk::PhysicalDeviceDrmPropertiesEXT>();
		bool found_device = false;
		if (!drmProps.hasPrimary) {
			MDEBUG << "Device " << pDev.getProperties().deviceName << " does not have a primary node" << endl;
			continue;
		}

		DRM_Device::id_t DRMid;

		for (const auto& drm_id : drmHandler.getDevices() | std::views::keys) {
			if (drm_id.major == drmProps.primaryMajor && drm_id.minor == drmProps.primaryMinor) {
				MDEBUG << "Found matching DRM device for " << pDev.getProperties().deviceName << endl;
				found_device = true;
				DRMid = drm_id;
				break;
			}
		}
		if (!found_device) {
			MINFO << "No matching DRM device for " << pDev.getProperties().deviceName << endl;
			continue;
		}

		constexpr auto max = std::numeric_limits<uint32_t>::max();
		uint32_t graphicsFamilyQueueIndex{max};
		uint32_t transferFamilyQueueIndex{max};
		uint8_t transferBits{std::numeric_limits<uint8_t>::max()};

		const auto queueFamilyProperties = pDev.getQueueFamilyProperties();

		for (size_t j = 0 ; j < queueFamilyProperties.size(); j++) {
			constexpr auto graphicsFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eTransfer;
			constexpr auto transferFlags = vk::QueueFlagBits::eTransfer;
			const auto& queueFamily = queueFamilyProperties[j];
			if (graphicsFamilyQueueIndex == max && queueFamily.queueFlags & graphicsFlags ) {
				MDEBUG << pDev.getProperties().deviceName  << " Found graphics queue family " << to_str(queueFamily.queueFlags) << endl;
				graphicsFamilyQueueIndex = j;
			}
			if (queueFamily.queueFlags & transferFlags && countBits(static_cast<uint32_t>(queueFamily.queueFlags)) < transferBits) {
				transferFamilyQueueIndex = j;
				transferBits = countBits(static_cast<uint32_t>(queueFamily.queueFlags));
			}
		}
		if (graphicsFamilyQueueIndex == max) {
			MWARN << "Could not find a queue family with graphics capabilities for device " << pDev.getProperties().deviceName << endl;
			continue;
		}
		MDEBUG << pDev.getProperties().deviceName << " Found transfer queue family " <<
			to_str(queueFamilyProperties[transferFamilyQueueIndex].queueFlags) << endl;
		std::unordered_set graphicsQueueFamilyIndex{graphicsFamilyQueueIndex, transferFamilyQueueIndex};

		vec<vk::DeviceQueueCreateInfo> queueCreateInfos;
		for (const auto& queueFamilyIndex : graphicsQueueFamilyIndex) {
			queueCreateInfos.push_back({
				.queueFamilyIndex = queueFamilyIndex,
				.queueCount = 1,
				.pQueuePriorities = &globals::queuePriority
			});
		}

		const vk::DeviceCreateInfo deviceCreateInfo{
			.queueCreateInfoCount = queueCreateInfos.size32(),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledExtensionCount = deviceExtensions.size32(),
			.ppEnabledExtensionNames = deviceExtensions.data()
		};

		str name = pDev.getProperties().deviceName.data();
		auto result = pDev.createDevice(deviceCreateInfo);
		if (!result.has_value()) {
			MERROR << name << " Could not create device: " << to_str(result.error()) << endl;
			continue;
		}

		VDevice dev {id, drmHandler.takeDevice(DRMid), (pDev.getProperties().deviceName.data())};
		const_cast<uint32_t&>(dev.graphicsQueueFamilyIndex) = graphicsFamilyQueueIndex;
		const_cast<uint32_t&>(dev.transferQueueFamilyIndex) = transferFamilyQueueIndex;
		dev.dev = std::move(result.value());
		dev.pDev = std::move(pDev);
		MDEBUG << name << " Created device " << endl;
		bool cont = false;
		for (const auto& j : graphicsQueueFamilyIndex) {
			const vk::CommandPoolCreateInfo cmdPoolCreateInfo{
				.queueFamilyIndex = j
			};
			auto cmdRes = dev.dev.createCommandPool(cmdPoolCreateInfo);
			if (!cmdRes.has_value()) {
				MERROR << dev.name << " Could not create command pool " << to_str(cmdRes.error()) << endl;
				cont = true;
				break;
			}
			MDEBUG << dev.name << " Created command pool " << j << endl;
			dev.cmdPools.emplace(j, std::move(cmdRes.value()));
			auto queue = dev.dev.getQueue(j, 0);
			if (!queue.has_value()) {
				MERROR << dev.name << " Could not get queue " << to_str(queue.error()) << endl;
				cont = true;
				break;
			}
			MDEBUG << dev.name << " Got queue " << j << endl;
			dev.queues.emplace(j, std::move(queue.value()));
		}
		if (cont)
			continue;
		auto vertShader = dev.createShaderModule(VERT_SHADER);
		auto fragShader = dev.createShaderModule(FRAG_SHADER);
		if (!vertShader.has_value() || !fragShader.has_value()) {
			MERROR << dev.name << " Could not create shader modules" << endl;
			continue;
		}
		dev.vertShader = std::move(vertShader.value());
		dev.fragShader = std::move(fragShader.value());
		dev.parent = this;
		devices.emplace(id, std::move(dev));
		ret.push_back(id);
	}
	return ret;
}

VInstance::~VInstance() {
	MDEBUG << "Destroying Vulkan instance" << endl;
}

vkr::Context VInstance::context{};
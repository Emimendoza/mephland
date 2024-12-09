#define MLAND_VINSTANCE_IMPL
#include <unordered_set>
#include "mland/vulk.h"
#include "mland/vinstance.h"
#include "mland/globals.h"


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
			// MINFO << pCallbackData->pMessage << endl;
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			MERROR << pCallbackData->pMessage << endl;
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			// MDEBUG << pCallbackData->pMessage << endl;
			break;
		default:
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			MWARN << pCallbackData->pMessage << endl;
		break;
	}

	return VK_FALSE;

}

VInstance::VInstance(const bool enableValidationLayers, Backend& backend): backend(&backend) {
	MDEBUG << "Creating Vulkan instance" << endl;
	static constexpr vk::ApplicationInfo appInfo{
		.pApplicationName = "MephLand Compositor",
		.applicationVersion = VK_MAKE_VERSION(0, 0, 0),
		.pEngineName = "MephLand",
		.engineVersion = VK_MAKE_VERSION(0, 0, 0),
		.apiVersion = VK_API_VERSION_1_3
	};

	vec<cstr> enabledLayerNames;
	vec<cstr> enabledExtensionNames{
		vk::KHRSurfaceExtensionName,
		vk::KHRGetSurfaceCapabilities2ExtensionName,
		vk::EXTSurfaceMaintenance1ExtensionName
	};

	for (const auto& ext : backend.requiredInstanceExtensions()) {
		enabledExtensionNames.push_back(ext);
	}

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

	auto inst = context.createInstance(createInfo);
	if (!inst.has_value()) {
		MERROR << "Failed to create Vulkan instance:" << to_str(inst.error()) << endl;
		throw std::runtime_error("Failed to create Vulkan instance");
	}

	instance = std::move(inst.value());
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
	vec<cstr> deviceExtensions {
		vk::KHRSwapchainExtensionName,
		vk::EXTSwapchainMaintenance1ExtensionName

	};

	for (const auto& ext : backend->requiredDeviceExtensions()) {
		deviceExtensions.push_back(ext);
	}

	auto res = instance.enumeratePhysicalDevices();
	if (!res.has_value()) {
		MERROR << "Failed to enumerate devices" << to_str(res.error()) << endl;
		return ret;
	}

	for (auto& pDev : res.value()) {
		str name = pDev.getProperties().deviceName.data();
		auto id = static_cast<VDevice::id_t>(pDev.getProperties().deviceID);
		if (devices.contains(id)) {
			MDEBUG << "Device " << name << " already exists" << endl;
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
				MWARN << "Device " << name << " does not support extension " << ext << endl;
				hasAllExtensions = false;
				break;
			}
		}
		if (!hasAllExtensions)
			continue;

		if (!deviceGood(pDev))
			continue;

		const auto devFeatures = pDev.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features>();
		const auto vulkan12Features = devFeatures.get<vk::PhysicalDeviceVulkan12Features>();
		if (!vulkan12Features.timelineSemaphore) {
			MDEBUG << "Device " << name << " does not support timeline semaphores" << endl;
			continue;
		}

		auto devCreate = createDevice(std::move(pDev), deviceExtensions);
		if (!devCreate.has_value()) {
			MERROR << "Failed to create device " << name << endl;
			continue;
		}
		devices.emplace(id, std::move(devCreate.value()));
		ret.push_back(id);
	}
	return ret;
}

VInstance::~VInstance() {
	MDEBUG << "Destroying Vulkan instance" << endl;
	devices.clear();
	debugMessenger.clear();
	instance.clear();
}

vkr::Context VInstance::context{};
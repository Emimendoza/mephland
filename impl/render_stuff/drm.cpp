#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <filesystem>
#include <sys/sysmacros.h>

#include "mland/drm_backend.h"
using namespace mland;
using DrmDevice = DrmBackend::DrmDevice;

DrmBackend::DrmVDevice::DrmVDevice(vkr::PhysicalDevice&& physicalDevice, const vec<cstr>& extensions, DrmVInstance* parent):
	VDevice(std::move(physicalDevice), extensions, parent) {
	if (!good)
		return;
	const_cast<bool&>(good) = false;
	auto retRes = pDev.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDrmPropertiesEXT>();
	const auto res = retRes.get<vk::PhysicalDeviceDrmPropertiesEXT>();
	if (!res.hasPrimary) {
		MERROR << "Device does not have a primary node" << endl;
		return;
	}
	DrmId id{res.primaryMajor, res.primaryMinor};
	auto& backend = static_cast<DrmBackend&>(*parent->getBackend());
	backend.refreshDevices();
	if (!backend.getDevices().contains(id)) {
		MERROR << "Device " << name << " not found in backend" << endl;
		return;
	}
	drmDev = backend.takeDevice(id);
	const_cast<bool&>(good) = true;
}

DrmDevice::~DrmDevice() {
	if (fd < 0)
		return;
	MDEBUG << "Dropping master on " << name << endl;
	drmDropMaster(fd);
	close(fd);
}

vec<DrmDevice::Connector> DrmDevice::refreshConnectors() {
	MDEBUG << "Refreshing connectors for " << name << endl;
	vec<Connector> ret;
	drmModeRes* res = drmModeGetResources(fd);
	if (!res) {
		MWARN << "Failed to get resources for " << name << endl;
		return ret;
	}
	for (int i = 0; i < res->count_connectors; i++) {
		const auto conn = drmModeGetConnector(fd, res->connectors[i]);
		if (!conn) {
			MWARN << "Failed to get connector " << i << " for " << name << endl;
			continue;
		}
		if (conn->connection == DRM_MODE_CONNECTED) {
			ret.push_back(conn->connector_id);
		}
		drmModeFreeConnector(conn);
	}
	drmModeFreeResources(res);
	return ret;
}

DrmBackend::DrmBackend(DrmPaths& drmPaths) : drmPaths(std::move(drmPaths)) {
	refreshDevices();
}

vec<DrmBackend::DrmId> DrmBackend::refreshDevices() {
	vec<DrmId> ret;
	vec<str> devices;

	if (!drmPaths.explicitInclude.empty()){
		devices = drmPaths.explicitInclude;
	} else if (!drmPaths.explicitExclude.empty()) {
		devices = listDrmDevices();
		for (const auto& excl : drmPaths.explicitExclude)
			std::erase(devices, excl);

	} else {
		devices = listDrmDevices();
	}
	// TODO: Handle hot unplugging

	for (auto& dev : devices) {
		if (dev.empty())
			continue;
		const int fd = open(dev.c_str(), O_RDWR);
		if (fd < 0) {
			MWARN << "Failed to open " << dev << endl;
			continue;
		}
		struct stat stat_buf{};
		if (fstat(fd, &stat_buf) != 0) {
			MWARN << "Failed to stat " << dev << endl;
			close(fd);
			continue;
		}
		DrmId id{gnu_dev_major(stat_buf.st_rdev), gnu_dev_minor(stat_buf.st_rdev)};
		if (drmDevices.contains(id)) {
			MDEBUG << "Device " << dev << " already exists" << endl;
			close(fd);
			continue;
		}

		if (drmSetMaster(fd) != 0) {
			MWARN << "Failed to set master on " << dev << endl;
			close(fd);
			continue;
		}

		MDEBUG << "Set master on " << dev << endl;
		DrmDevice device{dev, id};
		device.fd = fd;
		ret.emplace_back(id);
		drmDevices.emplace(id, std::move(device));
	}
	if (drmDevices.empty())
		throw std::runtime_error("No DRM devices found");
	return ret;
}

DrmBackend::~DrmBackend() = default;

namespace fs = std::filesystem;
vec<str> DrmBackend::listDrmDevices() {
	const fs::path DRM_DIR = "/dev/dri";
	vec<str> devices;
	for (const auto& entry : fs::directory_iterator(DRM_DIR)) {
		if (entry.is_character_file()) {
			if (entry.path().filename().string().find("card") == std::string::npos)
				continue;
			devices.push_back(entry.path().string());
		}
	}
	return devices;
}

const vec<cstr>& DrmBackend::requiredInstanceExtensions() const {
	static const vec<cstr> extensions = {
		vk::KHRDisplayExtensionName,
		vk::EXTDirectModeDisplayExtensionName,
		vk::EXTAcquireDrmDisplayExtensionName
	};
	return extensions;
}

const vec<cstr>& DrmBackend::requiredDeviceExtensions() const {
	static const vec<cstr> extensions = {
		vk::EXTPhysicalDeviceDrmExtensionName,
	};
	return extensions;
}

bool DrmBackend::DrmVInstance::deviceGood(const vkr::PhysicalDevice& pDev) {
	const auto devProps = pDev.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDrmPropertiesEXT>();
	const auto drmProps = devProps.get<vk::PhysicalDeviceDrmPropertiesEXT>();
	if (!drmProps.hasPrimary) {
		MDEBUG << "Device does not have a primary node" << endl;
		return false;
	}

	for (const auto& drm_id : static_cast<DrmBackend&>(*backend).getDevices() | std::views::keys) {
		if (drm_id.major == drmProps.primaryMajor && drm_id.minor == drmProps.primaryMinor) {
			MDEBUG << "Found matching DRM device " << endl;
			return true;
		}
	}

	MINFO << "No matching DRM device " << endl;
	return false;
}

opt<u_ptr<VDevice>> DrmBackend::DrmVInstance::createDevice(vkr::PhysicalDevice&& physicalDevice, const vec<cstr>& extensions) {
	auto ptr = u_ptr<VDevice>(new DrmVDevice(std::move(physicalDevice), extensions, this));
	if (ptr->good)
		return ptr;
	return std::nullopt;
}

u_ptr<VInstance> DrmBackend::createInstance(const bool validation_layers) {
	return u_ptr<VInstance>(new DrmVInstance(validation_layers, *this));
}

DrmBackend::DrmVDisplay::DrmVDisplay(str&& name, DrmVDevice* vDev, vkr::DisplayKHR&& display,
	const vk::DisplayPropertiesKHR& displayProps, const DrmDevice::Connector con) : VDisplay(std::move(name), vDev) {
	this->display = std::move(display);
	this->displayProps = displayProps;
	this->con = con;
	this->start();
}
DrmBackend::DrmVDisplay::~DrmVDisplay() {
	stop();
	static_cast<DrmVDevice&>(*vDev).connectors.erase(con);
}

opt<s_ptr<DrmBackend::DrmVDisplay>> DrmBackend::DrmVDevice::getDrmDisplay(const DrmDevice::Connector con) {
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
	const auto display= new  DrmVDisplay(name + ' ' + displayProps.displayName, this, std::move(res.value()), displayProps, con);
	if (!display->isGood()) {
		MERROR << name << " Display is not good" << endl;
		return std::nullopt;
	}
	display->size = displayProps.physicalDimensions;

	MDEBUG << name << " Created display for connector " << con << endl;
	return std::make_optional(s_ptr<DrmVDisplay>(display));
}

vec<s_ptr<VDisplay>> DrmBackend::DrmVDevice::updateMonitors() {
	vec<s_ptr<VDisplay>> ret;
	const auto cons = drmDev.refreshConnectors();
	MDEBUG << "Updating monitors for device " << name << endl;
	for (const auto& con : cons) {
		if (connectors.contains(con)) {
			MDEBUG << name << " Already have display for connector " << con << endl;
			continue;
		}
		auto res = getDrmDisplay(con);
		if (!res.has_value()) {
			MERROR << name << " Failed to get display for connector " << con << endl;
			continue;
		}
		MINFO << name << " Found display for connector " << con << endl;
		ret.push_back(std::move(res.value()));
	}
	return ret;
}

uint32_t DrmBackend::DrmVDisplay::createModes() {
	MDEBUG << name << " Creating display modes" << endl;
	displayModes = std::move(display.getModeProperties());
	const auto bestRes = displayProps.physicalResolution;
	uint32_t best = 0;
	uint32_t best_refresh = 0;
	for(uint32_t i = 0; i < displayModes.size(); i++) {
		const auto& [displayMode, parameters] = displayModes[i];
		MDEBUG << name << " Found mode: " << parameters.refreshRate/1000.0 << " Hz, " <<
			parameters.visibleRegion.width << "x" << parameters.visibleRegion.height << endl;
		if (parameters.visibleRegion != bestRes) {
			continue;
		}
		if (parameters.refreshRate > best_refresh) {
			best_refresh = parameters.refreshRate;
			best = i;
		}
	}
	MINFO << name << " Best mode: " << displayModes[best].parameters.refreshRate / 1000.0 << " Hz, " <<
		  displayModes[best].parameters.visibleRegion.width << "x" << displayModes[best].parameters.visibleRegion.height << endl;
	return best;
}

void DrmBackend::DrmVDisplay::createSurface(const uint32_t mode_index) {
	MDEBUG << name << " Creating surface" << endl;
	deleteSurface();
	assert(mode_index < displayModes.size());
	const auto &[displayMode, parameters] = displayModes[mode_index];
	const vk::DisplaySurfaceCreateInfoKHR surfaceCreateInfo{
		.displayMode = displayMode,
		.planeIndex = 0,
		.planeStackIndex = 0,
		.transform = vk::SurfaceTransformFlagBitsKHR::eIdentity,
		.globalAlpha = {}, // Ignored
		.alphaMode = vk::DisplayPlaneAlphaFlagBitsKHR::ePerPixel,
		.imageExtent = parameters.visibleRegion,
	};
	const auto &instance = static_cast<DrmVDevice&>(*vDev).parent->getInstance();
	auto res = instance.createDisplayPlaneSurfaceKHR(surfaceCreateInfo);
	if (!res.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create surface: " + to_str(res.error()));
	assert(static_cast<DrmVDevice&>(*vDev).pDev.getSurfaceSupportKHR(vDev->graphicsQueueFamilyIndex, res.value()));
	vkrSurface = std::move(res.value());
	surface = vkrSurface;
}

void DrmBackend::DrmVDisplay::deleteSurface() {
	vkrSurface.clear();
	surface = nullptr;
}


void DrmBackend::DrmVDisplay::createSurface() {
	createSurface(createModes());
}
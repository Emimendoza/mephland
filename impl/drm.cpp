#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <filesystem>
#include <sys/sysmacros.h>

#include "drm_backend.h"
using namespace mland;

DRM_Device::~DRM_Device() {
	if (fd < 0)
		return;
	MDEBUG << "Dropping master on " << name << endl;
	drmDropMaster(fd);
	close(fd);
}

vec<DRM_Device::Connector> DRM_Device::refreshConnectors() {
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

DRM_Handler::DRM_Handler(DRM_Paths drmPaths) : drmPaths(std::move(drmPaths)) {
	refreshDevices();
}

vec<DRM_Device::id_t> DRM_Handler::refreshDevices() {
	vec<DRM_Device::id_t> ret;
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
		DRM_Device::id_t id{gnu_dev_major(stat_buf.st_rdev), gnu_dev_minor(stat_buf.st_rdev)};
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
		DRM_Device device{dev, id};
		device.fd = fd;
		ret.emplace_back(id);
		drmDevices.emplace(id, std::move(device));
	}
	if (drmDevices.empty())
		throw std::runtime_error("No DRM devices found");
	return ret;
}

DRM_Handler::~DRM_Handler() = default;

namespace fs = std::filesystem;
vec<str> DRM_Handler::listDrmDevices() {
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

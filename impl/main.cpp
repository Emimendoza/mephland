#include <iostream>
#include <cstdlib>

#include "mland/controller.h"
#include "mland/drm_backend.h"
#include "mland/sdl_backend.h"
#include "mland/wayland_server.h"
#include "mland/env.h"


MCLASS(Main);

using namespace mland;

static void set_log_level();
static DrmBackend::DrmPaths get_drm_paths();
static bool get_validation_layers();


int main() {
	set_log_level();
	MINFO << "Starting MephLand Compositor" << endl;
	u_ptr<Backend> backend;

	const bool validation_layers = get_validation_layers();
	try {
		auto drm_Paths = get_drm_paths();
		backend = std::make_unique<DrmBackend>(drm_Paths);
	} catch (const std::exception& e) {
		MERROR << "Failed to create backend: " << e.what() << endl;
		MINFO << "Falling back to SDL backend" << endl;
		backend = std::make_unique<SdlBackend>(2);
	}

	u_ptr instance = backend->createInstance(validation_layers);
	Controller::create(std::move(instance));
	Controller::run();
	return  0;
}

static void set_log_level() {
	int log_level = 2;
	if (const auto log_env = std::getenv(DRM_LOG_LEVEL)) {
		log_level = std::strtoul(log_env, nullptr, 10);
	}
	switch (log_level) {
	case eDebug:
		globals::debug.rdbuf(std::cout.rdbuf());
	case eInfo:
		globals::info.rdbuf(std::cout.rdbuf());
	case eWarn:
		globals::warn.rdbuf(std::cout.rdbuf());
	case eError:
		globals::error.rdbuf(std::cout.rdbuf());
		break;
	default:
		std::cerr << "Invalid log level: " << log_level << std::endl;
		exit(1);
	}
}

static DrmBackend::DrmPaths get_drm_paths() {
	 DrmBackend::DrmPaths drm_Paths{};
	if (const auto drm_env = std::getenv(DRM_DEVICE_ENV)) {
		MDEBUG << "User Specified DRM devices: " << drm_env << endl;
		auto drm_devs = str_view(drm_env).split(':');
		for (auto& dv : drm_devs) {
			if (dv.empty()){
				continue;
			}
			if (dv[0] == '!') {
				MDEBUG << "Excluding " << dv.substr(1) << endl;
				drm_Paths.explicitExclude.emplace_back(dv.substr(1));
				continue;
			}
			MDEBUG << "Including " << dv << endl;
			drm_Paths.explicitInclude.emplace_back(dv);
		}
	}
	return drm_Paths;
}

static bool get_validation_layers() {
	if (const auto val_env = std::getenv(USE_VALIDATION_LAYERS)) {
		return std::strtoul(val_env, nullptr, 10);
	}
	return false;
}
#include <iostream>
#include <cstdlib>

#include "controller.h"
#include "drm_backend.h"
#include "env.h"
#include "vinstance.h"

MCLASS(Main);

using namespace mland;

static void set_log_level();
static DRM_Handler::DRM_Paths get_drm_paths();
static bool get_validation_layers();


int main() {
	MINFO << "Starting MephLand Compositor" << endl;
	set_log_level();
	auto drm_Paths = get_drm_paths();
	bool validation_layers = get_validation_layers();
	Controller controller = {{std::move(drm_Paths), validation_layers}};
	controller.run();
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
			globals::warn.rdbuf(std::cerr.rdbuf());
		case eError:
			globals::error.rdbuf(std::cerr.rdbuf());
			break;
		default:
			std::cerr << "Invalid log level: " << log_level << endl;
			exit(1);
	}
}

static DRM_Handler::DRM_Paths get_drm_paths() {
	DRM_Handler::DRM_Paths drm_Paths{};
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
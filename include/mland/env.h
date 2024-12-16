#pragma once

// Environment variables related to the program
namespace mland {
/**
 * The environment variable that specifies the DRM device(s) to use
 * @note Use : as a separator
 * @note Default: [all available DRM devices]
 * @note Type: string
 */
constexpr auto DRM_DEVICE_ENV = "MLAND_DRM_DEVICES";

/**
 * The environment variable that specifies the log level for the compositor
 * @note Type: int
 * @note Default: 3
 */
constexpr auto DRM_LOG_LEVEL = "MLAND_LOG_LEVEL";

enum log_level : int {
	eDebug = 1,
	eInfo = 2,
	eWarn = 3,
	eError = 4
};

/**
 * The environment variable that specifies whether to enable validation layers
 * @note Type: int
 * @note Default: 0
 */
constexpr auto USE_VALIDATION_LAYERS = "MLAND_VALIDATION_LAYERS";

/**
 * The environment variable that specifies the maximum number of windows to create when
 * using the SDL backend
 * @note Type: int
 * @note Default: 1
 */
constexpr auto MAX_WINDOWS = "MLAND_SDL_MAX_WINDOWS";

}

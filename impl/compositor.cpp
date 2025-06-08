#include <mland/compositor.h>
#include <vulkan/vulkan_core.h>
#include <csignal>

using namespace mland;

CompositorBase::CompositorBase() {
	instanceExtensions = {
			vk::KHRSurfaceExtensionName,
			vk::KHRGetSurfaceCapabilities2ExtensionName,
			vk::EXTSurfaceMaintenance1ExtensionName
	};
	instanceLayers = {};
	deviceExtensions = {};
	instance = nullptr;
	auto* disp = wl_display_create();
	if (!disp) {
		throw std::runtime_error("Failed to create Wayland display");
	}
	wlDisplay = std::shared_ptr<wl_display>(disp, wl_display_destroy);

	wlSocket = wl_display_add_socket_auto(disp);
	if (!wlSocket) {
		throw std::runtime_error("Failed to create Wayland socket");
	}

	auto* loop = wl_display_get_event_loop(disp);
	if (!loop) {
		throw std::runtime_error("Failed to get Wayland event loop");
	}

	auto termFunc = [](int, void* data) {
		auto* disp = static_cast<wl_display*>(data);
		wl_display_terminate(disp);
		return 0;
	};

	wl_event_loop_add_signal(loop, SIGINT, termFunc, disp);
	wl_event_loop_add_signal(loop, SIGTERM, termFunc, disp);

	wlCompositor = std::make_unique<wayland_xml::WlCompositor>(disp);
}

void
CompositorBase::run() {
	wl_display_run(wlDisplay.get());
}

#define CDRM Compositor<BackendType::eDRM>::backendSpecific
#define CSDL Compositor<BackendType::eSDL>::backendSpecific


template<> struct CDRM {
	backendSpecific();
	~backendSpecific();
};

CDRM::backendSpecific() {

}

CDRM::~backendSpecific() {

}

template<> struct CSDL {
	backendSpecific();
	~backendSpecific();
};

CSDL::backendSpecific() {

}

CSDL::~backendSpecific() {

}

#undef CDRM
#undef CSDL

template<BackendType backend>
Compositor<backend>::Compositor(bool enableValidationLayers) : CompositorBase() {
	if (enableValidationLayers) {
		instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
		instanceExtensions.push_back(vk::EXTDebugUtilsExtensionName);
	}

	backDat = std::make_unique<backendSpecific>();

	if constexpr (backend == BackendType::eDRM) {
		
	}
	if constexpr (backend == BackendType::eSDL) {

	}
}

CompositorBase::~CompositorBase() = default;

template<BackendType backend>
Compositor<backend>::~Compositor() = default;

void
CompositorBase::initInstance() {
	static constexpr vk::ApplicationInfo appInfo(
		"MephLand Compositor",
		VK_MAKE_VERSION(0, 0, 0),
		"MephLand",
		VK_MAKE_VERSION(0, 0, 0),
		VK_API_VERSION_1_3
	);

	vk::InstanceCreateInfo createInfo(
		{}, // flags
		&appInfo,
		instanceLayers,
		instanceExtensions
	);

	instance = std::make_shared<vkr::Instance>(vkrContext, createInfo);
}

template class mland::Compositor<BackendType::eDRM>;
template class mland::Compositor<BackendType::eSDL>;
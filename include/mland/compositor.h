#pragma once
#include <memory>
#include <wayland-server-protocol.h>
#include <wayland-server-core.h>

#include "./vulkan.h"
#include "./protocols/wayland_xml.h"

namespace mland {

enum class BackendType {
	eDRM,
	eSDL
};

class CompositorBase {
public:
	void run();
protected:
	CompositorBase();
	~CompositorBase();
	void initInstance();

	vkr::Context vkrContext{};
	std::vector<const char*> instanceExtensions;
	std::vector<const char*> instanceLayers;
	std::vector<const char*> deviceExtensions;
	std::shared_ptr<vkr::Instance> instance;
	std::shared_ptr<wl_display> wlDisplay;
	std::unique_ptr<wayland_xml::WlCompositor> wlCompositor;
	const char* wlSocket;
};

template<BackendType backend>
class Compositor final : public CompositorBase {
public:
	Compositor(bool enableValidationLayers = false); // NOLINT(*-explicit-constructor)
	~Compositor();
private:
	struct backendSpecific;
	std::unique_ptr<backendSpecific> backDat;

};

}
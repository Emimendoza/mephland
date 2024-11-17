#include "vdisplay.h"
#include "vdevice.h"

using namespace mland;

template<bool DIRECT, bool LAYERED>
void VDisplay::Renderer::renderMain() {
	for (auto& image : sri->images) {
		vk::SemaphoreCreateInfo semInfo{};
		auto semRes = dev->dev.createSemaphore(semInfo);
		if (!semRes.has_value()) [[unlikely]]
			throw std::runtime_error(name + " Failed to create semaphore: " + to_str(semRes.error()));
		image.imageAvailable = std::move(semRes.value());
		vk::FenceCreateInfo fenceInfo{};
		auto fenceRes = dev->dev.createFence(fenceInfo);
		if (!fenceRes.has_value()) [[unlikely]]
			throw std::runtime_error(name + " Failed to create fence: " + to_str(fenceRes.error()));
		image.renderFinished = std::move(fenceRes.value());
	}

	while (!stop.test()) {
		renderLoop<DIRECT, LAYERED>();
	}
}

template<bool DIRECT, bool LAYERED>
void VDisplay::Renderer::renderLoop() {

}

VDisplay::Renderer::Renderer(SurfaceRenderInfo* sri, str&& name, const RenderingMode mode, VDevice* dev) :
name(std::move(name)), sri(sri), dev(dev) {
	MDEBUG << this->name << " Starting renderer" << endl;
	stop.clear();
	if (mode & eDirect && mode & eLayered) {
		thread = std::thread(&Renderer::renderMain<true, true>, this);
		return;
	}
	if (mode & eDirect) {
		thread = std::thread(&Renderer::renderMain<true, false>, this);
		return;
	}
	if (mode & eLayered) {
		thread = std::thread(&Renderer::renderMain<false, true>, this);
		return;
	}
	thread = std::thread(&Renderer::renderMain<false, false>, this);
}

VDisplay::Renderer::~Renderer() {
	stop.test_and_set();
	thread.join();
}


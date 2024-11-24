#include "vdisplay.h"
#include "vdevice.h"

using namespace mland;

template<bool DIRECT, bool LAYERED>
void VDisplay::Renderer::renderMain() {
	cmdPool = dev->createCommandPool(dev->graphicsQueueFamilyIndex);
	for (auto& img : sri->images) {
		img.graphicsCmd = dev->createCommandBuffer(dev->graphicsQueueFamilyIndex, cmdPool);
	}
	while (!stop.test()) {
		renderLoop<DIRECT, LAYERED>();
	}
	for (auto& [key, val] : busySyncObjs) {
		waitFence(syncObjs[val].inFlight);
	}
	syncObjs.clear();
	for (auto& img : sri->images) {
		img.graphicsCmd.clear();
	}
	cmdPool.clear();
}

template<bool DIRECT, bool LAYERED>
void VDisplay::Renderer::renderLoop() {
	const auto syncIndex = getSyncObj();
	const auto& sync = syncObjs[syncIndex];
	auto [result, imageIndex] = sri->swapchain.acquireNextImage(UINT64_MAX, sync.imageAvailable, nullptr);
	if (result == vk::Result::eErrorOutOfDateKHR) {
		MERROR << name << " Swapchain out of date" << endl;
		return; // In theory this should never happen because we're rendering to a display
	}
	if (result == vk::Result::eSuboptimalKHR) {
		MWARN << name << " Swapchain suboptimal" << endl;
	}
	if (result != vk::Result::eSuccess) {
		MERROR << name << " Failed to acquire swapchain image: " << to_str(result) << endl;
		stop.test_and_set();
		return;
	}
	if (busySyncObjs.contains(imageIndex)) {
		const auto syncIndexRn = busySyncObjs[imageIndex];
		const auto& syncRn = syncObjs[syncIndexRn];
		waitFence(syncRn.inFlight);
		freeSyncObjs.push(syncIndexRn);
		busySyncObjs.erase(imageIndex);
	}

	busySyncObjs[imageIndex] = syncIndex;
	const auto& img = sri->images[imageIndex];
	const auto& cmd = img.graphicsCmd;
	cmd.reset();
	cmd.begin({});
	constexpr vk::ClearValue clearValue{
		.color = {std::array{1.0f, 0.0f, 0.0f, 1.0f}}
	};
	const vk::RenderPassBeginInfo renderPass {
		.renderPass = sri->renderPass,
		.framebuffer = img.framebuffer,
		.renderArea = {
			.offset = {},
			.extent = sri->extent
		},
		.clearValueCount = 1,
		.pClearValues = &clearValue
	};
	cmd.beginRenderPass(renderPass, vk::SubpassContents::eInline);
	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, sri->pipeline);
	cmd.draw(3, 1, 0, 0);
	cmd.endRenderPass();
	cmd.end();
	constexpr vk::PipelineStageFlags waitStages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
	const vk::SubmitInfo submit {
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*sync.imageAvailable,
		.pWaitDstStageMask = &waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &*cmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &*sync.renderFinished
	};
	dev->submit(dev->graphicsQueueFamilyIndex,submit, sync.inFlight);
	const vk::PresentInfoKHR present {
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*sync.renderFinished,
		.swapchainCount = 1,
		.pSwapchains = &*sri->swapchain,
		.pImageIndices = &imageIndex
	};
	const auto presentRes = dev->present(dev->graphicsQueueFamilyIndex, present);
	if (presentRes == vk::Result::eErrorOutOfDateKHR || presentRes == vk::Result::eSuboptimalKHR) {
		MWARN << name << " Swapchain out of date or suboptimal" << endl;
	}
	if (presentRes != vk::Result::eSuccess) {
		MERROR << name << " Failed to present swapchain image: " << to_str(presentRes) << endl;
		stop.test_and_set();
	}

}

uint32_t VDisplay::Renderer::getSyncObj() {
	if (!freeSyncObjs.empty()) {
		const auto temp = freeSyncObjs.top();
		freeSyncObjs.pop();
		return temp;
	}
	SyncObjs sync;
	constexpr vk::SemaphoreCreateInfo semInfo{};
	auto semRes = dev->dev.createSemaphore(semInfo);
	if (!semRes.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create semaphore: " + to_str(semRes.error()));
	sync.imageAvailable = std::move(semRes.value());
	semRes = dev->dev.createSemaphore(semInfo);
	if (!semRes.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create semaphore: " + to_str(semRes.error()));
	sync.renderFinished = std::move(semRes.value());
	constexpr vk::FenceCreateInfo fenceInfo{};
	auto fenceRes = dev->dev.createFence(fenceInfo);
	if (!fenceRes.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create fence: " + to_str(fenceRes.error()));
	sync.inFlight = std::move(fenceRes.value());
	syncObjs.push_back(std::move(sync));
	return syncObjs.size32() - 1;

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

void VDisplay::Renderer::waitFence(const vkr::Fence& fence) {
	const auto res = dev->dev.waitForFences(*fence, true, UINT64_MAX);
	if (res != vk::Result::eSuccess) {
		MERROR << name << " Failed to wait for fence: " << to_str(res) << endl;
	}
	dev->dev.resetFences(*fence);
}



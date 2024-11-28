#include "vdisplay.h"
#include "vdevice.h"

using namespace mland;

void VDisplay::workerMain() {
	try {
		createEverything();
		std::unique_lock lock(stateMutex);
		state = eIdle;
	} catch (const std::exception& e) {
		MERROR << name << " Exception in worker thread: " << e.what() << endl;
		std::lock_guard lock(stateMutex);
		state = eError;
		return;
	}
	while (step()) {}
	for (const auto& val : busySyncObjs | std::views::values) {
		waitFence(syncObjs[val].inFlight);
	}
	cleanup();
}

constexpr vk::CommandBufferBeginInfo begInf {
	.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
};

void VDisplay::transferBackground(const SyncObjs& sync, const Image& img) {
	const auto& cmd = img.backgroundCmd;
	cmd.reset();
	cmd.begin(begInf);
	constexpr vk::ImageSubresourceLayers imgSub {
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.mipLevel = 0,
		.baseArrayLayer = 0,
		.layerCount = 1
	};
	const vk::Extent3D extent {
		.width = this->extent.width,
		.height = this->extent.height,
		.depth = 1
	};
	const vk::ImageCopy imgCopy {
		.srcSubresource = imgSub,
		.srcOffset = {},
		.dstSubresource = imgSub,
		.dstOffset = {},
		.extent = extent
	};
	cmd.copyImage(
		background,
		vk::ImageLayout::eTransferSrcOptimal,
		img.image,
		vk::ImageLayout::eTransferDstOptimal,
		imgCopy
	);
	cmd.end();
	constexpr vk::PipelineStageFlags waitStages = {vk::PipelineStageFlagBits::eTransfer};
	const std::array semaphores = {
		*sync.imageAvailable
	};
	const vk::SubmitInfo submit {
		.waitSemaphoreCount = static_cast<uint32_t>(semaphores.size()),
		.pWaitSemaphores = semaphores.data(),
		.pWaitDstStageMask = &waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &*cmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &*sync.backgroundFinished
	};
	vDev->submit(vDev->transferQueueFamilyIndex, submit);
}

void VDisplay::drawFrame(const SyncObjs& sync, const Image& img) {
	const auto& cmd = img.graphicsCmd;
	cmd.reset();
	cmd.begin(begInf);
	const vk::Viewport viewport{
		.x = 0,
		.y = 0,
		.width = static_cast<float>(extent.width),
		.height = static_cast<float>(extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	const vk::Rect2D scissor{
		.offset ={},
		.extent = extent
	};
	const vk::RenderPassBeginInfo render_pass {
		.renderPass = renderPass,
		.framebuffer = img.framebuffer,
		.renderArea = {
			.offset = {},
			.extent = extent
		},
	};
	cmd.beginRenderPass(render_pass, vk::SubpassContents::eInline);
	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
	cmd.setViewport(0, viewport);
	cmd.setScissor(0, scissor);
	cmd.draw(3, 1, 0, 0);
	cmd.endRenderPass();
	cmd.end();
	static constexpr vk::PipelineStageFlags waitStages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
	const std::array waitSemaphores = {
		*sync.backgroundFinished,
		*std::get<0>(*timelineSemaphore), // To render one frame at a time
		*std::get<1>(*timelineSemaphore), // Used by windows to signal to render

	};
	const std::array signalSemaphores = {
		*sync.renderFinished,
		*std::get<0>(*timelineSemaphore),
	};
	const std::array waitValues = {
		timelineValue,
		timelineValue,
		timelineValue
	};
	timelineValue++;
	const std::array signalValues = {
		timelineValue,
		timelineValue
	};
	const vk::TimelineSemaphoreSubmitInfo timelineSubmit {
		.waitSemaphoreValueCount = static_cast<uint32_t>(waitSemaphores.size()),
		.pWaitSemaphoreValues = waitValues.data(),
		.signalSemaphoreValueCount = static_cast<uint32_t>(signalSemaphores.size()),
		.pSignalSemaphoreValues = signalValues.data()
	};
	const vk::SubmitInfo submit {
		.pNext = &timelineSubmit,
		.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
		.pWaitSemaphores = waitSemaphores.data(),
		.pWaitDstStageMask = &waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &*cmd,
		.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
		.pSignalSemaphores = signalSemaphores.data()
	};

	vDev->submit(vDev->graphicsQueueFamilyIndex,submit, sync.inFlight);
}

void VDisplay::present(const SyncObjs& sync, const Image& img, const uint32_t& imageIndex) {
	const vk::PresentInfoKHR present {
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*sync.renderFinished,
		.swapchainCount = 1,
		.pSwapchains = &*swapchain,
		.pImageIndices = &imageIndex
	};
	const auto presentRes = vDev->present(vDev->graphicsQueueFamilyIndex, present);
	if (presentRes == vk::Result::eErrorOutOfDateKHR || presentRes == vk::Result::eSuboptimalKHR) {
		MWARN << name << " Swapchain out of date or suboptimal" << endl;
	}
	if (presentRes != vk::Result::eSuccess) {
		MERROR << name << " Failed to present swapchain image: " << to_str(presentRes) << endl;
		std::lock_guard lock(stateMutex);
		state = eStop;
	}
}


void VDisplay::renderLoop() {
	const auto syncIndex = getSyncObj();
	const auto& sync = syncObjs[syncIndex];
	auto [result, imageIndex] = swapchain.acquireNextImage(UINT64_MAX, sync.imageAvailable, nullptr);
	if (result == vk::Result::eErrorOutOfDateKHR) {
		MERROR << name << " Swapchain out of date" << endl;
		return; // In theory this should never happen because we're rendering to a display
	}
	if (result == vk::Result::eSuboptimalKHR) {
		MWARN << name << " Swapchain suboptimal" << endl;
	}
	if (result != vk::Result::eSuccess) {
		MERROR << name << " Failed to acquire swapchain image: " << to_str(result) << endl;
		std::lock_guard lock(stateMutex);
		state = eStop;
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
	const auto& img = images[imageIndex];
	transferBackground(sync, img);
	drawFrame(sync, img);
	present(sync, img, imageIndex);
}

bool VDisplay::step() {
	switch (getState()) {
	case eStop:
		return false;
	case eIdle:
		renderLoop();
		return true;
	default:
		MERROR << name << " Invalid state" << endl;
		return false;
	}
}

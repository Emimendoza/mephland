#include "mland/vdisplay.h"
#include "mland/vdevice.h"

using namespace mland;

void VDisplay::workerMain() {
	try {
		createEverything();
		std::unique_lock lock(stateMutex);
		state = eIdle;
		stateCond.notify_all();
	} catch (const std::exception& e) {
		MERROR << name << " Exception in worker thread: " << e.what() << endl;
		std::lock_guard lock(stateMutex);
		state = eError;
		stateCond.notify_all();
		return;
	}
	startTime = std::chrono::system_clock::now();
	while (step()) {}
	const auto end = std::chrono::system_clock::now();
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime).count();
	const auto frames = std::get<0>(*timelineSemaphore).getCounterValue();
	const auto fps = static_cast<double>(frames) / (elapsed / 1000.0);
	MINFO << name << " Rendered " << frames << " frames in " << elapsed << "ms (" << fps << "fps)" << endl;
	cleanup();
	std::unique_lock lock(stateMutex);
	stateCond.wait(lock, [this] { return state == eStop; });
	state = eStopped;
	stateCond.notify_all();
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
	constexpr vk::ClearValue clearColor {

	};
	const vk::RenderPassBeginInfo render_pass {
		.renderPass = renderPass,
		.framebuffer = img.framebuffer,
		.renderArea = {
			.offset = {},
			.extent = extent
		},
		.clearValueCount = 1,
		.pClearValues = &clearColor,
	};
	cmd.beginRenderPass(render_pass, vk::SubpassContents::eInline);
	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
	cmd.setViewport(0, viewport);
	cmd.setScissor(0, scissor);
	cmd.draw(3, 1, 0, 0);
	cmd.endRenderPass();
	cmd.end();
	const std::array waitSemaphores = {
		//*sync.backgroundFinished,
		*sync.imageAvailable, // Temporary until background image is implemented
		*std::get<0>(*timelineSemaphore), // To render one frame at a time
		*std::get<1>(*timelineSemaphore), // Used by windows to signal to render

	};
	const std::array signalSemaphores = {
		*sync.renderFinished,
		*std::get<0>(*timelineSemaphore),
	};
	constexpr std::array<vk::PipelineStageFlags, 3> waitStages = {
		vk::PipelineStageFlagBits::eColorAttachmentOutput,
		vk::PipelineStageFlagBits::eColorAttachmentOutput,
		vk::PipelineStageFlagBits::eColorAttachmentOutput
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
		.pWaitDstStageMask = waitStages.data(),
		.commandBufferCount = 1,
		.pCommandBuffers = &*cmd,
		.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
		.pSignalSemaphores = signalSemaphores.data()
	};

	vDev->submit(vDev->graphicsQueueFamilyIndex,submit, sync.renderFinishedFence);
}

bool VDisplay::present(const SyncObjs& sync, const uint32_t& imageIndex) {
	const vk::SwapchainPresentFenceInfoEXT presentFence {
		.swapchainCount = 1,
		.pFences = &*sync.presented
	};
	const vk::PresentInfoKHR present {
		.pNext = &presentFence,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*sync.renderFinished,
		.swapchainCount = 1,
		.pSwapchains = &*swapchain,
		.pImageIndices = &imageIndex
	};
	const auto presentRes = vDev->present(vDev->graphicsQueueFamilyIndex, present);
	if (presentRes == vk::Result::eSuccess)
		return true;
	if (presentRes == vk::Result::eErrorOutOfDateKHR ) {
		MWARN << name << " Swapchain out of date" << endl;
		std::lock_guard lock(stateMutex);
		if (state < eError) {
			state = eSwapOutOfDate;
			stateCond.notify_all();
		}
		waitFence(sync.renderFinishedFence);
		const vk::ReleaseSwapchainImagesInfoEXT releaseInfo {
			.swapchain = swapchain,
			.imageIndexCount = 1,
			.pImageIndices = &imageIndex
		};
		vDev->dev.releaseSwapchainImagesEXT(releaseInfo);
		return false;
	}
	MERROR << name << " Failed to present swapchain image: " << to_str(presentRes) << endl;
	std::lock_guard lock(stateMutex);
	if (state < eError) {
		state = eError;
		stateCond.notify_all();
	}
	return false;
}


void VDisplay::renderLoop() {
	const auto syncIndex = getSyncObj();
	const auto& sync = syncObjs[syncIndex];
	constexpr uint64_t timeout = 5e9;
	auto [result, imageIndex] = swapchain.acquireNextImage(timeout, sync.imageAvailable, nullptr);
	switch (result) {
	case vk::Result::eSuccess:
		break;
	case vk::Result::eErrorOutOfDateKHR:
		MINFO << name << " Swapchain out of date" << endl;
		{
			std::lock_guard lock(stateMutex);
		if (state < eError) {
			state = eSwapOutOfDate;
			stateCond.notify_all();
		}
		return;
		}
	case vk::Result::eSuboptimalKHR:
		MINFO << name << " Swapchain sub optimal" << endl;
		{
		std::lock_guard lock(stateMutex);
		if (state < eError) {
			state = eSwapOutOfDate;
			stateCond.notify_all();
		}
		const vk::ReleaseSwapchainImagesInfoEXT releaseInfo {
			.swapchain = swapchain,
			.imageIndexCount = 1,
			.pImageIndices = &imageIndex
		};
		if (!waitImage(imageIndex))
			return;
		vDev->dev.releaseSwapchainImagesEXT(releaseInfo);
		return;
		}
	default:
		MERROR << name << " Failed to acquire swapchain image: " << to_str(result) << endl;
		{
			std::lock_guard lock(stateMutex);
		if (state < eError) {
			state = eError;
			stateCond.notify_all();
		}
		return;
		}
	}
	if (!waitImage(imageIndex))
		return;
	const auto& img = images[imageIndex];
	//transferBackground(sync, img);
	drawFrame(sync, img);
	if (present(sync, imageIndex))
		busySyncObjs[imageIndex] = syncIndex;
}

bool VDisplay::step() {
	switch (getState()) {
	case eError:
	case eStop:
		return false;
	case eIdle:
		renderLoop();
		return true;
	case eSwapOutOfDate:
		for (const auto& val : busySyncObjs | std::views::values) {
			waitSync(syncObjs[val]);
			freeSyncObjs.push(val);
		}
		busySyncObjs.clear();
		createSwapchain();
		createFramebuffers();
		{
			std::lock_guard lock(stateMutex);
			if (state < eError) {
				state = eIdle;
				stateCond.notify_all();
			}
		}
		return true;
	default:
		MERROR << name << " Invalid state" << endl;
		return false;
	}
}

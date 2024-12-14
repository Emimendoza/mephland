#include <iomanip>
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
	const auto startTime = std::chrono::steady_clock::now();
	while (step()) {}
	const auto end = std::chrono::steady_clock::now();
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime).count();
	const auto fps = static_cast<double>(framesRendered) / (elapsed / 1000.0);
	MINFO << name << " Rendered " << framesRendered << " frames in "  << elapsed
		  << "ms (" << std::setprecision(2) << std::fixed << fps << " fps)" << endl;

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
	vDev->submit(vDev->transferIndex, submit);
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
	};
	const std::array signalSemaphores = {
		*sync.renderFinished,
	};
	constexpr std::array waitStages = {
		static_cast<vk::PipelineStageFlags>(vk::PipelineStageFlagBits::eColorAttachmentOutput)
	};

	const vk::SubmitInfo submit {
		.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
		.pWaitSemaphores = waitSemaphores.data(),
		.pWaitDstStageMask = waitStages.data(),
		.commandBufferCount = 1,
		.pCommandBuffers = &*cmd,
		.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
		.pSignalSemaphores = signalSemaphores.data()
	};

	vDev->submit(vDev->graphicsIndex,submit, renderFinishedFence);
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
	switch (const auto presentRes = vDev->present(vDev->graphicsIndex, present)) {
	case vk::Result::eSuccess:
		return true;
	case vk::Result::eErrorOutOfDateKHR: {
		MWARN << name << " Swapchain out of date" << endl;
		std::lock_guard lock(stateMutex);
		if (state < eError) {
			state = eSwapOutOfDate;
			stateCond.notify_all();
		}
		waitFence<false>(renderFinishedFence);
		const vk::ReleaseSwapchainImagesInfoEXT releaseInfo{
				.swapchain = swapchain,
				.imageIndexCount = 1,
				.pImageIndices = &imageIndex
		};
		vDev->dev.releaseSwapchainImagesEXT(releaseInfo);
		return false;
	} case vk::Result::eSuboptimalKHR: {
		MDEBUG << name << " Swapchain sub optimal" << endl;
		std::lock_guard lock(stateMutex);
		if (state < eError) {
			state = eSwapOutOfDate;
			stateCond.notify_all();
		}
		return true;
	} default: {
		MERROR << name << " Failed to present swapchain image: " << to_str(presentRes) << endl;
		std::lock_guard lock(stateMutex);
		if (state < eError) {
			state = eError;
			stateCond.notify_all();
		}
		return false;
	}
	}
}


void VDisplay::renderLoop() {
	const auto syncIndex = getSyncObj();
	const auto& sync = syncObjs[syncIndex];
	constexpr uint64_t timeout = std::numeric_limits<uint64_t>::max();
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
	waitFence(renderFinishedFence);
	if (renderedNormally)
		readyDisplays++;
	renderedNormally = renderSemaphore.try_acquire_until(nextFrameTime);
	nextFrameTime = std::chrono::steady_clock::now() + maxTimeBetweenFrames.load();
	drawFrame(sync, img);
	if (!present(sync, imageIndex)){
		renderedNormally = false;
		renderSemaphore.release();
		return;
	}
	busySyncObjs[imageIndex] = syncIndex;
	framesRendered++;
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
			waitFence(syncObjs[val].presented);
			freeSyncObjs.push(val);
		}
		busySyncObjs.clear();
		createSwapchain();
			createFrameBuffers();
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

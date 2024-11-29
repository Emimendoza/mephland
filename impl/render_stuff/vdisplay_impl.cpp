#include "vdisplay.h"
#include "vdevice.h"

// Here are helper functions for the renderer and helper classes

using namespace mland;

// We use graphics pool for the background transfer because its GPU to GPU
VDisplay::Image::Image(const VDisplay& us, const vk::Image& img) :
image(img), graphicsCmd(us.vDev->createCommandBuffer(us.graphicsPool)), backgroundCmd(us.vDev->createCommandBuffer(us.graphicsPool)) {
	static constexpr vk::ComponentMapping mapping {
		.r = vk::ComponentSwizzle::eIdentity,
		.g = vk::ComponentSwizzle::eIdentity,
		.b = vk::ComponentSwizzle::eIdentity,
		.a = vk::ComponentSwizzle::eIdentity
	};
	static constexpr vk::ImageSubresourceRange range {
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.baseMipLevel = 0,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 1
	};
	const vk::ImageViewCreateInfo imageViewInfo{
		.image = image,
		.viewType = vk::ImageViewType::e2D,
		.format = us.format,
		.components = mapping,
		.subresourceRange = range
	};
	auto viewRet = us.vDev->dev.createImageView(imageViewInfo);
	if (!viewRet.has_value()) [[unlikely]]
		throw std::runtime_error(us.name + " Failed to create image view: " + to_str(viewRet.error()));
	view = std::move(viewRet.value());
	const vk::FramebufferCreateInfo framebuffer {
		.renderPass = us.renderPass,
		.attachmentCount = 1,
		.pAttachments = &*view, // Abhorrent but necessary
		.width = us.extent.width,
		.height = us.extent.height,
		.layers = 1
	};
	auto fbRet = us.vDev->dev.createFramebuffer(framebuffer);
	if (!fbRet.has_value()) [[unlikely]]
		throw std::runtime_error(us.name + " Failed to create framebuffer: " + to_str(fbRet.error()));
	this->framebuffer = std::move(fbRet.value());
}

vkr::Semaphore VDisplay::createSem() const {
	static constexpr vk::SemaphoreCreateInfo semInfo{};
	auto semRes = vDev->dev.createSemaphore(semInfo);
	if (!semRes.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create semaphore: " + to_str(semRes.error()));
	return std::move(semRes.value());
}

vkr::Fence VDisplay::createFence() const {
	static constexpr vk::FenceCreateInfo fenceInfo{};
	auto fenceRes = vDev->dev.createFence(fenceInfo);
	if (!fenceRes.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create fence: " + to_str(fenceRes.error()));
	return std::move(fenceRes.value());
}

VDisplay::SyncObjs::SyncObjs(const VDisplay& us) :
imageAvailable(us.createSem()),
backgroundFinished(us.createSem()),
renderFinished(us.createSem()),
inFlight(us.createFence()) {}

VDisplay::Image::~Image() {
	graphicsCmd.clear();
	framebuffer.clear();
	view.clear();
}

uint32_t VDisplay::getSyncObj() {
	if (!freeSyncObjs.empty()) {
		const auto temp = freeSyncObjs.top();
		freeSyncObjs.pop();
		return temp;
	}
	syncObjs.emplace_back(*this);
	return syncObjs.size() - 1;
}

void VDisplay::waitFence(const vkr::Fence& fence) const {
	const auto res = vDev->dev.waitForFences(*fence, true, UINT64_MAX);
	if (res != vk::Result::eSuccess) {
		MERROR << name << " Failed to wait for fence: " << to_str(res) << endl;
	}
	vDev->dev.resetFences(*fence);
}
#include "mland/vdisplay.h"
#include "mland/vdevice.h"

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

template<bool signaled>
vkr::Fence VDisplay::createFence() const {
	static constexpr vk::FenceCreateInfo fenceInfo{
		.flags = signaled ? vk::FenceCreateFlagBits::eSignaled : vk::FenceCreateFlagBits{}
	};
	auto fenceRes = vDev->dev.createFence(fenceInfo);
	if (!fenceRes.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create fence: " + to_str(fenceRes.error()));
	return std::move(fenceRes.value());
}
template vkr::Fence VDisplay::createFence<true>() const;
template vkr::Fence VDisplay::createFence<false>() const;

bool VDisplay::waitImage(const uint32_t imageIndex) {
	if (!busySyncObjs.contains(imageIndex)) {
		return true;
	}
	const auto syncIndex = busySyncObjs[imageIndex];
	const auto& sync = syncObjs[syncIndex];
	if (!waitFence(sync.presented)) {
		return false;
	}
	freeSyncObjs.push(syncIndex);
	busySyncObjs.erase(imageIndex);
	return true;
}

VDisplay::SyncObjs::SyncObjs(const VDisplay& us) :
imageAvailable(us.createSem()),
backgroundFinished(us.createSem()),
renderFinished(us.createSem()),
presented(us.createFence()) {}

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

template<bool reset>
bool VDisplay::waitFence(const vkr::Fence& fence) {
	static constexpr uint64_t waitTime = std::numeric_limits<uint64_t>::max();
	switch (const auto res = vDev->dev.waitForFences(*fence, true, waitTime)) {
		case vk::Result::eSuccess:
			if constexpr (reset)
				vDev->dev.resetFences(*fence);
			return true;
		default:
			MERROR << name << " Failed to wait for fence: " << to_str(res) << endl;
			std::lock_guard lock(stateMutex);
			if (state < eError) {
				state = eError;
				stateCond.notify_all();
			}
			return false;
	}
}

template bool VDisplay::waitFence<true>(const vkr::Fence&);
template bool VDisplay::waitFence<false>(const vkr::Fence&);
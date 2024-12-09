#include <mutex>

#include "mland/vdisplay.h"
#include "mland/vinstance.h"
#include "mland/globals.h"

// Here are non render function

using namespace mland;

void VDisplay::start() {
	thread = std::thread(&VDisplay::workerMain, this);
}

void VDisplay::stop() {
	bool weAreStopping = false;
	{
		std::lock_guard lock(stateMutex);
		if (state < eStop) {
			MDEBUG << name << " Stopping display" << endl;
			state = eStop;
			stateCond.notify_all();
			weAreStopping = true;
		}
	}
	std::unique_lock lock(stateMutex);
	if (!weAreStopping) {
		stateCond.wait(lock, [this] { return state == eJoined; });
		return;
	}
	stateCond.wait(lock, [this] {
		if (state == eStopped)
			return true;
		requestRenderForUs();
		return false;
	});
	thread.join();
	state = eJoined;
	stateCond.notify_all();
}

VDisplay::~VDisplay() {
	MDEBUG << name << " Destroying display" << endl;
	stop();
}


vec <vk::DisplayPlanePropertiesKHR> VDisplay::getDisplayPlaneProperties() const {
	vec<vk::DisplayPlanePropertiesKHR> displayPlaneProperties;
	auto allDisplayProps = vDev->pDev.getDisplayPlanePropertiesKHR();
	for (const auto& prop : allDisplayProps) {
		if (prop.currentDisplay == display) {
			displayPlaneProperties.push_back(prop);
		}
	}
	if (displayPlaneProperties.empty()) {
		for (const auto& prop : allDisplayProps) {
			if (prop.currentDisplay == VK_NULL_HANDLE) {
				displayPlaneProperties.push_back(prop);
				return displayPlaneProperties;
			}
		}
	}
	// Sort by stack index
	std::ranges::sort(displayPlaneProperties, [](const auto& a, const auto& b) {
		return a.currentStackIndex < b.currentStackIndex;
	});
	if (displayPlaneProperties.empty()) {
		MERROR << name << " No display plane properties" << endl;
	}
	return displayPlaneProperties;
}

VDisplay::SurfaceInfo VDisplay::getSurfaceInfo() const{
	return {vDev->pDev.getSurfaceFormatsKHR(surface), vDev->pDev.getSurfacePresentModesKHR(surface)};
}

vk::SurfaceFormatKHR VDisplay::bestFormat(const vec<vk::SurfaceFormatKHR> &formatPool, bool HDR) {
	// TODO: Implement HDR format selection
	return formatPool[0];
}

void VDisplay::createEverything() {
	createTimeLineSemaphores();
	createCommandPools();
	createSurface();
	createSwapchain();
	createPipelineLayout();
	createRenderPass();
	createRenderPipeline();
	createFramebuffers();
}

void VDisplay::createSwapchain() {
	const vec format = vDev->pDev.getSurfaceFormatsKHR(surface);
	const vec presentModes = vDev->pDev.getSurfacePresentModesKHR(surface);
	auto presentMode = presentModes[0];
	for (const auto& mode : presentModes) {
		if (mode == vk::PresentModeKHR::eMailbox) {
			MDEBUG << name << " Using mailbox present mode" << endl;
			presentMode = vk::PresentModeKHR::eMailbox;
			break;
		}
	}
	createSwapchain(presentMode, bestFormat(format, renderingMode & eHDR));
	updateOutput();
}

void VDisplay::createTimeLineSemaphores() {
	static constexpr vk::SemaphoreTypeCreateInfo semType {
		.semaphoreType = vk::SemaphoreType::eTimeline,
		.initialValue = 0
	};
	static constexpr vk::SemaphoreCreateInfo semInfo {
		.pNext = &semType
	};
	auto res = vDev->dev.createSemaphore(semInfo);
	auto res2 = vDev->dev.createSemaphore(semInfo);
	if (!res.has_value() || !res2.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create timeline semaphore: " + to_str(res.error()));
	timelineSemaphore = std::make_shared<DisplaySemaphore>(std::move(res.value()),std::move(res2.value()), vDev);
	std::lock_guard lock(timelineMutex);
	const s_ptr newSemaphores = std::make_shared<DisplaySemaphores>(*timelineSemaphores);
	newSemaphores->emplace_back(timelineSemaphore);
	timelineSemaphores = newSemaphores;
}

void VDisplay::createSwapchain(const vk::PresentModeKHR presentMode, const vk::SurfaceFormatKHR format) {
	MDEBUG << name << " Creating swapchain " << endl;
	const auto surfaceCaps = vDev->pDev.getSurfaceCapabilitiesKHR(surface);
	assert(surfaceCaps.currentExtent.height > 0 && surfaceCaps.currentExtent.width > 0);
	constexpr vk::ImageUsageFlags usage =
			vk::ImageUsageFlagBits::eColorAttachment |
			vk::ImageUsageFlagBits::eTransferDst;
	auto& imageCount = globals::bufferCount;

	if (imageCount <= surfaceCaps.minImageCount)
		imageCount = surfaceCaps.minImageCount + 1;
	else if (surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount)
		imageCount = surfaceCaps.maxImageCount;

	vkr::SwapchainKHR oldSwapchain = std::move(swapchain);
	swapchain.clear();
	const vk::SwapchainCreateInfoKHR swapchainInfo {
		.flags = {}, // Unused
		.surface = surface,
		.minImageCount = imageCount,
		.imageFormat = format.format,
		.imageColorSpace = format.colorSpace,
		.imageExtent = surfaceCaps.currentExtent,
		.imageArrayLayers = 1,
		.imageUsage = usage,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.queueFamilyIndexCount = 0, // Unused
		.pQueueFamilyIndices = nullptr, // Unused
		.preTransform = surfaceCaps.currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = presentMode,
		.clipped = true,
		.oldSwapchain = oldSwapchain
	};

	auto res = vDev->dev.createSwapchainKHR(swapchainInfo);
	if (!res.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create swapchain: " + to_str(res.error()));
	oldSwapchain.clear();
	swapchain = std::move(res.value());
	extent = surfaceCaps.currentExtent;
	this->format = format.format;
}

void VDisplay::createPipelineLayout() {
	MDEBUG << name << " Creating pipeline layout" << endl;
	// TODO: Use uniform buffers
	constexpr vk::PipelineLayoutCreateInfo layout {
	};
	auto res = vDev->dev.createPipelineLayout(layout);
	if (!res.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create pipeline layout: " + to_str(res.error()));
	pipelineLayout = std::move(res.value());
}
void VDisplay::createRenderPass() {
	MDEBUG << name << " Creating render pass" << endl;
	vec<vk::AttachmentDescription> attachments{};
	vec<vk::AttachmentReference> attachmentRefs{};
	vec<vk::SubpassDescription> subpasses{};
	vec<vk::SubpassDependency> dependencies{};
	attachments.push_back({
		.format = format,
		.samples = vk::SampleCountFlagBits::e1, // TODO: Implement anti-aliasing
		//.loadOp = vk::AttachmentLoadOp::eLoad,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare, // Maybe implement stencil buffer if depth buffer is implemented
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		//.initialLayout = vk::ImageLayout::eTransferDstOptimal, // TODO: Implement background image
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::ePresentSrcKHR
	});
	attachmentRefs.push_back({
		.attachment = 0,
		.layout = vk::ImageLayout::eColorAttachmentOptimal
	});
	subpasses.push_back({
		.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
		.colorAttachmentCount = 1,
		.pColorAttachments = &attachmentRefs[0]
	});
	dependencies.push_back({
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
		.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
		.srcAccessMask = {},
		.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite
	});
	const vk::RenderPassCreateInfo renderPassInfo{
		.attachmentCount = attachments.size32(),
		.pAttachments = attachments.data(),
		.subpassCount = subpasses.size32(),
		.pSubpasses = subpasses.data(),
		.dependencyCount = dependencies.size32(),
		.pDependencies = dependencies.data()
	};
	auto renderRes = vDev->dev.createRenderPass(renderPassInfo);
	if (!renderRes.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create render pass: " + to_str(renderRes.error()));
	renderPass = std::move(renderRes.value());
}

void VDisplay::createRenderPipeline() {
	MDEBUG << name << " Creating render pipeline" << endl;
	vec<vk::PipelineShaderStageCreateInfo> shaderStages{};
	shaderStages.push_back({
		.stage = vk::ShaderStageFlagBits::eVertex,
		.module = vDev->getVert(),
		.pName = "main"
	});
	shaderStages.push_back({
		.stage = vk::ShaderStageFlagBits::eFragment,
		.module = vDev->getFrag(),
		.pName = "main"
	});
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};

	vk::PipelineViewportStateCreateInfo viewportState{
		.viewportCount = 1,
		.scissorCount = 1,
	};
	vk::PipelineRasterizationStateCreateInfo rasterizer{
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eBack,
		.frontFace = vk::FrontFace::eClockwise,
		.depthBiasEnable = vk::False,
		.lineWidth = 1.0f
	};
	// TODO: Implement anti-aliasing
	vk::PipelineMultisampleStateCreateInfo multisampling{
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False
	};
	// TODO: Add depth buffer
	vk::PipelineColorBlendAttachmentState colorBlendAttachment {
		.blendEnable = vk::True,
		.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
		.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
		.colorBlendOp = vk::BlendOp::eAdd,
		.srcAlphaBlendFactor = vk::BlendFactor::eOne,
		.dstAlphaBlendFactor = vk::BlendFactor::eZero,
		.alphaBlendOp = vk::BlendOp::eAdd,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	};
	vk::PipelineColorBlendStateCreateInfo colorBlend {
		.logicOpEnable = vk::False,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment
	};
	constexpr std::array dynamicStates {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};
	vk::PipelineDynamicStateCreateInfo dynamicState{
		.dynamicStateCount = dynamicStates.size(),
		.pDynamicStates = dynamicStates.data()
	};
	vk::GraphicsPipelineCreateInfo pipelineInfo{
		.flags = vk::PipelineCreateFlagBits::eAllowDerivatives,
		.stageCount = shaderStages.size32(),
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pColorBlendState = &colorBlend,
		.pDynamicState = &dynamicState,
		.layout = pipelineLayout,
		.renderPass = renderPass,
		.subpass = 0,
		.basePipelineHandle = pipeline,
		.basePipelineIndex = -1
	};
	// TODO: Implement pipeline cache
	auto res = vDev->dev.createGraphicsPipeline(nullptr, pipelineInfo);
	if (!res.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create graphics pipeline: " + to_str(res.error()));
	pipeline = std::move(res.value());
}

void VDisplay::createFramebuffers() {
	MDEBUG << name << " Creating framebuffers"<< endl;
	images.clear();
	for (const auto& image : swapchain.getImages()) {
		images.emplace_back(*this, image);
	}
}

void VDisplay::createCommandPools() {
	MDEBUG << name << " Creating command pools" << endl;
	graphicsPool = vDev->createCommandPool(vDev->graphicsQueueFamilyIndex);
	transferPool = vDev->createCommandPool(vDev->transferQueueFamilyIndex);
}

bool VDisplay::isGood() {
	std::unique_lock lock(stateMutex);
	if (state == ePreInit)
		stateCond.wait(lock, [this] { return state > ePreInit; });
	return state < eError;
}

void VDisplay::cleanup() {
	s_ptr<DisplaySemaphores> oldSemaphores;
	{
		std::lock_guard lock(timelineMutex);
		oldSemaphores = timelineSemaphores;
		const s_ptr newSemaphores = std::make_shared<DisplaySemaphores>();
		for (const auto& sem : *timelineSemaphores) {
			if (sem != timelineSemaphore) {
				newSemaphores->push_back(sem);
			}
		}
		timelineSemaphores = newSemaphores;
	}
	while (oldSemaphores.use_count() > 1) {
		// waiting for other threads to stop using our semaphore
		std::this_thread::yield();
	}
	oldSemaphores.reset();
	while (timelineSemaphore.use_count() > 1) {
		std::this_thread::yield();
	}
	for (const auto& val : busySyncObjs | std::views::values) {
		const std:: array fences = {
			*syncObjs[val].renderFinishedFence,
			*syncObjs[val].presented
		};
		while (true) {
			constexpr uint64_t waitTime = 1e8;
			const auto res = vDev->dev.waitForFences(fences, true, waitTime);
			if (res == vk::Result::eSuccess) {
				break;
			}
			requestRenderForUs();
		}
	}

	syncObjs.clear();
	images.clear();
	timelineSemaphore.reset();
	pipeline.clear();
	renderPass.clear();
	pipelineLayout.clear();
	swapchain.clear();
	transferPool.clear();
	graphicsPool.clear();
	deleteSurface();
}

VDisplay::State VDisplay::getState() {
	std::unique_lock lock(stateMutex);
	return state;
}

void VDisplay::requestRender() {
	volatile auto semaphores = timelineSemaphores;
	for (const auto& sem : *const_cast<s_ptr<DisplaySemaphores>&>(semaphores)) {
		const auto& [renderedSem, signalSem, device] = *sem;
		if (renderedSem.getCounterValue() == signalSem.getCounterValue()) {
			continue;
		}
		const vk::SemaphoreSignalInfo signalInfo {
			.semaphore = signalSem,
			.value = renderedSem.getCounterValue()
		};
		device->dev.signalSemaphore(signalInfo);
	}
}

void VDisplay::requestRenderForUs() {
	const auto& [renderedSem, signalSem, device] = *timelineSemaphore;
	if (renderedSem.getCounterValue() == signalSem.getCounterValue()) {
		return;
	}
	const vk::SemaphoreSignalInfo signalInfo {
		.semaphore = signalSem,
		.value = renderedSem.getCounterValue()
	};
	device->dev.signalSemaphore(signalInfo);
}

std::mutex VDisplay::timelineMutex{};
s_ptr<VDisplay::DisplaySemaphores> VDisplay::timelineSemaphores = std::make_shared<DisplaySemaphores>();
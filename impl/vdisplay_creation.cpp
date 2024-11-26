#include "vdisplay_impl.h"
#include "vinstance.h"

// Here are non render function

using namespace mland;

void VDisplay::impl::start() {
	thread = std::thread(&impl::workerMain, this);
}

VDisplay::VDisplay(str&& name, VDevice* vDev, vkr::DisplayKHR&& display, const vk::DisplayPropertiesKHR& displayProps, const DRM_Device::Connector con) :
	p(std::make_unique<impl>()) {
	p->name = std::move(name);
	p->vDev = vDev;
	p->display = std::move(display);
	p->displayProps = displayProps;
	p->con = con;
	p->start();
}

VDisplay::~VDisplay() {
	if (p == nullptr) {
		return;
	}
	{
		std::unique_lock lock(p->stateMutex);
		p->state = eStop;
	}
	p->thread.join();
	p->vDev->connectors.erase(p->con);
}

vec <vk::DisplayPlanePropertiesKHR> VDisplay::getDisplayPlaneProperties() const {
	vec<vk::DisplayPlanePropertiesKHR> displayPlaneProperties;
	auto allDisplayProps = p->vDev->pDev.getDisplayPlanePropertiesKHR();
	for (const auto& prop : allDisplayProps) {
		if (prop.currentDisplay == p->display) {
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
		MERROR << p->name << " No display plane properties" << endl;
	}
	return displayPlaneProperties;
}

VDisplay::SurfaceInfo VDisplay::getSurfaceInfo() const{
	return {p->vDev->pDev.getSurfaceFormatsKHR(p->surface), p->vDev->pDev.getSurfacePresentModesKHR(p->surface)};
}

vk::SurfaceFormatKHR VDisplay::bestFormat(const vec<vk::SurfaceFormatKHR> &formatPool, bool HDR) {
	// TODO: Implement HDR format selection
	return formatPool[0];
}

void VDisplay::impl::createEverything() {
	createTimeLineSemaphores();
	createCommandPools();
	createSurface(createModes());
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
	createPipelineLayout();
	createRenderPass();
	createRenderPipeline();
	createFramebuffers();
}

void VDisplay::impl::createTimeLineSemaphores() {
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

uint32_t VDisplay::impl::createModes() {
	MDEBUG << name << " Creating display modes" << endl;
	displayModes = std::move(display.getModeProperties());
	const auto bestRes = displayProps.physicalResolution;
	uint32_t best = 0;
	uint32_t best_refresh = 0;
	for(uint32_t i = 0; i < displayModes.size(); i++) {
		const auto& [displayMode, parameters] = displayModes[i];
		MDEBUG << name << " Found mode: " << parameters.refreshRate/1000.0 << " Hz, " <<
			parameters.visibleRegion.width << "x" << parameters.visibleRegion.height << endl;
		if (parameters.visibleRegion != bestRes) {
			continue;
		}
		if (parameters.refreshRate > best_refresh) {
			best_refresh = parameters.refreshRate;
			best = i;
		}
	}
	MINFO << name << " Best mode: " << displayModes[best].parameters.refreshRate / 1000.0 << " Hz, " <<
		  displayModes[best].parameters.visibleRegion.width << "x" << displayModes[best].parameters.visibleRegion.height << endl;
	return best;
}

void VDisplay::impl::createSurface(const uint32_t mode_index) {
	MDEBUG << name << " Creating surface" << endl;
	assert(mode_index < displayModes.size());
	const auto &[displayMode, parameters] = displayModes[mode_index];
	const vk::DisplaySurfaceCreateInfoKHR surfaceCreateInfo{
		.displayMode = displayMode,
		.planeIndex = 0,
		.planeStackIndex = 0,
		.transform = vk::SurfaceTransformFlagBitsKHR::eIdentity,
		.globalAlpha = {}, // Ignored
		.alphaMode = vk::DisplayPlaneAlphaFlagBitsKHR::ePerPixel,
		.imageExtent = parameters.visibleRegion,
	};
	const auto &instance = vDev->parent->getInstance();
	auto res = instance.createDisplayPlaneSurfaceKHR(surfaceCreateInfo);
	if (!res.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create surface: " + to_str(res.error()));
	assert(vDev->pDev.getSurfaceSupportKHR(vDev->graphicsQueueFamilyIndex, res.value()));
	surface = std::move(res.value());
}

void VDisplay::impl::createSwapchain(const vk::PresentModeKHR presentMode, const vk::SurfaceFormatKHR format) {
	MDEBUG << name << " Creating swapchain " << endl;
	const auto surfaceCaps = vDev->pDev.getSurfaceCapabilitiesKHR(surface);
	assert(surfaceCaps.currentExtent.height > 0 && surfaceCaps.currentExtent.width > 0);
	static constexpr vk::ImageUsageFlags usage =
			vk::ImageUsageFlagBits::eColorAttachment |
			vk::ImageUsageFlagBits::eTransferDst;
	auto& imageCount = globals::bufferCount;

	if (imageCount <= surfaceCaps.minImageCount)
		imageCount = surfaceCaps.minImageCount + 1;
	else if (surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount)
		imageCount = surfaceCaps.maxImageCount;

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
		.oldSwapchain = swapchain
	};

	auto res = vDev->dev.createSwapchainKHR(swapchainInfo);
	if (!res.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create swapchain: " + to_str(res.error()));
	swapchain = std::move(res.value());
	extent = surfaceCaps.currentExtent;
	this->format = format.format;
}

void VDisplay::impl::createPipelineLayout() {
	MDEBUG << name << " Creating pipeline layout" << endl;
	// TODO: Use uniform buffers
	static constexpr vk::PipelineLayoutCreateInfo layout {
	};
	auto res = vDev->dev.createPipelineLayout(layout);
	if (!res.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create pipeline layout: " + to_str(res.error()));
	pipelineLayout = std::move(res.value());
}
void VDisplay::impl::createRenderPass() {
	MDEBUG << name << " Creating render pass" << endl;
	vec<vk::AttachmentDescription> attachments{};
	vec<vk::AttachmentReference> attachmentRefs{};
	vec<vk::SubpassDescription> subpasses{};
	vec<vk::SubpassDependency> dependencies{};
	attachments.push_back({
		.format = format,
		.samples = vk::SampleCountFlagBits::e1, // TODO: Implement anti-aliasing
		.loadOp = vk::AttachmentLoadOp::eLoad,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare, // Maybe implement stencil buffer if depth buffer is implemented
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eTransferDstOptimal,
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

void VDisplay::impl::createRenderPipeline() {
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
	static constexpr  std::array dynamicStates {
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

void VDisplay::impl::createFramebuffers() {
	MDEBUG << name << " Creating framebuffers"<< endl;
	images.clear();
	for (const auto& image : swapchain.getImages()) {
		images.emplace_back(*this, image);
	}
}

void VDisplay::impl::createCommandPools() {
	MDEBUG << name << " Creating command pools" << endl;
	graphicsPool = vDev->createCommandPool(vDev->graphicsQueueFamilyIndex);
	transferPool = vDev->createCommandPool(vDev->transferQueueFamilyIndex);
}

bool VDisplay::isGood() {
	if (p == nullptr) {
		MERROR << "Display is null" << endl;
		return false;
	}

	auto& us = *p;
	std::unique_lock lock(us.stateMutex);
	us.stateCond.wait(lock, [&us] { return us.state > ePreInit; });
	return us.state < eError;
}

void VDisplay::impl::cleanup() {
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
	images.clear();
	syncObjs.clear();
	pipeline.clear();
	pipelineLayout.clear();
	renderPass.clear();
	graphicsPool.clear();
	swapchain.clear();
	surface.clear();
	while (oldSemaphores.use_count() > 1) {
		// waiting for other threads to stop using our semaphore
		std::this_thread::yield();
	}
}

void VDisplay::setState(const State state) {
	const auto mask = eIdle | eStop | state;
	auto& us = *p;
	std::unique_lock lock(us.stateMutex);
	us.stateCond.wait(lock, [&us, mask]{return (us.state & mask) != 0; });
	if (us.state & state) {
		return; // Already in the desired state
	}
	if (us.state & eStop) {
		MERROR << us.name << " Display is stopped" << endl;
		return;
	}
	us.state = state;
}

VDisplay::State VDisplay::impl::getState() {
	std::unique_lock lock(stateMutex);
	return state;
}

void VDisplay::requestRender() {
	volatile auto semaphores = timelineSemaphores;
	for (const auto& sem : *const_cast<s_ptr<DisplaySemaphores>&>(semaphores)) {
		const auto& [renderedSem, signalSem, device] = *sem;
		const vk::SemaphoreSignalInfo signalInfo {
			.semaphore = signalSem,
			.value = renderedSem.getCounterValue()
		};
		device->dev.signalSemaphore(signalInfo);
	}
}


std::mutex VDisplay::timelineMutex{};
s_ptr<VDisplay::DisplaySemaphores> VDisplay::timelineSemaphores = std::make_shared<DisplaySemaphores>();
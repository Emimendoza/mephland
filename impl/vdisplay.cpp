#include "vdisplay.h"
#include "vulk.h"
#include "vdevice.h"
#include "vinstance.h"

using namespace mland;

uint32_t VDisplay::createModes() {
	MDEBUG << name << " Creating display modes" << endl;
	const_cast<vec<vk::DisplayModePropertiesKHR>&>(displayModes) = std::move(display.getModeProperties());
	auto bestRes = displayProps.physicalResolution;
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

void VDisplay::createSurfaces(const uint32_t mode_index) {
	MDEBUG << name << " Creating surfaces" << endl;
	assert(mode_index < displayModes.size());
	uint32_t count = 1;
	if (renderingMode & eLayered) {
		count = LayeredCount;
	}
	const auto &[displayMode, parameters] = displayModes[mode_index];
	sri.clear();
	for (uint32_t i = 0; i < count; i++) {
		const vk::DisplaySurfaceCreateInfoKHR surfaceCreateInfo{
				.displayMode = displayMode,
				.planeIndex = i,
				.planeStackIndex = i,
				.transform = vk::SurfaceTransformFlagBitsKHR::eIdentity,
				.globalAlpha = {}, // Ignored
				.alphaMode = vk::DisplayPlaneAlphaFlagBitsKHR::ePerPixel,
				.imageExtent = parameters.visibleRegion,
		};
		const auto &instance = parent->parent->getInstance();
		auto res = instance.createDisplayPlaneSurfaceKHR(surfaceCreateInfo);
		if (!res.has_value()) [[unlikely]]
			throw std::runtime_error(name + " Failed to create surface: " + to_str(res.error()));
		assert(parent->pDev.getSurfaceSupportKHR(parent->graphicsQueueFamilyIndex, res.value()));
		SurfaceRenderInfo surface{};
		surface.surface = std::move(res.value());
		sri.push_back(std::move(surface));
	}
}

vec <vk::DisplayPlanePropertiesKHR> VDisplay::getDisplayPlaneProperties() const {
	vec<vk::DisplayPlanePropertiesKHR> displayPlaneProperties;
	auto allDisplayProps = parent->pDev.getDisplayPlanePropertiesKHR();
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
		throw std::runtime_error(name + " No display plane properties for display");
	}
	return displayPlaneProperties;
}

VDisplay::SurfaceInfo VDisplay::getSurfaceInfo(uint32_t surface_index) const{
	assert(surface_index < sri.size());
	const auto& surface = sri[surface_index];
	return {parent->pDev.getSurfaceFormatsKHR(surface.surface), parent->pDev.getSurfacePresentModesKHR(surface.surface)};
}

vk::SurfaceFormatKHR VDisplay::bestFormat(const vec<vk::SurfaceFormatKHR> &formatPool, bool HDR) {
	// TODO: Implement HDR format selection
	return formatPool[0];
}

void VDisplay::createSwapchains() {
	for (auto i = 0; i < sri.size32(); i++) {
		const auto [format, presentModes] = getSurfaceInfo(i);
		auto presentMode = presentModes[0];
		for (const auto& mode : presentModes) {
			if (mode == vk::PresentModeKHR::eMailbox) {
				MDEBUG << name << " Using mailbox present mode" << endl;
				presentMode = vk::PresentModeKHR::eMailbox;
				break;
			}
		}
		createSwapchain(i, presentMode, bestFormat(format, renderingMode & eHDR));
		createPipelineLayout(i);
		createRenderPass(i);
		createRenderPipeline(i);
		createFramebuffers(i);
		createRenderer(i);
	}
}

void VDisplay::createSwapchain(const uint32_t surface_index, const vk::PresentModeKHR presentMode, const vk::SurfaceFormatKHR format) {
	MDEBUG << name << " Creating swapchain " << surface_index << endl;
	assert(surface_index < sri.size());
	const auto surfaceCaps = parent->pDev.getSurfaceCapabilitiesKHR(sri[surface_index].surface);
	assert(surfaceCaps.currentExtent.height > 0 && surfaceCaps.currentExtent.width > 0);
	constexpr vk::ImageUsageFlags usage =
			vk::ImageUsageFlagBits::eColorAttachment |
			vk::ImageUsageFlagBits::eTransferDst;
	auto& imageCount = globals::bufferCount;
	if (imageCount <= surfaceCaps.minImageCount)
		imageCount = surfaceCaps.minImageCount + 1;
	else if (surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount)
		imageCount = surfaceCaps.maxImageCount;

	auto oldSwapchain = std::move(sri[surface_index].swapchain);
	auto surface = std::move(sri[surface_index].surface);
	sri[surface_index].clear();


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

	auto res = parent->dev.createSwapchainKHR(swapchainInfo);
	if (!res.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create swapchain: " + to_str(res.error()));
	oldSwapchain.clear();
	sri[surface_index].surface = std::move(surface);
	sri[surface_index].swapchain = std::move(res.value());
	sri[surface_index].extent = surfaceCaps.currentExtent;
	sri[surface_index].format = format.format;
}

void VDisplay::createPipelineLayout(uint32_t surface_index) {
	MDEBUG << name << " Creating pipeline layout for swapchain " << surface_index << endl;
	assert(surface_index < sri.size());
	auto& us = sri[surface_index];
	// TODO: Use uniform buffers
	constexpr vk::PipelineLayoutCreateInfo layout {
	};
	auto res = parent->dev.createPipelineLayout(layout);
	if (!res.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create pipeline layout: " + to_str(res.error()));
	us.pipelineLayout = std::move(res.value());
}
void VDisplay::createRenderPass(uint32_t surface_index) {
	MDEBUG << name << " Creating render pass for swapchain " << surface_index << endl;
	assert(surface_index < sri.size());
	auto& us = sri[surface_index];
	vec<vk::AttachmentDescription> attachments{};
	vec<vk::AttachmentReference> attachmentRefs{};
	vec<vk::SubpassDescription> subpasses{};
	vec<vk::SubpassDependency> dependencies{};
	attachments.push_back({
		.format = us.format,
		.samples = vk::SampleCountFlagBits::e1, // TODO: Implement anti-aliasing
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare, // Maybe implement stencil buffer if depth buffer is implemented
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
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
	auto renderRes = parent->dev.createRenderPass(renderPassInfo);
	if (!renderRes.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create render pass: " + to_str(renderRes.error()));
	us.renderPass = std::move(renderRes.value());
}

void VDisplay::createRenderPipeline(const uint32_t surface_index) {
	MDEBUG << name << " Creating render pipeline for swapchain " << surface_index << endl;
	assert(surface_index < sri.size());
	auto& us = sri[surface_index];
	vec<vk::PipelineShaderStageCreateInfo> shaderStages{};
	shaderStages.push_back({
		.stage = vk::ShaderStageFlagBits::eVertex,
		.module = parent->getVert(),
		.pName = "main"
	});
	shaderStages.push_back({
		.stage = vk::ShaderStageFlagBits::eFragment,
		.module = parent->getFrag(),
		.pName = "main"
	});
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};
	vk::Viewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(us.extent.width),
		.height = static_cast<float>(us.extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	vk::Rect2D scissor{
		.offset = {0, 0},
		.extent = us.extent
	};
	vk::PipelineViewportStateCreateInfo viewportState{
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
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
	vk::PipelineDynamicStateCreateInfo dynamicState{
		.dynamicStateCount = 0,
		.pDynamicStates = nullptr
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
		.layout = us.pipelineLayout,
		.renderPass = us.renderPass,
		.subpass = 0,
		.basePipelineHandle = us.pipeline,
		.basePipelineIndex = -1
	};
	// TODO: Implement pipeline cache
	auto res = parent->dev.createGraphicsPipeline(nullptr, pipelineInfo);
	if (!res.has_value()) [[unlikely]]
		throw std::runtime_error(name + " Failed to create graphics pipeline: " + to_str(res.error()));
	us.pipeline = std::move(res.value());

}

void VDisplay::createFramebuffers(const uint32_t surface_index) {
	MDEBUG << name << " Creating framebuffers for swapchain " << surface_index << endl;
	assert(surface_index < sri.size());
	auto& us = sri[surface_index];
	us.images.clear();
	for (const auto& image : us.swapchain.getImages()) {
		Image img{};
		img.image = image;
		constexpr vk::ComponentMapping mapping {
			.r = vk::ComponentSwizzle::eIdentity,
			.g = vk::ComponentSwizzle::eIdentity,
			.b = vk::ComponentSwizzle::eIdentity,
			.a = vk::ComponentSwizzle::eIdentity
		};
		const vk::ImageViewCreateInfo imageViewInfo{
			.flags = {}, // Unused
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = us.format,
			.components = mapping,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};
		auto viewRet = parent->dev.createImageView(imageViewInfo);
		if (!viewRet.has_value()) [[unlikely]]
			throw std::runtime_error(name + " Failed to create image view: " + to_str(viewRet.error()));
		img.view = std::move(viewRet.value());
		const vk::FramebufferCreateInfo framebuffer {
			.renderPass = us.renderPass,
			.attachmentCount = 1,
			.pAttachments = &(*img.view), // Abhorrent but necessary
			.width = us.extent.width,
			.height = us.extent.height,
			.layers = 1
		};
		auto fbRet = parent->dev.createFramebuffer(framebuffer);
		if (!fbRet.has_value()) [[unlikely]]
			throw std::runtime_error(name + " Failed to create framebuffer: " + to_str(fbRet.error()));
		img.framebuffer = std::move(fbRet.value());
		us.images.push_back(std::move(img));
	}
}

void VDisplay::createRenderer(const uint32_t surface_index) {
	MDEBUG << name << " Creating renderer for swapchain " << surface_index << endl;
	assert(surface_index < sri.size());
	auto& us = sri[surface_index];
	us.renderer = std::make_unique<Renderer>(&us, name + " R" + std::to_string(surface_index), renderingMode, parent);
}

VDisplay::Image::~Image() {
	framebuffer.clear();
	view.clear();
}

void VDisplay::SurfaceRenderInfo::clear() {
	renderer.reset();
	images.clear();
	pipeline.clear();
	renderPass.clear();
	pipelineLayout.clear();
	swapchain.clear();
	surface.clear();
}

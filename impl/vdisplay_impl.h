#pragma once
#include <stack>

#include "vdisplay.h"
#include "vulk.h"
#include "vdevice.h"

namespace mland {
struct VDisplay::impl {
	struct Image {
		vk::Image image;
		vkr::ImageView view{nullptr};
		vkr::Framebuffer framebuffer{nullptr};
		vkr::CommandBuffer graphicsCmd;
		vkr::CommandBuffer backgroundCmd;
		Image(const impl& us, const vk::Image& img);
		Image(Image&&) = default;
		~Image();
	};
	struct SyncObjs {
		vkr::Semaphore imageAvailable;
		vkr::Semaphore backgroundFinished;
		vkr::Semaphore renderFinished;
		vkr::Fence inFlight;
		SyncObjs(const impl& us);
	};

	str name;
	State state{ePreInit};
	std::mutex stateMutex{};
	std::condition_variable stateCond{};
	// Assets
	vkr::Image background{nullptr};
	vkr::Semaphore backgroundSemaphore{nullptr};
	// Rendering data
	vec<vk::DisplayModePropertiesKHR> displayModes{};
	vk::DisplayPropertiesKHR displayProps{};
	RenderingMode renderingMode{};
	VDevice* vDev{nullptr};
	vkr::DisplayKHR display{nullptr};
	vkr::DisplayModeKHR mode{nullptr};
	vkr::SurfaceKHR surface{nullptr};
	vkr::SwapchainKHR swapchain{nullptr};
	vk::Rect2D displayRegion{};
	vk::Extent2D extent{};
	vk::Format format{};
	vec<Image> images{};
	vkr::PipelineLayout pipelineLayout{nullptr};
	vkr::RenderPass renderPass{nullptr};
	vkr::Pipeline pipeline{nullptr};
	// Sync objects
	SharedDisplaySemaphore timelineSemaphore;
	uint64_t timelineValue{0};
	std::thread thread{};
	vec<SyncObjs> syncObjs{};
	vkr::CommandPool graphicsPool{nullptr};
	vkr::CommandPool transferPool{nullptr};
	std::stack<uint32_t> freeSyncObjs{};
	map<uint32_t, uint32_t> busySyncObjs{};
	DRM_Device::Connector con{};

	void createEverything();
	void createTimeLineSemaphores();
	void createCommandPools();
	uint32_t createModes();
	void createSurface(uint32_t mode_index);
	void createSwapchain(vk::PresentModeKHR presentMode, vk::SurfaceFormatKHR);
	void createPipelineLayout();
	void createRenderPass();
	void createRenderPipeline();
	void createFramebuffers();

	void start();
	void workerMain();
	bool step();
	void renderLoop();
	void cleanup();


	// Within renderLoop
	void transferBackground(const SyncObjs& sync, const Image& img);
	void drawFrame(const SyncObjs& sync, const Image& img);
	void present(const SyncObjs& sync, const Image& img, const uint32_t& imageIndex);

	uint32_t getSyncObj();
	void waitFence(const vkr::Fence& fence) const;
	State getState();

	// Helpers
	vkr::Semaphore createSem() const;
	vkr::Fence createFence() const;
};
}
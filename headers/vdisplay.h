#pragma once
#include <condition_variable>
#include <thread>
#include <stack>
#include "common.h"
#include "vdevice.h"
#include "vulk.h"

namespace mland {
class Backend::VDisplay {
public:
	MCLASS(VDisplay);

	enum RenderingMode : uint32_t {
		eCompositing = 0 << 0,
		eDirect = 1 << 0,
		eHDR = 1 << 1, // TODO: IMPLEMENT
		eTearingFullscreen = 1 << 2,
		eTearingAllApps = 1 << 1
	};

	enum State : uint64_t {
		// Init sequence
		ePreInit,
		// main loop
		eIdle,
		eUpdateBackground,
		// Errors and cleanup
		eSubOptimal = std::numeric_limits<uint64_t>::max() / 2, // Potentially recoverable
		eError, // Unrecoverable
		eStop = std::numeric_limits<uint64_t>::max()-1,
		eStopped = std::numeric_limits<uint64_t>::max()
	};

	VDisplay(VDisplay&& other) = delete;
	VDisplay(const VDisplay&) = delete;
	virtual ~VDisplay();

	vec<vk::DisplayPlanePropertiesKHR> getDisplayPlaneProperties() const;
	typedef std::pair<vec<vk::SurfaceFormatKHR>, vec<vk::PresentModeKHR>> SurfaceInfo;
	SurfaceInfo getSurfaceInfo() const;
	static vk::SurfaceFormatKHR bestFormat(const vec<vk::SurfaceFormatKHR>& formatPool, bool HDR);
	static void requestRender();

	bool isGood();

	void setState(State state);

protected:
	friend VDevice;
	VDisplay(str&& name, VDevice* vDev) : name(std::move(name)), vDev(vDev) {}
	// <Rendered, Signal, Device>
	typedef std::tuple<vkr::Semaphore, vkr::Semaphore, VDevice*> DisplaySemaphore;
	typedef s_ptr<DisplaySemaphore> SharedDisplaySemaphore;
	typedef vec<SharedDisplaySemaphore> DisplaySemaphores;
	static std::mutex timelineMutex;
	static s_ptr<DisplaySemaphores> timelineSemaphores;
	struct Image {
		vk::Image image;
		vkr::ImageView view{nullptr};
		vkr::Framebuffer framebuffer{nullptr};
		vkr::CommandBuffer graphicsCmd;
		vkr::CommandBuffer backgroundCmd;
		Image(const VDisplay& us, const vk::Image& img);
		Image(Image&&) = default;
		~Image();
	};
	struct SyncObjs {
		vkr::Semaphore imageAvailable;
		vkr::Semaphore backgroundFinished;
		vkr::Semaphore renderFinished;
		vkr::Fence inFlight;
		SyncObjs(const VDisplay& us);
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
	VDevice* vDev;
	vkr::DisplayKHR display{nullptr};
	vkr::DisplayModeKHR mode{nullptr};
	vk::SurfaceKHR surface{nullptr};
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


	void createEverything();
	void createTimeLineSemaphores();
	void createCommandPools();
	virtual void createSurface() = 0;
	void createSwapchain(vk::PresentModeKHR presentMode, vk::SurfaceFormatKHR);
	void createPipelineLayout();
	void createRenderPass();
	void createRenderPipeline();
	void createFramebuffers();

	void start();
	void stop();
	void workerMain();
	bool step();
	void renderLoop();
	virtual void deleteSurface() = 0;
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

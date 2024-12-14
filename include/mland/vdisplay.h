#pragma once
#include <condition_variable>
#include <semaphore>
#include <thread>
#include <stack>
#include "common.h"
#include "vdevice.h"
#include "vulk.h"
#include "interfaces/output.h"

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
		eSwapOutOfDate,
		eError, // Unrecoverable
		eStop = std::numeric_limits<uint64_t>::max()-2,
		eStopped,
		eJoined
	};

	VDisplay(VDisplay&& other) = delete;
	VDisplay(const VDisplay&) = delete;
	virtual ~VDisplay();

	vec<vk::DisplayPlanePropertiesKHR> getDisplayPlaneProperties() const;
	typedef std::pair<vec<vk::SurfaceFormatKHR>, vec<vk::PresentModeKHR>> SurfaceInfo;
	SurfaceInfo getSurfaceInfo() const;
	static vk::SurfaceFormatKHR bestFormat(const vec<vk::SurfaceFormatKHR>& formatPool, bool HDR);



	std::condition_variable stateCond{};
	bool isGood();

	// Defined in interfaces::Output
	void bindToWayland(const WLServer& server);

protected:
	friend Controller;
	static void setMaxTimeBetweenFrames(std::chrono::milliseconds time);
	static void requestRender();

	friend interfaces::Output;
	friend VDevice;
	VDisplay(str&& name, VDevice* vDev) : name(std::move(name)), vDev(vDev) {}

	static std::atomic<std::chrono::milliseconds> maxTimeBetweenFrames;
	static std::atomic<uint8_t> readyDisplays;
	static std::counting_semaphore<std::numeric_limits<uint8_t>::max()> renderSemaphore;

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
		vkr::Fence presented;
		SyncObjs(const VDisplay& us);
		SyncObjs(SyncObjs&&) = default;
		~SyncObjs() = default;
	};

	// Meta info (used for logging and for wayland)
	str name;
	str make = "Unknown";
	str model = "Unknown";
	VkExtent2D size{};

	// State
	State state{ePreInit};
	std::mutex stateMutex{};

	uint64_t framesRendered{0};
	std::chrono::time_point<std::chrono::steady_clock> nextFrameTime{};
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
	std::mutex extentMutex{};
	vk::Extent2D extent{};

	vk::Format format{};
	vec<Image> images{};
	vkr::PipelineLayout pipelineLayout{nullptr};
	vkr::RenderPass renderPass{nullptr};
	vkr::Pipeline pipeline{nullptr};
	// Sync objects
	vkr::Fence renderFinishedFence{nullptr};
	std::thread thread{};
	vec<SyncObjs> syncObjs{};
	vkr::CommandPool graphicsPool{nullptr};
	vkr::CommandPool transferPool{nullptr};
	std::stack<uint32_t> freeSyncObjs{};
	map<uint32_t, uint32_t> busySyncObjs{};

	// Wayland stuff
	u_ptr<interfaces::Output> output;
	std::mutex modeMutex{};
	int32_t refreshRate{0};
	bool preferredMode{true};
	bool renderedNormally{true};


	void createEverything();

	void createCommandPools();
	virtual void createSurface() = 0;
	void createSwapchain();
	void createSwapchain(vk::PresentModeKHR presentMode, vk::SurfaceFormatKHR);
	void createPipelineLayout();
	void createRenderPass();
	void createRenderPipeline();
	void createFrameBuffers();

	// Defined in interfaces::Output
	void updateOutput();

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
	bool present(const SyncObjs& sync, const uint32_t& imageIndex);

	uint32_t getSyncObj();

	template<bool reset = true>
	bool waitFence(const vkr::Fence& fence);
	State getState();

	// Helpers
	vkr::Semaphore createSem() const;
	template<bool signaled = false>
	vkr::Fence createFence() const;
	bool waitImage(uint32_t imageIndex);

};
}

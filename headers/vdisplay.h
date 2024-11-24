#pragma once
#include <stack>
#include <thread>
#include "common.h"
#include "vulk.h"
namespace mland {
	class VDisplay {
		friend VDevice;
		str name;
		VDisplay(str&& name) : name(std::move(name)) {}
	public:
		enum RenderingMode : uint32_t {
			eCompositing = 0 << 0,
			eDirect = 1 << 0,
			eLayered = 1 << 1, // TODO: IMPLEMENT
			eHDR = 1 << 2, // TODO: IMPLEMENT
			eTearingFullscreen = 1 << 3,
			eTearingAllApps = 1 << 4
		};

		static constexpr uint32_t LayeredMainIndex = 0;
		static constexpr uint32_t LayeredOverlayIndex = 1;
		static constexpr uint32_t LayeredCursorIndex = 2;
		static constexpr uint32_t LayeredCount = 999; // TODO: IMPLEMENT
		typedef std::pair<vec<vk::SurfaceFormatKHR>, vec<vk::PresentModeKHR>> SurfaceInfo;



		VDisplay(VDisplay&& other) noexcept = default;
		VDisplay& operator=(VDisplay&&) = default;
		VDisplay(const VDisplay&) = delete;
		MCLASS(VDisplay);
		vec<vk::DisplayPlanePropertiesKHR> getDisplayPlaneProperties() const;
		uint32_t createModes();
		void createSurfaces(uint32_t mode_index);
		SurfaceInfo getSurfaceInfo(uint32_t surface_index) const;
		void createSwapchains();
		static vk::SurfaceFormatKHR bestFormat(const vec<vk::SurfaceFormatKHR>& formatPool, bool HDR);

	private:
		vec<vk::DisplayModePropertiesKHR> displayModes{};
		vk::DisplayPropertiesKHR displayProps{};
		RenderingMode renderingMode{};

		void createSwapchain(uint32_t surface_index, vk::PresentModeKHR presentMode, vk::SurfaceFormatKHR);
		void createPipelineLayout(uint32_t surface_index);
		void createRenderPass(uint32_t surface_index);
		void createRenderPipeline(uint32_t surface_index);
		void createFramebuffers(uint32_t surface_index);
		void createRenderer(uint32_t surface_index);


		struct Image {
			// Initialized by VDisplay::createFramebuffers
			vk::Image image{};
			vkr::ImageView view{nullptr};
			vkr::Framebuffer framebuffer{nullptr};
			// Initialized by VDisplay::Renderer::renderMain
			vkr::CommandBuffer graphicsCmd{nullptr};

			Image() = default;
			Image(const Image&) = delete;
			Image(Image&& other) noexcept = default;
			~Image();
		};

		class Renderer;

		struct SurfaceRenderInfo {
			vkr::SurfaceKHR surface{nullptr};
			vkr::SwapchainKHR swapchain{nullptr};
			vk::Extent2D extent{};
			vk::Format format{};
			vec<Image> images{};
			vkr::PipelineLayout pipelineLayout{nullptr};
			vkr::RenderPass renderPass{nullptr};
			vkr::Pipeline pipeline{nullptr};
			u_ptr<Renderer> renderer{};

			void clear();
			SurfaceRenderInfo() = default;
			SurfaceRenderInfo(const SurfaceRenderInfo&) = delete;
			SurfaceRenderInfo(SurfaceRenderInfo&& other) noexcept = default;
			~SurfaceRenderInfo() { clear(); }
		};

		struct SyncObjs {
			vkr::Semaphore imageAvailable{nullptr};
			vkr::Semaphore renderFinished{nullptr};
			vkr::Fence inFlight{nullptr};
		};

		class Renderer {
			MCLASS(Renderer);
			str name;
			SurfaceRenderInfo *sri{nullptr};
			VDevice* dev{nullptr};
			std::thread thread{};
			vec<SyncObjs> syncObjs{};
			vkr::CommandPool cmdPool{nullptr};
			std::stack<uint32_t> freeSyncObjs{};
			map<uint32_t, uint32_t> busySyncObjs{};

			std::atomic_flag stop = ATOMIC_FLAG_INIT;
			uint32_t getSyncObj();

			template<bool DIRECT, bool LAYERED>
			void renderMain();
			template<bool DIRECT, bool LAYERED>
			void renderLoop();
			void waitFence(const vkr::Fence& fence);

			static constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

		public:
			Renderer(SurfaceRenderInfo* sri, str&& name, RenderingMode mode, VDevice* dev);
			Renderer(const Renderer&) = delete;
			Renderer(Renderer&& other) noexcept = delete;
			~Renderer();
		};
		VDevice* parent{nullptr};
		vkr::DisplayKHR display{nullptr};
		vkr::DisplayModeKHR mode{nullptr};
		vec<SurfaceRenderInfo> sri{};
	};
}
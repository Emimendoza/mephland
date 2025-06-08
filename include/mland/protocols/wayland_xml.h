#pragma once
#include <vector>
#include <unordered_map>
#include <stack>

#include "./protocol_base.h"


namespace mland::wayland_xml {

struct WlRegion {
	enum class RectType {
		eAdd,
		eSub
	};
	struct SubRect {
		Rect rect;
		RectType type;
	};

	std::stack<SubRect> rects;
};

class WlCompositor final : public ProtocolBase {
public:
	explicit WlCompositor(wl_display* display);
protected:
	void bind(wl_client* client, int32_t version, uint32_t id) override;
private:
	struct wl_compositor_interface interface = {
		.create_surface = createSurface,
		.create_region = createRegion
	};

	struct wl_region_interface regionInterface = {
		.destroy = destroyRegion,
		.add = addRect,
		.subtract = subtractRect
	};

	static void createRegion(wl_client* client, wl_resource* resource, WaylandID id);
	static void createSurface(wl_client* client, wl_resource* resource, WaylandID id);

	static void destroyRegion(wl_client* client, wl_resource* resource);

	static void addRect(wl_client* client, wl_resource* resource, int32_t x, int32_t y, int32_t width, int32_t height);

	static void subtractRect(wl_client* client, wl_resource* resource, int32_t x, int32_t y, int32_t width, int32_t height);

	std::unordered_map<WaylandID, wl_resource*> surfaces;
	std::unordered_map<WaylandID, WlRegion> regions;

};

}
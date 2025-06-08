#pragma once
#include <memory>
#include <wayland-server-protocol.h>

namespace mland {

typedef uint32_t WaylandID;

struct Rect {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
};

class ProtocolBase {
protected:
	virtual void bind(wl_client* client, int32_t version, uint32_t id) = 0;

	ProtocolBase(wl_display* display, const wl_interface* interface, int32_t version);
	~ProtocolBase() = default;
private:
	static void WaylandBind(wl_client* client, void* data, uint32_t version, uint32_t id);
	std::unique_ptr<wl_global, void(*)(wl_global*)> global;
};

}
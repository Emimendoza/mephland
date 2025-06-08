#include <mland/protocols/protocol_base.h>

namespace mland {

void
ProtocolBase::WaylandBind(wl_client *client, void *data, uint32_t version, uint32_t id) {
	auto* self = static_cast<ProtocolBase*>(data);
	self->bind(client, version, id);
}


ProtocolBase::ProtocolBase(wl_display *display, const wl_interface *interface, int32_t version) : global(nullptr, [](wl_global*){}) {
	wl_global* glob = wl_global_create(display, interface, version, this, WaylandBind);
	if (!glob) {
		throw std::runtime_error("Failed to create Wayland global");
	}
	global = {glob, wl_global_destroy};
}

}
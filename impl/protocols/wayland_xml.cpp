#include <mland/protocols/wayland_xml.h>

using namespace mland::wayland_xml;

WlCompositor::WlCompositor(wl_display *display) :
	ProtocolBase(display, &wl_compositor_interface, wl_compositor_interface.version) {
}

void
WlCompositor::bind(wl_client *client, int32_t version, uint32_t id) {
	auto* resource = wl_resource_create(client, &wl_compositor_interface, version, id);
	wl_resource_set_implementation(resource, &interface, this, wl_resource_destroy);
}

void WlCompositor::createRegion(wl_client *client, wl_resource *resource, mland::WaylandID id) {

}

void WlCompositor::createSurface(wl_client *client, wl_resource *resource, mland::WaylandID id) {

}

void WlCompositor::destroyRegion(wl_client *client, wl_resource *resource) {

}

void
WlCompositor::addRect(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) {

}

void WlCompositor::subtractRect(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width,
								int32_t height) {

}

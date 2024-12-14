#include "mland/interfaces/compositor.h"
using namespace mland;
using namespace mland::interfaces;

Compositor::Compositor(wl_display* wlDisplay) :
WLInterface(wlDisplay, &WLCompositorImplementation, &wl_compositor_interface, 6) {}

void Compositor::bind(wl_client* client, uint32_t version, uint32_t id) {
	MDEBUG << "Binding compositor" << endl;
}

void Compositor::destroy(wl_resource* resource) {
	MDEBUG << "Destroying compositor" << endl;
}
#include "mland/interfaces/output.h"

#include "mland/vdisplay.h"
#include "mland/wayland_server.h"

using namespace mland;
using namespace mland::interfaces;

void VDisplay::bindToWayland(const WLServer& server) {
	MINFO << name << " Binding to Wayland" << endl;
	output = u_ptr<Output>(new Output(server.getDisplay(), *this));
}

void VDisplay::updateOutput() {
	if (!output)
		return;
	MDEBUG << name << " Updating output" << endl;
	std::lock_guard lock(extentMutex);
	for (const auto& display : output->clients | std::views::keys) {
		output->updateEvent(*display);
		Output::doneEvent(*display);
	}
}

Output::Output(wl_display* wlDisplay, VDisplay& display) :
	WLInterface(wlDisplay, &WLOutputImplementation, &wl_output_interface, 4),
	display(display) {

}

void Output::release(wl_client* client, wl_resource* resource) {
	wlDestroy(resource);
}

void Output::updateEvent(const Client& clientOutput) {
	wl_output_send_geometry(clientOutput.resource, display.extent.width, display.extent.height, display.size.width,
		display.size.height, subpixel, display.make, display.model, transform);
}

void Output::modeEvent(const Client& clientOutput, const uint32_t flags) const {
	wl_output_send_mode(clientOutput.resource, flags, display.extent.width, display.extent.height, display.refreshRate);
}


void Output::doneEvent(const Client& clientOutput) {
	wl_output_send_done(clientOutput.resource);
}

void Output::destroy(wl_resource* resource) {
	MDEBUG << display.name << " Destroying output" << endl;
	// TODO: Implement
}

void Output::bind(wl_client* client, const uint32_t version, const uint32_t id) {
	MDEBUG << display.name << " Binding output" << endl;
	const auto& client_output = createClient(client, version, id);
	std::lock_guard l1(display.modeMutex);
	std::lock_guard l2(display.extentMutex);
	const uint32_t flags = 0x1 | (display.preferredMode ? 0x2 : 0);
	updateEvent(client_output);
	modeEvent(client_output, flags);
	wl_output_send_name(client_output.resource, display.name.c_str());
	doneEvent(client_output);
}
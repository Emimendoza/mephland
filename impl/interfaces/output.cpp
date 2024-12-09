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
	for (const auto& display : output->clientOutputs | std::views::keys) {
		output->updateEvent(*display);
		Output::doneEvent(*display);
	}
}

Output::Output(wl_display* wlDisplay, VDisplay& display) : display(display){
	global = wl_global_create(wlDisplay, &wl_output_interface, 1, this, &bind);
}

Output::~Output() {
	if (global == nullptr)
		return;
	wl_global_destroy(global);
}

void Output::release(wl_client* client, wl_resource* resource) {
	resourceDestroy(resource);
}

void Output::updateEvent(const ClientOutput& clientOutput) {
	wl_output_send_geometry(clientOutput.resource, display.extent.width, display.extent.height, display.size.width,
		display.size.height, subpixel, "unknown", display.name, transform);
}

void Output::doneEvent(const ClientOutput& clientOutput) {
	wl_output_send_done(clientOutput.resource);
}

void Output::resourceDestroy(wl_resource* resource) {
	auto* client_output = static_cast<ClientOutput*>(wl_resource_get_user_data(resource));
	MDEBUG << client_output->parent->display.name << " Destroying output" << endl;
	// TODO: Implement
	client_output->parent->clientOutputs.erase(client_output);
}


void Output::bind(wl_client* client, void* data, const uint32_t version, const uint32_t id) {
	auto& us = *static_cast<Output*>(data);
	auto client_output = std::make_unique<ClientOutput>();
	client_output->resource = wl_resource_create(client, &wl_output_interface, version, id);
	client_output->parent = &us;
	MDEBUG << us.display.name << " Binding output" << endl;
	wl_resource_set_implementation(client_output->resource, &WLOutputImplementation, client_output.get(), &resourceDestroy);

	us.updateEvent(*client_output);
	doneEvent(*client_output);

	us.clientOutputs[client_output.get()] = std::move(client_output);
}
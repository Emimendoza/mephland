#include "mland/interfaces/wl_interface.h"

using namespace mland;
using namespace mland::interfaces;

WLInterface::WLInterface(wl_display* wl_display, const void* implementation, const wl_interface* interface, const uint32_t version) :
	interface(interface),
	implementation(implementation) {
	global = wl_global_create(wl_display, interface, version, this, &wlBind);
}

WLInterface::~WLInterface() {
	if (global == nullptr)
		return;
	wl_global_destroy(global);
}

void WLInterface::wlBind(wl_client* client, void* data, const uint32_t version, const uint32_t id) {
	auto* self = static_cast<WLInterface*>(data);
	self->bind(client, version, id);
}

void WLInterface::wlDestroy(wl_resource* resource) {
	auto* client = static_cast<Client*>(wl_resource_get_user_data(resource));
	client->parent->destroy(resource);
	client->parent->clients.erase(client);
}

WLInterface::Client& WLInterface::createClient(wl_client* client, const uint32_t version, const uint32_t id) {
	u_ptr<Client> clientPtr(new Client{wl_resource_create(client, interface, version, id), this});
	Client* clientPtrRaw = clientPtr.get();
	clients[clientPtrRaw] = std::move(clientPtr);
	wl_resource_set_implementation(clientPtrRaw->resource, implementation, clientPtrRaw, &wlDestroy);
	// ReSharper disable once CppDFALocalValueEscapesFunction
	return *clientPtrRaw;
}


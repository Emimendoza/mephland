#pragma once
#include <wayland-server-protocol.h>
#include "../common.h"


namespace mland::interfaces {


class WLInterface {
public:
	MCLASS(WLInterface);
	WLInterface(const WLInterface&) = delete;
	WLInterface(WLInterface&&) = delete;
	virtual ~WLInterface();

	static void wlBind(wl_client* client, void* data, uint32_t version, uint32_t id);
	static void wlDestroy(wl_resource* resource);

protected:

	struct Client {
		wl_resource* resource;
		WLInterface* parent;
	};

	WLInterface(wl_display* wl_display, const void* implementation, const wl_interface* interface, uint32_t version);
	virtual void bind(wl_client* client, uint32_t version, uint32_t id){}
	virtual void destroy(wl_resource* resource){}

	Client& createClient(wl_client* client, uint32_t version, uint32_t id);

	map<Client*, u_ptr<Client>> clients;

private:
	const wl_interface* interface;
	const void* implementation;
	wl_global* global;
};
}

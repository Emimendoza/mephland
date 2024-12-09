#pragma once
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include "mland/common.h"


namespace mland::interfaces {
class Output {
public:
	MCLASS(Output);
	~Output();
	struct ClientOutput {
		wl_resource* resource;
		Output* parent;
	};

	static void bind(wl_client* client, void* data, uint32_t version, uint32_t id);
	static void resourceDestroy(wl_resource* resource);
	static void release(wl_client* client, wl_resource* resource);


	static constexpr struct wl_output_interface WLOutputImplementation {
		.release = release,
	};



private:
	friend VDisplay;
	Output(wl_display* wlDisplay, VDisplay& display);
	Output(const Output&) = delete;
	Output(Output&&) = delete;

	void updateEvent(const ClientOutput& clientOutput);
	static void doneEvent(const ClientOutput& clientOutput);
	wl_output_subpixel subpixel{WL_OUTPUT_SUBPIXEL_UNKNOWN};
	wl_output_transform transform{WL_OUTPUT_TRANSFORM_NORMAL};
	wl_global* global{};
	VDisplay& display;
	// Maps the address to the object in the address
	map<ClientOutput*, u_ptr<ClientOutput>> clientOutputs;
};
};
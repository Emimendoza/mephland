#pragma once
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include "wl_interface.h"
#include "mland/common.h"


namespace mland::interfaces {
class Output final : WLInterface {
public:
	MCLASS(Output);
	~Output() override = default;
private:
	static void release(wl_client* client, wl_resource* resource);

	static constexpr struct wl_output_interface WLOutputImplementation {
		.release = release,
	};

	friend VDisplay;
	Output(wl_display* wlDisplay, VDisplay& display);
	Output(const Output&) = delete;
	Output(Output&&) = delete;

	// Events
	void updateEvent(const Client& clientOutput);
	void modeEvent(const Client& clientOutput, uint32_t flags) const;
	static void doneEvent(const Client& clientOutput);


	wl_output_subpixel subpixel{WL_OUTPUT_SUBPIXEL_UNKNOWN};
	wl_output_transform transform{WL_OUTPUT_TRANSFORM_NORMAL};
	VDisplay& display;
protected:
	void bind(wl_client* client, uint32_t version, uint32_t id) override;
	void destroy(wl_resource* resource) override;
};
};
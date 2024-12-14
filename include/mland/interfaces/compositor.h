#pragma once
#include "wl_interface.h"
#include "../common.h"
namespace mland::interfaces {

class Compositor final : public WLInterface {
public:
	MCLASS(Compositor);
	~Compositor() override = default;
private:
	static constexpr struct wl_compositor_interface WLCompositorImplementation {

	};

	friend Controller;
	Compositor(wl_display* wlDisplay);
	Compositor(const Compositor&) = delete;
	Compositor(Compositor&&) = delete;
protected:
	void bind(wl_client* client, uint32_t version, uint32_t id) override;
	void destroy(wl_resource* resource) override;

};

}
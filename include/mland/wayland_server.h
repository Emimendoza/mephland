#pragma once
#include <thread>
#include <wayland-server-core.h>

#include "common.h"


namespace mland {

class WLServer {
public:
	MCLASS(WLServer);
	WLServer();
	WLServer(const WLServer&) = delete;
	WLServer(WLServer&& other) = delete;
	~WLServer();

	void stop();
	void waitForStop();

	wl_display* getDisplay() const { return display_; }

private:
	friend Controller;
	void run();
	wl_display* display_{nullptr};
	const char* socket_{nullptr};
	std::thread thread_{};

	std::atomic_flag stop_ = ATOMIC_FLAG_INIT;
	std::atomic_flag stopped_ = ATOMIC_FLAG_INIT;
};

}
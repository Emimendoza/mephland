#include "wayalnd_server.h"


using namespace mland;

void WLServer::stop() {
	if (stop_.test_and_set()) {
		return;
	}
	MDEBUG << "Stopping Wayland Server" << endl;
	while(!stopped_.test()) {
		wl_display_terminate(display_);
		std::this_thread::yield();
	}
	thread_.join();
}

void WLServer::waitForStop() {
	if (stopped_.test()) {
		return;
	}
	stopped_.wait(false);
}

WLServer::WLServer() {
	MDEBUG << "Creating Wayland Server" << endl;
	display_ = wl_display_create();
	if (!display_) {
		MERROR << "Failed to create Wayland display" << endl;
		throw std::runtime_error("WLServer Error");
	}
	socket_ = wl_display_add_socket_auto(display_);
	if (!socket_) {
		MERROR << "Failed to create Wayland socket" << endl;
		throw std::runtime_error("WLServer Error");
	}
	stop_.clear();
	stopped_.clear();
	thread_ = std::thread(&WLServer::run, this);
}

WLServer::~WLServer() {
	MDEBUG << "Destroying Wayland Server" << endl;
	stop();
	wl_display_destroy(display_);
}

void WLServer::run() {
	MINFO << "Starting Wayland Server on " << socket_ << endl;
	wl_display_run(display_);
	MINFO << "Wayland Server stopped" << endl;
	stopped_.test_and_set();
	stopped_.notify_all();
}


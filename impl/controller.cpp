#include <csignal>

#include "controller.h"
#include "vinstance.h"
#include "vdisplay.h"
#include "vulk.h"
using namespace mland;

struct Controller::impl {
	u_ptr<VInstance> instance;
	vec<VDisplay> displays;
	impl(u_ptr<VInstance>& instance) : instance(std::move(instance)){}
};

Controller::Controller(u_ptr<VInstance>& instance) {
	p = std::make_unique<impl>(instance);
	MDEBUG << "Controller created" << endl;
}

Controller::~Controller() {
	MDEBUG << "Controller destroyed" << endl;
}

// TODO: Remove this
static volatile bool running = true;

static void signal_handler(int signal) {
	running = false;
}

void Controller::refreshMonitors() {
	MDEBUG << "Refreshing monitors" << endl;
	vec<VDisplay> displays;
	for (auto& display : p->displays) {
		if (display.isGood()) {
			displays.push_back(std::move(display));
		}
	}
	p->displays = std::move(displays);

	for (const auto& dev : p->instance->refreshDevices()) {
		auto& device = p->instance->getDevice(dev);
		auto monitors = device.updateMonitors();
		p->displays.insert(p->displays.end(), std::make_move_iterator(monitors.begin()), std::make_move_iterator(monitors.end()));
	}
}


void Controller::run() {
	MDEBUG << "Running controller" << endl;
	refreshMonitors();

	std::signal(SIGINT, signal_handler);
	while (running) {
		VDisplay::requestRender();
		refreshMonitors();
		std::this_thread::yield();
	}
}



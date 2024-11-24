#include <csignal>

#include "controller.h"
#include "vinstance.h"
#include "vdisplay.h"
#include "vulk.h"
using namespace mland;

struct Controller::impl {
	VInstance instance;
	impl(VInstance&& instance) : instance(std::move(instance)){}
};

Controller::Controller(VInstance&& instance) {
	p = std::make_unique<impl>(std::move(instance));
	MDEBUG << "Controller created" << endl;
}

Controller::~Controller() {
	MDEBUG << "Controller destroyed" << endl;
}

static volatile bool running = true;

static void signal_handler(int signal) {
	running = false;
}

void Controller::run() {
	MDEBUG << "Running controller" << endl;
	vec<VDisplay> displays;
	for (const auto& dev : p->instance.refreshDevices()) {
		auto& device = p->instance.getDevice(dev);
		auto monitors = device.updateMonitors();
		displays.insert(displays.end(), std::make_move_iterator(monitors.begin()), std::make_move_iterator(monitors.end()));
	}

	std::signal(SIGINT, signal_handler);
	while (running) {
		std::this_thread::yield();
	}
}



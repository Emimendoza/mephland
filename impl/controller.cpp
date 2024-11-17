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

void Controller::run() {
	MDEBUG << "Running controller" << endl;
	for (const auto& dev : p->instance.refreshDevices()) {
		auto& device = p->instance.getDevice(dev);
		auto monitors = device.updateMonitors();
	}
}



#include <csignal>

#include "controller.h"
#include "vinstance.h"
#include "vdisplay.h"
#include "vulk.h"
#include "wayalnd_server.h"
using namespace mland;

struct Controller::impl {
	s_ptr<WLServer> server;
	u_ptr<VInstance> instance;
	vec<u_ptr<VDisplay>> displays{};

	impl(u_ptr<VInstance>&& instance, const s_ptr<WLServer>& server) : server(server), instance(std::move(instance)){}
};

Controller::Controller(u_ptr<VInstance>&& instance, const s_ptr<WLServer>& server) {
	p = std::make_unique<impl>(std::move(instance), server);
	MDEBUG << "Controller created" << endl;
}

Controller::~Controller() {
	p.reset();
	MDEBUG << "Controller destroyed" << endl;
}

void Controller::refreshMonitors() {
	MDEBUG << "Refreshing monitors" << endl;
	vec<u_ptr<VDisplay>> displays;
	for (auto& display : p->displays) {
		if (display->isGood()) {
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
	//refreshMonitors();
	p->server->waitForStop();
}



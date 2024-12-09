#include <csignal>

#include "mland/controller.h"
#include "mland/vinstance.h"
#include "mland/vdisplay.h"
#include "mland/vulk.h"
#include "mland/wayland_server.h"
#include "mland/interfaces/output.h"
using namespace mland;


struct Controller::impl {
	s_ptr<WLServer> server;
	u_ptr<VInstance> instance;
	vec<s_ptr<VDisplay>> displays{};

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
	vec<s_ptr<VDisplay>> newDisplays;
	for (const auto& display : p->displays ) {
		if (display->isGood())
			newDisplays.push_back(display);
	}
	p->displays = std::move(newDisplays);


	for (const auto& dev : p->instance->refreshDevices()) {
		auto& device = p->instance->getDevice(dev);
		auto monitors = device.updateMonitors();
		for (auto& monitor : monitors) {
			if (!monitor->isGood())
				continue;
			monitor->bindToWayland(*p->server);
			p->displays.push_back(monitor);
		}
	}
}

// TODO: Move this somewhere else

static WLServer* pServer = nullptr;

static void signalHandler(int signal) {
	MCLASS(SigH);
	MINFO << "Caught signal " << signal << endl;
	if (signal == SIGINT || signal == SIGTERM) {
		MINFO << "Stopping server" << endl;
		if (pServer) {
			pServer->stop();
		}
	}
}

void Controller::run() {
	MDEBUG << "Running controller" << endl;
	pServer = p->server.get();
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	refreshMonitors();
	MDEBUG << "Starting server" << endl;
	while (!p->server->stopped_.test()) {
		VDisplay::requestRender();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	p->server->waitForStop();
}


